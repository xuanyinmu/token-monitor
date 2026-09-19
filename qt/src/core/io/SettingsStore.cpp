#include "core/io/SettingsStore.h"

#include "core/catalog/Catalog.h"
#include "core/io/JsonIo.h"
#include "core/io/Paths.h"
#include "core/tmon.h"

#include <QHostInfo>
#include <QJsonArray>
#include <QJsonValue>
#include <QSysInfo>
#include <QUuid>

namespace tmon {
namespace {

QString defaultDeviceId()
{
    const auto env = qEnvironmentVariable("TOKEN_MONITOR_DEVICE_ID").trimmed();
    if (!env.isEmpty()) return env;
    return QSysInfo::machineHostName() + QLatin1Char('-') + QSysInfo::currentCpuArchitecture();
}

} // namespace

QJsonObject SettingsStore::defaults()
{
    const auto envHub = qEnvironmentVariable("TOKEN_MONITOR_HUB_URL").trimmed();
    const auto windowBehavior = qEnvironmentVariable("TOKEN_MONITOR_ALWAYS_ON_TOP") == QLatin1String("0")
        ? QStringLiteral("normal")
        : QStringLiteral("floating");
    return QJsonObject{
        {QStringLiteral("hubMode"), envHub.isEmpty() ? QStringLiteral("local") : QStringLiteral("client")},
        {QStringLiteral("hubUrl"), envHub},
        {QStringLiteral("hubHostPort"), kHubDefaultPort},
        {QStringLiteral("hubHostSecret"), qEnvironmentVariable("TOKEN_MONITOR_SECRET")},
        {QStringLiteral("secret"), qEnvironmentVariable("TOKEN_MONITOR_SECRET")},
        {QStringLiteral("windowBehavior"), windowBehavior},
        {QStringLiteral("windowWidth"), kDefaultWidth},
        {QStringLiteral("windowHeight"), kDefaultHeight},
        {QStringLiteral("alwaysOnTop"), windowBehavior == QLatin1String("floating")},
        {QStringLiteral("keepAboveTaskbar"), false},
        {QStringLiteral("refreshMs"), 15000},
        {QStringLiteral("glassOpacity"), 68},
        {QStringLiteral("glassBlur"), 32},
        {QStringLiteral("systemGlass"), true},
        {QStringLiteral("windowsBackdrop"), QStringLiteral("acrylic")},
        {QStringLiteral("reduceMotion"), QStringLiteral("system")},
        {QStringLiteral("showLiveDot"), true},
        {QStringLiteral("showToolIcons"), true},
        {QStringLiteral("titleIconOnly"), true},
        {QStringLiteral("showCompactTotalTokens"), false},
        {QStringLiteral("showLiveTokenRate"), false},
        {QStringLiteral("liveTokenRateScope"), QStringLiteral("all")},
        {QStringLiteral("compactTokenUnits"), QStringLiteral("western")},
        {QStringLiteral("tokenRateMode"), QStringLiteral("speed")},
        {QStringLiteral("heatmapMetric"), QStringLiteral("cost")},
        {QStringLiteral("modelRankingMetric"), QStringLiteral("tokens")},
        {QStringLiteral("homeActiveDaysWindow"), QStringLiteral("all")},
        {QStringLiteral("periodMonthMode"), QStringLiteral("month")},
        {QStringLiteral("themeColors"), QJsonObject{}},
        {QStringLiteral("vendorColors"), QJsonObject{}},
        {QStringLiteral("interfaceFontFamily"), QString()},
        {QStringLiteral("displayFontFamily"), QStringLiteral("ui-monospace")},
        {QStringLiteral("floatingBubbleEnabled"), false},
        {QStringLiteral("floatingBubbleTrigger"), QStringLiteral("click")},
        {QStringLiteral("floatingBubbleContent"), QStringLiteral("icon")},
        {QStringLiteral("topEdgeHideEnabled"), false},
        {QStringLiteral("topEdgeHideDocked"), false},
        {QStringLiteral("lastViewState"), QJsonObject{{QStringLiteral("period"), QStringLiteral("today")}, {QStringLiteral("breakdown"), QStringLiteral("tool")}}},
        {QStringLiteral("discordRpcEnabled"), false},
        {QStringLiteral("deviceId"), defaultDeviceId()},
        {QStringLiteral("clients"), defaultClientsCsv()},
        {QStringLiteral("customScanPaths"), QJsonObject{}},
        {QStringLiteral("hiddenClients"), QString()},
        {QStringLiteral("pinnedClients"), QString()},
        {QStringLiteral("viewDisplayOrder"), QString()},
        {QStringLiteral("hiddenViews"), QString()},
        {QStringLiteral("homeModuleOrder"), QStringLiteral("limits,tool,device,model,trends")},
        {QStringLiteral("hiddenHomeModules"), QStringLiteral("tool,device")},
        {QStringLiteral("showHomeLimitBars"), false},
        {QStringLiteral("showHomeLimitProviderNames"), false},
        {QStringLiteral("projectsEnabled"), true},
        {QStringLiteral("historyEnabled"), true},
        {QStringLiteral("historyIntervalMs"), 900000},
        {QStringLiteral("sessionUsageArchiveEnabled"), true},
        {QStringLiteral("wslScanEnabled"), true},
        {QStringLiteral("exportAutoEnabled"), false},
        {QStringLiteral("exportDir"), QString()},
        {QStringLiteral("exportIntervalMs"), 60000},
        {QStringLiteral("collectionMode"), QStringLiteral("live")},
        {QStringLiteral("collectionIntervalMs"), kDefaultCollectionIntervalMs},
        {QStringLiteral("syncUploadIntervalMs"), 15000},
        {QStringLiteral("serviceProviderDisplayOrder"), QString()},
        {QStringLiteral("hiddenServiceProviders"), QString()},
        {QStringLiteral("serviceStatusRefreshMs"), 60000},
        {QStringLiteral("archivedClientUsage"), QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("clients"), QJsonObject{}}}},
        {QStringLiteral("allTimeSince"), QStringLiteral("2024-01-01")},
        {QStringLiteral("customModelPricing"), QJsonArray{}},
        {QStringLiteral("limitsEnabled"), true},
        {QStringLiteral("limitProviders"), defaultLimitProvidersCsv()},
        {QStringLiteral("limitProviderOrder"), defaultLimitProvidersCsv()},
        {QStringLiteral("homeLimitProviderOrder"), QString()},
        {QStringLiteral("hiddenHomeLimitProviders"), QString()},
        {QStringLiteral("homeLimitAccountCount"), 3},
        {QStringLiteral("limitsRefreshMode"), QStringLiteral("fixed")},
        {QStringLiteral("limitsRefreshMs"), kDefaultLimitsRefreshMs},
        {QStringLiteral("cursorDisabledAccountIds"), QJsonArray{}},
        {QStringLiteral("cursorManualAccountIds"), QJsonArray{}},
        {QStringLiteral("showLimitSource"), false},
        {QStringLiteral("maskLimitAccountEmails"), false},
        {QStringLiteral("claudePrepaidBalanceEnabled"), true},
        {QStringLiteral("opencodeAmbientEnabled"), true},
        {QStringLiteral("opencodeLocalLimitsEnabled"), false},
        {QStringLiteral("codexResetForecastEnabled"), false},
        {QStringLiteral("showCodexAdditionalLimits"), true},
        {QStringLiteral("showLimitUsed"), false},
        {QStringLiteral("subscriptions"), QJsonArray{}},
        {QStringLiteral("subscriptionsOrphaned"), QJsonObject{{QStringLiteral("hubUrl"), QString()}, {QStringLiteral("records"), QJsonArray{}}}},
        {QStringLiteral("subscriptionsCacheHub"), QString()},
        {QStringLiteral("windowBounds"), QJsonValue::Null},
        {QStringLiteral("windowMaximized"), false},
        {QStringLiteral("zoomFactor"), 1},
        {QStringLiteral("settingsInTitlebar"), false},
        {QStringLiteral("showTrayIcon"), true},
        {QStringLiteral("trayMode"), false},
        {QStringLiteral("hideAppIcon"), false},
        {QStringLiteral("trayContent"), QStringLiteral("tokens")},
        {QStringLiteral("showTrayProviderBadge"), false},
        {QStringLiteral("windowToggleShortcut"), QString()},
        {QStringLiteral("currency"), QStringLiteral("USD")},
        {QStringLiteral("currencyRates"), QJsonObject{}},
        {QStringLiteral("startAtLogin"), false},
        {QStringLiteral("automaticAppUpdates"), false},
        {QStringLiteral("language"), QStringLiteral("auto")},
        {QStringLiteral("clientDisplayOrder"), QString()},
        {QStringLiteral("lastPostedDeviceId"), QString()},
        {QStringLiteral("zaiApiRegion"), QStringLiteral("global")},
        {QStringLiteral("qoderSite"), QStringLiteral("global")},
        {QStringLiteral("copilotEnterpriseHost"), QString()},
        {QStringLiteral("alibabaVariant"), QString()},
        {QStringLiteral("volcengineRegion"), QString()},
        {QStringLiteral("volcengineAgentRegion"), QString()},
        {QStringLiteral("appUpdate"), QJsonObject{
            {QStringLiteral("lastCheckedAt"), QJsonValue::Null},
            {QStringLiteral("lastKnownLatest"), QJsonValue::Null},
            {QStringLiteral("dismissedVersion"), QJsonValue::Null}
        }}
    };
}

QJsonObject SettingsStore::merge(const QJsonObject &stored)
{
    auto out = defaults();
    for (auto it = stored.begin(); it != stored.end(); ++it) {
        if (it.key() == QLatin1String("clients"))
            out.insert(it.key(), clientsCsvForSetting(it.value().toString()));
        else
            out.insert(it.key(), it.value());
    }
    return out;
}

QJsonObject SettingsStore::load()
{
    return merge(readJsonObject(Paths::settingsPath()));
}

bool SettingsStore::save(const QJsonObject &settings)
{
    auto stripped = settings;
    const QStringList credKeys{
        QStringLiteral("hubHostSecret"), QStringLiteral("secret"), QStringLiteral("claudeWebCookie"),
        QStringLiteral("opencodeCookie"), QStringLiteral("deepseekApiKey"), QStringLiteral("minimaxApiKey"),
        QStringLiteral("copilotApiToken"), QStringLiteral("zaiApiKey"), QStringLiteral("zaiTeamApiKey"),
        QStringLiteral("volcengineAccessKeyId"), QStringLiteral("volcengineSecretAccessKey"),
        QStringLiteral("alibabaCookie"), QStringLiteral("qoderCookie"), QStringLiteral("traeAccessToken"),
        QStringLiteral("zedCookie"), QStringLiteral("commandcodeCookie"), QStringLiteral("kimiApiKey"),
        QStringLiteral("kimiWebAccessToken"), QStringLiteral("ollamaCookie"), QStringLiteral("openrouterProfiles"),
        QStringLiteral("thirdPartyProfiles"), QStringLiteral("opencodeProfiles"),
        QStringLiteral("volcengineAgentAccessKeyId"), QStringLiteral("volcengineAgentSecretAccessKey"),
        QStringLiteral("traeDeviceId"), QStringLiteral("zaiTeamOrganizationId"), QStringLiteral("zaiTeamProjectId")
    };
    for (const auto &key : credKeys) stripped.remove(key);
    return writeJsonAtomic(Paths::settingsPath(), stripped);
}

QVariantMap SettingsStore::toVariant(const QJsonObject &settings)
{
    return settings.toVariantMap();
}

} // namespace tmon
