#pragma once
#include <mq/Plugin.h>
#include "ActorFollow.pb.h"
#include "FollowController.h"

namespace actorfollow {

	class SubscriptionController
	{
	public:
		SubscriptionController(actorfollow::SettingsManager& settingsManager, actorfollow::FollowController& followController) : m_settingsManager(settingsManager), m_followController(followController)
		{
			dropbox = postoffice::AddActor([this](const std::shared_ptr<postoffice::Message>& msg) {
				ReceivedMessageHandler(msg);
				});
			ClearSubscribers();
			subscription.Server = std::nullopt;
			subscription.Character = std::nullopt;
			// Reset FollowController state
			m_followController.StopFollowing();
		}

		~SubscriptionController()
		{
			CancelSubscriptions();
			Unsubscribe();
			dropbox.Remove();
		}

		void Initialize();
		void Shutdown();

		void Subscribe(PlayerClient* pSpawn);
		void Unsubscribe();
		void SendUpdate(PcClient* pcClient);
		bool HasSubscriptions();
		void CancelSubscriptions();

		const std::vector<std::shared_ptr<postoffice::Address>>& GetSubscribers() const { return subscribers; }

	private:
		// Handlers
		void ReceivedMessageHandler(const std::shared_ptr<postoffice::Message>& message);
		void SetSubscription(std::string receiver);
		void ClearSubscribers();

		// Core messaging
		void Post(
			postoffice::Address address,
			mq::proto::actorfollow::MessageId messageId,
			const std::optional<proto::actorfollow::Position>& data = std::nullopt,
			const std::function<void(int, const std::shared_ptr<postoffice::Message>&)>& callback = nullptr
		);

	private:
		actorfollow::SettingsManager m_settingsManager;
		actorfollow::FollowController m_followController;
		postoffice::DropboxAPI dropbox;
		postoffice::Address subscription;
		std::vector<std::shared_ptr<postoffice::Address>> subscribers;
	};

} // namespace actorfollow
