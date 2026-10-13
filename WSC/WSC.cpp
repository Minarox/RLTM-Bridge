#include "pch.h"
#include "WSC.h"

BAKKESMOD_PLUGIN(WSC, "WebSocket Controller", plugin_version, PLUGINTYPE_SPECTATOR)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

/*
	--- Boilerplate ---
*/

void WSC::onLoad()
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

	LOG("WSC plugin v{} loaded", plugin_version);
}

void WSC::onUnload()
{
	UnhookEvents();

	// Clear the overlays before leaving
	ResetMatch();
	Disconnect();

	RestoreAutoSaveReplays();

	// Required for ixwebsocket
	ix::uninitNetSystem();

	LOG("WSC plugin unloaded");
}


/*
	--- Hooks ---
*/

void WSC::HookEvents()
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
		ApplyReplayName();
		UpdateMatch(true);
	});
	Hook("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", [this]() {
		// Last chance before the replay is saved, the name is only for this match
		ApplyReplayName();
		replayName.clear();
		UpdateMatch();
	});
	Hook("Function TAGame.GameEvent_Soccar_TA.OnBallHasBeenHit", [this]() {
		// Only needed to detect the start of the match, ignore the other touches
		if (!match.started) UpdateMatch();
	});
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

void WSC::UnhookEvents()
{
	for (const HookedEvent& event : hookedEvents)
	{
		if (event.post) gameWrapper->UnhookEventPost(event.name);
		else gameWrapper->UnhookEvent(event.name);
	}

	hookedEvents.clear();
}

void WSC::Hook(const std::string& eventName, std::function<void()> callback, bool post)
{
	auto hookCallback = [callback = std::move(callback)](std::string) { callback(); };

	if (post) gameWrapper->HookEventPost(eventName, hookCallback);
	else gameWrapper->HookEvent(eventName, hookCallback);

	hookedEvents.push_back({ eventName, post });
}
