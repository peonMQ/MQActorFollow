#include "SubscriptionController.h"
#include "PluginSettings.h"
#include "Movement.h"

namespace actorfollow {

	SubscriptionController& SubscriptionController::Instance()
	{
		static SubscriptionController instance;
		return instance;
	}

	void SubscriptionController::Initialize()
	{
		dropbox = postoffice::AddActor([this](const std::shared_ptr<postoffice::Message>& msg) {
			ReceivedMessageHandler(msg);
			});
		ClearSubscribers();
		subscription.Server = std::nullopt;
		subscription.Character = std::nullopt;
		// Reset FollowController state
		FollowController::Controller().StopFollowing();
	}

	void SubscriptionController::Shutdown()
	{
		ClearSubscribers();
		Unsubscribe();
		dropbox.Remove();
	}

	void SubscriptionController::Post(
		postoffice::Address address,
		mq::proto::actorfollowee::MessageId messageId,
		const std::optional<proto::actorfollowee::Position>& data,
		const std::function<void(int, const std::shared_ptr<postoffice::Message>&)>& callback)
	{
		proto::actorfollowee::Message message;
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

		FollowController::Controller().SetState(FollowState::ON);
	}

	void SubscriptionController::ClearSubscribers()
	{
		subscribers.clear();
	}

	void SubscriptionController::Unsubscribe()
	{
		if (!subscription.Character)
			return;

		FollowController::Controller().StopFollowing();
		Post(subscription, proto::actorfollowee::MessageId::UnSubscribe);
		WriteChatf("[MQActorFollow] Stopped following \ay%s\ax.", subscription.Character.value().c_str());

		subscription.Server = std::nullopt;
		subscription.Character = std::nullopt;
	}

	void SubscriptionController::Subscribe(PlayerClient* pSpawn)
	{
		postoffice::Address address;
		address.Server = GetServerShortName();
		address.Character = pSpawn->Name;

		Post(address, proto::actorfollowee::MessageId::Subscribe, std::nullopt,
			[this](int code, const std::shared_ptr<postoffice::Message>& reply) {
				if (GetGameState() != GAMESTATE_INGAME || code < 0)
					return;

				mq::proto::actorfollowee::Message msg;
				msg.ParseFromString(*reply->Payload);

				if (msg.id() == mq::proto::actorfollowee::MessageId::Subscribe &&
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

		proto::actorfollowee::Position pos;
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
			Post(*subPtr, mq::proto::actorfollowee::MessageId::PositionUpdate, pos,
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

		mq::proto::actorfollowee::Message msg;
		msg.ParseFromString(*message->Payload);

		switch (msg.id())
		{
		case mq::proto::actorfollowee::MessageId::Subscribe:
			if (message->Sender)
			{
				bool exists = std::any_of(subscribers.begin(), subscribers.end(),
					[&](const auto& s) { return s->Character == message->Sender->Character; });

				if (!exists)
				{
					subscribers.push_back(std::make_shared<postoffice::Address>(message->Sender.value()));
					proto::actorfollowee::Message reply;
					reply.set_id(mq::proto::actorfollowee::MessageId::Subscribe);
					dropbox.PostReply(message, reply);
				}
			}
			break;

		case mq::proto::actorfollowee::MessageId::UnSubscribe:
			subscribers.erase(
				std::remove_if(subscribers.begin(), subscribers.end(),
					[&](const auto& s) { return s->Character == message->Sender->Character; }),
				subscribers.end());
			break;

		case mq::proto::actorfollowee::MessageId::PositionUpdate: {
			if (!(pLocalPC->pSpawn && GetPcProfile()))
				return;

			auto pos = msg.position();
			if (pLocalPC->pSpawn->Zone != pos.zoneid())
				return;

			auto& controller = FollowController::Controller();
			if (!controller.HasDestinations())
			{
				controller.EnqueueDestination(std::make_shared<mq::proto::actorfollowee::Position>(pos));
			}
			else if (auto current = controller.GetCurrentDestination())
			{
				auto& settings = GetSettings();
				float dist = GetDistance3D(current->x(), current->y(), current->z(),
					pos.x(), pos.y(), pos.z());
				if (dist > settings.waypoint_min_distance)
				{
					controller.EnqueueDestination(std::make_shared<mq::proto::actorfollowee::Position>(pos));
				}
			}
		} break;
		}
	}

} // namespace actorfollow
