// Platform backend interface: a multitouch panel delivering slot frames.
//
// Linux: EvdevTouchSource (libevdev, exclusive grab).  Tests inject fakes.
// The daemon owns the clock; frames are stamped on arrival.
#pragma once

#include "Panels.h"
#include "TouchTypes.h"
#include "t2t/Mapper.h"

#include <QObject>
#include <QString>

namespace t2t {

class ITouchSource : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

    struct Info {
        const Panel* panel = nullptr;   // entry of panels() this device matched
        QString name;                   // device name as the OS reports it
        QString path;                   // node / identifier for logs and status
        RawRange range;                 // what raw coordinates span
    };

    virtual Info info() const = 0;

signals:
    void frame(const t2t::TouchFrame& frame);
    /// The device's event stream lost sync (e.g. evdev SYN_DROPPED); slot state must be reset.
    void dropped();
    /// The device went away; the source is unusable afterwards.
    void gone(const QString& why);
};

}  // namespace t2t
