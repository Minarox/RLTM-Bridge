#include "pch.h"
#include "RLTMBridge.h"

/*
	--- Local monitor ---

	WebSocket server on 127.0.0.1 (never reachable from the network), disabled by default, for the monitor page
	of the documentation: it receives the same messages as the server and can send the same commands, with the
	same validation and permissions (WebSocket/ServerCommands.cpp).

	A WebSocket connection isn't limited by the browser to the website of the page: any website open in the
	browser could connect to localhost. Only the documentation site and the local pages are accepted
	(Protocol::IsAllowedMonitorOrigin).
*/

namespace
{
	const std::string MONITOR_HOST = "127.0.0.1";

	// Monitor pages connected at the same time
	constexpr size_t MAX_MONITOR_CLIENTS = 8;

	constexpr int DEFAULT_MONITOR_PORT = 8765;
}

void RLTMBridge::ApplyMonitorSettings()
{
	StopMonitor();
	if (!IsCvarEnabled(Cvars::Monitor)) return;

	CVarWrapper port = GetCvar(Cvars::MonitorPort);
	StartMonitor(port ? port.getIntValue() : DEFAULT_MONITOR_PORT);
}

void RLTMBridge::StartMonitor(int port)
{
	auto server = std::make_shared<ix::WebSocketServer>(port, MONITOR_HOST, ix::SocketServer::kDefaultTcpBacklog, MAX_MONITOR_CLIENTS);

	// Called from the threads of the server
	server->setOnClientMessageCallback([this](std::shared_ptr<ix::ConnectionState> state, ix::WebSocket& client, const ix::WebSocketMessagePtr& msg) {
		switch (msg->type)
		{
			case ix::WebSocketMessageType::Open:
			{
				auto originHeader = msg->openInfo.headers.find("Origin");
				const std::string origin = originHeader != msg->openInfo.headers.end() ? originHeader->second : "";

				if (!Protocol::IsAllowedMonitorOrigin(origin))
				{
					LOG("Monitor: connection from {} rejected", origin);
					client.close(4003, "Origin not allowed");
					return;
				}

				{
					std::lock_guard<std::mutex> lock(monitorMutex);
					monitorConnections.insert(&client);
				}

				LOG("Monitor: page connected ({})", origin.empty() ? "no origin" : origin);
				SendStateToMonitorClient(client);
				break;
			}

			case ix::WebSocketMessageType::Message:
			{
				bool allowed;
				{
					std::lock_guard<std::mutex> lock(monitorMutex);
					allowed = monitorConnections.count(&client) > 0;
				}

				// Same validation, queue and permissions as the commands of the server
				if (allowed) OnServerMessage(msg->str);
				break;
			}

			case ix::WebSocketMessageType::Close:
			{
				std::lock_guard<std::mutex> lock(monitorMutex);
				monitorConnections.erase(&client);
				break;
			}

			default:
				break;
		}
	});

	const auto [listening, error] = server->listen();
	if (!listening)
	{
		LOG("Monitor: can't listen on port {}: {}", port, error);

		std::lock_guard<std::mutex> lock(monitorMutex);
		monitorError = error;
		return;
	}

	server->start();

	{
		std::lock_guard<std::mutex> lock(monitorMutex);
		monitor = server;
		monitorError.clear();
	}

	LOG("Monitor: listening on ws://{}:{}", MONITOR_HOST, port);
}

void RLTMBridge::StopMonitor()
{
	std::shared_ptr<ix::WebSocketServer> server;
	{
		std::lock_guard<std::mutex> lock(monitorMutex);
		server = std::move(monitor);
		monitor.reset();
		monitorConnections.clear();
		monitorError.clear();
	}

	// Outside of the lock: stopping waits for the threads of the server, which use it
	if (server)
	{
		server->stop();
		LOG("Monitor stopped");
	}
}

void RLTMBridge::BroadcastToMonitor(const std::string& message)
{
	std::shared_ptr<ix::WebSocketServer> server;
	std::set<const ix::WebSocket*> allowed;
	{
		std::lock_guard<std::mutex> lock(monitorMutex);
		if (!monitor || monitorConnections.empty()) return;

		server = monitor;
		allowed = monitorConnections;
	}

	for (const std::shared_ptr<ix::WebSocket>& client : server->getClients())
	{
		if (allowed.count(client.get())) client->send(message);
	}
}

// Same as a connection to the server: the plugin message, then the current state of the game
void RLTMBridge::SendStateToMonitorClient(ix::WebSocket& client)
{
	json plugin;
	{
		std::lock_guard<std::mutex> lock(pluginInfoMutex);
		plugin = pluginInfo;
	}
	client.send(Protocol::BuildMessage("plugin", plugin));

	json cache;
	{
		std::lock_guard<std::mutex> lock(sentCacheMutex);
		cache = sentCache;
	}

	for (auto& [topic, payload] : cache.items())
		client.send(Protocol::BuildMessage(topic, payload));
}

size_t RLTMBridge::GetMonitorClientsCount()
{
	std::lock_guard<std::mutex> lock(monitorMutex);
	return monitor ? monitorConnections.size() : 0;
}

std::string RLTMBridge::GetMonitorError()
{
	std::lock_guard<std::mutex> lock(monitorMutex);
	return monitorError;
}
