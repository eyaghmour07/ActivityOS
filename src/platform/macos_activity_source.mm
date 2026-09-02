#if defined(__APPLE__)

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include "activityos/activity_source.hpp"

#include <cmath>
#include <memory>
#include <string>

namespace activityos::detail {
namespace {

std::string toUtf8(NSString* value) {
    if (value == nil) {
        return {};
    }
    const char* utf8 = value.UTF8String;
    return utf8 == nullptr ? std::string{} : std::string{utf8};
}

std::string toUtf8(CFStringRef value) {
    if (value == nullptr) {
        return {};
    }

    const CFIndex length = CFStringGetLength(value);
    const CFIndex capacity =
        CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    std::string result(static_cast<std::size_t>(capacity), '\0');
    if (!CFStringGetCString(value, result.data(), capacity, kCFStringEncodingUTF8)) {
        return {};
    }
    result.resize(std::char_traits<char>::length(result.c_str()));
    return result;
}

std::optional<std::string> frontmostWindowTitle(pid_t process_id, bool& query_succeeded) {
    const CFArrayRef windows = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
        kCGNullWindowID);
    if (windows == nullptr) {
        query_succeeded = false;
        return std::nullopt;
    }

    query_succeeded = true;
    std::optional<std::string> title;
    const CFIndex count = CFArrayGetCount(windows);
    for (CFIndex index = 0; index < count; ++index) {
        const auto* info = static_cast<CFDictionaryRef>(
            CFArrayGetValueAtIndex(windows, index));

        int owner_pid = 0;
        int layer = -1;
        const auto* owner = static_cast<CFNumberRef>(
            CFDictionaryGetValue(info, kCGWindowOwnerPID));
        const auto* window_layer = static_cast<CFNumberRef>(
            CFDictionaryGetValue(info, kCGWindowLayer));
        if (owner == nullptr ||
            !CFNumberGetValue(owner, kCFNumberIntType, &owner_pid) ||
            owner_pid != process_id) {
            continue;
        }
        if (window_layer != nullptr) {
            CFNumberGetValue(window_layer, kCFNumberIntType, &layer);
        }
        if (layer != 0) {
            continue;
        }

        const auto* name = static_cast<CFStringRef>(
            CFDictionaryGetValue(info, kCGWindowName));
        std::string candidate = toUtf8(name);
        if (!candidate.empty()) {
            title = std::move(candidate);
            break;
        }
    }

    CFRelease(windows);
    return title;
}

class MacOSActivitySource final : public ActivitySource {
public:
    [[nodiscard]] ActivitySnapshot capture() override {
        @autoreleasepool {
            ActivitySnapshot snapshot;
            NSRunningApplication* application =
                NSWorkspace.sharedWorkspace.frontmostApplication;
            if (application == nil) {
                snapshot.metadata.status = ActivitySourceStatus::error;
                snapshot.metadata.error_message =
                    "NSWorkspace did not report a frontmost application";
                return snapshot;
            }

            snapshot.application_name = toUtf8(application.localizedName);
            if (snapshot.application_name.empty()) {
                snapshot.application_name = toUtf8(application.bundleIdentifier);
            }
            snapshot.metadata.capabilities.application_name =
                !snapshot.application_name.empty();

            const pid_t process_id = application.processIdentifier;
            snapshot.process_id = static_cast<std::uint64_t>(process_id);
            snapshot.metadata.capabilities.process_id = process_id > 0;

            bool window_query_succeeded = false;
            snapshot.window_title =
                frontmostWindowTitle(process_id, window_query_succeeded);
            snapshot.metadata.capabilities.window_title =
                snapshot.window_title.has_value();
            if (!window_query_succeeded) {
                snapshot.metadata.status = ActivitySourceStatus::degraded;
                snapshot.metadata.error_message =
                    "CoreGraphics window metadata is unavailable";
            }

            const double idle_seconds = CGEventSourceSecondsSinceLastEventType(
                kCGEventSourceStateCombinedSessionState, kCGAnyInputEventType);
            if (std::isfinite(idle_seconds) && idle_seconds >= 0.0) {
                snapshot.idle_duration =
                    std::chrono::milliseconds{static_cast<std::int64_t>(
                        idle_seconds * 1000.0)};
                snapshot.metadata.capabilities.idle_duration = true;
            } else {
                snapshot.metadata.status = ActivitySourceStatus::degraded;
                snapshot.metadata.error_message =
                    "CoreGraphics could not determine idle duration";
            }

            if (!snapshot.metadata.capabilities.application_name) {
                snapshot.metadata.status = ActivitySourceStatus::degraded;
                snapshot.metadata.error_message =
                    "The frontmost application's name is unavailable";
            }
            return snapshot;
        }
    }
};

}  // namespace

std::unique_ptr<ActivitySource> makeMacOSActivitySource() {
    return std::make_unique<MacOSActivitySource>();
}

}  // namespace activityos::detail

#endif
