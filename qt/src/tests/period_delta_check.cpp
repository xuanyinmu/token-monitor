#include "core/io/HashKey.h"
#include "core/limits/LimitsBurnRate.h"
#include "core/usage/JsonUtil.h"
#include "core/usage/PeriodDelta.h"
#include "core/usage/UsageNormalize.h"

#include <QJsonArray>
#include <QJsonObject>
#include <iostream>

int main()
{
    using namespace tmon;
    QJsonObject base{{QStringLiteral("totalTokens"), 10}, {QStringLiteral("clients"), QJsonObject{{QStringLiteral("codex"), 10}}}};
    QJsonObject fresh{{QStringLiteral("totalTokens"), 15}, {QStringLiteral("clients"), QJsonObject{{QStringLiteral("codex"), 15}}}};
    QJsonObject anchor{{QStringLiteral("totalTokens"), 10}, {QStringLiteral("clients"), QJsonObject{{QStringLiteral("codex"), 10}}}};
    const auto delta = applyPeriodDelta(base, fresh, anchor).toObject();
    if (asNumber(delta.value(QStringLiteral("totalTokens"))) != 15) {
        std::cerr << "delta total failed\n";
        return 1;
    }
    if (asNumber(delta.value(QStringLiteral("clients")).toObject().value(QStringLiteral("codex"))) != 15) {
        std::cerr << "delta clients failed\n";
        return 1;
    }

    QJsonObject nestedClients{
        {QStringLiteral("totalTokens"), 20},
        {QStringLiteral("clients"), QJsonObject{{QStringLiteral("cursor"), 8}, {QStringLiteral("codex"), 12}}}
    };
    QJsonObject nestedFresh{
        {QStringLiteral("totalTokens"), 24},
        {QStringLiteral("clients"), QJsonObject{{QStringLiteral("cursor"), 9}, {QStringLiteral("codex"), 15}}}
    };
    const auto nested = applyPeriodDelta(nestedClients, nestedFresh, nestedClients).toObject();
    if (asNumber(nested.value(QStringLiteral("clients")).toObject().value(QStringLiteral("cursor"))) != 9) {
        std::cerr << "nested delta failed\n";
        return 1;
    }

    const auto identity = projectIdentity(QStringLiteral("D:/Code/Project/token-monitor"));
    if (!identity.value(QStringLiteral("projectId")).toString().startsWith(QLatin1String("sha256:"))) {
        std::cerr << "projectIdentity failed\n";
        return 1;
    }

    QJsonObject tokscale{
        {QStringLiteral("entries"), QJsonArray{QJsonObject{
            {QStringLiteral("client"), QStringLiteral("codex")},
            {QStringLiteral("sessionId"), QStringLiteral("s1")},
            {QStringLiteral("workspaceKey"), QStringLiteral("wk")},
            {QStringLiteral("totalTokens"), 42}
        }}},
        {QStringLiteral("sessions"), QJsonArray{QJsonObject{
            {QStringLiteral("client"), QStringLiteral("codex")},
            {QStringLiteral("sessionId"), QStringLiteral("s1")},
            {QStringLiteral("title"), QStringLiteral("rewrite")},
            {QStringLiteral("firstActiveMs"), 1'700'000'000'000.0},
            {QStringLiteral("lastActiveMs"), 1'700'000'100'000.0}
        }}},
        {QStringLiteral("workspaces"), QJsonArray{QJsonObject{
            {QStringLiteral("workspaceKey"), QStringLiteral("wk")},
            {QStringLiteral("path"), QStringLiteral("D:/Code/Project/token-monitor")},
            {QStringLiteral("label"), QStringLiteral("token-monitor")}
        }}}
    };
    const auto folded = applyTokscaleSessionMetadata(tokscale, true).toObject();
    const auto row = folded.value(QStringLiteral("entries")).toArray().at(0).toObject();
    if (row.value(QStringLiteral("sessionTitle")).toString() != QLatin1String("rewrite")) {
        std::cerr << "session fold failed\n";
        return 1;
    }
    if (row.value(QStringLiteral("projectId")).toString().isEmpty()) {
        std::cerr << "project fold failed\n";
        return 1;
    }
    const auto period = extractUsageFromTokscale(tokscale, true);
    if (asNumber(period.value(QStringLiteral("totalTokens"))) < 42) {
        std::cerr << "extract after fold failed\n";
        return 1;
    }

    const auto h = hashKey(QStringLiteral("deepseek"), QStringLiteral("sk-test"));
    if (!h.startsWith(QLatin1String("sha256:"))) {
        std::cerr << "hashKey failed\n";
        return 1;
    }
    const auto empty = emptyPeriod();
    if (!empty.contains(QStringLiteral("totalTokens"))) {
        std::cerr << "emptyPeriod failed\n";
        return 1;
    }
    std::cout << "period_delta_check ok\n";
    return 0;
}
