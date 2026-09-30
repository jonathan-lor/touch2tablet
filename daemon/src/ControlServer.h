// Control socket: newline-delimited JSON over a QLocalServer (a Unix socket, or a named pipe on Windows).
#pragma once

#include "Daemon.h"
#include "t2t/GestureMachine.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QLocalServer>
#include <QObject>
#include <QString>
#include <QStringList>
#include <deque>
#include <unordered_map>

class QLocalSocket;

namespace t2t {

class ControlServer : public QObject {
    Q_OBJECT
public:
    struct Config {
        QString socketName;     // socket path, or pipe name on Windows
        QString settingsPath;   // where "save" writes
        int maxLogLines = 300;
        int posRateHz = 120;    // cap for "pos" events while a finger is down
    };

    ControlServer(Daemon& daemon, Config cfg, QObject* parent = nullptr);
    ~ControlServer() override;

    /// Start listening.  Removes a stale socket file first.  False + *err on failure.
    bool listen(QString* err);
    QString socketName() const { return cfg_.socketName; }

    /// Also usable by main() so console output and the ring buffer agree.
    void addLogLine(const QString& line);

    QJsonObject info() const;
    int clientCount() const { return int(clients_.size()); }

signals:
    void shutdownRequested();

private:
    struct Client {
        QByteArray buf;
        bool subscribed = false;
    };
    void onNewConnection();
    void onReadyRead(QLocalSocket* s);
    void handle(QLocalSocket* s, const QByteArray& line);
    void send(QLocalSocket* s, const QJsonObject& obj);
    void broadcast(const QJsonObject& obj);
    void onFrame(const GestureMachine::State& st);
    QJsonObject replyBase(const QJsonValue& id) const;

    Daemon& daemon_;
    Config cfg_;
    QLocalServer server_;
    std::unordered_map<QLocalSocket*, Client> clients_;
    std::deque<QString> log_;
    QElapsedTimer posClock_;
    qint64 lastPosMs_ = -1;
    bool lastHadFingers_ = false;
};

}  // namespace t2t
