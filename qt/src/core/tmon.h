#pragma once

#include <QtGlobal>

namespace tmon {

inline constexpr const char *kAppName = "Token Monitor";
inline constexpr const char *kAppId = "com.javis.tokenmonitor";
inline constexpr const char *kAppVersion = "0.57.0";
inline constexpr const char *kNativeRuntime = "native";
inline constexpr int kDefaultWidth = 340;
inline constexpr int kDefaultHeight = 650;
inline constexpr int kHubDefaultPort = 17321;
inline constexpr int kWatchDebounceMs = 1500;
inline constexpr int kDefaultCollectionIntervalMs = 5 * 60 * 1000;
inline constexpr int kDefaultLimitsRefreshMs = 5 * 60 * 1000;
inline constexpr int kDefaultStaleAfterMs = 10 * 60 * 1000;
inline constexpr int kHubBroadcastDelayMs = 100;
inline constexpr int kSseHeartbeatMs = 30000;
inline constexpr int kSubprocessGraceMs = 4000;
inline constexpr int kTokscaleTimeoutMs = 120000;
inline constexpr int kSelfSyncMinIntervalMs = 5 * 60 * 1000;
inline constexpr int kLimitsConcurrency = 3;

} // namespace tmon
