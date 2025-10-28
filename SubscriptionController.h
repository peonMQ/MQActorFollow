#pragma once
#include <mq/Plugin.h>
#include "ActorFollowee.pb.h"
#include "FollowController.h"

namespace actorfollow {

	class SubscriptionController
	{
	public:
		static SubscriptionController& Instance();

		void Initialize();
		void Shutdown();

		void Subscribe(PlayerClient* pSpawn);
		void Unsubscribe();
		void SendUpdate(PcClient* pcClient);
		bool HasSubscriptions();

		const std::vector<std::shared_ptr<postoffice::Address>>& GetSubscribers() const { return subscribers; }

	private:
		SubscriptionController() = default;

		// Handlers
		void ReceivedMessageHandler(const std::shared_ptr<postoffice::Message>& message);
		void SetSubscription(std::string receiver);
		void ClearSubscribers();

		// Core messaging
		void Post(
			postoffice::Address address,
			mq::proto::actorfollowee::MessageId messageId,
			const std::optional<proto::actorfollowee::Position>& data = std::nullopt,
			const std::function<void(int, const std::shared_ptr<postoffice::Message>&)>& callback = nullptr
		);

	private:
		postoffice::DropboxAPI dropbox;
		postoffice::Address subscription;
		std::vector<std::shared_ptr<postoffice::Address>> subscribers;
	};

} // namespace actorfollow
