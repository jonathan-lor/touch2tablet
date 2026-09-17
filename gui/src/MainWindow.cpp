#include "MainWindow.h"

#include "t2t/SettingsStore.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QKeySequence>
#include <QMenuBar>
#include <QMessageBox>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>
#include <cmath>
#include <string_view>

namespace t2t {

namespace {

QDoubleSpinBox* dspin(double lo, double hi, double step, int digits, const QString& suffix)
{
    auto* s = new QDoubleSpinBox;
    s->setRange(lo, hi);
    s->setSingleStep(step);
    s->setDecimals(digits);
    s->setSuffix(suffix);
    s->setKeyboardTracking(false);
    s->setMinimumWidth(110);
    return s;
}

QSpinBox* ispin(int lo, int hi, const QString& suffix)
{
    auto* s = new QSpinBox;
    s->setRange(lo, hi);
    s->setSuffix(suffix);
    s->setKeyboardTracking(false);
    s->setMinimumWidth(110);
    return s;
}

QWidget* field(const QString& label, QWidget* w)
{
    auto* box = new QWidget;
    auto* l = new QVBoxLayout(box);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(2);
    auto* lab = new QLabel(label);
    lab->setStyleSheet(QStringLiteral("color: palette(placeholder-text);"));
    l->addWidget(lab);
    l->addWidget(w);
    return box;
}

AreaCanvas::Area toArea(const TabletArea& t) { return {t.width, t.height, t.x, t.y, t.rotation}; }
AreaCanvas::Area toArea(const DisplayArea& d) { return {d.width, d.height, d.x, d.y, 0.0}; }

// A menu entry that mirrors a checkbox (OpenTabletDriver keeps these toggles in the area menu too).
void mirror(QMenu* menu, QCheckBox* cb)
{
    QAction* a = menu->addAction(cb->text());
    a->setCheckable(true);
    QObject::connect(a, &QAction::toggled, cb, &QCheckBox::setChecked);
    QObject::connect(cb, &QCheckBox::toggled, a, &QAction::setChecked);
}

bool looksLikeError(const QString& line)
{
    return line.startsWith(u"cannot ") || line.startsWith(u"error");
}

}  // namespace

MainWindow::MainWindow(ControlClient& client, QWidget* parent)
    : QMainWindow(parent), client_(client)
{
    setWindowTitle(QStringLiteral("touch2tablet"));
    resize(920, 880);

    auto* central = new QWidget;
    auto* v = new QVBoxLayout(central);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    banner_ = new QLabel;
    banner_->setStyleSheet(QStringLiteral("background: #b3261e; color: white; padding: 6px 12px;"));
    banner_->setWordWrap(true);
    banner_->hide();
    v->addWidget(banner_);

    tabs_ = new QTabWidget;
    tabs_->addTab(buildOutputTab(), QStringLiteral("Output"));
    tabs_->addTab(buildSettingsTab(), QStringLiteral("Settings"));
    tabs_->addTab(buildConsoleTab(), QStringLiteral("Console"));
    v->addWidget(tabs_, 1);

    auto* bar = new QWidget;
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(12, 6, 12, 6);
    status_ = new QLabel;
    status_->setStyleSheet(QStringLiteral("color: palette(placeholder-text);"));
    h->addWidget(status_, 1);
    btnSave_ = new QPushButton(QStringLiteral("Save"));
    btnApply_ = new QPushButton(QStringLiteral("Apply"));
    h->addWidget(btnSave_); h->addWidget(btnApply_);
    v->addWidget(bar);
    setCentralWidget(central);

    buildMenu();
    wire();
    updateStatus();
}

// -- tabs ---------------------------------------------------------------------------------

QWidget* MainWindow::buildOutputTab()
{
    auto* page = new QWidget;
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(12, 12, 12, 12);
    v->setSpacing(12);

    auto* g1 = new QGroupBox(QStringLiteral("Display"));
    auto* v1 = new QVBoxLayout(g1);
    dispCanvas_ = new AreaCanvas(QStringLiteral("px"), 0, /*rotatable=*/false);
    v1->addWidget(dispCanvas_, 1);
    auto* r1 = new QHBoxLayout;
    spDW_ = dspin(1, 65536, 1, 0, QStringLiteral(" px"));
    spDH_ = dspin(1, 65536, 1, 0, QStringLiteral(" px"));
    spDX_ = dspin(-65536, 65536, 1, 0, QStringLiteral(" px"));
    spDY_ = dspin(-65536, 65536, 1, 0, QStringLiteral(" px"));
    r1->addWidget(field(QStringLiteral("Width"), spDW_));
    r1->addWidget(field(QStringLiteral("Height"), spDH_));
    r1->addWidget(field(QStringLiteral("X"), spDX_));
    r1->addWidget(field(QStringLiteral("Y"), spDY_));
    r1->addStretch(1);
    v1->addLayout(r1);

    auto* g2 = new QGroupBox(QStringLiteral("Tablet"));
    auto* v2 = new QVBoxLayout(g2);
    tabCanvas_ = new AreaCanvas(QStringLiteral("mm"), 2, /*rotatable=*/true);
    v2->addWidget(tabCanvas_, 1);
    auto* r2 = new QHBoxLayout;
    spTW_ = dspin(0.5, 5000, 0.5, 2, QStringLiteral(" mm"));
    spTH_ = dspin(0.5, 5000, 0.5, 2, QStringLiteral(" mm"));
    spTX_ = dspin(-5000, 5000, 0.5, 2, QStringLiteral(" mm"));
    spTY_ = dspin(-5000, 5000, 0.5, 2, QStringLiteral(" mm"));
    spRot_ = dspin(-360, 720, 1, 1, QStringLiteral(" °"));
    r2->addWidget(field(QStringLiteral("Width"), spTW_));
    r2->addWidget(field(QStringLiteral("Height"), spTH_));
    r2->addWidget(field(QStringLiteral("X"), spTX_));
    r2->addWidget(field(QStringLiteral("Y"), spTY_));
    r2->addWidget(field(QStringLiteral("Rotation"), spRot_));
    r2->addStretch(1);
    v2->addLayout(r2);
    auto* r3 = new QHBoxLayout;
    ckLock_ = new QCheckBox(QStringLiteral("Lock aspect ratio"));
    ckClip_ = new QCheckBox(QStringLiteral("Clamp input outside area"));
    ckLimit_ = new QCheckBox(QStringLiteral("Ignore input outside area"));
    r3->addWidget(ckLock_); r3->addWidget(ckClip_); r3->addWidget(ckLimit_); r3->addStretch(1);
    v2->addLayout(r3);
    tabCanvas_->menu()->addSeparator();
    for (QCheckBox* cb : {ckLock_, ckClip_, ckLimit_}) mirror(tabCanvas_->menu(), cb);

    v->addWidget(g1, 1);
    v->addWidget(g2, 1);
    return page;
}

QWidget* MainWindow::buildSettingsTab()
{
    auto* page = new QWidget;
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(12, 12, 12, 12);

    auto* g1 = new QGroupBox(QStringLiteral("Panel"));
    auto* f1 = new QFormLayout(g1);
    panelName_ = new QLabel;
    glassSize_ = new QLabel;
    f1->addRow(QStringLiteral("Device"), panelName_);
    f1->addRow(QStringLiteral("Glass"), glassSize_);

    auto* g2 = new QGroupBox(QStringLiteral("Monitor (the virtual tablet spans this)"));
    auto* f2 = new QFormLayout(g2);
    spMW_ = ispin(16, 32768, QStringLiteral(" px"));
    spMH_ = ispin(16, 32768, QStringLiteral(" px"));
    spMWmm_ = dspin(10, 5000, 1, 0, QStringLiteral(" mm"));
    spMHmm_ = dspin(10, 5000, 1, 0, QStringLiteral(" mm"));
    f2->addRow(QStringLiteral("Width"), spMW_);
    f2->addRow(QStringLiteral("Height"), spMH_);
    f2->addRow(QStringLiteral("Physical width"), spMWmm_);
    f2->addRow(QStringLiteral("Physical height"), spMHmm_);
    auto* det = new QPushButton(QStringLiteral("Detect from the monitor this window is on"));
    connect(det, &QPushButton::clicked, this, &MainWindow::detectMonitor);
    f2->addRow(det);

    v->addWidget(g1); v->addWidget(g2);
    v->addStretch(1);
    return page;
}

QWidget* MainWindow::buildConsoleTab()
{
    auto* page = new QWidget;
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(12, 12, 12, 12);
    auto* bar = new QHBoxLayout;
    auto* clear = new QPushButton(QStringLiteral("Clear"));
    connect(clear, &QPushButton::clicked, this, [this] { console_->clear(); });
    bar->addWidget(clear); bar->addStretch(1);
    v->addLayout(bar);
    console_ = new QPlainTextEdit;
    console_->setReadOnly(true);
    console_->setMaximumBlockCount(3000);
    console_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    v->addWidget(console_, 1);
    return page;
}

void MainWindow::buildMenu()
{
    QMenu* file = menuBar()->addMenu(QStringLiteral("&File"));
    actApply_ = file->addAction(QStringLiteral("Apply settings"), QKeySequence(Qt::CTRL | Qt::Key_Return), this, &MainWindow::apply);
    actSave_ = file->addAction(QStringLiteral("Save settings"), QKeySequence::Save, this, &MainWindow::save);
    file->addSeparator();
    file->addAction(QStringLiteral("Save as preset..."), this, &MainWindow::saveAsPreset);
    file->addAction(QStringLiteral("Open presets folder"), this, [this] {
        QDir().mkpath(presetsDir());
        QDesktopServices::openUrl(QUrl::fromLocalFile(presetsDir()));
    });
    presetsMenu_ = file->addMenu(QStringLiteral("Presets"));
    connect(presetsMenu_, &QMenu::aboutToShow, this, &MainWindow::rebuildPresetsMenu);
    file->addSeparator();
    actReset_ = file->addAction(QStringLiteral("Reset to defaults"), this, &MainWindow::resetToDefaults);
    file->addSeparator();
    file->addAction(QStringLiteral("Quit"), QKeySequence::Quit, this, &QWidget::close);
}

// -- presets: whole (or partial) settings files next to settings.json, applied live ---------

QString MainWindow::presetsDir() const
{
    const QString sp = info_.value("settings_path").toString();
    const QString base = sp.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/touch2tablet")
        : QFileInfo(sp).dir().path();
    return base + QStringLiteral("/presets");
}

void MainWindow::rebuildPresetsMenu()
{
    presetsMenu_->clear();
    const QDir dir(presetsDir());
    const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    if (files.isEmpty()) {
        presetsMenu_->addAction(QStringLiteral("No presets"))->setEnabled(false);
        return;
    }
    for (const QString& f : files) {
        const QString path = dir.filePath(f);
        presetsMenu_->addAction(QFileInfo(f).completeBaseName(), this, [this, path] { applyPreset(path); })
            ->setEnabled(client_.isConnected() && haveSettings_);
    }
}

void MainWindow::applyPreset(const QString& path)
{
    QString err;
    const auto json = SettingsStore::read(path, &err);
    if (!json) {
        logLine(QStringLiteral("error: cannot load preset %1: %2").arg(path, err));
        return;
    }
    s_ = Settings::applied(s_, *json);   // a partial preset keeps the rest of the working copy
    changed();
    apply();
    logLine(QStringLiteral("applied preset '%1'").arg(QFileInfo(path).completeBaseName()));
}

void MainWindow::saveAsPreset()
{
    if (!haveSettings_) return;
    bool ok = false;
    QString name = QInputDialog::getText(this, QStringLiteral("Save as preset"), QStringLiteral("Preset name:"),
                                         QLineEdit::Normal, QString(), &ok).trimmed();
    name.replace('/', '-');
    if (!ok || name.isEmpty()) return;
    const QString path = presetsDir() + '/' + name + QStringLiteral(".json");
    QString err;
    if (SettingsStore::write(path, s_.toJson(), &err))
        logLine(QStringLiteral("saved preset '%1' to %2").arg(name, path));
    else
        logLine(QStringLiteral("error: cannot save preset: %1").arg(err));
}

// -- wiring -------------------------------------------------------------------------------

void MainWindow::wire()
{
    connect(&client_, &ControlClient::stateChanged, this, &MainWindow::onState);
    connect(&client_, &ControlClient::hello, this, &MainWindow::onHello);
    connect(&client_, &ControlClient::event, this, &MainWindow::onEvent);
    connect(btnApply_, &QPushButton::clicked, this, &MainWindow::apply);
    connect(btnSave_, &QPushButton::clicked, this, &MainWindow::save);

    // Every editor writes into the working copy and calls changed(); `syncing_` stops refresh()
    // from feeding back into these handlers.
    auto editD = [this](QDoubleSpinBox* sp, double* (*pick)(Settings&), const char* lockKey) {
        connect(sp, &QDoubleSpinBox::valueChanged, this, [this, pick, lockKey](double v) {
            if (syncing_ || !haveSettings_) return;
            *pick(s_) = v;
            changed(lockKey);
        });
    };
    auto editI = [this](QSpinBox* sp, int* (*pick)(Settings&)) {
        connect(sp, &QSpinBox::valueChanged, this, [this, pick](int v) {
            if (syncing_ || !haveSettings_) return;
            *pick(s_) = v;
            changed();
        });
    };
    editD(spDW_, [](Settings& s) { return &s.displayArea.width; }, nullptr);
    editD(spDH_, [](Settings& s) { return &s.displayArea.height; }, nullptr);
    editD(spDX_, [](Settings& s) { return &s.displayArea.x; }, nullptr);
    editD(spDY_, [](Settings& s) { return &s.displayArea.y; }, nullptr);
    editD(spTW_, [](Settings& s) { return &s.tabletArea.width; }, "width");
    editD(spTH_, [](Settings& s) { return &s.tabletArea.height; }, "height");
    editD(spTX_, [](Settings& s) { return &s.tabletArea.x; }, nullptr);
    editD(spTY_, [](Settings& s) { return &s.tabletArea.y; }, nullptr);
    editD(spMWmm_, [](Settings& s) { return &s.display.widthMm; }, nullptr);
    editD(spMHmm_, [](Settings& s) { return &s.display.heightMm; }, nullptr);
    editI(spMW_, [](Settings& s) { return &s.display.width; });
    editI(spMH_, [](Settings& s) { return &s.display.height; });
    connect(spRot_, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (syncing_ || !haveSettings_) return;
        double r = std::fmod(v, 360.0); if (r < 0) r += 360.0;
        s_.tabletArea.rotation = r;
        changed();
    });

    connect(dispCanvas_, &AreaCanvas::areaChanged, this, [this](const AreaCanvas::Area& a) {
        if (!haveSettings_) return;
        s_.displayArea = {a.width, a.height, a.x, a.y};
        changed();
    });
    connect(tabCanvas_, &AreaCanvas::areaChanged, this, [this](const AreaCanvas::Area& a) {
        if (!haveSettings_) return;
        s_.tabletArea = {a.width, a.height, a.x, a.y, a.rotation};
        changed();
    });

    auto flag = [this](QCheckBox* cb, bool Settings::*member) {
        connect(cb, &QCheckBox::toggled, this, [this, member](bool on) {
            if (syncing_ || !haveSettings_) return;
            s_.*member = on;
            changed();
        });
    };
    flag(ckLock_, &Settings::lockAspect);
    flag(ckClip_, &Settings::clip);
    flag(ckLimit_, &Settings::limit);

    // "Lock to usable area" is a GUI preference, remembered per canvas.
    QSettings prefs;
    dispCanvas_->setLockToUsable(prefs.value(QStringLiteral("lockUsableDisplay"), true).toBool());
    tabCanvas_->setLockToUsable(prefs.value(QStringLiteral("lockUsableTablet"), true).toBool());
    connect(dispCanvas_, &AreaCanvas::lockToUsableChanged, this, [this](bool on) {
        QSettings().setValue(QStringLiteral("lockUsableDisplay"), on); changed();
    });
    connect(tabCanvas_, &AreaCanvas::lockToUsableChanged, this, [this](bool on) {
        QSettings().setValue(QStringLiteral("lockUsableTablet"), on); changed();
    });
}

// -- model <-> widgets --------------------------------------------------------------------

void MainWindow::changed(const char* lockKey)
{
    if (!haveSettings_) return;
    if (s_.lockAspect && s_.displayArea.width > 0 && s_.displayArea.height > 0) {
        const double ratio = s_.displayArea.width / s_.displayArea.height;
        if (lockKey && std::string_view(lockKey) == "height")
            s_.tabletArea.width = std::round(s_.tabletArea.height * ratio * 100) / 100;
        else
            s_.tabletArea.height = std::round(s_.tabletArea.width / ratio * 100) / 100;
    }
    if (dispCanvas_->lockToUsable()) {
        const auto a = AreaCanvas::constrained(toArea(s_.displayArea), QSizeF(s_.display.width, s_.display.height), false, 0);
        s_.displayArea = {a.width, a.height, a.x, a.y};
    }
    if (tabCanvas_->lockToUsable()) {
        const auto a = AreaCanvas::constrained(toArea(s_.tabletArea), QSizeF(s_.tablet.width, s_.tablet.height), s_.lockAspect, 2);
        s_.tabletArea = {a.width, a.height, a.x, a.y, a.rotation};
    }
    refresh();
}

void MainWindow::refresh()
{
    if (!haveSettings_) return;
    syncing_ = true;
    const Settings& s = s_;
    dispCanvas_->setModel(QSizeF(s.display.width, s.display.height), toArea(s.displayArea));
    tabCanvas_->setModel(QSizeF(s.tablet.width, s.tablet.height), toArea(s.tabletArea));
    auto set = [](QDoubleSpinBox* sp, double v) { if (std::abs(sp->value() - v) > 1e-9) sp->setValue(v); };
    set(spDW_, s.displayArea.width); set(spDH_, s.displayArea.height); set(spDX_, s.displayArea.x); set(spDY_, s.displayArea.y);
    set(spTW_, s.tabletArea.width); set(spTH_, s.tabletArea.height); set(spTX_, s.tabletArea.x); set(spTY_, s.tabletArea.y);
    set(spRot_, s.tabletArea.rotation);
    ckLock_->setChecked(s.lockAspect); ckClip_->setChecked(s.clip); ckLimit_->setChecked(s.limit);
    spMW_->setValue(s.display.width); spMH_->setValue(s.display.height);
    set(spMWmm_, s.display.widthMm); set(spMHmm_, s.display.heightMm);
    syncing_ = false;
    updateStatus();
}

void MainWindow::updateStatus(const QString& extra)
{
    QStringList parts;
    if (!client_.isConnected()) {
        parts << QStringLiteral("daemon: not connected");
    } else {
        const QString tp = info_.value("touch_path").toString();
        parts << (tp.isEmpty() ? QStringLiteral("panel: not plugged in") : QStringLiteral("panel: %1").arg(tp));
        const QString vp = info_.value("tablet_path").toString();
        if (!vp.isEmpty()) parts << QStringLiteral("virtual tablet: %1").arg(vp);
        if (isDirty()) parts << QStringLiteral("unapplied changes");
    }
    const QJsonObject panel = info_.value("panel").toObject();
    panelName_->setText(panel.isEmpty() ? QStringLiteral("not plugged in") : panel.value("name").toString());
    glassSize_->setText(!haveSettings_ ? QString()
                        : QStringLiteral("%1 x %2 mm%3").arg(s_.tablet.width).arg(s_.tablet.height)
                              .arg(panel.isEmpty() ? QStringLiteral(" (last connected panel)") : QString()));
    if (!extra.isEmpty()) parts << extra;
    status_->setText(parts.join(QStringLiteral("  ·  ")));
    const bool on = client_.isConnected() && haveSettings_;
    btnApply_->setEnabled(on && isDirty());
    actApply_->setEnabled(on && isDirty());
    btnSave_->setEnabled(on);
    actSave_->setEnabled(on);
    actReset_->setEnabled(on);
}

void MainWindow::adopt(const QJsonObject& json, bool force)
{
    const Settings a = Settings::fromJson(json);
    const bool wasDirty = isDirty();
    applied_ = a;
    // The glass is the daemon's (it follows the panel); edits laid out on another glass are moot.
    if (force || !wasDirty || !haveSettings_ || s_.tablet != a.tablet) s_ = a;
    haveSettings_ = true;
    refresh();
}

void MainWindow::logLine(const QString& line)
{
    console_->appendPlainText(line);
    if (looksLikeError(line))
        tabs_->setCurrentIndex(tabs_->count() - 1);
}

// -- actions ------------------------------------------------------------------------------

void MainWindow::apply()
{
    if (!haveSettings_) return;
    client_.request({{"op", "apply"}, {"settings", s_.toJson()}}, [this](const QJsonObject& r) { onReply(r); });
}

void MainWindow::save()
{
    if (!haveSettings_) return;
    client_.request({{"op", "save"}, {"settings", s_.toJson()}}, [this](const QJsonObject& r) { onReply(r); });
}

void MainWindow::resetToDefaults()
{
    if (QMessageBox::question(this, QStringLiteral("Reset to defaults"), QStringLiteral("Reset settings to default?"))
        != QMessageBox::Yes)
        return;
    client_.request({{"op", "reset"}}, [this](const QJsonObject& r) { onReply(r); });
}

void MainWindow::detectMonitor()
{
    if (!haveSettings_) return;
    QScreen* scr = windowHandle() ? windowHandle()->screen() : nullptr;
    if (!scr) scr = QGuiApplication::primaryScreen();
    if (!scr) { logLine(QStringLiteral("error: no screen found")); return; }
    const QRect g = scr->geometry();
    const double dpr = scr->devicePixelRatio();
    s_.display.width = qRound(g.width() * dpr);
    s_.display.height = qRound(g.height() * dpr);
    const QSizeF mm = scr->physicalSize();
    if (mm.width() > 0 && mm.height() > 0) { s_.display.widthMm = mm.width(); s_.display.heightMm = mm.height(); }
    logLine(QStringLiteral("detected monitor %1: %2x%3 px, %4x%5 mm").arg(scr->name())
                .arg(s_.display.width).arg(s_.display.height).arg(mm.width(), 0, 'f', 0).arg(mm.height(), 0, 'f', 0));
    s_.displayArea = {double(s_.display.width), double(s_.display.height), s_.display.width / 2.0, s_.display.height / 2.0};
    changed();
}

void MainWindow::onReply(const QJsonObject& r)
{
    if (!r.value("ok").toBool()) {
        const QString err = r.value("error").toString();
        logLine(QStringLiteral("error: %1").arg(err));
        updateStatus(QStringLiteral("error: %1").arg(err));
        if (r.contains("settings")) adopt(r.value("settings").toObject(), false);
        return;
    }
    if (r.contains("settings"))
        adopt(r.value("settings").toObject(), true);
}

// -- daemon events ------------------------------------------------------------------------

void MainWindow::onState(bool connected, const QString& msg)
{
    banner_->setText(connected ? QString() : QStringLiteral("%1  —  is touch2tablet.service running?  (systemctl --user status touch2tablet)").arg(msg));
    banner_->setVisible(!connected);
    if (!connected) {
        dispCanvas_->setDot(std::nullopt);
        tabCanvas_->setDot(std::nullopt);
    }
    updateStatus();
}

void MainWindow::onHello(const QJsonObject& r)
{
    info_ = r.value("info").toObject();
    console_->clear();
    for (const auto& l : r.value("lines").toArray()) console_->appendPlainText(l.toString());
    const int proto = r.value("proto").toInt(0);
    if (proto != 1) logLine(QStringLiteral("error: daemon protocol %1, GUI expects 1").arg(proto));
    adopt(r.value("settings").toObject(), false);
}

void MainWindow::onEvent(const QJsonObject& m)
{
    const QString ev = m.value("ev").toString();
    if (ev == u"settings") {
        if (m.contains("info")) info_ = m.value("info").toObject();
        adopt(m.value("settings").toObject(), false);
    } else if (ev == u"device") {
        info_ = m.value("info").toObject();
        updateStatus();
    } else if (ev == u"log") {
        logLine(m.value("line").toString());
    } else if (ev == u"pos") {
        if (m.value("fingers").toInt() == 0) {
            dispCanvas_->setDot(std::nullopt);
            tabCanvas_->setDot(std::nullopt);
            updateStatus();
            return;
        }
        const bool inside = m.value("inside").toBool(true);
        const QJsonArray mm = m.value("mm").toArray(), out = m.value("out").toArray(), raw = m.value("raw").toArray();
        tabCanvas_->setDot(QPointF(mm.at(0).toDouble(), mm.at(1).toDouble()), inside);
        dispCanvas_->setDot(QPointF(out.at(0).toDouble(), out.at(1).toDouble()), inside);
        updateStatus(QStringLiteral("%1 finger(s)  raw %2,%3  →  %4,%5 px%6")
                         .arg(m.value("fingers").toInt()).arg(raw.at(0).toInt()).arg(raw.at(1).toInt())
                         .arg(out.at(0).toInt()).arg(out.at(1).toInt())
                         .arg(m.value("ignored").toBool() ? QStringLiteral("  [ignored]") : QString()));
    }
}

}  // namespace t2t
