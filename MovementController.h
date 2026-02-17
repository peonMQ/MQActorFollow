#pragma once
#include <mq/Plugin.h>
#include "SettingsManager.h"

namespace actorfollow {

	enum class MovementDirection {
		None,
		Forward,
		Backward,
		StrafeLeft,
		StrafeRight
	};

	enum class KeyAction {
		Press,
		Release
	};

	enum class DoorState {
		Closed = 0,
		Open = 1,
		Opening = 2,
		Closing = 3
	};

	class MovementController {
	public:
		MovementController(actorfollow::SettingsManager& settingsManager) : m_settingsManager(settingsManager)
		{
		}

		void Move(MovementDirection dir, KeyAction action);
		void ReleaseKeys();
		void StopMoving();
		void TryJump(PcClient* pcClient);
		void MoveTowards(PcClient* pcClient, CVector3 position);

	private:
		MovementController() = default;
		void LookAt(PcClient* pChar, CVector3 position);
		void AttemptToOpenDoor();
		bool IsStuck(PcClient* pcClient);

		MovementDirection currentDirection = MovementDirection::None;
		bool isKeyHeld = false;
		actorfollow::SettingsManager m_settingsManager;
	};

} // namespace actorfollow
