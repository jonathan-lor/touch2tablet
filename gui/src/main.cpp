// touch2tablet GUI entry point.
//   touch2tablet [--socket PATH]     (default: $XDG_RUNTIME_DIR/touch2tablet/control.sock)
#include "ControlClient.h"
#include "MainWindow.h"
#include "t2t/RuntimePaths.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QStandardPaths>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#include <QDir>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QLocalServer>
#include <QLockFile>
#endif

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("touch2tablet"));   // QSettings -> ~/.config/touch2tablet/touch2tablet.conf
    QApplication::setApplicationName(QStringLiteral("touch2tablet"));
    QApplication::setApplicationVersion(QStringLiteral("0.1"));
    // No setDesktopFileName(): the executable name already matches touch2tablet.desktop, and
    // setting it makes Qt >= 6.9 register with the desktop portal, which logs a harmless
    // "Could not register app ID: Connection already associated" warning on GNOME.

    const QString defaultSocket = t2t::defaultControlEndpoint();
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Tablet area and display area for a touch panel used as a pen tablet"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption optSocket({"k", "socket"}, QStringLiteral("Daemon control socket"), QStringLiteral("path"), defaultSocket);
    parser.addOption(optSocket);
    parser.addOption(QCommandLineOption("tray", "Start minimized in the notification area"));
    parser.process(app);

#ifdef Q_OS_WIN
    const QString guiName = parser.value(optSocket) + "-gui";
    const QString lockDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/touch2tablet";
    QDir().mkpath(lockDir);
    QLockFile guiLock(lockDir + "/gui-" + QString::fromLatin1(QCryptographicHash::hash(guiName.toUtf8(), QCryptographicHash::Sha256).toHex()) + ".lock");
    guiLock.setStaleLockTime(0);
    if (!guiLock.tryLock()) {
        if (guiLock.error() != QLockFile::LockFailedError) {
            QMessageBox::critical(nullptr, "touch2tablet", "Cannot create the application instance lock.");
            return 1;
        }
        QLocalSocket existing;
        existing.connectToServer(guiName);
        if (existing.waitForConnected(1000)) {
            existing.write(parser.isSet("tray") ? "tray" : "show");
            existing.waitForBytesWritten(1000);
        }
        return 0;
    }
    QLocalServer activation;
    activation.setSocketOptions(QLocalServer::UserAccessOption);
    if (!activation.listen(guiName)) {
        QMessageBox::critical(nullptr, "touch2tablet", activation.errorString());
        return 1;
    }
#endif

    t2t::ControlClient client(parser.value(optSocket));
    t2t::MainWindow win(client);
#ifdef Q_OS_WIN
    QObject::connect(&activation, &QLocalServer::newConnection, &win, [&] {
        while (auto* socket = activation.nextPendingConnection()) {
            const auto activate = [&, socket] {
                if (socket->bytesAvailable() < 4) return;
                if (socket->read(4) == "show") { win.show(); win.raise(); win.activateWindow(); }
                socket->disconnectFromServer();
            };
            QObject::connect(socket, &QLocalSocket::readyRead, &win, activate);
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QTimer::singleShot(1000, socket, &QLocalSocket::disconnectFromServer);
            activate();
        }
    });
    QSystemTrayIcon tray(app.style()->standardIcon(QStyle::SP_ComputerIcon));
    QMenu menu;
    menu.addAction("Open settings", &win, [&] { win.show(); win.raise(); win.activateWindow(); });
    auto* start = menu.addAction("Enable tablet mode", &client, [&] { client.request({{"op", "start"}}); });
    auto* stop = menu.addAction("Use as touchscreen", &client, [&] { client.request({{"op", "stop"}}); });
    menu.addSeparator();
    auto* startup = menu.addAction("Start with Windows");
    startup->setCheckable(true);
    QSettings login("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    startup->setChecked(login.contains("touch2tablet"));
    QObject::connect(startup, &QAction::toggled, &app, [&](bool enabled) {
        if (enabled) login.setValue("touch2tablet", '"' + QDir::toNativeSeparators(app.applicationFilePath()) + "\" --tray");
        else login.remove("touch2tablet");
        login.sync();
        if (login.status() != QSettings::NoError) QMessageBox::warning(&win, "touch2tablet", "Could not update Windows startup settings.");
    });
    menu.addSeparator();
    bool exiting = false;
    menu.addAction("Exit and restore touchscreen", &app, [&] {
        exiting = true;
        client.request({{"op", "shutdown"}}, [&](const QJsonObject&) { app.quit(); });
        QTimer::singleShot(1000, &app, &QCoreApplication::quit);
    });
    tray.setContextMenu(&menu);
    tray.setToolTip("touch2tablet");
    QObject::connect(&tray, &QSystemTrayIcon::activated, &win, [&](auto reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            win.show(); win.raise(); win.activateWindow();
        }
    });
    auto status = [&](const QJsonObject& reply) {
        const auto info = reply.value("info").toObject();
        if (info.isEmpty()) return;
        start->setEnabled(!info.value("running").toBool());
        stop->setEnabled(info.value("running").toBool());
        tray.setToolTip(info.value("connected").toBool() ? "touch2tablet: tablet active"
            : info.value("running").toBool() ? "touch2tablet: waiting for panel" : "touch2tablet: touchscreen mode");
    };
    QObject::connect(&client, &t2t::ControlClient::hello, &app, status);
    QObject::connect(&client, &t2t::ControlClient::event, &app, status);
    // UIAccess must be launched through ShellExecute, not QProcess/CreateProcess.
    // Retry after crashes; the daemon's lock prevents duplicate capture owners.
    auto launch = [&] {
        if (exiting || client.isConnected() || parser.isSet(optSocket)) return;
        const QString exe = QDir::toNativeSeparators(app.applicationDirPath() + "/touch2tabletd.exe");
        const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open",
            reinterpret_cast<LPCWSTR>(exe.utf16()), nullptr, nullptr, SW_HIDE));
        if (result <= 32) tray.setToolTip(QStringLiteral("touch2tablet: daemon could not start (Windows error %1)").arg(result));
    };
    QTimer retry;
    QObject::connect(&retry, &QTimer::timeout, &app, launch);
    retry.start(10000);
    QTimer::singleShot(750, &app, launch);
    const bool hasTray = QSystemTrayIcon::isSystemTrayAvailable();
    app.setQuitOnLastWindowClosed(!hasTray);
    if (hasTray) tray.show();
    if (!parser.isSet("tray") || !hasTray) win.show();
#else
    win.show();
#endif
    client.start();
    return app.exec();
}
