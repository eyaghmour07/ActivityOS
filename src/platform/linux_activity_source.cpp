#if defined(__linux__)

#include "activityos/activity_source.hpp"

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#if __has_include(<X11/Xatom.h>) && __has_include(<X11/Xlib.h>) && \
    __has_include(<X11/Xutil.h>) && __has_include(<X11/extensions/scrnsaver.h>)
#define ACTIVITYOS_HAS_X11_ACTIVITY_SOURCE 1
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/scrnsaver.h>
#else
#define ACTIVITYOS_HAS_X11_ACTIVITY_SOURCE 0
#endif

namespace activityos::detail {
namespace {

#if ACTIVITYOS_HAS_X11_ACTIVITY_SOURCE

std::optional<Window> activeWindow(Display* display) {
    const Window root = DefaultRootWindow(display);
    const Atom property = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
    if (property == None) {
        return std::nullopt;
    }

    Atom actual_type = None;
    int actual_format = 0;
    unsigned long item_count = 0;
    unsigned long bytes_after = 0;
    unsigned char* data = nullptr;
    const int result = XGetWindowProperty(
        display, root, property, 0, 1, False, XA_WINDOW, &actual_type,
        &actual_format, &item_count, &bytes_after, &data);
    if (result != Success || actual_type != XA_WINDOW || actual_format != 32 ||
        item_count != 1 || data == nullptr) {
        if (data != nullptr) {
            XFree(data);
        }
        return std::nullopt;
    }

    const Window window = *reinterpret_cast<unsigned long*>(data);
    XFree(data);
    return window == None ? std::nullopt : std::optional<Window>{window};
}

std::optional<std::uint64_t> windowProcessId(Display* display, Window window) {
    const Atom property = XInternAtom(display, "_NET_WM_PID", True);
    if (property == None) {
        return std::nullopt;
    }

    Atom actual_type = None;
    int actual_format = 0;
    unsigned long item_count = 0;
    unsigned long bytes_after = 0;
    unsigned char* data = nullptr;
    const int result = XGetWindowProperty(
        display, window, property, 0, 1, False, XA_CARDINAL, &actual_type,
        &actual_format, &item_count, &bytes_after, &data);
    if (result != Success || actual_type != XA_CARDINAL || actual_format != 32 ||
        item_count != 1 || data == nullptr) {
        if (data != nullptr) {
            XFree(data);
        }
        return std::nullopt;
    }

    const auto process_id =
        static_cast<std::uint64_t>(*reinterpret_cast<unsigned long*>(data));
    XFree(data);
    return process_id;
}

std::optional<std::string> utf8Property(
    Display* display, Window window, const char* property_name) {
    const Atom property = XInternAtom(display, property_name, True);
    if (property == None) {
        return std::nullopt;
    }

    Atom actual_type = None;
    int actual_format = 0;
    unsigned long item_count = 0;
    unsigned long bytes_after = 0;
    unsigned char* data = nullptr;
    const int result = XGetWindowProperty(
        display, window, property, 0, 4096, False, AnyPropertyType, &actual_type,
        &actual_format, &item_count, &bytes_after, &data);
    if (result != Success || actual_format != 8 || data == nullptr) {
        if (data != nullptr) {
            XFree(data);
        }
        return std::nullopt;
    }

    std::string value{reinterpret_cast<char*>(data), item_count};
    XFree(data);
    return value.empty() ? std::nullopt
                         : std::optional<std::string>{std::move(value)};
}

std::optional<std::string> windowTitle(Display* display, Window window) {
    if (auto title = utf8Property(display, window, "_NET_WM_NAME")) {
        return title;
    }

    char* legacy_title = nullptr;
    if (XFetchName(display, window, &legacy_title) == 0 ||
        legacy_title == nullptr) {
        return std::nullopt;
    }
    std::string title{legacy_title};
    XFree(legacy_title);
    return title.empty() ? std::nullopt
                         : std::optional<std::string>{std::move(title)};
}

std::string applicationName(Display* display, Window window) {
    XClassHint class_hint{};
    if (XGetClassHint(display, window, &class_hint) == 0) {
        return {};
    }

    std::string name;
    if (class_hint.res_class != nullptr) {
        name = class_hint.res_class;
    } else if (class_hint.res_name != nullptr) {
        name = class_hint.res_name;
    }
    if (class_hint.res_name != nullptr) {
        XFree(class_hint.res_name);
    }
    if (class_hint.res_class != nullptr) {
        XFree(class_hint.res_class);
    }
    return name;
}

class LinuxActivitySource final : public ActivitySource {
public:
    LinuxActivitySource()
        : wayland_detected_(std::getenv("WAYLAND_DISPLAY") != nullptr),
          display_(XOpenDisplay(nullptr)) {}

    ~LinuxActivitySource() override {
        if (display_ != nullptr) {
            XCloseDisplay(display_);
        }
    }

    [[nodiscard]] ActivitySnapshot capture() override {
        ActivitySnapshot snapshot;
        if (display_ == nullptr) {
            snapshot.metadata.status = wayland_detected_
                                           ? ActivitySourceStatus::unsupported
                                           : ActivitySourceStatus::error;
            snapshot.metadata.error_message =
                wayland_detected_
                    ? "Native Wayland activity collection is unsupported; an "
                      "X11/XWayland DISPLAY is required"
                    : "Could not open the X11 display";
            return snapshot;
        }

        if (wayland_detected_) {
            snapshot.metadata.status = ActivitySourceStatus::degraded;
            snapshot.metadata.error_message =
                "Running through XWayland; native Wayland windows may be omitted";
        }

        const auto window = activeWindow(display_);
        if (!window) {
            snapshot.metadata.status = ActivitySourceStatus::error;
            snapshot.metadata.error_message =
                "The X11 window manager did not report _NET_ACTIVE_WINDOW";
            captureIdleDuration(snapshot);
            return snapshot;
        }

        snapshot.application_name = applicationName(display_, *window);
        snapshot.metadata.capabilities.application_name =
            !snapshot.application_name.empty();

        snapshot.window_title = windowTitle(display_, *window);
        snapshot.metadata.capabilities.window_title =
            snapshot.window_title.has_value();

        if (const auto process_id = windowProcessId(display_, *window)) {
            snapshot.process_id = *process_id;
            snapshot.metadata.capabilities.process_id = true;
        }

        captureIdleDuration(snapshot);
        if (!snapshot.metadata.capabilities.application_name &&
            snapshot.metadata.status == ActivitySourceStatus::available) {
            snapshot.metadata.status = ActivitySourceStatus::degraded;
            snapshot.metadata.error_message =
                "The active X11 window has no WM_CLASS application name";
        }
        return snapshot;
    }

private:
    void captureIdleDuration(ActivitySnapshot& snapshot) const {
        XScreenSaverInfo* info = XScreenSaverAllocInfo();
        if (info != nullptr &&
            XScreenSaverQueryInfo(display_, DefaultRootWindow(display_), info)) {
            snapshot.idle_duration = std::chrono::milliseconds{info->idle};
            snapshot.metadata.capabilities.idle_duration = true;
            XFree(info);
            return;
        }
        if (info != nullptr) {
            XFree(info);
        }
        if (snapshot.metadata.status == ActivitySourceStatus::available) {
            snapshot.metadata.status = ActivitySourceStatus::degraded;
        }
        snapshot.metadata.error_message =
            "XScreenSaver could not determine idle duration";
    }

    bool wayland_detected_;
    Display* display_;
};

#else

class LinuxActivitySource final : public ActivitySource {
public:
    [[nodiscard]] ActivitySnapshot capture() override {
        ActivitySnapshot snapshot;
        snapshot.metadata.status = ActivitySourceStatus::unsupported;
        snapshot.metadata.error_message =
            "ActivityOS was built without X11 and XScreenSaver headers; native "
            "Wayland activity collection is unsupported";
        return snapshot;
    }
};

#endif

}  // namespace

std::unique_ptr<ActivitySource> makeLinuxActivitySource() {
    return std::make_unique<LinuxActivitySource>();
}

}  // namespace activityos::detail

#endif
