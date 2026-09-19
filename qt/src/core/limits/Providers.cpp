#include "core/limits/Providers.h"

#include "core/catalog/Catalog.h"
#include "core/io/HashKey.h"
#include "core/io/JsonIo.h"
#include "core/io/Paths.h"
#include "core/limits/LimitsCore.h"
#include "core/limits/SpendStore.h"
#include "core/limits/ZcodeDiscovery.h"
#include "core/net/HttpClient.h"
#include "core/process/Subprocess.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QTimeZone>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <cmath>
#include <optional>

namespace tmon {
namespace {

QString settingOrEnv(const QJsonObject &settings, const QString &key, const QStringList &envNames)
{
    const auto explicitValue = cleanSecret(settings.value(key).toString());
    if (!explicitValue.isEmpty()) return explicitValue;
    for (const auto &name : envNames) {
        const auto v = cleanSecret(qEnvironmentVariable(name.toUtf8().constData()));
        if (!v.isEmpty()) return v;
    }
    return {};
}

QJsonObject windowPct(const QString &kind, const QString &label, double usedPercent, const QString &resetsAt = {})
{
    const double used = std::isfinite(usedPercent) ? qBound(0.0, usedPercent, 100.0) : 0;
    QJsonObject w{
        {QStringLiteral("kind"), kind},
        {QStringLiteral("label"), label},
        {QStringLiteral("usedPercent"), used},
        {QStringLiteral("remainingPercent"), 100.0 - used},
        {QStringLiteral("showMeter"), true}
    };
    if (!resetsAt.isEmpty()) w.insert(QStringLiteral("resetsAt"), resetsAt);
    return w;
}

QJsonObject windowUsedLimit(const QString &kind, const QString &label, double used, double limit)
{
    return QJsonObject{
        {QStringLiteral("kind"), kind},
        {QStringLiteral("label"), label},
        {QStringLiteral("used"), used},
        {QStringLiteral("limit"), limit},
        {QStringLiteral("remaining"), qMax(0.0, limit - used)},
        {QStringLiteral("showMeter"), true}
    };
}

QJsonObject fetchDeepseek(const QJsonObject &settings, HttpClient &http, SpendStore &spend)
{
    const auto key = settingOrEnv(settings, QStringLiteral("deepseekApiKey"),
                                  {QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("DEEPSEEK_KEY")});
    if (key.isEmpty()) return notConfiguredProvider(QStringLiteral("deepseek"));
    auto res = http.get(QUrl(QStringLiteral("https://api.deepseek.com/user/balance")),
                        {{QStringLiteral("Authorization"), QStringLiteral("Bearer ") + key},
                         {QStringLiteral("Accept"), QStringLiteral("application/json")}});
    if (res.status != 200) return errorProvider(QStringLiteral("deepseek"), QStringLiteral("api"), statusForHttp(res.status), hashKey(QStringLiteral("deepseek"), key));
    const auto infos = res.json().object().value(QStringLiteral("balance_infos")).toArray();
    QJsonObject best;
    double bestAmt = -1;
    for (const auto &rowV : infos) {
        const auto row = rowV.toObject();
        const double amount = row.value(QStringLiteral("total_balance")).toDouble();
        const auto currency = row.value(QStringLiteral("currency")).toString().toUpper();
        if (amount > bestAmt) {
            bestAmt = amount;
            best = row;
            best.insert(QStringLiteral("currency"), currency);
        }
    }
    if (best.isEmpty()) return errorProvider(QStringLiteral("deepseek"), QStringLiteral("api"), QStringLiteral("unavailable"), hashKey(QStringLiteral("deepseek"), key));
    const auto accountKey = hashKey(QStringLiteral("deepseek"), key);
    const auto spendWin = spend.record(QStringLiteral("deepseek"), accountKey,
                                       best.value(QStringLiteral("currency")).toString(),
                                       best.value(QStringLiteral("topped_up_balance")).toDouble(),
                                       best.value(QStringLiteral("total_balance")).toDouble());
    return normalizeLimitProvider(QJsonObject{
        {QStringLiteral("provider"), QStringLiteral("deepseek")},
        {QStringLiteral("accountKey"), accountKey},
        {QStringLiteral("accountLabel"), QStringLiteral("Pay-as-you-go")},
        {QStringLiteral("source"), QStringLiteral("api")},
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("balance"), QJsonObject{
            {QStringLiteral("amount"), best.value(QStringLiteral("total_balance"))},
            {QStringLiteral("currency"), best.value(QStringLiteral("currency"))}
        }},
        {QStringLiteral("windows"), QJsonArray{
            QJsonObject{
                {QStringLiteral("kind"), QStringLiteral("billing")},
                {QStringLiteral("metric"), QStringLiteral("credits")},
                {QStringLiteral("label"), QStringLiteral("Balance")},
                {QStringLiteral("remaining"), best.value(QStringLiteral("total_balance"))},
                {QStringLiteral("currency"), best.value(QStringLiteral("currency"))}
            },
            spendWin
        }}
    });
}

QJsonObject fetchOpenrouter(const QJsonObject &settings, HttpClient &http)
{
    QString key = settingOrEnv(settings, QStringLiteral("openrouterApiKey"),
                               {QStringLiteral("TOKEN_MONITOR_OPENROUTER_API_KEY"), QStringLiteral("OPENROUTER_API_KEY")});
    const auto profiles = settings.value(QStringLiteral("openrouterProfiles"));
    if (key.isEmpty() && profiles.isObject()) {
        const auto obj = profiles.toObject();
        if (!obj.isEmpty()) key = cleanSecret(obj.begin().value().toObject().value(QStringLiteral("apiKey")).toString());
    }
    if (key.isEmpty()) return notConfiguredProvider(QStringLiteral("openrouter"));
    auto headers = QMap<QString, QString>{
        {QStringLiteral("Authorization"), QStringLiteral("Bearer ") + key},
        {QStringLiteral("Accept"), QStringLiteral("application/json")},
        {QStringLiteral("HTTP-Referer"), QStringLiteral("https://github.com/Javis603/token-monitor")},
        {QStringLiteral("X-OpenRouter-Title"), QStringLiteral("Token Monitor")}
    };
    const auto keyRes = http.get(QUrl(QStringLiteral("https://openrouter.ai/api/v1/key")), headers);
    if (keyRes.status != 200) return errorProvider(QStringLiteral("openrouter"), QStringLiteral("api"), statusForHttp(keyRes.status), hashKey(QStringLiteral("openrouter"), key));
    const auto data = keyRes.json().object().value(QStringLiteral("data")).toObject();
    QJsonArray windows;
    const double limit = data.value(QStringLiteral("limit")).toDouble();
    const double used = data.value(QStringLiteral("usage")).toDouble();
    if (limit > 0) windows.append(windowUsedLimit(QStringLiteral("billing"), QStringLiteral("API key limit"), used, limit));
    const auto credits = http.get(QUrl(QStringLiteral("https://openrouter.ai/api/v1/credits")), headers);
    if (credits.status == 200) {
        const auto c = credits.json().object().value(QStringLiteral("data")).toObject();
        const double total = c.value(QStringLiteral("total_credits")).toDouble();
        const double usage = c.value(QStringLiteral("total_usage")).toDouble();
        if (total >= 0) {
            windows.append(QJsonObject{
                {QStringLiteral("kind"), QStringLiteral("billing")},
                {QStringLiteral("metric"), QStringLiteral("credits")},
                {QStringLiteral("label"), QStringLiteral("Credits")},
                {QStringLiteral("remaining"), qMax(0.0, total - usage)},
                {QStringLiteral("currency"), QStringLiteral("USD")}
            });
        }
    }
    return normalizeLimitProvider(QJsonObject{
        {QStringLiteral("provider"), QStringLiteral("openrouter")},
        {QStringLiteral("accountKey"), hashKey(QStringLiteral("openrouter"), key)},
        {QStringLiteral("source"), QStringLiteral("api")},
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("windows"), windows}
    });
}

QJsonObject fetchMinimax(const QJsonObject &settings, HttpClient &http)
{
    const auto key = settingOrEnv(settings, QStringLiteral("minimaxApiKey"), {QStringLiteral("MINIMAX_CODING_API_KEY")});
    if (key.isEmpty()) return notConfiguredProvider(QStringLiteral("minimax"));
    const auto headers = QMap<QString, QString>{{QStringLiteral("Authorization"), QStringLiteral("Bearer ") + key}};
    auto res = http.get(QUrl(QStringLiteral("https://api.minimaxi.com/v1/api/openplatform/coding_plan/remains")), headers);
    if (res.status != 200)
        res = http.get(QUrl(QStringLiteral("https://api.minimax.io/v1/api/openplatform/coding_plan/remains")), headers);
    if (res.status != 200) return errorProvider(QStringLiteral("minimax"), QStringLiteral("api"), statusForHttp(res.status), hashKey(QStringLiteral("minimax"), key));
    auto body = res.json().object();
    auto rows = body.value(QStringLiteral("data")).toObject().value(QStringLiteral("model_remains")).toArray();
    if (rows.isEmpty()) rows = body.value(QStringLiteral("model_remains")).toArray();
    QJsonObject general;
    for (const auto &row : rows) {
        if (row.toObject().value(QStringLiteral("model_name")).toString() == QLatin1String("general"))
            general = row.toObject();
    }
    QJsonArray windows;
    auto addLane = [&](const QString &kind, const QString &label, const QString &pctKey) {
        const double pct = general.value(pctKey).toString().toDouble();
        if (general.value(QStringLiteral("status")).toInt() == 3) return;
        if (pct > 0 || general.contains(pctKey)) windows.append(windowPct(kind, label, pct));
    };
    addLane(QStringLiteral("session"), QStringLiteral("5h"), QStringLiteral("current_interval_usage_percent"));
    addLane(QStringLiteral("weekly"), QStringLiteral("Weekly"), QStringLiteral("current_weekly_usage_percent"));
    return normalizeLimitProvider(QJsonObject{
        {QStringLiteral("provider"), QStringLiteral("minimax")},
        {QStringLiteral("accountKey"), hashKey(QStringLiteral("minimax"), key)},
        {QStringLiteral("source"), QStringLiteral("api")},
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("windows"), windows}
    });
}

QJsonObject fetchZai(const QJsonObject &settings, HttpClient &http)
{
    // Electron lanes (src/shared/providers/zai/limits.js): the console-key
    // quota lane and the local ZCode-desktop login lane run independently and
    // merge; a ZCode-only row reports oauth. The ZCode lane reads ~/.zcode/v2
    // (mirror JWT) — never drives a browser.
    const auto key = settingOrEnv(settings, QStringLiteral("zaiApiKey"),
                                  {QStringLiteral("ZAI_API_KEY"), QStringLiteral("Z_AI_API_KEY"),
                                   QStringLiteral("GLM_API_KEY"), QStringLiteral("ZHIPU_API_KEY")});
    const auto region = settings.value(QStringLiteral("zaiApiRegion")).toString(QStringLiteral("global"));

    QString keyError;
    QJsonArray keyWindows;
    QString keyPlan;
    if (!key.isEmpty()) {
        const auto base = region == QLatin1String("bigmodel-cn")
            ? QStringLiteral("https://open.bigmodel.cn")
            : QStringLiteral("https://api.z.ai");
        const auto headers = QMap<QString, QString>{{QStringLiteral("Authorization"), QStringLiteral("Bearer ") + key}};
        const auto res = http.get(QUrl(base + QStringLiteral("/api/monitor/usage/quota/limit")), headers, 12000);
        if (res.status != 200) {
            keyError = statusForHttp(res.status);
        } else {
            const auto usage = parseZaiUsage(res.json().object());
            keyWindows = usage.value(QStringLiteral("windows")).toArray();
            keyPlan = usage.value(QStringLiteral("plan")).toString();
        }
    }

    QJsonArray planWindows;
    QString planLabel;
    QString planError;
    bool planAttempted = false;
    const auto discovery = discoverZcodeConnection();
    const auto mirrorKey = discovery.mirrorKey;
    if ((discovery.kind == QLatin1String("coding-quota") || discovery.kind == QLatin1String("start-billing"))
        && discovery.entitled && !mirrorKey.isEmpty()) {
        planAttempted = true;
        // Quota rides the console-key endpoint with the mirror token; billing
        // is account-level on ZCode's own endpoint and never blocks quota.
        if (discovery.kind == QLatin1String("coding-quota")) {
            const auto mirrorRegion = discovery.family == QLatin1String("bigmodel")
                ? QStringLiteral("bigmodel-cn") : QStringLiteral("global");
            if (!(mirrorKey == key && mirrorRegion == region)) {
                const auto mirrorBase = mirrorRegion == QLatin1String("bigmodel-cn")
                    ? QStringLiteral("https://open.bigmodel.cn")
                    : QStringLiteral("https://api.z.ai");
                const auto headers = QMap<QString, QString>{
                    {QStringLiteral("Authorization"), QStringLiteral("Bearer ") + mirrorKey},
                    {QStringLiteral("Accept"), QStringLiteral("application/json")}
                };
                const auto res = http.get(QUrl(mirrorBase + QStringLiteral("/api/monitor/usage/quota/limit")), headers, 12000);
                if (res.status != 200) {
                    if (key.isEmpty()) planError = QStringLiteral("unavailable");
                } else {
                    const auto usage = parseZaiUsage(res.json().object());
                    const auto ws = usage.value(QStringLiteral("windows")).toArray();
                    for (const auto &w : ws) planWindows.append(w);
                    if (planLabel.isEmpty()) planLabel = usage.value(QStringLiteral("plan")).toString();
                }
            }
        }
        const auto billingKey = discovery.billingKey.isEmpty() ? mirrorKey : discovery.billingKey;
        QMap<QString, QString> billingHeaders{
            {QStringLiteral("Authorization"), QStringLiteral("Bearer ") + billingKey},
            {QStringLiteral("Accept"), QStringLiteral("application/json")}
        };
        if (!discovery.deviceMid.isEmpty())
            billingHeaders.insert(QStringLiteral("X-Device-Mid"), discovery.deviceMid);
        const auto billing = http.get(QUrl(QStringLiteral("https://zcode.z.ai/api/v1/zcode-plan/billing/balance")),
                                      billingHeaders, 12000);
        if (billing.status != 200) {
            // Mirror tokens are ZCode-managed and rotate there; auth failures
            // degrade to unavailable rather than contradicting the login.
            if (planWindows.isEmpty() && key.isEmpty()) planError = QStringLiteral("unavailable");
        } else {
            const auto usage = parseZcodeStartPlanBalances(billing.json().object());
            const auto ws = usage.value(QStringLiteral("windows")).toArray();
            for (const auto &w : ws) planWindows.append(w);
            if (planLabel.isEmpty()) planLabel = usage.value(QStringLiteral("plan")).toString();
        }
    }

    QJsonArray windows = keyWindows;
    for (const auto &w : planWindows) windows.append(w);
    const bool hasAnything = !windows.isEmpty();
    const auto accountKey = !key.isEmpty() ? hashKey(QStringLiteral("zai"), key)
        : (!mirrorKey.isEmpty() ? hashKey(QStringLiteral("zai"), mirrorKey) : QString());
    const auto plan = !keyPlan.isEmpty() ? keyPlan : planLabel;
    const auto source = !key.isEmpty() ? QStringLiteral("api")
        : (hasAnything || !planError.isEmpty() || planAttempted ? QStringLiteral("oauth") : QString());
    QString status;
    if (!keyError.isEmpty()) status = keyError;
    else if (!planError.isEmpty()) status = planError;
    else if (hasAnything) status = QStringLiteral("ok");
    else if (!key.isEmpty() || planAttempted) status = QStringLiteral("unavailable");
    else status = QStringLiteral("notConfigured");

    return normalizeLimitProvider(QJsonObject{
        {QStringLiteral("provider"), QStringLiteral("zai")},
        {QStringLiteral("accountKey"), accountKey},
        {QStringLiteral("accountLabel"), plan},
        {QStringLiteral("source"), source},
        {QStringLiteral("status"), status},
        {QStringLiteral("windows"), windows},
        {QStringLiteral("region"), region}
    });
}

QJsonObject fetchZaiTeam(const QJsonObject &settings, HttpClient &http)
{
    const auto key = settingOrEnv(settings, QStringLiteral("zaiTeamApiKey"),
                                  {QStringLiteral("ZAI_TEAM_API_KEY"), QStringLiteral("BIGMODEL_TEAM_API_KEY")});
    if (key.isEmpty()) return notConfiguredProvider(QStringLiteral("zaiteam"));
    QMap<QString, QString> headers{{QStringLiteral("Authorization"), QStringLiteral("Bearer ") + key}};
    const auto org = cleanSecret(settings.value(QStringLiteral("zaiTeamOrganizationId")).toString());
    const auto proj = cleanSecret(settings.value(QStringLiteral("zaiTeamProjectId")).toString());
    if (!org.isEmpty()) headers.insert(QStringLiteral("X-Organization-Id"), org);
    if (!proj.isEmpty()) headers.insert(QStringLiteral("X-Project-Id"), proj);
    const auto res = http.get(QUrl(QStringLiteral("https://open.bigmodel.cn/api/monitor/usage/quota/limit?type=2")), headers);
    if (res.status != 200) return errorProvider(QStringLiteral("zaiteam"), QStringLiteral("api"), statusForHttp(res.status), hashKey(QStringLiteral("zaiteam"), key));
    return normalizeLimitProvider(QJsonObject{
        {QStringLiteral("provider"), QStringLiteral("zaiteam")},
        {QStringLiteral("accountKey"), hashKey(QStringLiteral("zaiteam"), key)},
        {QStringLiteral("source"), QStringLiteral("api")},
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("windows"), QJsonArray{}}
    });
}

QJsonObject fetchKimi(const QJsonObject &settings, HttpClient &http)
{
    const auto key = settingOrEnv(settings, QStringLiteral("kimiApiKey"), {QStringLiteral("KIMI_CODE_API_KEY")});
    const auto web = settingOrEnv(settings, QStringLiteral("kimiWebAccessToken"),
                                  {QStringLiteral("KIMI_AUTH_TOKEN"), QStringLiteral("KIMI_MANUAL_COOKIE")});
    if (key.isEmpty() && web.isEmpty()) return notConfiguredProvider(QStringLiteral("kimi"));
    QJsonArray windows;
    QString accountKey;
    if (!key.isEmpty()) {
        accountKey = hashKey(QStringLiteral("kimi"), key);
        const auto res = http.get(QUrl(QStringLiteral("https://api.kimi.com/coding/v1/usages")),
                                  {{QStringLiteral("Authorization"), QStringLiteral("Bearer ") + key}});
        if (res.status == 200) {
            const auto body = res.json().object();
            const auto usage = body.value(QStringLiteral("usage")).toObject();
            windows.append(windowPct(QStringLiteral("weekly"), QStringLiteral("Weekly"),
                                     usage.value(QStringLiteral("used_percent")).toDouble(usage.value(QStringLiteral("percentage")).toDouble())));
        }
    }
    if (!web.isEmpty()) {
        if (accountKey.isEmpty()) accountKey = hashKey(QStringLiteral("kimi-web"), web);
        http.get(QUrl(QStringLiteral("https://www.kimi.com/apiv2/kimi.gateway.billing.v1.BillingService/GetUsages")),
                 {{QStringLiteral("Authorization"), web}});
    }
    return normalizeLimitProvider(QJsonObject{
        {QStringLiteral("provider"), QStringLiteral("kimi")},
        {QStringLiteral("accountKey"), accountKey},
        {QStringLiteral("source"), QStringLiteral("api")},
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("windows"), windows}
    });
}

QJsonObject fetchTrae(const QJsonObject &settings, HttpClient &http)
{
    auto token = settingOrEnv(settings, QStringLiteral("traeAccessToken"),
                              {QStringLiteral("TOKEN_MONITOR_TRAE_ACCESS_TOKEN"), QStringLiteral("TRAE_ACCESS_TOKEN")});
    token.remove(QRegularExpression(QStringLiteral("^authorization\\s*:\\s*"), QRegularExpression::CaseInsensitiveOption));
    if (token.isEmpty()) return notConfiguredProvider(QStringLiteral("trae"));
    const auto device = settingOrEnv(settings, QStringLiteral("traeDeviceId"), {QStringLiteral("TRAE_DEVICE_ID")});
    QMap<QString, QString> headers{{QStringLiteral("x-cloud-ide-jwt"), token}};
    if (!device.isEmpty()) headers.insert(QStringLiteral("x-device-id"), device);
    const auto res = http.get(QUrl(QStringLiteral("https://api.trae.cn/trae/api/v2/pay/ide_user_ent_usage")), headers);
    if (res.status != 200) return errorProvider(QStringLiteral("trae"), QStringLiteral("api"), statusForHttp(res.status), hashKey(QStringLiteral("trae"), token));
    const auto data = res.json().object().value(QStringLiteral("data")).toObject();
    QJsonArray windows;
    windows.append(QJsonObject{
        {QStringLiteral("kind"), QStringLiteral("billing")},
        {QStringLiteral("metric"), QStringLiteral("credits")},
        {QStringLiteral("label"), QStringLiteral("Credits")},
        {QStringLiteral("remaining"), data.value(QStringLiteral("remain_credit")).toDouble(data.value(QStringLiteral("remainCredit")).toDouble())},
        {QStringLiteral("currency"), QStringLiteral("CNY")}
    });
    return normalizeLimitProvider(QJsonObject{
        {QStringLiteral("provider"), QStringLiteral("trae")},
        {QStringLiteral("accountKey"), hashKey(QStringLiteral("trae"), token)},
        {QStringLiteral("source"), QStringLiteral("api")},
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("windows"), windows}
    });
}

QJsonObject fetchCookie(const QString &id, const QString &url, const QString &cookieKey, const QJsonObject &settings, HttpClient &http)
{
    const auto cookie = cleanSecret(settings.value(cookieKey).toString());
    if (cookie.isEmpty()) return notConfiguredProvider(id, QStringLiteral("web"));
    const auto res = http.get(QUrl(url), {{QStringLiteral("Cookie"), cookie}, {QStringLiteral("Accept"), QStringLiteral("application/json")}});
    if (res.status != 200) return errorProvider(id, QStringLiteral("web"), statusForHttp(res.status), hashKey(id, cookie));
    return normalizeLimitProvider(QJsonObject{
        {QStringLiteral("provider"), id},
        {QStringLiteral("accountKey"), hashKey(id, cookie)},
        {QStringLiteral("source"), QStringLiteral("web")},
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("windows"), QJsonArray{}}
    });
}

QJsonObject fetchCodex(const QJsonObject &, HttpClient &http)
{
    const auto authPath = QDir(Paths::envOr(QStringLiteral("CODEX_HOME"), QDir(Paths::homeDir()).filePath(QStringLiteral(".codex")))).filePath(QStringLiteral("auth.json"));
    const auto auth = readJsonObject(authPath);
    const auto tokens = auth.value(QStringLiteral("tokens")).toObject();
    auto access = tokens.value(QStringLiteral("access_token")).toString();
    if (access.isEmpty()) access = auth.value(QStringLiteral("access_token")).toString();
    if (access.isEmpty()) return notConfiguredProvider(QStringLiteral("codex"), QStringLiteral("oauth"));
    const auto res = http.get(QUrl(QStringLiteral("https://chatgpt.com/backend-api/wham/usage")),
                              {{QStringLiteral("Authorization"), QStringLiteral("Bearer ") + access},
                               {QStringLiteral("Accept"), QStringLiteral("application/json")}});
    if (res.status != 200) return errorProvider(QStringLiteral("codex"), QStringLiteral("oauth"), statusForHttp(res.status), hashKey(QStringLiteral("codex"), access.left(12)));
    const auto body = res.json().object();
    QJsonArray windows;
    const auto rate = body.value(QStringLiteral("rate_limit")).toObject();
    auto add = [&](const QString &kind, const QString &label, const QJsonObject &node) {
        if (node.isEmpty()) return;
        const double used = node.value(QStringLiteral("used_percent")).toDouble();
        windows.append(windowPct(kind, label, used, node.value(QStringLiteral("reset_at")).toString()));
    };
    add(QStringLiteral("session"), QStringLiteral("5h"), rate.value(QStringLiteral("primary_window")).toObject());
    add(QStringLiteral("weekly"), QStringLiteral("Weekly"), rate.value(QStringLiteral("secondary_window")).toObject());
    return normalizeLimitProvider(QJsonObject{
        {QStringLiteral("provider"), QStringLiteral("codex")},
        {QStringLiteral("accountKey"), hashKey(QStringLiteral("codex"), access.left(24))},
        {QStringLiteral("source"), QStringLiteral("oauth")},
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("windows"), windows}
    });
}

QJsonObject fetchClaude(const QJsonObject &settings, HttpClient &http)
{
    const auto cookie = cleanSecret(settings.value(QStringLiteral("claudeWebCookie")).toString());
    const auto credPath = QDir(Paths::homeDir()).filePath(QStringLiteral(".claude/.credentials.json"));
    const auto creds = readJsonObject(credPath);
    QString token = creds.value(QStringLiteral("claudeAiOauth")).toObject().value(QStringLiteral("accessToken")).toString();
    if (token.isEmpty()) token = creds.value(QStringLiteral("accessToken")).toString();
    if (token.isEmpty() && cookie.isEmpty()) return notConfiguredProvider(QStringLiteral("claude"), QStringLiteral("oauth"));
    if (!token.isEmpty()) {
        const auto res = http.get(QUrl(QStringLiteral("https://api.anthropic.com/api/oauth/usage")),
                                  {{QStringLiteral("Authorization"), QStringLiteral("Bearer ") + token}});
        if (res.status == 200) {
            return normalizeLimitProvider(QJsonObject{
                {QStringLiteral("provider"), QStringLiteral("claude")},
                {QStringLiteral("accountKey"), hashKey(QStringLiteral("claude"), token.left(24))},
                {QStringLiteral("source"), QStringLiteral("oauth")},
                {QStringLiteral("status"), QStringLiteral("ok")},
                {QStringLiteral("windows"), QJsonArray{}}
            });
        }
    }
    if (!cookie.isEmpty()) {
        return fetchCookie(QStringLiteral("claude"), QStringLiteral("https://claude.ai/api/organizations"),
                           QStringLiteral("claudeWebCookie"), settings, http);
    }
    return notConfiguredProvider(QStringLiteral("claude"), QStringLiteral("oauth"));
}

constexpr auto kBrowserUa = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36";

std::optional<double> jsonNumber(const QJsonValue &value)
{
    if (value.isDouble()) return value.toDouble();
    if (value.isString()) {
        bool ok = false;
        const double n = value.toString().toDouble(&ok);
        if (ok) return n;
    }
    return std::nullopt;
}

std::optional<double> clampPercent(std::optional<double> n)
{
    if (!n || !std::isfinite(*n)) return std::nullopt;
    if (*n < 0) return 0.0;
    if (*n > 100) return 100.0;
    return *n;
}

bool jsonBool(const QJsonValue &value, bool fallback = false)
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

QString isoTimestamp(const QJsonValue &value)
{
    if (value.isString()) {
        const auto text = value.toString().trimmed();
        if (text.isEmpty()) return {};
        QDateTime at = QDateTime::fromString(text, Qt::ISODateWithMs);
        if (!at.isValid()) at = QDateTime::fromString(text, Qt::ISODate);
        if (at.isValid()) return at.toUTC().toString(Qt::ISODateWithMs);
        bool ok = false;
        const double n = text.toDouble(&ok);
        if (ok) return isoTimestamp(n);
        return text;
    }
    if (value.isDouble()) {
        double n = value.toDouble();
        if (!std::isfinite(n)) return {};
        if (n < 1e12) n *= 1000.0;
        return QDateTime::fromMSecsSinceEpoch(qint64(n), QTimeZone::UTC).toString(Qt::ISODateWithMs);
    }
    return {};
}

double centsToUsd(double cents)
{
    return std::round(cents) / 100.0;
}

QString normalizeCursorSessionToken(QString token)
{
    token = token.trimmed();
    if (token.isEmpty() || token.size() > 16 * 1024) return {};
    if (token.startsWith(QLatin1String("cookie:"), Qt::CaseInsensitive))
        token = token.mid(7).trimmed();
    static const QRegularExpression cookieRe(QStringLiteral("WorkosCursorSessionToken=([^;\\s]+)"),
                                             QRegularExpression::CaseInsensitiveOption);
    const auto cookieMatch = cookieRe.match(token);
    if (cookieMatch.hasMatch()) token = cookieMatch.captured(1);
    if ((token.startsWith(QLatin1Char('"')) && token.endsWith(QLatin1Char('"')))
        || (token.startsWith(QLatin1Char('\'')) && token.endsWith(QLatin1Char('\'')))) {
        token = token.mid(1, token.size() - 2).trimmed();
    }
    if (token.isEmpty() || token.contains(QRegularExpression(QStringLiteral("\\s")))) return {};
    const int sep = token.indexOf(QStringLiteral("::"));
    if (sep > 0)
        token = token.left(sep) + QStringLiteral("%3A%3A") + token.mid(sep + 2);
    return token;
}

QString canonicalCursorUserId(const QString &value)
{
    static const QRegularExpression re(QStringLiteral("user_[A-Za-z0-9_]+"));
    const auto m = re.match(value);
    return m.hasMatch() ? m.captured(0) : QString();
}

QString readCursorDesktopAccessToken()
{
    const QStringList candidates{
        QDir(Paths::appDataRoaming()).filePath(QStringLiteral("Cursor/User/globalStorage/state.vscdb")),
        QDir(Paths::homeDir()).filePath(QStringLiteral("AppData/Roaming/Cursor/User/globalStorage/state.vscdb"))
    };
    QString dbPath;
    for (const auto &path : candidates) {
        if (QFileInfo::exists(path)) {
            dbPath = path;
            break;
        }
    }
    if (dbPath.isEmpty()) return {};
    const auto conn = QStringLiteral("tmon-cursor-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString token;
    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(dbPath);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (db.open()) {
            QSqlQuery q(db);
            q.prepare(QStringLiteral("SELECT value FROM ItemTable WHERE key = ?"));
            q.addBindValue(QStringLiteral("cursorAuth/accessToken"));
            if (q.exec() && q.next()) token = q.value(0).toString().trimmed();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(conn);
    return token;
}

struct CursorAccount {
    QString id;
    QString sessionToken;
    QString userId;
    QString label;
};

QList<CursorAccount> listCursorAccounts()
{
    QList<CursorAccount> out;
    const auto credPath = QDir(Paths::homeDir()).filePath(QStringLiteral(".config/tokscale/cursor-credentials.json"));
    const auto store = readJsonObject(credPath);
    const auto accounts = store.value(QStringLiteral("accounts")).toObject();
    const auto active = store.value(QStringLiteral("activeAccountId")).toString();
    for (auto it = accounts.begin(); it != accounts.end(); ++it) {
        const auto obj = it.value().toObject();
        const auto token = normalizeCursorSessionToken(obj.value(QStringLiteral("sessionToken")).toString());
        if (token.isEmpty()) continue;
        CursorAccount acct;
        acct.id = it.key();
        acct.sessionToken = token;
        acct.userId = obj.value(QStringLiteral("userId")).toString();
        acct.label = obj.value(QStringLiteral("label")).toString();
        if (acct.id == active) out.prepend(acct);
        else out.append(acct);
    }
    if (out.isEmpty()) {
        const auto desktop = normalizeCursorSessionToken(readCursorDesktopAccessToken());
        if (!desktop.isEmpty()) {
            CursorAccount acct;
            acct.sessionToken = desktop;
            acct.userId = canonicalCursorUserId(desktop);
            acct.id = acct.userId.isEmpty() ? QStringLiteral("desktop") : acct.userId;
            out.append(acct);
        }
    }
    return out;
}

QMap<QString, QString> cursorHeaders(const QString &sessionToken)
{
    return {
        {QStringLiteral("Accept"), QStringLiteral("*/*")},
        {QStringLiteral("Accept-Language"), QStringLiteral("en-US,en;q=0.9")},
        {QStringLiteral("Referer"), QStringLiteral("https://cursor.com/dashboard")},
        {QStringLiteral("User-Agent"), QString::fromLatin1(kBrowserUa)},
        {QStringLiteral("Cookie"), QStringLiteral("WorkosCursorSessionToken=") + sessionToken}
    };
}

QString formatCursorMembership(QString type)
{
    type = type.trimmed();
    if (type.isEmpty()) return {};
    if (type.compare(QLatin1String("pro+"), Qt::CaseInsensitive) == 0
        || type.compare(QLatin1String("pro_plus"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("Pro+");
    if (!type.isEmpty()) type[0] = type[0].toUpper();
    return type;
}

QJsonObject fetchCursorAccount(const CursorAccount &account, HttpClient &http)
{
    const auto headers = cursorHeaders(account.sessionToken);
    const auto usageRes = http.get(QUrl(QStringLiteral("https://cursor.com/api/usage-summary")), headers);
    if (usageRes.status == 401 || usageRes.status == 403)
        return errorProvider(QStringLiteral("cursor"), QStringLiteral("web"), QStringLiteral("unauthorized"),
                             hashKey(QStringLiteral("cursor"), account.id.isEmpty() ? QStringLiteral("unknown") : account.id));
    if (usageRes.status != 200)
        return errorProvider(QStringLiteral("cursor"), QStringLiteral("web"), statusForHttp(usageRes.status),
                             hashKey(QStringLiteral("cursor"), account.id.isEmpty() ? QStringLiteral("unknown") : account.id));
    const auto usageDoc = usageRes.json();
    if (!usageDoc.isObject())
        return errorProvider(QStringLiteral("cursor"), QStringLiteral("web"), QStringLiteral("unavailable"),
                             hashKey(QStringLiteral("cursor"), account.id.isEmpty() ? QStringLiteral("unknown") : account.id));
    const auto summary = usageDoc.object();
    const auto individual = summary.value(QStringLiteral("individualUsage")).toObject();
    const auto plan = individual.value(QStringLiteral("plan")).toObject();
    const auto onDemand = individual.value(QStringLiteral("onDemand")).toObject();
    const auto overall = individual.value(QStringLiteral("overall")).toObject();
    const auto team = summary.value(QStringLiteral("teamUsage")).toObject();
    const auto teamOnDemand = team.value(QStringLiteral("onDemand")).toObject();
    const auto teamPooled = team.value(QStringLiteral("pooled")).toObject();
    const auto autoPercent = clampPercent(jsonNumber(plan.value(QStringLiteral("autoPercentUsed"))));
    const auto apiPercent = clampPercent(jsonNumber(plan.value(QStringLiteral("apiPercentUsed"))));
    const auto billingEnd = isoTimestamp(summary.value(QStringLiteral("billingCycleEnd")));
    const auto membership = summary.value(QStringLiteral("membershipType")).toString();

    const auto userRes = http.get(QUrl(QStringLiteral("https://cursor.com/api/auth/me")), headers);
    QString email;
    QString sub;
    if (userRes.status == 200) {
        const auto user = userRes.json().object();
        email = user.value(QStringLiteral("email")).toString();
        sub = canonicalCursorUserId(user.value(QStringLiteral("sub")).toString());
    }

    QJsonArray windows;
    if (autoPercent)
        windows.append(windowPct(QStringLiteral("billing"), QStringLiteral("Cursor Models"), *autoPercent, billingEnd));
    if (apiPercent)
        windows.append(windowPct(QStringLiteral("billing"), QStringLiteral("Other Models"), *apiPercent, billingEnd));
    if (windows.isEmpty()) {
        auto planPercent = clampPercent(jsonNumber(plan.value(QStringLiteral("totalPercentUsed"))));
        if (!planPercent) {
            const auto used = jsonNumber(plan.value(QStringLiteral("used"))).value_or(0);
            const auto limit = jsonNumber(plan.value(QStringLiteral("limit"))).value_or(0);
            if (limit > 0) planPercent = qBound(0.0, 100.0 * used / limit, 100.0);
        }
        if (!planPercent) {
            const auto used = jsonNumber(overall.value(QStringLiteral("used")));
            const auto limit = jsonNumber(overall.value(QStringLiteral("limit")));
            if (used && limit && *limit > 0) planPercent = qBound(0.0, 100.0 * *used / *limit, 100.0);
        }
        if (planPercent)
            windows.append(windowPct(QStringLiteral("billing"), QStringLiteral("Overall"), *planPercent, billingEnd));
    }

    auto grokHeaders = headers;
    grokHeaders.insert(QStringLiteral("Accept"), QStringLiteral("application/json"));
    grokHeaders.insert(QStringLiteral("Content-Type"), QStringLiteral("application/json"));
    grokHeaders.insert(QStringLiteral("Origin"), QStringLiteral("https://cursor.com"));
    const auto grokRes = http.post(QUrl(QStringLiteral("https://cursor.com/api/dashboard/get-sand-usage-status")),
                                   QByteArrayLiteral("{}"), grokHeaders, 15000);
    if (grokRes.status == 200) {
        auto grok = grokRes.json().object();
        if (grok.contains(QStringLiteral("usage")) && grok.value(QStringLiteral("usage")).isObject())
            grok = grok.value(QStringLiteral("usage")).toObject();
        auto usedPercent = clampPercent(jsonNumber(grok.value(QStringLiteral("usagePercent"))));
        if (!usedPercent) usedPercent = clampPercent(jsonNumber(grok.value(QStringLiteral("percent"))));
        const bool included = jsonBool(grok.value(QStringLiteral("hasNonZeroIncludedLimit")))
            || (jsonNumber(grok.value(QStringLiteral("includedLimit"))).value_or(0) > 0);
        if (included && usedPercent.has_value()) {
            windows.append(windowPct(QStringLiteral("weekly"), QStringLiteral("Grok Bot"), *usedPercent,
                                     isoTimestamp(grok.value(QStringLiteral("nextResetTimestampUtc")))));
        } else if (included) {
            windows.append(windowPct(QStringLiteral("weekly"), QStringLiteral("Grok Bot"), 0.0,
                                     isoTimestamp(grok.value(QStringLiteral("nextResetTimestampUtc")))));
        }
    } else {
        qWarning() << "cursor grok-bot HTTP" << grokRes.status << grokRes.error;
    }

    const auto personalUsed = jsonNumber(onDemand.value(QStringLiteral("used"))).value_or(0);
    const auto personalLimit = jsonNumber(onDemand.value(QStringLiteral("limit")));
    const auto teamUsed = jsonNumber(teamOnDemand.value(QStringLiteral("used"))).value_or(0);
    const auto teamLimit = jsonNumber(teamOnDemand.value(QStringLiteral("limit")));
    double spendUsed = 0;
    std::optional<double> spendLimit;
    std::optional<double> spendRemain;
    if (personalLimit && *personalLimit > 0) {
        spendUsed = personalUsed;
        spendLimit = *personalLimit;
        spendRemain = jsonNumber(onDemand.value(QStringLiteral("remaining")));
    } else if (teamLimit && *teamLimit > 0) {
        spendUsed = teamUsed;
        spendLimit = *teamLimit;
        spendRemain = jsonNumber(teamOnDemand.value(QStringLiteral("remaining")));
    } else if (personalUsed > 0) {
        spendUsed = personalUsed;
    } else if (teamUsed > 0) {
        spendUsed = teamUsed;
    }
    if (spendLimit || spendUsed > 0) {
        const double usedUsd = centsToUsd(spendUsed);
        const double limitUsd = spendLimit ? centsToUsd(*spendLimit) : 0;
        double remainUsd = spendRemain ? centsToUsd(*spendRemain) : (spendLimit ? qMax(0.0, *spendLimit - spendUsed) / 100.0 : 0);
        QJsonObject spend{
            {QStringLiteral("kind"), QStringLiteral("billing")},
            {QStringLiteral("label"), QStringLiteral("On-demand spend")},
            {QStringLiteral("metric"), QStringLiteral("spend")},
            {QStringLiteral("currency"), QStringLiteral("USD")},
            {QStringLiteral("used"), usedUsd},
            {QStringLiteral("limit"), limitUsd},
            {QStringLiteral("remaining"), remainUsd},
            {QStringLiteral("showMeter"), false},
            {QStringLiteral("resetsAt"), billingEnd}
        };
        if (spendLimit && *spendLimit > 0)
            spend.insert(QStringLiteral("usedPercent"), 100.0 * spendUsed / *spendLimit);
        windows.append(spend);
    }

    const auto pooledUsed = jsonNumber(teamPooled.value(QStringLiteral("used")));
    const auto pooledLimit = jsonNumber(teamPooled.value(QStringLiteral("limit")));
    if ((pooledLimit && *pooledLimit > 0) || (pooledUsed && *pooledUsed > 0)) {
        const double usedUsd = centsToUsd(pooledUsed.value_or(0));
        const double limitUsd = centsToUsd(pooledLimit.value_or(0));
        auto pooledPct = clampPercent(jsonNumber(teamPooled.value(QStringLiteral("percent"))));
        if (!pooledPct && pooledLimit && *pooledLimit > 0)
            pooledPct = qBound(0.0, 100.0 * pooledUsed.value_or(0) / *pooledLimit, 100.0);
        QJsonObject pool{
            {QStringLiteral("kind"), QStringLiteral("billing")},
            {QStringLiteral("label"), QStringLiteral("Team pool")},
            {QStringLiteral("used"), usedUsd},
            {QStringLiteral("limit"), limitUsd},
            {QStringLiteral("remaining"), qMax(0.0, limitUsd - usedUsd)},
            {QStringLiteral("showMeter"), true},
            {QStringLiteral("resetsAt"), billingEnd}
        };
        if (pooledPct) {
            pool.insert(QStringLiteral("usedPercent"), *pooledPct);
            pool.insert(QStringLiteral("remainingPercent"), 100.0 - *pooledPct);
        }
        windows.append(pool);
    }

    const auto accountKey = !sub.isEmpty() ? hashKey(QStringLiteral("cursor"), sub)
                                           : hashKey(QStringLiteral("cursor-local"), account.id.isEmpty() ? QStringLiteral("unknown") : account.id);
    return normalizeLimitProvider(QJsonObject{
        {QStringLiteral("provider"), QStringLiteral("cursor")},
        {QStringLiteral("accountKey"), accountKey},
        {QStringLiteral("accountLabel"), email.isEmpty() ? account.label : email},
        {QStringLiteral("accountEmail"), email},
        {QStringLiteral("planLabel"), formatCursorMembership(membership)},
        {QStringLiteral("source"), QStringLiteral("web")},
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("windows"), windows}
    });
}

QJsonObject fetchCursor(const QJsonObject &settings, HttpClient &http)
{
    auto accounts = listCursorAccounts();
    QSet<QString> disabled;
    const auto disabledV = settings.value(QStringLiteral("cursorDisabledAccountIds"));
    if (disabledV.isArray()) {
        for (const auto &id : disabledV.toArray()) disabled.insert(id.toString().trimmed());
    } else {
        for (const auto &id : disabledV.toString().split(QLatin1Char(','), Qt::SkipEmptyParts))
            disabled.insert(id.trimmed());
    }
    QList<CursorAccount> enabled;
    for (const auto &acct : accounts) {
        if (!disabled.contains(acct.id)) enabled.append(acct);
    }
    if (enabled.isEmpty()) return notConfiguredProvider(QStringLiteral("cursor"), QStringLiteral("web"));
    return fetchCursorAccount(enabled.first(), http);
}

QJsonObject fetchCli(const QString &id, const QString &program, const QStringList &args)
{
    const auto run = runProcess(program, args, 20000);
    if (!run.started) return notConfiguredProvider(id, QStringLiteral("cli"));
    if (run.exitCode != 0) return errorProvider(id, QStringLiteral("cli"), QStringLiteral("unavailable"));
    return normalizeLimitProvider(QJsonObject{
        {QStringLiteral("provider"), id},
        {QStringLiteral("accountKey"), hashKey(id, QStringLiteral("local"))},
        {QStringLiteral("source"), QStringLiteral("cli")},
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("windows"), QJsonArray{}}
    });
}

QJsonObject fetchThirdParty(const QJsonObject &settings, HttpClient &http)
{
    const auto profiles = settings.value(QStringLiteral("thirdPartyProfiles"));
    QJsonArray list;
    if (profiles.isArray()) list = profiles.toArray();
    else if (profiles.isObject()) {
        for (auto it = profiles.toObject().begin(); it != profiles.toObject().end(); ++it) {
            auto obj = it.value().toObject();
            obj.insert(QStringLiteral("name"), it.key());
            list.append(obj);
        }
    }
    if (list.isEmpty()) return notConfiguredProvider(QStringLiteral("thirdparty"));
    QJsonArray results;
    for (const auto &itemV : list) {
        const auto item = itemV.toObject();
        const auto adapter = item.value(QStringLiteral("adapter")).toString(item.value(QStringLiteral("adapterId")).toString());
        const auto base = item.value(QStringLiteral("baseUrl")).toString();
        const auto key = cleanSecret(item.value(QStringLiteral("apiKey")).toString());
        if (base.isEmpty()) continue;
        QUrl url(base);
        if (adapter.contains(QLatin1String("newapi"))) url.setPath(url.path() + QStringLiteral("/api/user/self"));
        auto res = http.get(url, {{QStringLiteral("Authorization"), QStringLiteral("Bearer ") + key}});
        results.append(normalizeLimitProvider(QJsonObject{
            {QStringLiteral("provider"), QStringLiteral("thirdparty")},
            {QStringLiteral("accountKey"), hashKey(QStringLiteral("thirdparty"), key.isEmpty() ? base : key)},
            {QStringLiteral("accountLabel"), item.value(QStringLiteral("name")).toString()},
            {QStringLiteral("source"), QStringLiteral("api")},
            {QStringLiteral("status"), res.status == 200 ? QStringLiteral("ok") : statusForHttp(res.status)},
            {QStringLiteral("windows"), QJsonArray{}}
        }));
    }
    return results.isEmpty() ? notConfiguredProvider(QStringLiteral("thirdparty")) : results.first().toObject();
}

} // namespace

QJsonObject fetchLimitProvider(const QString &id, const QJsonObject &settings, HttpClient &http, SpendStore &spend)
{
    if (id == QLatin1String("deepseek")) return fetchDeepseek(settings, http, spend);
    if (id == QLatin1String("openrouter")) return fetchOpenrouter(settings, http);
    if (id == QLatin1String("minimax")) return fetchMinimax(settings, http);
    if (id == QLatin1String("zai")) return fetchZai(settings, http);
    if (id == QLatin1String("zaiteam")) return fetchZaiTeam(settings, http);
    if (id == QLatin1String("kimi")) return fetchKimi(settings, http);
    if (id == QLatin1String("trae")) return fetchTrae(settings, http);
    if (id == QLatin1String("codex")) return fetchCodex(settings, http);
    if (id == QLatin1String("claude")) return fetchClaude(settings, http);
    if (id == QLatin1String("cursor")) return fetchCursor(settings, http);
    if (id == QLatin1String("ollama"))
        return fetchCookie(id, QStringLiteral("https://ollama.com/api/user"), QStringLiteral("ollamaCookie"), settings, http);
    if (id == QLatin1String("alibaba"))
        return fetchCookie(id, QStringLiteral("https://bailian.console.aliyun.com/"), QStringLiteral("alibabaCookie"), settings, http);
    if (id == QLatin1String("qoder"))
        return fetchCookie(id, QStringLiteral("https://qoder.com/api/user"), QStringLiteral("qoderCookie"), settings, http);
    if (id == QLatin1String("zed"))
        return fetchCookie(id, QStringLiteral("https://zed.dev/api/billing"), QStringLiteral("zedCookie"), settings, http);
    if (id == QLatin1String("commandcode"))
        return fetchCookie(id, QStringLiteral("https://commandcode.ai/api/limits"), QStringLiteral("commandcodeCookie"), settings, http);
    if (id == QLatin1String("mimo"))
        return fetchCookie(id, QStringLiteral("https://api.xiaomimimo.com/"), QStringLiteral("mimoCookie"), settings, http);
    if (id == QLatin1String("kiro"))
        return fetchCli(id, QStringLiteral("kiro-cli"), {QStringLiteral("chat"), QStringLiteral("--no-interactive"), QStringLiteral("/usage")});
    if (id == QLatin1String("grok"))
        return fetchCli(id, QStringLiteral("grok"), {QStringLiteral("agent"), QStringLiteral("stdio")});
    if (id == QLatin1String("volcengine")) {
        const auto ak = cleanSecret(settings.value(QStringLiteral("volcengineAccessKeyId")).toString());
        if (ak.isEmpty()) return notConfiguredProvider(id);
        return fetchCli(id, QStringLiteral("arkcli"), {QStringLiteral("quota")});
    }
    if (id == QLatin1String("copilot")) {
        const auto token = cleanSecret(settings.value(QStringLiteral("copilotApiToken")).toString());
        if (token.isEmpty()) return notConfiguredProvider(id);
        const auto res = http.get(QUrl(QStringLiteral("https://api.github.com/copilot_internal/user")),
                                  {{QStringLiteral("Authorization"), QStringLiteral("token ") + token}});
        return normalizeLimitProvider(QJsonObject{
            {QStringLiteral("provider"), id},
            {QStringLiteral("accountKey"), hashKey(id, token.left(12))},
            {QStringLiteral("source"), QStringLiteral("api")},
            {QStringLiteral("status"), res.status == 200 ? QStringLiteral("ok") : statusForHttp(res.status)},
            {QStringLiteral("windows"), QJsonArray{}}
        });
    }
    if (id == QLatin1String("antigravity")) {
        const QString gemini = QDir(Paths::homeDir()).filePath(QStringLiteral(".gemini"));
        if (!QFileInfo::exists(gemini)) return notConfiguredProvider(id, QStringLiteral("oauth"));
        return normalizeLimitProvider(QJsonObject{
            {QStringLiteral("provider"), id},
            {QStringLiteral("accountKey"), hashKey(id, QStringLiteral("local"))},
            {QStringLiteral("source"), QStringLiteral("oauth")},
            {QStringLiteral("status"), QStringLiteral("ok")},
            {QStringLiteral("windows"), QJsonArray{}}
        });
    }
    if (id == QLatin1String("opencode")) {
        const auto cookie = cleanSecret(settings.value(QStringLiteral("opencodeCookie")).toString());
        if (cookie.isEmpty()) return notConfiguredProvider(id, QStringLiteral("web"));
        return fetchCookie(id, QStringLiteral("https://opencode.ai/api/session"), QStringLiteral("opencodeCookie"), settings, http);
    }
    if (id == QLatin1String("workbuddy")) {
        const auto db = QDir(Paths::homeDir()).filePath(QStringLiteral(".workbuddy/workbuddy.db"));
        if (!QFileInfo::exists(db)) return notConfiguredProvider(id, QStringLiteral("local"));
        return normalizeLimitProvider(QJsonObject{
            {QStringLiteral("provider"), id},
            {QStringLiteral("accountKey"), hashKey(id, QStringLiteral("local"))},
            {QStringLiteral("source"), QStringLiteral("local")},
            {QStringLiteral("status"), QStringLiteral("ok")},
            {QStringLiteral("windows"), QJsonArray{}}
        });
    }
    if (id == QLatin1String("thirdparty")) return fetchThirdParty(settings, http);
    return notConfiguredProvider(id);
}

QJsonArray fetchAllLimitProviders(const QJsonObject &settings, HttpClient &http, SpendStore &spend)
{
    const auto enabled = settings.value(QStringLiteral("limitProviders")).toString(defaultLimitProvidersCsv())
                             .split(QLatin1Char(','), Qt::SkipEmptyParts);
    QJsonArray out;
    int n = 0;
    for (const auto &id : enabled) {
        if (n >= 24) break;
        out.append(fetchLimitProvider(id.trimmed(), settings, http, spend));
        ++n;
    }
    return out;
}

} // namespace tmon
