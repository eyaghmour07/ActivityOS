#include "main_window.hpp"

#include "activityos/storage.hpp"
#include "activityos/platform_permissions.hpp"

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QStandardPaths>

#include <exception>
#include <filesystem>

namespace {

constexpr auto kInstanceSocketName = "activityos-single-instance";

// Returns true when another ActivityOS is already running, after asking it to
// surface its window. Guards against a second copy launched from a different path.
bool activateExistingInstance() {
    QLocalSocket socket;
    socket.connectToServer(kInstanceSocketName);
    if (!socket.waitForConnected(300)) return false;
    socket.write("show");
    socket.waitForBytesWritten(300);
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    application.setApplicationName("ActivityOS");
    application.setApplicationDisplayName("ActivityOS");
    application.setOrganizationName("ActivityOS");
    application.setOrganizationDomain("activityos.local");
    application.setWindowIcon(QIcon(":/icons/activityos.png"));
    application.setQuitOnLastWindowClosed(false);

    if (activateExistingInstance()) return 0;
    QLocalServer::removeServer(kInstanceSocketName);
    QLocalServer instanceServer;
    instanceServer.listen(kInstanceSocketName);

    try {
        const auto dataDirectory =
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (!QDir().mkpath(dataDirectory)) {
            throw std::runtime_error("Unable to create the local ActivityOS data directory");
        }
        const auto databasePath =
            std::filesystem::path(dataDirectory.toStdString()) / "activityos.db";
        activityos::storage::Database database(databasePath.string());
        database.migrate();

        if (!database.setting("privacy_consent")) {
            const auto answer = QMessageBox::information(
                nullptr, "Welcome to ActivityOS",
                "ActivityOS records the active application, timestamps, idle periods, and "
                "derived work sessions on this computer.\n\n"
                "Window titles are not stored by default. ActivityOS never records keystrokes, "
                "screenshots, webcam data, or uploads your activity. You can pause, exclude, "
                "export, or delete your data at any time.\n\n"
                "Start local activity tracking?",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);
            if (answer != QMessageBox::Yes) return 0;
            database.setSetting("privacy_consent", "1");
        }

#if defined(__APPLE__)
        if (!database.setting("tracking_permissions_intro_shown")) {
            QMessageBox::information(
                nullptr, "One-Time macOS Permissions",
                "ActivityOS will now ask for the macOS permissions it needs to classify "
                "browser tabs. Chrome may open briefly so macOS can show the control "
                "prompt.\n\n"
                "• Control Google Chrome — reads the active tab site for Docs, YouTube, "
                "school portals, and similar pages\n"
                "• Screen & System Audio Recording — fallback for other browsers when a "
                "window title is needed\n\n"
                "Each system prompt appears only once. If you miss one, enable ActivityOS "
                "under System Settings > Privacy & Security. URLs and titles are not stored "
                "by default.",
                QMessageBox::Ok);
            database.setSetting("tracking_permissions_intro_shown", "1");
        }
#endif
        activityos::requestPlatformPermissions();

        activityos::ui::MainWindow window(database);
        window.show();
        QObject::connect(&instanceServer, &QLocalServer::newConnection, &window, [&] {
            while (QLocalSocket* client = instanceServer.nextPendingConnection()) {
                client->deleteLater();
            }
            window.showNormal();
            window.raise();
            window.activateWindow();
        });
        return application.exec();
    } catch (const std::exception& error) {
        QMessageBox::critical(nullptr, "ActivityOS could not start", error.what());
        return 1;
    }
}
