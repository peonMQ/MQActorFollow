#include "MQActorFollowUI.h"

// Initialize static color map
std::map<std::chrono::seconds, ImVec4, std::greater<std::chrono::seconds>> MQActorFollowUI::CountDownColorMap = {
	{std::chrono::seconds(600), MQActorFollowUI::ColorCyan},
	{std::chrono::seconds(60),  MQActorFollowUI::ColorYellow},
	{std::chrono::seconds(0),   MQActorFollowUI::ColorRed},
};

// Constructor
MQActorFollowUI::MQActorFollowUI()
	: showActorFollowUI(actorfollow::SettingsManager::Instance().Get().show_ui_on_startup),
	m_selectedTab(static_cast<int>(TabPage::Settings))
{
	/*int val = showActorFollowUI ? 1 : 0;
	WriteChatColorf("[MQActorFollow] MQActorFollowUI initialized %d", val);*/
}

// Helpers
std::tuple<int, int> MQActorFollowUI::ToMinutesAndSeconds(const std::chrono::seconds& time)
{
	int minutes = std::chrono::duration_cast<std::chrono::minutes>(time).count();
	int seconds = time.count() % 60;
	return std::make_tuple(minutes, seconds);
}

ImVec4 MQActorFollowUI::GetColor(std::chrono::seconds time)
{
	for (const auto& colorMap : CountDownColorMap)
	{
		if (time.count() > colorMap.first.count())
			return colorMap.second;
	}
	return ColorOrange;
}

// -------------------- UI Drawing --------------------
void MQActorFollowUI::DrawSettingsUI()
{
	bool changed = false;
	auto& settings = actorfollow::SettingsManager::Instance().Mutable();

	enum BreakBehavior { DoNothing = 0, Stop = 1, Pause = 2 };

	if (ImGui::Checkbox("Show UI", &settings.show_ui_on_startup))
		changed = true;
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Toggle displaying the UI");

	int current = DoNothing;
	if (settings.autobreak) current = Stop;
	else if (settings.autopause) current = Pause;

	ImGui::NewLine();
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
		ImGui::SetTooltip("Scans for nearby doors to open automatically while navigating.");

	ImGui::NewLine();
	if (ImGui::Checkbox("Attempt to get unstuck", &settings.attempt_unstuck))
		changed = true;
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Automatically try to get unstuck if movement is impeded.");

	ImGui::NewLine();
	if (ImGui::SliderInt("Waypoint min distance", &settings.waypoint_min_distance, 0, 50))
		changed = true;
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("The minimum required distance between waypoints.");

	ImGui::NewLine();
	if (ImGui::SliderInt("Waypoint timeout", &settings.waypoint_timeout_seconds, 0, 10))
		changed = true;
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("The maximum time in seconds to reach a waypoint.");

	if (changed)
		actorfollow::SettingsManager::Instance().Save();

	ImGui::Columns(1);
}

void MQActorFollowUI::DrawWaypointsUI(const std::vector<std::shared_ptr<proto::actorfollowee::Position>>& waypoints)
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

void MQActorFollowUI::DrawSubscribersUI(const std::vector<std::shared_ptr<postoffice::Address>>& subscribers)
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

void MQActorFollowUI::PerformUpdateTab(TabPage page)
{
	if (page == TabPage::Settings) DrawSettingsUI();
	else if (page == TabPage::Waypoints) DrawWaypointsUI(queueToVector(actorfollow::FollowController::Controller().GetPositionsCopy()));
	else if (page == TabPage::Subscribers) DrawSubscribersUI(actorfollow::SubscriptionController::Instance().GetSubscribers());
}

// -------------------- Main UI --------------------
void MQActorFollowUI::RenderUI()
{
	if (!showActorFollowUI) return;

	if (ImGui::Begin("MQActorFollow", &showActorFollowUI, 0))
	{
		if (ImGui::BeginTabBar("##main", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton | ImGuiTabBarFlags_FittingPolicyScroll))
		{
			for (int i = 0; i < static_cast<int>(TabPage::Max); ++i)
			{
				if (ImGui::BeginTabItem(s_tabNames[i]))
				{
					m_selectedTab = i;
					PerformUpdateTab(static_cast<TabPage>(m_selectedTab));
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}
		ImGui::End();
	}
}
