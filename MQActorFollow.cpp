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

/**
 * Avoid Globals if at all possible, since they persist throughout your program.
 * But if you must have them, here is the place to put them.
 */
 // bool ShowMQActorAdvPathWindow = true;

static std::chrono::steady_clock::time_point PulseTimer = std::chrono::steady_clock::now();
static std::chrono::steady_clock::time_point OpenDoorTimer = std::chrono::steady_clock::now();
static std::chrono::steady_clock::time_point StuckTimer = std::chrono::steady_clock::now();

float m_stuckX = 0;
float m_stuckY = 0;

enum class FOLLOW {
	OFF = 0,
	ON = 1,
};

enum class STATUS {
	OFF = 0,
	ON = 1,
	PAUSED = 2
};


enum struct DoorState {
	Closed = 0,
	Open = 1,
	Opening = 2,
	Closing = 3
};

const int MIN_DISTANCE_BETWEEN_POINTS = 5;
const std::chrono::milliseconds UPDATE_TICK_MILLISECONDS = std::chrono::milliseconds(250);
FOLLOW FollowState = FOLLOW::OFF;
STATUS StatusState = STATUS::OFF;

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
			Dest.Int = static_cast<int>(StatusState);
			return true;
		case MQActorFollowType::WaypointsCount:
			Dest.Type = mq::datatypes::pIntType;
			Dest.Int = Subscribers.size();
			return true;
		case MQActorFollowType::IsFollowing:
			Dest.DWord = (FollowState == FOLLOW::ON);
			Dest.Type = mq::datatypes::pBoolType;
			return true;
		case MQActorFollowType::IsPaused:
			Dest.DWord = (StatusState == STATUS::PAUSED);
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

bool dataActorBots(const char* szIndex, MQTypeVar& Ret) {
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
		auto newposition = advPathMessage.position();
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

static void Post(std::string reciever, mq::proto::actorfollowee::MessageId messageId)
{
	subscription.Server = GetServerShortName();
	subscription.Character = reciever;
	Post(subscription, messageId, std::nullopt);
}

void SendPositionUpdate()
{
	if (!(GetCharInfo()->pSpawn && GetPcProfile())) {
		return;
	}

	if (auto pSpawn = GetCharInfo()->pSpawn) {
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

void StartFollowing(PlayerClient* pSpawn) {
	WriteChatf("[MQActorFollow] Following \ay%s\ax.", pSpawn->Name);
	Post(pSpawn->Name, proto::actorfollowee::MessageId::Subscribe);
	FollowState = FOLLOW::ON;
	StatusState = STATUS::ON;
}

void EndFollowing() {
	if (subscription.Character) {
		StopMoving();
		FollowState = FOLLOW::OFF;
		StatusState = STATUS::OFF;
		std::queue<std::shared_ptr<proto::actorfollowee::Position>>().swap(Positions);
		Post(subscription, proto::actorfollowee::MessageId::UnSubscribe);
		WriteChatf("[MQActorFollow] Stopped following \ay%s\ax.", subscription.Character.value().c_str());
		subscription.Server = std::nullopt;
		subscription.Character = std::nullopt;
	}
}

void FollowCommand(SPAWNINFO* pChar, char* szLine) {
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
		if (FollowState == FOLLOW::ON) {
			StatusState = STATUS::PAUSED;
			WriteChatf("[MQActorFollow] Paused following.");
		}
	}
	else if (ci_equals(szArg1, "resume")) {
		if (FollowState == FOLLOW::ON) {
			StatusState = STATUS::ON;
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

void LookAt(float x, float y, float z) {
	if (PCHARINFO pChar = GetCharInfo())
	{
		if (auto spawn = pChar->pSpawn)
		{
			gFaceAngle = (atan2(x - spawn->X, y - spawn->Y) * 256.0f / PI);

			if (gFaceAngle >= 512.0f) {
				gFaceAngle -= 512.0f;
			}

			if (gFaceAngle < 0.0f) {
				gFaceAngle += 512.0f;
			}

			pCharSpawn->Heading = (float)gFaceAngle;
			gFaceAngle = 10000.0f;

			if (spawn->FeetWet || spawn->UnderWater == 5) {
				spawn->CameraAngle = (float)(atan2(z - spawn->Z, (float)GetDistance(spawn->X, spawn->Y, x, y)) * 256.0f / PI);
			}
			else if (spawn->mPlayerPhysicsClient.Levitate == 2) {
				if (z < spawn->Z - 5) {
					spawn->CameraAngle = -45.0f;
				}
				else if (z > spawn->Z + 5) {
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



void StuckCheck()
{
	if (StatusState != STATUS::ON)
		return;

	auto now = std::chrono::steady_clock::now();
	if (now > StuckTimer) {
		return;
	}

	// check every 100 ms
	StuckTimer = now + std::chrono::milliseconds(100);

	if (GetCharInfo())
	{
		if (GetCharInfo()->pSpawn->SpeedMultiplier != -10000
			&& FindSpeed(GetCharInfo()->pSpawn)
			&& (GetDistance(m_stuckX, m_stuckY) < FindSpeed(GetCharInfo()->pSpawn) / 600)
			&& !GetCharInfo()->pSpawn->mPlayerPhysicsClient.Levitate
			&& !GetCharInfo()->pSpawn->UnderWater
			&& !GetCharInfo()->Stunned
			&& StatusState == STATUS::ON)
		{
			ExecuteCmd(CMD_JUMP, 1, 0);
			ExecuteCmd(CMD_JUMP, 0, 0);
		}

		m_stuckX = GetCharInfo()->pSpawn->X;
		m_stuckY = GetCharInfo()->pSpawn->Y;
	}
}

void TryFollowActor() {
	if (Positions.size() && StatusState == STATUS::ON) {
		auto pSpawn = GetCharInfo()->pSpawn;
		auto position = Positions.front();
		if (position->zoneid() == pSpawn->Zone) {
			auto distance3d = GetDistance3D(pSpawn->X, pSpawn->Y, pSpawn->Z, position->x(), position->y(), position->z());
			if (distance3d > 50) {
				WriteChatf("[MQActorFollow] Possible warp detected, exiting...");
				EndFollowing();
				return;
			}

			if (GetDistance(pSpawn->X, pSpawn->Y, position->x(), position->y()) > MIN_DISTANCE_BETWEEN_POINTS) {
				LookAt(position->x(), position->y(), position->z());
				MoveForward(true);
				AttemptToOpenDoor();
				StuckCheck();
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

/**
 * @fn InitializePlugin
 *
 * This is called once on plugin initialization and can be considered the startup
 * routine for the plugin.
 */
PLUGIN_API void InitializePlugin()
{
	DebugSpewAlways("[MQActorFollow]::Initializing version %.2f", MQ2Version);
	Subscribers.clear();
	std::queue<std::shared_ptr<proto::actorfollowee::Position>>().swap(Positions);

	DropBox = postoffice::AddActor(ReceivedMessageHandler);

	// Examples:
	AddCommand("/actfollow", FollowCommand);
	pMQActorAdvPathType = new MQActorFollowType;
	AddMQ2Data("ActorFollow", dataActorBots);

	WriteChatf("[MQActorFollow] \ayv%.2f\ax", MQ2Version);
}

/**
 * @fn ShutdownPlugin
 *
 * This is called once when the plugin has been asked to shutdown.  The plugin has
 * not actually shut down until this completes.
 */
PLUGIN_API void ShutdownPlugin()
{
	DebugSpewAlways("[MQActorFollow]:: Shutting down");
	DropBox.Remove();
	RemoveCommand("/actfollow");
	RemoveMQ2Data("ActorFollow");
	delete pMQActorAdvPathType;
	//ClearAll();
}

/**
 * @fn SetGameState
 *
 * This is called when the GameState changes.  It is also called once after the
 * plugin is initialized.
 *
 * For a list of known GameState values, see the constants that begin with
 * GAMESTATE_.  The most commonly used of these is GAMESTATE_INGAME.
 *
 * When zoning, this is called once after @ref OnBeginZone @ref OnRemoveSpawn
 * and @ref OnRemoveGroundItem are all done and then called once again after
 * @ref OnEndZone and @ref OnAddSpawn are done but prior to @ref OnAddGroundItem
 * and @ref OnZoned
 *
 * @param GameState int - The value of GameState at the time of the call
 */
PLUGIN_API void SetGameState(int GameState)
{
	// DebugSpewAlways("MQActorFollow::SetGameState(%d)", GameState);
	if (GameState == GAMESTATE_CHARSELECT) {
		Subscribers.clear();
		EndFollowing();
	}
}

/**
 * @fn OnPulse
 *
 * This is called each time MQ2 goes through its heartbeat (pulse) function.
 *
 * Because this happens very frequently, it is recommended to have a timer or
 * counter at the start of this call to limit the amount of times the code in
 * this section is executed.
 */
PLUGIN_API void OnPulse()
{
	if (GetGameState() == GAMESTATE_INGAME) {
		// Run only after timer is up
		if (std::chrono::steady_clock::now() > PulseTimer) {
			PulseTimer = std::chrono::steady_clock::now() + UPDATE_TICK_MILLISECONDS;
			SendPositionUpdate();
		}
		TryFollowActor();
	}
}

/**
 * @fn OnIncomingChat
 *
 * This is called each time a line of chat is shown.  It occurs after MQ filters
 * and chat events have been handled.  If you need to know when MQ2 has sent chat,
 * consider using @ref OnWriteChatColor instead.
 *
 * For a list of Color values, see the constants for USERCOLOR_. The default is
 * USERCOLOR_DEFAULT.
 *
 * @param Line const char* - The line of text that was shown
 * @param Color int - The type of chat text this was sent as
 *
 * @return bool - Whether to filter this chat from display
 */
PLUGIN_API bool OnIncomingChat(const char* Line, DWORD Color)
{
	if (!_stricmp(Line, "You have been summoned!") && FollowState == FOLLOW::ON) {
		WriteChatf("[MQActorFollow]:: Summon detected");
		EndFollowing();
	}

	return false;
}

/**
 * @fn OnAddSpawn
 *
 * This is called each time a spawn is added to a zone (ie, something spawns). It is
 * also called for each existing spawn when a plugin first initializes.
 *
 * When zoning, this is called for all spawns in the zone after @ref OnEndZone is
 * called and before @ref OnZoned is called.
 *
 * @param pNewSpawn PSPAWNINFO - The spawn that was added
 */
PLUGIN_API void OnAddSpawn(PSPAWNINFO pNewSpawn)
{
	// DebugSpewAlways("MQActorFollow::OnAddSpawn(%s)", pNewSpawn->Name);
}

/**
 * @fn OnRemoveSpawn
 *
 * This is called each time a spawn is removed from a zone (ie, something despawns
 * or is killed).  It is NOT called when a plugin shuts down.
 *
 * When zoning, this is called for all spawns in the zone after @ref OnBeginZone is
 * called.
 *
 * @param pSpawn PSPAWNINFO - The spawn that was removed
 */
PLUGIN_API void OnRemoveSpawn(PSPAWNINFO pSpawn)
{
	// DebugSpewAlways("MQActorFollow::OnRemoveSpawn(%s)", pSpawn->Name);
}

/**
 * @fn OnBeginZone
 *
 * This is called just after entering a zone line and as the loading screen appears.
 */
PLUGIN_API void OnBeginZone()
{
	// DebugSpewAlways("MQActorFollow::OnBeginZone()");
	//Subscribers.clear();
	//EndFollowing();
}

/**
 * @fn OnEndZone
 *
 * This is called just after the loading screen, but prior to the zone being fully
 * loaded.
 *
 * This should occur before @ref OnAddSpawn and @ref OnAddGroundItem are called. It
 * always occurs before @ref OnZoned is called.
 */
PLUGIN_API void OnEndZone()
{
	// DebugSpewAlways("MQActorFollow::OnEndZone()");
}

/**
 * @fn OnZoned
 *
 * This is called after entering a new zone and the zone is considered "loaded."
 *
 * It occurs after @ref OnEndZone @ref OnAddSpawn and @ref OnAddGroundItem have
 * been called.
 */
PLUGIN_API void OnZoned()
{
	// DebugSpewAlways("MQActorFollow::OnZoned()");
}