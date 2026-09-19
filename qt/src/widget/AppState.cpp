#include "widget/AppState.h"

#include "core/catalog/Catalog.h"
#include "core/catalog/ClientRoots.h"
#include "core/export/Exporter.h"
#include "core/hub/HubClient.h"
#include "core/io/CredentialStore.h"
#include "core/io/Paths.h"
#include "core/net/HttpClient.h"
#include "core/tmon.h"
#include "core/usage/JsonUtil.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QThread>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QPointer>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QtConcurrent>
#include <QTimer>
#include <QUrl>
#include <QVector>
#include <QWindow>
#include <algorithm>
#include <cmath>

namespace tmon {

bool jsonFlag(const QJsonValue &value, bool fallback);

namespace {

QVariantMap rowOf(const QString &id, const QString &label, double tokens, double cost, double total)
{
    return QVariantMap{
        {QStringLiteral("id"), id},
        {QStringLiteral("label"), label},
        {QStringLiteral("tokens"), tokens},
        {QStringLiteral("cost"), cost},
        {QStringLiteral("percent"), total > 0 ? tokens / total : 0.0},
        {QStringLiteral("mark"), label.left(1).toUpper()}
    };
}

QString markColor(const QString &id)
{
    static const QHash<QString, QString> colors{
        {QStringLiteral("cursor"), QStringLiteral("#000000")},
        {QStringLiteral("claude"), QStringLiteral("#cc7c5e")},
        {QStringLiteral("codex"), QStringLiteral("#49a3b0")},
        {QStringLiteral("zai"), QStringLiteral("#111111")},
        {QStringLiteral("deepseek"), QStringLiteral("#4d6bfe")},
        {QStringLiteral("grok"), QStringLiteral("#e8e8e8")}
    };
    return colors.value(id, QStringLiteral("#73bdf5"));
}

QString cssFirstFamily(const QString &list)
{
    const auto parts = list.split(QLatin1Char(','));
    for (auto part : parts) {
        auto s = part.trimmed();
        if (s.startsWith(QLatin1Char('"')) || s.startsWith(QLatin1Char('\'')))
            s = s.mid(1, qMax(0, s.size() - 2)).trimmed();
        if (s.isEmpty()) continue;
        if (s.startsWith(QLatin1String("-apple"))
            || s == QLatin1String("BlinkMacSystemFont")
            || s == QLatin1String("ui-monospace")
            || s == QLatin1String("system-ui")
            || s == QLatin1String("sans-serif")
            || s == QLatin1String("monospace"))
            continue;
        return s;
    }
    return {};
}

QString pickInstalledFont(const QStringList &candidates, const QString &fallback)
{
    const auto families = QFontDatabase::families();
    for (const auto &name : candidates) {
        if (families.contains(name)) return name;
    }
    return fallback;
}

QString formatUsd(double value)
{
    const int digits = std::abs(value) >= 10.0 ? 2 : 4;
    return QLatin1Char('$') + QString::number(value, 'f', digits);
}

QString formatDurationMs(qint64 ms)
{
    const qint64 totalMinutes = qMax(qint64(0), qRound(ms / 60000.0));
    const qint64 days = totalMinutes / 1440;
    const qint64 hours = (totalMinutes % 1440) / 60;
    const qint64 minutes = totalMinutes % 60;
    if (days > 0) return QStringLiteral("%1d %2h").arg(days).arg(hours);
    if (hours > 0) return QStringLiteral("%1h %2m").arg(hours).arg(minutes);
    if (minutes > 0) return QStringLiteral("%1m").arg(minutes);
    return QStringLiteral("<1m");
}

QString formatResetText(const QString &resetsAt)
{
    if (resetsAt.isEmpty()) return {};
    auto at = QDateTime::fromString(resetsAt, Qt::ISODateWithMs);
    if (!at.isValid()) at = QDateTime::fromString(resetsAt, Qt::ISODate);
    if (!at.isValid()) return {};
    const auto diff = QDateTime::currentDateTimeUtc().msecsTo(at.toUTC());
    if (diff < 0) return QStringLiteral("Reset now");
    return QStringLiteral("Reset ") + formatDurationMs(diff);
}

QString formatUpdatedText(const QString &updatedAt)
{
    if (updatedAt.isEmpty()) return {};
    auto at = QDateTime::fromString(updatedAt, Qt::ISODateWithMs);
    if (!at.isValid()) at = QDateTime::fromString(updatedAt, Qt::ISODate);
    if (!at.isValid()) return QStringLiteral("Update unknown");
    const qint64 diffMs = qMax(qint64(0), at.toUTC().msecsTo(QDateTime::currentDateTimeUtc()));
    if (diffMs < 45'000) return QStringLiteral("Updated just now");
    const qint64 minutes = qRound(diffMs / 60000.0);
    if (minutes < 60) return QStringLiteral("Updated %1m ago").arg(minutes);
    const qint64 hours = qRound(minutes / 60.0);
    if (hours < 24) return QStringLiteral("Updated %1h ago").arg(hours);
    return QStringLiteral("Updated %1d ago").arg(qRound(hours / 24.0));
}

int windowKindPriority(const QString &kind)
{
    if (kind == QLatin1String("session")) return 0;
    if (kind == QLatin1String("daily")) return 1;
    if (kind == QLatin1String("weekly")) return 2;
    if (kind == QLatin1String("billing")) return 3;
    if (kind == QLatin1String("monthly")) return 4;
    return 10;
}

QString modelVendorFor(const QString &model)
{
    const auto name = model.toLower();
    auto matches = [&](const char *pattern) {
        return QRegularExpression(QString::fromUtf8(pattern)).match(name).hasMatch();
    };
    if (matches("^(cursor-)?auto$")) return QStringLiteral("cursor");
    if (matches("claude|anthropic|sonnet|opus|haiku")) return QStringLiteral("claude");
    if (matches("gpt|openai|codex|^o[134](?:-|$)|o[134]-(mini|pro|preview)|chatgpt")) return QStringLiteral("codex");
    if (matches("gemini|gemma|google")) return QStringLiteral("gemini");
    if (matches("grok|xai")) return QStringLiteral("xai");
    if (matches("deepseek")) return QStringLiteral("deepseek");
    if (matches("llama|meta|muse-spark(?:-|$)")) return QStringLiteral("meta");
    if (matches("mistral|mixtral|codestral")) return QStringLiteral("mistral");
    if (matches("qwen|qwq|qvq")) return QStringLiteral("qwen");
    if (matches("kimi|moonshot|k2d6-agent|k3-agent")) return QStringLiteral("kimi");
    if (matches("chatglm|\\bglm-|\\bzai\\b|z\\.ai|zhipu")) return QStringLiteral("zai");
    if (matches("cohere|command-r")) return QStringLiteral("cohere");
    if (matches("mimo|xiaomi")) return QStringLiteral("xiaomi");
    if (matches("minimax|\\babab")) return QStringLiteral("minimax");
    if (matches("doubao|\\bseed(?:-|$)")) return QStringLiteral("doubao");
    if (matches("hy\\d|hunyuan")) return QStringLiteral("hunyuan");
    if (matches("^big-pickle$")) return QStringLiteral("opencode");
    return {};
}

QString homeWindowLabel(const QString &kind, const QString &label, const I18n &i18n)
{
    if (kind == QLatin1String("billing") && !label.isEmpty())
        return label;
    static const QHash<QString, QString> keys{
        {QStringLiteral("session"), QStringLiteral("home.limit.session")},
        {QStringLiteral("daily"), QStringLiteral("home.limit.daily")},
        {QStringLiteral("weekly"), QStringLiteral("home.limit.weekly")},
        {QStringLiteral("billing"), QStringLiteral("home.limit.billing")},
        {QStringLiteral("monthly"), QStringLiteral("home.limit.monthly")}
    };
    const auto key = keys.value(kind);
    if (!key.isEmpty()) {
        const auto translated = i18n.t(key);
        if (!translated.isEmpty() && translated != key)
            return translated;
        if (kind == QLatin1String("weekly")) return QStringLiteral("Weekly");
        if (kind == QLatin1String("session")) return QStringLiteral("Session");
        if (kind == QLatin1String("daily")) return QStringLiteral("Daily");
        if (kind == QLatin1String("monthly")) return QStringLiteral("Monthly");
        if (kind == QLatin1String("billing")) return QStringLiteral("Billing");
    }
    return label;
}

QString compactSessionTime(const QString &iso)
{
    auto at = QDateTime::fromString(iso, Qt::ISODateWithMs);
    if (!at.isValid()) at = QDateTime::fromString(iso, Qt::ISODate);
    if (!at.isValid()) return {};
    at = at.toLocalTime();
    const auto t = at.time().toString(QStringLiteral("HH:mm"));
    if (at.date() == QDate::currentDate()) return t;
    return QStringLiteral("%1/%2 %3")
        .arg(at.date().month(), 2, 10, QLatin1Char('0'))
        .arg(at.date().day(), 2, 10, QLatin1Char('0'))
        .arg(t);
}

QString sessionModelLabel(const QJsonObject &session)
{
    const auto models = session.value(QStringLiteral("models")).toObject();
    QStringList names;
    for (auto it = models.begin(); it != models.end(); ++it) {
        if (asNumber(it.value()) > 0) names.append(it.key());
    }
    names.sort();
    if (names.isEmpty()) return {};
    if (names.size() == 1) return names[0];
    return QString::number(names.size()) + QStringLiteral(" models");
}

QString sessionIdLabel(const QString &id)
{
    auto raw = id.trimmed();
    if (raw.isEmpty()) return {};
    if (raw.startsWith(QLatin1String("reasonix:"), Qt::CaseInsensitive)) {
        raw = raw.mid(raw.indexOf(QLatin1Char(':')) + 1);
        if (raw.startsWith(QLatin1String("reasonix-stats:"), Qt::CaseInsensitive)) return {};
        return raw;
    }
    if (raw.startsWith(QLatin1String("reasonix-stats:"), Qt::CaseInsensitive)) return {};
    static const QRegularExpression uuidRe(
        QStringLiteral("[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}"),
        QRegularExpression::CaseInsensitiveOption);
    QStringList uuids;
    auto it = uuidRe.globalMatch(raw);
    while (it.hasNext()) uuids.append(it.next().captured());
    if (uuids.size() > 1) return uuids.join(QStringLiteral(" · "));
    static const QRegularExpression rolloutRe(
        QStringLiteral("^rollout-\\d{4}-\\d{2}-\\d{2}T\\d{2}[:-]\\d{2}[:-]\\d{2}-(.+)$"));
    const auto rollout = rolloutRe.match(raw);
    if (rollout.hasMatch()) return uuids.isEmpty() ? rollout.captured(1) : uuids[0];
    static const QRegularExpression isoRe(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}T\\d{2}[:-]\\d{2}"));
    if (isoRe.match(raw).hasMatch()) return {};
    return raw;
}

qint64 sessionTimestampMs(const QJsonObject &session)
{
    auto iso = session.value(QStringLiteral("lastUsedAt")).toString();
    if (iso.isEmpty()) iso = session.value(QStringLiteral("startedAt")).toString();
    auto at = QDateTime::fromString(iso, Qt::ISODateWithMs);
    if (!at.isValid()) at = QDateTime::fromString(iso, Qt::ISODate);
    return at.isValid() ? at.toMSecsSinceEpoch() : 0;
}

QString groupedInt(qint64 value)
{
    return QLocale(QLocale::English).toString(value);
}

QString devicePeriodKey(const QString &period)
{
    if (period == QLatin1String("today") || period == QLatin1String("month") || period == QLatin1String("allTime"))
        return period;
    return QStringLiteral("month");
}

// Fixed headline selections (本周/最近 7 天/最近 30 天) are derived from the
// daily history, mirroring Electron fixedPeriodRanges.isDerived().
bool isDerivedPeriod(const QString &period)
{
    return period == QLatin1String("week") || period == QLatin1String("last7")
        || period == QLatin1String("last30");
}

QJsonObject devicePeriodObject(const QJsonObject &device, const QString &period)
{
    const auto key = devicePeriodKey(period);
    auto direct = device.value(key).toObject();
    if (!direct.isEmpty()) return direct;
    return device.value(QStringLiteral("periods")).toObject().value(key).toObject();
}

QVariantMap fetchOneServiceStatus(const char *id, const char *label, const char *page, const char *url, const char *icon,
                                  const QString &ua, const QString &checkedAt)
{
    QVariantMap row{
        {QStringLiteral("id"), QString::fromUtf8(id)},
        {QStringLiteral("label"), QString::fromUtf8(label)},
        {QStringLiteral("pageUrl"), QString::fromUtf8(page)},
        {QStringLiteral("iconId"), QString::fromUtf8(icon)},
        {QStringLiteral("checkedAt"), checkedAt}
    };
    HttpClient http;
    const auto result = http.get(QUrl(QString::fromUtf8(url)), {{QStringLiteral("User-Agent"), ua}}, 5000);
    const auto obj = result.json().object();
    if (result.status != 200 || obj.isEmpty()) {
        row.insert(QStringLiteral("status"), QStringLiteral("unknown"));
        row.insert(QStringLiteral("description"), QStringLiteral("Unable to check status"));
        row.insert(QStringLiteral("error"), result.error);
        return row;
    }
    const auto st = obj.value(QStringLiteral("status")).toObject();
    const auto indicator = st.value(QStringLiteral("indicator")).toString().trimmed().toLower();
    QString tone = QStringLiteral("unknown");
    if (indicator == QLatin1String("none")) tone = QStringLiteral("ok");
    else if (indicator == QLatin1String("minor")) tone = QStringLiteral("degraded");
    else if (indicator == QLatin1String("major") || indicator == QLatin1String("critical"))
        tone = QStringLiteral("outage");
    row.insert(QStringLiteral("status"), tone);
    row.insert(QStringLiteral("description"), st.value(QStringLiteral("description")).toString().trimmed());
    QString incidentTitle;
    int incidentCount = 0;
    for (const auto &iv : obj.value(QStringLiteral("incidents")).toArray()) {
        const auto status = iv.toObject().value(QStringLiteral("status")).toString().toLower();
        if (status == QLatin1String("resolved") || status == QLatin1String("completed")
            || status == QLatin1String("postmortem"))
            continue;
        if (incidentTitle.isEmpty()) incidentTitle = iv.toObject().value(QStringLiteral("name")).toString();
        ++incidentCount;
    }
    int maint = 0;
    for (const auto &mv : obj.value(QStringLiteral("scheduled_maintenances")).toArray()) {
        const auto status = mv.toObject().value(QStringLiteral("status")).toString().toLower();
        if (status == QLatin1String("completed") || status == QLatin1String("canceled")) continue;
        ++maint;
    }
    row.insert(QStringLiteral("incidentTitle"), incidentTitle);
    row.insert(QStringLiteral("incidentCount"), incidentCount);
    row.insert(QStringLiteral("maintenanceCount"), maint);
    return row;
}

QVariantList fetchServiceStatusPayloads()
{
    const auto ua = QStringLiteral("TokenMonitor/%1 (+https://github.com/Javis603/token-monitor)")
                        .arg(QLatin1String(kAppVersion));
    const auto checkedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    struct Spec { const char *id; const char *label; const char *page; const char *url; const char *icon; };
    const Spec specs[] = {
        {"claude", "Claude", "https://status.claude.com", "https://status.claude.com/api/v2/summary.json", "claude"},
        {"openai", "OpenAI", "https://status.openai.com", "https://status.openai.com/api/v2/summary.json", "codex"},
        {"cursor", "Cursor", "https://status.cursor.com", "https://status.cursor.com/api/v2/summary.json", "cursor"},
        {"deepseek", "DeepSeek", "https://status.deepseek.com", "https://deepseek.statuspage.io/api/v2/summary.json", "deepseek"},
    };
    QVariantMap rows[4];
    QVector<QThread *> threads;
    for (int i = 0; i < 4; ++i) {
        const auto spec = specs[i];
        auto *thread = QThread::create([spec, ua, checkedAt, &slot = rows[i]]() {
            slot = fetchOneServiceStatus(spec.id, spec.label, spec.page, spec.url, spec.icon, ua, checkedAt);
        });
        threads.append(thread);
        thread->start();
    }
    for (auto *thread : threads) {
        thread->wait();
        thread->deleteLater();
    }
    QVariantList out;
    for (const auto &row : rows) out.append(row);
    return out;
}

} // namespace

AppState::AppState(QObject *parent)
    : QObject(parent)
{
    m_settings = CredentialStore::overlayOnto(SettingsStore::load());
    m_i18n.setLanguage(m_settings.value(QStringLiteral("language")).toString(QStringLiteral("auto")));
    m_period = m_settings.value(QStringLiteral("lastViewState")).toObject().value(QStringLiteral("period")).toString(QStringLiteral("today"));
    m_view = m_settings.value(QStringLiteral("lastViewState")).toObject().value(QStringLiteral("breakdown")).toString(QStringLiteral("tool"));
    if (m_view == QLatin1String("settings") || m_view.isEmpty())
        m_view = QStringLiteral("tool");
    applyTheme();
    m_settingsUi = CredentialStore::redactedForUi(m_settings);
    seedServiceStatusPlaceholders();
    connect(&m_runtime, &DeviceRuntime::updated, this, &AppState::rebuildRows);
    connect(&m_runtime, &DeviceRuntime::statusChanged, this, [this](const QString &text) {
        m_status = text;
        emit statusChanged();
    });
    connect(&m_i18n, &I18n::languageChanged, this, [this]() {
        rebuildTrayMenu();
        emit viewChanged();
        emit settingsChanged();
    });
    m_runtime.configure(m_settings);
    rebuildRows();
    rebuildTrayMenu();
    connect(&m_tray, &TrayController::activated, this, [this]() {
        if (m_window) m_window->showWindow();
    });
    connect(&m_tray, &TrayController::showHome, this, [this]() { setView(QStringLiteral("home")); });
    connect(&m_tray, &TrayController::showLimits, this, [this]() { setView(QStringLiteral("limits")); });
    connect(&m_tray, &TrayController::showSettings, this, [this]() { setSettingsOpen(true); });
    connect(&m_tray, &TrayController::quitRequested, qApp, &QCoreApplication::quit);
    connect(&m_tray, &TrayController::refreshRequested, this, &AppState::refresh);
    connect(&m_tray, &TrayController::openViewRequested, this, [this](const QString &view) {
        if (m_window) m_window->showWindow();
        forceView(view);
    });
    connect(&m_tray, &TrayController::trayContentRequested, this, [this](const QString &content) {
        updateSetting(QStringLiteral("trayContent"), content);
    });
    connect(&m_tray, &TrayController::presentationRequested, this, [this](const QString &presentation) {
        if (presentation == QLatin1String("tray")) {
            // Electron tray presentation parks the widget in the tray; the
            // window returns on the next tray activation.
            updateSetting(QStringLiteral("trayMode"), true);
            if (m_window) m_window->hideWindow();
            return;
        }
        updateSetting(QStringLiteral("trayMode"), false);
        updateSetting(QStringLiteral("windowBehavior"), presentation);
    });
    connect(&m_hotkey, &Hotkey::activated, this, [this]() {
        if (!m_window) return;
        if (m_window->window() && m_window->window()->isVisible()) m_window->hideWindow();
        else m_window->showWindow();
    });
    // Electron export.autoEnabled: write CSV+JSON on an interval while enabled.
    m_exportTimer = new QTimer(this);
    m_exportTimer->setInterval(30'000);
    connect(m_exportTimer, &QTimer::timeout, this, [this]() {
        if (!m_settings.value(QStringLiteral("exportAutoEnabled")).toBool()) return;
        const qint64 interval = m_settings.value(QStringLiteral("exportIntervalMs")).toInt(60'000);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_lastExportAt > 0 && now - m_lastExportAt < interval) return;
        exportNow();
        m_lastExportAt = now;
    });
    m_exportTimer->start();
}

// Electron tray.js buildTrayMenuTemplate, localized through the shared keys.
void AppState::rebuildTrayMenu()
{
    const auto t = [this](const QString &key, const QString &fallback) {
        const auto translated = m_i18n.t(key);
        return (translated.isEmpty() || translated == key) ? fallback : translated;
    };
    QVariantList views;
    for (const auto &id : allViews()) {
        views.append(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("label"), viewLabelFor(id)},
            {QStringLiteral("enabled"), !viewIsHidden(id)}
        });
    }
    const struct { const char *value; const char *key; const char *fallback; } contentItems[] = {
        {"tokens", "trayMenu.content.todayTokens", "Today tokens"},
        {"cost", "trayMenu.content.todayCost", "Today cost"},
        {"both", "trayMenu.content.todayBoth", "Today tokens & cost"},
        {"tokensAll", "trayMenu.content.totalTokens", "Total tokens"},
        {"costAll", "trayMenu.content.totalCost", "Total cost"},
        {"bothAll", "trayMenu.content.totalBoth", "Total tokens & cost"},
        {"limitsAllSessions", "trayMenu.content.aiToolLimits", "AI tool limits"},
        {"liveTokenRate", "trayMenu.content.liveTokenRate", "Live token rate"},
        {"barsSession", "trayMenu.content.sessionLimitBar", "Session limit bar"},
        {"barsWeekly", "trayMenu.content.weeklyLimitBar", "Weekly limit bar"},
        {"barsAllSessions", "trayMenu.content.allToolsLimitBars", "All tools limit bars"},
        {"bars", "trayMenu.content.lowestRemainingLimitBar", "Lowest remaining bar"},
        {"icon", "trayMenu.content.appIconOnly", "App icon only"},
        {"custom", "trayMenu.content.custom", "Custom"}
    };
    QVariantList contentOptions;
    for (const auto &item : contentItems) {
        contentOptions.append(QVariantMap{
            {QStringLiteral("value"), QString::fromUtf8(item.value)},
            {QStringLiteral("label"), t(QString::fromUtf8(item.key), QString::fromUtf8(item.fallback))}
        });
    }
    const struct { const char *value; const char *key; const char *fallback; } presentationItems[] = {
        {"tray", "trayMenu.presentation.tray", "Tray only"},
        {"floating", "trayMenu.presentation.floating", "Floating"},
        {"normal", "trayMenu.presentation.normal", "Normal"},
        {"desktop", "trayMenu.presentation.desktop", "Desktop"}
    };
    QVariantList presentationOptions;
    for (const auto &item : presentationItems) {
        presentationOptions.append(QVariantMap{
            {QStringLiteral("value"), QString::fromUtf8(item.value)},
            {QStringLiteral("label"), t(QString::fromUtf8(item.key), QString::fromUtf8(item.fallback))}
        });
    }
    const auto presentation = m_settings.value(QStringLiteral("trayMode")).toBool()
        ? QStringLiteral("tray")
        : m_settings.value(QStringLiteral("windowBehavior")).toString();
    m_tray.rebuildMenu(QVariantMap{
        {QStringLiteral("refreshLabel"), t(QStringLiteral("trayMenu.refreshNow"), QStringLiteral("Refresh now"))},
        {QStringLiteral("refreshEnabled"), true},
        {QStringLiteral("openViewLabel"), t(QStringLiteral("trayMenu.openView"), QStringLiteral("Open View"))},
        {QStringLiteral("views"), views},
        {QStringLiteral("contentLabel"), t(QStringLiteral("trayMenu.trayDisplay"), QStringLiteral("Tray Display"))},
        {QStringLiteral("contentOptions"), contentOptions},
        {QStringLiteral("contentCurrent"), m_settings.value(QStringLiteral("trayContent")).toString(QStringLiteral("tokens"))},
        {QStringLiteral("presentationLabel"), t(QStringLiteral("trayMenu.windowPresentation"), QStringLiteral("Window Presentation"))},
        {QStringLiteral("presentationOptions"), presentationOptions},
        {QStringLiteral("presentationCurrent"), presentation},
        {QStringLiteral("versionLabel"), QStringLiteral("v") + QString::fromUtf8(kAppVersion)},
        {QStringLiteral("settingsLabel"), t(QStringLiteral("trayMenu.settings"), QStringLiteral("Settings"))},
        {QStringLiteral("quitLabel"), t(QStringLiteral("trayMenu.quit"), QStringLiteral("Quit"))}
    });
}

void AppState::attachWindow(QWindow *window)
{
    m_window = new WidgetWindow(window, this);
    window->setMinimumWidth(240);
    window->setMinimumHeight(140);
    window->setMaximumWidth(1200);
    window->setMaximumHeight(1400);
    const int w = m_settings.value(QStringLiteral("windowWidth")).toInt(kDefaultWidth);
    const int h = m_settings.value(QStringLiteral("windowHeight")).toInt(kDefaultHeight);
    window->resize(qBound(240, w, 1200), qBound(140, h, 1400));
    m_window->applyChrome(m_settings.value(QStringLiteral("windowBehavior")).toString(),
                          jsonFlag(m_settings.value(QStringLiteral("systemGlass")), true)
                              ? m_settings.value(QStringLiteral("windowsBackdrop")).toString()
                              : QStringLiteral("off"),
                          m_settings.value(QStringLiteral("glassOpacity")).toInt(68),
                          m_settings.value(QStringLiteral("glassBlur")).toInt(32),
                          m_settings.value(QStringLiteral("keepAboveTaskbar")).toBool(),
                          m_settings.value(QStringLiteral("hideAppIcon")).toBool());
    if (!m_previewOnly) {
        m_tray.setVisible(m_settings.value(QStringLiteral("showTrayIcon")).toBool(true));
        m_hotkey.registerShortcut(m_settings.value(QStringLiteral("windowToggleShortcut")).toString());
        m_runtime.start(true);
        QTimer::singleShot(0, this, &AppState::ensureCurrencyRate);
        QTimer::singleShot(1200, this, &AppState::checkUpdates);
    } else {
        m_runtime.start(false);
        m_status = QStringLiteral("Live");
        emit statusChanged();
        rebuildRows();
    }
    applyTopEdge();
    if (m_view == QLatin1String("status")) refreshServiceStatus();
}

void AppState::setPreviewOnly(bool on)
{
    m_previewOnly = on;
}

namespace {

const QStringList &kAllViews()
{
    static const QStringList ids{
        QStringLiteral("home"), QStringLiteral("tool"), QStringLiteral("status"),
        QStringLiteral("device"), QStringLiteral("model"), QStringLiteral("project"),
        QStringLiteral("session"), QStringLiteral("limits"), QStringLiteral("trends")};
    return ids;
}

QStringList orderedViewIds(const QJsonObject &settings)
{
    const QSet<QString> known(kAllViews().begin(), kAllViews().end());
    QStringList out;
    QSet<QString> seen;
    const auto raw = settings.value(QStringLiteral("viewDisplayOrder")).toString()
                        .split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (auto item : raw) {
        item = item.trimmed();
        if (!known.contains(item) || seen.contains(item)) continue;
        out.append(item);
        seen.insert(item);
    }
    for (const auto &id : kAllViews()) {
        if (!seen.contains(id)) out.append(id);
    }
    return out;
}

QStringList hiddenViewIds(const QJsonObject &settings)
{
    QStringList hidden;
    QSet<QString> seen;
    const QSet<QString> known(kAllViews().begin(), kAllViews().end());
    const auto raw = settings.value(QStringLiteral("hiddenViews")).toString()
                        .split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (auto item : raw) {
        item = item.trimmed();
        if (!known.contains(item) || seen.contains(item)) continue;
        hidden.append(item);
        seen.insert(item);
    }
    if (hidden.size() >= kAllViews().size()) return {};
    return hidden;
}

} // namespace

QStringList AppState::allViews() const
{
    return orderedViewIds(m_settings);
}

QStringList AppState::views() const
{
    const auto hidden = hiddenViewIds(m_settings);
    QStringList out;
    for (const auto &id : orderedViewIds(m_settings)) {
        if (!hidden.contains(id)) out.append(id);
    }
    return out.isEmpty() ? orderedViewIds(m_settings) : out;
}

bool AppState::viewIsHidden(const QString &id) const
{
    return hiddenViewIds(m_settings).contains(id);
}

void AppState::toggleViewHidden(const QString &id)
{
    auto hidden = hiddenViewIds(m_settings);
    const int idx = hidden.indexOf(id);
    if (idx >= 0) {
        hidden.removeAt(idx);
    } else {
        if (views().size() <= 1 && views().contains(id)) return;
        hidden.append(id);
    }
    updateSetting(QStringLiteral("hiddenViews"), hidden.join(QLatin1Char(',')));
    if (viewIsHidden(m_view)) {
        const auto visible = views();
        if (!visible.isEmpty()) {
            m_view = visible.first();
            auto last = m_settings.value(QStringLiteral("lastViewState")).toObject();
            last.insert(QStringLiteral("breakdown"), m_view);
            m_settings.insert(QStringLiteral("lastViewState"), last);
            persist();
            emit viewChanged();
        }
    }
}

void AppState::moveViewDisplay(const QString &id, int direction)
{
    auto order = orderedViewIds(m_settings);
    const int from = order.indexOf(id);
    const int to = from + direction;
    if (from < 0 || to < 0 || to >= order.size()) return;
    order.move(from, to);
    updateSetting(QStringLiteral("viewDisplayOrder"), order.join(QLatin1Char(',')));
}

void AppState::resetViewDisplayOrder()
{
    updateSetting(QStringLiteral("viewDisplayOrder"), QString());
}

void AppState::showAllViews()
{
    updateSetting(QStringLiteral("hiddenViews"), QString());
}

// Electron setBreakdown: the back-home row only survives when the jump left
// Home through a home-module click (fromHome); any other navigation resets it.
void AppState::navigate(const QString &view, bool fromHome)
{
    if (view == QLatin1String("settings")) {
        setSettingsOpen(true);
        return;
    }
    if (m_settingsOpen) setSettingsOpen(false);
    m_homeReturnVisible = fromHome && m_view == QLatin1String("home") && view != QLatin1String("home");
    if (m_view == view) return;
    m_view = view;
    auto last = m_settings.value(QStringLiteral("lastViewState")).toObject();
    last.insert(QStringLiteral("breakdown"), view);
    m_settings.insert(QStringLiteral("lastViewState"), last);
    persist();
    emit viewChanged();
    if (m_view == QLatin1String("status")) refreshServiceStatus();
}

void AppState::setView(const QString &view)
{
    navigate(view, false);
}

void AppState::setViewFromHome(const QString &view)
{
    navigate(view, true);
}

void AppState::cycleView()
{
    const auto order = views();
    if (order.isEmpty()) return;
    const int index = order.indexOf(m_view);
    setView(order[(index < 0 ? 0 : index + 1) % order.size()]);
}

void AppState::setSettingsOpen(bool on)
{
    if (m_settingsOpen == on) return;
    m_settingsOpen = on;
    emit settingsOpenChanged();
}

void AppState::toggleSettings()
{
    setSettingsOpen(!m_settingsOpen);
}

void AppState::setChromeHover(bool on)
{
    if (m_chromeHover == on) return;
    m_chromeHover = on;
    emit chromeHoverChanged();
}

void AppState::setUtilityHover(bool on)
{
    if (m_utilityHover == on) return;
    m_utilityHover = on;
    emit utilityHoverChanged();
}

bool AppState::statusVisible() const
{
    const auto s = m_status.toLower();
    return s.contains(QLatin1String("error"))
        || s.contains(QLatin1String("fail"))
        || s.contains(QLatin1String("offline"))
        || s.contains(QLatin1String("timeout"));
}

int AppState::periodIndex() const
{
    if (m_period == QLatin1String("allTime")) return 2;
    if (m_period == QLatin1String("today")) return 0;
    return 1;
}

QString AppState::viewLabel() const
{
    return viewLabelFor(m_view);
}

QString AppState::viewIcon() const
{
    return viewIconFor(m_view);
}

bool jsonFlag(const QJsonValue &value, bool fallback)
{
    if (value.isBool()) return value.toBool();
    if (value.isDouble()) return value.toDouble() != 0;
    if (value.isString()) {
        const auto s = value.toString().trimmed().toLower();
        if (s == QLatin1String("true") || s == QLatin1String("1")) return true;
        if (s == QLatin1String("false") || s == QLatin1String("0")) return false;
    }
    return fallback;
}

bool AppState::titleIconOnly() const
{
    return jsonFlag(m_settings.value(QStringLiteral("titleIconOnly")), true);
}

bool AppState::showLiveTokenRate() const
{
    return jsonFlag(m_settings.value(QStringLiteral("showLiveTokenRate")), false);
}

QString AppState::viewLabelFor(const QString &id) const
{
    const auto key = QStringLiteral("views.") + id;
    const auto translated = m_i18n.t(key);
    if (!translated.isEmpty() && translated != key)
        return translated;
    static const QHash<QString, QString> fallback{
        {QStringLiteral("home"), QStringLiteral("Home")},
        {QStringLiteral("tool"), QStringLiteral("Tool")},
        {QStringLiteral("status"), QStringLiteral("Status")},
        {QStringLiteral("device"), QStringLiteral("Device")},
        {QStringLiteral("model"), QStringLiteral("Model")},
        {QStringLiteral("project"), QStringLiteral("Project")},
        {QStringLiteral("session"), QStringLiteral("Session")},
        {QStringLiteral("limits"), QStringLiteral("Limits")},
        {QStringLiteral("trends"), QStringLiteral("Trends")},
    };
    return fallback.value(id, id);
}

QString AppState::viewIconFor(const QString &id) const
{
    return QStringLiteral("qrc:/ui/icons/views/%1.svg").arg(id);
}

QVariantMap AppState::clientHealthCounts() const
{
    return m_healthCounts;
}

void AppState::refreshHealthCounts()
{
    const auto csv = m_settings.value(QStringLiteral("clients")).toString();
    QStringList tracked;
    for (const auto &part : csv.split(QLatin1Char(','))) {
        const auto id = normalizeTrackedClientId(part.trimmed());
        if (!id.isEmpty()) tracked.append(id);
    }
    if (tracked.isEmpty()) tracked = defaultClientIds();
    if (tracked.isEmpty()) return;

    QHash<QString, int> checked;
    QHash<QString, int> detected;
    for (const auto &root : clientSourceRoots(csv.isEmpty() ? tracked.join(QLatin1Char(',')) : csv)) {
        ++checked[root.client];
        const auto path = root.sourcePath.isEmpty() ? root.dir : root.sourcePath;
        if (QFileInfo::exists(path) || QDir(root.dir).exists())
            ++detected[root.client];
    }

    const auto usage = periodNamed(QStringLiteral("allTime")).value(QStringLiteral("clients")).toObject();
    int healthy = 0, review = 0, unavailable = 0;
    for (const auto &id : tracked) {
        const int nCheck = checked.value(id);
        const QString source = nCheck <= 0 ? QStringLiteral("unknown")
            : (detected.value(id) > 0 ? QStringLiteral("detected") : QStringLiteral("missing"));
        // Electron deriveClientOverall: unknown source → unknown overall → summary fallback.
        // Count it as review so a missing Qt health document still shows the toolsHealth line.
        if (source == QLatin1String("unknown") || source == QLatin1String("missing")) {
            if (source == QLatin1String("missing")) ++unavailable;
            else ++review;
            continue;
        }
        if (asNumber(usage.value(id)) > 0) ++healthy;
        else ++review;
    }
    m_healthCounts = {
        {QStringLiteral("healthy"), healthy},
        {QStringLiteral("review"), review},
        {QStringLiteral("unavailable"), unavailable}
    };
}

QString AppState::uiIcon(const QString &rel) const
{
    return QStringLiteral("qrc:/ui/icons/%1").arg(rel);
}

QString AppState::iconUrl(const QString &id) const
{
    // Clients that reuse a vendor mark have no file of their own — the same
    // mapping Electron's .row-icon-<id> CSS table encodes (zcode→zai,
    // micode→xiaomi, hermes→hermes-agent, grok→xai).
    static const QHash<QString, QString> aliases{
        {QStringLiteral("zcode"), QStringLiteral("zai")},
        {QStringLiteral("micode"), QStringLiteral("xiaomi")},
        {QStringLiteral("hermes"), QStringLiteral("hermes-agent")},
        {QStringLiteral("grok"), QStringLiteral("xai")}
    };
    const auto path = QStringLiteral(":/icons/icons/%1.svg").arg(aliases.value(id, id));
    return QFile::exists(path) ? QStringLiteral("qrc") + path : QString();
}

void AppState::saveScreenshot(const QString &path)
{
    if (!m_window || !m_window->window()) return;
    auto *quick = qobject_cast<QQuickWindow *>(m_window->window());
    if (!quick) return;
    const auto img = quick->grabWindow();
    QDir().mkpath(QFileInfo(path).absolutePath());
    img.save(path);
}

void AppState::forceView(const QString &view)
{
    m_homeReturnVisible = false;
    if (view == QLatin1String("settings")) {
        m_settingsOpen = true;
        emit settingsOpenChanged();
        return;
    }
    if (m_settingsOpen) {
        m_settingsOpen = false;
        emit settingsOpenChanged();
    }
    if (m_view == view) {
        emit viewChanged();
        return;
    }
    m_view = view;
    emit viewChanged();
    if (m_view == QLatin1String("status")) refreshServiceStatus();
}

void AppState::setPeriod(const QString &period)
{
    if (m_period == period) return;
    m_period = period;
    auto last = m_settings.value(QStringLiteral("lastViewState")).toObject();
    last.insert(QStringLiteral("period"), period);
    m_settings.insert(QStringLiteral("lastViewState"), last);
    persist();
    emit periodChanged();
    rebuildRows();
}

void AppState::refresh()
{
    if (!m_refreshing) {
        m_refreshing = true;
        emit refreshingChanged();
        QTimer::singleShot(700, this, [this]() {
            m_refreshing = false;
            emit refreshingChanged();
        });
    }
    m_runtime.refreshUsage();
    m_runtime.refreshLimits();
    if (m_view == QLatin1String("status")) refreshServiceStatus();
}

void AppState::seedServiceStatusPlaceholders()
{
    const struct {
        const char *id;
        const char *label;
        const char *page;
        const char *icon;
    } providers[] = {
        {"claude", "Claude", "https://status.claude.com", "claude"},
        {"openai", "OpenAI", "https://status.openai.com", "codex"},
        {"cursor", "Cursor", "https://status.cursor.com", "cursor"},
        {"deepseek", "DeepSeek", "https://status.deepseek.com", "deepseek"},
    };
    auto loading = m_i18n.t(QStringLiteral("serviceStatus.loading"));
    if (loading == QLatin1String("serviceStatus.loading")) loading = QStringLiteral("Checking status...");
    auto pill = m_i18n.t(QStringLiteral("serviceStatus.unknown"));
    if (pill == QLatin1String("serviceStatus.unknown")) pill = QStringLiteral("Unknown");
    m_serviceStatusRows.clear();
    for (const auto &p : providers) {
        m_serviceStatusRows.append(QVariantMap{
            {QStringLiteral("id"), QString::fromUtf8(p.id)},
            {QStringLiteral("label"), QString::fromUtf8(p.label)},
            {QStringLiteral("pageUrl"), QString::fromUtf8(p.page)},
            {QStringLiteral("icon"), iconUrl(QString::fromUtf8(p.icon))},
            {QStringLiteral("status"), QStringLiteral("unknown")},
            {QStringLiteral("pill"), pill},
            {QStringLiteral("description"), loading},
            {QStringLiteral("meta"), QString()}
        });
    }
    emit serviceStatusChanged();
}

void AppState::refreshServiceStatus()
{
    if (m_statusBusy) return;
    m_statusBusy = true;
    if (m_serviceStatusRows.isEmpty()) seedServiceStatusPlaceholders();
    QPointer<AppState> self(this);
    (void)QtConcurrent::run([self]() {
        auto raw = fetchServiceStatusPayloads();
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, raw]() {
            if (!self) return;
            QVariantList rows;
            for (const auto &v : raw) {
                auto m = v.toMap();
                const auto status = m.value(QStringLiteral("status")).toString();
                QString pillKey = QStringLiteral("serviceStatus.unknown");
                QString pillFallback = QStringLiteral("Unknown");
                if (status == QLatin1String("ok")) {
                    pillKey = QStringLiteral("serviceStatus.ok");
                    pillFallback = QStringLiteral("Operational");
                } else if (status == QLatin1String("degraded")) {
                    pillKey = QStringLiteral("serviceStatus.degraded");
                    pillFallback = QStringLiteral("Degraded");
                } else if (status == QLatin1String("outage")) {
                    pillKey = QStringLiteral("serviceStatus.outage");
                    pillFallback = QStringLiteral("Outage");
                }
                auto pill = self->m_i18n.t(pillKey);
                if (pill == pillKey) pill = pillFallback;
                auto desc = m.value(QStringLiteral("incidentTitle")).toString().trimmed();
                if (desc.isEmpty()) desc = m.value(QStringLiteral("description")).toString().trimmed();
                if (desc.isEmpty()) {
                    desc = self->m_i18n.t(QStringLiteral("serviceStatus.unknown"));
                    if (desc == QLatin1String("serviceStatus.unknown")) desc = QStringLiteral("Unknown");
                }
                QStringList meta;
                const int incidents = m.value(QStringLiteral("incidentCount")).toInt();
                const int maint = m.value(QStringLiteral("maintenanceCount")).toInt();
                if (incidents > 0) {
                    auto s = self->m_i18n.t(QStringLiteral("serviceStatus.incidents"), {{QStringLiteral("count"), incidents}});
                    meta.append(s == QLatin1String("serviceStatus.incidents")
                                    ? QStringLiteral("Incidents: %1").arg(incidents)
                                    : s);
                }
                if (maint > 0) {
                    auto s = self->m_i18n.t(QStringLiteral("serviceStatus.maintenance"), {{QStringLiteral("count"), maint}});
                    meta.append(s == QLatin1String("serviceStatus.maintenance")
                                    ? QStringLiteral("Maintenance: %1").arg(maint)
                                    : s);
                }
                if (status == QLatin1String("ok") && meta.isEmpty()) {
                    auto s = self->m_i18n.t(QStringLiteral("serviceStatus.noIssues"));
                    meta.append(s == QLatin1String("serviceStatus.noIssues") ? QStringLiteral("No active issues") : s);
                }
                const auto checked = QDateTime::fromString(m.value(QStringLiteral("checkedAt")).toString(), Qt::ISODateWithMs);
                if (checked.isValid()) {
                    const qint64 sec = qMax(qint64(0), checked.toUTC().secsTo(QDateTime::currentDateTimeUtc()));
                    QString ago;
                    if (sec < 60) {
                        ago = self->m_i18n.t(QStringLiteral("serviceStatus.agoSeconds"), {{QStringLiteral("n"), int(sec)}});
                        if (ago == QLatin1String("serviceStatus.agoSeconds")) ago = QStringLiteral("%1s ago").arg(sec);
                    } else if (sec < 3600) {
                        const int n = int(sec / 60);
                        ago = self->m_i18n.t(QStringLiteral("serviceStatus.agoMinutes"), {{QStringLiteral("n"), n}});
                        if (ago == QLatin1String("serviceStatus.agoMinutes")) ago = QStringLiteral("%1m ago").arg(n);
                    } else {
                        const int n = int(sec / 3600);
                        ago = self->m_i18n.t(QStringLiteral("serviceStatus.agoHours"), {{QStringLiteral("n"), n}});
                        if (ago == QLatin1String("serviceStatus.agoHours")) ago = QStringLiteral("%1h ago").arg(n);
                    }
                    meta.append(ago);
                }
                m.insert(QStringLiteral("pill"), pill);
                m.insert(QStringLiteral("description"), desc);
                m.insert(QStringLiteral("meta"), meta.join(QStringLiteral(" · ")));
                m.insert(QStringLiteral("icon"), self->iconUrl(m.value(QStringLiteral("iconId")).toString()));
                rows.append(m);
            }
            self->m_serviceStatusRows = rows;
            self->m_statusBusy = false;
            emit self->serviceStatusChanged();
        }, Qt::QueuedConnection);
    });
}

void AppState::updateSetting(const QString &key, const QVariant &value)
{
    const auto json = QJsonValue::fromVariant(value);
    m_settings.insert(key, json);
    if (key.contains(QLatin1String("Key"), Qt::CaseInsensitive)
        || key.contains(QLatin1String("Cookie"))
        || key.contains(QLatin1String("Token"))
        || key.contains(QLatin1String("Secret"))
        || key.contains(QLatin1String("Profiles"))
        || key.contains(QLatin1String("AccessKey"))) {
        CredentialStore::set(key, json);
    }
    if (key == QLatin1String("language")) m_i18n.setLanguage(value.toString());
    if (key == QLatin1String("startAtLogin")) {
#ifdef Q_OS_WIN
        QSettings run(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"), QSettings::NativeFormat);
        if (value.toBool())
            run.setValue(QStringLiteral("TokenMonitor"), QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
        else
            run.remove(QStringLiteral("TokenMonitor"));
#endif
    }
    SettingsStore::save(m_settings);
    m_settingsUi = CredentialStore::redactedForUi(m_settings);
    applyTheme();
    const bool chromeOnly = key == QLatin1String("hiddenViews")
        || key == QLatin1String("viewDisplayOrder")
        || key == QLatin1String("showLiveDot")
        || key == QLatin1String("showToolIcons")
        || key == QLatin1String("showCompactTotalTokens")
        || key == QLatin1String("showHomeLimitBars")
        || key == QLatin1String("showHomeLimitProviderNames")
        || key == QLatin1String("periodMonthMode")
        || key == QLatin1String("currency")
        || key == QLatin1String("modelRankingMetric")
        || key == QLatin1String("language")
        || key == QLatin1String("titleIconOnly")
        || key == QLatin1String("showLiveTokenRate")
        || key == QLatin1String("liveTokenRateScope")
        || key == QLatin1String("compactTokenUnits")
        || key == QLatin1String("systemGlass")
        || key == QLatin1String("windowsBackdrop")
        || key == QLatin1String("glassOpacity")
        || key == QLatin1String("glassBlur")
        || key == QLatin1String("zoomFactor")
        || key == QLatin1String("settingsInTitlebar")
        || key == QLatin1String("reduceMotion")
        || key == QLatin1String("heatmapMetric")
        || key == QLatin1String("themeColors")
        || key == QLatin1String("hiddenClients")
        || key == QLatin1String("clientDisplayOrder")
        || key == QLatin1String("windowWidth")
        || key == QLatin1String("windowHeight");
    if (!chromeOnly) {
        m_runtime.stop();
        m_runtime.configure(m_settings);
        m_runtime.start();
    }
    if (m_window) {
        const auto backdrop = jsonFlag(m_settings.value(QStringLiteral("systemGlass")), true)
            ? m_settings.value(QStringLiteral("windowsBackdrop")).toString()
            : QStringLiteral("off");
        m_window->applyChrome(m_settings.value(QStringLiteral("windowBehavior")).toString(),
                              backdrop,
                              m_settings.value(QStringLiteral("glassOpacity")).toInt(68),
                              m_settings.value(QStringLiteral("glassBlur")).toInt(32),
                              m_settings.value(QStringLiteral("keepAboveTaskbar")).toBool(),
                          m_settings.value(QStringLiteral("hideAppIcon")).toBool());
    }
    m_tray.setVisible(m_settings.value(QStringLiteral("showTrayIcon")).toBool(true));
    m_hotkey.registerShortcut(m_settings.value(QStringLiteral("windowToggleShortcut")).toString());
    applyTopEdge();
    rebuildTrayMenu();
    emit settingsChanged();
}

void AppState::cycleBehavior()
{
    const auto cur = m_settings.value(QStringLiteral("windowBehavior")).toString();
    QString next = QStringLiteral("normal");
    if (cur == QLatin1String("floating")) next = QStringLiteral("normal");
    else if (cur == QLatin1String("normal")) next = QStringLiteral("desktop");
    else next = QStringLiteral("floating");
    updateSetting(QStringLiteral("windowBehavior"), next);
}

void AppState::exportNow()
{
    auto dir = m_settings.value(QStringLiteral("exportDir")).toString();
    if (dir.isEmpty()) dir = QDir(Paths::userDataDir()).filePath(QStringLiteral("export"));
    tmon::exportNow(m_runtime.displayStats(), dir);
}

void AppState::exportDiagnostics()
{
    auto dir = m_settings.value(QStringLiteral("exportDir")).toString();
    if (dir.isEmpty()) dir = QDir(Paths::userDataDir()).filePath(QStringLiteral("export"));
    tmon::exportDiagnostics(QJsonObject::fromVariantMap(m_settingsUi), m_runtime.displayStats(),
                            m_runtime.usage()->lastError(), dir);
}

// Electron's export folder picker (dialog.showOpenDialog): a native directory
// dialog seeded with the current export dir.
void AppState::pickExportDir()
{
    auto dir = m_settings.value(QStringLiteral("exportDir")).toString();
    if (dir.isEmpty()) dir = QDir(Paths::userDataDir()).filePath(QStringLiteral("export"));
    const auto picked = QFileDialog::getExistingDirectory(nullptr,
        tr("Choose export folder"), dir, QFileDialog::ShowDirsOnly);
    if (picked.isEmpty()) return;
    updateSetting(QStringLiteral("exportDir"), QDir::toNativeSeparators(picked));
}

void AppState::checkUpdates()
{
    QPointer<AppState> self(this);
    auto *thread = QThread::create([self]() {
        HttpClient http;
        const QMap<QString, QString> headers{
            {QStringLiteral("Accept"), QStringLiteral("application/json")},
            {QStringLiteral("User-Agent"), QStringLiteral("token-monitor/0.57.0")}
        };
        const auto res = http.get(QUrl(QStringLiteral("https://github.com/Javis603/token-monitor/releases/latest")),
                                  headers, 15000);
        const auto obj = res.json().object();
        auto version = obj.value(QStringLiteral("tag_name")).toString().trimmed();
        if (version.startsWith(QLatin1Char('v')) || version.startsWith(QLatin1Char('V')))
            version = version.mid(1);
        const auto url = obj.value(QStringLiteral("html_url")).toString();
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, version, url]() {
            if (!self) return;
            const bool ready = !version.isEmpty() && version != QString::fromUtf8(kAppVersion);
            self->m_appUpdateReady = ready;
            self->m_appUpdateLabel = ready ? (QStringLiteral("↑ v") + version) : QString();
            self->m_appUpdateUrl = url;
            emit self->appUpdateChanged();
            if (ready) emit self->updateAvailable(version, url);
        }, Qt::QueuedConnection);
    });
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

QString AppState::formatUsd(double value) const
{
    const int digits = std::abs(value) >= 10.0 ? 2 : 4;
    return QLatin1Char('$') + QString::number(value, 'f', digits);
}

QString AppState::formatDuration(double ms) const
{
    const auto totalMinutes = qMax(0, qRound(ms / 60000.0));
    const auto hours = totalMinutes / 60;
    const auto minutes = totalMinutes % 60;
    if (hours > 0) return QStringLiteral("%1h %2m").arg(hours).arg(minutes);
    if (minutes > 0) return QStringLiteral("%1m").arg(minutes);
    return QStringLiteral("0m");
}

void AppState::openUpdate()
{
    if (!m_appUpdateUrl.isEmpty())
        QDesktopServices::openUrl(QUrl(m_appUpdateUrl));
}

void AppState::dismissUpdate()
{
    m_appUpdateReady = false;
    m_appUpdateLabel.clear();
    emit appUpdateChanged();
}

void AppState::setBubbleCollapsed(bool on)
{
    if (m_bubbleCollapsed == on) return;
    m_bubbleCollapsed = on;
    emit bubbleChanged();
}

QString AppState::formatTokens(double tokens) const
{
    const auto rounded = std::llround(tokens);
    const auto unitSystem = m_settings.value(QStringLiteral("compactTokenUnits")).toString();
    const auto locale = m_i18n.resolvedLanguage();
    const bool localized = (unitSystem == QLatin1String("localized") || unitSystem == QLatin1String("cn")
                            || unitSystem == QLatin1String("chinese"))
        && (locale.startsWith(QLatin1String("zh")) || locale.startsWith(QLatin1String("ja"))
            || locale.startsWith(QLatin1String("ko")));
    struct Unit { double divisor; QString suffix; };
    QVector<Unit> units;
    if (localized) {
        QString tenThousand = QStringLiteral("万");
        QString hundredMillion = QStringLiteral("亿");
        if (locale.startsWith(QLatin1String("ko"))) {
            tenThousand = QStringLiteral("만");
            hundredMillion = QStringLiteral("억");
        } else if (locale.startsWith(QLatin1String("ja"))) {
            tenThousand = QStringLiteral("万");
            hundredMillion = QStringLiteral("億");
        } else if (locale.startsWith(QLatin1String("zh-TW")) || locale.startsWith(QLatin1String("zh-HK"))) {
            tenThousand = QStringLiteral("萬");
            hundredMillion = QStringLiteral("億");
        }
        units = {{1e4, tenThousand}, {1e8, hundredMillion}};
    } else {
        units = {{1e3, QStringLiteral("K")}, {1e6, QStringLiteral("M")}, {1e9, QStringLiteral("B")}};
    }
    const double abs = std::fabs(double(rounded));
    int unitIndex = -1;
    for (int i = units.size() - 1; i >= 0; --i) {
        if (abs >= units[i].divisor) {
            unitIndex = i;
            break;
        }
    }
    if (unitIndex < 0) return QString::number(rounded);
    auto formatScaled = [&]() {
        const double scaled = double(rounded) / units[unitIndex].divisor;
        const int digits = localized ? (std::fabs(scaled) < 10 ? 2 : 1) : 1;
        return QString::number(scaled, 'f', digits);
    };
    auto display = formatScaled();
    const double promotion = localized ? 10000.0 : 1000.0;
    if (std::fabs(display.toDouble()) >= promotion && unitIndex < units.size() - 1) {
        ++unitIndex;
        display = formatScaled();
    }
    while (display.contains(QLatin1Char('.')) && (display.endsWith(QLatin1Char('0')) || display.endsWith(QLatin1Char('.'))))
        display.chop(1);
    return display + units[unitIndex].suffix;
}

QString AppState::formatNumber(double tokens) const
{
    return QLocale(QLocale::English).toString(qint64(std::llround(tokens)));
}

QString AppState::clientLabelOf(const QString &id) const
{
    return clientLabel(id);
}

bool AppState::floatingBubble() const
{
    return m_settings.value(QStringLiteral("floatingBubbleEnabled")).toBool();
}

QStringList AppState::homeModules() const
{
    auto order = m_settings.value(QStringLiteral("homeModuleOrder")).toString(QStringLiteral("limits,tool,device,model,trends"))
                     .split(QLatin1Char(','), Qt::SkipEmptyParts);
    const auto hidden = m_settings.value(QStringLiteral("hiddenHomeModules")).toString()
                            .split(QLatin1Char(','), Qt::SkipEmptyParts);
    QStringList out;
    for (auto item : order) {
        item = item.trimmed();
        if (!item.isEmpty() && !hidden.contains(item)) out.append(item);
    }
    return out;
}

QVariantList AppState::subscriptions() const
{
    return m_settings.value(QStringLiteral("subscriptions")).toArray().toVariantList();
}

void AppState::openSession(const QString &id)
{
    for (const auto &row : m_sessionRows) {
        const auto map = row.toMap();
        if (map.value(QStringLiteral("id")).toString() == id) {
            m_sessionDetail = map;
            m_sessionDetailOpen = true;
            emit sessionDetailChanged();
            return;
        }
    }
}

void AppState::closeSession()
{
    if (!m_sessionDetailOpen) return;
    m_sessionDetailOpen = false;
    m_sessionDetail.clear();
    emit sessionDetailChanged();
}

void AppState::addSubscription(const QString &name, double amount, const QString &period)
{
    auto list = m_settings.value(QStringLiteral("subscriptions")).toArray();
    list.append(QJsonObject{
        {QStringLiteral("name"), name},
        {QStringLiteral("amount"), amount},
        {QStringLiteral("period"), period.isEmpty() ? QStringLiteral("month") : period}
    });
    const auto mode = m_settings.value(QStringLiteral("hubMode")).toString();
    if (mode == QLatin1String("client")) {
        const auto result = m_runtime.hubClient()->putSubscriptions(
            list, m_settings.value(QStringLiteral("subscriptionsUpdatedAt")).toString());
        if (result.value(QStringLiteral("httpStatus")).toInt() == 409) return;
        if (result.contains(QStringLiteral("updatedAt")))
            m_settings.insert(QStringLiteral("subscriptionsUpdatedAt"), result.value(QStringLiteral("updatedAt")));
        if (result.contains(QStringLiteral("records")))
            list = result.value(QStringLiteral("records")).toArray();
    }
    m_settings.insert(QStringLiteral("subscriptions"), list);
    persist();
    m_settingsUi = CredentialStore::redactedForUi(m_settings);
    emit settingsChanged();
}

void AppState::removeSubscription(int index)
{
    auto list = m_settings.value(QStringLiteral("subscriptions")).toArray();
    if (index < 0 || index >= list.size()) return;
    list.removeAt(index);
    m_settings.insert(QStringLiteral("subscriptions"), list);
    persist();
    m_settingsUi = CredentialStore::redactedForUi(m_settings);
    emit settingsChanged();
}

void AppState::showDashboard()
{
    emit dashboardRequested();
}

void AppState::ensureCurrencyRate()
{
    if (m_currencyFetched) return;
    const auto code = m_settings.value(QStringLiteral("currency")).toString(QStringLiteral("USD")).toUpper();
    if (code == QLatin1String("USD")) return;
    auto rates = m_settings.value(QStringLiteral("currencyRates")).toObject();
    if (rates.contains(code) && rates.value(code).toDouble() > 0) return;
    HttpClient http;
    const auto res = http.get(QUrl(QStringLiteral("https://open.er-api.com/v6/latest/USD")));
    m_currencyFetched = true;
    if (res.status != 200) return;
    const auto fetched = res.json().object().value(QStringLiteral("rates")).toObject();
    if (fetched.contains(code)) {
        rates.insert(code, fetched.value(code));
        m_settings.insert(QStringLiteral("currencyRates"), rates);
        persist();
        emit statsChanged();
    }
}

void AppState::applyTopEdge()
{
    if (!m_window) return;
    m_window->setTopEdgeEnabled(m_settings.value(QStringLiteral("topEdgeHideEnabled")).toBool());
}

void AppState::startMove() { if (m_window) m_window->startMove(); }
void AppState::startResize(const QString &edge)
{
    if (!m_window) return;
    Qt::Edges edges;
    if (edge.contains(QLatin1Char('l'))) edges |= Qt::LeftEdge;
    if (edge.contains(QLatin1Char('r'))) edges |= Qt::RightEdge;
    if (edge.contains(QLatin1Char('t'))) edges |= Qt::TopEdge;
    if (edge.contains(QLatin1Char('b'))) edges |= Qt::BottomEdge;
    if (edges) m_window->startResize(edges);
}
void AppState::persistWindowSize(int w, int h)
{
    if (w < 240 || h < 140) return;
    m_settings.insert(QStringLiteral("windowWidth"), w);
    m_settings.insert(QStringLiteral("windowHeight"), h);
    SettingsStore::save(m_settings);
}

static QStringList settingCsv(const QJsonObject &settings, const QString &key, const QString &fallback)
{
    return settings.value(key).toString(fallback).split(QLatin1Char(','), Qt::SkipEmptyParts);
}

static QString joinSettingCsv(QStringList ids)
{
    for (auto &id : ids) id = id.trimmed();
    ids.removeAll(QString());
    return ids.join(QLatin1Char(','));
}

QVariantList AppState::catalogClientRows() const
{
    const auto tracked = settingCsv(m_settings, QStringLiteral("clients"), defaultClientsCsv());
    const auto hidden = settingCsv(m_settings, QStringLiteral("hiddenClients"), {});
    QStringList order = settingCsv(m_settings, QStringLiteral("clientDisplayOrder"), {});
    QSet<QString> seen;
    QVariantList out;
    auto appendId = [&](const QString &id) {
        if (id.isEmpty() || seen.contains(id)) return;
        seen.insert(id);
        out.append(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("label"), clientLabel(id)},
            {QStringLiteral("tracked"), tracked.contains(id)},
            {QStringLiteral("hidden"), hidden.contains(id)},
            {QStringLiteral("icon"), iconUrl(id)}
        });
    };
    for (const auto &id : order) appendId(id.trimmed());
    for (const auto &c : clientCatalog()) appendId(c.id);
    return out;
}

void AppState::toggleClientTracked(const QString &id)
{
    auto list = settingCsv(m_settings, QStringLiteral("clients"), defaultClientsCsv());
    if (list.contains(id)) list.removeAll(id);
    else list.append(id);
    if (list.isEmpty()) list.append(id);
    updateSetting(QStringLiteral("clients"), joinSettingCsv(list));
}

void AppState::toggleClientHidden(const QString &id)
{
    auto list = settingCsv(m_settings, QStringLiteral("hiddenClients"), {});
    if (list.contains(id)) list.removeAll(id);
    else list.append(id);
    updateSetting(QStringLiteral("hiddenClients"), joinSettingCsv(list));
}

QVariantList AppState::catalogLimitRows() const
{
    const auto enabled = settingCsv(m_settings, QStringLiteral("limitProviders"), defaultLimitProvidersCsv());
    QStringList order = settingCsv(m_settings, QStringLiteral("limitProviderOrder"), defaultLimitProvidersCsv());
    QHash<QString, QVariantMap> byId;
    for (const auto &row : m_limitRows) {
        const auto map = row.toMap();
        byId.insert(map.value(QStringLiteral("id")).toString(), map);
    }
    static const QHash<QString, QStringList> tags{
        {QStringLiteral("claude"), {QStringLiteral("自动"), QStringLiteral("OAuth/CLI"), QStringLiteral("Web")}},
        {QStringLiteral("codex"), {QStringLiteral("自动"), QStringLiteral("OAuth/App/CLI")}},
        {QStringLiteral("cursor"), {QStringLiteral("自动"), QStringLiteral("Web")}},
        {QStringLiteral("antigravity"), {QStringLiteral("自动"), QStringLiteral("OAuth/App/CLI")}},
        {QStringLiteral("opencode"), {QStringLiteral("自动"), QStringLiteral("API/Web")}},
        {QStringLiteral("kimi"), {QStringLiteral("Coding Plan"), QStringLiteral("Web/API")}},
        {QStringLiteral("grok"), {QStringLiteral("自动"), QStringLiteral("CLI/Web")}},
        {QStringLiteral("copilot"), {QStringLiteral("手动登录"), QStringLiteral("API")}},
        {QStringLiteral("zed"), {QStringLiteral("手动登录"), QStringLiteral("Web")}},
        {QStringLiteral("commandcode"), {QStringLiteral("手动登录"), QStringLiteral("Web")}},
        {QStringLiteral("mimo"), {QStringLiteral("Token Plan"), QStringLiteral("Web")}},
        {QStringLiteral("kiro"), {QStringLiteral("自动"), QStringLiteral("CLI")}},
    };
    QSet<QString> seen;
    QVariantList out;
    auto appendId = [&](const QString &id) {
        if (id.isEmpty() || seen.contains(id)) return;
        seen.insert(id);
        const auto live = byId.value(id);
        const auto status = live.value(QStringLiteral("status")).toString();
        QString statusLabel = QStringLiteral("尚未设定");
        if (id == QLatin1String("antigravity") || id == QLatin1String("grok") || id == QLatin1String("kiro"))
            statusLabel = QStringLiteral("自动检测");
        else if (status == QLatin1String("ok")) {
            statusLabel = QStringLiteral("已连接");
            if (!live.value(QStringLiteral("windows")).toList().isEmpty())
                statusLabel = QStringLiteral("1/1 已连接");
        }
        else if (status == QLatin1String("unauthorized")) statusLabel = QStringLiteral("未授权");
        out.append(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("label"), limitProviderSettingsLabel(id)},
            {QStringLiteral("enabled"), enabled.contains(id)},
            {QStringLiteral("tags"), tags.value(id)},
            {QStringLiteral("statusLabel"), statusLabel},
            {QStringLiteral("icon"), iconUrl(id)}
        });
    };
    for (const auto &id : order) appendId(id.trimmed());
    for (const auto &p : limitProviderCatalog()) appendId(p.id);
    return out;
}

void AppState::toggleLimitProvider(const QString &id)
{
    auto list = settingCsv(m_settings, QStringLiteral("limitProviders"), defaultLimitProvidersCsv());
    if (list.contains(id)) list.removeAll(id);
    else list.append(id);
    updateSetting(QStringLiteral("limitProviders"), joinSettingCsv(list));
}

void AppState::showAllClients()
{
    updateSetting(QStringLiteral("hiddenClients"), QString());
}

void AppState::resetClientDisplayOrder()
{
    updateSetting(QStringLiteral("clientDisplayOrder"), QString());
}

void AppState::minimizeWindow() { if (m_window) m_window->minimize(); }
void AppState::closeWindow() { if (m_window) m_window->closeWindow(); }
void AppState::hideWindow() { if (m_window) m_window->hideWindow(); }
void AppState::showWindow() { if (m_window) m_window->showWindow(); }

void AppState::startAtLogin(bool on)
{
#ifdef Q_OS_WIN
    QSettings run(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"), QSettings::NativeFormat);
    if (on) run.setValue(QStringLiteral("TokenMonitor"), QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    else run.remove(QStringLiteral("TokenMonitor"));
#endif
    updateSetting(QStringLiteral("startAtLogin"), on);
}

QJsonObject AppState::periodNamed(const QString &name) const
{
    if (isDerivedPeriod(name))
        return m_derivedPeriod;
    const auto stats = m_runtime.displayStats();
    auto periods = stats.value(QStringLiteral("periods")).toObject();
    if (periods.isEmpty()) periods = QJsonObject{
        {QStringLiteral("today"), m_runtime.deviceRecord().value(QStringLiteral("today"))},
        {QStringLiteral("month"), m_runtime.deviceRecord().value(QStringLiteral("month"))},
        {QStringLiteral("allTime"), m_runtime.deviceRecord().value(QStringLiteral("allTime"))}
    };
    return periods.value(name).toObject();
}

bool AppState::fixedPeriodActive() const
{
    return isDerivedPeriod(m_period);
}

QStringList AppState::fixedPeriodRange() const
{
    return m_derivedRange;
}

QString AppState::periodTabLabel() const
{
    // Electron syncPeriodTabs: the middle slot always carries a month-mode
    // label — the live selection while the month slot is active, otherwise the
    // periodMonthMode preference. It never reads TOTAL.
    QString mode;
    if (m_period == QLatin1String("month") || isDerivedPeriod(m_period))
        mode = m_period;
    else
        mode = m_settings.value(QStringLiteral("periodMonthMode")).toString(QStringLiteral("month"));
    if (mode == QLatin1String("week")) return QStringLiteral("WEEK");
    if (mode == QLatin1String("last7")) return QStringLiteral("7D");
    if (mode == QLatin1String("last30")) return QStringLiteral("30D");
    return QStringLiteral("MONTH");
}

// Rebuilds the derived 本周/最近 7 天/最近 30 天 period from the retained daily
// history, mirroring Electron fixedPeriodRanges.derivePeriod(): sum tokens/cost
// over the selected date range and fold the per-day client/model attribution
// back into a period object. A day's activeTimeMs travels with the day record
// (Electron's archive writes it; a Qt-only install currently has none), so the
// range-scoped active time matches the Electron summary semantics.
void AppState::rebuildDerivedPeriod()
{
    m_derivedPeriod = QJsonObject{};
    m_derivedRange.clear();
    m_derivedReady = false;
    if (!isDerivedPeriod(m_period) || m_historyDays.isEmpty()) return;

    const auto today = QDate::currentDate();
    QDate start;
    if (m_period == QLatin1String("week")) {
        // Electron rangeForSelection: locale-aware week start, ISO Monday fallback.
        auto localeName = m_i18n.resolvedLanguage();
        if (localeName.isEmpty()) localeName = QLocale::system().name();
        const auto loc = QLocale(localeName);
        const int firstDay = int(loc.firstDayOfWeek()); // Qt: Monday=1 … Sunday=7
        start = today.addDays(-((today.dayOfWeek() - firstDay) + 7) % 7);
    } else if (m_period == QLatin1String("last7")) {
        start = today.addDays(-6);
    } else {
        start = today.addDays(-29);
    }
    const QString startKey = start.toString(Qt::ISODate);
    const QString endKey = today.toString(Qt::ISODate);
    m_derivedRange = {startKey, endKey};

    double tokens = 0, cost = 0;
    QHash<QString, double> clientTokens, clientCosts, modelTokens, modelCosts;
    for (const auto &v : m_historyDays) {
        const auto day = v.toMap();
        const auto key = day.value(QStringLiteral("date")).toString();
        if (key < startKey || key > endKey) continue;
        tokens += day.value(QStringLiteral("tokens")).toDouble();
        cost += day.value(QStringLiteral("cost")).toDouble();
        const auto dayClients = day.value(QStringLiteral("clients")).toMap();
        const auto dayClientCosts = day.value(QStringLiteral("clientCosts")).toMap();
        for (auto it = dayClients.begin(); it != dayClients.end(); ++it) {
            clientTokens[it.key()] += it.value().toDouble();
            clientCosts[it.key()] += dayClientCosts.value(it.key()).toDouble();
        }
        const auto dayModels = day.value(QStringLiteral("models")).toMap();
        const auto dayModelCosts = day.value(QStringLiteral("modelCosts")).toMap();
        for (auto it = dayModels.begin(); it != dayModels.end(); ++it) {
            modelTokens[it.key()] += it.value().toDouble();
            modelCosts[it.key()] += dayModelCosts.value(it.key()).toDouble();
        }
    }

    auto mapOf = [](const QHash<QString, double> &hash) {
        QJsonObject out;
        for (auto it = hash.begin(); it != hash.end(); ++it)
            out.insert(it.key(), qMax(0.0, std::round(it.value())));
        return out;
    };
    auto costMapOf = [](const QHash<QString, double> &hash) {
        QJsonObject out;
        for (auto it = hash.begin(); it != hash.end(); ++it) {
            if (it.value() != 0) out.insert(it.key(), it.value());
        }
        return out;
    };
    m_derivedPeriod = QJsonObject{
        {QStringLiteral("totalTokens"), qMax(0.0, std::round(tokens))},
        {QStringLiteral("costUsd"), cost},
        {QStringLiteral("clients"), mapOf(clientTokens)},
        {QStringLiteral("clientCosts"), costMapOf(clientCosts)},
        {QStringLiteral("models"), mapOf(modelTokens)},
        {QStringLiteral("modelCosts"), costMapOf(modelCosts)},
        {QStringLiteral("derivedFixedRange"), true}
    };
    m_derivedReady = true;
}

QJsonObject AppState::currentPeriod() const
{
    return periodNamed(m_period);
}

void AppState::rebuildDashboard()
{
    const auto allTime = periodNamed(QStringLiteral("allTime"));
    const double total = asNumber(allTime.value(QStringLiteral("totalTokens")));
    const auto clients = allTime.value(QStringLiteral("clients")).toObject();
    const auto costs = allTime.value(QStringLiteral("clientCosts")).toObject();
    m_dashboardToolRows.clear();
    QList<QVariantMap> toolMaps;
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        auto row = rowOf(it.key(), clientLabel(it.key()), asNumber(it.value()), asNumber(costs.value(it.key())), total);
        row.insert(QStringLiteral("icon"), iconUrl(it.key() == QLatin1String("grok") ? QStringLiteral("xai") : it.key()));
        row.insert(QStringLiteral("color"), markColor(it.key()));
        toolMaps.append(row);
    }
    std::sort(toolMaps.begin(), toolMaps.end(), [](const QVariantMap &a, const QVariantMap &b) {
        return a.value(QStringLiteral("tokens")).toDouble() > b.value(QStringLiteral("tokens")).toDouble();
    });
    for (const auto &row : toolMaps) m_dashboardToolRows.append(row);

    const auto models = allTime.value(QStringLiteral("models")).toObject();
    const auto modelCosts = allTime.value(QStringLiteral("modelCosts")).toObject();
    m_dashboardModelRows.clear();
    QList<QVariantMap> modelMaps;
    QString favorite;
    double favoriteTokens = -1;
    for (auto it = models.begin(); it != models.end(); ++it) {
        const auto tokens = asNumber(it.value());
        if (tokens > favoriteTokens) {
            favoriteTokens = tokens;
            favorite = it.key();
        }
        auto row = rowOf(it.key(), it.key(), tokens, asNumber(modelCosts.value(it.key())), total);
        const auto vendor = modelVendorFor(it.key());
        QString iconId = vendor;
        if (vendor.isEmpty())
            iconId = QStringLiteral("token-monitor");
        else if (vendor == QLatin1String("xai"))
            iconId = QStringLiteral("grok");
        const auto icon = iconUrl(iconId);
        row.insert(QStringLiteral("icon"), icon.isEmpty() ? iconUrl(QStringLiteral("token-monitor")) : icon);
        row.insert(QStringLiteral("color"), markColor(vendor.isEmpty() ? it.key() : vendor));
        modelMaps.append(row);
    }
    std::sort(modelMaps.begin(), modelMaps.end(), [](const QVariantMap &a, const QVariantMap &b) {
        return a.value(QStringLiteral("tokens")).toDouble() > b.value(QStringLiteral("tokens")).toDouble();
    });
    for (const auto &row : modelMaps) m_dashboardModelRows.append(row);

    double histTokens = 0, histCost = 0, histTime = 0, peak = 0;
    int activeDays = 0;
    QHash<QString, double> byDate;
    for (const auto &v : m_historyDays) {
        const auto day = v.toMap();
        const auto tokens = day.value(QStringLiteral("tokens")).toDouble();
        const auto cost = day.value(QStringLiteral("cost")).toDouble();
        const auto ms = day.value(QStringLiteral("activeTimeMs")).toDouble();
        const auto key = day.value(QStringLiteral("date")).toString();
        byDate.insert(key, tokens);
        histTokens += tokens;
        histCost += cost;
        histTime += ms;
        if (tokens > 0) ++activeDays;
        peak = qMax(peak, tokens);
    }
    int streak = 0;
    auto cursor = QDate::currentDate();
    while (byDate.value(cursor.toString(Qt::ISODate)) > 0) {
        ++streak;
        cursor = cursor.addDays(-1);
    }
    double messages = 0;
    const auto sessions = allTime.value(QStringLiteral("sessions")).toObject();
    for (auto it = sessions.begin(); it != sessions.end(); ++it)
        messages += asNumber(it.value().toObject().value(QStringLiteral("messageCount")));

    m_dashboardSummary = {
        {QStringLiteral("totalTokens"), histTokens > 0 ? histTokens : total},
        {QStringLiteral("totalCost"), histCost > 0 ? histCost : asNumber(allTime.value(QStringLiteral("costUsd")))},
        {QStringLiteral("activeDays"), activeDays},
        {QStringLiteral("currentStreak"), streak},
        {QStringLiteral("activeTimeMs"), histTime},
        {QStringLiteral("peakDayTokens"), peak},
        {QStringLiteral("favoriteModel"), favorite},
        {QStringLiteral("messages"), messages}
    };
}

QString AppState::totalText() const
{
    // Electron renders '—' for a fixed range when no device answers with
    // history, so a missing history never reads as silent zeroes.
    if (fixedPeriodActive() && !m_derivedReady) return QStringLiteral("—");
    return QLocale(QLocale::English).toString(qint64(asNumber(currentPeriod().value(QStringLiteral("totalTokens")))));
}

QString AppState::compactText() const
{
    if (fixedPeriodActive() && !m_derivedReady) return QStringLiteral("0");
    return formatTokens(asNumber(currentPeriod().value(QStringLiteral("totalTokens"))));
}

QString AppState::costText() const
{
    if (fixedPeriodActive() && !m_derivedReady) return QString();
    const double usd = asNumber(currentPeriod().value(QStringLiteral("costUsd")));
    const auto currency = m_settings.value(QStringLiteral("currency")).toString(QStringLiteral("USD"));
    double rate = 1;
    const auto rates = m_settings.value(QStringLiteral("currencyRates")).toObject();
    if (rates.contains(currency)) rate = rates.value(currency).toDouble(1);
    return formatUsd(usd * rate);
}

void AppState::applyTheme()
{
    auto colors = m_settings.value(QStringLiteral("themeColors")).toObject();
    const auto preset = colors.value(QStringLiteral("preset")).toString(QStringLiteral("default"));
    QString accent = QStringLiteral("#b7ead4");
    QString bg = QStringLiteral("#303438");
    QString text = QStringLiteral("#eef5fb");
    QString muted = QStringLiteral("#a3adbb");
    if (preset == QLatin1String("obsidian")) {
        accent = QStringLiteral("#e6e8ec"); bg = QStringLiteral("#0b0c0e"); text = QStringLiteral("#eceef2"); muted = QStringLiteral("#8f949c");
    } else if (preset == QLatin1String("porcelain")) {
        accent = QStringLiteral("#2563eb"); bg = QStringLiteral("#f6f7f9"); text = QStringLiteral("#1c1f26"); muted = QStringLiteral("#5b626d");
    }
    if (colors.contains(QStringLiteral("accent"))) accent = colors.value(QStringLiteral("accent")).toString(accent);
    if (colors.contains(QStringLiteral("bg"))) bg = colors.value(QStringLiteral("bg")).toString(bg);
    if (colors.contains(QStringLiteral("text"))) text = colors.value(QStringLiteral("text")).toString(text);
    if (colors.contains(QStringLiteral("muted"))) muted = colors.value(QStringLiteral("muted")).toString(muted);
    const QString number = preset == QLatin1String("porcelain") ? QStringLiteral("#1c1f26") : QStringLiteral("#f3fbf7");
    auto uiFont = cssFirstFamily(m_settings.value(QStringLiteral("interfaceFontFamily")).toString());
    if (uiFont.isEmpty())
        // Electron's --ui-font default leads with ui-monospace, which Chromium
        // resolves to Consolas on Windows — not Cascadia (wider, and the reason
        // Qt rows read crowded next to Electron's).
        uiFont = pickInstalledFont({QStringLiteral("Consolas"), QStringLiteral("Cascadia Mono"), QStringLiteral("Cascadia Code")}, QStringLiteral("Segoe UI"));
    auto displayFont = cssFirstFamily(m_settings.value(QStringLiteral("displayFontFamily")).toString());
    if (displayFont.isEmpty())
        displayFont = pickInstalledFont({QStringLiteral("Segoe UI"), QStringLiteral("SF Pro Display")}, QStringLiteral("Segoe UI"));
    m_theme = QVariantMap{
        {QStringLiteral("accent"), accent},
        {QStringLiteral("bg"), bg},
        {QStringLiteral("text"), text},
        {QStringLiteral("muted"), muted},
        {QStringLiteral("number"), number},
        {QStringLiteral("glassOpacity"), jsonFlag(m_settings.value(QStringLiteral("systemGlass")), true)
            ? m_settings.value(QStringLiteral("glassOpacity")).toInt(68) / 100.0
            : 1.0},
        {QStringLiteral("radius"), 8},
        {QStringLiteral("font"), uiFont},
        {QStringLiteral("displayFont"), displayFont},
        {QStringLiteral("preset"), preset}
    };
}

void AppState::persist()
{
    SettingsStore::save(m_settings);
}

void AppState::rebuildRows()
{
    // History first: a fixed-range selection (本周/最近 7 天/最近 30 天) derives
    // its period from these days, so everything below must see the fresh
    // derivation — not the one built during the previous stats frame.
    m_historyDays.clear();
    const auto hist = m_runtime.usage()->history().value(QStringLiteral("days")).toObject();
    QStringList keys = hist.keys();
    keys.sort();
    const auto start = keys.size() > 371 ? keys.size() - 371 : 0;
    auto dayRowOf = [](const QString &key, const QJsonObject &day) {
        return QVariantMap{
            {QStringLiteral("date"), key},
            {QStringLiteral("tokens"), asNumber(day.value(QStringLiteral("tokens")))},
            {QStringLiteral("cost"), asNumber(day.value(QStringLiteral("costUsd")))},
            {QStringLiteral("activeTimeMs"), asNumber(day.value(QStringLiteral("activeTimeMs")))},
            {QStringLiteral("clients"), day.value(QStringLiteral("clients")).toObject().toVariantMap()},
            {QStringLiteral("clientCosts"), day.value(QStringLiteral("clientCosts")).toObject().toVariantMap()},
            {QStringLiteral("models"), day.value(QStringLiteral("models")).toObject().toVariantMap()},
            {QStringLiteral("modelCosts"), day.value(QStringLiteral("modelCosts")).toObject().toVariantMap()}
        };
    };
    for (int i = start; i < keys.size(); ++i)
        m_historyDays.append(dayRowOf(keys[i], hist.value(keys[i]).toObject()));
    // Electron clampDaily(points, 45) slices the history series as stored.
    // History omits zero days, so the sparkline can span from first usage
    // (e.g. Apr) rather than the last 45 calendar days of zeros + a spike.
    const auto todayKey = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    const auto todayPeriod = periodNamed(QStringLiteral("today"));
    const double liveTokens = asNumber(todayPeriod.value(QStringLiteral("totalTokens")));
    const double liveCost = asNumber(todayPeriod.value(QStringLiteral("costUsd")));
    bool patchedToday = false;
    for (int i = 0; i < m_historyDays.size(); ++i) {
        auto map = m_historyDays[i].toMap();
        if (map.value(QStringLiteral("date")).toString() != todayKey) continue;
        // Electron dailyWithLiveToday only overrides the stored day when the
        // live period is at least as large — an empty/absent today period
        // (cold start before the first scan) must not zero a real day.
        if (liveTokens < map.value(QStringLiteral("tokens")).toDouble()) {
            patchedToday = true;
            break;
        }
        map.insert(QStringLiteral("tokens"), liveTokens);
        map.insert(QStringLiteral("cost"), liveCost);
        // Electron dailyWithLiveToday folds the live period's attribution over
        // today's row, so derived ranges count today's usage per client/model.
        map.insert(QStringLiteral("clients"), todayPeriod.value(QStringLiteral("clients")).toObject().toVariantMap());
        map.insert(QStringLiteral("clientCosts"), todayPeriod.value(QStringLiteral("clientCosts")).toObject().toVariantMap());
        map.insert(QStringLiteral("models"), todayPeriod.value(QStringLiteral("models")).toObject().toVariantMap());
        map.insert(QStringLiteral("modelCosts"), todayPeriod.value(QStringLiteral("modelCosts")).toObject().toVariantMap());
        m_historyDays[i] = map;
        patchedToday = true;
    }
    if (!patchedToday && (liveTokens > 0 || liveCost > 0)) {
        QVariantMap liveRow{
            {QStringLiteral("date"), todayKey},
            {QStringLiteral("tokens"), liveTokens},
            {QStringLiteral("cost"), liveCost},
            {QStringLiteral("clients"), todayPeriod.value(QStringLiteral("clients")).toObject().toVariantMap()},
            {QStringLiteral("clientCosts"), todayPeriod.value(QStringLiteral("clientCosts")).toObject().toVariantMap()},
            {QStringLiteral("models"), todayPeriod.value(QStringLiteral("models")).toObject().toVariantMap()},
            {QStringLiteral("modelCosts"), todayPeriod.value(QStringLiteral("modelCosts")).toObject().toVariantMap()}
        };
        m_historyDays.append(liveRow);
    }
    rebuildDerivedPeriod();

    const auto period = currentPeriod();
    const double total = asNumber(period.value(QStringLiteral("totalTokens")));
    const auto clients = period.value(QStringLiteral("clients")).toObject();
    const auto costs = period.value(QStringLiteral("clientCosts")).toObject();
    m_clientRows.clear();
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        auto row = rowOf(it.key(), clientLabel(it.key()), asNumber(it.value()), asNumber(costs.value(it.key())), total);
        row.insert(QStringLiteral("icon"), iconUrl(it.key() == QLatin1String("grok") ? QStringLiteral("xai") : it.key()));
        row.insert(QStringLiteral("color"), markColor(it.key()));
        m_clientRows.append(row);
    }
    std::sort(m_clientRows.begin(), m_clientRows.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("tokens")).toDouble() > b.toMap().value(QStringLiteral("tokens")).toDouble();
    });
    const auto models = period.value(QStringLiteral("models")).toObject();
    const auto modelCosts = period.value(QStringLiteral("modelCosts")).toObject();
    m_modelRows.clear();
    for (auto it = models.begin(); it != models.end(); ++it) {
        auto row = rowOf(it.key(), it.key(), asNumber(it.value()), asNumber(modelCosts.value(it.key())), total);
        const auto vendor = modelVendorFor(it.key());
        QString iconId = vendor;
        if (vendor.isEmpty())
            iconId = QStringLiteral("token-monitor");
        else if (vendor == QLatin1String("xai"))
            iconId = QStringLiteral("grok");
        const auto icon = iconUrl(iconId);
        row.insert(QStringLiteral("icon"), icon.isEmpty() ? iconUrl(QStringLiteral("token-monitor")) : icon);
        row.insert(QStringLiteral("color"), markColor(vendor.isEmpty() ? it.key() : vendor));
        m_modelRows.append(row);
    }
    std::sort(m_modelRows.begin(), m_modelRows.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("tokens")).toDouble() > b.toMap().value(QStringLiteral("tokens")).toDouble();
    });
    m_projectRows.clear();
    const auto projects = period.value(QStringLiteral("projects")).toObject();
    for (auto it = projects.begin(); it != projects.end(); ++it) {
        const auto p = it.value().toObject();
        m_projectRows.append(rowOf(it.key(), p.value(QStringLiteral("label")).toString(it.key()),
                                   asNumber(p.value(QStringLiteral("tokens"))), asNumber(p.value(QStringLiteral("costUsd"))), total));
    }
    m_sessionRows.clear();
    const auto sessions = period.value(QStringLiteral("sessions")).toObject();
    for (auto it = sessions.begin(); it != sessions.end(); ++it) {
        const auto s = it.value().toObject();
        const auto client = s.value(QStringLiteral("client")).toString();
        const auto sessionId = s.value(QStringLiteral("sessionId")).toString(it.key());
        const auto tokens = asNumber(s.value(QStringLiteral("totalTokens")));
        if (tokens <= 0) continue;
        QStringList titleParts;
        titleParts.append(clientLabel(client));
        const auto modelLabel = sessionModelLabel(s);
        if (!modelLabel.isEmpty()) titleParts.append(modelLabel);
        const auto sessionTitle = s.value(QStringLiteral("title")).toString().trimmed();
        const auto msgs = qint64(asNumber(s.value(QStringLiteral("messageCount"))));
        QStringList activity;
        const auto when = compactSessionTime(s.value(QStringLiteral("lastUsedAt")).toString(s.value(QStringLiteral("startedAt")).toString()));
        if (!when.isEmpty()) activity.append(when);
        if (msgs > 0) {
            activity.append(groupedInt(msgs) + (msgs == 1 ? QStringLiteral(" msg") : QStringLiteral(" msgs")));
        }
        const auto titleJoined = titleParts.join(QStringLiteral(" · "));
        const auto activityJoined = activity.join(QStringLiteral(" · "));
        auto row = rowOf(it.key(), sessionTitle.isEmpty() ? titleJoined : sessionTitle,
                         tokens, asNumber(s.value(QStringLiteral("costUsd"))), total);
        row.insert(QStringLiteral("client"), client);
        row.insert(QStringLiteral("extra"), sessionTitle.isEmpty() ? activityJoined : titleJoined);
        row.insert(QStringLiteral("activity"), sessionTitle.isEmpty() ? QString() : activityJoined);
        row.insert(QStringLiteral("detail"), sessionIdLabel(sessionId));
        row.insert(QStringLiteral("sortTime"), sessionTimestampMs(s));
        row.insert(QStringLiteral("icon"), iconUrl(client == QLatin1String("grok") ? QStringLiteral("xai") : client));
        row.insert(QStringLiteral("color"), markColor(client));
        m_sessionRows.append(row);
    }
    std::sort(m_sessionRows.begin(), m_sessionRows.end(), [](const QVariant &a, const QVariant &b) {
        const auto am = a.toMap();
        const auto bm = b.toMap();
        const auto at = am.value(QStringLiteral("sortTime")).toLongLong();
        const auto bt = bm.value(QStringLiteral("sortTime")).toLongLong();
        if (at != bt) return at > bt;
        return am.value(QStringLiteral("tokens")).toDouble() > bm.value(QStringLiteral("tokens")).toDouble();
    });
    m_limitRows.clear();
    const auto stats = m_runtime.displayStats();
    const auto providers = stats.value(QStringLiteral("limits")).toObject().value(QStringLiteral("providers")).toArray();
    auto localProviders = m_runtime.deviceRecord().value(QStringLiteral("limits")).toObject().value(QStringLiteral("providers")).toArray();
    const auto use = providers.isEmpty() ? localProviders : providers;
    const bool showUsed = m_settings.value(QStringLiteral("showLimitUsed")).toBool(false);
    for (const auto &pV : use) {
        const auto p = pV.toObject();
        double pct = 0;
        auto windows = p.value(QStringLiteral("windows")).toArray();
        QString detail;
        QVariantList windowRows;
        int windowCount = 0;
        for (const auto &wV : windows) {
            const auto w = wV.toObject();
            double usedPct = w.value(QStringLiteral("usedPercent")).toDouble(qQNaN());
            if (!std::isfinite(usedPct) && w.value(QStringLiteral("limit")).toDouble() > 0)
                usedPct = 100.0 * w.value(QStringLiteral("used")).toDouble() / w.value(QStringLiteral("limit")).toDouble();
            double remainPct = w.contains(QStringLiteral("remainingPercent")) && w.value(QStringLiteral("remainingPercent")).isDouble()
                ? w.value(QStringLiteral("remainingPercent")).toDouble()
                : (std::isfinite(usedPct) ? (100.0 - usedPct) : qQNaN());
            if (!std::isfinite(usedPct) && std::isfinite(remainPct)) usedPct = 100.0 - remainPct;
            if (windowCount == 0 && std::isfinite(usedPct)) {
                pct = usedPct;
                detail = w.value(QStringLiteral("label")).toString();
            }
            QString kind = w.value(QStringLiteral("kind")).toString();
            QString label = w.value(QStringLiteral("label")).toString();
            if (label.isEmpty()) label = kind;
            const bool showMeter = w.value(QStringLiteral("showMeter")).toBool(true);
            const QString metric = w.value(QStringLiteral("metric")).toString();
            QString valueText;
            if (metric == QLatin1String("spend")) {
                const double used = w.value(QStringLiteral("used")).toDouble();
                const double limit = w.value(QStringLiteral("limit")).toDouble();
                valueText = limit > 0 ? formatUsd(used) + QStringLiteral(" / ") + formatUsd(limit) : formatUsd(used);
            } else if (metric == QLatin1String("credits")) {
                valueText = w.value(QStringLiteral("currency")).toString() + QLatin1Char(' ')
                    + QString::number(w.value(QStringLiteral("remaining")).toDouble(), 'f', 2);
            } else if (std::isfinite(remainPct) || std::isfinite(usedPct)) {
                const double fill = showUsed ? (std::isfinite(usedPct) ? usedPct : 100.0 - remainPct)
                                             : (std::isfinite(remainPct) ? remainPct : 100.0 - usedPct);
                valueText = QString::number(qRound(fill)) + QStringLiteral("% ")
                    + (showUsed ? QStringLiteral("used") : QStringLiteral("left"));
            }
            const double barRemain = std::isfinite(remainPct) ? remainPct : 0;
            // Electron formatLimitBoundary: a grant expiry reads "Expires in…",
            // a mixed boundary "Changes in…", everything else "Reset in…";
            // a window without a timestamp falls back to its cadence label.
            QString resetText;
            auto resetAt = QDateTime::fromString(w.value(QStringLiteral("resetsAt")).toString(), Qt::ISODateWithMs);
            if (!resetAt.isValid())
                resetAt = QDateTime::fromString(w.value(QStringLiteral("resetsAt")).toString(), Qt::ISODate);
            const auto boundaryKind = w.value(QStringLiteral("boundaryKind")).toString();
            if (resetAt.isValid()) {
                const qint64 diffMs = QDateTime::currentDateTimeUtc().msecsTo(resetAt);
                const bool mixed = boundaryKind == QLatin1String("mixed");
                const QString prefix = boundaryKind == QLatin1String("expiry") ? QStringLiteral("Expires")
                    : mixed ? QStringLiteral("Changes in") : QStringLiteral("Reset");
                if (diffMs <= 0)
                    resetText = mixed ? QStringLiteral("Changes now") : prefix + QStringLiteral(" now");
                else
                    resetText = prefix + QLatin1Char(' ') + formatDurationMs(diffMs);
            } else {
                const auto desc = w.value(QStringLiteral("resetDescription")).toString();
                if (!desc.isEmpty()) {
                    resetText = m_i18n.t(QStringLiteral("home.reset"), {{QStringLiteral("value"), desc}});
                    if (resetText == QLatin1String("home.reset")) resetText = QStringLiteral("Reset ") + desc;
                }
            }
            windowRows.append(QVariantMap{
                {QStringLiteral("kind"), kind},
                {QStringLiteral("label"), label},
                {QStringLiteral("homeLabel"), homeWindowLabel(kind, label, m_i18n)},
                {QStringLiteral("remainingPercent"), barRemain},
                {QStringLiteral("usedPercent"), std::isfinite(usedPct) ? usedPct : 0},
                {QStringLiteral("value"), valueText},
                {QStringLiteral("metric"), metric},
                {QStringLiteral("showMeter"), showMeter},
                {QStringLiteral("resetText"), resetText}
            });
            ++windowCount;
        }
        QVariantList homeWindows;
        QVector<QPair<int, QVariantMap>> ranked;
        for (int i = 0; i < windowRows.size(); ++i) {
            auto map = windowRows[i].toMap();
            const bool showMeter = map.value(QStringLiteral("showMeter"), true).toBool();
            if (!showMeter && map.value(QStringLiteral("value")).toString().isEmpty()) continue;
            if (!showMeter && map.value(QStringLiteral("metric")).toString() != QLatin1String("spend")
                && map.value(QStringLiteral("metric")).toString() != QLatin1String("credits"))
                continue;
            ranked.append({windowKindPriority(map.value(QStringLiteral("kind")).toString()) * 100 + i, map});
        }
        std::sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
        for (int i = 0; i < ranked.size() && homeWindows.size() < 2; ++i)
            homeWindows.append(ranked[i].second);
        const auto status = p.value(QStringLiteral("status")).toString();
        QString statusLabel;
        if (status == QLatin1String("notConfigured")) statusLabel = QStringLiteral("Not signed in");
        else if (status == QLatin1String("unauthorized")) statusLabel = QStringLiteral("Sign in again");
        else if (status == QLatin1String("disabled")) statusLabel = QStringLiteral("Disabled");
        else if (status == QLatin1String("unavailable")) statusLabel = QStringLiteral("Unavailable");
        m_limitRows.append(QVariantMap{
            {QStringLiteral("id"), p.value(QStringLiteral("provider")).toString()},
            {QStringLiteral("label"), limitProviderLabel(p.value(QStringLiteral("provider")).toString())},
            {QStringLiteral("status"), status},
            {QStringLiteral("statusLabel"), statusLabel},
            {QStringLiteral("percent"), pct / 100.0},
            {QStringLiteral("detail"), detail},
            {QStringLiteral("plan"), p.value(QStringLiteral("planLabel")).toString()},
            {QStringLiteral("updatedText"), formatUpdatedText(p.value(QStringLiteral("updatedAt")).toString())},
            {QStringLiteral("icon"), iconUrl(p.value(QStringLiteral("provider")).toString())},
            {QStringLiteral("color"), markColor(p.value(QStringLiteral("provider")).toString())},
            {QStringLiteral("windows"), windowRows},
            {QStringLiteral("homeWindows"), homeWindows}
        });
    }
    m_deviceRows.clear();
    const auto devices = stats.value(QStringLiteral("devices")).toArray();
    const auto localId = m_settings.value(QStringLiteral("deviceId")).toString();
    double maxDevice = 0;
    QVector<QVariantMap> deviceMaps;
    if (fixedPeriodActive()) {
        // Electron derives per-device rows from each device's history snapshot;
        // Qt only has the local device's history, so a lone device derives from
        // the fixed-range period and multi-device hubs show no rows at all
        // (the range note replaces the list, like Electron's unavailable state).
        if (devices.size() == 1) {
            const auto d = devices.first().toObject();
            const auto updatedAt = d.value(QStringLiteral("updatedAt")).toString();
            QStringList extra;
            const auto platform = d.value(QStringLiteral("osName")).toString(d.value(QStringLiteral("platform")).toString());
            if (!platform.isEmpty()) extra.append(platform);
            if (d.value(QStringLiteral("stale")).toBool()) extra.append(QStringLiteral("stale"));
            else if (!updatedAt.isEmpty()) extra.append(formatUpdatedText(updatedAt));
            maxDevice = asNumber(m_derivedPeriod.value(QStringLiteral("totalTokens")));
            deviceMaps.append(QVariantMap{
                {QStringLiteral("id"), d.value(QStringLiteral("deviceId")).toString()},
                {QStringLiteral("label"), d.value(QStringLiteral("hostname")).toString()},
                {QStringLiteral("stale"), d.value(QStringLiteral("stale")).toBool()},
                {QStringLiteral("tokens"), maxDevice},
                {QStringLiteral("cost"), asNumber(m_derivedPeriod.value(QStringLiteral("costUsd")))},
                {QStringLiteral("platform"), d.value(QStringLiteral("platform")).toString()},
                {QStringLiteral("extra"), extra.join(QStringLiteral(" · "))},
                {QStringLiteral("local"), !localId.isEmpty()
                    && d.value(QStringLiteral("deviceId")).toString() == localId}
            });
        }
    } else {
        for (const auto &dV : devices) {
            const auto d = dV.toObject();
            const auto periodObj = devicePeriodObject(d, m_period);
            const auto tokens = asNumber(periodObj.value(QStringLiteral("totalTokens")));
            maxDevice = qMax(maxDevice, tokens);
            const auto updatedAt = d.value(QStringLiteral("updatedAt")).toString();
            QStringList extra;
            const auto platform = d.value(QStringLiteral("osName")).toString(d.value(QStringLiteral("platform")).toString());
            if (!platform.isEmpty()) extra.append(platform);
            if (d.value(QStringLiteral("stale")).toBool()) extra.append(QStringLiteral("stale"));
            else if (!updatedAt.isEmpty()) extra.append(formatUpdatedText(updatedAt));
            deviceMaps.append(QVariantMap{
                {QStringLiteral("id"), d.value(QStringLiteral("deviceId")).toString()},
                {QStringLiteral("label"), d.value(QStringLiteral("hostname")).toString()},
                {QStringLiteral("stale"), d.value(QStringLiteral("stale")).toBool()},
                {QStringLiteral("tokens"), tokens},
                {QStringLiteral("cost"), asNumber(periodObj.value(QStringLiteral("costUsd")))},
                {QStringLiteral("platform"), d.value(QStringLiteral("platform")).toString()},
                {QStringLiteral("extra"), extra.join(QStringLiteral(" · "))},
                {QStringLiteral("local"), !localId.isEmpty() && d.value(QStringLiteral("deviceId")).toString() == localId}
            });
        }
    }
    std::sort(deviceMaps.begin(), deviceMaps.end(), [](const QVariantMap &a, const QVariantMap &b) {
        return a.value(QStringLiteral("tokens")).toDouble() > b.value(QStringLiteral("tokens")).toDouble();
    });
    for (auto &row : deviceMaps) {
        const auto tokens = row.value(QStringLiteral("tokens")).toDouble();
        row.insert(QStringLiteral("percent"), maxDevice > 0 ? tokens / maxDevice : 0.0);
        m_deviceRows.append(row);
    }
    m_trendPoints.clear();
    const int trendStart = m_historyDays.size() > 45 ? m_historyDays.size() - 45 : 0;
    for (int i = trendStart; i < m_historyDays.size(); ++i) {
        const auto map = m_historyDays[i].toMap();
        m_trendPoints.append(QVariantMap{
            {QStringLiteral("date"), map.value(QStringLiteral("date"))},
            {QStringLiteral("value"), map.value(QStringLiteral("tokens"))}
        });
    }
    m_clientModel.setRows(m_clientRows);
    m_modelModel.setRows(m_modelRows);
    m_projectModel.setRows(m_projectRows);
    m_sessionModel.setRows(m_sessionRows);
    m_deviceModel.setRows(m_deviceRows);
    m_limitModel.setRows(m_limitRows);
    refreshHealthCounts();
    rebuildDashboard();
    m_tray.setTooltip(QStringLiteral("Token Monitor  ") + totalText());
    emit statsChanged();
}

} // namespace tmon
