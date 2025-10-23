#pragma once

#include <mq/Plugin.h>
#include "PluginSettings.h"

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

	// Interface functions
	void Move(MovementDirection dir, KeyAction action);
	void ReleaseKeys();
	void StopMoving();
	void TryJump(PcClient* pcClient);
	void MoveTowards(PcClient* pChar, CVector3 position);
}