#pragma once
#include <mq/Plugin.h>
#include "MovementController.h"
#include "ActorFollowee.pb.h"

namespace actorfollow {

	enum class FollowState {
		OFF = 0,
		ON = 1,
		PAUSED = 2
	};

	class FollowController
	{
	public:
		static FollowController& Controller();

		FollowState GetState() const;
		void SetState(FollowState newState);

		bool HasDestinations() const;
		void EnqueueDestination(std::shared_ptr<proto::actorfollowee::Position> pos);
		std::shared_ptr<proto::actorfollowee::Position> GetCurrentDestination();
		void ClearDestinations();

		void TryFollowActor(PcClient* pcClient, const std::function<void()>& unsubscribeCallback);
		void StopFollowing();
		void InterruptFollowing(const std::function<void()>& unsubscribeCallback);

		int WaypointCount();
		std::queue<std::shared_ptr<proto::actorfollowee::Position>> GetPositionsCopy() const;

	private:
		FollowController();
		void PopDestination();

		MovementController& movement = MovementController::Instance();
		FollowState state;
		std::queue<std::shared_ptr<proto::actorfollowee::Position>> positions;
		std::shared_ptr<proto::actorfollowee::Position> lastPosition;
		std::chrono::steady_clock::time_point samePositionTimer;
	};

} // namespace actorfollow
