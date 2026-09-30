// Platform backend interface: something that presents pen events to the OS.
// Linux: UinputSink.  Windows: AbsoluteMouseSink.  Tests inject fakes.
#pragma once

#include "t2t/GestureMachine.h"
#include "t2t/Settings.h"

#include <QString>

namespace t2t {

class IPenSink {
public:
    virtual ~IPenSink() = default;

    /// Create the output device for `s`.  False + err on failure.
    virtual bool open(const Settings& s, QString* err) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    /// True if the open device cannot represent `s` and must be re-created via close()/open().
    virtual bool needsReopen(const Settings& s) const = 0;

    virtual void handle(const PenEvent& ev) = 0;

    virtual QString path() const = 0;   // node / identifier for logs and status
    virtual QString kind() const { return QStringLiteral("virtual device"); }   // what the OS sees, for logs
    virtual QString lastError() const { return QStringLiteral("output unavailable"); }
};

}  // namespace t2t
