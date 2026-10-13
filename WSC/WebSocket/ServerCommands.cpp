#include "pch.h"
#include "WSC.h"

#include <algorithm>

#include "Protocol/Protocol.h"

/*
	--- Server commands ---

	Received message: { "command": "<name>", "payload": { ... }, "id": <optional string or number> }
	Response sent:    { "topic": "response", "payload": { "command": "<name>", "id": <id>, "success": true|false, "error": "<message>" } }

	A bad command must never crash the plugin or the game:
	- the message is parsed without exceptions and its size and depth are limited (Protocol/Protocol.cpp),
	- only the commands registered below can be executed (no raw console commands),
	- the commands are queued and executed on the game thread, never from the WebSocket thread,
	- every parameter is validated and every game object is null-checked,
	- every exception thrown by a command is caught and sent back as an error.
*/

namespace
{
	using namespace Protocol;

	// Commands waiting to be executed, the next ones are rejected (flood protection)
	constexpr size_t MAX_PENDING_COMMANDS = 64;

	// Commands executed per game tick, the others wait for the next tick
	constexpr int MAX_COMMANDS_PER_TICK = 8;

	// Plugin cvars the server is allowed to change with the "set_setting" command
	// (not the connection nor the server control settings)
	const char* const SERVER_SETTINGS[] = {
		Cvars::SendEntities,
		Cvars::EntitiesInterval,
		Cvars::SendSpectator,
		Cvars::SendStats,
		Cvars::AutoSpectate,
		Cvars::HideHud,
		Cvars::HudNames,
		Cvars::HudMatchInfo,
		Cvars::HideStatGraphs,
		Cvars::AutoReady,
		Cvars::AutoSaveReplays,
		Cvars::ReplayNameFormat
	};
}

void WSC::RegisterServerCommand(const std::string& name, const char* permission, CommandHandler handler)
{
	commandHandlers[name] = RegisteredCommand{ permission, std::move(handler) };
}

void WSC::RegisterServerCommands()
{
	RegisterServerCommand("ping", nullptr, [](const json& payload) -> json { return nullptr; });

	// Send every state again (match, scoreboard, stats, spectator, players, entities, result)
	RegisterServerCommand("resync", nullptr, [this](const json& payload) -> json {
		ResendState();
		return nullptr;
	});

	RegisterServerCommand("spectate", Cvars::ServerAllowActions, [this](const json& payload) -> json {
		if (!JoinSpectators(true)) throw CommandError("Not in a match");
		return nullptr;
	});

	RegisterServerCommand("ready", Cvars::ServerAllowActions, [this](const json& payload) -> json {
		if (!ReadyUp(true)) throw CommandError("Not in a match");
		return nullptr;
	});

	RegisterServerCommand("hide_hud", Cvars::ServerAllowActions, [this](const json& payload) -> json {
		if (!HideSpectatorHud(0, true)) throw CommandError("Not spectating a match");
		return nullptr;
	});

	// Payload: { "uid": "<uid>", "name": "<name>" }, at least one of them
	RegisterServerCommand("focus_player", Cvars::ServerAllowCamera, [this](const json& payload) -> json {
		const std::string uid = GetOptionalString(payload, "uid");
		const std::string name = GetOptionalString(payload, "name");
		if (uid.empty() && name.empty()) throw CommandError("\"uid\" or \"name\" is required");

		if (!FocusPlayer(uid, name)) throw CommandError("Player not found or camera not available");
		return nullptr;
	});

	// Payload: { "name": "<cvar>", "value": <string, number or boolean> }
	RegisterServerCommand("set_setting", Cvars::ServerAllowSettings, [this](const json& payload) -> json {
		const std::string name = GetRequiredString(payload, "name");
		const std::string value = GetCvarValue(payload, "value");

		auto allowed = std::find_if(std::begin(SERVER_SETTINGS), std::end(SERVER_SETTINGS), [&name](const char* setting) { return name == setting; });
		if (allowed == std::end(SERVER_SETTINGS)) throw CommandError(fmt::format("Setting \"{}\" can't be changed by the server", name));

		CVarWrapper cvar = GetCvar(*allowed);
		if (!cvar) throw CommandError(fmt::format("Setting \"{}\" not found", name));

		cvar.setValue(value);
		return nullptr;
	});

	// Name of the replay of the current match, or of the next one if the current match is ended
	// Payload: { "blue": "<team>", "orange": "<team>", "game": <number> } formatted with wsc_replay_name_format,
	// or { "name": "<replay name>" } to give the whole name
	RegisterServerCommand("set_replay_name", Cvars::ServerAllowReplayName, [this](const json& payload) -> json {
		std::string name = GetOptionalString(payload, "name");

		if (name.empty())
			name = BuildReplayName(GetRequiredString(payload, "blue"), GetRequiredString(payload, "orange"), GetRequiredInt(payload, "game", 1, MAX_GAME_NUMBER));
		else
			name = SanitizeReplayName(name);

		if (name.empty()) throw CommandError("The replay name is empty");

		SetReplayName(name);

		json result = json::object();
		result["name"] = name;
		result["applied"] = !match.ended && ApplyReplayName(); // false: applied when the next match starts
		return result;
	});

	RegisterServerCommand("clear_replay_name", Cvars::ServerAllowReplayName, [this](const json& payload) -> json {
		SetReplayName("");
		return nullptr;
	});
}

// Replay name from the wsc_replay_name_format cvar, also used for the preview of the settings window
std::string WSC::BuildReplayName(const std::string& blue, const std::string& orange, int game)
{
	CVarWrapper format = GetCvar(Cvars::ReplayNameFormat);

	return SanitizeReplayName(FormatReplayName(format ? format.getStringValue() : "", {
		{ "blue", blue },
		{ "orange", orange },
		{ "game", std::to_string(game) }
	}));
}

// Called from the ixwebsocket thread: only validate and queue the command
void WSC::OnServerMessage(const std::string& message)
{
	if (debug) LOG("[debug] Received: {}", message.substr(0, 500));

	ServerCommand command;
	const std::string error = ParseCommand(message, command);
	if (!error.empty())
	{
		SendCommandResponse(command.name, command.id, error);
		return;
	}

	{
		std::lock_guard<std::mutex> lock(pendingCommandsMutex);
		if (pendingCommands.size() < MAX_PENDING_COMMANDS)
		{
			pendingCommands.push_back(std::move(command));
			return;
		}
	}

	SendCommandResponse(command.name, command.id, "Too many pending commands");
}

// Called from the game thread on each tick
void WSC::ProcessServerCommands()
{
	for (int i = 0; i < MAX_COMMANDS_PER_TICK; i++)
	{
		ServerCommand command;
		{
			std::lock_guard<std::mutex> lock(pendingCommandsMutex);
			if (pendingCommands.empty()) return;

			command = std::move(pendingCommands.front());
			pendingCommands.pop_front();
		}

		ExecuteServerCommand(command);
	}
}

void WSC::ExecuteServerCommand(const ServerCommand& command)
{
	std::string error;
	json result;

	try
	{
		auto handler = commandHandlers.find(command.name);

		if (!IsCvarEnabled(Cvars::ServerControl)) error = "Server control is disabled";
		else if (handler == commandHandlers.end()) error = fmt::format("Unknown command \"{}\"", command.name);
		else if (handler->second.permission && !IsCvarEnabled(handler->second.permission)) error = fmt::format("Command \"{}\" is disabled in the plugin settings", command.name);
		else result = handler->second.handler(command.payload);
	}
	catch (const CommandError& e)
	{
		error = e.what();
	}
	catch (const std::exception& e)
	{
		error = fmt::format("Internal error: {}", e.what());
	}
	catch (...)
	{
		error = "Internal error";
	}

	if (!error.empty()) LOG("Server command \"{}\" failed: {}", command.name, error);

	commandsReceived++;
	{
		std::lock_guard<std::mutex> lock(lastCommandMutex);
		lastCommand = error.empty() ? command.name : fmt::format("{} (failed: {})", command.name, error);
	}

	SendCommandResponse(command.name, command.id, error, error.empty() ? result : json());
}

void WSC::SendCommandResponse(const std::string& command, const json& id, const std::string& error, const json& result)
{
	json payload = json::object();
	payload["command"] = command;
	payload["id"] = id;
	payload["success"] = error.empty();
	if (!error.empty()) payload["error"] = error;
	if (!result.is_null()) payload["result"] = result;

	Send(Topic::Response, payload);
}
