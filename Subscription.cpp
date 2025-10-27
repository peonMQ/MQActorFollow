#pragma once
#include "Subscription.h"

namespace actorfollow {
	postoffice::DropboxAPI DropBox;
	postoffice::Address subscription;

	void SetSubscription(std::string reciever) {
		WriteChatf("[MQActorFollow] Following \ay%s\ax.", reciever.c_str());
		subscription.Server = GetServerShortName();
		subscription.Character = reciever;
		actorfollow::FollowingState = actorfollow::FollowState::ON;
	}

	void Post(postoffice::Address address, mq::proto::actorfollowee::MessageId messageId, const std::optional<proto::actorfollowee::Position>& data = std::nullopt, const std::function<void(int, const std::shared_ptr<postoffice::Message>&)>& callback = nullptr)
	{
		proto::actorfollowee::Message message;
		message.set_id(messageId);
		if (data) {
			*message.mutable_position() = *data;
		}

		DropBox.Post(address, message, callback);
	}

	void ReceivedMessageHandler(const std::shared_ptr<postoffice::Message>& message)
	{
		if (GetGameState() != GAMESTATE_INGAME) {
			return;
		}

		mq::proto::actorfollowee::Message actorFollowMessage;
		actorFollowMessage.ParseFromString(*message->Payload);
		switch (actorFollowMessage.id())
		{
		case mq::proto::actorfollowee::MessageId::Subscribe:
			if (message->Sender) {
				if (!any_of(Subscribers.begin(), Subscribers.end(), [message](const std::shared_ptr<postoffice::Address> subscriber) { return subscriber->Character.value() == message->Sender->Character.value(); })) {
					Subscribers.push_back(std::make_shared<postoffice::Address>(message->Sender.value()));
					proto::actorfollowee::Message reply;
					reply.set_id(mq::proto::actorfollowee::MessageId::Subscribe);
					DropBox.PostReply(message, reply);
				}
			}
			break;
		case mq::proto::actorfollowee::MessageId::UnSubscribe:
			Subscribers.erase(std::remove_if(Subscribers.begin(), Subscribers.end(), [message](const std::shared_ptr<postoffice::Address> subscriber)
				{
					return subscriber->Character == message->Sender->Character;
				}), Subscribers.end());
			break;
		case mq::proto::actorfollowee::MessageId::PositionUpdate:
			if (!(pLocalPC->pSpawn && GetPcProfile())) {
				return;
			}

			auto newposition = actorFollowMessage.position();
			if (pLocalPC->pSpawn->Zone != newposition.zoneid()) {
				return;
			}

			if (Positions.empty()) {
				Positions.push(std::make_shared<mq::proto::actorfollowee::Position>(newposition));
			}
			else {
				auto& settings = actorfollow::GetSettings();
				auto previousposition = Positions.back();
				auto distanceToPreviousPoint = GetDistance3D(previousposition->x(), previousposition->y(), previousposition->z(), newposition.x(), newposition.y(), newposition.z());
				if (distanceToPreviousPoint > 0 && distanceToPreviousPoint > settings.waypoint_min_distance) {
					Positions.push(std::make_shared<mq::proto::actorfollowee::Position>(newposition));
				}
			}
			break;
		}
	}

	void InitializeSubscription() {
		DropBox = postoffice::AddActor(ReceivedMessageHandler);
	}

	void ShutdownSubscription() {
		Subscribers.clear();
		UnSubscribe();
		DropBox.Remove();
	}

	void ClearSubscribers() {
		Subscribers.clear();
	}

	void SendUpdate(PcClient* pcClient)
	{
		if (!pcClient || Subscribers.empty()) return;

		if (auto pSpawn = pcClient->pSpawn) {
			proto::actorfollowee::Position newposition;
			newposition.set_spawnid(pSpawn->SpawnID);
			newposition.set_name(pSpawn->Name);
			newposition.set_zoneid(pSpawn->Zone);
			newposition.set_x(pSpawn->X);
			newposition.set_y(pSpawn->Y);
			newposition.set_z(pSpawn->Z);
			newposition.set_heading(pSpawn->Heading);

			// We’ll copy the name once for safe capture in the lambda
			for (std::shared_ptr<postoffice::Address> subscriber : Subscribers) {
				// capture subscriber by value (safe if lambda runs async)
				auto subscriberPtr = subscriber;
				Post(*subscriberPtr,
					mq::proto::actorfollowee::MessageId::PositionUpdate,
					newposition,
					[subscriberPtr](int code, const std::shared_ptr<postoffice::Message>& reply) {
						if (code < 0) {
							WriteChatf("[MQActorFollow] Failed sending position update to \ay%s\ax>, removing subscriber.", subscriberPtr->Character.value().c_str());
							// Remove the subscriber whose Character matches nameCopy
							Subscribers.erase(std::remove_if(Subscribers.begin(), Subscribers.end(),
								[subscriberPtr](const std::shared_ptr<postoffice::Address> sub) {
									return sub->Character.value() == subscriberPtr->Character.value();
								}), Subscribers.end());
						}
					});
			}
		}
	}

	void Subscribe(PlayerClient* pSpawn) {
		postoffice::Address address;
		address.Server = GetServerShortName();
		address.Character = pSpawn->Name;
		Post(address, proto::actorfollowee::MessageId::Subscribe, std::nullopt, [](int code, const std::shared_ptr<postoffice::Message>& reply) {
			if (GetGameState() != GAMESTATE_INGAME) {
				return;
			}

			if (code < 0) {
				return;
			}

			mq::proto::actorfollowee::Message actorFollowMessage;
			actorFollowMessage.ParseFromString(*reply->Payload);
			switch (actorFollowMessage.id())
			{
			case mq::proto::actorfollowee::MessageId::Subscribe:
				if (reply->Sender && reply->Sender->Character.has_value()) {
					SetSubscription(reply->Sender->Character.value());
				}
				break;
			}
			});
	}

	void UnSubscribe() {
		WriteChatf("[MQActorFollow] EndFollowing.");
		if (subscription.Character) {
			actorfollow::StopMoving();
			actorfollow::FollowingState = actorfollow::FollowState::OFF;
			std::queue<std::shared_ptr<proto::actorfollowee::Position>>().swap(Positions);
			Post(subscription, proto::actorfollowee::MessageId::UnSubscribe);
			WriteChatf("[MQActorFollow] Stopped following \ay%s\ax.", subscription.Character.value().c_str());
			subscription.Server = std::nullopt;
			subscription.Character = std::nullopt;
		}
	}
}