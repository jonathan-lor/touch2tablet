#include "UnixSignals.h"

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

namespace t2t {

int UnixSignals::fds_[2] = {-1, -1};

UnixSignals::UnixSignals(QObject* parent) : QObject(parent)
{
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds_) != 0)
        return;
    notifier_ = std::make_unique<QSocketNotifier>(fds_[1], QSocketNotifier::Read);
    connect(notifier_.get(), &QSocketNotifier::activated, this, [this] {
        int signo = 0;
        if (::read(fds_[1], &signo, sizeof signo) != ssize_t(sizeof signo))
            return;
        if (signo == SIGHUP)
            emit hangup();
        else
            emit terminate(signo);
    });
    struct sigaction sa{};
    sa.sa_handler = &UnixSignals::handler;
    sa.sa_flags = SA_RESTART;
    ::sigemptyset(&sa.sa_mask);
    for (int s : {SIGTERM, SIGINT, SIGHUP})
        ::sigaction(s, &sa, nullptr);
}

UnixSignals::~UnixSignals()
{
    for (int s : {SIGTERM, SIGINT, SIGHUP})
        ::signal(s, SIG_DFL);
    for (int& fd : fds_) {
        if (fd >= 0) ::close(fd);
        fd = -1;
    }
}

void UnixSignals::handler(int signo)
{
    if (fds_[0] >= 0) {
        const ssize_t r = ::write(fds_[0], &signo, sizeof signo);
        (void)r;
    }
}

}  // namespace t2t
