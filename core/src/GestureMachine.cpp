#include "t2t/GestureMachine.h"

#include <algorithm>
#include <utility>

namespace t2t {

GestureMachine::GestureMachine(const Settings& s, const RawRange& raw, Emit sink)
    : settings_(s), raw_(raw), mapper_(s, raw), emit_(std::move(sink))
{
}

void GestureMachine::setSettings(const Settings& s)
{
    settings_ = s;
    mapper_ = Mapper(settings_, raw_);
    updateState();
}

void GestureMachine::setRawRange(const RawRange& raw)
{
    raw_ = raw;
    mapper_ = Mapper(settings_, raw_);
    updateState();
}

GestureMachine::Slot* GestureMachine::find(int slot)
{
    auto it = std::find_if(slots_.begin(), slots_.end(), [&](const Slot& s) { return s.id == slot; });
    return it == slots_.end() ? nullptr : &*it;
}

GestureMachine::Slot* GestureMachine::primary()
{
    return primary_ ? find(*primary_) : nullptr;
}

void GestureMachine::trackingBegin(int slot)
{
    if (!find(slot))
        slots_.push_back(Slot{slot, std::nullopt, std::nullopt});
    if (!primary_)
        primary_ = slot;
}

void GestureMachine::trackingEnd(int slot)
{
    std::erase_if(slots_, [&](const Slot& s) { return s.id == slot; });
    if (!primary_ || *primary_ != slot || slots_.empty())
        return;
    // The stroke's finger is gone but others remain: finish the stroke, ignore the leftovers.
    if (active_) {
        emit_(PenEvent{PenEvent::Type::Up});
        emit_(PenEvent{PenEvent::Type::ProximityOut});
        active_ = false;
    }
    primary_.reset();
    ignored_ = true;
}

void GestureMachine::position(int slot, std::optional<int> x, std::optional<int> y)
{
    // Coordinates for a slot we have not seen a tracking id for are dropped, as libinput does.
    if (Slot* s = find(slot)) {
        if (x) s->x = *x;
        if (y) s->y = *y;
    }
}

void GestureMachine::frame()
{
    if (slots_.empty()) {
        release();
    } else if (!ignored_) {
        Slot* p = primary();
        if (!p || !p->ready()) {
            // Primary has no coordinates yet; use any slot that does, else wait for the next frame.
            auto it = std::find_if(slots_.begin(), slots_.end(), [](const Slot& s) { return s.ready(); });
            if (it == slots_.end()) { updateState(); return; }
            primary_ = it->id;
            p = &*it;
        }
        const MapResult m = mapper_.map(*p->x, *p->y);
        if (active_) {
            emit_(PenEvent{PenEvent::Type::Move, m.x, m.y});
        } else if (settings_.limit && !m.inside) {
            ignored_ = true;
        } else {
            active_ = true;
            emit_(PenEvent{PenEvent::Type::ProximityIn});
            emit_(PenEvent{PenEvent::Type::Move, m.x, m.y});   // hover first, so the compositor re-picks
            emit_(PenEvent{PenEvent::Type::Down, m.x, m.y});
        }
    }
    updateState();
}

void GestureMachine::reset()
{
    slots_.clear();
    release();
    updateState();
}

void GestureMachine::release()
{
    if (active_) {
        emit_(PenEvent{PenEvent::Type::Up});
        emit_(PenEvent{PenEvent::Type::ProximityOut});
        active_ = false;
    }
    ignored_ = false;
    primary_.reset();
}

void GestureMachine::updateState()
{
    state_.fingers = static_cast<int>(slots_.size());
    state_.active = active_;
    state_.ignored = ignored_;
    Slot* p = primary();
    state_.primaryReady = p && p->ready();
    if (state_.primaryReady) {
        state_.rawX = *p->x;
        state_.rawY = *p->y;
        state_.mapped = mapper_.map(*p->x, *p->y);
    }
}

}  // namespace t2t
