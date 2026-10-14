#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX // windows.h (included by ixwebsocket) defines min and max macros breaking std::min and std::max
#define _CRT_SECURE_NO_WARNINGS
#include "bakkesmod/plugin/bakkesmodplugin.h"

#include <string>
#include <vector>
#include <functional>
#include <memory>

#include "imgui/imgui.h"

#include "fmt/core.h"
#include "fmt/ranges.h"

extern std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

template<typename... Args>
void LOG(fmt::format_string<Args...> format_str, Args&&... args)
{
	_globalCvarManager->log(fmt::format(format_str, std::forward<Args>(args)...));
}