#include <mq/Plugin.h>
#include "MQActorFollow.h"

using namespace mq::proto::actorfollow;
using namespace actorfollow;

PreSetup("MQActorFollow");
PLUGIN_VERSION(0.2);

const std::chrono::milliseconds UPDATE_TICK_MILLISECONDS = std::chrono::milliseconds(250);

class MQActorFollowType* pMQActorFollowType = nullptr;

actorfollow::SettingsManager* g_SettingsManager = nullptr;
actorfollow::FollowController* g_FollowController = nullptr;
actorfollow::SubscriptionController* g_SubscriptionController = nullptr;
MQActorFollowUI* g_MQActorFollowUI = nullptr;

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

        switch ((Members)(pMember->ID))
        {
        case MQActorFollowType::IsActive:
            Dest.DWord = (gbInZone && pLocalPC && pLocalPC->pSpawn);
            Dest.Type = mq::datatypes::pBoolType;
            return true;

        case MQActorFollowType::Status:
            Dest.Type = mq::datatypes::pIntType;
            Dest.Int = static_cast<int>(g_FollowController->GetState());
            return true;

        case MQActorFollowType::WaypointsCount:
            Dest.Type = mq::datatypes::pIntType;
            Dest.Int = g_FollowController->WaypointCount();
            return true;

        case MQActorFollowType::IsFollowing:
            Dest.DWord = (g_FollowController->GetState() == FollowState::ON);
            Dest.Type = mq::datatypes::pBoolType;
            return true;

        case MQActorFollowType::IsPaused:
            Dest.DWord = (g_FollowController->GetState() == FollowState::PAUSED);
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

bool DataRemote(const char* szIndex, MQTypeVar& Ret) {
    Ret.Type = pMQActorFollowType;
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

    if (ci_equals(szArg1, "on")) {
        if (g_SubscriptionController->HasSubscriptions()) {
            WriteChatf("[MQActorFollow] Im being followed so cannot follow others.");
            return;
        } else if (pTarget) {
            g_SubscriptionController->Subscribe(pTarget);
        } else {
            WriteChatf("[MQActorFollow] No target specified.");
        }
    }
    else if (ci_equals(szArg1, "off")) {
		g_SubscriptionController->Unsubscribe();
    }
    else if (ci_equals(szArg1, "pause")) {
        if (g_FollowController->GetState() == FollowState::ON) {
			g_FollowController->SetState(FollowState::PAUSED);
            WriteChatf("[MQActorFollow] Paused following.");
        }
    }
    else if (ci_equals(szArg1, "resume")) {
        if (g_FollowController->GetState() == FollowState::PAUSED) {
			g_FollowController->SetState(FollowState::ON);
            WriteChatf("[MQActorFollow] Resumed following.");
        }
    }
    else if (ci_equals(szArg1, "ui")) {
		if (g_MQActorFollowUI->IsVisible()) {
			g_MQActorFollowUI->Hide();
		}
		else {
			g_MQActorFollowUI->Show();
		}
    }
    else if (ci_equals(szArg1, "id")) {
        auto spawnID = GetIntFromString(GetArg(szArg1, szLine, 2), 0);
        if (auto pSpawn = GetSpawnByID(spawnID)) {
            if (g_SubscriptionController->HasSubscriptions()) {
                WriteChatf("[MQActorFollow] Im being followed so cannot follow others.");
                return;
            }
			g_SubscriptionController->Subscribe(pSpawn);
        } else {
            WriteChatf("[MQActorFollow] SpawnID not found \aw%s\ax.", spawnID);
        }
    }
    else {
        if (auto pSpawn = GetSpawnByName(szArg1)) {
            if (g_SubscriptionController->HasSubscriptions()) {
                WriteChatf("[MQActorFollow] Im being followed so cannot follow others.");
                return;
            }
			g_SubscriptionController->Subscribe(pSpawn);
        } else {
            WriteChatf("[MQActorFollow] Character not found \aw%s\ax.", szArg1);
        }
    }
}

// -------------------- Plugin Lifecycle --------------------

PLUGIN_API void InitializePlugin() {
    DebugSpewAlways("[MQActorFollow]::Initializing version %.2f", MQ2Version);
	g_SettingsManager = new actorfollow::SettingsManager();
	g_FollowController = new actorfollow::FollowController(g_SettingsManager);
	g_SubscriptionController = new actorfollow::SubscriptionController(g_SettingsManager, g_FollowController);
	g_MQActorFollowUI = new MQActorFollowUI(g_SettingsManager, g_SubscriptionController, g_FollowController);

	g_SettingsManager->Load();
	g_SubscriptionController->Initialize();

    AddCommand("/actfollow", FollowCommandHandler);
	pMQActorFollowType = new MQActorFollowType;
    AddMQ2Data("ActorFollow", DataRemote);
}

PLUGIN_API void ShutdownPlugin() {
    DebugSpewAlways("[MQActorFollow]:: Shutting down");
    g_SubscriptionController->Shutdown();
    RemoveCommand("/actfollow");
    RemoveMQ2Data("ActorFollow");
    delete pMQActorFollowType;
	delete g_MQActorFollowUI;
	delete g_SubscriptionController;
	delete g_FollowController;
	delete g_SettingsManager;
}

// Reset following if leaving the game
PLUGIN_API void SetGameState(int gameState) {
    if (gameState != GAMESTATE_INGAME) {
		g_SubscriptionController->CancelSubscriptions();
		g_SubscriptionController->Unsubscribe();
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
		g_SubscriptionController->SendUpdate(pLocalPC);
    }

	g_FollowController->TryFollowActor(pLocalPC, []() { g_SubscriptionController->Unsubscribe(); });
}

// Handle summon messages
PLUGIN_API bool OnIncomingChat(const char* Line, DWORD Color) {
    if (!_stricmp(Line, "You have been summoned!") && g_FollowController->GetState() == FollowState::ON)
    {
        WriteChatf("[MQActorFollow]:: Summon detected. Breaking follow.");
		g_FollowController->InterruptFollowing([]() { g_SubscriptionController->Unsubscribe(); });
    }
    return false;
}

// ImGui UI render
PLUGIN_API void OnUpdateImGui() {	
    if (GetGameState() == GAMESTATE_INGAME) {
		g_MQActorFollowUI->RenderUI();
    }
}
