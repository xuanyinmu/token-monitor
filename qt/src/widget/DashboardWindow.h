#pragma once

#include <QObject>
#include <QQmlApplicationEngine>

namespace tmon {

class AppState;

class DashboardWindow : public QObject {
    Q_OBJECT
public:
    explicit DashboardWindow(AppState *state, QObject *parent = nullptr);
    Q_INVOKABLE void show();
    Q_INVOKABLE void hide();

private:
    AppState *m_state = nullptr;
    QQmlApplicationEngine m_engine;
    bool m_loaded = false;
};

} // namespace tmon
