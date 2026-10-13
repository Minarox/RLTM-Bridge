#include "pch.h"
#include "RLTMBridge.h"
#include "Game/GameUtils.h"

using namespace GameUtils;

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

void RLTMBridge::ApplyAutoSaveReplays()
{
	CVarWrapper autoSaveReplay = GetCvar(AUTO_SAVE_REPLAY_CVAR);
	if (!autoSaveReplay) return;

	// Keep the user value to restore it when the plugin is unloaded
	if (initialAutoSaveReplay.empty()) initialAutoSaveReplay = autoSaveReplay.getStringValue();

	autoSaveReplay.setValue(IsCvarEnabled(Cvars::AutoSaveReplays) ? "1" : initialAutoSaveReplay);
}

void RLTMBridge::RestoreAutoSaveReplays()
{
	if (initialAutoSaveReplay.empty()) return;

	CVarWrapper autoSaveReplay = GetCvar(AUTO_SAVE_REPLAY_CVAR);
	if (autoSaveReplay) autoSaveReplay.setValue(initialAutoSaveReplay);
}


std::string RLTMBridge::GetReplayName()
{
	std::lock_guard<std::mutex> lock(replayNameMutex);
	return replayName;
}

void RLTMBridge::SetReplayName(const std::string& name)
{
	std::lock_guard<std::mutex> lock(replayNameMutex);
	replayName = name;
}

// Gives its name to the replay recorded during the match, before it is saved at the end of the match
bool RLTMBridge::ApplyReplayName()
{
	const std::string name = GetReplayName();
	if (name.empty()) return false;

	ServerWrapper server = GetServerWrapper();
	if (!server) return false;

	ReplayDirectorWrapper replayDirector = server.GetReplayDirector();
	if (!replayDirector) return false;

	ReplaySoccarWrapper replay = replayDirector.GetReplay();
	if (!replay) return false;

	replay.SetReplayName(name);
	return true;
}


/*
	--- Game HUD ---
*/

// The actions return false when they can't be done (no match, not spectating, etc.)
// "force" ignores the cvar enabling the automatic action, used by the server commands

bool RLTMBridge::HideSpectatorHud(int delay, bool force)
{
	if (!force && !IsCvarEnabled(Cvars::HideHud)) return false;

	ServerWrapper server = GetServerWrapper();
	if (!server) return false;

	PlayerControllerWrapper primaryPlayer = server.GetLocalPrimaryPlayer();
	if (!primaryPlayer) return false;

	PriWrapper player = primaryPlayer.GetPRI();
	if (!player || !player.IsSpectator()) return false;

	// Wait for the spectator HUD to be initialized
	cvarManager->executeCommand(fmt::format("sleep {}; sleep 16; replay_gui hud 0; replay_gui names {}; replay_gui matchinfo {}",
		delay, IsCvarEnabled(Cvars::HudNames) ? 1 : 0, IsCvarEnabled(Cvars::HudMatchInfo) ? 1 : 0), false);
	return true;
}

bool RLTMBridge::RestoreSpectatorHud()
{
	ServerWrapper server = GetServerWrapper();
	if (!server) return false;

	PlayerControllerWrapper primaryPlayer = server.GetLocalPrimaryPlayer();
	if (!primaryPlayer) return false;

	PriWrapper player = primaryPlayer.GetPRI();
	if (!player || !player.IsSpectator()) return false;

	cvarManager->executeCommand("replay_gui hud 1; replay_gui names 1; replay_gui matchinfo 1", false);
	return true;
}

bool RLTMBridge::HideStatGraphs()
{
	if (!IsCvarEnabled(Cvars::HideStatGraphs)) return false;

	ServerWrapper server = GetServerWrapper();
	if (!server) return false;

	EngineTAWrapper engine = gameWrapper->GetEngine();
	if (!engine) return false;

	StatGraphSystemWrapper statGraphs = engine.GetStatGraphs();
	if (!statGraphs) return false;

	statGraphs.SetGraphLevel(STAT_GRAPHS_LEVEL);
	return true;
}


/*
	--- Player actions ---
*/

bool RLTMBridge::ReadyUp(bool force)
{
	if (!force && !IsCvarEnabled(Cvars::AutoReady)) return false;

	ServerWrapper server = GetServerWrapper();
	if (!server) return false;

	PlayerControllerWrapper playerController = gameWrapper->GetPlayerController();
	if (!playerController) return false;

	PriWrapper player = playerController.GetPRI();
	if (!player) return false;

	player.ServerReadyUp();
	return true;
}

bool RLTMBridge::JoinSpectators(bool force)
{
	if (!force && !IsCvarEnabled(Cvars::AutoSpectate)) return false;

	ServerWrapper server = GetServerWrapper();
	if (!server) return false;

	PlayerControllerWrapper playerController = gameWrapper->GetPlayerController();
	if (!playerController) return false;

	PriWrapper player = playerController.GetPRI();
	if (!player) return false;

	if (player.IsPlayer()) player.ServerSpectate();
	return true;
}


/*
	--- Spectator camera ---
*/

bool RLTMBridge::FocusPlayer(const std::string& uid, const std::string& name)
{
	ServerWrapper server = GetServerWrapper();
	if (!server) return false;

	CameraWrapper camera = gameWrapper->GetCamera();
	if (!camera) return false;

	for (PriWrapper player : server.GetPRIs())
	{
		if (!player || player.GetTeamNum() == NO_TEAM) continue;

		UnrealStringWrapper playerName = player.GetPlayerName();
		if (!playerName) continue;

		const bool sameUid = !uid.empty() && player.GetUniqueIdWrapper().GetIdString() == uid;
		const bool sameName = !name.empty() && playerName.ToString() == name;

		// Bots share the same uid, the name is required to tell them apart
		if ((uid.empty() || sameUid) && (name.empty() || sameName))
			return camera.SetFocusActor(playerName.ToString());
	}

	return false;
}
