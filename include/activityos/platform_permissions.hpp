#pragma once

namespace activityos {

// Requests platform permissions needed for activity tracking. Each macOS prompt is
// shown at most once per machine. Safe to call on every launch.
void requestPlatformPermissions();

} // namespace activityos
