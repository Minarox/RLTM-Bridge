#include "pch.h"
#include "WSC.h"

namespace
{
	// Characters of a message written in the console in debug mode
	constexpr size_t MAX_DEBUG_LOG_SIZE = 500;

	const char* GetTopicName(Topic topic)
	{
		switch (topic)
		{
			case Topic::Plugin: return "plugin";
			case Topic::Match: return "match";
			case Topic::Scoreboard: return "scoreboard";
			case Topic::Spectator: return "spectator";
			case Topic::Statistic: return "statistic";
			case Topic::Entities: return "entities";
			case Topic::Players: return "players";
			case Topic::Stats: return "stats";
			case Topic::Event: return "event";
			case Topic::Result: return "result";
			case Topic::Response: return "response";
		}

		return "unknown";
	}
}

/*
	--- WebSocket ---
*/

void WSC::InitSocket()
{
	socket.addSubProtocol("json");
	ConfigureSocket();

	// Called from the ixwebsocket thread
	socket.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
		switch (msg->type)
		{
			case ix::WebSocketMessageType::Open:
			{
				LOG("WebSocket connection opened");
				SetSocketStatus();

				// Let the server check that it understands the messages, then send it the current state of the game
				json plugin;
				{
					std::lock_guard<std::mutex> lock(pluginInfoMutex);
					plugin = pluginInfo;
				}
				Send(Topic::Plugin, plugin);

				ResendState();
				break;
			}

			case ix::WebSocketMessageType::Close:
				LOG("WebSocket connection closed: {} ({})", msg->closeInfo.reason, msg->closeInfo.code);
				SetSocketStatus();
				break;

			case ix::WebSocketMessageType::Error:
			{
				// The same error is repeated on each reconnection retry, only log it once
				bool isNewError;
				{
					std::lock_guard<std::mutex> lock(socketStatusMutex);
					isNewError = socketLastError != msg->errorInfo.reason;
				}

				if (isNewError)
				{
					LOG("WebSocket error: {}", msg->errorInfo.reason);
					SetSocketStatus(msg->errorInfo.reason);
				}
				break;
			}

			case ix::WebSocketMessageType::Message:
				OnServerMessage(msg->str);
				break;

			default:
				break;
		}
	});
}

void WSC::ConfigureSocket()
{
	if (IsCvarEnabled(Cvars::AutoReconnect)) socket.enableAutomaticReconnection();
	else socket.disableAutomaticReconnection();

	if (IsCvarEnabled(Cvars::PerMessageDeflate)) socket.enablePerMessageDeflate();
	else socket.disablePerMessageDeflate();

	if (CVarWrapper cvar = GetCvar(Cvars::HandshakeTimeout)) socket.setHandshakeTimeout(cvar.getIntValue());
	if (CVarWrapper cvar = GetCvar(Cvars::PingInterval)) socket.setPingInterval(cvar.getIntValue());
	if (CVarWrapper cvar = GetCvar(Cvars::MinWaitBetweenRetries)) socket.setMinWaitBetweenReconnectionRetries(cvar.getIntValue());
	if (CVarWrapper cvar = GetCvar(Cvars::MaxWaitBetweenRetries)) socket.setMaxWaitBetweenReconnectionRetries(cvar.getIntValue());

	// Identity of this client, sent in the headers of the connection so the server can reject an unknown client
	const std::string clientName = GetClientName();
	ix::WebSocketHttpHeaders headers;
	headers["X-WSC-Client"] = clientName;

	CVarWrapper token = GetCvar(Cvars::Token);
	const std::string tokenValue = token ? Protocol::SanitizeHeaderValue(token.getStringValue()) : "";
	if (!tokenValue.empty()) headers["Authorization"] = "Bearer " + tokenValue;
	socket.setExtraHeaders(headers);

	// Built here (game thread) because it is sent from the ixwebsocket thread
	std::lock_guard<std::mutex> lock(pluginInfoMutex);
	pluginInfo = json::object();
	pluginInfo["version"] = plugin_version;
	pluginInfo["protocol"] = protocol_version;
	pluginInfo["client"] = clientName;
}

// Name of this client for the server: the wsc_client_name cvar, or the name of the computer
std::string WSC::GetClientName()
{
	CVarWrapper cvar = GetCvar(Cvars::ClientName);
	std::string name = cvar ? Protocol::SanitizeHeaderValue(cvar.getStringValue()) : "";
	if (!name.empty()) return name;

	const char* computerName = std::getenv("COMPUTERNAME");
	name = computerName ? Protocol::SanitizeHeaderValue(computerName) : "";

	return name.empty() ? "WSC" : name;
}

void WSC::Connect()
{
	CVarWrapper url = GetCvar(Cvars::Url);
	if (!url || url.getStringValue().empty())
	{
		LOG("Can't connect to the server: WebSocket URL is empty");
		return;
	}

	if (socketRunning) socket.stop();

	socket.setUrl(url.getStringValue());
	socket.start();
	socketRunning = true;
	SetSocketStatus();

	LOG("Connecting to {}", url.getStringValue());
}

void WSC::Disconnect()
{
	if (!socketRunning) return;

	socketRunning = false;
	socket.stop();
	SetSocketStatus();
}

void WSC::Send(Topic topic, const json& payload)
{
	const std::string topicName = GetTopicName(topic);

	// The states are cached (skip the duplicates, sent again on each connection), the other topics are one-shot messages
	if (topic != Topic::Plugin && topic != Topic::Statistic && topic != Topic::Event && topic != Topic::Response)
	{
		std::lock_guard<std::mutex> lock(sentCacheMutex);

		json& cached = sentCache[topicName];
		if (cached == payload) return;
		cached = payload;
	}

	if (socket.getReadyState() != ix::ReadyState::Open) return;

	const std::string message = Protocol::BuildMessage(topicName, payload);
	CountSent(socket.send(message));

	// The entities are sent several times per second, they would flood the console
	if (debug && topic != Topic::Entities && topic != Topic::Stats) LOG("[debug] Sent: {}", message.substr(0, MAX_DEBUG_LOG_SIZE));
}

// Sends the current state of the game to the server, on (re)connection or when asked by the server
void WSC::ResendState()
{
	json cache;
	{
		std::lock_guard<std::mutex> lock(sentCacheMutex);
		cache = sentCache;
	}

	if (socket.getReadyState() != ix::ReadyState::Open) return;

	std::string topics;
	for (auto& [topic, payload] : cache.items())
	{
		CountSent(socket.send(Protocol::BuildMessage(topic, payload)));
		topics += (topics.empty() ? "" : ", ") + topic;
	}

	if (debug) LOG("[debug] State sent again: {}", topics);
}

// Statistics shown in the settings window
void WSC::CountSent(const ix::WebSocketSendInfo& info)
{
	if (!info.success) return;

	messagesSent++;
	bytesSent += info.wireSize;
}

void WSC::SetSocketStatus(const std::string& error)
{
	std::lock_guard<std::mutex> lock(socketStatusMutex);

	socketLastError = error;
	socketStatusSince = std::chrono::system_clock::now();
}
