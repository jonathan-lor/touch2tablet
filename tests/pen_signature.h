// Test helper: a PenEvent sequence as a string, one letter per event type.
#pragma once

#include "t2t/GestureMachine.h"

#include <QString>
#include <vector>

/// I = ProximityIn, M = Move, D = Down, U = Up, O = ProximityOut; runs of Moves collapse to one 'M'.
inline QString signature(const std::vector<t2t::PenEvent>& events)
{
    using T = t2t::PenEvent::Type;
    QString s;
    for (const auto& e : events) {
        QChar c;
        switch (e.type) {
        case T::ProximityIn: c = 'I'; break;
        case T::Move: c = 'M'; break;
        case T::Down: c = 'D'; break;
        case T::Up: c = 'U'; break;
        case T::ProximityOut: c = 'O'; break;
        }
        if (c == 'M' && s.endsWith('M')) continue;
        s += c;
    }
    return s;
}
