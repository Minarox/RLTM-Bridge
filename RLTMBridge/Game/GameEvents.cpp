#include "pch.h"
#include "RLTMBridge.h"
#include "Game/GameUtils.h"

using namespace GameUtils;

namespace
{
	// Wait for the stat ticker messages of a goal (scorer, assist) before sending it
	constexpr auto GOAL_TICKER_WAIT = std::chrono::milliseconds(500);

	// A stat ticker message received this long before or after a goal belongs to it
	constexpr auto GOAL_TICKER_WINDOW = std::chrono::seconds(2);

	// The ball can hit the goal several times for the same goal
	constexpr auto DUPLICATE_GOAL_WINDOW = std::chrono::seconds(1);

	template<typename Ticker, typename Time>
	bool BelongsToGoal(const Ticker& ticker, Time goalTime)
	{
		if (ticker.player.is_null()) return false;

		const auto delay = ticker.time > goalTime ? ticker.time - goalTime : goalTime - ticker.time;
		return delay <= GOAL_TICKER_WINDOW;
	}
}

/*
	--- Game events ---
	One-shot messages of the "event" topic: { "type": "<type>", "clock": <seconds remaining>, ... }
*/

void RLTMBridge::SendEvent(const std::string& type, json data)
{
	if (!data.is_object()) data = json::object();
	data["type"] = type;
	data["clock"] = match.clock;

	Send(Topic::Event, data);
}

void RLTMBridge::OnGoal(BallWrapper ball)
{
	match.inPlay = false;
	if (!ball) return;

	const auto now = std::chrono::steady_clock::now();
	if (match.pendingGoal.active || now - match.pendingGoal.time < DUPLICATE_GOAL_WINDOW) return;

	match.pendingGoal.active = true;
	match.pendingGoal.time = now;
	match.pendingGoal.ballSpeed = ToKmh(ball.GetVelocity());
	match.pendingGoal.ballY = ball.GetLocation().Y;
}

void RLTMBridge::OnGoalTicker(const std::string& eventName, const json& player)
{
	if (player.is_null()) return;

	GoalTicker ticker;
	ticker.player = player;
	ticker.time = std::chrono::steady_clock::now();

	if (eventName == "Goal") match.goalTicker = ticker;
	else if (eventName == "Assist") match.assistTicker = ticker;
	else if (eventName == "OwnGoal") match.ownGoalTicker = ticker;
}

void RLTMBridge::SendPendingGoal(bool force)
{
	PendingGoal& goal = match.pendingGoal;
	if (!goal.active) return;
	if (!force && std::chrono::steady_clock::now() - goal.time < GOAL_TICKER_WAIT) return;

	goal.active = false;

	json scorer = BelongsToGoal(match.goalTicker, goal.time) ? match.goalTicker.player : json();
	const bool ownGoal = scorer.is_null() && BelongsToGoal(match.ownGoalTicker, goal.time);
	if (ownGoal) scorer = match.ownGoalTicker.player;
	const json assister = !ownGoal && BelongsToGoal(match.assistTicker, goal.time) ? match.assistTicker.player : json();

	// Team of the scorer, or the side of the goal: the orange goal is on the positive Y side
	json team = goal.ballY > 0 ? 0 : 1;
	if (!ownGoal && scorer.is_object() && scorer["teamIndex"].is_number()) team = scorer["teamIndex"];

	json data = json::object();
	data["teamIndex"] = team;
	data["scorer"] = scorer;
	data["assister"] = assister;
	data["isOwnGoal"] = ownGoal;
	data["ballSpeed"] = goal.ballSpeed;
	SendEvent("goal", data);

	match.goalTicker = GoalTicker();
	match.assistTicker = GoalTicker();
	match.ownGoalTicker = GoalTicker();
}


/*
	--- Result ---
	Sent once at the end of the match (after its last goal), kept until the end of the next match
	so a server connected later still receives it: the server deduplicates the results with the guid
*/

void RLTMBridge::SendResult()
{
	match.resultPending = false;
	if (match.resultSent) return;
	match.resultSent = true;

	ServerWrapper server = GetServerWrapper();
	if (!server) return;

	const json score = GetScore(server);
	const json scoreboard = GetScoreboard(server);

	json winner;
	TeamWrapper winnerTeam = server.GetMatchWinner();
	if (winnerTeam) winner = winnerTeam.GetTeamNum();
	else if (score.size() == 2 && score[0] != score[1]) winner = score[0] > score[1] ? 0 : 1;

	json mvp = GetPlayerReference(server.GetMVP());
	if (mvp.is_null())
	{
		for (const auto& [key, player] : scoreboard.items())
		{
			if (player.value("mvp", false))
			{
				mvp = { { "uid", player["uid"] }, { "name", player["name"] }, { "bot", player["bot"] }, { "teamIndex", player["teamIndex"] } };
				break;
			}
		}
	}

	const auto endedAt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

	json result = json::object();
	result["guid"] = GetMatchGuid(server);
	result["client"] = GetClientName();
	result["endedAt"] = endedAt.count();
	result["arenaCode"] = gameWrapper->GetCurrentMap();
	result["duration"] = server.GetGameTime();
	result["timePlayed"] = static_cast<int>(std::lround(server.GetTotalGameTimePlayed()));
	result["isOvertime"] = match.overtime;
	result["score"] = score;
	result["winner"] = winner;
	result["mvp"] = mvp;
	result["teams"] = GetTeams(server);
	result["scoreboard"] = scoreboard;
	result["stats"] = GetAdvancedStats(server);
	result["replayName"] = match.replayName.empty() ? json() : json(match.replayName);

	json event = json::object();
	event["winner"] = winner;
	event["score"] = score;
	SendEvent("matchEnded", event);

	Send(Topic::Result, result);
}
