#include "pch.h"
#include "WSC.h"

namespace
{
	// Parameters of "Function TAGame.GFxHUD_TA.HandleStatTickerMessage"
	struct StatTickerParams
	{
		uintptr_t Receiver;
		uintptr_t Victim;
		uintptr_t StatEvent;
	};

	// Playlist ID of the private matches
	constexpr int PRIVATE_MATCH_PLAYLIST = 6;

	// Team number of the players that aren't in a team (spectators)
	constexpr int NO_TEAM = 255;

	// Stat events counted for each player, always sent (0 by default) in the match statistics
	constexpr const char* STAT_EVENTS[] = {
		"Demolish", "Demolition", "AerialGoal", "BackwardsGoal", "BicycleGoal", "LongGoal", "TurtleGoal", "PoolShot",
		"OvertimeGoal", "HatTrick", "Playmaker", "EpicSave", "Savior", "Center", "Clear", "FirstTouch", "BreakoutDamage",
		"BreakoutDamageLarge", "LowFive", "HighFive", "HoopsSwishGoal", "BicycleHit", "OwnGoal", "KO_Winner", "KO_Knockout",
		"KO_DoubleKO", "KO_TripleKO", "KO_Death", "KO_LightHit", "KO_HeavyHit", "KO_AerialLightHit", "KO_AerialHeavyHit",
		"KO_HitTaken", "KO_BlockTaken", "KO_Grabbed", "KO_Thrown", "KO_LightBlock", "KO_HeavyBlock", "KO_PlayerGrabbed",
		"KO_PlayerThrown"
	};

	// Unreal units per second to km/h, rounded
	int ToKmh(const Vector& velocity)
	{
		return static_cast<int>(velocity.magnitude() * 0.036f + 0.5f);
	}

	json ToJson(const Vector& location)
	{
		return { static_cast<int>(location.X), static_cast<int>(location.Y), static_cast<int>(location.Z) };
	}

	// Fills the fields identifying a player in every topic, returns false if the player isn't fully loaded
	bool GetPlayerIdentity(PriWrapper player, json& data)
	{
		UnrealStringWrapper name = player.GetPlayerName();
		if (!name) return false;

		data["uid"] = player.GetUniqueIdWrapper().GetIdString();
		data["name"] = name.ToString();
		data["bot"] = static_cast<bool>(player.GetbBot());

		return true;
	}

	// Unique key of a player: "<uid>|<name>" (bots share the same uid)
	std::string GetPlayerKey(const json& identity)
	{
		return identity["uid"].get<std::string>() + '|' + identity["name"].get<std::string>();
	}
}

/*
	--- Game data ---
*/

ServerWrapper WSC::GetServerWrapper()
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

bool WSC::IsMatchLive() const
{
	return match.started && !match.paused && !match.ended;
}

void WSC::UpdateMatch(bool countdownStarted)
{
	ServerWrapper server = GetServerWrapper();
	if (!server) return;

	SendPlayers(server);

	match.started = match.started || server.GetbBallHasBeenHit() || countdownStarted;
	match.ended = server.GetbMatchEnded();
	match.paused = gameWrapper->IsPaused() && !match.ended;

	// The clock is frozen when the match ends in overtime
	if (!(match.ended && server.GetbOverTime())) match.clock = server.GetSecondsRemaining();

	json payload = json::object();
	payload["arenaCode"] = gameWrapper->GetCurrentMap();
	payload["score"] = GetScore(server);
	payload["clock"] = match.clock;
	payload["duration"] = server.GetGameTime();
	payload["isUnlimited"] = static_cast<bool>(server.GetbUnlimitedTime());
	payload["isStarted"] = match.started;
	payload["isPaused"] = match.paused;
	payload["isOvertime"] = static_cast<bool>(server.GetbOverTime());
	payload["isEnded"] = match.ended;
	payload["isReplay"] = match.replay;
	payload["statistics"] = match.started ? GetStatistics(server) : json::object();

	Send(Topic::Match, payload);
}

void WSC::SetReplayState(bool replay)
{
	if (match.replay == replay) return;

	match.replay = replay;
	UpdateMatch();
}

json WSC::GetScore(ServerWrapper server)
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

json WSC::GetStatistics(ServerWrapper server)
{
	json statistics = json::object();

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

		const std::string key = GetPlayerKey(playerData);
		const std::map<std::string, int>& events = match.statEvents[key];

		for (const char* eventName : STAT_EVENTS)
		{
			auto it = events.find(eventName);
			playerData[eventName] = it != events.end() ? it->second : 0;
		}

		statistics[key] = playerData;
	}

	return statistics;
}

void WSC::OnStatTicker(void* params)
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

	// The stat ticker message is received twice for the same event, skip the duplicate
	const std::string tick = eventName + '|' + key;
	if (match.lastStatTick == tick)
	{
		match.lastStatTick.clear();
		return;
	}
	match.lastStatTick = tick;

	if (match.started) match.statEvents[key][eventName]++;

	payload["eventName"] = eventName;
	Send(Topic::Statistic, payload);

	// Refresh the statistics of the match with the new event
	UpdateMatch();
}

void WSC::OnTick()
{
	if (!IsMatchLive()) return;

	CVarWrapper intervalCvar = GetCvar(Cvars::EntitiesInterval);
	const auto interval = std::chrono::milliseconds(intervalCvar ? intervalCvar.getIntValue() : 100);
	const auto now = std::chrono::steady_clock::now();
	if (now - lastEntitiesUpdate < interval) return;
	lastEntitiesUpdate = now;

	ServerWrapper server = GetServerWrapper();
	if (!server) return;

	SendPlayers(server);
	SendEntities(server);
}

void WSC::SendEntities(ServerWrapper server)
{
	json payload = json::object();
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
		carData["asFlip"] = static_cast<bool>(car.HasFlip());

		BoostWrapper boost = car.GetBoostComponent();
		carData["boost"] = boost ? static_cast<int>(boost.GetCurrentBoostAmount() * 100) : 0;

		payload["cars"] += carData;
	}

	Send(Topic::Entities, payload);
}

void WSC::SendPlayers(ServerWrapper server)
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

void WSC::ResetMatch()
{
	match = MatchState();

	for (Topic topic : { Topic::Match, Topic::Entities, Topic::Players })
		Send(topic, nullptr);
}
