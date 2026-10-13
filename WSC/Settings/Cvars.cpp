#include "pch.h"
#include "WSC.h"

/*
	--- Cvars ---
*/

void WSC::RegisterCvars()
{
	persistentStorage = std::make_shared<PersistentStorage>(this, "wsc", true, true);

	auto onSocketSettingChanged = [this](std::string oldValue, CVarWrapper cvar) { ConfigureSocket(); };

	// WebSocket
	persistentStorage->RegisterPersistentCvar(Cvars::Url, "ws://localhost:3000", "URL of the WebSocket server")
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
			// Restart the connection on the new URL
			if (socketRunning) Connect();
		});
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

	persistentStorage->RegisterPersistentCvar(Cvars::ServerControl, "1", "Allow the server to control the game (spectator camera, ready, etc.)", true, true, 0, true, 1);

	// Game
	persistentStorage->RegisterPersistentCvar(Cvars::EntitiesInterval, "100", "Interval between two entities updates (milliseconds)", true, true, 16);
	persistentStorage->RegisterPersistentCvar(Cvars::AutoSaveReplays, "1", "Save the replay at the end of the match", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) { ApplyAutoSaveReplays(); });
	persistentStorage->RegisterPersistentCvar(Cvars::ReplayNameFormat, "{blue} vs {orange} - Match {game}", "Name of the replays sent by the server, with {blue}, {orange} and {game}");
	persistentStorage->RegisterPersistentCvar(Cvars::AutoSpectate, "1", "Join the spectators when joining a match", true, true, 0, true, 1);
	persistentStorage->RegisterPersistentCvar(Cvars::AutoReady, "1", "Mark as ready at the end of the match", true, true, 0, true, 1);
	persistentStorage->RegisterPersistentCvar(Cvars::HideHud, "1", "Hide the HUD when spectating", true, true, 0, true, 1);
	persistentStorage->RegisterPersistentCvar(Cvars::HideStatGraphs, "1", "Hide the engine statistics graphs (F10) when spectating", true, true, 0, true, 1);
}

CVarWrapper WSC::GetCvar(const char* name)
{
	return cvarManager->getCvar(name);
}

bool WSC::IsCvarEnabled(const char* name)
{
	CVarWrapper cvar = GetCvar(name);
	return cvar && cvar.getBoolValue();
}

void WSC::SetCvarFromUI(const char* name, const std::string& value)
{
	// The settings window is rendered on another thread, apply the change on the game thread
	gameWrapper->Execute([this, name, value](GameWrapper*) {
		CVarWrapper cvar = GetCvar(name);
		if (cvar) cvar.setValue(value);
	});
}


/*
	--- Notifiers ---
*/

void WSC::RegisterNotifiers()
{
	cvarManager->registerNotifier("wsc_version", [this](std::vector<std::string> args) {
		LOG("WSC plugin version: {}", plugin_version);
	}, "Show the plugin version", PERMISSION_ALL);

	cvarManager->registerNotifier("wsc_connect", [this](std::vector<std::string> args) {
		Connect();
	}, "Connect to the WebSocket server", PERMISSION_ALL);

	cvarManager->registerNotifier("wsc_disconnect", [this](std::vector<std::string> args) {
		Disconnect();
	}, "Disconnect from the WebSocket server", PERMISSION_ALL);
}
