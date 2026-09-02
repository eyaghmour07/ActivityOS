#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace activityos {

enum class ActivitySourceStatus {
    available,
    degraded,
    unsupported,
    error,
};

struct ActivityCapabilities {
    bool application_name{false};
    bool window_title{false};
    bool browser_url{false};
    bool process_id{false};
    bool idle_duration{false};

    friend bool operator==(const ActivityCapabilities&, const ActivityCapabilities&) = default;
};

struct ActivitySourceMetadata {
    ActivitySourceStatus status{ActivitySourceStatus::available};
    ActivityCapabilities capabilities{};
    std::optional<std::string> error_message;

    friend bool operator==(const ActivitySourceMetadata&, const ActivitySourceMetadata&) = default;
};

struct ActivitySnapshot {
    std::string application_name;
    std::optional<std::string> window_title;
    std::optional<std::string> browser_url;
    std::uint64_t process_id{0};
    std::chrono::milliseconds idle_duration{0};
    ActivitySourceMetadata metadata{};

    friend bool operator==(const ActivitySnapshot&, const ActivitySnapshot&) = default;
};

class ActivitySource {
public:
    virtual ~ActivitySource() = default;

    [[nodiscard]] virtual ActivitySnapshot capture() = 0;
};

// A deterministic source for tests and simulations. Each capture advances to
// the next snapshot; after the sequence is exhausted, the final value repeats.
class FakeActivitySource final : public ActivitySource {
public:
    explicit FakeActivitySource(std::vector<ActivitySnapshot> snapshots)
        : snapshots_(std::move(snapshots)) {}

    [[nodiscard]] ActivitySnapshot capture() override;

    void reset() noexcept { next_index_ = 0; }
    [[nodiscard]] std::size_t captureCount() const noexcept { return capture_count_; }

private:
    std::vector<ActivitySnapshot> snapshots_;
    std::size_t next_index_{0};
    std::size_t capture_count_{0};
};

[[nodiscard]] std::unique_ptr<ActivitySource> makePlatformActivitySource();

}  // namespace activityos
