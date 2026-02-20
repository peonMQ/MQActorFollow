#pragma once
#include <mq/Plugin.h>
#include "MovementController.h"
#include "ActorFollow.pb.h"

namespace actorfollow {

	enum class FollowState {
		OFF = 0,
		ON = 1,
		PAUSED = 2
	};

	class FollowController
	{
	public:
		FollowController(actorfollow::SettingsManager* settingsManager)
			: state(FollowState::OFF), samePositionTimer(std::chrono::steady_clock::now()), m_settingsManager(settingsManager), m_movementController(new actorfollow::MovementController(m_settingsManager)) {
		}

		FollowState GetState() const;
		void SetState(FollowState newState);

		bool HasDestinations() const;
		void EnqueueDestination(std::shared_ptr<proto::actorfollow::Position> pos);
		std::shared_ptr<proto::actorfollow::Position> GetCurrentDestination();
		void ClearDestinations();

		void TryFollowActor(PcClient* pcClient, const std::function<void()>& unsubscribeCallback);
		void StopFollowing();
		void InterruptFollowing(const std::function<void()>& unsubscribeCallback);

		int WaypointCount();
		std::queue<std::shared_ptr<proto::actorfollow::Position>> GetPositionsCopy() const;

	private:
		void PopDestination();

		actorfollow::SettingsManager* m_settingsManager;
		actorfollow::MovementController* m_movementController;
		FollowState state;
		std::queue<std::shared_ptr<proto::actorfollow::Position>> positions;
		std::shared_ptr<proto::actorfollow::Position> lastPosition;
		std::chrono::steady_clock::time_point samePositionTimer;
	};

} // namespace actorfollow
