#pragma once
#include <mq/Plugin.h>
#include "PluginSettings.h"
#include "Following.h"

namespace actorfollow {
	inline std::vector<std::shared_ptr<postoffice::Address>> Subscribers;

	void InitializeSubscription();
	void ClearSubscribers();
	void UnSubscribe();
	void SendUpdate(PcClient* pcClient);
	void Subscribe(PlayerClient* pSpawn);
	void ShutdownSubscription();
}