#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>

namespace tmon {

// Read-only discovery of the locally installed ZCode desktop app's connection
// state, mirroring src/shared/providers/zai/zcodeDiscovery.js. ZCode persists
// its provider registry and selection under ~/.zcode/v2/ as plain JSON; files
// may be missing (ZCode not installed) and malformed JSON reads as 'none'.
struct ZcodeConnection {
    // none | start-billing | coding-quota | api-unsupported
    QString kind;
    QString family;      // 'zai' | 'bigmodel'
    QString providerId;
    bool entitled = false;
    QString reason;
    QString mirrorKey;   // ZCode's on-disk mirror JWT (in-memory use only)
    QString billingKey;  // start-plan entry's mirror key for the billing lane
    QString deviceMid;   // telemetry-state deviceMid (X-Device-Mid)
};

ZcodeConnection discoverZcodeConnection();

// zcode.z.ai start-plan billing buckets → { plan, windows }, mirroring
// parseZcodeStartPlanBalances (bucket grouping, boundary picking, plan pick).
QJsonObject parseZcodeStartPlanBalances(const QJsonObject &payload);

// GLM quota payload (+ optional subscription list) → { plan, windows },
// mirroring parseZaiUsage: 5-hour session window, weekly window, MCP billing
// window with a Monthly cadence.
QJsonObject parseZaiUsage(const QJsonObject &quotaBody, const QJsonObject &subscriptionBody = {});

} // namespace tmon
