#include "MovementController.h"

namespace actorfollow {

	void MovementController::Move(MovementDirection dir, KeyAction action) {
		if (action == KeyAction::Press) {
			// If pressing a new direction, release the previous one
			if (isKeyHeld && currentDirection != dir) {
				Move(currentDirection, KeyAction::Release);
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
				default:
					break;
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
				default:
					break;
				}
				isKeyHeld = false;
				currentDirection = MovementDirection::None;
			}
		}
	}

	void MovementController::ReleaseKeys() {
		Move(MovementDirection::Forward, KeyAction::Release);
		Move(MovementDirection::Backward, KeyAction::Release);
		Move(MovementDirection::StrafeLeft, KeyAction::Release);
		Move(MovementDirection::StrafeRight, KeyAction::Release);
		isKeyHeld = false;
		currentDirection = MovementDirection::None;
	}

	void MovementController::StopMoving() {
		ReleaseKeys();
	}

	void MovementController::TryJump(PcClient* pcClient) {
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

	void MovementController::LookAt(PcClient* pChar, CVector3 position) {
		if (!pChar || !pChar->pSpawn) return;

		auto spawn = pChar->pSpawn;
		DoFace(spawn, position);

		pLocalPlayer->Heading = (float)gFaceAngle;
		gFaceAngle = 10000.0f;

		if (spawn->FeetWet || spawn->UnderWater == 5) {
			spawn->CameraAngle = (float)(atan2(position.Z - spawn->Z,
				(float)GetDistance(spawn->X, spawn->Y, position.X, position.Y)) * 256.0f / PI);
		}
		else if (spawn->mPlayerPhysicsClient.Levitate == 2) {
			if (position.Z < spawn->Z - 5) spawn->CameraAngle = -45.0f;
			else if (position.Z > spawn->Z + 5) spawn->CameraAngle = 45.0f;
			else spawn->CameraAngle = 0.0f;
		}
		else {
			spawn->CameraAngle = 0.0f;
		}

		gLookAngle = 10000.0f;
	}

	void MovementController::AttemptToOpenDoor() {
		const auto& settings = actorfollow::SettingsManager::Instance().Get();
		if (!settings.open_doors) return;

		static std::chrono::steady_clock::time_point openDoorTimer = std::chrono::steady_clock::now();

		if (GetPcProfile()->GetInventorySlot(InvSlot_Cursor)) return;

		auto now = std::chrono::steady_clock::now();
		if (now > openDoorTimer) return;

		openDoorTimer = now + std::chrono::milliseconds(500);

		auto pSwitch = FindSwitchByName();
		if (pSwitch && GetDistance(pSwitch->X, pSwitch->Y) < 25 &&
			(pSwitch->State == (BYTE)DoorState::Closed || pSwitch->State == (BYTE)DoorState::Closing))
		{
			pSwitch->UseSwitch(pLocalPC->pSpawn->SpawnID, 0xFFFFFFFF, 0, nullptr);
		}
	}

	bool MovementController::IsStuck(PcClient* pcClient) {
		const auto& settings = actorfollow::SettingsManager::Instance().Get();
		if (!settings.attempt_unstuck) return false;

		static std::chrono::steady_clock::time_point stuckTimer = std::chrono::steady_clock::now();
		static float previousX = 0;
		static float previousY = 0;

		auto now = std::chrono::steady_clock::now();
		if (now > stuckTimer) return false;

		auto playerClient = pcClient->pSpawn;
		auto isStuck = false;

		if (playerClient->SpeedMultiplier != -10000
			&& FindSpeed(playerClient)
			&& (GetDistance(previousX, previousY) <= FindSpeed(playerClient) / 600)
			&& !pcClient->Stunned)
		{
			isStuck = true;
		}

		stuckTimer = now + std::chrono::milliseconds(100);
		previousX = playerClient->X;
		previousY = playerClient->Y;
		return isStuck;
	}

	void MovementController::MoveTowards(PcClient* pcClient, CVector3 position) {
		LookAt(pcClient, position);
		Move(MovementDirection::Forward, KeyAction::Press);
		AttemptToOpenDoor();
		if (IsStuck(pcClient)) {
			TryJump(pcClient);
		}
	}
}
