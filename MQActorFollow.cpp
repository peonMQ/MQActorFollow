// MQActorFollow.cpp : Defines the entry point for the DLL application.
//

// PLUGIN_API is only to be used for callbacks.  All existing callbacks at this time
// are shown below. Remove the ones your plugin does not use.  Always use Initialize
// and Shutdown for setup and cleanup.

#include <mq/Plugin.h>

#include "MQActorFollow.h"
using namespace mq::proto::actorfollowee;

PreSetup("MQActorFollow");
PLUGIN_VERSION(0.2);

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

static bool ShowMQActorFollowWindow = true;
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
			Dest.DWord = (gbInZone && pLocalPC && pLocalPC->pSpawn);
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

	mq::proto::actorfollowee::Message actorFollowMessage;
	actorFollowMessage.ParseFromString(*message->Payload);
	switch (actorFollowMessage.id())
	{
	case mq::proto::actorfollowee::MessageId::Subscribe:
		if (message->Sender) {
			if (!any_of(Subscribers.begin(), Subscribers.end(), [message](const std::shared_ptr<postoffice::Address> subscriber) { return subscriber->Character.value() == message->Sender->Character.value(); })) {
				Subscribers.push_back(std::make_shared<postoffice::Address>(message->Sender.value()));
				proto::actorfollowee::Message reply;
				reply.set_id(mq::proto::actorfollowee::MessageId::Subscribe);
				DropBox.PostReply(message, reply);
			}
		}
		break;
	case mq::proto::actorfollowee::MessageId::UnSubscribe:
		Subscribers.erase(std::remove_if(Subscribers.begin(), Subscribers.end(), [message](const std::shared_ptr<postoffice::Address> subscriber)
			{
				return subscriber->Character == message->Sender->Character;
			}), Subscribers.end());
		break;
	case mq::proto::actorfollowee::MessageId::PositionUpdate:
		if (!(pLocalPC->pSpawn && GetPcProfile())) {
			return;
		}

		auto newposition = actorFollowMessage.position();
		if (pLocalPC->pSpawn->Zone != newposition.zoneid()) {
			return;
		}

		if (Positions.empty()) {
			Positions.push(std::make_shared<mq::proto::actorfollowee::Position>(newposition));
		}
		else {
			auto& settings = actorfollow::GetSettings();
			auto previousposition = Positions.back();
			auto distanceToPreviousPoint = GetDistance3D(previousposition->x(), previousposition->y(), previousposition->z(), newposition.x(), newposition.y(), newposition.z());
			if (distanceToPreviousPoint > 0 && distanceToPreviousPoint > settings.waypoint_min_distance) {
				Positions.push(std::make_shared<mq::proto::actorfollowee::Position>(newposition));
			}
		}
		break;
	}
}

static void SetSubscription(std::string reciever) {
	WriteChatf("[MQActorFollow] Following \ay%s\ax.", reciever.c_str());
	subscription.Server = GetServerShortName();
	subscription.Character = reciever;
	FollowingState = FollowState::ON;
}

static void Post(postoffice::Address address, mq::proto::actorfollowee::MessageId messageId, const std::optional<proto::actorfollowee::Position>& data = std::nullopt, const std::function<void(int, const std::shared_ptr<postoffice::Message>&)>& callback)
{
	proto::actorfollowee::Message message;
	message.set_id(messageId);
	if (data) {
		*message.mutable_position() = *data;
	}

	DropBox.Post(address, message, callback);
}

void SendPositionUpdate(PcClient* pcClient)
{
	if (!pcClient || Subscribers.empty()) return;

	if (auto pSpawn = pcClient->pSpawn) {
		proto::actorfollowee::Position newposition;
		newposition.set_spawnid(pSpawn->SpawnID);
		newposition.set_name(pSpawn->Name);
		newposition.set_zoneid(pSpawn->Zone);
		newposition.set_x(pSpawn->X);
		newposition.set_y(pSpawn->Y);
		newposition.set_z(pSpawn->Z);
		newposition.set_heading(pSpawn->Heading);

		// We’ll copy the name once for safe capture in the lambda
		for (std::shared_ptr<postoffice::Address> subscriber : Subscribers) {
			// capture subscriber by value (safe if lambda runs async)
			auto subscriberPtr = subscriber;
			Post(*subscriberPtr,
				mq::proto::actorfollowee::MessageId::PositionUpdate,
				newposition,
				[subscriberPtr](int code, const std::shared_ptr<postoffice::Message>& reply) {
					if (code < 0) {
						WriteChatf("[MQActorFollow] Failed sending position update to \ay%s\ax>, removing subscriber.", subscriberPtr->Character.value().c_str());
						// Remove the subscriber whose Character matches nameCopy
						Subscribers.erase(std::remove_if(Subscribers.begin(), Subscribers.end(),
							[subscriberPtr](const std::shared_ptr<postoffice::Address> sub) {
								return sub->Character.value() == subscriberPtr->Character.value();
							}), Subscribers.end());
					}
				});
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

void TrySubscribeFollowing(PlayerClient* pSpawn) {
	postoffice::Address address;
	address.Server = GetServerShortName();
	address.Character = pSpawn->Name;
	Post(address, proto::actorfollowee::MessageId::Subscribe, std::nullopt, [](int code, const std::shared_ptr<postoffice::Message>& reply) {
		if (GetGameState() != GAMESTATE_INGAME) {
			return;
		}

		if (code < 0) {
			return;
		}

		mq::proto::actorfollowee::Message actorFollowMessage;
		actorFollowMessage.ParseFromString(*reply->Payload);
		switch (actorFollowMessage.id())
		{
		case mq::proto::actorfollowee::MessageId::Subscribe:
			if (reply->Sender && reply->Sender->Character.has_value()) {
				SetSubscription(reply->Sender->Character.value());
			}
			break;
		}
		});
}

void EndFollowing() {
	WriteChatf("[MQActorFollow] EndFollowing.");
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

void InterruptFollowing()
{
	auto& settings = actorfollow::GetSettings();
	if (settings.autobreak) 
	{
		EndFollowing();
	}
	else if (settings.autopause) 
	{
		FollowingState = FollowState::PAUSED;
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
			TrySubscribeFollowing(pTarget);
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

			TrySubscribeFollowing(pSpawn);
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

			TrySubscribeFollowing(pSpawn);
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
		&& !pcClient->Stunned
		&& FollowingState == FollowState::ON)
	{
		isStuck = true;
	}
	
	// check every 100 ms
	s_stuck_timer = now + std::chrono::milliseconds(100);
	previousX = playerClient->X;
	previousY = playerClient->Y;
	return isStuck;
}

std::shared_ptr<proto::actorfollowee::Position> GetNextDestination()
{
	static std::chrono::steady_clock::time_point s_same_position_timer = std::chrono::steady_clock::now();
	static std::shared_ptr<proto::actorfollowee::Position> position;

	if (Positions.empty()) {
		return nullptr;
	}

	auto newDestination = Positions.front();
	auto now = std::chrono::steady_clock::now();
	if (position == newDestination && now > s_same_position_timer) {
		WriteChatf("[MQActorFollow]::Timed out trying to reach waypoint, removing from queue");
		Positions.pop();
		return nullptr;
	}
	else if (position != newDestination) {
		auto& settings = actorfollow::GetSettings();
		s_same_position_timer = now + std::chrono::milliseconds(settings.waypoint_timeout_seconds * 1000);
		position = newDestination;
	}

	return position;
}

void PopDestination(bool forceStop) 
{
	Positions.pop();
	if (Positions.empty() || forceStop) {
		StopMoving();
	}
}

void TryFollowActor(PcClient* pcClient) {
	if (pcClient && FollowingState == FollowState::ON)
	{
		if (auto pSpawn = pcClient->pSpawn)
		{
			if (auto destination = GetNextDestination()) {
				auto& settings = actorfollow::GetSettings();
				if (destination->zoneid() == pSpawn->Zone) {
					auto position = CVector3{ destination->x(), destination->y(), destination->z() };
					auto distance3d = GetDistance3D(pSpawn->X, pSpawn->Y, pSpawn->Z, position.X, position.Y, position.Z);
					if (distance3d < 0 || distance3d > settings.warp_alert_distance) {
						WriteChatf("[MQActorFollow] Possible warp detected, exiting (\aw%.5f\ax)...", distance3d);
						PopDestination(true);
						return;
					}

					if (distance3d > settings.waypoint_min_distance) {
						LookAt(pcClient, position);
						MoveForward(true);
						AttemptToOpenDoor();
						if (IsStuck(pcClient)) {
							TryJump(pcClient);
						}
					}
					else {
						PopDestination(false);
					}
				}
			}
		}
	}
}

PLUGIN_API void InitializePlugin()
{
	DebugSpewAlways("[MQActorFollow]::Initializing version %.2f", MQ2Version);
	actorfollow::LoadSettings();
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
	EndFollowing();
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
	static std::chrono::steady_clock::time_point s_pulse_timer = std::chrono::steady_clock::now();
	if (GetGameState() == GAMESTATE_INGAME) {
		if (pLocalPC) {
			// Run only after timer is up
			if (std::chrono::steady_clock::now() > s_pulse_timer) {
				s_pulse_timer = std::chrono::steady_clock::now() + UPDATE_TICK_MILLISECONDS;
				SendPositionUpdate(pLocalPC);
			}
			TryFollowActor(pLocalPC);
		}
	}
}

PLUGIN_API bool OnIncomingChat(const char* Line, DWORD Color)
{
	if (!_stricmp(Line, "You have been summoned!") && FollowingState == FollowState::ON) {
		WriteChatf("[MQActorFollow]:: Summon detected. Breaking follow.");
		InterruptFollowing();
	}

	return false;
}

PLUGIN_API void OnUpdateImGui()
{
	if (GetGameState() == GAMESTATE_INGAME)
	{
		if (ShowMQActorFollowWindow) {
			RenderUI(&ShowMQActorFollowWindow, Subscribers, queueToVector(Positions));
		}
	}
}