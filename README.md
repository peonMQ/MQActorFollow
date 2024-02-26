# MQActorFollow

This plugin allows you to follow a player movement more precisely using MQ actors

## Getting Started

Quick start instructions to get users up and going

```txt
/plugin MQActorFollow
```

### Commands

Describe the commands available and how to use them.

```txt
========= Actor Advance Pathing Help =========
/actfollow JohnDoe
/actfollow pause
/actfollow resume
/actfollow off
===============================================
```


### TLO

```txt
${ActorAdvPath.IsActive} - Plugin Loaded and ready
${ActorAdvPath.FollowState} - FollowState, 0 = off, 1 = on
${ActorAdvPath.Status} - Status 0 = off , 1 = on , 2 = paused
${ActorAdvPath.WaypointsCount} - Total Number of current waypoints
${ActorAdvPath.IsFollowing} - BOOL Is following spawn
${ActorAdvPath.IsPaused} - BOOL Is paused
```


## Authors

* **ProjectEon** - *Initial work*

## Acknowledgments

* Inspiration from MQ2NetAdvPath and MQ2AdvPath as well as MQ2Nav
