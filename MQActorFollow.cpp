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

const std::chrono::milliseconds UPDATE_TICK_MILLISECONDS = std::chrono::milliseconds(250);


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
			Dest.Int = static_cast<int>(actorfollow::FollowingState);
			return true;
		case MQActorFollowType::WaypointsCount:
			Dest.Type = mq::datatypes::pIntType;
			Dest.Int = actorfollow::Subscribers.size();
			return true;
		case MQActorFollowType::IsFollowing:
			Dest.DWord = (actorfollow::FollowingState == actorfollow::FollowState::ON);
			Dest.Type = mq::datatypes::pBoolType;
			return true;
		case MQActorFollowType::IsPaused:
			Dest.DWord = (actorfollow::FollowingState == actorfollow::FollowState::PAUSED);
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

void FollowCommandHandler(SPAWNINFO* pChar, char* szLine) {
	if (szLine && szLine[0] == '\0')
	{
		WriteChatf("[MQActorFollow] Usage:");
		WriteChatf("    /actfollow [character]");
		WriteChatf("    /actfollow [on|off|pause|resume]");
		WriteChatf("    /actfollow [ui] -- Toggle ui display");
		return;
	}

	char szArg1[MAX_STRING] = { 0 };
	GetArg(szArg1, szLine, 1);

	if (ci_equals(szArg1, "on")) {
		if (actorfollow::Subscribers.size()) {
			WriteChatf("[MQActorFollow] Im being followed so cannot follow others.");
			return;
		}
		else if (pTarget) {
			actorfollow::TrySubscribeFollowing(pTarget);
		}
		else {
			WriteChatf("[MQActorFollow] No target specified.");
			return;
		}
	}
	else if (ci_equals(szArg1, "off")) {
		actorfollow::EndFollowing();
	}
	else if (ci_equals(szArg1, "pause")) {
		if (actorfollow::FollowingState == actorfollow::FollowState::ON) {
			actorfollow::FollowingState = actorfollow::FollowState::PAUSED;
			WriteChatf("[MQActorFollow] Paused following.");
		}
	}
	else if (ci_equals(szArg1, "resume")) {
		if (actorfollow::FollowingState == actorfollow::FollowState::PAUSED) {
			actorfollow::FollowingState = actorfollow::FollowState::ON;
			WriteChatf("[MQActorFollow] Resumed following.");
		}
	}
	else if (ci_equals(szArg1, "ui")) {
		ShowMQActorFollowWindow = !ShowMQActorFollowWindow;
	}
	else if (ci_equals(szArg1, "id")) {
		auto spawnID = GetIntFromString(GetArg(szArg1, szLine, 2), 0);
		if (auto pSpawn = GetSpawnByID(spawnID)) {
			if (actorfollow::Subscribers.size()) {
				WriteChatf("[MQActorFollow] Im being followed so cannot follow others.");
				return;
			}

			actorfollow::TrySubscribeFollowing(pSpawn);
		}
		else {
			WriteChatf("[MQActorFollow] SpawnID not found \aw%s\ax.", spawnID);
			return;
		}
	}
	else {
		if (auto pSpawn = GetSpawnByName(szArg1)) {
			if (actorfollow::Subscribers.size()) {
				WriteChatf("[MQActorFollow] Im being followed so cannot follow others.");
				return;
			}

			actorfollow::TrySubscribeFollowing(pSpawn);
		}
		else {
			WriteChatf("[MQActorFollow] Character not found \aw%s\ax.", szArg1);
			return;
		}
	}
}

PLUGIN_API void InitializePlugin()
{
	DebugSpewAlways("[MQActorFollow]::Initializing version %.2f", MQ2Version);
	actorfollow::LoadSettings();
	actorfollow::ClearSubscribers();
	std::queue<std::shared_ptr<proto::actorfollowee::Position>>().swap(actorfollow::Positions);

	actorfollow::InitializeSubscription();

	AddCommand("/actfollow", FollowCommandHandler);
	pMQActorAdvPathType = new MQActorFollowType;
	AddMQ2Data("ActorFollow", DataActorFollow);

	WriteChatf("[MQActorFollow] \ayv%.2f\ax", MQ2Version);
}

PLUGIN_API void ShutdownPlugin()
{
	DebugSpewAlways("[MQActorFollow]:: Shutting down");
	actorfollow::CleanupSubscription();
	RemoveCommand("/actfollow");
	RemoveMQ2Data("ActorFollow");
	delete pMQActorAdvPathType;
}

PLUGIN_API void SetGameState(int gameState)
{
	if (gameState != GAMESTATE_INGAME) {
		actorfollow::ClearSubscribers();
		actorfollow::EndFollowing();
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
				actorfollow::SendPositionUpdate(pLocalPC);
			}
			actorfollow::TryFollowActor(pLocalPC);
		}
	}
}

PLUGIN_API bool OnIncomingChat(const char* Line, DWORD Color)
{
	if (!_stricmp(Line, "You have been summoned!") && actorfollow::FollowingState == actorfollow::FollowState::ON) {
		WriteChatf("[MQActorFollow]:: Summon detected. Breaking follow.");
		actorfollow::InterruptFollowing();
	}

	return false;
}

PLUGIN_API void OnUpdateImGui()
{
	if (GetGameState() == GAMESTATE_INGAME)
	{
		RenderUI();
	}
}