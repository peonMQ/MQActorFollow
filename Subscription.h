#pragma once
#include <mq/Plugin.h>
#include "PluginSettings.h"
#include "Following.h"

namespace actorfollow {
	inline std::vector<std::shared_ptr<postoffice::Address>> Subscribers;

	void InitializeSubscription();
	void ClearSubscribers();
	void EndFollowing();
	void InterruptFollowing();
	void SendPositionUpdate(PcClient* pcClient);
	void TrySubscribeFollowing(PlayerClient* pSpawn);
	void CleanupSubscription();
}