#include "pch.h"
#include "WSC.h"
#include "Game/GameUtils.h"

using namespace GameUtils;

namespace
{
	// Longest frame counted: a longer frame (loading, alt-tab) would add a wrong time
	constexpr double MAX_FRAME_SECONDS = 0.1;

	// Seconds rounded to a tenth (divided last so the JSON shows "12.3" and not "12.300000000000001")
	double RoundTenth(double value)
	{
		return std::round(value * 10.0) / 10.0;
	}
}

/*
	--- Advanced statistics ---
	Accumulated on each frame while the ball is in play (from the first touch after a kickoff to the next goal),
	more precise than what the server could compute from the entities updates
*/

void WSC::UpdateAdvancedStats(ServerWrapper server)
{
	const auto now = std::chrono::steady_clock::now();
	const double frameSeconds = std::min(std::chrono::duration<double>(now - match.lastStatsUpdate).count(), MAX_FRAME_SECONDS);
	match.lastStatsUpdate = now;

	// The cars of a goal replay replay their boost: nothing is counted
	if (match.replay)
	{
		for (auto& [key, stats] : match.playerStats) stats.lastBoost = -1;
		return;
	}

	for (PriWrapper player : server.GetPRIs())
	{
		if (!player || player.GetTeamNum() == NO_TEAM) continue;

		json identity = GetPlayerReference(player);
		if (identity.is_null()) continue;

		PlayerStats& stats = match.playerStats[GetPlayerKey(identity)];
		stats.identity = identity;

		CarWrapper car = player.GetCar();
		if (!car)
		{
			// Demolished: the boost of the next car isn't a boost used
			stats.lastBoost = -1;
			continue;
		}

		// The boost used is counted even before the first touch of the kickoff
		BoostWrapper boostComponent = car.GetBoostComponent();
		const double boost = boostComponent ? boostComponent.GetCurrentBoostAmount() * 100.0 : 0.0;
		if (stats.lastBoost >= 0 && boost < stats.lastBoost) stats.boostUsed += stats.lastBoost - boost;
		stats.lastBoost = boost;

		if (!match.inPlay) continue;

		stats.timeInPlay += frameSeconds;
		stats.distance += car.GetVelocity().magnitude() * frameSeconds;
		if (car.GetbSuperSonic()) stats.timeSupersonic += frameSeconds;

		if (car.IsOnGround()) stats.timeOnGround += frameSeconds;
		else if (car.IsOnWall()) stats.timeOnWall += frameSeconds;
		else stats.timeInAir += frameSeconds;
	}
}

json WSC::GetAdvancedStats(ServerWrapper server)
{
	json payload = json::object();

	// Boost pickups are counted by the game
	std::map<std::string, int> boostPickups;
	for (PriWrapper player : server.GetPRIs())
	{
		json identity = GetPlayerReference(player);
		if (!identity.is_null()) boostPickups[GetPlayerKey(identity)] = player.GetBoostPickups();
	}

	for (const auto& [key, stats] : match.playerStats)
	{
		json playerData = stats.identity;
		playerData["boostPickups"] = boostPickups.count(key) ? boostPickups[key] : 0;
		playerData["boostUsed"] = static_cast<int>(std::lround(stats.boostUsed));
		playerData["timeInPlay"] = RoundTenth(stats.timeInPlay);
		playerData["timeSupersonic"] = RoundTenth(stats.timeSupersonic);
		playerData["timeOnGround"] = RoundTenth(stats.timeOnGround);
		playerData["timeOnWall"] = RoundTenth(stats.timeOnWall);
		playerData["timeInAir"] = RoundTenth(stats.timeInAir);
		playerData["averageSpeed"] = stats.timeInPlay > 0 ? static_cast<int>(std::lround(stats.distance / stats.timeInPlay * UU_TO_KMH)) : 0;

		payload[key] = playerData;
	}

	return payload;
}
