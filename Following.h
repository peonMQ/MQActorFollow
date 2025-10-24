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

	inline  FollowState FollowingState = FollowState::OFF;
	inline  std::queue<std::shared_ptr<mq::proto::actorfollowee::Position>> Positions;

	void TryFollowActor(PcClient* pcClient);
	void InterruptFollowing(const std::function<void()>& unsubscribeCallback);
}