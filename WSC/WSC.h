#pragma once
#pragma comment (lib, "pluginsdk.lib")
#pragma comment (lib, "ws2_32.lib")
#pragma comment (lib, "crypt32.lib")

#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <map>
#include <mutex>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "bakkesmod/wrappers/GameObject/Stats/StatEventWrapper.h"

#include "GuiBase.h"
#include "PersistentStorage.h"
#include "version.h"

using json = nlohmann::json;

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

// Version of the messages format, sent to the server on connection. Increase it on each breaking change
constexpr int protocol_version = 2;

// Names of the plugin cvars, shared by the logic and the settings window
namespace Cvars
{
	// WebSocket
	inline constexpr const char* Url = "wsc_url";
	inline constexpr const char* AutoConnect = "wsc_auto_connect";
	inline constexpr const char* AutoReconnect = "wsc_auto_reconnect";
	inline constexpr const char* HandshakeTimeout = "wsc_handshake_timeout";
	inline constexpr const char* MinWaitBetweenRetries = "wsc_min_wait_between_retries";
	inline constexpr const char* MaxWaitBetweenRetries = "wsc_max_wait_between_retries";
	inline constexpr const char* PerMessageDeflate = "wsc_per_message_deflate";
	inline constexpr const char* PingInterval = "wsc_ping_interval";
	inline constexpr const char* ServerControl = "wsc_server_control";

	// Game
	inline constexpr const char* EntitiesInterval = "wsc_entities_interval";
	inline constexpr const char* AutoSaveReplays = "wsc_auto_save_replays";
	inline constexpr const char* ReplayNameFormat = "wsc_replay_name_format";
	inline constexpr const char* AutoSpectate = "wsc_auto_spectate";
	inline constexpr const char* AutoReady = "wsc_auto_ready";
	inline constexpr const char* HideHud = "wsc_hide_hud";
	inline constexpr const char* HideStatGraphs = "wsc_hide_stat_graphs";
}

// Topics of the messages sent to the server: { "topic": "<topic>", "payload": <data> }
enum class Topic
{
	Plugin,
	Match,
	Scoreboard,
	Spectator,
	Statistic,
	Entities,
	Players,
	Response
};

class WSC: public BakkesMod::Plugin::BakkesModPlugin, public SettingsWindowBase
{
public:
	// Boilerplate (WSC.cpp)
	void onLoad() override;
	void onUnload() override;

	// Settings window (Settings/SettingsWindow.cpp)
	void RenderSettings() override;

private:
	// Hooks (WSC.cpp)
	struct HookedEvent
	{
		std::string name;
		bool post;
	};
	std::vector<HookedEvent> hookedEvents;
	void HookEvents();
	void UnhookEvents();
	void Hook(const std::string& eventName, std::function<void()> callback, bool post = false);

	// Cvars and notifiers (Settings/Cvars.cpp)
	std::shared_ptr<PersistentStorage> persistentStorage;
	void RegisterCvars();
	void RegisterNotifiers();
	CVarWrapper GetCvar(const char* name);
	bool IsCvarEnabled(const char* name);
	void SetCvarFromUI(const char* name, const std::string& value);

	// Settings window (Settings/SettingsWindow.cpp)
	struct TextInput
	{
		std::array<char, 512> buffer{};
		bool active = false;
	};
	TextInput urlInput;
	TextInput replayNameFormatInput;
	void RenderConnection();
	void RenderWebSocketSettings();
	void RenderGameSettings();
	void RenderCvarCheckbox(const char* label, const char* cvarName, const char* tooltip);
	void RenderCvarInputInt(const char* label, const char* cvarName, const char* tooltip);
	void RenderCvarInputText(const char* label, const char* cvarName, TextInput& input, const char* tooltip);

	// WebSocket (WebSocket/WebSocket.cpp)
	ix::WebSocket socket;
	std::atomic<bool> socketRunning = false; // true between Connect() and Disconnect(), even while reconnecting
	std::mutex sentCacheMutex;
	json sentCache = json::object(); // Last payload sent per topic, used to skip duplicates and to resync the server on (re)connection
	std::mutex socketStatusMutex;
	std::string socketLastError;
	std::chrono::system_clock::time_point socketStatusSince = std::chrono::system_clock::now();
	void InitSocket();
	void ConfigureSocket();
	void Connect();
	void Disconnect();
	void Send(Topic topic, const json& payload);
	void ResendState();
	void SetSocketStatus(const std::string& error = "");

	// Server commands (WebSocket/ServerCommands.cpp)
	struct ServerCommand
	{
		std::string name;
		json payload;
		json id; // Optional, sent back in the response
	};
	using CommandHandler = std::function<json(const json& payload)>; // Returns the result sent in the response (null for none)
	std::map<std::string, CommandHandler> commandHandlers;
	std::mutex pendingCommandsMutex;
	std::deque<ServerCommand> pendingCommands;
	void RegisterServerCommands();
	void OnServerMessage(const std::string& message);
	void ProcessServerCommands();
	void ExecuteServerCommand(const ServerCommand& command);
	void SendCommandResponse(const std::string& command, const json& id, const std::string& error = "", const json& result = nullptr);

	// Game data (Game/GameData.cpp)
	struct MatchState
	{
		bool started = false;
		bool paused = false;
		bool ended = false;
		bool replay = false;
		json clock = 0;
		std::string lastStatTick;
		std::chrono::steady_clock::time_point lastStatTickTime;
		std::map<std::string, std::map<std::string, int>> statEvents; // "<uid>|<name>" -> stat event name -> count
	};
	MatchState match;
	std::chrono::steady_clock::time_point lastEntitiesUpdate;
	bool entitiesSkipped = false; // true while the server doesn't read the messages fast enough
	ServerWrapper GetServerWrapper();
	bool IsMatchLive() const;
	void UpdateMatch(bool countdownStarted = false);
	void SetReplayState(bool replay);
	json GetScore(ServerWrapper server);
	json GetScoreboard(ServerWrapper server);
	void OnStatTicker(void* params);
	void OnTick();
	void SendEntities(ServerWrapper server);
	void SendPlayers(ServerWrapper server);
	void SendSpectatorTarget();
	void ResetMatch();

	// Game actions (Game/GameActions.cpp)
	std::string initialAutoSaveReplay; // value of "ranked_autosavereplay_all" before the plugin changed it
	std::string replayName; // Name of the replay of the current or next match, set by the server
	void ApplyAutoSaveReplays();
	void RestoreAutoSaveReplays();
	bool ApplyReplayName();
	bool HideSpectatorHud(int delay, bool force = false);
	bool HideStatGraphs();
	bool ReadyUp(bool force = false);
	bool JoinSpectators(bool force = false);
	bool FocusPlayer(const std::string& uid, const std::string& name);
};
