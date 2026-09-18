#include "core/io/CredentialStore.h"

#include "core/io/JsonIo.h"
#include "core/io/Paths.h"

#include <QJsonArray>
#include <QMap>

namespace tmon {
namespace {

const QMap<QString, QStringList> kPaths = {
    {QStringLiteral("hubHostSecret"), {QStringLiteral("hub"), QStringLiteral("hostSecret")}},
    {QStringLiteral("secret"), {QStringLiteral("hub"), QStringLiteral("clientSecret")}},
    {QStringLiteral("claudeWebCookie"), {QStringLiteral("providers"), QStringLiteral("claude"), QStringLiteral("webCookie")}},
    {QStringLiteral("opencodeCookie"), {QStringLiteral("providers"), QStringLiteral("opencode"), QStringLiteral("cookie")}},
    {QStringLiteral("opencodeProfiles"), {QStringLiteral("providers"), QStringLiteral("opencode"), QStringLiteral("profiles")}},
    {QStringLiteral("openrouterProfiles"), {QStringLiteral("providers"), QStringLiteral("openrouter"), QStringLiteral("profiles")}},
    {QStringLiteral("deepseekApiKey"), {QStringLiteral("providers"), QStringLiteral("deepseek"), QStringLiteral("apiKey")}},
    {QStringLiteral("minimaxApiKey"), {QStringLiteral("providers"), QStringLiteral("minimax"), QStringLiteral("apiKey")}},
    {QStringLiteral("copilotApiToken"), {QStringLiteral("providers"), QStringLiteral("copilot"), QStringLiteral("apiToken")}},
    {QStringLiteral("zaiApiKey"), {QStringLiteral("providers"), QStringLiteral("zai"), QStringLiteral("apiKey")}},
    {QStringLiteral("zaiTeamApiKey"), {QStringLiteral("providers"), QStringLiteral("zaiTeam"), QStringLiteral("apiKey")}},
    {QStringLiteral("zaiTeamOrganizationId"), {QStringLiteral("providers"), QStringLiteral("zaiTeam"), QStringLiteral("organizationId")}},
    {QStringLiteral("zaiTeamProjectId"), {QStringLiteral("providers"), QStringLiteral("zaiTeam"), QStringLiteral("projectId")}},
    {QStringLiteral("volcengineAccessKeyId"), {QStringLiteral("providers"), QStringLiteral("volcengine"), QStringLiteral("accessKeyId")}},
    {QStringLiteral("volcengineSecretAccessKey"), {QStringLiteral("providers"), QStringLiteral("volcengine"), QStringLiteral("secretAccessKey")}},
    {QStringLiteral("volcengineAgentAccessKeyId"), {QStringLiteral("providers"), QStringLiteral("volcengine"), QStringLiteral("agentAccessKeyId")}},
    {QStringLiteral("volcengineAgentSecretAccessKey"), {QStringLiteral("providers"), QStringLiteral("volcengine"), QStringLiteral("agentSecretAccessKey")}},
    {QStringLiteral("alibabaCookie"), {QStringLiteral("providers"), QStringLiteral("alibaba"), QStringLiteral("cookie")}},
    {QStringLiteral("qoderCookie"), {QStringLiteral("providers"), QStringLiteral("qoder"), QStringLiteral("cookie")}},
    {QStringLiteral("traeAccessToken"), {QStringLiteral("providers"), QStringLiteral("trae"), QStringLiteral("accessToken")}},
    {QStringLiteral("traeDeviceId"), {QStringLiteral("providers"), QStringLiteral("trae"), QStringLiteral("deviceId")}},
    {QStringLiteral("zedCookie"), {QStringLiteral("providers"), QStringLiteral("zed"), QStringLiteral("cookie")}},
    {QStringLiteral("commandcodeCookie"), {QStringLiteral("providers"), QStringLiteral("commandcode"), QStringLiteral("cookie")}},
    {QStringLiteral("kimiApiKey"), {QStringLiteral("providers"), QStringLiteral("kimi"), QStringLiteral("apiKey")}},
    {QStringLiteral("kimiWebAccessToken"), {QStringLiteral("providers"), QStringLiteral("kimi"), QStringLiteral("webAccessToken")}},
    {QStringLiteral("ollamaCookie"), {QStringLiteral("providers"), QStringLiteral("ollama"), QStringLiteral("cookie")}},
    {QStringLiteral("thirdPartyProfiles"), {QStringLiteral("providers"), QStringLiteral("thirdparty"), QStringLiteral("profiles")}}
};

QJsonObject emptyDocument()
{
    return QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("credentials"), QJsonObject{}},
        {QStringLiteral("migrations"), QJsonObject{}}
    };
}

QJsonValue readPath(const QJsonObject &root, const QStringList &path)
{
    QJsonValue cur = root;
    for (const auto &part : path) {
        if (!cur.isObject()) return {};
        cur = cur.toObject().value(part);
    }
    return cur;
}

void writePath(QJsonObject &root, const QStringList &path, const QJsonValue &value)
{
    if (path.isEmpty()) return;
    if (path.size() == 1) {
        root.insert(path.first(), value);
        return;
    }
    auto child = root.value(path.first()).toObject();
    writePath(child, path.mid(1), value);
    root.insert(path.first(), child);
}

bool present(const QJsonValue &value)
{
    if (value.isString()) return !value.toString().isEmpty();
    if (value.isArray()) return !value.toArray().isEmpty();
    if (value.isObject()) return !value.toObject().isEmpty();
    return false;
}

} // namespace

QJsonObject CredentialStore::load()
{
    auto doc = readJsonObject(Paths::credentialsPath());
    if (doc.isEmpty()) return emptyDocument();
    if (!doc.contains(QStringLiteral("credentials"))) doc.insert(QStringLiteral("credentials"), QJsonObject{});
    return doc;
}

bool CredentialStore::save(const QJsonObject &document)
{
    return writePrivateJsonAtomic(Paths::credentialsPath(), document);
}

QJsonValue CredentialStore::getValue(const QString &settingsKey)
{
    const auto path = kPaths.value(settingsKey);
    if (path.isEmpty()) return {};
    return readPath(load().value(QStringLiteral("credentials")).toObject(), path);
}

QString CredentialStore::get(const QString &settingsKey)
{
    return getValue(settingsKey).toString();
}

void CredentialStore::set(const QString &settingsKey, const QJsonValue &value)
{
    const auto path = kPaths.value(settingsKey);
    if (path.isEmpty()) return;
    auto doc = load();
    auto creds = doc.value(QStringLiteral("credentials")).toObject();
    writePath(creds, path, value);
    doc.insert(QStringLiteral("credentials"), creds);
    save(doc);
}

QJsonObject CredentialStore::overlayOnto(const QJsonObject &settings)
{
    auto out = settings;
    auto creds = load().value(QStringLiteral("credentials")).toObject();
    for (auto it = kPaths.begin(); it != kPaths.end(); ++it) {
        const auto value = readPath(creds, it.value());
        if (!value.isUndefined() && !value.isNull()) out.insert(it.key(), value);
    }
    return out;
}

QVariantMap CredentialStore::redactedForUi(const QJsonObject &settings)
{
    auto map = settings.toVariantMap();
    const QStringList expose{QStringLiteral("hubHostSecret"), QStringLiteral("secret")};
    for (auto it = kPaths.begin(); it != kPaths.end(); ++it) {
        if (expose.contains(it.key())) continue;
        const auto value = settings.value(it.key());
        if (present(value)) map.insert(it.key(), QStringLiteral("set"));
        else map.insert(it.key(), QString());
    }
    return map;
}

} // namespace tmon
