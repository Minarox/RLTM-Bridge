#include "pch.h"
#include "RLTMBridge.h"

#include <algorithm>

#include "fmt/chrono.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#endif

namespace
{
	const ImVec4 COLOR_GREEN = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
	const ImVec4 COLOR_YELLOW = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
	const ImVec4 COLOR_RED = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
	const ImVec4 COLOR_GREY = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

	// Monitor page of the documentation, connecting to the local monitor of the plugin
	const std::string MONITOR_PAGE_URL = "https://minarox.github.io/RLTM-Bridge/monitor/";

	void OpenInBrowser(const std::string& url)
	{
#ifdef _WIN32
		ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
	}

	void RenderTooltip(const char* tooltip)
	{
		if (tooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
	}

	// Explanation of a section, wrapped to the window width
	void RenderHelp(const char* text)
	{
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextDisabled("%s", text);
		ImGui::PopTextWrapPos();
	}
}

/*
	--- Settings window ---
	F2 > Plugins > RLTM Bridge
	Rendered on the render thread: cvars and plugin state are only modified through gameWrapper->Execute
*/

void RLTMBridge::RenderSettings()
{
	ImGui::TextUnformatted("RLTM Bridge");
	ImGui::SameLine();
	ImGui::TextDisabled("v%s (protocol %d) - by @Minarox", plugin_version, protocol_version);

	ImGui::Separator();
	RenderConnection();
	ImGui::Separator();

	if (ImGui::CollapsingHeader("Status", ImGuiTreeNodeFlags_DefaultOpen)) RenderStatus();
	if (ImGui::CollapsingHeader("Data sent to the server", ImGuiTreeNodeFlags_DefaultOpen)) RenderDataSettings();
	if (ImGui::CollapsingHeader("Spectator", ImGuiTreeNodeFlags_DefaultOpen)) RenderSpectatorSettings();
	if (ImGui::CollapsingHeader("End of the match", ImGuiTreeNodeFlags_DefaultOpen)) RenderMatchEndSettings();
	if (ImGui::CollapsingHeader("Server control")) RenderServerControlSettings();
	if (ImGui::CollapsingHeader("Monitor")) RenderMonitorSettings();
	if (ImGui::CollapsingHeader("Advanced WebSocket settings")) RenderWebSocketSettings();
	if (ImGui::CollapsingHeader("Reset")) RenderResetSettings();
}

void RLTMBridge::RenderConnection()
{
	RenderCvarInputText("Server URL", Cvars::Url, urlInput, "WebSocket server URL, e.g. ws://localhost:3000 or wss://example.com");
	RenderCvarInputText("Client name", Cvars::ClientName, clientNameInput, "Name of this computer for the server, to tell the matches apart when several computers are connected (the name of the computer if empty)");
	RenderCvarInputText("Token", Cvars::Token, tokenInput, "Optional token given by the server to authenticate this client (sent in the Authorization header)", ImGuiInputTextFlags_Password);
	RenderCvarCheckbox("Connect when the plugin is loaded", Cvars::AutoConnect, nullptr);

	// Connection state
	const ix::ReadyState state = socket.getReadyState();
	const bool running = socketRunning;

	if (state == ix::ReadyState::Closed && !running)
	{
		if (ImGui::Button("Connect")) gameWrapper->Execute([this](GameWrapper*) { Connect(); });
	}
	else if (ImGui::Button("Disconnect")) gameWrapper->Execute([this](GameWrapper*) { Disconnect(); });

	ImGui::SameLine();
	switch (state)
	{
		case ix::ReadyState::Open:
			ImGui::TextColored(COLOR_GREEN, "Connected");
			break;
		case ix::ReadyState::Connecting:
			ImGui::TextColored(COLOR_YELLOW, "Connecting...");
			break;
		case ix::ReadyState::Closing:
			ImGui::TextColored(COLOR_YELLOW, "Disconnecting...");
			break;
		case ix::ReadyState::Closed:
			if (running) ImGui::TextColored(COLOR_YELLOW, "Reconnecting...");
			else ImGui::TextColored(COLOR_GREY, "Disconnected");
			break;
	}

	std::string lastError;
	std::time_t since;
	{
		std::lock_guard<std::mutex> lock(socketStatusMutex);
		lastError = socketLastError;
		since = std::chrono::system_clock::to_time_t(socketStatusSince);
	}

	ImGui::SameLine();
	ImGui::TextDisabled("since %s", fmt::format("{:%H:%M:%S}", fmt::localtime(since)).c_str());

	if (!lastError.empty() && state != ix::ReadyState::Open)
		ImGui::TextColored(COLOR_RED, "Error: %s", lastError.c_str());
}

void RLTMBridge::RenderStatus()
{
	ImGui::Text("Messages sent: %llu (%.1f KB)", static_cast<unsigned long long>(messagesSent), bytesSent / 1024.0);
	ImGui::SameLine();
	if (ImGui::Button("Resend the state")) gameWrapper->Execute([this](GameWrapper*) { ResendState(); });
	RenderTooltip("Send every state (match, scoreboard, statistics, spectator, players, entities, result) to the server again");

	std::string command;
	{
		std::lock_guard<std::mutex> lock(lastCommandMutex);
		command = lastCommand;
	}
	ImGui::Text("Commands received: %llu", static_cast<unsigned long long>(commandsReceived));
	if (!command.empty())
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(last: %s)", command.c_str());
	}

	const std::string name = GetReplayName();
	if (name.empty()) ImGui::TextDisabled("Next replay name: default name of the game");
	else
	{
		ImGui::Text("Next replay name: %s", name.c_str());
		ImGui::SameLine();
		if (ImGui::Button("Clear")) gameWrapper->Execute([this](GameWrapper*) { SetReplayName(""); });
		RenderTooltip("Keep the default name of the game for the replay of this match");
	}
}

void RLTMBridge::RenderDataSettings()
{
	RenderHelp("The match, scoreboard, players, events and result are always sent. Disabling a data clears it on the server.");

	if (RenderCvarCheckbox("Send the balls and cars data", Cvars::SendEntities, "Speed, location, boost, etc. while the match is playing (minimap, boost gauges)"))
	{
		ImGui::Indent();
		RenderCvarSliderInt("Update interval", Cvars::EntitiesInterval, entitiesIntervalInput, 16, 1000, "%d ms", "Interval between two updates, lower is smoother but sends more messages");
		ImGui::Unindent();
	}

	RenderCvarCheckbox("Send the advanced statistics", Cvars::SendStats, "Boost used and collected, time supersonic, on the ground, on the wall and in the air, average speed (every second, always in the result)");
	RenderCvarCheckbox("Send the player followed by the camera", Cvars::SendSpectator, "Player watched by the spectator camera, nothing with the free camera (dynamic player overlay)");
}

void RLTMBridge::RenderSpectatorSettings()
{
	RenderCvarCheckbox("Join the spectators", Cvars::AutoSpectate, "Join the spectators when joining a match");

	if (RenderCvarCheckbox("Hide the HUD", Cvars::HideHud, "Hide the HUD of the game when spectating, to show the overlays instead"))
	{
		ImGui::Indent();
		RenderCvarCheckbox("Keep the players names", Cvars::HudNames, "Names above the cars");
		RenderCvarCheckbox("Keep the match info", Cvars::HudMatchInfo, "Score and clock at the top of the screen");
		ImGui::Unindent();
	}

	RenderCvarCheckbox("Hide the statistics graphs", Cvars::HideStatGraphs, "Hide the engine statistics graphs (F10)");
}

void RLTMBridge::RenderMatchEndSettings()
{
	RenderCvarCheckbox("Mark as ready", Cvars::AutoReady, "Mark as ready at the end of the match");

	if (RenderCvarCheckbox("Save the replay", Cvars::AutoSaveReplays, "Save the replay of every match"))
	{
		ImGui::Indent();
		RenderCvarInputText("Replay name", Cvars::ReplayNameFormat, replayNameFormatInput,
			"Used when the server sends the teams of the match: {blue}, {orange} and {game} are replaced by the blue team, the orange team and the game number of the series");
		ImGui::TextDisabled("Preview: %s", BuildReplayName("Blue Team", "Orange Team", 1).c_str());
		ImGui::Unindent();
	}
}

void RLTMBridge::RenderServerControlSettings()
{
	RenderHelp("The server can send commands to the game. Only the commands below can be executed, an invalid command is rejected.");

	if (RenderCvarCheckbox("Allow the server to send commands", Cvars::ServerControl, "Disable to ignore every command of the server"))
	{
		ImGui::Indent();
		RenderCvarCheckbox("Spectator camera", Cvars::ServerAllowCamera, "Focus the camera on a player (focus_player)");
		RenderCvarCheckbox("Player actions", Cvars::ServerAllowActions, "Join the spectators, mark as ready and hide the HUD (spectate, ready, hide_hud)");
		RenderCvarCheckbox("Replay names", Cvars::ServerAllowReplayName, "Name the replays with the teams of the match (set_replay_name, clear_replay_name)");
		RenderCvarCheckbox("Game settings", Cvars::ServerAllowSettings, "Change the data, spectator and end of the match settings, never the connection nor the server control (set_setting)");
		ImGui::Unindent();
	}
}

void RLTMBridge::RenderMonitorSettings()
{
	RenderHelp("Shows the messages of the plugin and sends commands from a web page, without a server. "
		"The plugin opens a WebSocket server only reachable from this computer (127.0.0.1), for the monitor page of the documentation and the local pages.");

	if (!RenderCvarCheckbox("Enable the monitor", Cvars::Monitor, "Open the local WebSocket server of the monitor")) return;

	ImGui::Indent();
	RenderCvarInputInt("Port", Cvars::MonitorPort, "Port of the local monitor, 8765 by default (restarts the monitor)");

	const std::string error = GetMonitorError();
	CVarWrapper port = GetCvar(Cvars::MonitorPort);
	const int portValue = port ? port.getIntValue() : 8765;

	if (!error.empty()) ImGui::TextColored(COLOR_RED, "Error: %s", error.c_str());
	else ImGui::TextColored(COLOR_GREEN, "Listening on ws://127.0.0.1:%d (%zu page(s) connected)", portValue, GetMonitorClientsCount());

	if (ImGui::Button("Open the monitor page")) OpenInBrowser(MONITOR_PAGE_URL + "?port=" + std::to_string(portValue));
	RenderTooltip("Opens the monitor page of the documentation in the browser");
	ImGui::Unindent();
}

void RLTMBridge::RenderWebSocketSettings()
{
	RenderCvarCheckbox("Reconnect automatically", Cvars::AutoReconnect, "Reconnect to the server when the connection is lost");
	RenderCvarCheckbox("Compress messages", Cvars::PerMessageDeflate, "Per message deflate, applied on the next connection");
	RenderCvarInputInt("Handshake timeout (s)", Cvars::HandshakeTimeout, "Applied on the next connection");
	RenderCvarInputInt("Ping interval (s)", Cvars::PingInterval, "Applied on the next connection");
	RenderCvarInputInt("Min wait between retries (ms)", Cvars::MinWaitBetweenRetries, nullptr);
	RenderCvarInputInt("Max wait between retries (ms)", Cvars::MaxWaitBetweenRetries, nullptr);
	RenderCvarCheckbox("Debug mode", Cvars::Debug, "Log the messages sent to and received from the server in the BakkesMod console (F6), except the entities");
}

void RLTMBridge::RenderResetSettings()
{
	if (ImGui::Button("Reset all the settings")) ImGui::OpenPopup("Reset the settings");

	if (ImGui::BeginPopupModal("Reset the settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Reset every setting of the plugin to its default value?");

		if (ImGui::Button("Reset"))
		{
			gameWrapper->Execute([this](GameWrapper*) { ResetCvars(); });
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}
}


/*
	--- Cvar widgets ---
*/

// Returns the current value, to show the sub-settings of an enabled setting
bool RLTMBridge::RenderCvarCheckbox(const char* label, const char* cvarName, const char* tooltip)
{
	CVarWrapper cvar = GetCvar(cvarName);
	if (!cvar) return false;

	bool value = cvar.getBoolValue();
	if (ImGui::Checkbox(label, &value)) SetCvarFromUI(cvarName, value ? "1" : "0");
	RenderTooltip(tooltip);

	return value;
}

void RLTMBridge::RenderCvarInputInt(const char* label, const char* cvarName, const char* tooltip)
{
	CVarWrapper cvar = GetCvar(cvarName);
	if (!cvar) return;

	int value = cvar.getIntValue();
	ImGui::SetNextItemWidth(150.0f);
	if (ImGui::InputInt(label, &value, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue)) SetCvarFromUI(cvarName, std::to_string(value));
	RenderTooltip(tooltip);
}

// Slider of a cvar, applied when the slider is released (the cvars are saved to the disk on each change)
void RLTMBridge::RenderCvarSliderInt(const char* label, const char* cvarName, IntInput& input, int min, int max, const char* format, const char* tooltip)
{
	CVarWrapper cvar = GetCvar(cvarName);
	if (!cvar) return;

	if (!input.active) input.value = cvar.getIntValue();

	ImGui::SetNextItemWidth(250.0f);
	ImGui::SliderInt(label, &input.value, min, max, format);
	input.active = ImGui::IsItemActive();
	if (ImGui::IsItemDeactivatedAfterEdit()) SetCvarFromUI(cvarName, std::to_string(std::clamp(input.value, min, max)));
	RenderTooltip(tooltip);
}

// Text input of a cvar, applied when the input loses focus
void RLTMBridge::RenderCvarInputText(const char* label, const char* cvarName, TextInput& input, const char* tooltip, ImGuiInputTextFlags flags)
{
	CVarWrapper cvar = GetCvar(cvarName);
	if (!cvar) return;

	if (!input.active)
	{
		const std::string value = cvar.getStringValue();
		const size_t length = std::min(value.size(), input.buffer.size() - 1);
		std::copy_n(value.begin(), length, input.buffer.begin());
		input.buffer[length] = '\0';
	}

	ImGui::InputText(label, input.buffer.data(), input.buffer.size(), flags);
	input.active = ImGui::IsItemActive();
	if (ImGui::IsItemDeactivatedAfterEdit()) SetCvarFromUI(cvarName, input.buffer.data());
	RenderTooltip(tooltip);
}
