#pragma once

#include "core/device/DeviceRuntime.h"
#include "core/i18n/I18n.h"
#include "core/io/SettingsStore.h"
#include "widget/Hotkey.h"
#include "widget/RowListModel.h"
#include "widget/TrayController.h"
#include "widget/WidgetWindow.h"

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class QTimer;

namespace tmon {

class AppState : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(QString period READ period WRITE setPeriod NOTIFY periodChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QString totalText READ totalText NOTIFY statsChanged)
    Q_PROPERTY(QString compactText READ compactText NOTIFY statsChanged)
    Q_PROPERTY(QString costText READ costText NOTIFY statsChanged)
    Q_PROPERTY(bool live READ live NOTIFY statusChanged)
    Q_PROPERTY(QVariantList clientRows READ clientRows NOTIFY statsChanged)
    Q_PROPERTY(QVariantList modelRows READ modelRows NOTIFY statsChanged)
    Q_PROPERTY(QVariantList projectRows READ projectRows NOTIFY statsChanged)
    Q_PROPERTY(QVariantList sessionRows READ sessionRows NOTIFY statsChanged)
    Q_PROPERTY(QVariantList deviceRows READ deviceRows NOTIFY statsChanged)
    Q_PROPERTY(QVariantList limitRows READ limitRows NOTIFY statsChanged)
    Q_PROPERTY(QVariantList historyDays READ historyDays NOTIFY statsChanged)
    Q_PROPERTY(QVariantList trendPoints READ trendPoints NOTIFY statsChanged)
    Q_PROPERTY(QVariantList subscriptions READ subscriptions NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList serviceStatusRows READ serviceStatusRows NOTIFY serviceStatusChanged)
    Q_PROPERTY(RowListModel *clientModel READ clientModel CONSTANT)
    Q_PROPERTY(RowListModel *modelModel READ modelModel CONSTANT)
    Q_PROPERTY(RowListModel *projectModel READ projectModel CONSTANT)
    Q_PROPERTY(RowListModel *sessionModel READ sessionModel CONSTANT)
    Q_PROPERTY(RowListModel *deviceModel READ deviceModel CONSTANT)
    Q_PROPERTY(RowListModel *limitModel READ limitModel CONSTANT)
    Q_PROPERTY(QVariantMap settings READ settingsMap NOTIFY settingsChanged)
    Q_PROPERTY(bool titleIconOnly READ titleIconOnly NOTIFY settingsChanged)
    Q_PROPERTY(bool showLiveTokenRate READ showLiveTokenRate NOTIFY settingsChanged)
    Q_PROPERTY(bool appUpdateReady READ appUpdateReady NOTIFY appUpdateChanged)
    Q_PROPERTY(QString appUpdateLabel READ appUpdateLabel NOTIFY appUpdateChanged)
    Q_PROPERTY(QString appUpdateUrl READ appUpdateUrl NOTIFY appUpdateChanged)
    Q_PROPERTY(QString appUpdateLatest READ appUpdateLatest NOTIFY appUpdateChanged)
    Q_PROPERTY(QVariantMap dashboardSummary READ dashboardSummary NOTIFY statsChanged)
    Q_PROPERTY(QVariantList dashboardToolRows READ dashboardToolRows NOTIFY statsChanged)
    Q_PROPERTY(QVariantList dashboardModelRows READ dashboardModelRows NOTIFY statsChanged)
    Q_PROPERTY(bool fixedPeriodActive READ fixedPeriodActive NOTIFY periodChanged)
    Q_PROPERTY(bool fixedPeriodReady READ fixedPeriodReady NOTIFY statsChanged)
    Q_PROPERTY(QStringList fixedPeriodRange READ fixedPeriodRange NOTIFY statsChanged)
    Q_PROPERTY(QString periodTabLabel READ periodTabLabel NOTIFY periodChanged)
    Q_PROPERTY(QVariantMap theme READ theme NOTIFY settingsChanged)
    Q_PROPERTY(I18n *i18n READ i18n CONSTANT)
    Q_PROPERTY(bool bubbleCollapsed READ bubbleCollapsed WRITE setBubbleCollapsed NOTIFY bubbleChanged)
    Q_PROPERTY(bool floatingBubble READ floatingBubble NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap sessionDetail READ sessionDetail NOTIFY sessionDetailChanged)
    Q_PROPERTY(bool sessionDetailOpen READ sessionDetailOpen NOTIFY sessionDetailChanged)
    Q_PROPERTY(bool settingsOpen READ settingsOpen WRITE setSettingsOpen NOTIFY settingsOpenChanged)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
    Q_PROPERTY(bool chromeHover READ chromeHover WRITE setChromeHover NOTIFY chromeHoverChanged)
    Q_PROPERTY(bool utilityHover READ utilityHover WRITE setUtilityHover NOTIFY utilityHoverChanged)
    Q_PROPERTY(bool statusVisible READ statusVisible NOTIFY statusChanged)
    Q_PROPERTY(int periodIndex READ periodIndex NOTIFY periodChanged)
    Q_PROPERTY(QString viewLabel READ viewLabel NOTIFY viewChanged)
    Q_PROPERTY(QString viewIcon READ viewIcon NOTIFY viewChanged)
    Q_PROPERTY(bool homeReturnVisible READ homeReturnVisible NOTIFY viewChanged)
    Q_PROPERTY(QStringList views READ views NOTIFY settingsChanged)
    Q_PROPERTY(QStringList homeModules READ homeModules NOTIFY settingsChanged)
public:
    explicit AppState(QObject *parent = nullptr);
    void attachWindow(QWindow *window);
    void setPreviewOnly(bool on);
    QString view() const { return m_view; }
    QString period() const { return m_period; }
    QString statusText() const { return m_status; }
    QString totalText() const;
    QString compactText() const;
    QString costText() const;
    bool live() const { return !statusVisible(); }
    QVariantList clientRows() const { return m_clientRows; }
    QVariantList modelRows() const { return m_modelRows; }
    QVariantList projectRows() const { return m_projectRows; }
    QVariantList sessionRows() const { return m_sessionRows; }
    QVariantList deviceRows() const { return m_deviceRows; }
    QVariantList limitRows() const { return m_limitRows; }
    QVariantList historyDays() const { return m_historyDays; }
    QVariantList trendPoints() const { return m_trendPoints; }
    QVariantList serviceStatusRows() const { return m_serviceStatusRows; }
    QVariantList subscriptions() const;
    RowListModel *clientModel() { return &m_clientModel; }
    RowListModel *modelModel() { return &m_modelModel; }
    RowListModel *projectModel() { return &m_projectModel; }
    RowListModel *sessionModel() { return &m_sessionModel; }
    RowListModel *deviceModel() { return &m_deviceModel; }
    RowListModel *limitModel() { return &m_limitModel; }
    QVariantMap settingsMap() const { return m_settingsUi; }
    bool titleIconOnly() const;
    bool showLiveTokenRate() const;
    QVariantMap theme() const { return m_theme; }
    I18n *i18n() { return &m_i18n; }
    bool bubbleCollapsed() const { return m_bubbleCollapsed; }
    bool floatingBubble() const;
    QVariantMap sessionDetail() const { return m_sessionDetail; }
    bool sessionDetailOpen() const { return m_sessionDetailOpen; }
    QStringList homeModules() const;
    bool settingsOpen() const { return m_settingsOpen; }
    bool refreshing() const { return m_refreshing; }
    bool homeReturnVisible() const { return m_homeReturnVisible; }
    bool chromeHover() const { return m_chromeHover; }
    bool utilityHover() const { return m_utilityHover; }
    bool statusVisible() const;
    int periodIndex() const;
    QString viewLabel() const;
    QString viewIcon() const;
    QStringList views() const;
    Q_INVOKABLE QStringList allViews() const;
    Q_INVOKABLE bool viewIsHidden(const QString &id) const;
    Q_INVOKABLE void toggleViewHidden(const QString &id);
    Q_INVOKABLE void moveViewDisplay(const QString &id, int direction);
    Q_INVOKABLE void resetViewDisplayOrder();
    Q_INVOKABLE void showAllViews();
    Q_INVOKABLE int visibleViewCount() const { return views().size(); }
    Q_INVOKABLE int totalViewCount() const { return 9; }

    Q_INVOKABLE void setSettingsOpen(bool on);
    Q_INVOKABLE void toggleSettings();
    Q_INVOKABLE void setChromeHover(bool on);
    Q_INVOKABLE void setUtilityHover(bool on);
    Q_INVOKABLE void setView(const QString &view);
    Q_INVOKABLE void setViewFromHome(const QString &view);
    Q_INVOKABLE void cycleView();
    Q_INVOKABLE void forceView(const QString &view);
    Q_INVOKABLE void setPeriod(const QString &period);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void refreshServiceStatus();
    Q_INVOKABLE void updateSetting(const QString &key, const QVariant &value);
    Q_INVOKABLE void cycleBehavior();
    Q_INVOKABLE void exportNow();
    Q_INVOKABLE void exportDiagnostics();
    Q_INVOKABLE void pickExportDir();
    Q_INVOKABLE void checkUpdates();
    Q_INVOKABLE void setBubbleCollapsed(bool on);
    Q_INVOKABLE QString formatTokens(double tokens) const;
    Q_INVOKABLE QString formatNumber(double tokens) const;
    Q_INVOKABLE QString formatUsd(double value) const;
    Q_INVOKABLE QString formatDuration(double ms) const;
    Q_INVOKABLE void openUpdate();
    Q_INVOKABLE void dismissUpdate();
    Q_INVOKABLE QString clientLabelOf(const QString &id) const;
    Q_INVOKABLE QString iconUrl(const QString &id) const;
    Q_INVOKABLE QString uiIcon(const QString &rel) const;
    Q_INVOKABLE QString viewLabelFor(const QString &id) const;
    Q_INVOKABLE QString viewIconFor(const QString &id) const;
    Q_INVOKABLE QVariantMap clientHealthCounts() const;
    Q_INVOKABLE void saveScreenshot(const QString &path);
    // Drives the top-edge dock/reveal state machine without user input, so the
    // packaging smoke test can cover the path that used to crash the process
    // (see WidgetWindow::m_yAnimation). Prints top-edge-ok and exits 0; the
    // watchdog prints top-edge-timeout and exits 1.
    void selfTestTopEdge();
    Q_INVOKABLE void startAtLogin(bool on);
    Q_INVOKABLE void startMove();
    Q_INVOKABLE void startResize(const QString &edge);
    Q_INVOKABLE void persistWindowSize(int w, int h);
    Q_INVOKABLE QVariantList catalogClientRows() const;
    Q_INVOKABLE void toggleClientTracked(const QString &id);
    Q_INVOKABLE void toggleClientHidden(const QString &id);
    Q_INVOKABLE QVariantList catalogLimitRows() const;
    Q_INVOKABLE void toggleLimitProvider(const QString &id);
    Q_INVOKABLE void showAllClients();
    Q_INVOKABLE void resetClientDisplayOrder();
    Q_INVOKABLE void minimizeWindow();
    Q_INVOKABLE void closeWindow();
    Q_INVOKABLE void hideWindow();
    Q_INVOKABLE void showWindow();
    Q_INVOKABLE void openSession(const QString &id);
    Q_INVOKABLE void closeSession();
    Q_INVOKABLE void addSubscription(const QString &name, double amount, const QString &period);
    Q_INVOKABLE void removeSubscription(int index);
    Q_INVOKABLE void showDashboard();
    bool appUpdateReady() const { return m_appUpdateReady; }
    QString appUpdateLabel() const { return m_appUpdateLabel; }
    // "↑ v1.2.3" → "v1.2.3"; empty until a check answers.
    QString appUpdateLatest() const { return m_appUpdateReady && m_appUpdateLabel.startsWith(QStringLiteral("↑ ")) ? m_appUpdateLabel.mid(2) : QString(); }
    QString appUpdateUrl() const { return m_appUpdateUrl; }
    QVariantMap dashboardSummary() const { return m_dashboardSummary; }
    QVariantList dashboardToolRows() const { return m_dashboardToolRows; }
    QVariantList dashboardModelRows() const { return m_dashboardModelRows; }
    // Fixed headline ranges (本周 / 最近 7 天 / 最近 30 天) are derived from the
    // daily history rather than scanned; mirror Electron fixedPeriodRanges.
    bool fixedPeriodActive() const;
    bool fixedPeriodReady() const { return m_derivedReady; }
    QStringList fixedPeriodRange() const;
    QString periodTabLabel() const;
    WidgetWindow *windowChrome() { return m_window; }
    TrayController *tray() { return &m_tray; }
    Hotkey *hotkey() { return &m_hotkey; }

signals:
    void settingsOpenChanged();
    void refreshingChanged();
    void chromeHoverChanged();
    void utilityHoverChanged();
    void viewChanged();
    void periodChanged();
    void statusChanged();
    void statsChanged();
    void settingsChanged();
    void bubbleChanged();
    void sessionDetailChanged();
    void serviceStatusChanged();
    void updateAvailable(const QString &version, const QString &url);
    void appUpdateChanged();
    void dashboardRequested();

private:
    void rebuildRows();
    void rebuildDerivedPeriod();
    void navigate(const QString &view, bool fromHome);
    void rebuildTrayMenu();
    void applyTheme();
    void persist();
    void ensureCurrencyRate();
    void applyTopEdge();
    void seedServiceStatusPlaceholders();
    void refreshHealthCounts();
    void rebuildDashboard();
    QJsonObject currentPeriod() const;
    QJsonObject periodNamed(const QString &name) const;
    DeviceRuntime m_runtime;
    I18n m_i18n;
    WidgetWindow *m_window = nullptr;
    TrayController m_tray;
    Hotkey m_hotkey;
    RowListModel m_clientModel;
    RowListModel m_modelModel;
    RowListModel m_projectModel;
    RowListModel m_sessionModel;
    RowListModel m_deviceModel;
    RowListModel m_limitModel;
    QJsonObject m_settings;
    QVariantMap m_settingsUi;
    QVariantMap m_theme;
    QVariantMap m_sessionDetail;
    QVariantList m_clientRows, m_modelRows, m_projectRows, m_sessionRows, m_deviceRows, m_limitRows, m_historyDays, m_trendPoints, m_serviceStatusRows;
    QVariantList m_dashboardToolRows, m_dashboardModelRows;
    QVariantMap m_dashboardSummary;
    QVariantMap m_healthCounts;
    QJsonObject m_derivedPeriod;
    QStringList m_derivedRange;
    bool m_derivedReady = false;
    QTimer *m_exportTimer = nullptr;
    qint64 m_lastExportAt = 0;
    QString m_view = QStringLiteral("tool");
    QString m_period = QStringLiteral("today");
    QString m_status = QStringLiteral("Starting");
    bool m_bubbleCollapsed = false;
    bool m_sessionDetailOpen = false;
    bool m_currencyFetched = false;
    bool m_homeReturnVisible = false;
    bool m_settingsOpen = false;
    bool m_refreshing = false;
    bool m_chromeHover = false;
    bool m_utilityHover = false;
    bool m_previewOnly = false;
    bool m_statusBusy = false;
    bool m_appUpdateReady = false;
    QString m_appUpdateLabel;
    QString m_appUpdateUrl;
};

} // namespace tmon
