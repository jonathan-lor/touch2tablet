#include "Daemon.h"

namespace t2t {

Daemon::Daemon(Settings settings, SourceProbe probe, SinkFactory makeSink, QObject* parent)
    : QObject(parent), settings_(std::move(settings)), probe_(std::move(probe)), makeSink_(std::move(makeSink))
{
    connect(&scanTimer_, &QTimer::timeout, this, &Daemon::scan);
}

Daemon::~Daemon()
{
    stop();
}

void Daemon::start()
{
    running_ = true;
    scan();
    if (!source_)
        scanTimer_.start(scanIntervalMs);
}

void Daemon::stop()
{
    running_ = false;
    scanTimer_.stop();
    if (source_)
        detach(QStringLiteral("shutdown"));
}

// -- device lifecycle ---------------------------------------------------------------------

void Daemon::scan()
{
    if (source_ || !running_)
        return;
    std::unique_ptr<ITouchSource> src = probe_ ? probe_() : nullptr;
    if (src)
        attach(std::move(src));
}

void Daemon::attach(std::unique_ptr<ITouchSource> src)
{
    auto sink = makeSink_();
    QString err;
    if (!sink || !sink->open(settings_, &err)) {
        emit log(QStringLiteral("cannot create virtual tablet: %1").arg(err));
        return;   // keep scanning; the source is dropped and re-probed next time
    }
    source_ = std::move(src);
    sink_ = std::move(sink);
    const auto info = source_->info();
    if (const Settings fit = fitted(settings_); fit != settings_) {
        settings_ = fit;
        emit settingsChanged(settings_);
    }
    machine_ = std::make_unique<GestureMachine>(settings_, info.range, [this](const PenEvent& e) {
        if (sink_ && sink_->isOpen()) sink_->handle(e);
    });

    connect(source_.get(), &ITouchSource::frame, this, &Daemon::onFrame);
    connect(source_.get(), &ITouchSource::dropped, this, [this] {
        emit log(QStringLiteral("touch stream lost sync; resetting"));
        machine_->reset();
    });
    connect(source_.get(), &ITouchSource::gone, this, [this](const QString& why) {
        detach(why);
        scanTimer_.start(scanIntervalMs);
    });

    scanTimer_.stop();
    emit log(QStringLiteral("grabbed %1 (%2, %3) raw %4..%5 x %6..%7; virtual tablet at %8")
                 .arg(info.path, info.name, info.panel->name)
                 .arg(info.range.xmin).arg(info.range.xmax).arg(info.range.ymin).arg(info.range.ymax)
                 .arg(sink_->path()));
    emit deviceChanged(true);
}

void Daemon::detach(const QString& why)
{
    recoveryPending_ = false;
    if (machine_)
        machine_->reset();
    machine_.reset();
    if (source_) {
        source_->disconnect(this);
        source_.reset();
    }
    if (sink_) {
        sink_->close();
        sink_.reset();
    }
    emit log(QStringLiteral("device detached (%1); waiting for it to return").arg(why));
    emit deviceChanged(false);
}

// -- data path ----------------------------------------------------------------------------

void Daemon::onFrame(const TouchFrame& f)
{
    if (!machine_)
        return;
    for (const auto& ev : f.events) {
        switch (ev.type) {
        case TouchEvent::Type::Begin: machine_->trackingBegin(ev.slot); break;
        case TouchEvent::Type::End: machine_->trackingEnd(ev.slot); break;
        case TouchEvent::Type::Position: machine_->position(ev.slot, ev.x, ev.y); break;
        }
    }
    machine_->frame();
    ++frameCount_;
    emit frameProcessed(machine_->state());
    recoverOutput();
}

void Daemon::recoverOutput()
{
    if (!source_ || !sink_ || sink_->isOpen() || recoveryPending_) return;
    recoveryPending_ = true;
    const QString reason = sink_->lastError();
    // Wait until any native input callback has returned before deleting its source.
    QTimer::singleShot(0, source_.get(), [this, reason] {
        detach(QStringLiteral("pen output failed: %1").arg(reason));
        if (running_) scanTimer_.start(scanIntervalMs);
    });
}

// -- settings -----------------------------------------------------------------------------

// The glass size is the attached panel's, not a preference; `tablet` remembers it while the panel
// is away.  An area laid out on a different glass does not carry over.
Settings Daemon::fitted(Settings s)
{
    if (!source_) return s;
    const Panel* p = source_->info().panel;
    if (s.tablet.width != p->widthMm || s.tablet.height != p->heightMm) {
        emit log(QStringLiteral("settings were made for a %1x%2mm glass, this one is %3x%4mm: tablet area reset to the full glass")
                     .arg(s.tablet.width).arg(s.tablet.height).arg(p->widthMm).arg(p->heightMm));
        s.tablet = {p->widthMm, p->heightMm};
        s.tabletArea = {p->widthMm, p->heightMm, p->widthMm / 2.0, p->heightMm / 2.0, 0.0};
    }
    return s;
}

void Daemon::applySettings(const Settings& requested)
{
    const Settings s = fitted(requested);
    settings_ = s;
    if (sink_ && sink_->needsReopen(s)) {
        emit log(QStringLiteral("display size changed: recreating virtual tablet"));
        if (machine_)
            machine_->reset();
        sink_->close();
        QString err;
        if (!sink_->open(s, &err))
            emit log(QStringLiteral("cannot recreate virtual tablet: %1").arg(err));
    }
    if (machine_)
        machine_->setSettings(s);
    recoverOutput();
    emit log(QStringLiteral("settings applied: tablet area %1x%2mm @(%3,%4) rot %5 -> display %6x%7 @(%8,%9); clip %10 limit %11")
                 .arg(s.tabletArea.width, 0, 'f', 2).arg(s.tabletArea.height, 0, 'f', 2)
                 .arg(s.tabletArea.x, 0, 'f', 2).arg(s.tabletArea.y, 0, 'f', 2).arg(s.tabletArea.rotation, 0, 'f', 1)
                 .arg(s.displayArea.width, 0, 'f', 0).arg(s.displayArea.height, 0, 'f', 0)
                 .arg(s.displayArea.x, 0, 'f', 0).arg(s.displayArea.y, 0, 'f', 0)
                 .arg(s.clip ? "on" : "off", s.limit ? "on" : "off"));
    emit settingsChanged(s);
}

std::optional<ITouchSource::Info> Daemon::sourceInfo() const
{
    if (!source_) return std::nullopt;
    return source_->info();
}

QString Daemon::sinkPath() const
{
    return sink_ && sink_->isOpen() ? sink_->path() : QString();
}

}  // namespace t2t
