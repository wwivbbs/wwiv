# WWIVD HTTP API Enhancements - Changelog

## Overview

Enhanced WWIVD HTTP API with new endpoints and improved status information. These changes add comprehensive monitoring, IP blocking management, and sysop information endpoints to the WWIVD daemon.

## Changes

### 1. Node Manager Enhancements (`wwivd/node_manager.cpp`, `wwivd/node_manager.h`)

**Added User Information Tracking:**
- Added `user_number` field to `NodeStatus` struct to track which user is connected to each node
- Updated `update_nodes()` to populate `user_number` from instance data
- Modified `nodes()` method to filter nodes by valid range `[start_, end_]` to prevent returning invalid node entries

**Code Changes:**
```cpp
// Added to NodeStatus struct
int user_number = 0;

// Updated in update_nodes()
n.user_number = inst.user_number();

// Enhanced nodes() filtering
if (n.first >= start_ && n.first <= end_) {
  v.emplace_back(n.second);
}
```

### 2. Enhanced Status Endpoint (`wwivd/wwivd_http.cpp`, `wwivd/wwivd_http.h`)

**Improved `/status` Endpoint:**
- Enhanced to accept `Config` parameter for user name lookups
- Added `user_number` and `user_handle` fields to `status_line_t` structure
- Updated JSON serialization to include user information

**New `/instances` Endpoint:**
- Added version 1 API endpoint (`/instances`) that provides enhanced instance information
- Includes user numbers and user handles for all connected nodes
- Automatically determines version based on path (no version parameter needed)

**Code Changes:**
```cpp
// Enhanced status_line_t structure
struct status_line_t {
  // ... existing fields ...
  int user_number{ 0 };
  std::string user_handle;
};

// Version detection from path
if (path == "/instances") {
  version = 1;
} else if (path == "/status") {
  version = 0;
}
```

### 3. New `/blocking` Endpoint (`wwivd/wwivd_http.cpp`, `wwivd/wwivd_http.h`)

**IP Blocking Management:**
- New endpoint to view IP whitelist and blacklist information
- Parses `goodip.txt` (whitelist) and `badip.txt` (blacklist) files
- Includes auto-blocked IPs from the auto-blocker system
- Provides expiration timestamps and block counts for auto-blocked entries
- Filters out expired auto-blocked entries

**Response Format:**
```json
{
  "whitelist": [...],
  "whitelist_count": 0,
  "blacklist": [...],
  "blacklist_count": 0,
  "auto_blocked": [...],
  "auto_blocked_count": 0
}
```

**Features:**
- Parses ISO8601 timestamps from file comments
- Tracks permanent vs temporary blocks
- Shows block counts for auto-blocked IPs
- Automatically filters expired entries

### 4. New `/sysop` Endpoint (`wwivd/wwivd_http.cpp`, `wwivd/wwivd_http.h`)

**Sysop Information Dashboard:**
- Comprehensive BBS statistics endpoint
- Real-time system information including:
  - Calls, uploads, messages, email, and feedback statistics for today
  - Minutes used today and percentage of day
  - Total users and total calls
  - Call/day ratio calculation
  - WWIV and network version information
  - Chat status (sysop availability)
  - Last user information
  - Operating system information
  - Current date and time

**Response Format:**
```json
{
  "calls_today": 0,
  "feedback_waiting": 0,
  "uploads_today": 0,
  "messages_today": 0,
  "email_today": 0,
  "feedback_today": 0,
  "mins_used_today": 0,
  "mins_used_today_percent": "0.00",
  "wwiv_version": "...",
  "net_version": "...",
  "total_users": 0,
  "total_calls": 0,
  "call_day_ratio": "N/A",
  "chat_status": "Not Available",
  "last_user": "Nobody",
  "os": "...",
  "date": "01/01/24",
  "time": "12:00:00"
}
```

**Features:**
- Reads from `status.dat` for BBS statistics
- Checks sysop user (user 1) for feedback waiting
- Determines last user from instance data
- Checks if sysop is online and in chat location
- Calculates call/day ratio and minutes percentage

### 5. HTTP Server Registration (`wwivd/wwivd.cpp`)

**Updated Endpoint Registration:**
- Registered all new endpoints with proper handlers
- Updated `/status` handler to pass config parameter
- Added `/instances`, `/blocking`, and `/sysop` endpoint handlers

**Code Changes:**
```cpp
svr->Get("/status", std::bind(StatusHandler, data.nodes, data.config, _1, _2));
svr->Get("/instances", std::bind(StatusHandler, data.nodes, data.config, _1, _2));
svr->Get("/blocking", std::bind(BlockingHandler, &data, _1, _2));
svr->Get("/sysop", std::bind(SysopHandler, &data, _1, _2));
```

## Dependencies Added

- `<regex>` - For parsing ISO8601 timestamps from IP file comments
- `core/datetime.h` - For date/time formatting
- `core/file.h` - For file existence checks
- `core/textfile.h` - For reading IP files
- `core/version.h` - For WWIV version information
- `fmt/format.h` - For formatted string output
- `sdk/filenames.h` - For file path construction
- `sdk/instance.h` - For instance data access
- `sdk/names.h` - For user name lookups
- `sdk/status.h` - For BBS status information
- `sdk/usermanager.h` - For user data access

## API Endpoints Summary

| Endpoint | Version | Description |
|----------|---------|-------------|
| `/status` | 0 | Legacy status endpoint (backward compatible) |
| `/instances` | 1 | Enhanced status with user information |
| `/blocking` | N/A | IP whitelist/blacklist management |
| `/sysop` | N/A | Sysop dashboard with BBS statistics |

## Benefits

- ✅ Enhanced monitoring capabilities with user information
- ✅ Better node filtering prevents invalid node entries
- ✅ Comprehensive IP blocking visibility
- ✅ Real-time sysop dashboard information
- ✅ Backward compatible with existing `/status` endpoint
- ✅ JSON-formatted responses for easy integration
- ✅ Thread-safe implementations with mutex protection

