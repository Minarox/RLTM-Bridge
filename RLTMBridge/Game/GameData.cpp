#include "pch.h"
#include "RLTMBridge.h"
#include "Game/GameUtils.h"

using namespace GameUtils;

namespace
{
	// Parameters of "Function TAGame.GFxHUD_TA.HandleStatTickerMessage"
	struct StatTickerParams
	{
		uintptr_t Receiver;
		uintptr_t Victim;
		uintptr_t StatEvent;
	};

	// Bytes waiting to be sent above which the entities updates are skipped
	constexpr size_t MAX_BUFFERED_BYTES = 64 * 1024;

	// The same stat ticker message received again within this delay is a duplicate
	constexpr auto STAT_TICKER_DUPLICATE_WINDOW = std::chrono::milliseconds(100);

	// Playlist ID of the private matches
	constexpr int PRIVATE_MATCH_PLAYLIST = 6;

	// Interval between two updates of the advanced statistics
	constexpr auto STATS_INTERVAL = std::chrono::seconds(1);
}

/*
	--- Game data ---
*/

ServerWrapper RLTMBridge::GetServerWrapper()
{
	ServerWrapper onlineServer = gameWrapper->GetOnlineGame();
	if (onlineServer)
	{
		GameSettingPlaylistWrapper playlist = onlineServer.GetPlaylist();
		if (playlist && playlist.GetPlaylistId() == PRIVATE_MATCH_PLAYLIST) return onlineServer;
	}

	ServerWrapper localServer = gameWrapper->GetGameEventAsServer();
	if (localServer) return localServer;

	return ServerWrapper(0);
}

bool RLTMBridge::IsMatchLive() const
{
	return match.started && !match.paused && !match.ended;
}

void RLTMBridge::UpdateMatch(bool countdownStarted)
{
	ServerWrapper server = GetServerWrapper();
	if (!server) return;

	SendPlayers(server);

	match.started = match.started || server.GetbBallHasBeenHit() || countdownStarted;
	match.ended = server.GetbMatchEnded();
	match.paused = gameWrapper->IsPaused() && !match.ended;

	// The clock is frozen when the match ends in overtime
	if (!(match.ended && server.GetbOverTime())) match.clock = server.GetSecondsRemaining();

	const bool overtime = server.GetbOverTime();
	if (overtime && !match.overtime) SendEvent("overtime");
	match.overtime = overtime;

	json payload = json::object();
	payload["guid"] = GetMatchGuid(server);
	payload["arenaCode"] = gameWrapper->GetCurrentMap();
	payload["score"] = GetScore(server);
	payload["teams"] = GetTeams(server);
	payload["clock"] = match.clock;
	payload["duration"] = server.GetGameTime();
	payload["isUnlimited"] = static_cast<bool>(server.GetbUnlimitedTime());
	payload["isStarted"] = match.started;
	payload["isPaused"] = match.paused;
	payload["isOvertime"] = overtime;
	payload["isEnded"] = match.ended;
	payload["isReplay"] = match.replay;

	Send(Topic::Match, payload);

	// Separated from the match: the clock changes every second, the statistics only a few times per minute
	Send(Topic::Scoreboard, match.started ? GetScoreboard(server) : json::object());
}

void RLTMBridge::SetReplayState(bool replay)
{
	if (match.replay == replay) return;

	match.replay = replay;
	SendEvent(replay ? "replayStart" : "replayEnd");
	UpdateMatch();
}

json RLTMBridge::GetScore(ServerWrapper server)
{
	json score = json::array();

	ArrayWrapper<TeamWrapper> teams = server.GetTeams();
	if (teams.Count() < 2) return score;

	TeamWrapper team0 = teams.Get(0);
	TeamWrapper team1 = teams.Get(1);
	if (!team0 || !team1) return score;

	score += team0.GetScore();
	score += team1.GetScore();

	return score;
}

json RLTMBridge::GetScoreboard(ServerWrapper server)
{
	json scoreboard = json::object();

	for (PriWrapper player : server.GetPRIs())
	{
		if (!player || player.GetTeamNum() == NO_TEAM) continue;

		json playerData = json::object();
		if (!GetPlayerIdentity(player, playerData)) continue;

		playerData["teamIndex"] = player.GetTeamNum();
		playerData["mvp"] = static_cast<bool>(player.GetbMatchMVP());
		playerData["score"] = player.GetMatchScore();
		playerData["goals"] = player.GetMatchGoals();
		playerData["shots"] = player.GetMatchShots();
		playerData["assists"] = player.GetMatchAssists();
		playerData["saves"] = player.GetMatchSaves();
		playerData["ballTouches"] = player.GetBallTouches();
		playerData["carTouches"] = player.GetCarTouches();

		// Only the stat events received for the player, a missing event means 0
		const std::string key = GetPlayerKey(playerData);
		playerData["events"] = json::object();
		for (const auto& [eventName, count] : match.statEvents[key])
			playerData["events"][eventName] = count;

		scoreboard[key] = playerData;
	}

	return scoreboard;
}

void RLTMBridge::OnStatTicker(void* params)
{
	ServerWrapper server = GetServerWrapper();
	if (!server || !params) return;

	StatTickerParams* tickerParams = static_cast<StatTickerParams*>(params);
	PriWrapper player = PriWrapper(tickerParams->Receiver);
	StatEventWrapper event = StatEventWrapper(tickerParams->StatEvent);
	if (!player || !event) return;

	json payload = json::object();
	if (!GetPlayerIdentity(player, payload)) return;

	const std::string eventName = event.GetEventName();
	const std::string key = GetPlayerKey(payload);

	// The stat ticker message is received twice for the same event: skip the copies received right after it,
	// but keep two real events in a row (e.g. two demolitions by the same player)
	const std::string tick = eventName + '|' + key;
	const auto now = std::chrono::steady_clock::now();
	if (match.lastStatTick == tick && now - match.lastStatTickTime < STAT_TICKER_DUPLICATE_WINDOW) return;

	match.lastStatTick = tick;
	match.lastStatTickTime = now;

	if (match.started) match.statEvents[key][eventName]++;

	// Scorer, assist and own goal of the goal event
	OnGoalTicker(eventName, GetPlayerReference(player));

	payload["eventName"] = eventName;
	Send(Topic::Statistic, payload);

	// Refresh the scoreboard with the new event
	UpdateMatch();
}

void RLTMBridge::OnTick()
{
	ProcessServerCommands();

	// Every frame, even during the replays and the pauses: only sent when the camera target changes
	if (IsCvarEnabled(Cvars::SendSpectator)) SendSpectatorTarget();

	// Sent once the stat ticker messages of the goal (scorer, assist) are received, then the result after the last goal
	SendPendingGoal();
	if (match.resultPending && !match.pendingGoal.active) SendResult();

	if (!IsMatchLive()) return;

	ServerWrapper server = GetServerWrapper();
	if (!server) return;

	// Accumulated on each frame for the precision of the times, sent once per second
	UpdateAdvancedStats(server);

	const auto now = std::chrono::steady_clock::now();
	if (now - match.lastStatsSent >= STATS_INTERVAL && IsCvarEnabled(Cvars::SendStats))
	{
		match.lastStatsSent = now;
		Send(Topic::Stats, GetAdvancedStats(server));
	}

	CVarWrapper intervalCvar = GetCvar(Cvars::EntitiesInterval);
	const auto interval = std::chrono::milliseconds(intervalCvar ? intervalCvar.getIntValue() : 100);
	if (now - lastEntitiesUpdate < interval) return;
	lastEntitiesUpdate = now;

	SendPlayers(server);

	// The messages are written to the network from the game thread: if the server doesn't read them fast enough,
	// skip the entities (outdated on the next update anyway) instead of filling the send buffer
	const bool overloaded = socket.bufferedAmount() > MAX_BUFFERED_BYTES;
	if (overloaded != entitiesSkipped)
	{
		entitiesSkipped = overloaded;
		if (overloaded) LOG("The server doesn't read the messages fast enough, entities updates are skipped");
		else LOG("The server caught up, entities updates are resumed");
	}

	if (!overloaded && IsCvarEnabled(Cvars::SendEntities)) SendEntities(server);
}

void RLTMBridge::SendEntities(ServerWrapper server)
{
	// Monotonic time of the sample in milliseconds, only the difference between two updates is meaningful (interpolation)
	const auto time = std::chrono::steady_clock::now().time_since_epoch();

	json payload = json::object();
	payload["time"] = std::chrono::duration_cast<std::chrono::milliseconds>(time).count();
	payload["balls"] = json::array();
	payload["cars"] = json::array();

	for (BallWrapper ball : server.GetGameBalls())
	{
		if (!ball) continue;

		json ballData = json::object();
		ballData["speed"] = ToKmh(ball.GetVelocity());
		ballData["location"] = ToJson(ball.GetLocation());

		payload["balls"] += ballData;
	}

	for (PriWrapper player : server.GetPRIs())
	{
		if (!player || player.GetTeamNum() == NO_TEAM) continue;

		CarWrapper car = player.GetCar();
		if (!car) continue;

		json carData = json::object();
		if (!GetPlayerIdentity(player, carData)) continue;

		carData["teamIndex"] = player.GetTeamNum();
		carData["speed"] = ToKmh(car.GetVelocity());
		carData["location"] = ToJson(car.GetLocation());
		carData["isSuperSonic"] = static_cast<bool>(car.GetbSuperSonic());
		carData["isOnWall"] = car.IsOnWall();
		carData["isOnGround"] = car.IsOnGround();
		carData["isInGoal"] = static_cast<bool>(car.GetbWasInGoalZone());
		carData["isDodging"] = car.IsDodging();
		carData["hasFlip"] = static_cast<bool>(car.HasFlip());

		BoostWrapper boost = car.GetBoostComponent();
		carData["boost"] = boost ? static_cast<int>(boost.GetCurrentBoostAmount() * 100) : 0;

		payload["cars"] += carData;
	}

	Send(Topic::Entities, payload);
}

void RLTMBridge::SendPlayers(ServerWrapper server)
{
	json players = json::array();

	for (PriWrapper player : server.GetPRIs())
	{
		if (!player) continue;

		json playerData = json::object();
		if (!GetPlayerIdentity(player, playerData)) continue;

		playerData["teamIndex"] = player.GetTeamNum();

		CarWrapper car = player.GetCar();
		playerData["carId"] = car ? car.GetLoadoutBody() : 0;

		players += playerData;
	}

	Send(Topic::Players, players);
}

void RLTMBridge::SendSpectatorTarget()
{
	// No match: no target (only sent once, the payload is compared with the last one sent)
	ServerWrapper server = GetServerWrapper();
	CameraWrapper camera = gameWrapper->GetCamera();
	if (!server || !camera)
	{
		Send(Topic::Spectator, nullptr);
		return;
	}

	json payload = json::object();
	payload["camera"] = camera.GetCameraState();
	payload["target"] = nullptr;

	// The view target points to the PRI of the followed player, and to nothing with the free camera.
	// It is compared to the players of the match instead of being used directly, in case it isn't a valid player.
	const uintptr_t targetPri = reinterpret_cast<uintptr_t>(camera.GetViewTarget().PRI);
	if (targetPri)
	{
		for (PriWrapper player : server.GetPRIs())
		{
			if (!player || player.memory_address != targetPri || player.GetTeamNum() == NO_TEAM) continue;

			json target = json::object();
			if (!GetPlayerIdentity(player, target)) break;

			target["teamIndex"] = player.GetTeamNum();
			payload["target"] = target;
			break;
		}
	}

	Send(Topic::Spectator, payload);
}

// Custom name (null if not set in the match settings) and colors of the teams
json RLTMBridge::GetTeams(ServerWrapper server)
{
	json teams = json::array();

	for (TeamWrapper team : server.GetTeams())
	{
		if (!team) continue;

		UnrealStringWrapper customName = team.GetCustomTeamName();
		const std::string name = customName ? customName.ToString() : "";

		const LinearColor primary = team.GetPrimaryColor();
		const LinearColor secondary = team.GetSecondaryColor();

		json teamData = json::object();
		teamData["index"] = team.GetTeamNum();
		teamData["name"] = name.empty() ? json() : json(name);
		teamData["primaryColor"] = Protocol::ToHexColor(primary.R, primary.G, primary.B);
		teamData["secondaryColor"] = Protocol::ToHexColor(secondary.R, secondary.G, secondary.B);

		teams += teamData;
	}

	return teams;
}

void RLTMBridge::ResetMatch()
{
	match = MatchState();

	// The result of the last match is kept: the server may not have received it yet
	for (Topic topic : { Topic::Match, Topic::Scoreboard, Topic::Stats, Topic::Spectator, Topic::Entities, Topic::Players })
		Send(topic, nullptr);
}
