#include "pch.h"
#include "WSC.h"

namespace
{
	// BakkesMod cvar saving the replay at the end of every match
	constexpr const char* AUTO_SAVE_REPLAY_CVAR = "ranked_autosavereplay_all";

	// Graph level of the engine statistics graphs (F10) set when spectating
	constexpr int STAT_GRAPHS_LEVEL = 6;
}

/*
	--- Game replays ---
*/

void WSC::ApplyAutoSaveReplays()
{
	CVarWrapper autoSaveReplay = GetCvar(AUTO_SAVE_REPLAY_CVAR);
	if (!autoSaveReplay) return;

	// Keep the user value to restore it when the plugin is unloaded
	if (initialAutoSaveReplay.empty()) initialAutoSaveReplay = autoSaveReplay.getStringValue();

	autoSaveReplay.setValue(IsCvarEnabled(Cvars::AutoSaveReplays) ? "1" : initialAutoSaveReplay);
}

void WSC::RestoreAutoSaveReplays()
{
	if (initialAutoSaveReplay.empty()) return;

	CVarWrapper autoSaveReplay = GetCvar(AUTO_SAVE_REPLAY_CVAR);
	if (autoSaveReplay) autoSaveReplay.setValue(initialAutoSaveReplay);
}


/*
	--- Game HUD ---
*/

void WSC::HideSpectatorHud(int delay)
{
	if (!IsCvarEnabled(Cvars::HideHud)) return;

	ServerWrapper server = GetServerWrapper();
	if (!server) return;

	PlayerControllerWrapper primaryPlayer = server.GetLocalPrimaryPlayer();
	if (!primaryPlayer) return;

	PriWrapper player = primaryPlayer.GetPRI();
	if (!player || !player.IsSpectator()) return;

	// Wait for the spectator HUD to be initialized
	cvarManager->executeCommand("sleep " + std::to_string(delay) + "; sleep 16; replay_gui hud 0; replay_gui names 1; replay_gui matchinfo 1", false);
}

void WSC::HideStatGraphs()
{
	if (!IsCvarEnabled(Cvars::HideStatGraphs)) return;

	ServerWrapper server = GetServerWrapper();
	if (!server) return;

	EngineTAWrapper engine = gameWrapper->GetEngine();
	if (!engine) return;

	StatGraphSystemWrapper statGraphs = engine.GetStatGraphs();
	if (statGraphs) statGraphs.SetGraphLevel(STAT_GRAPHS_LEVEL);
}


/*
	--- Player actions ---
*/

void WSC::ReadyUp()
{
	if (!IsCvarEnabled(Cvars::AutoReady)) return;

	ServerWrapper server = GetServerWrapper();
	if (!server) return;

	PlayerControllerWrapper playerController = gameWrapper->GetPlayerController();
	if (!playerController) return;

	PriWrapper player = playerController.GetPRI();
	if (player) player.ServerReadyUp();
}

void WSC::JoinSpectators()
{
	if (!IsCvarEnabled(Cvars::AutoSpectate)) return;

	ServerWrapper server = GetServerWrapper();
	if (!server) return;

	PlayerControllerWrapper playerController = gameWrapper->GetPlayerController();
	if (!playerController) return;

	PriWrapper player = playerController.GetPRI();
	if (player && player.IsPlayer()) player.ServerSpectate();
}
