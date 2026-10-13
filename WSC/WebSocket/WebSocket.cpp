#include "pch.h"
#include "WSC.h"

namespace
{
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
				json plugin = json::object();
				plugin["version"] = plugin_version;
				plugin["protocol"] = protocol_version;
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

	// Match, scoreboard, spectator, players and entities are states, the other topics are one-shot messages
	if (topic == Topic::Match || topic == Topic::Scoreboard || topic == Topic::Spectator || topic == Topic::Players || topic == Topic::Entities)
	{
		std::lock_guard<std::mutex> lock(sentCacheMutex);

		json& cached = sentCache[topicName];
		if (cached == payload) return;
		cached = payload;
	}

	if (socket.getReadyState() != ix::ReadyState::Open) return;

	CountSent(socket.send(Protocol::BuildMessage(topicName, payload)));
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

	for (auto& [topic, payload] : cache.items())
		CountSent(socket.send(Protocol::BuildMessage(topic, payload)));
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
