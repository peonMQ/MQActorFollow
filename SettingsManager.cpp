#include "SettingsManager.h"
#include <mq/base/Config.h>
#include <mq/plugin/pluginapi.h>
#include <spdlog/spdlog.h>

namespace actorfollow {

	static const std::string SettingsSection = "Settings";

	static inline bool LoadBool(const std::string& name, bool defaultValue) {
		return mq::GetPrivateProfileBool(SettingsSection, name, defaultValue, INIFileName);
	}

	template <typename T>
	static inline T LoadNum(const std::string& name, T defaultValue) {
		return mq::GetPrivateProfileValue<T>(SettingsSection, name, defaultValue, INIFileName);
	}

	static inline void SaveBool(const std::string& name, bool value) {
		mq::WritePrivateProfileBool(SettingsSection, name, value, INIFileName);
	}

	template <typename T>
	static inline void SaveNum(const std::string& name, T value) {
		mq::WritePrivateProfileString(SettingsSection, name, std::to_string(value).c_str(), INIFileName);
	}

	void SettingsManager::Load(bool showMessage) {
		if (showMessage) SPDLOG_INFO("Loading settings...");

		SettingsData defaults;
		auto& s = settings;

		s.autobreak = LoadBool("AutoBreak", defaults.autobreak);
		s.autopause = LoadBool("AutoPause", defaults.autopause);
		s.attempt_unstuck = LoadBool("AttemptUnstuck", defaults.attempt_unstuck);
		s.open_doors = LoadBool("OpenDoors", defaults.open_doors);
		s.show_ui_on_startup = LoadBool("ShowUIOnStartup", defaults.show_ui_on_startup);
		s.waypoint_min_distance = LoadNum("WaypointMinDistance", defaults.waypoint_min_distance);
		s.warp_alert_distance = LoadNum("WarpAlertDistance", defaults.warp_alert_distance);
		s.waypoint_timeout_seconds = LoadNum("WaypointTimeout", defaults.waypoint_timeout_seconds);
	}

	void SettingsManager::Save(bool showMessage) const {
		if (showMessage) SPDLOG_INFO("Saving settings...");

		const auto& s = settings;
		SaveBool("AutoBreak", s.autobreak);
		SaveBool("AutoPause", s.autopause);
		SaveBool("AttemptUnstuck", s.attempt_unstuck);
		SaveBool("OpenDoors", s.open_doors);
		SaveBool("ShowUIOnStartup", s.show_ui_on_startup);
		SaveNum("WaypointMinDistance", s.waypoint_min_distance);
		SaveNum("WarpAlertDistance", s.warp_alert_distance);
		SaveNum("WaypointTimeout", s.waypoint_timeout_seconds);
	}

} // namespace actorfollow
