#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "activityos/activity_source.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace activityos::detail {
namespace {

std::string toUtf8(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

std::optional<std::string> windowTitle(HWND window) {
    const int length = GetWindowTextLengthW(window);
    if (length <= 0) {
        return std::nullopt;
    }
    std::vector<wchar_t> buffer(static_cast<std::size_t>(length) + 1);
    const int copied =
        GetWindowTextW(window, buffer.data(), static_cast<int>(buffer.size()));
    if (copied <= 0) {
        return std::nullopt;
    }
    return toUtf8(std::wstring_view{buffer.data(), static_cast<std::size_t>(copied)});
}

std::string processName(DWORD process_id) {
    HANDLE process = OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr) {
        return {};
    }

    std::vector<wchar_t> path(32768);
    DWORD path_size = static_cast<DWORD>(path.size());
    const BOOL succeeded =
        QueryFullProcessImageNameW(process, 0, path.data(), &path_size);
    CloseHandle(process);
    if (!succeeded || path_size == 0) {
        return {};
    }

    std::wstring_view full_path{path.data(), path_size};
    const std::size_t separator = full_path.find_last_of(L"\\/");
    const std::wstring_view filename =
        separator == std::wstring_view::npos ? full_path
                                             : full_path.substr(separator + 1);
    return toUtf8(filename);
}

class WindowsActivitySource final : public ActivitySource {
public:
    [[nodiscard]] ActivitySnapshot capture() override {
        ActivitySnapshot snapshot;
        const HWND foreground = GetForegroundWindow();
        if (foreground == nullptr) {
            snapshot.metadata.status = ActivitySourceStatus::error;
            snapshot.metadata.error_message =
                "Windows did not report a foreground window";
            captureIdleDuration(snapshot);
            return snapshot;
        }

        DWORD process_id = 0;
        GetWindowThreadProcessId(foreground, &process_id);
        snapshot.process_id = static_cast<std::uint64_t>(process_id);
        snapshot.metadata.capabilities.process_id = process_id != 0;

        snapshot.window_title = windowTitle(foreground);
        snapshot.metadata.capabilities.window_title =
            snapshot.window_title.has_value();

        if (process_id != 0) {
            snapshot.application_name = processName(process_id);
            snapshot.metadata.capabilities.application_name =
                !snapshot.application_name.empty();
        }
        if (!snapshot.metadata.capabilities.application_name) {
            snapshot.metadata.status = ActivitySourceStatus::degraded;
            snapshot.metadata.error_message =
                "The foreground process name is unavailable";
        }

        captureIdleDuration(snapshot);
        return snapshot;
    }

private:
    static void captureIdleDuration(ActivitySnapshot& snapshot) {
        LASTINPUTINFO input{};
        input.cbSize = sizeof(input);
        if (GetLastInputInfo(&input)) {
            // DWORD subtraction intentionally preserves wrap-around semantics.
            const DWORD elapsed = GetTickCount() - input.dwTime;
            snapshot.idle_duration = std::chrono::milliseconds{elapsed};
            snapshot.metadata.capabilities.idle_duration = true;
            return;
        }

        snapshot.metadata.status =
            snapshot.metadata.status == ActivitySourceStatus::error
                ? ActivitySourceStatus::error
                : ActivitySourceStatus::degraded;
        snapshot.metadata.error_message =
            "Windows could not determine idle duration";
    }
};

}  // namespace

std::unique_ptr<ActivitySource> makeWindowsActivitySource() {
    return std::make_unique<WindowsActivitySource>();
}

}  // namespace activityos::detail

#endif
