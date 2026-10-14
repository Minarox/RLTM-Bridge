#include "pch.h"
#include "RLTMBridge.h"

/*
	--- Cvars ---
*/

void RLTMBridge::RegisterCvars()
{
	persistentStorage = std::make_shared<PersistentStorage>(this, "rltm_bridge", true, true);

	auto onSocketSettingChanged = [this](std::string oldValue, CVarWrapper cvar) { ConfigureSocket(); };

	// WebSocket
	persistentStorage->RegisterPersistentCvar(Cvars::Url, "ws://localhost:3000", "URL of the WebSocket server")
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
			// Restart the connection on the new URL
			if (socketRunning) Connect();
		});
	// Sent in the headers of the connection: restart the connection when they change
	auto onIdentityChanged = [this](std::string oldValue, CVarWrapper cvar) {
		ConfigureSocket();
		if (socketRunning) Connect();
	};
	persistentStorage->RegisterPersistentCvar(Cvars::ClientName, "", "Name of this client for the server (the name of the computer if empty)")
		.addOnValueChanged(onIdentityChanged);
	persistentStorage->RegisterPersistentCvar(Cvars::Token, "", "Token sent to the server to authenticate this client (optional)", false)
		.addOnValueChanged(onIdentityChanged);
	persistentStorage->RegisterPersistentCvar(Cvars::AutoConnect, "0", "Connect to the server when the plugin is loaded", true, true, 0, true, 1);
	persistentStorage->RegisterPersistentCvar(Cvars::AutoReconnect, "1", "Reconnect automatically to the server", true, true, 0, true, 1)
		.addOnValueChanged(onSocketSettingChanged);
	persistentStorage->RegisterPersistentCvar(Cvars::HandshakeTimeout, "3", "Handshake timeout (seconds)", true, true, 1)
		.addOnValueChanged(onSocketSettingChanged);
	persistentStorage->RegisterPersistentCvar(Cvars::MinWaitBetweenRetries, "500", "Minimum wait between reconnection retries (milliseconds)", true, true, 1)
		.addOnValueChanged(onSocketSettingChanged);
	persistentStorage->RegisterPersistentCvar(Cvars::MaxWaitBetweenRetries, "500", "Maximum wait between reconnection retries (milliseconds)", true, true, 1)
		.addOnValueChanged(onSocketSettingChanged);
	persistentStorage->RegisterPersistentCvar(Cvars::PerMessageDeflate, "1", "Compress the messages (per message deflate)", true, true, 0, true, 1)
		.addOnValueChanged(onSocketSettingChanged);
	persistentStorage->RegisterPersistentCvar(Cvars::PingInterval, "1", "Ping interval (seconds)", true, true, 1)
		.addOnValueChanged(onSocketSettingChanged);
	persistentStorage->RegisterPersistentCvar(Cvars::Debug, "0", "Log the messages sent to and received from the server (except the entities)", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { debug = cvar.getBoolValue(); });
	debug = IsCvarEnabled(Cvars::Debug);

	// Monitor: started or stopped when its settings change (and when they are loaded from the cfg file)
	auto onMonitorSettingChanged = [this](std::string oldValue, CVarWrapper cvar) { ApplyMonitorSettings(); };
	persistentStorage->RegisterPersistentCvar(Cvars::Monitor, "0", "Open a local WebSocket server (127.0.0.1) for the monitor page of the documentation", true, true, 0, true, 1)
		.addOnValueChanged(onMonitorSettingChanged);
	persistentStorage->RegisterPersistentCvar(Cvars::MonitorPort, "8765", "Port of the local monitor", true, true, 1024, true, 65535)
		.addOnValueChanged(onMonitorSettingChanged);

	// Server control
	persistentStorage->RegisterPersistentCvar(Cvars::ServerControl, "1", "Allow the server to send commands", true, true, 0, true, 1);
	persistentStorage->RegisterPersistentCvar(Cvars::ServerAllowCamera, "1", "Allow the server to move the spectator camera", true, true, 0, true, 1);
	persistentStorage->RegisterPersistentCvar(Cvars::ServerAllowActions, "1", "Allow the server to join the spectators, mark as ready and hide the HUD", true, true, 0, true, 1);
	persistentStorage->RegisterPersistentCvar(Cvars::ServerAllowReplayName, "1", "Allow the server to name the replays", true, true, 0, true, 1);
	persistentStorage->RegisterPersistentCvar(Cvars::ServerAllowSettings, "1", "Allow the server to change the game settings of the plugin", true, true, 0, true, 1);

	// Data, the overlays are cleared when a topic is disabled
	persistentStorage->RegisterPersistentCvar(Cvars::SendEntities, "1", "Send the balls and cars data", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { if (!cvar.getBoolValue()) Send(Topic::Entities, nullptr); });
	persistentStorage->RegisterPersistentCvar(Cvars::EntitiesInterval, "100", "Interval between two entities updates (milliseconds)", true, true, 16, true, 1000);
	persistentStorage->RegisterPersistentCvar(Cvars::SendSpectator, "1", "Send the player followed by the spectator camera", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { if (!cvar.getBoolValue()) Send(Topic::Spectator, nullptr); });
	persistentStorage->RegisterPersistentCvar(Cvars::SendStats, "1", "Send the advanced statistics (boost, supersonic, air time, etc.) every second", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { if (!cvar.getBoolValue()) Send(Topic::Stats, nullptr); });

	// Spectator
	auto onHudSettingChanged = [this](std::string oldValue, CVarWrapper cvar) {
		if (IsCvarEnabled(Cvars::HideHud)) HideSpectatorHud(0);
		else RestoreSpectatorHud();
	};
	persistentStorage->RegisterPersistentCvar(Cvars::AutoSpectate, "1", "Join the spectators when joining a match", true, true, 0, true, 1);
	persistentStorage->RegisterPersistentCvar(Cvars::HideHud, "1", "Hide the HUD when spectating", true, true, 0, true, 1)
		.addOnValueChanged(onHudSettingChanged);
	persistentStorage->RegisterPersistentCvar(Cvars::HudNames, "1", "Keep the players names when the HUD is hidden", true, true, 0, true, 1)
		.addOnValueChanged(onHudSettingChanged);
	persistentStorage->RegisterPersistentCvar(Cvars::HudMatchInfo, "1", "Keep the match info (score, clock) when the HUD is hidden", true, true, 0, true, 1)
		.addOnValueChanged(onHudSettingChanged);
	persistentStorage->RegisterPersistentCvar(Cvars::HideStatGraphs, "1", "Hide the engine statistics graphs (F10) when spectating", true, true, 0, true, 1);

	// Match end
	persistentStorage->RegisterPersistentCvar(Cvars::AutoReady, "1", "Mark as ready at the end of the match", true, true, 0, true, 1);
	persistentStorage->RegisterPersistentCvar(Cvars::AutoSaveReplays, "1", "Save the replay at the end of the match", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { ApplyAutoSaveReplays(); });
	persistentStorage->RegisterPersistentCvar(Cvars::ReplayNameFormat, "{blue} vs {orange} - Match {game}", "Name of the replays sent by the server, with {blue}, {orange} and {game}");
	persistentStorage->RegisterPersistentCvar(Cvars::UploadReplays, "1", "Send the replay of each match to the server", true, true, 0, true, 1);
	persistentStorage->RegisterPersistentCvar(Cvars::ReplaysFolder, "", "Folder of the replays of the game (Documents\\My Games\\Rocket League\\TAGame\\Demos if empty)");
}

CVarWrapper RLTMBridge::GetCvar(const char* name)
{
	return cvarManager->getCvar(name);
}

bool RLTMBridge::IsCvarEnabled(const char* name)
{
	CVarWrapper cvar = GetCvar(name);
	return cvar && cvar.getBoolValue();
}

void RLTMBridge::SetCvarFromUI(const char* name, const std::string& value)
{
	// The settings window is rendered on another thread, apply the change on the game thread
	gameWrapper->Execute([this, name, value](GameWrapper*) {
		CVarWrapper cvar = GetCvar(name);
		if (cvar) cvar.setValue(value);
	});
}


void RLTMBridge::ResetCvars()
{
	for (const char* name : Cvars::All)
	{
		CVarWrapper cvar = GetCvar(name);
		if (cvar) cvar.ResetToDefault();
	}

	LOG("Settings reset to the default values");
}


/*
	--- Notifiers ---
*/

void RLTMBridge::RegisterNotifiers()
{
	cvarManager->registerNotifier("rltm_version", [this](std::vector<std::string> args) {
		LOG("RLTM Bridge version: {}", plugin_version);
	}, "Show the plugin version", PERMISSION_ALL);

	cvarManager->registerNotifier("rltm_connect", [this](std::vector<std::string> args) {
		Connect();
	}, "Connect to the WebSocket server", PERMISSION_ALL);

	cvarManager->registerNotifier("rltm_disconnect", [this](std::vector<std::string> args) {
		Disconnect();
	}, "Disconnect from the WebSocket server", PERMISSION_ALL);
}
