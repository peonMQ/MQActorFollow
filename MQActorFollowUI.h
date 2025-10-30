#pragma once
#include <mq/Plugin.h>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/ImGuiUtils.h"

#include "SettingsManager.h"
#include "SubscriptionController.h"
#include "ActorFollowee.pb.h"

enum class TabPage
{
	Settings,
	Waypoints,
	Subscribers,
	Max
};

class MQActorFollowUI
{
public:
	static MQActorFollowUI& Instance()
	{
		static MQActorFollowUI instance;
		return instance;
	}

	void RenderUI();
	void Show() { showActorFollowUI = true; }
	void Hide() { showActorFollowUI = false; }
	bool IsVisible() const { return showActorFollowUI; }

private:
	MQActorFollowUI();
	MQActorFollowUI(const MQActorFollowUI&) = delete;
	MQActorFollowUI& operator=(const MQActorFollowUI&) = delete;

	// Persistent UI open/close state
	bool showActorFollowUI;

	// Current selected tab
	int m_selectedTab;

	// Static UI colors
	inline static const ImVec4 ColorRed = ImVec4(0.990f, 0.148f, 0.148f, 1.0f);
	inline static const ImVec4 ColorGreen = ImVec4(0.0142f, 0.710f, 0.0490f, 1.0f);
	inline static const ImVec4 ColorCyan = ImVec4(0.165f, 0.631f, 0.596f, 1.0f);
	inline static const ImVec4 ColorYellow = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
	inline static const ImVec4 ColorMagenta = ImVec4(0.827f, 0.212f, 0.51f, 1.0f);
	inline static const ImVec4 ColorOrange = ImVec4(0.796f, 0.294f, 0.086f, 1.0f);

	inline static const char* s_tabNames[(size_t)TabPage::Max] = {
		"Settings",
		"Waypoints",
		"Subscribers"
	};

	static std::map<std::chrono::seconds, ImVec4, std::greater<std::chrono::seconds>> CountDownColorMap;

	// Helpers
	static std::tuple<int, int> ToMinutesAndSeconds(const std::chrono::seconds& time);
	static ImVec4 GetColor(std::chrono::seconds time);

	void DrawSettingsUI();
	void DrawWaypointsUI(const std::vector<std::shared_ptr<proto::actorfollowee::Position>>& waypoints);
	void DrawSubscribersUI(const std::vector<std::shared_ptr<postoffice::Address>>& subscribers);
	void PerformUpdateTab(TabPage page);

public:
	template <typename T>
	static std::vector<std::shared_ptr<T>> queueToVector(const std::queue<std::shared_ptr<T>>& q)
	{
		std::vector<std::shared_ptr<T>> result;
		std::queue<std::shared_ptr<T>> copy = q;
		while (!copy.empty())
		{
			result.push_back(copy.front());
			copy.pop();
		}
		return result;
	}
};
