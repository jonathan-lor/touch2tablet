// Route SIGTERM / SIGINT / SIGHUP into the Qt event loop (self-pipe trick).
#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <memory>

namespace t2t {

class UnixSignals : public QObject {
    Q_OBJECT
public:
    explicit UnixSignals(QObject* parent = nullptr);
    ~UnixSignals() override;

signals:
    void terminate(int signo);   // SIGTERM / SIGINT
    void hangup();               // SIGHUP

private:
    static void handler(int signo);
    static int fds_[2];
    std::unique_ptr<QSocketNotifier> notifier_;
};

}  // namespace t2t
