#include "settings_page.hpp"

#include "demo_data.hpp"
#include "ui_helpers.hpp"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <string>
#include <utility>

namespace activityos::ui {
namespace {

bool setLaunchAtLogin(bool enabled) {
#if defined(Q_OS_WIN)
    QSettings startup(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        QSettings::NativeFormat);
    if (enabled) {
        startup.setValue("ActivityOS",
                         QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    } else {
        startup.remove("ActivityOS");
    }
    return startup.status() == QSettings::NoError;
#elif defined(Q_OS_MACOS)
    const auto directory = QDir::homePath() + "/Library/LaunchAgents";
    QDir().mkpath(directory);
    const auto path = directory + "/com.activityos.desktop.plist";
    if (!enabled) return QFile::remove(path) || !QFile::exists(path);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
    QTextStream stream(&file);
    stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
              "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
           << "<plist version=\"1.0\"><dict>"
           << "<key>Label</key><string>com.activityos.desktop</string>"
           << "<key>ProgramArguments</key><array><string>"
           << QCoreApplication::applicationFilePath().toHtmlEscaped()
           << "</string></array><key>RunAtLoad</key><true/>"
           << "</dict></plist>\n";
    return stream.status() == QTextStream::Ok;
#else
    const auto directory = QDir::homePath() + "/.config/autostart";
    QDir().mkpath(directory);
    const auto path = directory + "/activityos.desktop";
    if (!enabled) return QFile::remove(path) || !QFile::exists(path);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
    QTextStream stream(&file);
    stream << "[Desktop Entry]\nType=Application\nName=ActivityOS\nExec=\""
           << QCoreApplication::applicationFilePath().replace("\"", "\\\"")
           << "\"\nX-GNOME-Autostart-enabled=true\n";
    return stream.status() == QTextStream::Ok;
#endif
}

} // namespace

SettingsPage::SettingsPage(ActivityService& activity, storage::Database& database,
                           TrackerService& tracker, Callbacks callbacks, QWidget* parent)
    : DashboardPage(parent),
      activity_(activity),
      database_(database),
      tracker_(tracker),
      callbacks_(std::move(callbacks)) {
    auto* layout = contentLayout();
    addPageHeader(layout, "Configuration", "Settings");

    auto* settingsCard = new QFrame;
    settingsCard->setObjectName("sectionCard");
    auto* settingsLayout = new QVBoxLayout(settingsCard);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(0);

    settingsLayout->addWidget(settingsSection("Privacy"));
    pauseTracking_ = new QCheckBox;
    storeTitles_ = new QCheckBox;
    launchAtLogin_ = new QCheckBox;
    settingsLayout->addWidget(settingsRow("Pause tracking",
                                          "Stop recording activity until you resume.",
                                          pauseTracking_));
    settingsLayout->addWidget(settingsRow("Record window titles",
                                          "Include window and tab titles in session data.",
                                          storeTitles_));
    settingsLayout->addWidget(settingsRow("Launch at login",
                                          "Start ActivityOS when you log in.",
                                          launchAtLogin_));

    settingsLayout->addWidget(settingsSection("Data"));
    idleThreshold_ = new QSpinBox;
    idleThreshold_->setRange(1, 60);
    idleThreshold_->setSuffix(" min");
    idleThreshold_->setMinimumWidth(92);
    idleThreshold_->setValue(5);
    settingsLayout->addWidget(settingsRow("Auto-pause on idle",
                                          "Stop recording after this many minutes of inactivity.",
                                          idleThreshold_));
    layout->addWidget(settingsCard);

    auto* exportCsv = new QPushButton("Export CSV");
    auto* exportJson = new QPushButton("Export JSON");
    auto* deleteAll = new QPushButton("Delete All Data");
    deleteAll->setObjectName("dangerButton");
    auto* demoData = new QPushButton("Load Safe Demo Data");
    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(10);
    buttons->addWidget(deleteAll);
    buttons->addWidget(exportCsv);
    buttons->addWidget(exportJson);
    buttons->addStretch();
    buttons->addWidget(demoData);
    layout->addLayout(buttons);
    layout->addStretch();

    connect(exportCsv, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getSaveFileName(window(), "Export Activity", "activity.csv",
                                                       "CSV files (*.csv)");
        if (!path.isEmpty()) activity_.exportCsv({0, unixMillisecondsNow() + 1},
                                                 path.toStdString());
    });
    connect(exportJson, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getSaveFileName(window(), "Export Activity", "activity.json",
                                                       "JSON files (*.json)");
        if (!path.isEmpty()) activity_.exportJson({0, unixMillisecondsNow() + 1},
                                                  path.toStdString());
    });
    connect(deleteAll, &QPushButton::clicked, this, [this] {
        if (QMessageBox::warning(window(), "Delete All Activity",
                                 "This permanently deletes all recorded activity. Continue?",
                                 QMessageBox::Yes | QMessageBox::Cancel,
                                 QMessageBox::Cancel) == QMessageBox::Yes) {
            activity_.deleteAllActivity();
            callbacks_.data_changed();
        }
    });

    connect(pauseTracking_, &QCheckBox::toggled, this, [this](bool paused) {
        if (paused) tracker_.pause(unixMillisecondsNow());
        else tracker_.resume();
        database_.setSetting("tracking_paused", paused ? "1" : "0");
        callbacks_.tracking_changed();
    });
    const auto updateTrackerConfig = [this] {
        TrackerConfig config;
        config.idle_threshold_ms =
            static_cast<std::int64_t>(idleThreshold_->value()) * kMinuteMs;
        config.persist_window_titles = storeTitles_->isChecked();
        tracker_.updateConfig(config);
        database_.setSetting("persist_window_titles", storeTitles_->isChecked() ? "1" : "0");
        database_.setSetting("idle_threshold_minutes",
                             std::to_string(idleThreshold_->value()));
    };
    connect(storeTitles_, &QCheckBox::toggled, this,
            [updateTrackerConfig](bool) { updateTrackerConfig(); });
    connect(idleThreshold_, &QSpinBox::valueChanged, this,
            [updateTrackerConfig](int) { updateTrackerConfig(); });
    connect(launchAtLogin_, &QCheckBox::toggled, this, [this](bool enabled) {
        if (setLaunchAtLogin(enabled)) {
            database_.setSetting("launch_at_login", enabled ? "1" : "0");
        } else {
            QMessageBox::warning(window(), "Launch at Login",
                                 "ActivityOS could not update your login settings.");
        }
    });
    connect(demoData, &QPushButton::clicked, this, [this] {
        loadDemoData(database_, window());
        callbacks_.data_changed();
    });

    storeTitles_->setChecked(database_.setting("persist_window_titles") == "1");
    pauseTracking_->setChecked(database_.setting("tracking_paused") == "1");
    launchAtLogin_->setChecked(database_.setting("launch_at_login") == "1");
    if (const auto value = database_.setting("idle_threshold_minutes")) {
        idleThreshold_->setValue(std::max(1, std::stoi(*value)));
    }
    updateTrackerConfig();
}

void SettingsPage::refresh(const DashboardSnapshot&) {}

void SettingsPage::setTrackingPaused(bool paused) {
    pauseTracking_->setChecked(paused);
}

} // namespace activityos::ui
