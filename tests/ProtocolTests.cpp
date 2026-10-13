// Unit tests of the messages format (RLTMBridge/Protocol), run by the CI: tests/run.sh
#include <cmath>
#include <functional>
#include <iostream>
#include <string>

#include "Protocol/Protocol.h"

using namespace Protocol;

namespace
{
	int failures = 0;

	void Check(bool condition, const std::string& name)
	{
		if (!condition)
		{
			failures++;
			std::cout << "FAILED: " << name << "\n";
		}
	}

	// Returns the message of the CommandError thrown by the function, empty if nothing is thrown
	std::string GetError(const std::function<void()>& function)
	{
		try
		{
			function();
		}
		catch (const CommandError& e)
		{
			return e.what();
		}

		return "";
	}

	std::string Parse(const std::string& message, Command& command)
	{
		command = Command();
		return ParseCommand(message, command);
	}

	void TestParseCommand()
	{
		Command command;

		Check(Parse(R"({"command":"focus_player","payload":{"name":"Bob"},"id":"42"})", command).empty(), "valid command");
		Check(command.name == "focus_player" && command.payload["name"] == "Bob" && command.id == "42", "valid command fields");

		Check(Parse(R"({"command":"ping"})", command).empty() && command.payload.is_object() && command.id.is_null(), "default payload and id");
		Check(Parse(R"({"command":"ping","id":7})", command).empty() && command.id == 7, "number id");
		Check(Parse(R"({"command":"ping","id":{"a":1}})", command).empty() && command.id.is_null(), "object id ignored");

		Check(Parse("{", command) == "Invalid JSON object", "truncated JSON");
		Check(Parse("[1,2]", command) == "Invalid JSON object", "array");
		Check(Parse("null", command) == "Invalid JSON object", "null");
		Check(Parse("\xff\xfe{}", command) == "Invalid JSON object", "invalid UTF-8");
		Check(Parse(R"({"command":42,"id":"x"})", command) == "\"command\" must be a string" && command.id == "x", "invalid command keeps the id");

		Check(Parse(std::string(MAX_MESSAGE_SIZE + 1, ' '), command) == "Message too large", "message too large");
		Check(Parse(std::string(MAX_JSON_DEPTH + 1, '[') + std::string(MAX_JSON_DEPTH + 1, ']'), command) == "JSON too deep", "JSON too deep");
		Check(Parse(std::string(MAX_JSON_DEPTH, '[') + std::string(MAX_JSON_DEPTH, ']'), command) == "Invalid JSON object", "maximum depth parsed");
	}

	void TestJsonDepth()
	{
		Check(GetJsonDepth("{}") == 1, "depth of an object");
		Check(GetJsonDepth(R"({"a":[{"b":1}]})") == 3, "nested depth");
		Check(GetJsonDepth(R"({"a":"[[[[\"{{{"})") == 1, "brackets in a string are ignored");
	}

	void TestParameters()
	{
		const json payload = { { "name", "Bob" }, { "number", 3 }, { "float", 1.5 }, { "flag", true }, { "empty", "" }, { "nothing", nullptr } };

		Check(GetOptionalString(payload, "name") == "Bob", "optional string");
		Check(GetOptionalString(payload, "missing").empty() && GetOptionalString(payload, "nothing").empty(), "missing optional string");
		Check(GetOptionalString(json::array(), "name").empty(), "optional string of a non-object payload");
		Check(GetError([&] { GetOptionalString(payload, "number"); }) == "\"number\" must be a string", "string type");
		Check(GetError([&] { GetRequiredString(payload, "empty"); }) == "\"empty\" is required", "required string");

		Check(GetRequiredInt(payload, "number", 1, 99) == 3, "required int");
		Check(GetError([&] { GetRequiredInt(payload, "float", 1, 99); }) == "\"float\" must be an integer", "int type");
		Check(GetError([&] { GetRequiredInt(payload, "number", 4, 99); }) == "\"number\" must be between 4 and 99", "int range");
		Check(GetError([&] { GetRequiredInt(json("text"), "number", 1, 99); }) == "\"number\" must be an integer", "int of a non-object payload");

		Check(GetCvarValue(payload, "flag") == "1" && GetCvarValue(payload, "number") == "3" && GetCvarValue(payload, "name") == "Bob", "cvar values");
		Check(GetError([&] { GetCvarValue(payload, "missing"); }) == "\"missing\" is required", "missing cvar value");
		Check(!GetError([&] { GetCvarValue(json{ { "value", json::array() } }, "value"); }).empty(), "invalid cvar value");
	}

	void TestReplayName()
	{
		const std::map<std::string, std::string> teams = { { "blue", "Équipe 1" }, { "orange", "Équipe 2" }, { "game", "3" } };

		Check(FormatReplayName("", teams) == "Équipe 1 vs Équipe 2 - Match 3", "default format");
		Check(FormatReplayName("{game}: {blue}/{orange}", teams) == "3: Équipe 1/Équipe 2", "custom format");
		Check(FormatReplayName("{blue} {unknown} {game", teams) == "Équipe 1 {unknown} {game", "unknown and unclosed placeholders");
		Check(FormatReplayName("{blue} vs {orange}", { { "blue", "A{orange}" }, { "orange", "B" } }) == "A{orange} vs B", "braces in the values");

		Check(SanitizeReplayName("  \tA\n vs B  ") == "A vs B", "control characters and spaces");
		Check(SanitizeReplayName("   ").empty(), "only spaces");

		std::string longName;
		for (int i = 0; i < 100; i++) longName += "é";
		const std::string sanitized = SanitizeReplayName(longName);
		Check(sanitized.size() <= MAX_REPLAY_NAME_SIZE && sanitized.size() % 2 == 0, "UTF-8 characters are not cut");
		Check(!json(sanitized).dump(-1, ' ', false, json::error_handler_t::strict).empty(), "truncated name is valid UTF-8");
	}

	void TestMessages()
	{
		Check(BuildMessage("match", { { "clock", 10 } }) == R"({"payload":{"clock":10},"topic":"match"})", "message format");
		Check(BuildMessage("players", json::array({ "\xff" })).find("\xef\xbf\xbd") != std::string::npos, "invalid UTF-8 replaced");
	}

	void TestHeaders()
	{
		Check(SanitizeHeaderValue(" Caster 1 ") == "Caster 1", "header spaces");
		Check(SanitizeHeaderValue("token\r\nX-Injected: 1") == "tokenX-Injected: 1", "header injection");
	}

	void TestColors()
	{
		Check(ToHexColor(1.0f, 0.5f, 0.0f) == "#FF8000", "hex color");
		Check(ToHexColor(2.0f, -1.0f, NAN) == "#FF0000", "clamped color");
	}
}

int main()
{
	TestParseCommand();
	TestJsonDepth();
	TestParameters();
	TestReplayName();
	TestMessages();
	TestHeaders();
	TestColors();

	if (failures) std::cout << failures << " test(s) failed\n";
	else std::cout << "All tests passed\n";

	return failures ? 1 : 0;
}
