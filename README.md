# MQActorFollow

This plugin allows you to follow a player movement more precisely using MQ actors. Both the follower and the player being followed must run the plugin.

## Getting Started

Quick start instructions to get users up and going

```txt
/plugin MQActorFollow
```

### Commands

Describe the commands available and how to use them.

```txt
========= Actor Advance Pathing Help =========
/actfollow JohnDoe  # follow by pc name
/actfollow 123      # follow by pc spawn id
/actfollow pause
/actfollow resume
/actfollow off
/actfollow ui       # toggles ui
===============================================
```


### TLO

```txt
${ActorFollow.IsActive} - Plugin Loaded and ready
${ActorFollow.FollowState} - FollowState, 0 = off, 1 = on
${ActorFollow.Status} - Status 0 = off , 1 = on , 2 = paused
${ActorFollow.WaypointsCount} - Total Number of current waypoints
${ActorFollow.IsFollowing} - BOOL Is following spawn
${ActorFollow.IsPaused} - BOOL Is paused
```


## Authors

* **ProjectEon** - *Initial work*

## Acknowledgments

* Inspiration from MQ2NetAdvPath and MQ2AdvPath as well as MQ2Nav
