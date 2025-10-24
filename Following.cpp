#pragma once
#include "Following.h"

namespace actorfollow
{
	std::shared_ptr<proto::actorfollowee::Position> GetNextDestination()
	{
		static std::chrono::steady_clock::time_point s_same_position_timer = std::chrono::steady_clock::now();
		static std::shared_ptr<proto::actorfollowee::Position> position;

		if (Positions.empty()) {
			return nullptr;
		}

		auto newDestination = Positions.front();
		auto now = std::chrono::steady_clock::now();
		if (position == newDestination && now > s_same_position_timer) {
			WriteChatf("[MQActorFollow]::Timed out trying to reach waypoint, removing from queue");
			Positions.pop();
			return nullptr;
		}
		else if (position != newDestination) {
			auto& settings = actorfollow::GetSettings();
			s_same_position_timer = now + std::chrono::milliseconds(settings.waypoint_timeout_seconds * 1000);
			position = newDestination;
		}

		return position;
	}

	void PopDestination(bool forceStop)
	{
		Positions.pop();
		if (Positions.empty() || forceStop) {
			actorfollow::StopMoving();
		}
	}

	void TryFollowActor(PcClient* pcClient) {
		if (pcClient && FollowingState == FollowState::ON)
		{
			if (auto pSpawn = pcClient->pSpawn)
			{
				if (auto destination = GetNextDestination()) {
					auto& settings = actorfollow::GetSettings();
					if (destination->zoneid() == pSpawn->Zone) {
						auto position = CVector3{ destination->x(), destination->y(), destination->z() };
						auto distance3d = GetDistance3D(pSpawn->X, pSpawn->Y, pSpawn->Z, position.X, position.Y, position.Z);
						if (distance3d < 0 || distance3d > settings.warp_alert_distance) {
							WriteChatf("[MQActorFollow] Possible warp detected, exiting (\aw%.5f\ax)...", distance3d);
							PopDestination(true);
							return;
						}

						if (distance3d > settings.waypoint_min_distance) {
							actorfollow::MoveTowards(pcClient, position);
						}
						else {
							PopDestination(false);
						}
					}
				}
			}
		}
	}

	void InterruptFollowing(const std::function<void()>& unsubscribeCallback)
	{
		auto& settings = actorfollow::GetSettings();
		if (settings.autobreak)
		{
			unsubscribeCallback();
		}
		else if (settings.autopause)
		{
			actorfollow::FollowingState = actorfollow::FollowState::PAUSED;
		}
	}
}