#include "pch.h"
#include "RLTMBridge.h"

BAKKESMOD_PLUGIN(RLTMBridge, "RLTM Bridge", plugin_version, PLUGINTYPE_SPECTATOR)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

/*
	--- Boilerplate ---
*/

void RLTMBridge::onLoad()
{
	_globalCvarManager = cvarManager;

	// Required for ixwebsocket
	ix::initNetSystem();

	RegisterCvars();
	RegisterNotifiers();
	RegisterServerCommands();
	InitSocket();
	HookEvents();

	ApplyAutoSaveReplays();
	HideSpectatorHud(100);
	HideStatGraphs();
	JoinSpectators();

	ResetMatch();
	UpdateMatch();

	// The persistent cvars are loaded from the cfg file after 0.1s, wait for them before auto connecting
	gameWrapper->SetTimeout([this](GameWrapper*) {
		if (IsCvarEnabled(Cvars::AutoConnect)) Connect();
	}, 0.5f);

	LOG("RLTM Bridge v{} loaded", plugin_version);
}

void RLTMBridge::onUnload()
{
	UnhookEvents();

	// Clear the overlays before leaving
	ResetMatch();
	Disconnect();

	RestoreAutoSaveReplays();

	// Required for ixwebsocket
	ix::uninitNetSystem();

	LOG("RLTM Bridge unloaded");
}


/*
	--- Hooks ---
*/

void RLTMBridge::HookEvents()
{
	// Match data
	for (const char* eventName : {
		"Function TAGame.GameEvent_TA.EventPlayerAdded",
		"Function TAGame.GameEvent_TA.EventPlayerRemoved",
		"Function TAGame.PRI_TA.OnTeamChanged",
		"Function TAGame.Team_TA.OnScoreUpdated",
		"Function TAGame.GameEvent_Soccar_TA.OnGameTimeUpdated",
		"Function TAGame.GameEvent_Soccar_TA.OnOvertimeUpdated",
		"Function Engine.WorldInfo.EventPauseChanged"
	})
		Hook(eventName, [this]() { UpdateMatch(); });

	Hook("Function GameEvent_TA.Countdown.BeginState", [this]() {
		match.inPlay = false;
		ApplyReplayName();
		UpdateMatch(true);
		SendEvent("kickoff");
	});
	Hook("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", [this]() {
		// Last chance before the replay is saved, the name is only for this match
		if (ApplyReplayName()) match.replayName = GetReplayName();
		SetReplayName("");

		match.inPlay = false;
		match.resultPending = true; // Sent on the next tick, after the last goal (Game/GameEvents.cpp)
		UpdateMatch();
	});
	Hook("Function TAGame.GameEvent_Soccar_TA.OnBallHasBeenHit", [this]() {
		// First touch after a kickoff: start of the match, and the ball is in play (advanced statistics)
		match.inPlay = true;
		if (!match.started) UpdateMatch();
	});
	Hook("Function GameEvent_Soccar_TA.PodiumSpotlight.BeginState", [this]() { SendEvent("podium"); });

	// Goals (ball speed), the scorer and the assist come from the stat ticker
	gameWrapper->HookEventWithCaller<BallWrapper>("Function TAGame.Ball_TA.OnHitGoal",
		[this](BallWrapper ball, void* params, std::string eventName) { OnGoal(ball); });
	hookedEvents.push_back({ "Function TAGame.Ball_TA.OnHitGoal", false });
	Hook("Function TAGame.GameEvent_Soccar_TA.AddLocalPlayer", [this]() {
		JoinSpectators();
		UpdateMatch();
	});
	Hook("Function GameEvent_Soccar_TA.ReplayPlayback.BeginState", [this]() { SetReplayState(true); });
	Hook("Function GameEvent_Soccar_TA.ReplayPlayback.EndState", [this]() { SetReplayState(false); });
	Hook("Function TAGame.GameEvent_Soccar_TA.Destroyed", [this]() { ResetMatch(); });

	// Statistics events (goal, save, demolition, etc.)
	gameWrapper->HookEventWithCallerPost<ActorWrapper>("Function TAGame.GFxHUD_TA.HandleStatTickerMessage",
		[this](ActorWrapper caller, void* params, std::string eventName) { OnStatTicker(params); });
	hookedEvents.push_back({ "Function TAGame.GFxHUD_TA.HandleStatTickerMessage", true });

	// Entities data (balls, cars), throttled in OnTick
	Hook("Function Engine.GameViewportClient.Tick", [this]() { OnTick(); }, true);

	// Game actions
	Hook("Function TAGame.GFxHUD_Spectator_TA.InitGFx", [this]() { HideSpectatorHud(100); });
	Hook("Function TAGame.GFxHUD_Spectator_TA.CycleHUD", [this]() { HideSpectatorHud(0); });
	Hook("Function TAGame.StatGraphSystem_TA.GetDisplayGraphs", [this]() { HideStatGraphs(); });
	Hook("Function TAGame.GameEvent_Soccar_TA.BeginHighlightsReplay", [this]() { ReadyUp(); });
}

void RLTMBridge::UnhookEvents()
{
	for (const HookedEvent& event : hookedEvents)
	{
		if (event.post) gameWrapper->UnhookEventPost(event.name);
		else gameWrapper->UnhookEvent(event.name);
	}

	hookedEvents.clear();
}

void RLTMBridge::Hook(const std::string& eventName, std::function<void()> callback, bool post)
{
	auto hookCallback = [callback = std::move(callback)](std::string) { callback(); };

	if (post) gameWrapper->HookEventPost(eventName, hookCallback);
	else gameWrapper->HookEvent(eventName, hookCallback);

	hookedEvents.push_back({ eventName, post });
}
