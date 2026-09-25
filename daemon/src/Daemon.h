// Orchestration: hotplug scanning -> ITouchSource frames -> GestureMachine -> IPenSink.
// Platform-neutral; backends are injected as factories so tests can use fakes.
#pragma once

#include "IPenSink.h"
#include "ITouchSource.h"
#include "t2t/GestureMachine.h"
#include "t2t/Settings.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>
#include <memory>
#include <optional>

namespace t2t {

class Daemon : public QObject {
    Q_OBJECT
public:
    /// Try to open the touch panel; nullptr if it is not present right now.
    using SourceProbe = std::function<std::unique_ptr<ITouchSource>()>;
    using SinkFactory = std::function<std::unique_ptr<IPenSink>()>;

    Daemon(Settings settings, SourceProbe probe, SinkFactory makeSink, QObject* parent = nullptr);
    ~Daemon() override;

    void start();   // begin scanning for the device
    void stop();    // release everything (called before exit)

    const Settings& settings() const { return settings_; }
    /// Live update: swaps the mapper, re-creates the sink if it says so.  `tablet` always follows
    /// the attached panel (see fitted()), so read settings() back for what actually runs.
    void applySettings(const Settings& s);

    bool connected() const { return source_ != nullptr; }
    bool running() const { return running_; }
    quint64 frameCount() const { return frameCount_; }
    std::optional<GestureMachine::State> inputState() const {
        return machine_ ? std::optional<GestureMachine::State>(machine_->state()) : std::nullopt;
    }
    std::optional<ITouchSource::Info> sourceInfo() const;
    QString sinkPath() const;

    int scanIntervalMs = 2000;

signals:
    void log(const QString& line);
    void deviceChanged(bool connected);
    void settingsChanged(const t2t::Settings& s);
    /// After every processed frame, for status streams (GUI live dot).
    void frameProcessed(const t2t::GestureMachine::State& st);

private:
    void scan();
    void attach(std::unique_ptr<ITouchSource> src);
    void detach(const QString& why);
    void onFrame(const TouchFrame& f);
    void recoverOutput();
    Settings fitted(Settings s);

    Settings settings_;
    SourceProbe probe_;
    SinkFactory makeSink_;
    std::unique_ptr<ITouchSource> source_;
    std::unique_ptr<IPenSink> sink_;
    std::unique_ptr<GestureMachine> machine_;
    QTimer scanTimer_;
    bool running_ = false;
    bool recoveryPending_ = false;
    quint64 frameCount_ = 0;
};

}  // namespace t2t
