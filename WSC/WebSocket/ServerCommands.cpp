#include "pch.h"
#include "WSC.h"

#include <algorithm>
#include <stdexcept>

/*
	--- Server commands ---

	Received message: { "command": "<name>", "payload": { ... }, "id": <optional string or number> }
	Response sent:    { "topic": "response", "payload": { "command": "<name>", "id": <id>, "success": true|false, "error": "<message>" } }

	A bad command must never crash the plugin or the game:
	- the message is parsed without exceptions and its size is limited,
	- only the commands registered below can be executed (no raw console commands),
	- the commands are queued and executed on the game thread, never from the WebSocket thread,
	- every parameter is validated and every game object is null-checked,
	- every exception thrown by a command is caught and sent back as an error.
*/

namespace
{
	// Larger messages are rejected without being parsed
	constexpr size_t MAX_MESSAGE_SIZE = 16 * 1024;

	// Deeper JSON is rejected without being parsed (stack overflow protection)
	constexpr int MAX_JSON_DEPTH = 32;

	// Commands waiting to be executed, the next ones are rejected (flood protection)
	constexpr size_t MAX_PENDING_COMMANDS = 64;

	// Commands executed per game tick, the others wait for the next tick
	constexpr int MAX_COMMANDS_PER_TICK = 8;

	// Replay name format used when the cvar is empty
	const std::string DEFAULT_REPLAY_NAME_FORMAT = "{blue} vs {orange} - Match {game}";

	// Maximum size of a replay name in bytes (UTF-8)
	constexpr size_t MAX_REPLAY_NAME_SIZE = 128;

	// Maximum game number of a series
	constexpr int MAX_GAME_NUMBER = 99;

	// Plugin cvars the server is allowed to change with the "set_setting" command
	const char* const SERVER_SETTINGS[] = {
		Cvars::EntitiesInterval,
		Cvars::AutoSaveReplays,
		Cvars::ReplayNameFormat,
		Cvars::AutoSpectate,
		Cvars::AutoReady,
		Cvars::HideHud,
		Cvars::HideStatGraphs
	};

	// Error of a command, its message is sent back to the server
	struct CommandError : std::runtime_error
	{
		using std::runtime_error::runtime_error;
	};

	// Nesting depth of the arrays and objects of a JSON text, without parsing it
	int GetJsonDepth(const std::string& text)
	{
		int depth = 0;
		int maxDepth = 0;
		bool inString = false;
		bool escaped = false;

		for (char c : text)
		{
			if (inString)
			{
				if (escaped) escaped = false;
				else if (c == '\\') escaped = true;
				else if (c == '"') inString = false;
			}
			else if (c == '"') inString = true;
			else if (c == '{' || c == '[') maxDepth = std::max(maxDepth, ++depth);
			else if (c == '}' || c == ']') depth--;
		}

		return maxDepth;
	}

	std::string GetOptionalString(const json& payload, const char* key)
	{
		if (!payload.is_object() || !payload.contains(key) || payload[key].is_null()) return "";
		if (!payload[key].is_string()) throw CommandError(fmt::format("\"{}\" must be a string", key));

		return payload[key].get<std::string>();
	}

	std::string GetRequiredString(const json& payload, const char* key)
	{
		std::string value = GetOptionalString(payload, key);
		if (value.empty()) throw CommandError(fmt::format("\"{}\" is required", key));

		return value;
	}

	int GetRequiredInt(const json& payload, const char* key, int min, int max)
	{
		if (!payload.is_object() || !payload.contains(key) || !payload[key].is_number_integer())
			throw CommandError(fmt::format("\"{}\" must be an integer", key));

		const json::number_integer_t value = payload[key].get<json::number_integer_t>();
		if (value < min || value > max) throw CommandError(fmt::format("\"{}\" must be between {} and {}", key, min, max));

		return static_cast<int>(value);
	}

	// Replaces the "{key}" placeholders of the format in a single pass (the values can contain braces)
	std::string FormatReplayName(const std::string& format, const std::map<std::string, std::string>& values)
	{
		const std::string& pattern = format.empty() ? DEFAULT_REPLAY_NAME_FORMAT : format;
		std::string name;

		for (size_t i = 0; i < pattern.size(); i++)
		{
			if (pattern[i] == '{')
			{
				const size_t end = pattern.find('}', i);
				if (end != std::string::npos)
				{
					auto value = values.find(pattern.substr(i + 1, end - i - 1));
					if (value != values.end())
					{
						name += value->second;
						i = end;
						continue;
					}
				}
			}

			name += pattern[i];
		}

		return name;
	}

	// Removes the control characters and the surrounding spaces, and limits the length without cutting a UTF-8 character
	std::string SanitizeReplayName(const std::string& name)
	{
		std::string sanitized;
		for (char c : name)
		{
			const unsigned char byte = static_cast<unsigned char>(c);
			if (byte >= 0x20 && byte != 0x7F) sanitized += c;
		}

		const size_t first = sanitized.find_first_not_of(' ');
		if (first == std::string::npos) return "";
		sanitized = sanitized.substr(first, sanitized.find_last_not_of(' ') - first + 1);

		if (sanitized.size() > MAX_REPLAY_NAME_SIZE)
		{
			size_t size = MAX_REPLAY_NAME_SIZE;
			while (size > 0 && (static_cast<unsigned char>(sanitized[size]) & 0xC0) == 0x80) size--; // UTF-8 continuation byte
			sanitized.resize(size);
		}

		return sanitized;
	}

	// Converts a string, number or boolean value to a cvar value
	std::string GetCvarValue(const json& payload, const char* key)
	{
		if (!payload.is_object() || !payload.contains(key)) throw CommandError(fmt::format("\"{}\" is required", key));

		const json& value = payload[key];
		if (value.is_boolean()) return value.get<bool>() ? "1" : "0";
		if (value.is_number()) return value.dump();
		if (value.is_string()) return value.get<std::string>();

		throw CommandError(fmt::format("\"{}\" must be a string, a number or a boolean", key));
	}
}

void WSC::RegisterServerCommands()
{
	commandHandlers["ping"] = [](const json& payload) -> json { return nullptr; };

	// Send the current state of every topic again (match, scoreboard, spectator, players, entities)
	commandHandlers["resync"] = [this](const json& payload) -> json {
		ResendState();
		return nullptr;
	};

	commandHandlers["spectate"] = [this](const json& payload) -> json {
		if (!JoinSpectators(true)) throw CommandError("Not in a match");
		return nullptr;
	};

	commandHandlers["ready"] = [this](const json& payload) -> json {
		if (!ReadyUp(true)) throw CommandError("Not in a match");
		return nullptr;
	};

	commandHandlers["hide_hud"] = [this](const json& payload) -> json {
		if (!HideSpectatorHud(0, true)) throw CommandError("Not spectating a match");
		return nullptr;
	};

	// Payload: { "uid": "<uid>", "name": "<name>" }, at least one of them
	commandHandlers["focus_player"] = [this](const json& payload) -> json {
		const std::string uid = GetOptionalString(payload, "uid");
		const std::string name = GetOptionalString(payload, "name");
		if (uid.empty() && name.empty()) throw CommandError("\"uid\" or \"name\" is required");

		if (!FocusPlayer(uid, name)) throw CommandError("Player not found or camera not available");
		return nullptr;
	};

	// Payload: { "name": "<cvar>", "value": <string, number or boolean> }
	commandHandlers["set_setting"] = [this](const json& payload) -> json {
		const std::string name = GetRequiredString(payload, "name");
		const std::string value = GetCvarValue(payload, "value");

		auto allowed = std::find_if(std::begin(SERVER_SETTINGS), std::end(SERVER_SETTINGS), [&name](const char* setting) { return name == setting; });
		if (allowed == std::end(SERVER_SETTINGS)) throw CommandError(fmt::format("Setting \"{}\" can't be changed by the server", name));

		CVarWrapper cvar = GetCvar(*allowed);
		if (!cvar) throw CommandError(fmt::format("Setting \"{}\" not found", name));

		cvar.setValue(value);
		return nullptr;
	};

	// Name of the replay of the current match, or of the next one if the current match is ended
	// Payload: { "blue": "<team>", "orange": "<team>", "game": <number> } formatted with wsc_replay_name_format,
	// or { "name": "<replay name>" } to give the whole name
	commandHandlers["set_replay_name"] = [this](const json& payload) -> json {
		std::string name = GetOptionalString(payload, "name");

		if (name.empty())
		{
			CVarWrapper format = GetCvar(Cvars::ReplayNameFormat);
			name = FormatReplayName(format ? format.getStringValue() : "", {
				{ "blue", GetRequiredString(payload, "blue") },
				{ "orange", GetRequiredString(payload, "orange") },
				{ "game", std::to_string(GetRequiredInt(payload, "game", 1, MAX_GAME_NUMBER)) }
			});
		}

		name = SanitizeReplayName(name);
		if (name.empty()) throw CommandError("The replay name is empty");

		replayName = name;

		json result = json::object();
		result["name"] = replayName;
		result["applied"] = !match.ended && ApplyReplayName(); // false: applied when the next match starts
		return result;
	};

	commandHandlers["clear_replay_name"] = [this](const json& payload) -> json {
		replayName.clear();
		return nullptr;
	};
}

// Called from the ixwebsocket thread: only validate and queue the command
void WSC::OnServerMessage(const std::string& message)
{
	if (message.size() > MAX_MESSAGE_SIZE)
	{
		SendCommandResponse("", nullptr, "Message too large");
		return;
	}

	if (GetJsonDepth(message) > MAX_JSON_DEPTH)
	{
		SendCommandResponse("", nullptr, "JSON too deep");
		return;
	}

	const json data = json::parse(message, nullptr, false);
	if (data.is_discarded() || !data.is_object())
	{
		SendCommandResponse("", nullptr, "Invalid JSON object");
		return;
	}

	// Only strings and numbers are sent back as id
	json id = data.contains("id") && (data["id"].is_string() || data["id"].is_number()) ? data["id"] : json();

	if (!data.contains("command") || !data["command"].is_string())
	{
		SendCommandResponse("", id, "\"command\" must be a string");
		return;
	}

	ServerCommand command;
	command.name = data["command"].get<std::string>();
	command.payload = data.contains("payload") ? data["payload"] : json::object();
	command.id = std::move(id);

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
		else result = handler->second(command.payload);
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
