// touch2tablet GUI main window: Output / Settings / Console tabs, Save and Apply.
// Holds a working copy of the settings; `applied_` mirrors what the daemon runs.
#pragma once

#include "AreaCanvas.h"
#include "ControlClient.h"
#include "t2t/Settings.h"

#include <QAction>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>

namespace t2t {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ControlClient& client, QWidget* parent = nullptr);

private:
    QWidget* buildOutputTab();
    QWidget* buildSettingsTab();
    QWidget* buildConsoleTab();
    void buildMenu();
    void wire();

    QString presetsDir() const;
    void rebuildPresetsMenu();
    void applyPreset(const QString& path);
    void saveAsPreset();

    bool isDirty() const { return haveSettings_ && !(s_ == applied_); }
    void changed(const char* lockKey = nullptr);   // after any edit: aspect lock, bounds, widgets
    void refresh();                                // working copy -> widgets
    void updateStatus(const QString& extra = QString());
    void adopt(const QJsonObject& settingsJson, bool force);
    void logLine(const QString& line);

    void apply();
    void save();
    void resetToDefaults();
    void detectMonitor();

    void onState(bool connected, const QString& msg);
    void onHello(const QJsonObject& r);
    void onEvent(const QJsonObject& ev);
    void onReply(const QJsonObject& r);

    ControlClient& client_;
    Settings s_, applied_;
    bool haveSettings_ = false;
    bool syncing_ = false;
    QJsonObject info_;

    QAction *actApply_ = nullptr, *actSave_ = nullptr, *actReset_ = nullptr;
    QMenu* presetsMenu_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    AreaCanvas* dispCanvas_ = nullptr;
    AreaCanvas* tabCanvas_ = nullptr;
    QDoubleSpinBox *spDW_ = nullptr, *spDH_ = nullptr, *spDX_ = nullptr, *spDY_ = nullptr;
    QDoubleSpinBox *spTW_ = nullptr, *spTH_ = nullptr, *spTX_ = nullptr, *spTY_ = nullptr, *spRot_ = nullptr;
    QCheckBox *ckLock_ = nullptr, *ckClip_ = nullptr, *ckLimit_ = nullptr;
    QLabel *panelName_ = nullptr, *glassSize_ = nullptr;
    QSpinBox *spMW_ = nullptr, *spMH_ = nullptr;
    QDoubleSpinBox *spMWmm_ = nullptr, *spMHmm_ = nullptr;
    QPlainTextEdit* console_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* banner_ = nullptr;
    QPushButton *btnApply_ = nullptr, *btnSave_ = nullptr;
};

}  // namespace t2t
