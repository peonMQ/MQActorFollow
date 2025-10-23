//
// PluginSettings.cpp
//

#include "PluginSettings.h"

#include <mq/base/Config.h>
#include <mq/plugin/pluginapi.h>
#include <spdlog/spdlog.h>

namespace actorfollow {

	SettingsData g_settings;
	static const std::string SettingsSection = "Settings";

	SettingsData& GetSettings()
	{
		return g_settings;
	}

	static inline bool LoadBoolSetting(const std::string& name, bool default)
	{
		return mq::GetPrivateProfileBool(SettingsSection, name, default, INIFileName);
	}

	template <typename T>
	static inline T LoadNumberSetting(const std::string& name, T defaultValue)
	{
		return mq::GetPrivateProfileValue<T>(SettingsSection, name, defaultValue, INIFileName);
	}

	static inline void SaveBoolSetting(const std::string& name, bool value)
	{
		mq::WritePrivateProfileBool(SettingsSection, name, value, INIFileName);
	}

	template <typename T>
	static inline void SaveNumberSetting(const std::string& name, T value)
	{
		mq::WritePrivateProfileString(SettingsSection, name, std::to_string(value).c_str(), INIFileName);
	}

	void LoadSettings(bool showMessage/* = true*/)
	{
		if (showMessage)
		{
			SPDLOG_INFO("Loading settings...");
		}

		SettingsData defaults, & settings = g_settings;

		settings.autobreak = LoadBoolSetting("AutoBreak", defaults.autobreak);
		settings.autopause = LoadBoolSetting("AutoPause", defaults.autopause);
		settings.attempt_unstuck = LoadBoolSetting("AttemptUnstuck", defaults.attempt_unstuck);
		settings.open_doors = LoadBoolSetting("OpenDoors", defaults.open_doors);
		settings.waypoint_min_distance = LoadNumberSetting("WaypointMinDistance", defaults.waypoint_min_distance);
		settings.warp_alert_distance = LoadNumberSetting("WarpAlertDistance", defaults.warp_alert_distance);
		settings.waypoint_timeout_seconds = LoadNumberSetting("WaypointTimeout", defaults.waypoint_timeout_seconds);
	}

	void SaveSettings(bool showMessage/* = true*/)
	{
		if (showMessage)
		{
			SPDLOG_INFO("Saving settings...");
		}

		// default settings
		SaveBoolSetting("AutoBreak", g_settings.autobreak);
		SaveBoolSetting("AutoPause", g_settings.autopause);
		SaveBoolSetting("AttemptUnstuck", g_settings.attempt_unstuck);
		SaveBoolSetting("OpenDoors", g_settings.open_doors);
		SaveNumberSetting("WaypointMinDistance", g_settings.waypoint_min_distance);
		SaveNumberSetting("WarpAlertDistance", g_settings.warp_alert_distance);
		SaveNumberSetting("WaypointTimeout", g_settings.waypoint_timeout_seconds);
	}

	bool ReadIniSetting(const char* keyName, char* pBuffer, size_t length)
	{
		return mq::GetPrivateProfileString("Settings", keyName, "", pBuffer, length, INIFileName) != 0;
	}

	//----------------------------------------------------------------------------

} // namespace actorfollow