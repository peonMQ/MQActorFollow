#pragma once
#include <string>

namespace actorfollow {

	struct SettingsData
	{
		bool autobreak = true;
		bool autopause = false;
		bool attempt_unstuck = false;
		bool open_doors = true;
		int waypoint_min_distance = 10;
		int warp_alert_distance = 50;
		int waypoint_timeout_seconds = 5;
		bool show_ui_on_startup = false;
	};

	class SettingsManager
	{
	public:
		static SettingsManager& Instance() {
			static SettingsManager instance;
			return instance;
		}

		// Access current settings
		const SettingsData& Get() const { return settings; }
		SettingsData& Mutable() { return settings; }

		// Load/save
		void Load(bool showMessage = true);
		void Save(bool showMessage = true) const;

		// Reload from disk dynamically
		void Reload() {
			Load(false);
		}

	private:
		SettingsManager() = default;
		SettingsData settings;
	};

} // namespace actorfollow
