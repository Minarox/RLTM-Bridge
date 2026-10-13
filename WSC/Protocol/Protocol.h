#pragma once

// Messages format between the plugin and the server, without any dependency on the BakkesMod SDK
// so it can be unit tested (see tests/ProtocolTests.cpp)

#include <map>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace Protocol
{
	using json = nlohmann::json;

	// Larger messages are rejected without being parsed
	constexpr size_t MAX_MESSAGE_SIZE = 16 * 1024;

	// Deeper JSON is rejected without being parsed (stack overflow protection)
	constexpr int MAX_JSON_DEPTH = 32;

	// Maximum size of a replay name in bytes (UTF-8)
	constexpr size_t MAX_REPLAY_NAME_SIZE = 128;

	// Maximum game number of a series
	constexpr int MAX_GAME_NUMBER = 99;

	// Replay name format used when the format is empty
	inline const std::string DEFAULT_REPLAY_NAME_FORMAT = "{blue} vs {orange} - Match {game}";

	// Command received from the server: { "command": "<name>", "payload": { ... }, "id": <optional string or number> }
	struct Command
	{
		std::string name;
		json payload = json::object();
		json id; // Sent back in the response, null if missing or invalid
	};

	// Error of a command, its message is sent back to the server
	struct CommandError : std::runtime_error
	{
		using std::runtime_error::runtime_error;
	};

	// Message sent to the server: { "topic": "<topic>", "payload": <data> }, the invalid UTF-8 characters are replaced
	std::string BuildMessage(const std::string& topic, const json& payload);

	// Validates and parses a message of the server, returns the error (empty if the command is valid).
	// The id is set as soon as it is valid, so even a rejected command can be answered with its id.
	std::string ParseCommand(const std::string& message, Command& command);

	// Nesting depth of the arrays and objects of a JSON text, without parsing it
	int GetJsonDepth(const std::string& text);

	// Parameters of a command, throw a CommandError when invalid
	std::string GetOptionalString(const json& payload, const char* key);
	std::string GetRequiredString(const json& payload, const char* key);
	int GetRequiredInt(const json& payload, const char* key, int min, int max);
	std::string GetCvarValue(const json& payload, const char* key); // String, number or boolean

	// Replaces the "{key}" placeholders of the format in a single pass (the values can contain braces)
	std::string FormatReplayName(const std::string& format, const std::map<std::string, std::string>& values);

	// Removes the control characters and the surrounding spaces, limits the size without cutting a UTF-8 character
	std::string SanitizeReplayName(const std::string& name);

	// Value of an HTTP header: without the control characters (no header injection) nor the surrounding spaces
	std::string SanitizeHeaderValue(const std::string& value);

	// "#RRGGBB" from color components between 0 and 1 (clamped)
	std::string ToHexColor(float r, float g, float b);
}
