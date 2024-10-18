// MQActorFollow.cpp : Defines the entry point for the DLL application.
//

// PLUGIN_API is only to be used for callbacks.  All existing callbacks at this time
// are shown below. Remove the ones your plugin does not use.  Always use Initialize
// and Shutdown for setup and cleanup.

#include <mq/Plugin.h>

#include "MQActorFollow.h"
using namespace mq::proto::actorfollowee;

PreSetup("MQActorFollow");
PLUGIN_VERSION(0.1);

enum class FollowState {
	OFF = 0,
	ON = 1,
	PAUSED = 2
};

enum class DoorState {
	Closed = 0,
	Open = 1,
	Opening = 2,
	Closing = 3
};

constexpr int MIN_DISTANCE_BETWEEN_POINTS = 10;
constexpr int WARP_ALERT_DISTANCE = 50;
const std::chrono::milliseconds UPDATE_TICK_MILLISECONDS = std::chrono::milliseconds(250);
FollowState FollowingState = FollowState::OFF;

postoffice::Address subscription;

class MQActorFollowType* pMQActorAdvPathType = nullptr;
class MQActorFollowType : public MQ2Type
{
public:
	enum Members {
		IsActive = 1,
		Status = 3,
		WaypointsCount = 4,
		IsFollowing = 5,
		IsPaused = 6,
	};

	MQActorFollowType() : MQ2Type("ActorFollow") {
		TypeMember(IsActive);
		TypeMember(Status);
		TypeMember(WaypointsCount);
		TypeMember(IsFollowing);
		TypeMember(IsPaused);
	}

	virtual bool GetMember(MQVarPtr VarPtr, const char* Member, char* Index, MQTypeVar& Dest) override {
		auto pMember = MQActorFollowType::FindMember(Member);
		if (!pMember)
			return false;

		switch ((Members)(pMember->ID))
		{
		case MQActorFollowType::IsActive:
			Dest.DWord = (gbInZone && GetCharInfo() && GetCharInfo()->pSpawn);
			Dest.Type = mq::datatypes::pBoolType;
			return true;
		case MQActorFollowType::Status:
			Dest.Type = mq::datatypes::pIntType;
			Dest.Int = static_cast<int>(FollowingState);
			return true;
		case MQActorFollowType::WaypointsCount:
			Dest.Type = mq::datatypes::pIntType;
			Dest.Int = Subscribers.size();
			return true;
		case MQActorFollowType::IsFollowing:
			Dest.DWord = (FollowingState == FollowState::ON);
			Dest.Type = mq::datatypes::pBoolType;
			return true;
		case MQActorFollowType::IsPaused:
			Dest.DWord = (FollowingState == FollowState::PAUSED);
			Dest.Type = mq::datatypes::pBoolType;
			return true;
		}

		return false;
	}

	bool ToString(MQVarPtr VarPtr, char* Destination) {
		strcpy_s(Destination, MAX_STRING, "ActorFollow");
		return true;
	}
};

bool DataActorFollow(const char* szIndex, MQTypeVar& Ret) {
	Ret.Type = pMQActorAdvPathType;
	return true;
}

void ReceivedMessageHandler(const std::shared_ptr<postoffice::Message>& message)
{
	if (GetGameState() != GAMESTATE_INGAME) {
		return;
	}

	mq::proto::actorfollowee::Message advPathMessage;
	advPathMessage.ParseFromString(*message->Payload);
	switch (advPathMessage.id())
	{
	case mq::proto::actorfollowee::MessageId::Subscribe:
		if (message->Sender) {
			if (!any_of(Subscribers.begin(), Subscribers.end(), [message](const auto& subscriber) { return subscriber->Character == message->Sender->Character; })) {
				Subscribers.push_back(std::make_shared<postoffice::Address>(message->Sender.value()));
			}
		}
		break;
	case mq::proto::actorfollowee::MessageId::UnSubscribe:
		Subscribers.erase(std::remove_if(Subscribers.begin(), Subscribers.end(), [message](const auto& subscriber)
			{
				return subscriber->Character == message->Sender->Character;
			}), Subscribers.end());
		break;
	case mq::proto::actorfollowee::MessageId::PositionUpdate:
		if (!(GetCharInfo()->pSpawn && GetPcProfile())) {
			return;
		}

		auto newposition = advPathMessage.position();
		if (GetCharInfo()->pSpawn->Zone != newposition.zoneid()) {
			return;
		}

		if (Positions.empty()) {
			Positions.push(std::make_shared<mq::proto::actorfollowee::Position>(newposition));
		}
		else {
			auto previousposition = Positions.back();
			if (GetDistance3D(previousposition->x(), previousposition->y(), previousposition->z(), newposition.x(), newposition.y(), newposition.z()) > MIN_DISTANCE_BETWEEN_POINTS) {
				Positions.push(std::make_shared<mq::proto::actorfollowee::Position>(newposition));
			}
		}
		break;
	}
}

static void Post(postoffice::Address address, mq::proto::actorfollowee::MessageId messageId, const std::optional<proto::actorfollowee::Position>& data = std::nullopt)
{
	proto::actorfollowee::Message message;
	message.set_id(messageId);
	if (data) {
		*message.mutable_position() = *data;
	}

	DropBox.Post(address, message);
}

static void SetSubscription(std::string reciever) {
	subscription.Server = GetServerShortName();
	subscription.Character = reciever;
}

void SendPositionUpdate(PcClient* pcClient)
{
	if (Subscribers.empty() || !pcClient) {
		return;
	}

	if (auto pSpawn = pcClient->pSpawn) {
		proto::actorfollowee::Position newposition;
		newposition.set_spawnid(pSpawn->SpawnID);
		newposition.set_name(pSpawn->Name);
		newposition.set_zoneid(pSpawn->Zone);
		newposition.set_x(pSpawn->X);
		newposition.set_y(pSpawn->Y);
		newposition.set_z(pSpawn->Z);
		newposition.set_heading(pSpawn->Heading);

		for (auto& subscriber : Subscribers) // access by reference to avoid copying
		{
			Post(*subscriber, mq::proto::actorfollowee::MessageId::PositionUpdate, newposition);
		}
	}
}

void MoveForward(bool hold) {
	static bool held = false;
	if (hold) {
		MoveBackwards(false);
		if (!held) ExecuteCmd(FindMappableCommand("forward"), 1, 0);
		held = true;
	}
	else {
		if (held) ExecuteCmd(FindMappableCommand("forward"), 0, 0);
		held = false;
	}
}

void MoveBackwards(bool hold) {
	static bool held = false;
	if (hold) {
		MoveForward(false);
		if (!held) ExecuteCmd(FindMappableCommand("back"), 1, 0);
		held = true;
	}
	else {
		if (held) ExecuteCmd(FindMappableCommand("back"), 0, 0);
		held = false;
	}
}

void StrafeLeft(bool hold) {
	static bool held = false;
	if (hold) {
		StrafeRight(false);
		if (!held) ExecuteCmd(FindMappableCommand("strafe_left"), 1, 0);
		held = true;
	}
	else {
		if (held) ExecuteCmd(FindMappableCommand("strafe_left"), 0, 0);
		held = false;
	}
}

void StrafeRight(bool hold) {
	static bool held = false;
	if (hold) {
		StrafeLeft(false);
		if (!held) ExecuteCmd(FindMappableCommand("strafe_right"), 1, 0);
		held = true;
	}
	else {
		if (held) ExecuteCmd(FindMappableCommand("strafe_right"), 0, 0);
		held = false;
	}
}

void ReleaseKeys() {
	MoveForward(false);
	MoveBackwards(false);
	StrafeRight(false);
	StrafeLeft(false);
}

void StopMoving() {
	MoveBackwards(true);
	ReleaseKeys();
}

void TryJump(PcClient* pcClient) {
	auto playerClient = pcClient->pSpawn;
	if (!playerClient->mPlayerPhysicsClient.Levitate
		&& !playerClient->UnderWater
		&& !pcClient->Stunned)
	{
		ExecuteCmd(CMD_JUMP, 1, 0);
		ExecuteCmd(CMD_JUMP, 0, 0);
	}
}

void StartFollowing(PlayerClient* pSpawn) {
	WriteChatf("[MQActorFollow] Following \ay%s\ax.", pSpawn->Name);
	SetSubscription(pSpawn->Name);
	Post(subscription, proto::actorfollowee::MessageId::Subscribe);
	FollowingState = FollowState::ON;
}

void EndFollowing() {
	if (subscription.Character) {
		StopMoving();
		FollowingState = FollowState::OFF;
		std::queue<std::shared_ptr<proto::actorfollowee::Position>>().swap(Positions);
		Post(subscription, proto::actorfollowee::MessageId::UnSubscribe);
		WriteChatf("[MQActorFollow] Stopped following \ay%s\ax.", subscription.Character.value().c_str());
		subscription.Server = std::nullopt;
		subscription.Character = std::nullopt;
	}
}

void FollowCommandHandler(SPAWNINFO* pChar, char* szLine) {
	if (szLine && szLine[0] == '\0')
	{
		WriteChatf("[MQActorFollow] Usage:");
		WriteChatf("    /actfollow [character]");
		WriteChatf("    /actfollow [on|off|pause|resume]");
		return;
	}

	char szArg1[MAX_STRING] = { 0 };
	GetArg(szArg1, szLine, 1);

	if (ci_equals(szArg1, "on")) {
		if (Subscribers.size()) {
			WriteChatf("[MQActorFollow] Im being followed so cannot follow others.");
			return;
		}
		else if (pTarget) {
			StartFollowing(pTarget);
		}
		else {
			WriteChatf("[MQActorFollow] No target specified.");
			return;
		}
	}
	else if (ci_equals(szArg1, "off")) {
		EndFollowing();
	}
	else if (ci_equals(szArg1, "pause")) {
		if (FollowingState == FollowState::ON) {
			FollowingState = FollowState::PAUSED;
			WriteChatf("[MQActorFollow] Paused following.");
		}
	}
	else if (ci_equals(szArg1, "resume")) {
		if (FollowingState == FollowState::PAUSED) {
			FollowingState = FollowState::ON;
			WriteChatf("[MQActorFollow] Resumed following.");
		}
	}
	else if (ci_equals(szArg1, "id")) {
		auto spawnID = GetIntFromString(GetArg(szArg1, szLine, 2), 0);
		if (auto pSpawn = GetSpawnByID(spawnID)) {
			if (Subscribers.size()) {
				WriteChatf("[MQActorFollow] Im being followed so cannot follow others.");
				return;
			}

			StartFollowing(pSpawn);
		}
		else {
			WriteChatf("[MQActorFollow] SpawnID not found \aw%s\ax.", spawnID);
			return;
		}
	}
	else {
		if (auto pSpawn = GetSpawnByName(szArg1)) {
			if (Subscribers.size()) {
				WriteChatf("[MQActorFollow] Im being followed so cannot follow others.");
				return;
			}

			StartFollowing(pSpawn);
		}
		else {
			WriteChatf("[MQActorFollow] Character not found \aw%s\ax.", szArg1);
			return;
		}
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
		pSwitch->UseSwitch(GetCharInfo()->pSpawn->SpawnID, 0xFFFFFFFF, 0, nullptr);
	}
}


bool IsStuck(PcClient* pcClient) {
	static std::chrono::steady_clock::time_point StuckTimer = std::chrono::steady_clock::now();
	static float previousX = 0;
	static float previousY = 0;

	auto now = std::chrono::steady_clock::now();
	if (now > StuckTimer) {
		return false;
	}

	auto playerClient = pcClient->pSpawn;
	auto isStuck = false;
	// check every 100 ms
	StuckTimer = now + std::chrono::milliseconds(100);

	if (playerClient->SpeedMultiplier != -10000
		&& FindSpeed(playerClient)
		&& (GetDistance(previousX, previousY) <= FindSpeed(playerClient) / 600)
		&& !pcClient->Stunned
		&& FollowingState == FollowState::ON)
	{
		isStuck = true;
	}

	previousX = playerClient->X;
	previousY = playerClient->Y;
	return isStuck;
}

std::shared_ptr<proto::actorfollowee::Position> GetNextDestination()
{
	if (Positions.empty()) {
		return nullptr;
	}

	return Positions.front();
}

void TryFollowActor(PcClient* pcClient) {
	if (pcClient && FollowingState == FollowState::ON)
	{
		if (auto pSpawn = pcClient->pSpawn)
		{
			if (auto destination = GetNextDestination()) {
				if (destination->zoneid() == pSpawn->Zone) {
					auto position = CVector3{ destination->x(), destination->y(), destination->z() };
					auto distance3d = GetDistance3D(pSpawn->X, pSpawn->Y, pSpawn->Z, position.X, position.Y, position.Z);
					if (distance3d < 0 || distance3d > WARP_ALERT_DISTANCE) {
						WriteChatf("[MQActorFollow] Possible warp detected, exiting (\aw%d\ax)...", distance3d);
						StopMoving();
						Positions.pop();
						return;
					}

					if (distance3d > MIN_DISTANCE_BETWEEN_POINTS) {
						LookAt(pcClient, position);
						MoveForward(true);
						AttemptToOpenDoor();
						if (IsStuck(pcClient)) {
							TryJump(pcClient);
						}
					}
					else {
						Positions.pop();
						if (Positions.empty()) {
							StopMoving();
						}
					}
				}
			}
		}
	}
}

PLUGIN_API void InitializePlugin()
{
	DebugSpewAlways("[MQActorFollow]::Initializing version %.2f", MQ2Version);
	Subscribers.clear();
	std::queue<std::shared_ptr<proto::actorfollowee::Position>>().swap(Positions);

	DropBox = postoffice::AddActor(ReceivedMessageHandler);

	AddCommand("/actfollow", FollowCommandHandler);
	pMQActorAdvPathType = new MQActorFollowType;
	AddMQ2Data("ActorFollow", DataActorFollow);

	WriteChatf("[MQActorFollow] \ayv%.2f\ax", MQ2Version);
}

PLUGIN_API void ShutdownPlugin()
{
	DebugSpewAlways("[MQActorFollow]:: Shutting down");
	DropBox.Remove();
	RemoveCommand("/actfollow");
	RemoveMQ2Data("ActorFollow");
	delete pMQActorAdvPathType;
}

PLUGIN_API void SetGameState(int gameState)
{
	if (gameState != GAMESTATE_INGAME) {
		Subscribers.clear();
		EndFollowing();
	}
}

PLUGIN_API void OnPulse()
{
	static std::chrono::steady_clock::time_point PulseTimer = std::chrono::steady_clock::now();
	if (GetGameState() == GAMESTATE_INGAME) {
		if (auto pcClient = GetCharInfo()) {
			// Run only after timer is up
			if (std::chrono::steady_clock::now() > PulseTimer) {
				PulseTimer = std::chrono::steady_clock::now() + UPDATE_TICK_MILLISECONDS;
				SendPositionUpdate(pcClient);
			}
			TryFollowActor(pcClient);
		}
	}
}

PLUGIN_API bool OnIncomingChat(const char* Line, DWORD Color)
{
	if (!_stricmp(Line, "You have been summoned!") && FollowingState == FollowState::ON) {
		WriteChatf("[MQActorFollow]:: Summon detected");
		EndFollowing();
	}

	return false;
}