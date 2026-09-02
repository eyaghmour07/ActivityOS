#include "main_window.hpp"

#include "activityos/storage.hpp"

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QMessageBox>
#include <QStandardPaths>

#include <exception>
#include <filesystem>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    application.setApplicationName("ActivityOS");
    application.setApplicationDisplayName("ActivityOS");
    application.setOrganizationName("ActivityOS");
    application.setOrganizationDomain("activityos.local");
    application.setWindowIcon(QIcon(":/icons/activityos.png"));
    application.setQuitOnLastWindowClosed(false);

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

        activityos::ui::MainWindow window(database);
        window.show();
        return application.exec();
    } catch (const std::exception& error) {
        QMessageBox::critical(nullptr, "ActivityOS could not start", error.what());
        return 1;
    }
}
