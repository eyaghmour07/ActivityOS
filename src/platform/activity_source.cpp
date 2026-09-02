#include "activityos/activity_source.hpp"

#include <algorithm>
#include <memory>
#include <string>

namespace activityos {

ActivitySnapshot FakeActivitySource::capture() {
    ++capture_count_;

    if (snapshots_.empty()) {
        ActivitySnapshot snapshot;
        snapshot.metadata.status = ActivitySourceStatus::error;
        snapshot.metadata.error_message = "FakeActivitySource has no snapshots";
        return snapshot;
    }

    const std::size_t index = std::min(next_index_, snapshots_.size() - 1);
    if (next_index_ < snapshots_.size()) {
        ++next_index_;
    }
    return snapshots_[index];
}

namespace {

class UnsupportedActivitySource final : public ActivitySource {
public:
    explicit UnsupportedActivitySource(std::string reason) : reason_(std::move(reason)) {}

    [[nodiscard]] ActivitySnapshot capture() override {
        ActivitySnapshot snapshot;
        snapshot.metadata.status = ActivitySourceStatus::unsupported;
        snapshot.metadata.error_message = reason_;
        return snapshot;
    }

private:
    std::string reason_;
};

}  // namespace

namespace detail {

#if defined(__APPLE__)
std::unique_ptr<ActivitySource> makeMacOSActivitySource();
#elif defined(_WIN32)
std::unique_ptr<ActivitySource> makeWindowsActivitySource();
#elif defined(__linux__)
std::unique_ptr<ActivitySource> makeLinuxActivitySource();
#endif

}  // namespace detail

std::unique_ptr<ActivitySource> makePlatformActivitySource() {
#if defined(__APPLE__)
    return detail::makeMacOSActivitySource();
#elif defined(_WIN32)
    return detail::makeWindowsActivitySource();
#elif defined(__linux__)
    return detail::makeLinuxActivitySource();
#else
    return std::make_unique<UnsupportedActivitySource>(
        "Activity collection is unsupported on this platform");
#endif
}

}  // namespace activityos
