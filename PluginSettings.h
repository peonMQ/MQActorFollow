//
// PluginSettings.h
//

#pragma once

namespace actorfollow {

	struct SettingsData
	{
		// auto break navigation if keyboard input is received
		bool autobreak = true;

		// auto pause navigation if keyboard input is received
		bool autopause = false;

		// attempt to get unstuck
		bool attempt_unstuck = false;

		// open doors while navigation
		bool open_doors = true;

		// minimum distance in 3d from the given player for a waytpoint to be valid
		int waypoint_min_distance = 10;

		// distance for a warp alert to kick in
		int warp_alert_distance = 50;

		// max number of seconds to reach a waypoint
		int waypoint_timeout_seconds = 5;
	};
	SettingsData& GetSettings();

	// Load settings from the .ini file
	void LoadSettings(bool showMessage = false);

	// Save settings to the .ini file
	void SaveSettings(bool showMessage = false);

	// Parse a command to change settings
	//bool ParseIniCommand(const char* command);

	// Read a setting string into the buffer
	bool ReadIniSetting(const char* keyName, char* pBuffer, size_t length);

} // namespace actorfollow