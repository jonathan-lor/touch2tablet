#include "AreaCanvas.h"

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QtMath>
#include <algorithm>

namespace t2t {

namespace {

const QColor kAreaBlue(26, 133, 219);

// Half extents of the area's rotated bounding box.
QPointF halfExtents(const AreaCanvas::Area& a)
{
    const double r = qDegreesToRadians(a.rotation), c = std::abs(std::cos(r)), s = std::abs(std::sin(r));
    return {a.width / 2 * c + a.height / 2 * s, a.width / 2 * s + a.height / 2 * c};
}

double roundTo(double v, int digits)
{
    const double q = std::pow(10.0, digits);
    return std::round(v * q) / q;
}

}  // namespace

AreaCanvas::AreaCanvas(QString unit, int digits, bool rotatable, QWidget* parent)
    : QWidget(parent), unit_(std::move(unit)), digits_(digits)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setToolTip(QStringLiteral("Drag to move, drag an edge to resize, right-click to align, resize or flip."));
    buildMenu(rotatable);
}

void AreaCanvas::setModel(QSizeF outer, const Area& area)
{
    outer_ = outer;
    area_ = area;
    update();
}

void AreaCanvas::setDot(std::optional<QPointF> pt, bool inside)
{
    dot_ = pt;
    dotInside_ = inside;
    update();
}

QString AreaCanvas::fmt(double v) const
{
    return digits_ ? QString::number(v, 'f', digits_) : QString::number(qRound(v));
}

// -- menu ---------------------------------------------------------------------------------

void AreaCanvas::buildMenu(bool rotatable)
{
    QMenu* align = menu_.addMenu(QStringLiteral("Align"));
    align->addAction(QStringLiteral("Left"), this, [this] { edit([](Area& a) { a.x = halfExtents(a).x(); }); });
    align->addAction(QStringLiteral("Right"), this, [this] { edit([this](Area& a) { a.x = outer_.width() - halfExtents(a).x(); }); });
    align->addAction(QStringLiteral("Top"), this, [this] { edit([](Area& a) { a.y = halfExtents(a).y(); }); });
    align->addAction(QStringLiteral("Bottom"), this, [this] { edit([this](Area& a) { a.y = outer_.height() - halfExtents(a).y(); }); });
    align->addAction(QStringLiteral("Center"), this, [this] {
        edit([this](Area& a) { a.x = outer_.width() / 2; a.y = outer_.height() / 2; });
    });
    QMenu* resize = menu_.addMenu(QStringLiteral("Resize"));
    resize->addAction(QStringLiteral("Full area"), this, [this] {
        edit([this](Area& a) { a = {outer_.width(), outer_.height(), outer_.width() / 2, outer_.height() / 2, a.rotation}; });
    });
    resize->addAction(QStringLiteral("Quarter area"), this, [this] {
        edit([this](Area& a) { a.width = outer_.width() / 2; a.height = outer_.height() / 2; });
    });
    QMenu* flip = menu_.addMenu(QStringLiteral("Flip"));
    flip->addAction(QStringLiteral("Horizontal"), this, [this] { edit([this](Area& a) { a.x = outer_.width() - a.x; }); });
    flip->addAction(QStringLiteral("Vertical"), this, [this] { edit([this](Area& a) { a.y = outer_.height() - a.y; }); });
    if (rotatable) {
        flip->addAction(QStringLiteral("Handedness"), this, [this] {
            edit([this](Area& a) {
                a.rotation = std::fmod(a.rotation + 180.0, 360.0);
                a.x = outer_.width() - a.x;
                a.y = outer_.height() - a.y;
            });
        });
    }
    lockUsable_ = menu_.addAction(QStringLiteral("Lock to usable area"));
    lockUsable_->setCheckable(true);
    lockUsable_->setChecked(true);
    connect(lockUsable_, &QAction::toggled, this, &AreaCanvas::lockToUsableChanged);
}

void AreaCanvas::edit(const std::function<void(Area&)>& f)
{
    Area a = area_;
    f(a);
    for (double* v : {&a.width, &a.height, &a.x, &a.y}) *v = roundTo(*v, digits_);
    area_ = a;
    update();
    emit areaChanged(area_);
}

void AreaCanvas::contextMenuEvent(QContextMenuEvent* e)
{
    menu_.popup(e->globalPos());
}

AreaCanvas::Area AreaCanvas::constrained(Area a, QSizeF outer, bool keepRatio, int digits)
{
    if (std::fmod(a.rotation, 180.0) == 0.0) {
        if (keepRatio) {
            const double s = std::min({1.0, outer.width() / a.width, outer.height() / a.height});
            a.width *= s;
            a.height *= s;
        } else {
            a.width = std::min(a.width, outer.width());
            a.height = std::min(a.height, outer.height());
        }
    }
    const QPointF e = halfExtents(a);
    a.x = 2 * e.x() >= outer.width() ? outer.width() / 2 : std::clamp(a.x, e.x(), outer.width() - e.x());
    a.y = 2 * e.y() >= outer.height() ? outer.height() / 2 : std::clamp(a.y, e.y(), outer.height() - e.y());
    for (double* v : {&a.width, &a.height, &a.x, &a.y}) *v = roundTo(*v, digits);
    return a;
}

// -- geometry -----------------------------------------------------------------------------

void AreaCanvas::updateScale() const
{
    const double m = 40;
    const double w = std::max(1.0, width() - 2 * m), h = std::max(1.0, height() - 2 * m);
    sc_ = std::max(1e-9, std::min(w / outer_.width(), h / outer_.height()));
    origin_ = QPointF((width() - outer_.width() * sc_) / 2, (height() - outer_.height() * sc_) / 2);
}

QPointF AreaCanvas::toModel(QPointF p) const
{
    updateScale();
    return (p - origin_) / sc_;
}

QPointF AreaCanvas::toLocal(QPointF p) const
{
    const QPointF u = toModel(p);
    const double dx = u.x() - area_.x, dy = u.y() - area_.y;
    const double r = qDegreesToRadians(area_.rotation), c = std::cos(r), s = std::sin(r);
    return {dx * c + dy * s, -dx * s + dy * c};
}

int AreaCanvas::hitTest(QPointF p) const
{
    const QPointF l = toLocal(p);
    const double w = area_.width, h = area_.height, tol = kHandlePx / sc_;
    int hit = None;
    if (std::abs(l.x() + w / 2) < tol && std::abs(l.y()) <= h / 2 + tol) hit |= Left;
    if (std::abs(l.x() - w / 2) < tol && std::abs(l.y()) <= h / 2 + tol) hit |= Right;
    if (std::abs(l.y() + h / 2) < tol && std::abs(l.x()) <= w / 2 + tol) hit |= Top;
    if (std::abs(l.y() - h / 2) < tol && std::abs(l.x()) <= w / 2 + tol) hit |= Bottom;
    if (hit) return hit;
    if (std::abs(l.x()) <= w / 2 && std::abs(l.y()) <= h / 2) return Move;
    return None;
}

AreaCanvas::Area AreaCanvas::dragged(int hit, const Area& a0, QPointF d) const
{
    updateScale();
    Area a = a0;
    const double minSz = digits_ ? std::pow(10.0, -digits_) : 1.0;
    if (hit == Move) {
        a.x = a0.x + d.x() / sc_;
        a.y = a0.y + d.y() / sc_;
    } else if (hit) {
        const double r = qDegreesToRadians(a0.rotation), c = std::cos(r), s = std::sin(r);
        const double dlx = (d.x() * c + d.y() * s) / sc_, dly = (-d.x() * s + d.y() * c) / sc_;
        double l = -a0.width / 2, rt = a0.width / 2, t = -a0.height / 2, b = a0.height / 2;
        if (hit & Left) l = std::min(l + dlx, rt - minSz);
        if (hit & Right) rt = std::max(rt + dlx, l + minSz);
        if (hit & Top) t = std::min(t + dly, b - minSz);
        if (hit & Bottom) b = std::max(b + dly, t + minSz);
        a.width = rt - l;
        a.height = b - t;
        const double cl = (l + rt) / 2, ct = (t + b) / 2;
        a.x = a0.x + cl * c - ct * s;
        a.y = a0.y + cl * s + ct * c;
    }
    for (double* v : {&a.width, &a.height, &a.x, &a.y}) *v = roundTo(*v, digits_);
    return a;
}

// -- mouse --------------------------------------------------------------------------------

void AreaCanvas::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    dragHit_ = hitTest(e->position());
    if (dragHit_ == None) return;
    dragStart_ = area_;
    pressPos_ = e->position();
}

void AreaCanvas::mouseMoveEvent(QMouseEvent* e)
{
    if (dragHit_ != None && (e->buttons() & Qt::LeftButton)) {
        area_ = dragged(dragHit_, dragStart_, e->position() - pressPos_);
        update();
        emit areaChanged(area_);
        return;
    }
    const int hit = hitTest(e->position());
    Qt::CursorShape shape = Qt::ArrowCursor;
    if (hit == Move) shape = Qt::SizeAllCursor;
    else if (hit == Left || hit == Right) shape = Qt::SizeHorCursor;
    else if (hit == Top || hit == Bottom) shape = Qt::SizeVerCursor;
    else if (hit == (Left | Top) || hit == (Right | Bottom)) shape = Qt::SizeFDiagCursor;
    else if (hit) shape = Qt::SizeBDiagCursor;
    setCursor(shape);
}

void AreaCanvas::mouseReleaseEvent(QMouseEvent*)
{
    dragHit_ = None;
}

void AreaCanvas::leaveEvent(QEvent*)
{
    setCursor(Qt::ArrowCursor);
}

// -- painting -----------------------------------------------------------------------------

void AreaCanvas::paintEvent(QPaintEvent*)
{
    updateScale();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const QColor fg = palette().color(QPalette::WindowText);
    QFont f = font();
    f.setPointSizeF(std::max(7.0, f.pointSizeF() - 1));
    p.setFont(f);
    const QFontMetricsF fm(f);

    // outer
    const QRectF outer(origin_, QSizeF(outer_.width() * sc_, outer_.height() * sc_));
    QColor fill = fg; fill.setAlphaF(0.05);
    QColor line = fg; line.setAlphaF(0.55);
    p.fillRect(outer, fill);
    p.setPen(QPen(line, 1));
    p.drawRect(outer);
    QColor dim = fg; dim.setAlphaF(0.6);
    p.setPen(dim);
    const QString outerLabel = QStringLiteral("%1 × %2 %3").arg(fmt(outer_.width()), fmt(outer_.height()), unit_);
    p.drawText(QPointF(outer.right() - fm.horizontalAdvance(outerLabel), outer.bottom() + fm.ascent() + 4), outerLabel);

    // area (rotated frame)
    const double aw = area_.width * sc_, ah = area_.height * sc_;
    p.save();
    p.translate(origin_ + QPointF(area_.x * sc_, area_.y * sc_));
    p.rotate(area_.rotation);
    const QRectF ar(-aw / 2, -ah / 2, aw, ah);
    QColor afill = kAreaBlue; afill.setAlphaF(0.8);
    p.fillRect(ar, afill);
    p.setPen(QPen(kAreaBlue, 1));
    p.drawRect(ar);
    const double hs = kHandlePx - 3;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 240));
    for (double hx : {-aw / 2, aw / 2})
        for (double hy : {-ah / 2, ah / 2})
            p.drawRect(QRectF(hx - hs / 2, hy - hs / 2, hs, hs));
    // Labels stay readable at any rotation: drawn centered on an anchor in the area's frame,
    // flipped by 180 when the net rotation would put them upside down.
    auto label = [&](QPointF anchor, double rot, const QString& text) {
        p.save();
        p.translate(anchor);
        double net = std::fmod(area_.rotation + rot, 360.0);
        if (net < 0) net += 360.0;
        p.rotate(rot + (net > 90.0 && net <= 270.0 ? 180.0 : 0.0));
        const double w = fm.horizontalAdvance(text) + 4, h = fm.height();
        p.drawText(QRectF(-w / 2, -h / 2, w, h), Qt::AlignCenter, text);
        p.restore();
    };
    const double off = fm.height() / 2 + 4;
    QColor col = fg; col.setAlphaF(0.9);
    p.setPen(col);
    label(QPointF(0, -ah / 2 - off), 0, QStringLiteral("%1 %2").arg(fmt(area_.width), unit_));
    label(QPointF(-aw / 2 - off, 0), -90, QStringLiteral("%1 %2").arg(fmt(area_.height), unit_));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 230));
    p.drawEllipse(QPointF(0, 0), 2.5, 2.5);
    p.setPen(QColor(255, 255, 255, 230));
    label(QPointF(0, off + 4), 0, QStringLiteral("%1, %2").arg(fmt(area_.x), fmt(area_.y)));
    p.restore();

    // live dot
    if (dot_) {
        const QPointF d = origin_ + QPointF(dot_->x() * sc_, dot_->y() * sc_);
        p.setPen(QPen(QColor(255, 255, 255, 230), 1.5));
        p.setBrush(dotInside_ ? QColor(51, 217, 89) : QColor(242, 77, 64));
        p.drawEllipse(d, 6.5, 6.5);
    }
}

}  // namespace t2t
