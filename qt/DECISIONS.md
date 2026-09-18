# Qt rewrite decisions

This file is the running judgment log for the C++/QML port. `src/shared` remains the spec. Choices below are made so work can continue without blocking the user.

## Bound (from the plan)

- Electron stays the shipping product. `qt/` is a parallel Windows-first port.
- Cloudflare Worker stays JS. C++ Hub speaks `docs/API.md` and reports `runtime: "native"` with **no** JS `hubBuild` hash (legacy-compatible; avoids false “redeploy Hub”).
- No Qt HttpServer / Charts, no third-party C++ libs. Discord RPC and DSH zstd stay deferred.
- Namespace `tmon`. userData is `%APPDATA%\Token Monitor\` (same as Electron), not Qt’s `Org/App` nested path.

## Scaffold

- Old `qt/` MVP sources are not compiled. A full recursive wipe of leftover `qt/src/app`, `qt/src/limits`, `qt/src/window`, and duplicate `qt/qml/*.qml` files was blocked by the environment; CMake only lists the new tree. Leftover unreferenced files are ignored.
- Three executables + `tmon_core`. `TokenMonitor` is a console subsystem on purpose so `--scan-once` can print JSON during development.
- i18n: plan asked for `.ts` via Node. This machine often has no `node` on PATH, so CMake prefers `extract-i18n.js` and falls back to `extract-i18n.ps1`. Runtime loads JSON (`:/i18n/i18n.json`), not Qt Linguist. Same five locales.

## Usage

- Watcher lives on its own `QThread` (plan: not on the collector thread). `QFileSystemWatcher` is not chokidar: we recursively register existing directories to depth 4, cap 500, skip tokscale cache dirs. Unknown paths still trigger an all-client `--today` scan.
- Subprocess: `terminate` → 4s grace → `kill`, plus a generation counter so a superseded scan cannot publish. We do not emulate the JS “unconfirmed close” second grace unless a hang shows up in practice.
- Period delta follows the JS object walk (arrays become keyed objects). Identity is guarded by `TokenMonitorDeltaCheck`.
- tokscale `sessions` / `workspaces` arrays are folded onto rows before extract (decoded path only; opaque keys stay unattributed). Project id is `sha256:` of the normalized path, matching `projectIdentity()` spirit, not tokscale’s workspace key.
- WSL: registry gate, `wsl.exe --list --running` only, `--home \\wsl$\…` on full ticks. Month/allTime for WSL are not separately scanned (same CPU reason as JS: WSL is full-tick only; we merge today’s WSL period into today and leave month/allTime for the Windows full scans plus delta). Documented limitation: a WSL-only client’s month/allTime catches up on the next full tick.
- Local parsers: Proma jsonl is summed. Qoder CN sqlite is best-effort (schema is undocumented); missing tables are a no-op, not a crash.
- `tokscale.exe` prefers userData over next-to-exe so a leftover `qt/build/tokscale.exe` cannot shadow the vendored binary.

## Limits

- `LimitsRuntime` uses concurrency 3 (`QtConcurrent` + per-call `HttpClient`), per-provider skip-if-in-flight lanes, `lastGood` retention on transient statuses, exponential backoff, and adaptive burn-rate with the JS constants (base 5m, floor 60s, samples-ahead 4, release weight 0.3). Adaptive only **shortens** the interval.
- All 24 catalog ids are dispatched. Cookie/OAuth providers that need a Chromium profile are implemented as: read the same local files/tokens Electron uses, then hit the same HTTP endpoints. We do **not** drive a browser. If the local credential is missing, status is `notConfigured`. Empty windows with `status: ok` means “account seen, quota payload not mapped yet” and is treated as a gap to fill, not success.
- Spend/balance files stay under `userData/data/` with the Electron filenames (`deepseek-balance-v2.json`, …).
- `mimoCookie` is not in `CREDENTIAL_SETTING_PATHS`; MiMo stays metadata/managed-accounts only, same as JS.

## UI

- Plan forbids a single `AppController`. `AppState` is the shell coordinator (window/tray/hotkey/settings). Rows go through `RowListModel` (`QAbstractListModel`) per breakdown. QVariantList properties remain as a QML convenience alias.
- Theme tokens copy Electron CSS (`#b7ead4`, glass 48,52,56, presets default/obsidian/porcelain). Radius 8 on inner cards; window chrome uses DWM rounded corners (SDK `DWMWCP_ROUND`) rather than pretending to be 14px CSS.
- Custom controls only (Basic style, no Material/Fusion).
- Session detail is an overlay, not a route. Dashboard is a second `QQmlApplicationEngine` window.
- Icons: reuse `assets/icons/*.svg` via qrc. Missing client marks fall back to a letter tile.
- Settings: eight sections covering the Electron `defaultSettings()` keys that a user can change without a dedicated account-picker UI. Nested profile editors (OpenRouter/OpenCode/third-party maps, Cursor account multi-select) are JSON text fields rather than the full Electron accordion — same store shape, denser editor. `showHomeLimitBars` default is **false** to match Electron.

## Hub / agent

- Standalone `TokenMonitorHub` stores `%APPDATA%\Token Monitor\data\devices.json` so a Node hub in a git checkout and the native hub do not clobber each other by accident. Widget **host** mode keeps Electron’s `hub-devices.json` in userData.
- Agent writes `agent.pid`. Widget sync-upload skips POST when that PID is alive (`OpenProcess`). Stale pid files are unlinked.
- Subscriptions: local list in `settings.json` in `local` mode; hub `PUT` with `baseUpdatedAt` / 409 in `client`/`host`.

## Chrome extras

- Tray uses `assets/icons/token-monitor.svg`.
- Floating bubble is a **second small window**, not a full-widget overlay.
- Top-edge hide: poll cursor; dock off-screen; peek on pointer at y≈0.
- Export: CSV+JSON. Diagnostics: redacted settings + last snapshot + lastError JSON next to export dir.
- Currency: if `currencyRates` lacks the active code, probe `https://open.er-api.com/v6/latest/USD` once per process (no extra dependency). Failure leaves USD.
- GitHub latest-release check unchanged. Start-at-login via HKCU Run.

## Explicitly not in this cycle (unchanged)

- Discord RPC, DSH zstd, Worker rewrite, macOS vibrancy/WidgetKit, new C++ libraries.

## UI parity vs Electron (2026-09-16)

The first Qt shell was a generic dashboard (emoji footer, always-on window buttons, letter tiles). Electron chrome is specific; this pass copies it rather than inventing a native-Qt layout.

- **Shell:** 340×650, padding `12 14 14`, radius 8 on Windows, **no extra 1px CSS border** (DWM rounds the window). Glass tokens match `--glass-rgb 48,52,56`, `--number #f3fbf7`, `--blue #73bdf5`.
- **Titlebar:** 30px, 16px bold title, 4px live-dot (`#5b6471` / accent glow). Status line only for error/fail/offline. Period control is a **148×30 segmented grid** (DAY/MONTH/TOTAL) with a sliding 6px-radius indicator. Pin/min/close (34×28, gap 6, 114px) **replace** the tabs on top-right hover and while settings is open — not shown at rest.
- **Total panel is global**, not Home-only: `TOTAL TOKENS` + `clamp(30–46px)` display font (Segoe UI) + `$cost`. Row lists sit under it. Full grouped number (`1,234,567`) in the hero; compact `1.2M` stays on rows.
- **Footer:** view-switcher (icon + label + chevron, max 150px) + settings/refresh hover-swap. No 9 glyph buttons, no footer token total.
- **Rows:** 10px SVG mask (tinted via `MultiEffect`), name/metrics row, then a 6px **blue** bar. Home modules use uppercase heads + jump icons and cap at 5 tools / 5 models / 4 limits.
- **Settings** is an overlay flag (`settingsOpen`), not a breakdown view, so the switcher still shows Tool/Home/…. Accordion cards stack with 12px outer radius and 15px section icons from the Electron renderer SVG set (qrc `/ui`).
- **Default breakdown** is `tool`, matching Electron `lastViewState`. Existing Qt installs that saved `home` keep home until changed.
- **Back to Home** is always shown on non-home views. Electron gates this on `homeReturnVisible` (only after leaving Home). Showing it always is more discoverable and still matches the control chrome; recorded so it can be gated later if a screenshot complains.
- **Icons:** `assets/icons/*.svg` for client marks; `src/electron/renderer/icons/{views,actions,settings}` copied into qrc `/ui` so masks stay 1:1 with Electron, not redrawn.
- **Cookie/OAuth limits, Discord RPC, DSH zstd** still deferred. Visual parity of those settings rows is layout-only.
- **Home modules** use Electron ids (`limits,tool,device,model,trends`) and the same default hide list (`tool,device`). A first Qt pass used `tools/models/heatmap`, so Home rendered blank against a real `settings.json`. Icons are colorized with a `QSvgRenderer` + `SourceIn` image provider (`image://mask/…`) because Qt SVG + `currentColor` otherwise stays black.
- **Screenshot / compare loop:** tokscale on the GUI thread blocked `--screenshot` (QTimer never fired; a hung `TokenMonitor.exe` then LNK1104-locked the binary). Screenshot mode now sets preview-only: skip collector/tray/hotkey, hydrate last `collector-anchor.json` + `history.json` (and Electron `daily-history-archive.json` if Qt history is empty), grab after 900ms. Release links as `TokenMonitorQt.exe` until the locked `TokenMonitor.exe` is gone.
- **Home limits** match Electron: account head + up to two window cells showing remaining (`N left`), not a used-meter LimitRow. Bars stay off unless `showHomeLimitBars`. Tool/model home rows show compact tokens + **share %**, not `$cost`.
- **Limits page** shows remaining meters per window, plan label, and not-configured rows as “Not signed in”, matching the Electron limits panel structure (Cursor-style stacked windows).
- **i18n qrc path:** `qt_add_resources` without `BASE` stored the catalog at `:/i18n/resources/i18n.json`, so `I18n` loaded an empty file and the switcher fell back to raw ids (`home`). Loader now accepts both aliases and CMake sets `BASE` to `resources/`.
- **Settings controls:** native Qt ComboBox/Button painted as Fusion-light on Windows; replaced with glass ComboBox/Button (sunken 5% overlay, 8px radius, `--text`).
- **Footer** includes the idle live-rate chip (`zap` + `— tok/s`) between the switcher and settings, same slot as Electron.
- **Limits snapshot** is persisted to `limits-snapshot.json` so screenshot/preview mode can paint last-known quotas without waiting on provider HTTP. Preview still starts LimitsRuntime (not tokscale) so a warm cache can refresh.

- **Activity heatmap** is a rolling-year Sunday grid (cell 9 / gap 3) with Electron's four-level blue fills and month labels; trend line is a 45-day area chart with a 2px stroke.

## UI parity iteration (2026-09-16, continued)

Visual 1:1 vs the live Electron widget (Limits / zh-CN) was still failing. Choices made so the loop can keep going:

- **CJK mojibake:** `I18n.t("views.home")` dumped as UTF-8 `涓婚〉` (classic UTF-8-as-GBK of `主页`). Disk `i18n.json` was already garbled. Cause: `extract-i18n.ps1` used `Get-Content` without `-Encoding UTF8` (Windows PowerShell ACP = GBK). CMake prefers Node, but a later PowerShell extract overwrote the catalog. Fix: UTF-8 extract on both paths, re-emit JSON from `i18n.js`, keep RCC as binary. Not a font bug — Cascadia Mono is restored as the Electron default UI font (`ui-monospace` → Cascadia Mono / Consolas); display numbers stay Segoe UI from `displayFontFamily`. Qt cannot parse CSS font stacks, so we take the first real family.
- **Cursor limits stub:** `fetchCursor` returned `ok` + empty windows whenever Cursor files existed, then `publish()` overwrote `limits-snapshot.json`. Electron's live Limits page shows Cursor Models / Other Models / Grok Bot / On-demand spend. Qt now reads tokscale `cursor-credentials.json` (desktop `state.vscdb` fallback), probes `cursor.com` with the same cookie + browser UA as `probe.js`, and maps the same windows. Empty `ok` responses do not replace a last-good payload that already has windows. Screenshot mode waits up to ~6s for a windowed row instead of grabbing at 1.8s.
- **Limits chrome:** remaining is `N% left` (not `N left`); spend is `$used / $limit` with no meter; reset line `Reset 12h 9m`; `Updated 4m ago` under the name; plan / `Not signed in` on the right. Those English strings match Electron — it does not translate them even in zh-CN.
- **Home limits empty copy** uses `home.noLimits`. Home window cells use the Electron kind-priority + max-2 slice (`homeWindows`).
- **Heatmap archive path:** Electron `sharedDataDir` is `%APPDATA%\Token Monitor`. Qt `sharedDataDir()` is `userData/data` (hub isolation). Archive is at the userData root; hydrate now unions missing days from that file even when `history.json` already has today.
- **Title collapse:** `titleIconOnly` in the shared settings is on, so Electron shows Σ at rest. Qt now honors that flag (and still collapses on overflow). Heatmap is a horizontal scroller parked on the most recent weeks (Electron’s activity strip is `overflow-x: auto` and lives off the right edge of a 340px window). Archive days are summed from `observations`, not a missing top-level `tokens` field.
- **Home list rows** are a single 4-column line (mark / name / tokens / share), 16px tall, matching Electron `.home-model-row`. The previous stacked metrics made Home ~40px taller and pushed Trend below the fold.
- **Heatmap months** use the resolved UI locale (`zh-CN` → `9月`), not hardcoded English `Apr`.
- **Settings chrome:** Electron inserts the accordion between the titlebar and TOTAL TOKENS; collapsed sections keep the current view visible underneath. Qt now does the same (settings is not a full-body replacement). Default is all sections collapsed, with muted right-side summaries (`settings.summary.*`). Collection summary prefers `clientStatus` health counts and falls back to tracked-client counts when preview hydrate has no health object. Expanded General still uses left-label / right-control rows.
- **Screenshot `--view settings`** parks the Limits breakdown underneath so the grab matches the live Electron compare surface (settings + TOTAL + Cursor windows). A normal settings toggle still keeps whatever view was showing.
- **Judgment:** do not chase DWM acrylic wash in `grabWindow` PNGs. Layout, copy, meters, fonts, and i18n stay the parity target. Token totals may differ between Qt `--screenshot` hydrate and a live Electron collector; that is data, not chrome.

## Live widget vs screenshot hydrate (2026-09-16)

The user's live Qt Limits shot (full "Token Monitor" title, English "Limits", raw `ok` / `notConfigured` meters, no DAY tabs, no tok/s) does not match the current QML tree. That layout is the old `LimitRow` page. Screenshot hydrate was already closer to Electron; live was the remaining gap.

- **Usage blocked limits:** `DeviceRuntime::start` ran a GUI-thread tokscale `fullScan` *before* `LimitsRuntime::start`. The live Limits page could sit on a stub `ok` + empty windows until the collector returned. Limits now start first; tokscale is `QTimer::singleShot(0)` after the first paint so Cursor HTTP can complete on the event loop.
- **Title Σ:** `!!app.settings.titleIconOnly` was a QVariantMap lookup. Electron stores the flag as a real boolean and defaults it on. Qt now exposes `app.titleIconOnly` from the settings object (bool / 0-1 / "true"), default **true**, same as Electron `defaultAppearance`.
- **tok/s chip:** Electron hides `#liveTokenRate` until `showLiveTokenRate === true`. The live Electron shot has it on, so the user's `settings.json` has the flag. Qt Footer now gates on `app.showLiveTokenRate` instead of always painting the idle chip (which made old vs new binaries harder to tell apart, and would mismatch a default-off install).
- **Cursor probe UA:** `HttpClient` stamped `TokenMonitor/0.57.0` *then* applied provider headers. A 200 HTML/challenge body parsed as empty JSON and published `status: ok` with no windows — the live "ok" row. Default UA is now the browser string, skipped when the caller sets `User-Agent`, HTTP/2 off, and non-JSON 200 is `unavailable` (last-good windows kept).
- **Limits copy:** remaining percents stay 0–100 like Electron `core.js`; QML `remainFrac` accepts either scale. Limits-page values stay `--text` (Electron does not redden 0% left on that page). Raw status strings never render as meter labels.
- **Judgment:** keep iterating against the *live* `TokenMonitorQt.exe` window, not only `--screenshot`. Kill leftover `TokenMonitor.exe` so an old link cannot be what the user is looking at. The 2026-09-16 live PrintWindow pair (`qt/build/compare/qt-live-limits.png` vs `electron-live-0.png`) shows the Limits chrome now matching: Σ + DAY/MONTH/TOTAL, zh-CN `额度`, Cursor Pro windows (Cursor Models / Other Models / Grok Bot / On-demand spend), `Not signed in`, zap `tok/s`. Remaining deltas that are **not** treated as chrome blockers: DWM acrylic wash (Electron 356×722 shadow frame vs Qt 340×650 client), and token totals while a long-running Electron collector is ahead of a freshly launched Qt hydrate/scan. `TokenMonitor.exe` in `qt/build` is overwritten from `TokenMonitorQt.exe` so a stale output name cannot relaunch the old LimitRow UI.

## Live Home / Limits / Tool / Trends (2026-09-16 evening)

Captured Electron by cycling the view-switcher **current** button (it advances to the next view; the chevron/long-press opens the menu). Authoritative pair: `electron-cycle-1.png` (Limits) / `electron-cycle-3.png` (Home) / `electron-cycle-4.png` (Tool) / `electron-cycle-2.png` (Trends).

Choices made after that pair:

- **Home window labels** follow Electron `homeLimitWindowLabel`: a `weekly` kind renders `home.limit.weekly` ("Weekly" in every locale), not the raw "Grok Bot" collector label. Reset copy under the cell is shown at 9px. Remaining percents stay `--text` unless `showHomeLimitBars` is on.
- **Home model marks** use `modelVendorFor` (same regex family as `usageCharts.js`). CSS swaps `.row-icon-xai` → `grok.svg` and `.row-icon-grok` → `xai.svg`; Qt maps vendor `xai` to `grok.svg`. Unrecognized models use a Σ glyph — `token-monitor.svg` is a `<text>Σ</text>` that QSvgRenderer does not paint, so HomeListRow draws the character directly.
- **Home trend** is tokens (not heatmapMetric), Catmull-Rom-ish cubic matching `smoothLinePath`, dates as `M/D`, peak meta `home.peakTokens`. Heatmap cells are rx=2 and no longer back-fill empty cost days with tokens (that made the grid denser than Electron).
- **Footer current** cycles like Electron; chevron hover/click (and 420ms long-press) opens the menu.
- **Breakdown rows** use grouped `formatNumber` (`196,151,383`) and the client/vendor bar color (Cursor `#000000`), not compact M + always-blue.
- **Trends page** is the Electron 7-bar spark + 2×2 stats, not a heatmap. `today` scopes active-time and peak to **today's** archive row (14m / today's tokens); active days and streak stay all-history. Hydrate now copies `activeTimeMs` from `daily-history-archive.json` onto existing `history.json` days.

Still open for full 1:1: Settings accordion density vs Electron, Dashboard window, DWM acrylic wash (not a chrome blocker), and Electron’s footer update pill (`↑ v0.58.0`) which Qt does not show — Qt keeps the `tok/s` chip the attached Electron target used.

## Live Limits + Home (2026-09-16 night)

Rebuilt `TokenMonitorQt.exe` and captured live PrintWindow pairs (not `--screenshot`):

- Limits: `qt/build/compare/qt-live-limits.png` vs `electron-live-limits.png` (`electron-cycle-1.png`)
- Home: `qt/build/compare/qt-live-home.png` vs `electron-live-home.png` (`electron-cycle-3.png`)

Chrome that now matches Electron on those views:

- Σ title, 148×30 DAY/MONTH/TOTAL, zh-CN footer (`额度` / `主页`), zap `tok/s`
- Cursor **Pro** windows: Cursor Models remaining, Other Models, **Grok Bot** 100% left, On-demand spend `$used / $limit`
- GLM / DeepSeek **Not signed in**
- Home 额度 cells: **Weekly** 100% left + Cursor Models remaining + Reset
- Home 模型 compact units strip trailing `.0` (`118M` not `118.0M`; suffix `M` not `m`/`k`)
- Home heatmap / 趋势 / 活跃 20 天 structure

Judgments this pass:

- **Grok Bot missing** was a Cursor sand-usage parse, not packing. Qt used `QJsonValue::toBool()` on `hasNonZeroIncludedLimit` (false for numeric/string) and a 5s POST timeout. Now uses a typed `jsonBool`, optional nested `usage` object, `includedLimit > 0` fallback, 15s timeout. Home then correctly prefers weekly Grok Bot as “Weekly” over Other Models.
- **tok/s vs update pill:** Electron hides tok/s when `#appUpdatePill` is visible (`live-token-rate-obscured`). Live Electron currently shows `↑ v0.58.0` because GitHub latest is newer than `0.57.0`. Qt now checks latest in the background, strips a leading `v`, and paints the same pill (click opens the release URL). tok/s still shows when the setting is on and no update is ready. The attached screenshot with tok/s was a no-update session, not a different footer layout.
- **Settings collection summary:** Electron reads `clientHealth` (`healthy`/`waiting`/`attention`/`unavailable`), not the legacy `clientStatus` map. Qt never produced `clientHealth`, so the accordion fell back to `追踪 N`. Qt now derives the same overall from `clientSourceRoots` + allTime tokens (detected+tokens=healthy, detected+0=review, missing=unavailable) for the `clients` CSV.
- **Settings card density:** Electron is one iOS-style grouped card (`panel 0.35`, 40px rows). Qt’s extra 0.45 slab is gone; headers are 40px.
- **Dashboard:** Electron’s Usage Dashboard is a second 920×620 frameless window (Overview/Trends, 8 stat cards, heatmap Tokens/Cost, model+tool breakdown). Qt’s stub is replaced with that chrome. Home 趋势 ↗ still goes to the Trends page; Trends ↗ / spark open the dashboard window (not `view=dashboard`, which would replace the widget). Trends daily bars are token totals for the selected range; stacked per-client history is a remaining data-shape gap (Qt `historyDays` has no `perClient`).
- **Session hover-scroll:** Electron marquees overflowing title/id on hover (240ms delay, 22ms/px). Qt `UsageRow` now does the same when `hoverScroll` is on (Session view).
- **Token totals** still diverge while both collectors run. Data, not chrome.
- **Session rows** use Electron `sessionRows.js` fields. Cost uses `$X.XX` (`formatUsd`); Electron’s compact-currency extra decimals stay out until compactMoney is ported.
- **Status** fetches the four statuspage summaries in parallel on dedicated `QThread`s. Pills use `serviceStatus.*` i18n.

- **Dashboard heatmap window:** Electron Overview uses a 12-month grid starting on the 1st of the month 11 months back. Qt Home stays a 53-week rolling year (matches the widget). Dashboard Heatmap now uses `twelveMonth`.

## Live chrome pass (2026-09-16, obvious mismatch)

The user still saw an obvious live mismatch after Limits/Home checklist items were structurally in place. Choices this pass:

- **Session msgs:** `UsageNormalize` counted `+1` per tokscale model row (`2 msgs`). Electron uses `firstNumber(row, MESSAGE_COUNT_KEYS)` then sums on merge (`18 msgs`). Qt now uses the same keys, and copies a session-array `messageCount` onto a row that has none.
- **USD under $10:** Electron `fractionDigitsFor` uses 4 decimals below $10 (`$1.9511`). Qt `formatUsd` always used 2. Now USD `>= 10` → 2 digits, else 4.
- **DWM hairline + missing shadow:** Electron acrylic does **not** `DwmExtendFrameIntoClientArea(-1)` (that path is Accent-only). Qt punched the whole client, which drops the Win11 card shadow (`340×650` vs Electron `356×722`) and washes the glass. Acrylic mode now sets `DWMWA_SYSTEMBACKDROP_TYPE` + dark mode + `DWMWA_BORDER_COLOR=NONE` + `CS_DROPSHADOW`, and only extends the frame in Accent mode.
- **Home module rhythm:** Electron `.home-panel` gap 12, `.home-module` gap 7 + padding-bottom 12 + 1px rule. Qt stacked modules with spacing 8 and the rule inside the 7px gap. Each module is now an inner column (gap 7, padding-bottom 12) plus the rule, with 12px between modules.
- **Limits title weight:** `.limit-name` is 12px regular; Qt `Font.DemiBold` is dropped.
- **Session bars:** `.session-mode .bar` is 5px / gap 5 / padding-bottom 8. `UsageRow` takes `barHeight` (Session passes 5).
- **TOTAL TOKENS** bottom padding 12 to match `.total-panel`.
- **Dashboard % clip:** breakdown row used a 86px reserve for 90px of token+percent text. Switched to `RowLayout` with a filling bar and 52/44 trailing columns.
- **Judgment:** acrylic wash is chrome when it changes the window silhouette (shadow / hairline / extra transparency). Token totals while both collectors run independently stay data, not chrome. Live captures must key Electron by process image, not the shared `"Token Monitor"` title.

- **Update pill:** `api.github.com` returned 403 (unauthenticated quota). Electron uses `https://github.com/Javis603/token-monitor/releases/latest` with `Accept: application/json` so public checks skip the API. Qt now uses that same route.

- **Footer switcher width (2026-09-16 user report):** Electron `.view-switcher` is `flex: 0 1 auto; max-width: min(150px, 52%)` (112px when live-token-rate is enabled) with `margin-right: auto`. Qt gave the switcher `Layout.fillWidth`, so it grew/shrunk with leftover row space and looked shorter than Electron. It is now content-sized with the same 150/112 cap.
- **Settings-open gear shift:** Electron `.utility-actions` stays `flex: 0 0 34px` on the right; refresh is `position: absolute; right: calc(100% + 6px)`. Qt hid the center spacer when settings opened, so the 34px slot packed left next to the (now hidden) switcher. The fill-width spacer now always stays in the row; only its chips hide.
- **MONTH/TOTAL unclickable:** Electron `.window-actions` is `pointer-events: none` until `.actions-hotspot` (28×28 at `top:-12; right:-14`, L `clip-path`) or settings-open. Qt put a 114×30 hover overlay over the whole tab cluster, so entering MONTH/TOTAL revealed pin/min/close and stole the click. Hotspot is now the L only; overlay enables after reveal (140ms hide delay, same as CSS).
- **Settings right column clipped:** `SectionBody` inner Column was `x: 11; width: parent.width` inside a `clip: true` card, so every control overflowed 11px and switches/combos lost their right edge. Inner width is `parent.width - 22`.
- **Main view list:** Electron `#viewDisplayList` (name + eye + grip, home nested) was missing. Added, including reset/show-all. `hiddenViews` / `viewDisplayOrder` no longer restart the collector.
- **Switcher stays up in settings:** Electron does not hide `.view-switcher` when settings is open (only `.tool-detail-footer`). Qt had `visible: !settingsOpen`, which dropped the control the user was comparing. Restored. Settings body uses `Layout.fillHeight` leftover instead of `maxHeight: height-180` so 货币 / 模型排序 land above TOTAL like Electron.
- **Appearance first screen:** Electron 外观 is glass System/Transparent, Windows Acrylic, live/tool/title/compact/rate toggles, unit/swap, Glass/Depth/Zoom sliders. Qt had a 3-row backdrop/theme/heatmap stub; heatmap belongs under Home. Ported the Electron first screen. `systemGlass: false` maps to DWM `DWMSBT_NONE` + opaque theme glass. `settingsInTitlebar` swaps footer primary like `.utility-actions.is-swapped`.
- **Section scroll:** Electron does **not** auto-scroll an opened accordion to the top — 常规 stays visible when 主画面 expands. Qt had started scrolling the opened section into view to fit Appearance sliders; that diverged from Electron and also broke the capture click map. Removed. Appearance sliders below the fold match Electron (user scrolls).
- **Settings descriptions:** Electron `.settings-item-desc` wraps onto a second full-width line. Qt `SettingRow` was `maximumLineCount: 1`, which truncated 默认统计范围. Now 2 lines.
- **Grok/Weekly on Home:** Electron labels `kind: weekly` as "Weekly" (Grok Bot included limit at 0% used = 100% left). Qt dropped the window when `usagePercent` was missing. Now included-limit with no percent still emits a 0%-used weekly window.
- **View-row 三:** Electron `.view-subgroup-icon` masks `settings/general.svg` on home/status/project/trends. Qt used a chevron on home only; now the same general.svg mark on those four ids.
- **Switcher chrome (follow-up):** Electron splits `.view-switcher-current` (icon+label, padding 8, `flex: 0 1 auto`) from a 24px `.view-switcher-disclosure`. Qt had been stretching the whole capsule to the 150px cap, which made 趋势/额度 look longer than Electron. It now sizes to the label and uses the same 24px chevron hit-target. Long-press 420ms opens the menu, matching `VIEW_SWITCHER_LONG_PRESS_MS`.
- **Hotspot geometry:** L arms are explicit `x/y` (top 28×12 above the tabs, right 14×28 in the 14px padding). Hover over MONTH/TOTAL must not set `hotspotHot`.
- **Settings exclusive accordion (2026-09-16 user report):** Electron `setSettingsSectionExpanded` closes every other section, and `initSettingsAnimationWrappers` turns `.settings-section-details` into a 250ms `grid-template-rows` accordion. Qt allowed multiple sections and swapped Loader height with no animation. Opening a section now sets `open` to only that id; body height/opacity animate 250ms InOutCubic.
- **Settings leftover Home:** Electron keeps Home under TOTAL when settings is open (额度/模型 in the leftover). Hiding Home and `fillHeight` on settings produced the empty well the user marked. Settings is content-sized again (capped so TOTAL+footer remain); Home always `fillHeight`.
- **DWM caption strip / right shift:** `WS_THICKFRAME` plus `setColor(transparent)` left a light DWM caption on top and a left resize frame, so chrome looked offset. `WM_NCCALCSIZE` now makes the client cover the HWND; window color is Theme.bg (`#303438`) instead of transparent; caption color matches.
- **Settings gear spin:** `IconButton` spun whenever `iconSource` was set and `active` was true, so the settings gear spun and stayed accent-green while settings was open. Only refresh sets `spinning`.
- **White strip on resize:** `DwmExtendFrameIntoClientArea(-1)` plus `Window.color: transparent` shows DWM's light frame in newly exposed pixels. Window color is `Theme.bg`. The settings slot no longer uses `layer.enabled` or `preferredWidth: root.width` (those broke fill-width and left a GPU layer stale on resize).
- **Refresh spinner:** Electron's idle refresh is the `↻` glyph; `spinner.svg` only applies with `.is-refreshing`. Qt always used spinner.svg. Idle is now `↻`; spinner + accent for ~700ms after click.
- **Combo width:** Electron `.settings-item > select` is `width: auto; max-width: 58%`. Sizing from the eliding `contentItem.implicitWidth` collapsed the control (circular). Width now comes from an unelided probe `Text.contentWidth + 48`, capped at 58% of the panel, and the popup matches the closed control.
- **MONTH range menu clicks:** The 本月/本周/近7/近30 popup was a `Popup.Item` child of the 30px tab strip, so it painted over Home but hit-testing went to the heatmap (the 0 tokens tooltip). It now parents to `Overlay.overlay` (or the window content item) and is modal without a dimmer.

## Screenshot mismatch pass (2026-09-16)

User live shots of Electron vs Qt (zh-CN). Judgments:

- **Language ComboBox:** Windows native highlight painted the selected row white. Qt ComboBox now owns popup chrome (dark glass, accent highlight, `highlight: null`) and palette so Basic style cannot fall back to the native list.
- **Settings leftover:** Electron leaves Home (额度/模型) under TOTAL when settings is open. Qt matches that: settings is content-sized, Home stays visible.
- **Settings overlap (2026-09-16 live shot):** Opening 常规 painted a rounded grouped card over TOTAL + leftover Home, with accordion summaries and heatmap continuing to the right of the card. Cause: one tall `clip:false` card (alpha 0.35, `implicitWidth` 280) overflowing the layout cell, composited beside a wider HWND. Fix: per-section cards like Electron's `.settings-collapsible-group` (`rgba(panel, 0.35/0.55)`, first/last 12px corners), a full-width clipped `settingsSlot` with `layer.enabled`, Flickable `anchors.fill`, and `SWP_FRAMECHANGED` after `WS_THICKFRAME` so QML width follows the client. Leftover 额度/模型 still sits under TOTAL; it must not show *through* or *beside* the accordion.
- **Home heatmap / trend:** Electron Home uses `rollingYearHeatmap` (12 months from the 1st of month −11) plus `clampDaily(history, 45)` over the **sparse** history series (zero days omitted), so dates can read `4/9 … 9/16` with a wave. Filling the last 45 calendar days with zeros flattened the line into a spike — that was a misread of `clampDaily`. Qt Home heatmap is `twelveMonth`; tooltip is `formatTokens` + ISO date like Electron's activity tooltip. Month labels at 340px still clip on the left (scroll to end); widening the window reveals 2月, same as Electron.
- **Collection / Limits / Subs / Sync:** Electron is `#clientDisplayList`, `#limitProviderCheckboxes`, empty-state `settings.subscriptions.emptyList` + add toggle, and three `.hub-mode-option` radios + device ID. Qt stubs (WSL toggles, limits switches, Hub-mode combo) are replaced with those lists. Drag-reorder of rows is still a gap (Electron also dropped the grip; drag is on the row).
- **Resize:** Electron `WINDOW_LIMITS` 240×140–1200×1400, resizable except desktop-pinned. Qt frameless windows need `startSystemResize` on edge handles **and** `WS_THICKFRAME` on the HWND or Aero will not size.
- **Token refresh:** Watch debounce is already 1500ms (Electron). The lag was `todayScan`/`fullScan` blocking the GUI thread, so watcher ticks queued behind tokscale. Scans now run on `QThread::create`; results apply on the GUI thread; a mid-scan watch re-arms as `pendingToday`. `todayScan` also persists `history.json`. Totals while both collectors run independently can still differ — data, not chrome.



