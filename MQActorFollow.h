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

#include "routing/PostOffice.h"
#include "ActorFollowee.pb.h"

#include <Windows.h>
#include <wincrypt.h>

#ifdef _DEBUG
#pragma comment(lib, "libprotobufd")
#else
#pragma comment(lib, "libprotobuf")
#endif

#pragma comment(lib, "login")

std::vector<std::shared_ptr<postoffice::Address>> Subscribers;
std::queue<std::shared_ptr<proto::actorfollowee::Position>> Positions;
postoffice::DropboxAPI DropBox;

void MoveForward(bool hold);
void MoveBackwards(bool hold);
void StrafeLeft(bool hold);
void StrafeRight(bool hold);