#pragma once

// Helpers shared by the game sources (Game/*.cpp)

namespace GameUtils
{
	// Team number of the players that aren't in a team (spectators)
	constexpr int NO_TEAM = 255;

	// Unreal units per second to km/h
	constexpr float UU_TO_KMH = 0.036f;

	// Unreal units per second to km/h, rounded
	inline int ToKmh(const Vector& velocity)
	{
		return static_cast<int>(velocity.magnitude() * UU_TO_KMH + 0.5f);
	}

	inline json ToJson(const Vector& location)
	{
		return { static_cast<int>(location.X), static_cast<int>(location.Y), static_cast<int>(location.Z) };
	}

	// Fills the fields identifying a player in every topic, returns false if the player isn't fully loaded
	inline bool GetPlayerIdentity(PriWrapper player, json& data)
	{
		UnrealStringWrapper name = player.GetPlayerName();
		if (!name) return false;

		data["uid"] = player.GetUniqueIdWrapper().GetIdString();
		data["name"] = name.ToString();
		data["bot"] = static_cast<bool>(player.GetbBot());

		return true;
	}

	// Unique key of a player: "<uid>|<name>" (bots share the same uid)
	inline std::string GetPlayerKey(const json& identity)
	{
		return identity["uid"].get<std::string>() + '|' + identity["name"].get<std::string>();
	}

	// Unique ID of the match, null when the game doesn't give one (e.g. offline matches)
	inline json GetMatchGuid(ServerWrapper server)
	{
		const std::string guid = server.GetMatchGUID();
		if (guid.find_first_not_of("0-{}") == std::string::npos) return nullptr;

		return guid;
	}

	// Identity and team of a player, null if the player isn't valid
	inline json GetPlayerReference(PriWrapper player)
	{
		if (!player) return nullptr;

		json data = json::object();
		if (!GetPlayerIdentity(player, data)) return nullptr;

		data["teamIndex"] = player.GetTeamNum();
		return data;
	}
}
