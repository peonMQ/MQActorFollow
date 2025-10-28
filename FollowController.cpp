#include "FollowController.h"

namespace actorfollow {
	FollowController& FollowController::Controller() {
		static FollowController instance;
		return instance;
	}

	FollowController::FollowController()
		: state(FollowState::OFF), samePositionTimer(std::chrono::steady_clock::now()) {
		settings = GetSettings();
	}

	FollowState FollowController::GetState() const { return state; }
	void FollowController::SetState(FollowState newState) { state = newState; }

	bool FollowController::HasDestinations() const { return !positions.empty(); }
	void FollowController::EnqueueDestination(std::shared_ptr<proto::actorfollowee::Position> pos) {
		positions.push(pos);
	}

	std::shared_ptr<proto::actorfollowee::Position> FollowController::GetCurrentDestination() {
		if (positions.empty()) return nullptr;
		auto newDestination = positions.front();
		auto now = std::chrono::steady_clock::now();

		if (lastPosition == newDestination && now > samePositionTimer) {
			positions.pop();
			return nullptr;
		}
		else if (lastPosition != newDestination) {
			samePositionTimer = now + std::chrono::milliseconds(settings.waypoint_timeout_seconds * 1000);
			lastPosition = newDestination;
		}
		return lastPosition;
	}

	void FollowController::PopDestination() {
		if (!positions.empty()) positions.pop();
		if (positions.empty()) actorfollow::StopMoving();
	}

	// Movement
	void FollowController::TryFollowActor(PcClient* pcClient, const std::function<void()>& unsubscribeCallback) {
		if (pcClient && state == FollowState::ON)
		{
			if (auto pSpawn = pcClient->pSpawn)
			{
				if (auto destination = GetCurrentDestination()) {
					auto& settings = actorfollow::GetSettings();
					if (destination->zoneid() == pSpawn->Zone) {
						auto position = CVector3{ destination->x(), destination->y(), destination->z() };
						auto distance3d = GetDistance3D(pSpawn->X, pSpawn->Y, pSpawn->Z, position.X, position.Y, position.Z);
						if (distance3d < 0 || distance3d > settings.warp_alert_distance) {
							WriteChatf("[MQActorFollow] Possible warp detected, exiting (\aw%.5f\ax)...", distance3d);
							unsubscribeCallback();
							return;
						}

						if (distance3d > settings.waypoint_min_distance) {
							WriteChatf("[MQActorFollow] TryFollowActor (\aw%.5f\ax)...", distance3d);
							actorfollow::MoveTowards(pcClient, position);
						}
						else {
							PopDestination();
						}
					}
				}
			}
		}
	}

	void FollowController::ClearDestinations() {
		while (!positions.empty()) positions.pop();
	}

	void FollowController::StopFollowing() {
		state = FollowState::OFF;
		actorfollow::StopMoving();
		ClearDestinations();
	}

	void FollowController::InterruptFollowing(const std::function<void()>& unsubscribeCallback) {
		if (settings.autobreak) {
			unsubscribeCallback();
		}
		else if (settings.autopause) {
			state = FollowState::PAUSED;
		}
	}

	std::queue<std::shared_ptr<proto::actorfollowee::Position>> FollowController::GetPositionsCopy() const {
		return positions;
	}

} // namespace actorfollow
