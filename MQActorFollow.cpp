#include <mq/Plugin.h>
#include "MQActorFollow.h"

using namespace mq::proto::actorfollowee;
using namespace actorfollow;

PreSetup("MQActorFollow");
PLUGIN_VERSION(0.2);

const std::chrono::milliseconds UPDATE_TICK_MILLISECONDS = std::chrono::milliseconds(250);

class MQActorFollowType* pMQActorAdvPathType = nullptr;

class MQActorFollowType : public MQ2Type
{
public:
    enum Members {
        IsActive = 1,
        Status = 2,
        WaypointsCount = 3,
        IsFollowing = 4,
        IsPaused = 5,
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

        auto& controller = FollowController::Controller();

        switch ((Members)(pMember->ID))
        {
        case MQActorFollowType::IsActive:
            Dest.DWord = (gbInZone && pLocalPC && pLocalPC->pSpawn);
            Dest.Type = mq::datatypes::pBoolType;
            return true;

        case MQActorFollowType::Status:
            Dest.Type = mq::datatypes::pIntType;
            Dest.Int = static_cast<int>(controller.GetState());
            return true;

        case MQActorFollowType::WaypointsCount:
            Dest.Type = mq::datatypes::pIntType;
            Dest.Int = controller.WaypointCount();
            return true;

        case MQActorFollowType::IsFollowing:
            Dest.DWord = (controller.GetState() == FollowState::ON);
            Dest.Type = mq::datatypes::pBoolType;
            return true;

        case MQActorFollowType::IsPaused:
            Dest.DWord = (controller.GetState() == FollowState::PAUSED);
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

// -------------------- Command Handler --------------------

void FollowCommandHandler(SPAWNINFO* pChar, char* szLine) {
    if (!szLine || szLine[0] == '\0') {
        WriteChatf("[MQActorFollow] Usage:");
        WriteChatf("    /actfollow [character]");
        WriteChatf("    /actfollow [on|off|pause|resume]");
        WriteChatf("    /actfollow [ui]");
        return;
    }

    char szArg1[MAX_STRING] = { 0 };
    GetArg(szArg1, szLine, 1);

    auto& controller = FollowController::Controller();

    if (ci_equals(szArg1, "on")) {
        if (actorfollow::SubscriptionController::Instance().HasSubscriptions()) {
            WriteChatf("[MQActorFollow] Im being followed so cannot follow others.");
            return;
        } else if (pTarget) {
            actorfollow::SubscriptionController::Instance().Subscribe(pTarget);
        } else {
            WriteChatf("[MQActorFollow] No target specified.");
        }
    }
    else if (ci_equals(szArg1, "off")) {
		actorfollow::SubscriptionController::Instance().Unsubscribe();
    }
    else if (ci_equals(szArg1, "pause")) {
        if (controller.GetState() == FollowState::ON) {
            controller.SetState(FollowState::PAUSED);
            WriteChatf("[MQActorFollow] Paused following.");
        }
    }
    else if (ci_equals(szArg1, "resume")) {
        if (controller.GetState() == FollowState::PAUSED) {
            controller.SetState(FollowState::ON);
            WriteChatf("[MQActorFollow] Resumed following.");
        }
    }
    else if (ci_equals(szArg1, "ui")) {
		if (MQActorFollowUI::Instance().IsVisible()) {
			MQActorFollowUI::Instance().Hide();
		}
		else {
			MQActorFollowUI::Instance().Show();
		}
    }
    else if (ci_equals(szArg1, "id")) {
        auto spawnID = GetIntFromString(GetArg(szArg1, szLine, 2), 0);
        if (auto pSpawn = GetSpawnByID(spawnID)) {
            if (actorfollow::SubscriptionController::Instance().HasSubscriptions()) {
                WriteChatf("[MQActorFollow] Im being followed so cannot follow others.");
                return;
            }
			actorfollow::SubscriptionController::Instance().Subscribe(pSpawn);
        } else {
            WriteChatf("[MQActorFollow] SpawnID not found \aw%s\ax.", spawnID);
        }
    }
    else {
        if (auto pSpawn = GetSpawnByName(szArg1)) {
            if (actorfollow::SubscriptionController::Instance().HasSubscriptions()) {
                WriteChatf("[MQActorFollow] Im being followed so cannot follow others.");
                return;
            }
			actorfollow::SubscriptionController::Instance().Subscribe(pSpawn);
        } else {
            WriteChatf("[MQActorFollow] Character not found \aw%s\ax.", szArg1);
        }
    }
}

// -------------------- Plugin Lifecycle --------------------

PLUGIN_API void InitializePlugin() {
    DebugSpewAlways("[MQActorFollow]::Initializing version %.2f", MQ2Version);
	actorfollow::SettingsManager::Instance().Load();
	actorfollow::SubscriptionController::Instance().Initialize();

    AddCommand("/actfollow", FollowCommandHandler);
    pMQActorAdvPathType = new MQActorFollowType;
    AddMQ2Data("ActorFollow", DataActorFollow);

    WriteChatf("[MQActorFollow] \ayv%.2f\ax", MQ2Version);
}

PLUGIN_API void ShutdownPlugin() {
    DebugSpewAlways("[MQActorFollow]:: Shutting down");
    actorfollow::SubscriptionController::Instance().Shutdown();
    RemoveCommand("/actfollow");
    RemoveMQ2Data("ActorFollow");
    delete pMQActorAdvPathType;
}

// Reset following if leaving the game
PLUGIN_API void SetGameState(int gameState) {
    if (gameState != GAMESTATE_INGAME) {
		actorfollow::SubscriptionController::Instance().CancelSubscriptions();
		actorfollow::SubscriptionController::Instance().Unsubscribe();
    }
}

// Tick-based updates
PLUGIN_API void OnPulse() {
    static std::chrono::steady_clock::time_point s_pulse_timer = std::chrono::steady_clock::now();

    if (GetGameState() != GAMESTATE_INGAME) return;
    if (!pLocalPC) return;

    auto now = std::chrono::steady_clock::now();
    if (now > s_pulse_timer) {
        s_pulse_timer = now + UPDATE_TICK_MILLISECONDS;
		actorfollow::SubscriptionController::Instance().SendUpdate(pLocalPC);
    }

    FollowController::Controller().TryFollowActor(pLocalPC, []() { SubscriptionController::Instance().Unsubscribe(); });
}

// Handle summon messages
PLUGIN_API bool OnIncomingChat(const char* Line, DWORD Color) {
    if (!_stricmp(Line, "You have been summoned!") && FollowController::Controller().GetState() == FollowState::ON)
    {
        WriteChatf("[MQActorFollow]:: Summon detected. Breaking follow.");
        FollowController::Controller().InterruptFollowing([]() { SubscriptionController::Instance().Unsubscribe(); });
    }
    return false;
}

// ImGui UI render
PLUGIN_API void OnUpdateImGui() {	
    if (GetGameState() == GAMESTATE_INGAME) {
        MQActorFollowUI::Instance().RenderUI();
    }
}
