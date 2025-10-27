#include "Subscription.h"

namespace actorfollow {

	postoffice::DropboxAPI DropBox;
	postoffice::Address subscription;

	// -------------------- Helpers --------------------

	void SetSubscription(std::string receiver) {
		WriteChatf("[MQActorFollow] Following \ay%s\ax.", receiver.c_str());
		subscription.Server = GetServerShortName();
		subscription.Character = receiver;

		// Activate following through the controller
		FollowController::Controller().SetState(FollowState::ON);
	}

	void Post(postoffice::Address address, mq::proto::actorfollowee::MessageId messageId,
		const std::optional<proto::actorfollowee::Position>& data = std::nullopt,
		const std::function<void(int, const std::shared_ptr<postoffice::Message>&)>& callback = nullptr)
	{
		proto::actorfollowee::Message message;
		message.set_id(messageId);
		if (data) {
			*message.mutable_position() = *data;
		}
		DropBox.Post(address, message, callback);
	}

	void ReceivedMessageHandler(const std::shared_ptr<postoffice::Message>& message) {
		if (GetGameState() != GAMESTATE_INGAME) return;

		mq::proto::actorfollowee::Message actorFollowMessage;
		actorFollowMessage.ParseFromString(*message->Payload);

		switch (actorFollowMessage.id()) {
		case mq::proto::actorfollowee::MessageId::Subscribe:
			if (message->Sender) {
				auto exists = std::any_of(Subscribers.begin(), Subscribers.end(),
					[message](const std::shared_ptr<postoffice::Address>& sub) {
						return sub->Character.value() == message->Sender->Character.value();
					});

				if (!exists) {
					Subscribers.push_back(std::make_shared<postoffice::Address>(message->Sender.value()));
					proto::actorfollowee::Message reply;
					reply.set_id(mq::proto::actorfollowee::MessageId::Subscribe);
					DropBox.PostReply(message, reply);
				}
			}
			break;

		case mq::proto::actorfollowee::MessageId::UnSubscribe:
			Subscribers.erase(
				std::remove_if(Subscribers.begin(), Subscribers.end(),
					[message](const std::shared_ptr<postoffice::Address>& sub) {
						return sub->Character == message->Sender->Character;
					}),
				Subscribers.end());
			break;

		case mq::proto::actorfollowee::MessageId::PositionUpdate:
			if (!(pLocalPC->pSpawn && GetPcProfile())) return;

			auto newPosition = actorFollowMessage.position();

			if (pLocalPC->pSpawn->Zone != newPosition.zoneid()) return;

			auto& controller = FollowController::Controller();
			if (!controller.HasDestinations()) {
				controller.EnqueueDestination(std::make_shared<mq::proto::actorfollowee::Position>(newPosition));
			}
			else {
				auto currentDestination = controller.GetCurrentDestination();
				auto& settings = actorfollow::GetSettings();
				auto distance = GetDistance3D(
					currentDestination->x(), currentDestination->y(), currentDestination->z(),
					newPosition.x(), newPosition.y(), newPosition.z()
				);

				if (distance > 0 && distance > settings.waypoint_min_distance) {
					controller.EnqueueDestination(std::make_shared<mq::proto::actorfollowee::Position>(newPosition));
				}
			}
			break;
		}
	}

	// -------------------- Public API --------------------

	void InitializeSubscription() {
		DropBox = postoffice::AddActor(ReceivedMessageHandler);
	}

	void ShutdownSubscription() {
		ClearSubscribers();
		UnSubscribe();
		DropBox.Remove();
	}

	void ClearSubscribers() {
		Subscribers.clear();
	}

	void SendUpdate(PcClient* pcClient) {
		if (!pcClient || Subscribers.empty()) return;
		if (!pcClient->pSpawn) return;

		proto::actorfollowee::Position newPosition;
		newPosition.set_spawnid(pcClient->pSpawn->SpawnID);
		newPosition.set_name(pcClient->pSpawn->Name);
		newPosition.set_zoneid(pcClient->pSpawn->Zone);
		newPosition.set_x(pcClient->pSpawn->X);
		newPosition.set_y(pcClient->pSpawn->Y);
		newPosition.set_z(pcClient->pSpawn->Z);
		newPosition.set_heading(pcClient->pSpawn->Heading);

		for (auto& subscriber : Subscribers) {
			auto subscriberPtr = subscriber;
			Post(*subscriberPtr, mq::proto::actorfollowee::MessageId::PositionUpdate, newPosition,
				[subscriberPtr](int code, const std::shared_ptr<postoffice::Message>& reply) {
					if (code < 0) {
						WriteChatf("[MQActorFollow] Failed sending position update to \ay%s\ax, removing subscriber.",
							subscriberPtr->Character.value().c_str());

						Subscribers.erase(
							std::remove_if(Subscribers.begin(), Subscribers.end(),
								[subscriberPtr](const std::shared_ptr<postoffice::Address>& sub) {
									return sub->Character.value() == subscriberPtr->Character.value();
								}),
							Subscribers.end());
					}
				});
		}
	}

	void Subscribe(PlayerClient* pSpawn) {
		postoffice::Address address;
		address.Server = GetServerShortName();
		address.Character = pSpawn->Name;

		Post(address, proto::actorfollowee::MessageId::Subscribe, std::nullopt,
			[](int code, const std::shared_ptr<postoffice::Message>& reply) {
				if (GetGameState() != GAMESTATE_INGAME) return;
				if (code < 0) return;

				mq::proto::actorfollowee::Message msg;
				msg.ParseFromString(*reply->Payload);

				if (msg.id() == mq::proto::actorfollowee::MessageId::Subscribe &&
					reply->Sender && reply->Sender->Character.has_value()) {
					SetSubscription(reply->Sender->Character.value());
				}
			});
	}

	void UnSubscribe() {
		WriteChatf("[MQActorFollow] EndFollowing.");
		if (subscription.Character) {
			// Pop all remaining destinations
			while (FollowController::Controller().HasDestinations()) {
				FollowController::Controller().PopDestination(false);
			}

			// Set following state to OFF
			FollowController::Controller().SetState(FollowState::OFF);

			Post(subscription, proto::actorfollowee::MessageId::UnSubscribe);
			WriteChatf("[MQActorFollow] Stopped following \ay%s\ax.", subscription.Character.value().c_str());

			subscription.Server = std::nullopt;
			subscription.Character = std::nullopt;
		}
	}

} // namespace actorfollow
