#include "pch.h"
#include "RLTMBridge.h"
#include "Game/GameUtils.h"

#include <fstream>

#ifdef _WIN32
#include <shlobj.h> // SHGetKnownFolderPath (folder of the replays)
#pragma comment (lib, "shell32.lib")
#pragma comment (lib, "ole32.lib")
#endif

using namespace GameUtils;
using namespace std::chrono_literals;

namespace
{
	// The game saves the replay at the end of the match: it is waited for until it stops growing
	constexpr auto REPLAY_WAIT = 3min;
	constexpr auto REPLAY_POLL = 2s;

	// Retries of the upload when the server can't be reached or is busy
	constexpr std::chrono::seconds RETRY_DELAYS[] = { 5s, 15s, 60s };
}


/*
	--- Replays sent to the server ---
	At the end of a match, the game thread only reads the guid of the match and the id of its replay.
	A worker thread waits for the file saved by the game and sends it to the server over HTTP
	(the WebSocket stays for the live data), with retries.
*/

void RLTMBridge::StartReplayUploader()
{
	{
		std::lock_guard<std::mutex> lock(uploadMutex);
		uploadStopping = false;
	}
	uploadThread = std::thread([this]() { RunReplayUploader(); });
}

void RLTMBridge::StopReplayUploader()
{
	{
		std::lock_guard<std::mutex> lock(uploadMutex);
		uploadStopping = true;
		uploadQueue.clear();
		if (uploadRequest) uploadRequest->cancel = true;
	}
	uploadCondition.notify_all();

	if (uploadThread.joinable()) uploadThread.join();
}

void RLTMBridge::QueueReplayUpload(ServerWrapper server)
{
	if (!server || match.replayQueued) return;
	if (!IsCvarEnabled(Cvars::UploadReplays) || !IsCvarEnabled(Cvars::AutoSaveReplays)) return;
	match.replayQueued = true;

	ReplayUpload upload;

	const json guid = GetMatchGuid(server);
	if (!guid.is_string())
	{
		SetUploadStatus("Replay not sent: the match has no guid (offline match)");
		return;
	}
	upload.guid = guid.get<std::string>();

	CVarWrapper url = GetCvar(Cvars::Url);
	upload.url = Protocol::GetReplayUploadUrl(url ? url.getStringValue() : "", upload.guid);
	if (upload.url.empty())
	{
		SetUploadStatus("Replay not sent: invalid server URL");
		return;
	}

	upload.folder = GetReplaysFolder();
	if (upload.folder.empty())
	{
		SetUploadStatus("Replay not sent: replays folder not found");
		return;
	}

	// Name of the file saved by the game
	ReplayDirectorWrapper replayDirector = server.GetReplayDirector();
	if (replayDirector)
	{
		ReplaySoccarWrapper replay = replayDirector.GetReplay();
		if (replay) upload.replayId = replay.GetId().ToString();
	}

	upload.matchEnd = std::filesystem::file_time_type::clock::now();
	CVarWrapper token = GetCvar(Cvars::Token);
	upload.token = token ? token.getStringValue() : "";
	upload.clientName = GetClientName();

	{
		std::lock_guard<std::mutex> lock(uploadMutex);
		if (uploadStopping) return;
		uploadQueue.push_back(upload);
	}
	uploadCondition.notify_all();

	SetUploadStatus("Waiting for the replay of the match");
}

// Waits for the duration, false if the plugin is unloading
bool RLTMBridge::WaitForUploader(std::chrono::milliseconds duration)
{
	std::unique_lock<std::mutex> lock(uploadMutex);
	return !uploadCondition.wait_for(lock, duration, [this]() { return uploadStopping; });
}

void RLTMBridge::RunReplayUploader()
{
	while (true)
	{
		ReplayUpload upload;
		{
			std::unique_lock<std::mutex> lock(uploadMutex);
			uploadCondition.wait(lock, [this]() { return uploadStopping || !uploadQueue.empty(); });
			if (uploadStopping) return;

			upload = uploadQueue.front();
			uploadQueue.pop_front();
		}

		// The file is ready when its size stops changing
		std::filesystem::path file;
		std::uintmax_t lastSize = 0;
		const auto deadline = std::chrono::steady_clock::now() + REPLAY_WAIT;
		while (true)
		{
			const std::filesystem::path found = FindReplayFile(upload);
			std::error_code error;
			const std::uintmax_t size = found.empty() ? 0 : std::filesystem::file_size(found, error);
			if (!found.empty() && !error && size > 0 && size == lastSize)
			{
				file = found;
				break;
			}
			lastSize = error ? 0 : size;

			if (std::chrono::steady_clock::now() > deadline) break;
			if (!WaitForUploader(REPLAY_POLL)) return;
		}

		if (file.empty())
		{
			SetUploadStatus("Replay not sent: replay file not found (is the replay saved by the game?)");
			LOG("Replay of match {} not found in {}", upload.guid, upload.folder.u8string());
			continue;
		}

		for (size_t attempt = 0;; attempt++)
		{
			bool retry = false;
			if (UploadReplay(upload, file, retry) || !retry || attempt >= std::size(RETRY_DELAYS)) break;
			if (!WaitForUploader(RETRY_DELAYS[attempt])) return;
		}
	}
}

// Replay saved by the game: the file named by the id of the replay, or the newest replay saved after the end of the match
std::filesystem::path RLTMBridge::FindReplayFile(const ReplayUpload& upload)
{
	std::error_code error;

	if (!upload.replayId.empty())
	{
		const std::filesystem::path file = upload.folder / std::filesystem::u8path(upload.replayId + ".replay");
		if (std::filesystem::is_regular_file(file, error)) return file;
	}

	std::filesystem::path newest;
	std::filesystem::file_time_type newestTime = upload.matchEnd - 10s;
	for (const auto& entry : std::filesystem::directory_iterator(upload.folder, error))
	{
		std::error_code entryError;
		if (!entry.is_regular_file(entryError) || entry.path().extension() != ".replay") continue;

		const auto time = entry.last_write_time(entryError);
		if (!entryError && time >= newestTime)
		{
			newest = entry.path();
			newestTime = time;
		}
	}

	return newest;
}

bool RLTMBridge::UploadReplay(const ReplayUpload& upload, const std::filesystem::path& file, bool& retry)
{
	retry = false;

	std::error_code error;
	const std::uintmax_t size = std::filesystem::file_size(file, error);
	if (error || size == 0 || size > Protocol::MAX_REPLAY_FILE_SIZE)
	{
		SetUploadStatus(fmt::format("Replay not sent: invalid file size ({} bytes)", error ? 0 : size));
		return false;
	}

	std::string body(static_cast<size_t>(size), '\0');
	std::ifstream input(file, std::ios::binary);
	if (!input.read(body.data(), static_cast<std::streamsize>(body.size())))
	{
		SetUploadStatus("Replay not sent: the file can't be read");
		retry = true;
		return false;
	}

	ix::HttpClient client;
	ix::HttpRequestArgsPtr args = client.createRequest(upload.url, ix::HttpClient::kPut);
	args->extraHeaders["Content-Type"] = "application/octet-stream";
	args->extraHeaders["X-RLTM-Client"] = Protocol::SanitizeHeaderValue(upload.clientName);
	const std::string token = Protocol::SanitizeHeaderValue(upload.token);
	if (!token.empty()) args->extraHeaders["Authorization"] = "Bearer " + token;
	args->connectTimeout = 10;
	args->transferTimeout = 120;
	args->compress = false;
	args->followRedirects = false;

	{
		std::lock_guard<std::mutex> lock(uploadMutex);
		if (uploadStopping) return false;
		uploadRequest = args;
	}
	const ix::HttpResponsePtr response = client.request(upload.url, ix::HttpClient::kPut, body, args);
	{
		std::lock_guard<std::mutex> lock(uploadMutex);
		uploadRequest = nullptr;
	}

	const int status = response ? response->statusCode : 0;
	if (status == 200 || status == 201)
	{
		SetUploadStatus(fmt::format("Replay of the last match sent ({:.1f} MB)", static_cast<double>(size) / (1024 * 1024)));
		LOG("Replay of match {} sent to the server", upload.guid);
		return true;
	}

	// Server unreachable, busy or failing: retried. Refused (token, file...): not retried
	retry = status == 0 || status == 408 || status == 429 || status >= 500;

	std::string reason = status ? std::to_string(status) : (response ? response->errorMsg : "no response");
	if (status && response && !response->body.empty() && response->body.size() <= 64) reason += " " + response->body;
	SetUploadStatus("Replay not sent: " + reason + (retry ? " (retrying)" : ""));
	LOG("Replay of match {} not sent: {}", upload.guid, reason);
	return false;
}

// Folder where the game saves the replays: the setting, or Documents\My Games\Rocket League\TAGame\Demos
std::filesystem::path RLTMBridge::GetReplaysFolder()
{
	CVarWrapper folder = GetCvar(Cvars::ReplaysFolder);
	const std::string value = folder ? folder.getStringValue() : "";
	if (!value.empty()) return std::filesystem::u8path(value);

#ifdef _WIN32
	PWSTR documents = nullptr;
	std::filesystem::path path;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &documents)))
		path = std::filesystem::path(documents) / "My Games" / "Rocket League" / "TAGame" / "Demos";
	CoTaskMemFree(documents);
	return path;
#else
	return {};
#endif
}

void RLTMBridge::SetUploadStatus(const std::string& status)
{
	std::lock_guard<std::mutex> lock(uploadStatusMutex);
	uploadStatus = status;
}

std::string RLTMBridge::GetUploadStatus()
{
	std::lock_guard<std::mutex> lock(uploadStatusMutex);
	return uploadStatus;
}
