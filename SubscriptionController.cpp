#include "SubscriptionController.h"

namespace actorfollow 
{
	void SubscriptionController::Initialize()
	{
		ClearSubscribers();
		subscription.Server = std::nullopt;
		subscription.Character = std::nullopt;
		// Reset FollowController state
		m_followController.StopFollowing();
	}

	void SubscriptionController::Shutdown()
	{
		CancelSubscriptions();
		Unsubscribe();
	}

	void SubscriptionController::Post(
		postoffice::Address address,
		mq::proto::actorfollow::MessageId messageId,
		const std::optional<proto::actorfollow::Position>& data,
		const std::function<void(int, const std::shared_ptr<postoffice::Message>&)>& callback)
	{
		proto::actorfollow::Message message;
		message.set_id(messageId);
		if (data)
			*message.mutable_position() = *data;

		dropbox.Post(address, message, callback);
	}

	void SubscriptionController::SetSubscription(std::string receiver)
	{
		WriteChatf("[MQActorFollow] Following \ay%s\ax.", receiver.c_str());
		subscription.Server = GetServerShortName();
		subscription.Character = receiver;

		m_followController.SetState(FollowState::ON);
	}

	void SubscriptionController::ClearSubscribers()
	{
		subscribers.clear();
	}

	void SubscriptionController::CancelSubscriptions()
	{
		if (HasSubscriptions()) {
			for (auto& sub : subscribers)
			{
				auto subPtr = sub;
				Post(*subPtr, mq::proto::actorfollow::MessageId::CancelSubscription);
			}
			ClearSubscribers();
		}
	}

	void SubscriptionController::Unsubscribe()
	{
		if (!subscription.Character)
			return;

		m_followController.StopFollowing();
		Post(subscription, proto::actorfollow::MessageId::UnSubscribe);
		WriteChatf("[MQActorFollow] Stopped following \ay%s\ax.", subscription.Character.value().c_str());

		subscription.Server = std::nullopt;
		subscription.Character = std::nullopt;
	}

	void SubscriptionController::Subscribe(PlayerClient* pSpawn)
	{
		postoffice::Address address;
		address.Server = GetServerShortName();
		address.Character = pSpawn->Name;

		Post(address, proto::actorfollow::MessageId::Subscribe, std::nullopt,
			[this](int code, const std::shared_ptr<postoffice::Message>& reply) {
				if (GetGameState() != GAMESTATE_INGAME || code < 0)
					return;

				mq::proto::actorfollow::Message msg;
				msg.ParseFromString(*reply->Payload);

				if (msg.id() == mq::proto::actorfollow::MessageId::Subscribe &&
					reply->Sender && reply->Sender->Character.has_value())
				{
					SetSubscription(reply->Sender->Character.value());
				}
			});
	}

	void SubscriptionController::SendUpdate(PcClient* pcClient)
	{
		if (!pcClient || !pcClient->pSpawn || subscribers.empty())
			return;

		proto::actorfollow::Position pos;
		pos.set_spawnid(pcClient->pSpawn->SpawnID);
		pos.set_name(pcClient->pSpawn->Name);
		pos.set_zoneid(pcClient->pSpawn->Zone);
		pos.set_x(pcClient->pSpawn->X);
		pos.set_y(pcClient->pSpawn->Y);
		pos.set_z(pcClient->pSpawn->Z);
		pos.set_heading(pcClient->pSpawn->Heading);

		for (auto& sub : subscribers)
		{
			auto subPtr = sub;
			Post(*subPtr, mq::proto::actorfollow::MessageId::PositionUpdate, pos,
				[this, subPtr](int code, const std::shared_ptr<postoffice::Message>&) {
					if (code < 0)
					{
						WriteChatf("[MQActorFollow] Failed sending update to \ay%s\ax, removing subscriber.",
							subPtr->Character.value().c_str());

						subscribers.erase(std::remove_if(subscribers.begin(), subscribers.end(),
							[subPtr](const std::shared_ptr<postoffice::Address>& s) {
								return s->Character.value() == subPtr->Character.value();
							}), subscribers.end());
					}
				});
		}
	}

	bool SubscriptionController::HasSubscriptions() {
		return !subscribers.empty();
	}

	void SubscriptionController::ReceivedMessageHandler(const std::shared_ptr<postoffice::Message>& message)
	{
		if (GetGameState() != GAMESTATE_INGAME)
			return;

		mq::proto::actorfollow::Message msg;
		msg.ParseFromString(*message->Payload);

		switch (msg.id())
		{
		case mq::proto::actorfollow::MessageId::Subscribe:
			if (message->Sender)
			{
				bool exists = std::any_of(subscribers.begin(), subscribers.end(),
					[&](const auto& s) { return s->Character == message->Sender->Character; });

				if (!exists)
				{
					subscribers.push_back(std::make_shared<postoffice::Address>(message->Sender.value()));
					proto::actorfollow::Message reply;
					reply.set_id(mq::proto::actorfollow::MessageId::Subscribe);
					dropbox.PostReply(message, reply);
				}
			}
			break;

		case mq::proto::actorfollow::MessageId::UnSubscribe:
			subscribers.erase(
				std::remove_if(subscribers.begin(), subscribers.end(),
					[&](const auto& s) { return s->Character == message->Sender->Character; }),
				subscribers.end());
			break;

		case mq::proto::actorfollow::MessageId::CancelSubscription:
			subscribers.erase(
				std::remove_if(subscribers.begin(), subscribers.end(),
					[&](const auto& s) { return s->Character == message->Sender->Character; }),
				subscribers.end());
			break;

		case mq::proto::actorfollow::MessageId::PositionUpdate: {
			if (!(pLocalPC->pSpawn && GetPcProfile()))
				return;

			auto pos = msg.position();
			if (pLocalPC->pSpawn->Zone != pos.zoneid())
				return;

			auto& controller = m_followController;
			if (!controller.HasDestinations())
			{
				controller.EnqueueDestination(std::make_shared<mq::proto::actorfollow::Position>(pos));
			}
			else if (auto current = controller.GetCurrentDestination())
			{
				const auto& settings = m_settingsManager.Get();
				float dist = GetDistance3D(current->x(), current->y(), current->z(),
					pos.x(), pos.y(), pos.z());
				if (dist > settings.waypoint_min_distance)
				{
					controller.EnqueueDestination(std::make_shared<mq::proto::actorfollow::Position>(pos));
				}
			}
		} break;
		}
	}

} // namespace actorfollow
