#if defined(__APPLE__)

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include "activityos/platform_permissions.hpp"

namespace activityos {
namespace {

NSString* const kScreenRecordingPromptAttemptedKey = @"ScreenRecordingPromptAttempted";
NSString* const kChromeAutomationPromptAttemptedKey = @"ChromeAutomationPromptAttempted_v3";

void migrateLegacyPermissionFlags(NSUserDefaults* defaults) {
    for (NSString* key in @[
             @"ScreenRecordingAccessRequested",
             @"ChromeAutomationAccessRequested",
             @"ChromeAutomationPromptAttempted",
             @"ChromeAutomationPromptAttempted_v2",
         ]) {
        [defaults removeObjectForKey:key];
    }
}

bool chromeIsInstalled() {
    return [NSWorkspace.sharedWorkspace
               URLForApplicationWithBundleIdentifier:@"com.google.Chrome"] != nil;
}

void ensureChromeIsRunning() {
    if ([NSRunningApplication runningApplicationsWithBundleIdentifier:@"com.google.Chrome"].count >
        0) {
        return;
    }

    NSURL* chromeUrl =
        [NSWorkspace.sharedWorkspace URLForApplicationWithBundleIdentifier:@"com.google.Chrome"];
    if (chromeUrl == nil) return;

    [[NSWorkspace sharedWorkspace]
        openApplicationAtURL:chromeUrl
               configuration:[NSWorkspaceOpenConfiguration configuration]
           completionHandler:nil];
    [NSThread sleepForTimeInterval:2.0];
}

bool chromeCanReadActiveTabUrl() {
    if (!chromeIsInstalled()) return false;

    NSString* scriptSource =
        @"tell application \"Google Chrome\"\n"
         @"if (count of windows) is 0 then return \"\"\n"
         @"return URL of active tab of front window\n"
         @"end tell";
    NSDictionary* errorInfo = nil;
    NSAppleScript* script = [[NSAppleScript alloc] initWithSource:scriptSource];
    NSAppleEventDescriptor* result = [script executeAndReturnError:&errorInfo];
    if (errorInfo != nil || result == nil) return false;

    NSString* output = result.stringValue;
    return output != nil && [output hasPrefix:@"http"];
}

void requestScreenRecordingOnce(NSUserDefaults* defaults) {
    if (CGPreflightScreenCaptureAccess()) return;
    if ([defaults boolForKey:kScreenRecordingPromptAttemptedKey]) return;

    CGRequestScreenCaptureAccess();
    [defaults setBool:YES forKey:kScreenRecordingPromptAttemptedKey];
}

void requestChromeAutomationOnce(NSUserDefaults* defaults) {
    if (!chromeIsInstalled()) return;
    if (chromeCanReadActiveTabUrl()) return;
    if ([defaults boolForKey:kChromeAutomationPromptAttemptedKey]) return;

    ensureChromeIsRunning();

    NSString* scriptSource =
        @"tell application \"Google Chrome\"\n"
         @"if (count of windows) is 0 then make new window\n"
         @"return URL of active tab of front window\n"
         @"end tell";
    NSDictionary* errorInfo = nil;
    NSAppleScript* script = [[NSAppleScript alloc] initWithSource:scriptSource];
    [script executeAndReturnError:&errorInfo];

    [defaults setBool:YES forKey:kChromeAutomationPromptAttemptedKey];
}

} // namespace

void requestPlatformPermissions() {
    NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
    migrateLegacyPermissionFlags(defaults);
    requestScreenRecordingOnce(defaults);
    requestChromeAutomationOnce(defaults);
    [defaults synchronize];
}

} // namespace activityos

#endif
