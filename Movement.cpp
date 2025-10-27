
#pragma once
#include "Movement.h"

namespace actorfollow {

	static MovementDirection currentDirection = MovementDirection::None;
	static bool isKeyHeld = false; // tracks if any movement key is pressed

	void Move(MovementDirection dir, KeyAction action) {
		if (action == KeyAction::Press) {
			// If pressing a new direction, release the previous one
			if (isKeyHeld && currentDirection != dir) {
				switch (currentDirection) {
				case MovementDirection::Forward:
					ExecuteCmd(FindMappableCommand("forward"), 0, 0); break;
				case MovementDirection::Backward:
					ExecuteCmd(FindMappableCommand("back"), 0, 0); break;
				case MovementDirection::StrafeLeft:
					ExecuteCmd(FindMappableCommand("strafe_left"), 0, 0); break;
				case MovementDirection::StrafeRight:
					ExecuteCmd(FindMappableCommand("strafe_right"), 0, 0); break;
				}
			}

			// Press the new key if not already pressed
			if (!isKeyHeld || currentDirection != dir) {
				switch (dir) {
				case MovementDirection::Forward:
					ExecuteCmd(FindMappableCommand("forward"), 1, 0); break;
				case MovementDirection::Backward:
					ExecuteCmd(FindMappableCommand("back"), 1, 0); break;
				case MovementDirection::StrafeLeft:
					ExecuteCmd(FindMappableCommand("strafe_left"), 1, 0); break;
				case MovementDirection::StrafeRight:
					ExecuteCmd(FindMappableCommand("strafe_right"), 1, 0); break;
				}
				currentDirection = dir;
				isKeyHeld = true;
			}

		}
		else { // KeyAction::Release
			if (isKeyHeld && currentDirection == dir) {
				switch (dir) {
				case MovementDirection::Forward:
					ExecuteCmd(FindMappableCommand("forward"), 0, 0); break;
				case MovementDirection::Backward:
					ExecuteCmd(FindMappableCommand("back"), 0, 0); break;
				case MovementDirection::StrafeLeft:
					ExecuteCmd(FindMappableCommand("strafe_left"), 0, 0); break;
				case MovementDirection::StrafeRight:
					ExecuteCmd(FindMappableCommand("strafe_right"), 0, 0); break;
				}
				isKeyHeld = false;
			}
		}
	}

	void ReleaseKeys() {
		Move(MovementDirection::Forward, KeyAction::Release);
		Move(MovementDirection::Backward, KeyAction::Release);
		Move(MovementDirection::StrafeLeft, KeyAction::Release);
		Move(MovementDirection::StrafeRight, KeyAction::Release);
		isKeyHeld = false;
	}

	void StopMoving() {
		ReleaseKeys();
	}

	void TryJump(PcClient* pcClient) {
		if (!pcClient || !pcClient->pSpawn) return;

		auto playerClient = pcClient->pSpawn;
		if (!playerClient->mPlayerPhysicsClient.Levitate &&
			!playerClient->UnderWater &&
			!pcClient->Stunned)
		{
			ExecuteCmd(CMD_JUMP, 1, 0);
			ExecuteCmd(CMD_JUMP, 0, 0);
		}
	}

	void LookAt(PcClient* pChar, CVector3 position) {
		if (pChar)
		{
			if (auto spawn = pChar->pSpawn)
			{
				DoFace(spawn, position);

				pLocalPlayer->Heading = (float)gFaceAngle;
				gFaceAngle = 10000.0f;

				if (spawn->FeetWet || spawn->UnderWater == 5) {
					spawn->CameraAngle = (float)(atan2(position.Z - spawn->Z, (float)GetDistance(spawn->X, spawn->Y, position.X, position.Y)) * 256.0f / PI);
				}
				else if (spawn->mPlayerPhysicsClient.Levitate == 2) {
					if (position.Z < spawn->Z - 5) {
						spawn->CameraAngle = -45.0f;
					}
					else if (position.Z > spawn->Z + 5) {
						spawn->CameraAngle = 45.0f;
					}
					else {
						spawn->CameraAngle = 0.0f;
					}
				}
				else {
					spawn->CameraAngle = 0.0f;
				}

				gLookAngle = 10000.0f;
			}
		}
	}

	void AttemptToOpenDoor()
	{
		auto& settings = actorfollow::GetSettings();
		if (!settings.open_doors)
			return;

		static std::chrono::steady_clock::time_point OpenDoorTimer = std::chrono::steady_clock::now();

		// don't execute if we've got an item on the cursor.
		if (GetPcProfile()->GetInventorySlot(InvSlot_Cursor))
			return;

		auto now = std::chrono::steady_clock::now();
		if (now > OpenDoorTimer) {
			return;
		}

		OpenDoorTimer = now + std::chrono::milliseconds(500);

		auto pSwitch = FindSwitchByName();
		if (pSwitch && GetDistance(pSwitch->X, pSwitch->Y) < 25 && (pSwitch->State == (BYTE)DoorState::Closed || pSwitch->State == (BYTE)DoorState::Closing))
		{
			pSwitch->UseSwitch(pLocalPC->pSpawn->SpawnID, 0xFFFFFFFF, 0, nullptr);
		}
	}


	bool IsStuck(PcClient* pcClient)
	{
		auto& settings = actorfollow::GetSettings();
		if (!settings.attempt_unstuck)
			return false;

		static std::chrono::steady_clock::time_point s_stuck_timer = std::chrono::steady_clock::now();
		static float previousX = 0;
		static float previousY = 0;

		auto now = std::chrono::steady_clock::now();
		if (now > s_stuck_timer) {
			return false;
		}

		auto playerClient = pcClient->pSpawn;
		auto isStuck = false;

		if (playerClient->SpeedMultiplier != -10000
			&& FindSpeed(playerClient)
			&& (GetDistance(previousX, previousY) <= FindSpeed(playerClient) / 600)
			&& !pcClient->Stunned)
		{
			isStuck = true;
		}

		// check every 100 ms
		s_stuck_timer = now + std::chrono::milliseconds(100);
		previousX = playerClient->X;
		previousY = playerClient->Y;
		return isStuck;
	}

	void MoveTowards(PcClient* pcClient, CVector3 position) {
		LookAt(pcClient, position);
		Move(actorfollow::MovementDirection::Forward, actorfollow::KeyAction::Press);
		AttemptToOpenDoor();
		if (IsStuck(pcClient)) {
			TryJump(pcClient);
		}
	}
}