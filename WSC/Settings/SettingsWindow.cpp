#include "pch.h"
#include "WSC.h"

#include <algorithm>

#include "fmt/chrono.h"

namespace
{
	const ImVec4 COLOR_GREEN = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
	const ImVec4 COLOR_YELLOW = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
	const ImVec4 COLOR_RED = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
	const ImVec4 COLOR_GREY = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

	void RenderTooltip(const char* tooltip)
	{
		if (tooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
	}
}

/*
	--- Settings window ---
	Rendered on the render thread: cvars are only modified through gameWrapper->Execute
*/

void WSC::RenderSettings()
{
	ImGui::TextUnformatted("WebSocket Controller");
	ImGui::SameLine();
	ImGui::TextDisabled("v%s - by @Minarox", plugin_version);

	ImGui::Separator();
	RenderConnection();

	if (ImGui::CollapsingHeader("WebSocket settings")) RenderWebSocketSettings();
	if (ImGui::CollapsingHeader("Game settings", ImGuiTreeNodeFlags_DefaultOpen)) RenderGameSettings();
}

void WSC::RenderConnection()
{
	// Server URL, applied when the input loses focus
	CVarWrapper url = GetCvar(Cvars::Url);
	if (!url) return;

	if (!urlInputActive)
	{
		const std::string value = url.getStringValue();
		const size_t length = std::min(value.size(), urlInput.size() - 1);
		std::copy_n(value.begin(), length, urlInput.begin());
		urlInput[length] = '\0';
	}

	ImGui::InputText("Server URL", urlInput.data(), urlInput.size());
	urlInputActive = ImGui::IsItemActive();
	if (ImGui::IsItemDeactivatedAfterEdit()) SetCvarFromUI(Cvars::Url, urlInput.data());
	RenderTooltip("WebSocket server URL, e.g. ws://localhost:3000");

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

void WSC::RenderWebSocketSettings()
{
	RenderCvarCheckbox("Reconnect automatically", Cvars::AutoReconnect, "Reconnect to the server when the connection is lost");
	RenderCvarCheckbox("Allow server control", Cvars::ServerControl, "Allow the server to send commands (spectator camera, ready, etc.)");
	RenderCvarCheckbox("Compress messages", Cvars::PerMessageDeflate, "Per message deflate, applied on the next connection");
	RenderCvarInputInt("Handshake timeout (s)", Cvars::HandshakeTimeout, "Applied on the next connection");
	RenderCvarInputInt("Ping interval (s)", Cvars::PingInterval, "Applied on the next connection");
	RenderCvarInputInt("Min wait between retries (ms)", Cvars::MinWaitBetweenRetries, nullptr);
	RenderCvarInputInt("Max wait between retries (ms)", Cvars::MaxWaitBetweenRetries, nullptr);
}

void WSC::RenderGameSettings()
{
	RenderCvarInputInt("Entities update interval (ms)", Cvars::EntitiesInterval, "Interval between two updates of the balls and cars data");
	RenderCvarCheckbox("Join the spectators", Cvars::AutoSpectate, "Join the spectators when joining a match");
	RenderCvarCheckbox("Hide the HUD", Cvars::HideHud, "Hide the HUD when spectating, keep the names and the match info");
	RenderCvarCheckbox("Hide the statistics graphs", Cvars::HideStatGraphs, "Hide the engine statistics graphs (F10)");
	RenderCvarCheckbox("Mark as ready", Cvars::AutoReady, "Mark as ready at the end of the match");
	RenderCvarCheckbox("Save replays", Cvars::AutoSaveReplays, "Save the replay at the end of the match");
}

void WSC::RenderCvarCheckbox(const char* label, const char* cvarName, const char* tooltip)
{
	CVarWrapper cvar = GetCvar(cvarName);
	if (!cvar) return;

	bool value = cvar.getBoolValue();
	if (ImGui::Checkbox(label, &value)) SetCvarFromUI(cvarName, value ? "1" : "0");
	RenderTooltip(tooltip);
}

void WSC::RenderCvarInputInt(const char* label, const char* cvarName, const char* tooltip)
{
	CVarWrapper cvar = GetCvar(cvarName);
	if (!cvar) return;

	int value = cvar.getIntValue();
	ImGui::SetNextItemWidth(150.0f);
	if (ImGui::InputInt(label, &value, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue)) SetCvarFromUI(cvarName, std::to_string(value));
	RenderTooltip(tooltip);
}
