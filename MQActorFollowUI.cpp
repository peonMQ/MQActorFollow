#include "MQActorFollowUI.h"
#include "FollowController.h"
#include <map>
#include <tuple>

ImVec4 ColorRed = ImVec4(0.990f, 0.148f, 0.148f, 1.0f);
ImVec4 ColorGreen = ImVec4(0.0142f, 0.710f, 0.0490f, 1.0f);
ImVec4 ColorCyan = ImVec4(0.165f, 0.631f, 0.596f, 1.0f);
ImVec4 ColorYellow = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
ImVec4 ColorMagenta = ImVec4(0.827f, 0.212f, 0.51f, 1.0f);
ImVec4 ColorOrange = ImVec4(0.796f, 0.294f, 0.086f, 1.0f);

int m_selectedTab = static_cast<int>(TabPage::Settings);

namespace
{
	static const char* s_tabNames[(size_t)TabPage::Max] = {
		"Settings",
		"Waypoints",
		"Subscribers"
	};
}

static std::map<std::chrono::seconds, ImVec4, std::greater<std::chrono::seconds>> CountDownColorMap = {
	{std::chrono::seconds(600), ColorCyan},
	{std::chrono::seconds(60), ColorYellow},
	{std::chrono::seconds(0), ColorRed},
};

static std::tuple<int, int> ToMinutesAndSeconds(const std::chrono::seconds& time) {
	int minutes = std::chrono::duration_cast<std::chrono::minutes>(time).count();
	int seconds = time.count() % 60;
	return  std::make_tuple(minutes, seconds);
}

static const ImVec4 GetColor(std::chrono::seconds time) {
	for (const auto& colorMap : CountDownColorMap) {
		if (time.count() > colorMap.first.count()) {
			return colorMap.second;
		}
	}
	return ColorOrange;
}

// -------------------- Settings Tab --------------------

void DrawSettingsUI()
{
	bool changed = false;
	auto& settings = actorfollow::GetSettings();

	enum BreakBehavior {
		DoNothing = 0,
		Stop = 1,
		Pause = 2
	};

	int current = DoNothing;
	if (settings.autobreak)
		current = Stop;
	else if (settings.autopause)
		current = Pause;

	ImGui::BeginDisabled();
	if (ImGui::Combo("Break Behavior", &current, "Disabled\0Stop Navigation\0Pause Navigation"))
	{
		settings.autobreak = current == Stop;
		settings.autopause = current == Pause;
		changed = true;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(
			"Auto Break Behavior\n"
			"-------------------------------------------------\n"
			"What happens when a movement key is pressed.\n"
			"  Disable - Auto break is turned off\n"
			"  Stop - Stop the navigation\n"
			"  Pause - Pause navigation. /nav pause to unpause");
	}
	ImGui::EndDisabled();

	ImGui::NewLine();
	if (ImGui::Checkbox("Automatically click nearby doors", &settings.open_doors))
		changed = true;
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Scans for nearby doors to open automatically while navigating.\nTries to avoid clicking teleporters and elevators.");

	ImGui::NewLine();
	if (ImGui::Checkbox("Attempt to get unstuck", &settings.attempt_unstuck))
		changed = true;
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Automatically try to get unstuck if movement is impeded.\nThis may jump or click randomly. Use with caution!");

	ImGui::NewLine();
	if (ImGui::SliderInt("Waypoint min distance", &settings.waypoint_min_distance, 0, 50))
		changed = true;
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("The minimum required distance between waypoints.");

	ImGui::NewLine();
	if (ImGui::SliderInt("Waypoint timeout", &settings.waypoint_timeout_seconds, 0, 10))
		changed = true;
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("The maximum time in seconds to reach a waypoint. If timeout is reached, the waypoint is removed.");

	if (changed)
		actorfollow::SaveSettings();

	ImGui::Columns(1);
}

// -------------------- Waypoints Tab --------------------
void DrawWaypointsUI(const std::vector<std::shared_ptr<proto::actorfollowee::Position>>& waypoints)
{
	if (ImGui::BeginTable("##Waypoints", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable))
	{
		ImGui::TableSetupColumn("Queue#", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Waypoint", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Distance", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		size_t index = 0;
		for (const auto& waypoint : waypoints) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%d", index);
			ImGui::TableNextColumn(); ImGui::Text("%.2f, %.2f, %.2f", waypoint->x(), waypoint->y(), waypoint->z());
			ImGui::TableNextColumn();
			if (pLocalPC && pLocalPC->pSpawn) {
				auto pSpawn = pLocalPC->pSpawn;
				auto pos = CVector3{ waypoint->x(), waypoint->y(), waypoint->z() };
				auto distance = GetDistance3D(pos.X, pos.Y, pos.Z, pSpawn->X, pSpawn->Y, pSpawn->Z);
				ImGui::Text("%.2f", distance);
			}
			else {
				ImGui::Text("N/A");
			}
			++index;
		}

		ImGui::EndTable();
	}
}

// -------------------- Subscribers Tab --------------------

void DrawSubscribersUI(const std::vector<std::shared_ptr<postoffice::Address>>& subscribers)
{
	if (ImGui::BeginTable("##Subscribers", 1, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable))
	{
		ImGui::TableSetupColumn("Subscriber", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		for (const auto& sub : subscribers) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (sub->Name.has_value()) ImGui::Text("%s", sub->Name->c_str());
			else ImGui::Text("%s", sub->Character->c_str());
		}

		ImGui::EndTable();
	}
}

// -------------------- Tab Dispatcher --------------------

void PerformUpdateTab(TabPage page)
{
	if (page == TabPage::Settings) DrawSettingsUI();
	else if (page == TabPage::Waypoints) DrawWaypointsUI(queueToVector(actorfollow::FollowController::Controller().GetPositionsCopy()));
	else if (page == TabPage::Subscribers) DrawSubscribersUI(actorfollow::Subscribers);
}

// -------------------- Main UI --------------------

void RenderUI(bool* p_open) {
	if (ImGui::Begin("MQActorFollow", p_open, 0))
	{
		if (ImGui::BeginTabBar("##main", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton | ImGuiTabBarFlags_FittingPolicyScroll))
		{
			for (int i = 0; i < static_cast<int>(TabPage::Max); ++i) {
				if (ImGui::BeginTabItem(s_tabNames[i])) {
					m_selectedTab = i;
					PerformUpdateTab(static_cast<TabPage>(m_selectedTab));
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
}
