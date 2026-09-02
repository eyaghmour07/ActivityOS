#include "activityos/platform_permissions.hpp"

namespace activityos {

#if !defined(__APPLE__)

void requestPlatformPermissions() {}

#endif

} // namespace activityos
