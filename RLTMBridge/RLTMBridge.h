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
#include "Protocol/Protocol.h"
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
	inline constexpr const char* Url = "rltm_url";
	inline constexpr const char* ClientName = "rltm_client_name";
	inline constexpr const char* Token = "rltm_token";
	inline constexpr const char* AutoConnect = "rltm_auto_connect";
	inline constexpr const char* AutoReconnect = "rltm_auto_reconnect";
	inline constexpr const char* HandshakeTimeout = "rltm_handshake_timeout";
	inline constexpr const char* MinWaitBetweenRetries = "rltm_min_wait_between_retries";
	inline constexpr const char* MaxWaitBetweenRetries = "rltm_max_wait_between_retries";
	inline constexpr const char* PerMessageDeflate = "rltm_per_message_deflate";
	inline constexpr const char* PingInterval = "rltm_ping_interval";
	inline constexpr const char* Debug = "rltm_debug";

	// Server control
	inline constexpr const char* ServerControl = "rltm_server_control";
	inline constexpr const char* ServerAllowCamera = "rltm_server_allow_camera";
	inline constexpr const char* ServerAllowActions = "rltm_server_allow_actions";
	inline constexpr const char* ServerAllowReplayName = "rltm_server_allow_replay_name";
	inline constexpr const char* ServerAllowSettings = "rltm_server_allow_settings";

	// Data
	inline constexpr const char* SendEntities = "rltm_send_entities";
	inline constexpr const char* EntitiesInterval = "rltm_entities_interval";
	inline constexpr const char* SendSpectator = "rltm_send_spectator";
	inline constexpr const char* SendStats = "rltm_send_stats";

	// Spectator
	inline constexpr const char* AutoSpectate = "rltm_auto_spectate";
	inline constexpr const char* HideHud = "rltm_hide_hud";
	inline constexpr const char* HudNames = "rltm_hud_names";
	inline constexpr const char* HudMatchInfo = "rltm_hud_match_info";
	inline constexpr const char* HideStatGraphs = "rltm_hide_stat_graphs";

	// Match end
	inline constexpr const char* AutoReady = "rltm_auto_ready";
	inline constexpr const char* AutoSaveReplays = "rltm_auto_save_replays";
	inline constexpr const char* ReplayNameFormat = "rltm_replay_name_format";

	// Every cvar of the plugin (reset to the default values)
	inline constexpr const char* All[] = {
		Url, ClientName, Token, AutoConnect, AutoReconnect, HandshakeTimeout, MinWaitBetweenRetries, MaxWaitBetweenRetries, PerMessageDeflate, PingInterval, Debug,
		ServerControl, ServerAllowCamera, ServerAllowActions, ServerAllowReplayName, ServerAllowSettings,
		SendEntities, EntitiesInterval, SendSpectator, SendStats,
		AutoSpectate, HideHud, HudNames, HudMatchInfo, HideStatGraphs,
		AutoReady, AutoSaveReplays, ReplayNameFormat
	};
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
	Stats,
	Event,
	Result,
	Response
};

class RLTMBridge: public BakkesMod::Plugin::BakkesModPlugin, public SettingsWindowBase
{
public:
	// Boilerplate (RLTMBridge.cpp)
	void onLoad() override;
	void onUnload() override;

	// Settings window (Settings/SettingsWindow.cpp)
	void RenderSettings() override;

private:
	// Hooks (RLTMBridge.cpp)
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
	void ResetCvars();

	// Settings window (Settings/SettingsWindow.cpp)
	struct TextInput
	{
		std::array<char, 512> buffer{};
		bool active = false;
	};
	struct IntInput
	{
		int value = 0;
		bool active = false;
	};
	TextInput urlInput;
	TextInput clientNameInput;
	TextInput tokenInput;
	TextInput replayNameFormatInput;
	IntInput entitiesIntervalInput;
	void RenderConnection();
	void RenderStatus();
	void RenderDataSettings();
	void RenderServerControlSettings();
	void RenderSpectatorSettings();
	void RenderMatchEndSettings();
	void RenderWebSocketSettings();
	void RenderResetSettings();
	bool RenderCvarCheckbox(const char* label, const char* cvarName, const char* tooltip);
	void RenderCvarInputInt(const char* label, const char* cvarName, const char* tooltip);
	void RenderCvarSliderInt(const char* label, const char* cvarName, IntInput& input, int min, int max, const char* format, const char* tooltip);
	void RenderCvarInputText(const char* label, const char* cvarName, TextInput& input, const char* tooltip, ImGuiInputTextFlags flags = 0);

	// WebSocket (WebSocket/WebSocket.cpp)
	ix::WebSocket socket;
	std::atomic<bool> socketRunning = false; // true between Connect() and Disconnect(), even while reconnecting
	std::mutex sentCacheMutex;
	json sentCache = json::object(); // Last payload sent per topic, used to skip duplicates and to resync the server on (re)connection
	std::mutex socketStatusMutex;
	std::string socketLastError;
	std::chrono::system_clock::time_point socketStatusSince = std::chrono::system_clock::now();
	std::atomic<bool> debug = false; // Copy of the rltm_debug cvar, read from every thread
	std::atomic<uint64_t> messagesSent = 0;
	std::atomic<uint64_t> bytesSent = 0; // On the network, after compression
	std::mutex pluginInfoMutex;
	json pluginInfo; // Payload of the "plugin" message sent on each connection (version, client name)
	std::string GetClientName();
	void InitSocket();
	void ConfigureSocket();
	void Connect();
	void Disconnect();
	void Send(Topic topic, const json& payload);
	void ResendState();
	void CountSent(const ix::WebSocketSendInfo& info);
	void SetSocketStatus(const std::string& error = "");

	// Server commands (WebSocket/ServerCommands.cpp)
	using ServerCommand = Protocol::Command;
	using CommandHandler = std::function<json(const json& payload)>; // Returns the result sent in the response (null for none)
	struct RegisteredCommand
	{
		const char* permission; // Cvar allowing the command, nullptr if always allowed
		CommandHandler handler;
	};
	std::map<std::string, RegisteredCommand> commandHandlers;
	std::atomic<uint64_t> commandsReceived = 0;
	std::mutex lastCommandMutex;
	std::string lastCommand; // Name and result of the last command, shown in the settings window
	std::mutex pendingCommandsMutex;
	std::deque<ServerCommand> pendingCommands;
	void RegisterServerCommands();
	void RegisterServerCommand(const std::string& name, const char* permission, CommandHandler handler);
	void OnServerMessage(const std::string& message);
	void ProcessServerCommands();
	void ExecuteServerCommand(const ServerCommand& command);
	std::string BuildReplayName(const std::string& blue, const std::string& orange, int game);
	void SendCommandResponse(const std::string& command, const json& id, const std::string& error = "", const json& result = nullptr);

	// Game data (Game/GameData.cpp)
	using SteadyTime = std::chrono::steady_clock::time_point;

	// Advanced statistics of a player, accumulated on each frame while the ball is in play (Game/AdvancedStats.cpp)
	struct PlayerStats
	{
		json identity;
		double timeInPlay = 0; // Seconds with a car on the field while the ball is in play
		double timeSupersonic = 0;
		double timeOnGround = 0;
		double timeOnWall = 0;
		double timeInAir = 0;
		double distance = 0; // Unreal units
		double boostUsed = 0; // 0 to 100 per full tank
		double lastBoost = -1; // -1 when the car isn't on the field
	};

	// Stat ticker message received for a goal (scorer, assist, own goal)
	struct GoalTicker
	{
		json player; // null when not received
		SteadyTime time;
	};

	// Goal waiting for the stat ticker messages (scorer, assist) before being sent (Game/GameEvents.cpp)
	struct PendingGoal
	{
		bool active = false;
		SteadyTime time;
		int ballSpeed = 0;
		float ballY = 0;
	};

	struct MatchState
	{
		bool started = false;
		bool paused = false;
		bool ended = false;
		bool replay = false;
		bool overtime = false;
		bool inPlay = false; // Between the first touch after a kickoff and the next goal
		bool resultPending = false; // The match is ended, the result is sent after its last goal
		bool resultSent = false;
		json clock = 0;
		std::string replayName; // Name given to the replay of this match
		std::string lastStatTick;
		SteadyTime lastStatTickTime;
		std::map<std::string, std::map<std::string, int>> statEvents; // "<uid>|<name>" -> stat event name -> count
		std::map<std::string, PlayerStats> playerStats; // "<uid>|<name>" -> advanced statistics
		SteadyTime lastStatsUpdate;
		SteadyTime lastStatsSent;
		PendingGoal pendingGoal;
		GoalTicker goalTicker;
		GoalTicker assistTicker;
		GoalTicker ownGoalTicker;
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
	json GetTeams(ServerWrapper server);
	void ResetMatch();

	// Advanced statistics (Game/AdvancedStats.cpp)
	void UpdateAdvancedStats(ServerWrapper server);
	json GetAdvancedStats(ServerWrapper server);

	// Game events and result (Game/GameEvents.cpp)
	void SendEvent(const std::string& type, json data = json::object());
	void OnGoal(BallWrapper ball);
	void OnGoalTicker(const std::string& eventName, const json& player);
	void SendPendingGoal(bool force = false);
	void SendResult();

	// Game actions (Game/GameActions.cpp)
	std::string initialAutoSaveReplay; // value of "ranked_autosavereplay_all" before the plugin changed it
	std::mutex replayNameMutex;
	std::string replayName; // Name of the replay of the current or next match, set by the server (read by the settings window)
	std::string GetReplayName();
	void SetReplayName(const std::string& name);
	void ApplyAutoSaveReplays();
	void RestoreAutoSaveReplays();
	bool ApplyReplayName();
	bool HideSpectatorHud(int delay, bool force = false);
	bool RestoreSpectatorHud();
	bool HideStatGraphs();
	bool ReadyUp(bool force = false);
	bool JoinSpectators(bool force = false);
	bool FocusPlayer(const std::string& uid, const std::string& name);
};
