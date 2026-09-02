#include "activityos/activity_source.hpp"

#include <cassert>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace std::chrono_literals;
using activityos::ActivityCapabilities;
using activityos::ActivitySnapshot;
using activityos::ActivitySource;
using activityos::ActivitySourceMetadata;
using activityos::ActivitySourceStatus;
using activityos::FakeActivitySource;

ActivitySnapshot snapshot(
    std::string application_name,
    std::optional<std::string> window_title,
    std::uint64_t process_id,
    std::chrono::milliseconds idle_duration) {
    return ActivitySnapshot{
        .application_name = std::move(application_name),
        .window_title = std::move(window_title),
        .process_id = process_id,
        .idle_duration = idle_duration,
        .metadata =
            ActivitySourceMetadata{
                .status = ActivitySourceStatus::available,
                .capabilities =
                    ActivityCapabilities{
                        .application_name = true,
                        .window_title = true,
                        .process_id = true,
                        .idle_duration = true,
                    },
                .error_message = std::nullopt,
            },
    };
}

void capturesInOrderAndRepeatsFinalSnapshot() {
    const ActivitySnapshot editor =
        snapshot("Code", "activity_source.cpp", 101, 250ms);
    const ActivitySnapshot browser =
        snapshot("Browser", "ActivityOS docs", 202, 1500ms);
    FakeActivitySource source{{editor, browser}};

    assert(source.capture() == editor);
    assert(source.capture() == browser);
    assert(source.capture() == browser);
    assert(source.captureCount() == 3);
}

void resetRestartsTheSequence() {
    const ActivitySnapshot first = snapshot("Terminal", std::nullopt, 303, 0ms);
    const ActivitySnapshot second = snapshot("Editor", "main.cpp", 404, 5ms);
    FakeActivitySource source{{first, second}};

    assert(source.capture() == first);
    assert(source.capture() == second);
    source.reset();
    assert(source.capture() == first);
    assert(source.captureCount() == 3);
}

void emptySequenceReturnsExplicitError() {
    FakeActivitySource source{std::vector<ActivitySnapshot>{}};

    const ActivitySnapshot captured = source.capture();

    assert(captured.application_name.empty());
    assert(!captured.window_title.has_value());
    assert(captured.process_id == 0);
    assert(captured.idle_duration == 0ms);
    assert(captured.metadata.status == ActivitySourceStatus::error);
    assert(captured.metadata.error_message.has_value());
    assert(source.captureCount() == 1);
}

void supportsActivitySourcePolymorphism() {
    const ActivitySnapshot expected = snapshot("IDE", "test.cpp", 505, 42ms);
    std::unique_ptr<ActivitySource> source =
        std::make_unique<FakeActivitySource>(
            std::vector<ActivitySnapshot>{expected});

    assert(source->capture() == expected);
}

}  // namespace

int main() {
    capturesInOrderAndRepeatsFinalSnapshot();
    resetRestartsTheSequence();
    emptySequenceReturnsExplicitError();
    supportsActivitySourcePolymorphism();
}
