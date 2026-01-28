# WWIVD HTTP API Enhancements - Changelog

## Security Notice

⚠️ **IMPORTANT**: All HTTP endpoints (`/status`, `/instances`, `/blocking`, `/sysop`, `/laston`) expose sensitive BBS information. These endpoints should be protected by firewall rules and **NOT** exposed to public IP space. Restrict access to trusted networks only or use VPN/SSH tunneling for remote access.

## Changes

### [2026-01-28] Added

- **New `/instances` endpoint** - Enhanced status endpoint with user numbers and handles for all connected nodes
- **New `/blocking` endpoint** - View IP whitelist and blacklist information, including auto-blocked IPs with expiration timestamps
- **New `/sysop` endpoint** - Comprehensive BBS statistics dashboard with real-time system information
- **New `/laston` endpoint** - Retrieve recent login history (up to 8 entries) from laston.txt in JSON format

### [2026-01-28] Changed

- **Enhanced `/status` endpoint** - Now includes user numbers and handles in responses
- **Node Manager** - Added user_number tracking to NodeStatus struct and improved node range filtering

### API Endpoints

| Endpoint | Description |
|----------|-------------|
| `/status` | Legacy status endpoint (backward compatible) |
| `/instances` | Enhanced status with user information |
| `/blocking` | IP whitelist/blacklist management |
| `/sysop` | Sysop dashboard with BBS statistics |
| `/laston` | Login history from laston.txt |

### Technical Details

- All endpoints return JSON-formatted responses
- Thread-safe implementations with mutex protection
- Supports both laston.txt format variants (with/without city/state)
- Automatically strips ANSI color codes from laston.txt entries
- Filters expired auto-blocked IPs automatically
