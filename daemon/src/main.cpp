// touch2tabletd: present a supported touch panel (Panels.h) as a virtual pen tablet (Linux).
//
//   touch2tabletd [--settings PATH] [--socket PATH] [--no-device]
//   touch2tabletd --identify | --print-udev-rules | --print-panel-table
//
// Settings: JSON as written by the GUI (see README).  Missing file = defaults.
// Control socket: see ControlServer.h.  --no-device serves the socket without touching hardware.
#include "ControlServer.h"
#include "Daemon.h"
#ifdef Q_OS_LINUX
#include "UnixSignals.h"
#endif
#ifdef T2T_ENABLE_LINUX_BACKEND
#include "backend/linux/EvdevTouchSource.h"
#include "backend/linux/UinputPenSink.h"
#endif
#include "t2t/SettingsStore.h"
#include "t2t/RuntimePaths.h"
#ifdef T2T_ENABLE_WINDOWS_BACKEND
#include "backend/windows/PointerTouchSource.h"
#include "backend/windows/AbsoluteMouseSink.h"
#include <QJsonArray>
#include <QJsonDocument>
#endif

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QTextStream>
#include <cstdio>
#include <QDir>
#include <QFile>
#include <QLockFile>

using namespace t2t;

namespace {

Settings loadSettings(const QString& path, QString* note)
{
    QString err;
    const auto json = SettingsStore::read(path, &err);
    if (!json) {
        *note = err.isEmpty() ? QStringLiteral("no settings file at %1; using defaults").arg(path)
                              : QStringLiteral("%1; using defaults").arg(err);
        return Settings::defaults();
    }
    *note = QStringLiteral("settings loaded from %1").arg(path);
    return Settings::fromJson(*json);
}

}  // namespace

int main(int argc, char** argv)
{
#ifdef T2T_ENABLE_WINDOWS_BACKEND
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("touch2tabletd"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Present a supported touch panel as a virtual pen tablet"));
    parser.addHelpOption();
    parser.addVersionOption();
    // Default: the invoking user's config dir (~/.config/touch2tablet/settings.json); the systemd
    // units pass the path explicitly so behavior does not depend on who runs the daemon.
    const QString defaultSettings = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                                    + QStringLiteral("/touch2tablet/settings.json");
    QCommandLineOption optSettings({"s", "settings"}, QStringLiteral("Settings JSON file"), QStringLiteral("path"),
                                   defaultSettings);
    const QString defaultSocket = defaultControlEndpoint();
    QCommandLineOption optSocket({"k", "socket"}, QStringLiteral("Control socket path"), QStringLiteral("path"), defaultSocket);
    QCommandLineOption optNoDevice(QStringLiteral("no-device"), QStringLiteral("Serve the control socket only; never open input devices"));
    parser.addOption(optSettings);
    parser.addOption(optSocket);
    QCommandLineOption optLog("log", "Write daemon log to a file", "path");
    parser.addOption(optLog);
    QCommandLineOption optIdentify(QStringLiteral("identify"), QStringLiteral("Print a panels.json entry for every multitouch device, then exit"));
    QCommandLineOption optUdev(QStringLiteral("print-udev-rules"), QStringLiteral("Print the udev rule for the supported panels, then exit"));
    QCommandLineOption optTable(QStringLiteral("print-panel-table"), QStringLiteral("Print the table of docs/supported-panels.md, then exit"));
    parser.addOption(optNoDevice);
    parser.addOption(optIdentify);
    parser.addOption(optUdev);
    parser.addOption(optTable);
    parser.process(app);
    if (parser.isSet(optIdentify)) {
#ifdef T2T_ENABLE_LINUX_BACKEND
        QTextStream(stdout) << EvdevTouchSource::identify();
        return 0;
#elif defined(T2T_ENABLE_WINDOWS_BACKEND)
        QString error;
        QJsonArray devices;
        for (const auto& d : windows::pointerDevices(&error)) devices.append(d.json());
        QTextStream(stdout) << QJsonDocument(QJsonObject{{"devices", devices}, {"error", error},
            {"ui_access", windows::hasUiAccess(nullptr)}}).toJson();
        return error.isEmpty() ? 0 : 1;
#else
        QTextStream(stderr) << "Device identification is unavailable: this build has no hardware backend.\n";
        return 1;
#endif
    }
    if (parser.isSet(optUdev) || parser.isSet(optTable)) {
        QTextStream(stdout) << (parser.isSet(optUdev) ? udevRules(panels()) : markdownTable(panels()));
        return 0;
    }
    const bool noDevice = parser.isSet(optNoDevice);
#if !defined(T2T_ENABLE_LINUX_BACKEND) && !defined(T2T_ENABLE_WINDOWS_BACKEND)
    if (!noDevice) {
        QTextStream(stderr) << "This build has no hardware backend. Use --no-device for GUI and protocol development.\n";
        return 1;
    }
#endif

    // Guard before opening the endpoint: Windows permits multiple servers on one
    // pipe name, and two daemons must never compete for touch capture.
    const QString lockDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/touch2tablet");
    QDir().mkpath(lockDir);
    QLockFile instanceLock(lockDir + '/' + QString::fromLatin1(QCryptographicHash::hash(
        parser.value(optSocket).toUtf8(), QCryptographicHash::Sha256).toHex()) + ".lock");
    instanceLock.setStaleLockTime(0);
    if (!instanceLock.tryLock()) {
        if (instanceLock.error() == QLockFile::LockFailedError) return 0;
        QTextStream(stderr) << "Cannot create daemon instance lock in " << lockDir << '\n';
        return 1;
    }
    QFile logFile;
    if (parser.isSet(optLog)) {
        logFile.setFileName(parser.value(optLog));
        if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return 1;
    }
    QTextStream out(stdout);
    const bool underJournal = qEnvironmentVariableIsSet("JOURNAL_STREAM");   // journald stamps lines itself
    auto logLine = [&out, &logFile, underJournal](const QString& line) {
        if (logFile.isOpen()) {
            logFile.write((QDateTime::currentDateTime().toString(Qt::ISODate) + ' ' + line + '\n').toUtf8());
            logFile.flush();
        }
        if (!underJournal)
            out << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")) << ' ';
        out << line << '\n';
        out.flush();
    };

    QString note;
    Settings settings = loadSettings(parser.value(optSettings), &note);
#ifdef T2T_ENABLE_WINDOWS_BACKEND
    if (!QFile::exists(parser.value(optSettings))) {
        settings.display.width = GetSystemMetrics(SM_CXSCREEN);
        settings.display.height = GetSystemMetrics(SM_CYSCREEN);
        settings.displayArea = {double(settings.display.width), double(settings.display.height),
                               settings.display.width / 2.0, settings.display.height / 2.0};
    }
#endif
    logLine(note);

    ControlServer* controlLog = nullptr;
    auto probeLog = [&logLine, &controlLog](const QString& line) {
        logLine(line);
        if (controlLog) controlLog->addLogLine(line);
    };
    Daemon daemon(
        settings,
#ifdef T2T_ENABLE_LINUX_BACKEND
        [&probeLog, noDevice]() -> std::unique_ptr<ITouchSource> {
            if (noDevice) return nullptr;
            QString err;
            auto src = EvdevTouchSource::probe(&err);
            static QString lastErr;
            if (!src && !err.isEmpty() && err != lastErr) {   // log a changed failure reason once
                probeLog(QStringLiteral("touch panel not usable yet: %1").arg(err));
            }
            lastErr = err;
            return std::unique_ptr<ITouchSource>(std::move(src));
        },
        [] { return std::unique_ptr<IPenSink>(std::make_unique<UinputPenSink>()); }
#elif defined(T2T_ENABLE_WINDOWS_BACKEND)
        [&probeLog, noDevice]() -> std::unique_ptr<ITouchSource> {
            if (noDevice) return nullptr;
            QString err;
            auto src = windows::PointerTouchSource::probe(&err);
            static QString lastErr;
            if (!src && !err.isEmpty() && err != lastErr)
                probeLog(QStringLiteral("touch panel not usable yet: %1").arg(err));
            lastErr = err;
            return src;
        },
        []() -> std::unique_ptr<IPenSink> {
            return std::make_unique<windows::AbsoluteMouseSink>();
        }
#else
        {}, {}
#endif
    );
    QObject::connect(&daemon, &Daemon::log, &app, logLine);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &daemon, &Daemon::stop);

    ControlServer control(daemon, ControlServer::Config{parser.value(optSocket), parser.value(optSettings)});
    controlLog = &control;
    control.addLogLine(note);
    QObject::connect(&control, &ControlServer::shutdownRequested, &app, &QCoreApplication::quit);

#ifdef Q_OS_LINUX
    UnixSignals unixSignals;
    QObject::connect(&unixSignals, &UnixSignals::terminate, &app, [&](int signo) {
        logLine(QStringLiteral("signal %1: shutting down").arg(signo));
        daemon.stop();
        app.quit();
    });
    QObject::connect(&unixSignals, &UnixSignals::hangup, &app, [&] {
        QString n;
        daemon.applySettings(loadSettings(parser.value(optSettings), &n));
        logLine(QStringLiteral("SIGHUP: %1").arg(n)); control.addLogLine(QStringLiteral("SIGHUP: %1").arg(n));
    });
#endif

    {
        QString err;
        if (control.listen(&err)) {
            const QString l = QStringLiteral("control socket at %1").arg(control.socketName());
            logLine(l); control.addLogLine(l);
        } else {
            const QString l = QStringLiteral("control socket unavailable (%1); exiting").arg(err);
            logLine(l); control.addLogLine(l);
            return 1;
        }
    }
    const QString l = QStringLiteral("touch2tabletd %1 starting; looking for a supported panel%2")
                          .arg(QCoreApplication::applicationVersion(), noDevice ? QStringLiteral(" (--no-device)") : QString());
    logLine(l); control.addLogLine(l);
    daemon.start();
    return app.exec();
}
