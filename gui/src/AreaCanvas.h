// Outer rectangle (the monitor or the glass) with a draggable, resizable, rotatable inner area
// and a live finger dot.  Units are the model's (px or mm); the widget only scales to fit.
// Right-click menu: Align, Resize, Flip, Lock to usable area; extras can be appended via menu().
#pragma once

#include <QMenu>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QWidget>
#include <optional>

namespace t2t {

class AreaCanvas : public QWidget {
    Q_OBJECT
public:
    struct Area {
        double width = 1, height = 1, x = 0.5, y = 0.5, rotation = 0;
        bool operator==(const Area&) const = default;
    };

    AreaCanvas(QString unit, int digits, bool rotatable, QWidget* parent = nullptr);

    void setModel(QSizeF outer, const Area& area);
    const Area& area() const { return area_; }
    void setDot(std::optional<QPointF> pt, bool inside = true);

    QMenu* menu() { return &menu_; }
    bool lockToUsable() const { return lockUsable_->isChecked(); }
    void setLockToUsable(bool on) { lockUsable_->setChecked(on); }

    /// `a` kept inside `outer`: size clamped (uniformly when keepRatio) while axis-aligned,
    /// then the rotated bounding box pushed back in.
    static Area constrained(Area a, QSizeF outer, bool keepRatio, int digits);

    QSize minimumSizeHint() const override { return {320, 180}; }
    QSize sizeHint() const override { return {640, 300}; }

signals:
    /// Emitted while the user drags/resizes or picks a menu action; already applied to the canvas.
    void areaChanged(const t2t::AreaCanvas::Area& area);
    void lockToUsableChanged(bool on);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;

private:
    enum Hit { None = 0, Move = 1, Left = 2, Right = 4, Top = 8, Bottom = 16 };
    int hitTest(QPointF widgetPos) const;
    Area dragged(int hit, const Area& start, QPointF deltaPx) const;
    void updateScale() const;
    QPointF toModel(QPointF widgetPos) const;
    QPointF toLocal(QPointF widgetPos) const;   // area frame (unrotated), origin = area center
    QString fmt(double v) const;
    void buildMenu(bool rotatable);
    void edit(const std::function<void(Area&)>& f);

    QString unit_;
    int digits_;
    QSizeF outer_{1, 1};
    Area area_;
    std::optional<QPointF> dot_;
    bool dotInside_ = true;
    QMenu menu_;
    QAction* lockUsable_ = nullptr;

    mutable double sc_ = 1;        // widget px per model unit
    mutable QPointF origin_;       // widget position of the outer rect's top-left

    int dragHit_ = None;
    Area dragStart_;
    QPointF pressPos_;
    static constexpr double kHandlePx = 9;
};

}  // namespace t2t
