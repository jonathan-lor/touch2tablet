// GUI side of the control socket: QLocalSocket, JSON lines, request/reply by id, event stream,
// automatic reconnect while the daemon is down.
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>
#include <map>

namespace t2t {

class ControlClient : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(const QJsonObject& reply)>;

    explicit ControlClient(QString socketName, QObject* parent = nullptr);
    ~ControlClient() override;

    void start();
    bool isConnected() const { return connected_; }
    QString socketName() const { return name_; }

    /// Send a request; `cb` receives the reply (or {"ok":false,"error":"not connected"}).
    void request(QJsonObject req, Callback cb = {});

    int retryIntervalMs = 2000;

signals:
    void stateChanged(bool connected, const QString& message);
    /// Reply to the automatic "subscribe": settings, info, lines, proto.
    void hello(const QJsonObject& reply);
    /// Unsolicited {"ev": ...} messages.
    void event(const QJsonObject& ev);

private:
    void tryConnect();
    void onConnected();
    void onReadyRead();
    void onLost(const QString& why);

    QString name_;
    QLocalSocket sock_;
    QByteArray buf_;
    std::map<int, Callback> pending_;
    int nextId_ = 1;
    bool connected_ = false;
    QTimer retry_;
};

}  // namespace t2t
