#pragma once
#include <mq/Plugin.h>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/ImGuiUtils.h"

#include "PluginSettings.h"
#include "ActorFollowee.pb.h"

enum class TabPage {
	Settings,
	Waypoints,
	Subscribers,
	Max
};

static bool ShowMQActorFollowWindow = true;

void RenderUI(const std::vector<std::shared_ptr<postoffice::Address>>& subscribers, const std::vector<std::shared_ptr<proto::actorfollowee::Position>>& waypoints);
