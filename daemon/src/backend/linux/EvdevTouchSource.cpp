#include "EvdevTouchSource.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <unistd.h>

namespace t2t {

namespace {

QByteArray sysfs(const QString& node, const char* leaf)
{
    QFile f(QStringLiteral("/sys/class/input/%1/device/%2").arg(node, QLatin1String(leaf)));
    return f.open(QIODevice::ReadOnly) ? f.readAll().trimmed() : QByteArray();
}

struct Id { int bus, vendor, product; };

Id sysfsId(const QString& node)
{
    auto hex = [&](const char* leaf) {
        bool ok = false;
        const int v = sysfs(node, leaf).toInt(&ok, 16);
        return ok ? v : -1;
    };
    return {hex("id/bustype"), hex("id/vendor"), hex("id/product")};
}

// Ids from sysfs, so unlisted nodes are never opened (and never grabbed by mistake).
const Panel* listedPanel(const QString& node)
{
    const Id id = sysfsId(node);
    return findPanel(id.bus, id.vendor, id.product, QString::fromUtf8(sysfs(node, "name")));
}

// capabilities/abs is the ABS bitmask as hex words, most significant first; ABS_MAX is 0x3f, so
// that is one word on 64-bit and up to two on 32-bit.
bool hasMtPosition(const QString& node)
{
    quint64 bits = 0;
    for (const QByteArray& word : sysfs(node, "capabilities/abs").split(' '))
        bits = (bits << 32) | word.toULongLong(nullptr, 16);
    return (bits >> ABS_MT_POSITION_X & 1) && (bits >> ABS_MT_POSITION_Y & 1) && (bits >> ABS_MT_SLOT & 1);
}

QString jsonString(const QString& s)
{
    const QByteArray a = QJsonDocument(QJsonArray{s}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(a.mid(1, a.size() - 2));
}

QStringList eventNodes()
{
    return QDir(QStringLiteral("/dev/input")).entryList({QStringLiteral("event*")}, QDir::System | QDir::Files);
}

}  // namespace

std::unique_ptr<EvdevTouchSource> EvdevTouchSource::probe(QString* err)
{
    const QDir dir(QStringLiteral("/dev/input"));
    QString lastErr;
    for (const QString& node : eventNodes()) {
        const Panel* panel = listedPanel(node);
        if (!panel)
            continue;
        const QByteArray path = dir.filePath(node).toLocal8Bit();
        const QString where = QStringLiteral("%1 at %2").arg(panel->name, QString::fromLocal8Bit(path));
        const int fd = ::open(path.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            lastErr = QStringLiteral("%1: %2 (is the udev rule installed?)").arg(where, QString::fromLocal8Bit(std::strerror(errno)));
            continue;
        }
        libevdev* dev = nullptr;
        if (libevdev_new_from_fd(fd, &dev) < 0) {
            ::close(fd);
            continue;
        }
        // A USB panel exposes several nodes (e.g. a keyboard interface); only the multitouch one counts.
        if (!libevdev_has_event_code(dev, EV_ABS, ABS_MT_POSITION_X) || !libevdev_has_event_code(dev, EV_ABS, ABS_MT_POSITION_Y)) {
            libevdev_free(dev);
            ::close(fd);
            continue;
        }
        if (libevdev_grab(dev, LIBEVDEV_GRAB) < 0) {
            lastErr = QStringLiteral("%1: grab failed: %2").arg(where, QString::fromLocal8Bit(std::strerror(errno)));
            libevdev_free(dev);
            ::close(fd);
            continue;
        }
        Info info;
        info.panel = panel;
        info.name = QString::fromUtf8(libevdev_get_name(dev));
        info.path = QString::fromLocal8Bit(path);
        const input_absinfo* ax = libevdev_get_abs_info(dev, ABS_MT_POSITION_X);
        const input_absinfo* ay = libevdev_get_abs_info(dev, ABS_MT_POSITION_Y);
        info.range = RawRange{ax->minimum, ax->maximum, ay->minimum, ay->maximum};
        return std::unique_ptr<EvdevTouchSource>(new EvdevTouchSource(fd, dev, info));
    }
    if (err) *err = lastErr;
    return nullptr;
}

QString EvdevTouchSource::identify()
{
    QString out;
    for (const QString& node : eventNodes()) {
        if (!hasMtPosition(node))
            continue;
        const Id id = sysfsId(node);
        const QString path = QStringLiteral("/dev/input/") + node;
        const QString evdevName = QString::fromUtf8(sysfs(node, "name"));
        const Panel* listed = findPanel(id.bus, id.vendor, id.product, evdevName);
        out += QStringLiteral("%1%2\n").arg(path, listed ? QStringLiteral(": already listed as \"%1\"").arg(listed->name) : QString());

        // The size needs the node itself; the kernel's resolution is units per mm.
        double w = 0, h = 0;
        QString sizeNote = QStringLiteral("the device does not report its size; use the datasheet or listing, or measure the active glass area");
        const int fd = ::open(path.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        libevdev* dev = nullptr;
        if (fd < 0) {
            sizeNote = QStringLiteral("cannot open %1 (%2); re-run with sudo for the size the device reports, or use "
                                      "the datasheet or listing").arg(path, QString::fromLocal8Bit(std::strerror(errno)));
        } else if (libevdev_new_from_fd(fd, &dev) == 0) {
            const input_absinfo* ax = libevdev_get_abs_info(dev, ABS_MT_POSITION_X);
            const input_absinfo* ay = libevdev_get_abs_info(dev, ABS_MT_POSITION_Y);
            if (ax && ay && ax->resolution > 0 && ay->resolution > 0) {
                w = std::round(10.0 * (ax->maximum - ax->minimum) / ax->resolution) / 10.0;
                h = std::round(10.0 * (ay->maximum - ay->minimum) / ay->resolution) / 10.0;
                sizeNote = QStringLiteral("size as the device reports it");
            }
            libevdev_free(dev);
        }
        if (fd >= 0) ::close(fd);

        out += QStringLiteral("    {\n"
                              "      \"bus\": %1,\n"
                              "      \"vendor\": \"%2\",\n"
                              "      \"product\": \"%3\",\n"
                              "      \"evdev_name\": %4,\n"
                              "      \"name\": \"TODO what the GUI and logs should call it\",\n"
                              "      \"width_mm\": %5,\n"
                              "      \"height_mm\": %6\n"
                              "    }\n"
                              "  %7\n\n")
                   .arg(jsonString(busToString(id.bus)),
                        QStringLiteral("%1").arg(id.vendor, 4, 16, QLatin1Char('0')),
                        QStringLiteral("%1").arg(id.product, 4, 16, QLatin1Char('0')),
                        jsonString(evdevName),
                        QString::number(w), QString::number(h), sizeNote);
    }
    if (out.isEmpty())
        out = QStringLiteral("no slotted multitouch device found (ABS_MT_SLOT + ABS_MT_POSITION_X/Y)\n");
    return out;
}

EvdevTouchSource::EvdevTouchSource(int fd, libevdev* dev, Info info)
    : fd_(fd), dev_(dev), info_(std::move(info))
{
    // The kernel reports the slot that is current at open time; start from it.
    slot_ = libevdev_get_current_slot(dev_);
    if (slot_ < 0) slot_ = 0;
    notifier_ = std::make_unique<QSocketNotifier>(fd_, QSocketNotifier::Read);
    connect(notifier_.get(), &QSocketNotifier::activated, this, [this] { readAll(); });
}

EvdevTouchSource::~EvdevTouchSource()
{
    notifier_.reset();
    if (dev_) {
        libevdev_grab(dev_, LIBEVDEV_UNGRAB);
        libevdev_free(dev_);
    }
    if (fd_ >= 0)
        ::close(fd_);
}

void EvdevTouchSource::flushPos()
{
    if (px_ || py_) {
        pending_.events.push_back(TouchEvent{TouchEvent::Type::Position, slot_, px_, py_});
        px_.reset();
        py_.reset();
    }
}

void EvdevTouchSource::readAll()
{
    input_event ev{};
    for (;;) {
        const int rc = libevdev_next_event(dev_, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        if (rc == -EAGAIN)
            return;
        if (rc == LIBEVDEV_READ_STATUS_SYNC) {
            // Buffer overrun: drain the sync events, then tell the daemon our slot state is stale.
            while (libevdev_next_event(dev_, LIBEVDEV_READ_FLAG_SYNC, &ev) == LIBEVDEV_READ_STATUS_SYNC) {}
            pending_ = TouchFrame{};
            px_.reset(); py_.reset();
            emit dropped();
            continue;
        }
        if (rc < 0) {
            notifier_->setEnabled(false);
            emit gone(QString::fromLocal8Bit(std::strerror(-rc)));
            return;
        }
        if (ev.type == EV_ABS) {
            switch (ev.code) {
            case ABS_MT_SLOT:
                flushPos();
                slot_ = ev.value;
                break;
            case ABS_MT_TRACKING_ID:
                flushPos();
                pending_.events.push_back(TouchEvent{ev.value >= 0 ? TouchEvent::Type::Begin : TouchEvent::Type::End, slot_, {}, {}});
                break;
            case ABS_MT_POSITION_X: px_ = ev.value; break;
            case ABS_MT_POSITION_Y: py_ = ev.value; break;
            default: break;
            }
        } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
            flushPos();
            TouchFrame f = std::move(pending_);
            pending_ = TouchFrame{};
            emit frame(f);
        }
    }
}

}  // namespace t2t
