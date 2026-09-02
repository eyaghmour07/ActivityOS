#if defined(__APPLE__)

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include "activityos/activity_source.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <algorithm>
#include <cctype>

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

// Window names are withheld unless the user has granted Screen & System Audio Recording.
// Permission is requested once at app startup; capture only checks current access here.
bool screenRecordingAccessGranted() {
    return CGPreflightScreenCaptureAccess();
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

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool isGoogleChrome(const std::string& application_name, NSString* bundle_identifier) {
    if (bundle_identifier != nil &&
        [bundle_identifier isEqualToString:@"com.google.Chrome"]) {
        return true;
    }
    const auto name = lower(application_name);
    return name.find("google chrome") != std::string::npos || name == "chrome";
}

struct ChromeTabInfo {
    std::optional<std::string> url;
    std::optional<std::string> title;
    bool automation_allowed{true};
};

ChromeTabInfo queryChromeActiveTab() {
    ChromeTabInfo info;
    NSString* scriptSource =
        @"tell application \"Google Chrome\"\n"
         @"if (count of windows) is 0 then return \"\"\n"
         @"set tabUrl to URL of active tab of front window\n"
         @"set tabTitle to title of active tab of front window\n"
         @"return tabUrl & \"\\t\" & tabTitle\n"
         @"end tell";

    NSDictionary* errorInfo = nil;
    NSAppleScript* script = [[NSAppleScript alloc] initWithSource:scriptSource];
    NSAppleEventDescriptor* result = [script executeAndReturnError:&errorInfo];
    if (errorInfo != nil) {
        info.automation_allowed = false;
        return info;
    }

    NSString* output = result.stringValue;
    if (output == nil || output.length == 0) return info;

    NSArray<NSString*>* parts = [output componentsSeparatedByString:@"\t"];
    if (parts.count >= 1) {
        const std::string url = toUtf8(parts[0]);
        if (!url.empty()) info.url = url;
    }
    if (parts.count >= 2) {
        const std::string title = toUtf8(parts[1]);
        if (!title.empty()) info.title = title;
    }
    return info;
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

            if (isGoogleChrome(snapshot.application_name, application.bundleIdentifier)) {
                const ChromeTabInfo tab = queryChromeActiveTab();
                snapshot.browser_url = tab.url;
                snapshot.window_title = tab.title;
                snapshot.metadata.capabilities.browser_url = tab.url.has_value();
                snapshot.metadata.capabilities.window_title = tab.title.has_value();

                if (!tab.url && !tab.title) {
                    bool window_query_succeeded = false;
                    snapshot.window_title =
                        frontmostWindowTitle(process_id, window_query_succeeded);
                    snapshot.metadata.capabilities.window_title =
                        snapshot.window_title.has_value();
                    if (!tab.automation_allowed) {
                        snapshot.metadata.status = ActivitySourceStatus::degraded;
                        snapshot.metadata.error_message =
                            "Allow ActivityOS to control Google Chrome under Privacy & "
                            "Security > Automation to classify tabs by site";
                    } else if (!window_query_succeeded) {
                        snapshot.metadata.status = ActivitySourceStatus::degraded;
                        snapshot.metadata.error_message =
                            "Chrome tab metadata is unavailable";
                    } else if (!snapshot.window_title && !screenRecordingAccessGranted()) {
                        snapshot.metadata.status = ActivitySourceStatus::degraded;
                        snapshot.metadata.error_message =
                            "Allow ActivityOS to control Google Chrome, or enable Screen & "
                            "System Audio Recording, to classify browser tabs";
                    }
                }
            } else {
                bool window_query_succeeded = false;
                snapshot.window_title =
                    frontmostWindowTitle(process_id, window_query_succeeded);
                snapshot.metadata.capabilities.window_title =
                    snapshot.window_title.has_value();
                if (!window_query_succeeded) {
                    snapshot.metadata.status = ActivitySourceStatus::degraded;
                    snapshot.metadata.error_message =
                        "CoreGraphics window metadata is unavailable";
                } else if (!snapshot.window_title && !screenRecordingAccessGranted()) {
                    snapshot.metadata.status = ActivitySourceStatus::degraded;
                    snapshot.metadata.error_message =
                        "Allow ActivityOS under Privacy & Security > Screen & System Audio "
                        "Recording to classify browser tabs in non-Chrome browsers";
                }
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
