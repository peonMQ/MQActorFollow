#pragma once
#include <mq/Plugin.h>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/ImGuiUtils.h"

#include "SettingsManager.h"
#include "SubscriptionController.h"
#include "ActorFollowee.pb.h"

enum class TabPage {
	Settings,
	Waypoints,
	Subscribers,
	Max
};

void RenderUI(bool* p_open);

template <typename T>
std::vector<std::shared_ptr<T>> queueToVector(const std::queue<std::shared_ptr<T>>& q) {
	std::vector<std::shared_ptr<T>> result;
	std::queue<std::shared_ptr<T>> copy = q;  // copy the queue

	while (!copy.empty()) {
		result.push_back(copy.front());
		copy.pop();
	}

	return result;
}
