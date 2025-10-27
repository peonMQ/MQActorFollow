#pragma once
#include <mq/Plugin.h>
#include "PluginSettings.h"
#include "Movement.h"
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
		void PopDestination(bool forceStop);

		void TryFollowActor(PcClient* pcClient);
		void StopMoving();
		void InterruptFollowing(const std::function<void()>& unsubscribeCallback);

		std::queue<std::shared_ptr<proto::actorfollowee::Position>> GetPositionsCopy() const;

	private:
		FollowController();

		FollowState state;
		std::queue<std::shared_ptr<proto::actorfollowee::Position>> positions;
		std::shared_ptr<proto::actorfollowee::Position> lastPosition;
		std::chrono::steady_clock::time_point samePositionTimer;
		SettingsData settings;
	};

} // namespace actorfollow
