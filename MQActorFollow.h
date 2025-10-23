/*
 * MacroQuest: The extension platform for EverQuest
 * Copyright (C) 2002-2023 MacroQuest Authors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#pragma once

#include <mq/Plugin.h>
#include "routing/PostOffice.h"
#include "MQActorFollowUI.h"
#include "Subscription.h"

#include <Windows.h>
#include <wincrypt.h>

#ifdef _DEBUG
#pragma comment(lib, "libprotobufd")
#else
#pragma comment(lib, "libprotobuf")
#endif

#pragma comment(lib, "login")

template <typename T>
std::vector<std::shared_ptr<T>> queueToVector(const std::queue<std::shared_ptr<T>>& q) {
	std::vector<std::shared_ptr<T>> result;
	std::queue<std::shared_ptr<T>> copy = q;  // copy the queue

	while (!copy.empty()) {
		result.push_back(copy.front());
		copy.pop();
	}

	return result;
}