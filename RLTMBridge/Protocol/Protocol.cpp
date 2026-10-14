// Not using the precompiled header: this file doesn't depend on the BakkesMod SDK
#include "Protocol.h"

#include <algorithm>
#include <cmath>

#include "fmt/format.h"

namespace Protocol
{
	std::string BuildMessage(const std::string& topic, const json& payload)
	{
		json message = json::object();
		message["topic"] = topic;
		message["payload"] = payload;

		return message.dump(-1, ' ', false, json::error_handler_t::replace);
	}

	std::string ParseCommand(const std::string& message, Command& command)
	{
		if (message.size() > MAX_MESSAGE_SIZE) return "Message too large";
		if (GetJsonDepth(message) > MAX_JSON_DEPTH) return "JSON too deep";

		const json data = json::parse(message, nullptr, false);
		if (data.is_discarded() || !data.is_object()) return "Invalid JSON object";

		// Only strings and numbers are sent back as id
		auto id = data.find("id");
		if (id != data.end() && (id->is_string() || id->is_number())) command.id = *id;

		auto name = data.find("command");
		if (name == data.end() || !name->is_string()) return "\"command\" must be a string";
		command.name = name->get<std::string>();

		auto payload = data.find("payload");
		command.payload = payload != data.end() ? *payload : json::object();

		return "";
	}

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
		if (!payload.is_object()) return "";

		auto value = payload.find(key);
		if (value == payload.end() || value->is_null()) return "";
		if (!value->is_string()) throw CommandError(fmt::format("\"{}\" must be a string", key));

		return value->get<std::string>();
	}

	std::string GetRequiredString(const json& payload, const char* key)
	{
		std::string value = GetOptionalString(payload, key);
		if (value.empty()) throw CommandError(fmt::format("\"{}\" is required", key));

		return value;
	}

	int GetRequiredInt(const json& payload, const char* key, int min, int max)
	{
		auto value = payload.is_object() ? payload.find(key) : payload.end();
		if (!payload.is_object() || value == payload.end() || !value->is_number_integer())
			throw CommandError(fmt::format("\"{}\" must be an integer", key));

		const json::number_integer_t number = value->get<json::number_integer_t>();
		if (number < min || number > max) throw CommandError(fmt::format("\"{}\" must be between {} and {}", key, min, max));

		return static_cast<int>(number);
	}

	std::string GetCvarValue(const json& payload, const char* key)
	{
		auto value = payload.is_object() ? payload.find(key) : payload.end();
		if (!payload.is_object() || value == payload.end()) throw CommandError(fmt::format("\"{}\" is required", key));

		if (value->is_boolean()) return value->get<bool>() ? "1" : "0";
		if (value->is_number()) return value->dump();
		if (value->is_string()) return value->get<std::string>();

		throw CommandError(fmt::format("\"{}\" must be a string, a number or a boolean", key));
	}

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

			const size_t last = sanitized.find_last_not_of(' ');
			sanitized.resize(last == std::string::npos ? 0 : last + 1);
		}

		return sanitized;
	}

	std::string SanitizeHeaderValue(const std::string& value)
	{
		std::string sanitized;
		for (char c : value)
		{
			const unsigned char byte = static_cast<unsigned char>(c);
			if (byte >= 0x20 && byte != 0x7F) sanitized += c;
		}

		const size_t first = sanitized.find_first_not_of(' ');
		if (first == std::string::npos) return "";

		return sanitized.substr(first, sanitized.find_last_not_of(' ') - first + 1);
	}

	bool IsAllowedMonitorOrigin(const std::string& origin)
	{
		if (origin.empty() || origin == "null" || origin == MONITOR_SITE_ORIGIN) return true;

		// Local page, with any port: http://localhost:8000, http://127.0.0.1:8000
		for (const std::string host : { "http://localhost", "http://127.0.0.1" })
		{
			if (origin.compare(0, host.size(), host) != 0) continue;

			const std::string rest = origin.substr(host.size());
			if (rest.empty()) return true;
			if (rest[0] == ':' && rest.size() > 1 && rest.find_first_not_of("0123456789", 1) == std::string::npos) return true;
		}

		return false;
	}

	std::string ToHexColor(float r, float g, float b)
	{
		auto component = [](float value) {
			if (!std::isfinite(value)) value = 0.0f;
			return static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
		};

		return fmt::format("#{:02X}{:02X}{:02X}", component(r), component(g), component(b));
	}

	bool IsValidMatchGuid(const std::string& guid)
	{
		if (guid.empty() || guid.size() > 64) return false;

		for (const char c : guid)
		{
			if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')) return false;
		}

		return true;
	}

	std::string GetReplayUploadUrl(const std::string& webSocketUrl, const std::string& guid)
	{
		if (!IsValidMatchGuid(guid)) return "";

		std::string scheme;
		std::string rest;
		if (webSocketUrl.rfind("ws://", 0) == 0)
		{
			scheme = "http://";
			rest = webSocketUrl.substr(5);
		}
		else if (webSocketUrl.rfind("wss://", 0) == 0)
		{
			scheme = "https://";
			rest = webSocketUrl.substr(6);
		}
		else return "";

		// Host and port: up to the path, the query or the fragment
		const size_t hostEnd = rest.find_first_of("/?#");
		const std::string host = rest.substr(0, hostEnd);
		if (host.empty() || host.find_first_of(" \t\r\n@") != std::string::npos) return "";

		// Token of the query, kept as is (already encoded in the URL)
		std::string token;
		const size_t queryStart = rest.find('?');
		if (queryStart != std::string::npos)
		{
			const size_t queryEnd = rest.find('#', queryStart);
			const std::string query = rest.substr(queryStart + 1, queryEnd == std::string::npos ? std::string::npos : queryEnd - queryStart - 1);

			size_t start = 0;
			while (start <= query.size())
			{
				const size_t end = std::min(query.find('&', start), query.size());
				const std::string parameter = query.substr(start, end - start);
				if (parameter.rfind("token=", 0) == 0) token = parameter.substr(6);
				start = end + 1;
			}
		}

		std::string url = scheme + host + "/api/replays/" + guid;
		if (!token.empty()) url += "?token=" + token;
		return url;
	}
}
