#include "core/i18n/I18n.h"

#include <QFile>
#include <QJsonDocument>
#include <QLocale>

inline void initI18nResource()
{
    Q_INIT_RESOURCE(i18n);
}

namespace tmon {

I18n::I18n(QObject *parent)
    : QObject(parent)
{
    initI18nResource();
    QFile file(QStringLiteral(":/i18n/i18n.json"));
    if (!file.exists())
        file.setFileName(QStringLiteral(":/i18n/resources/i18n.json"));
    if (file.open(QIODevice::ReadOnly)) {
        const auto raw = file.readAll();
        m_messages = QJsonDocument::fromJson(raw).object().value(QStringLiteral("messages")).toObject();
    }
}

void I18n::load(const QJsonObject &messages)
{
    if (!messages.isEmpty()) m_messages = messages;
}

void I18n::setLanguage(const QString &language)
{
    if (m_language == language) return;
    m_language = language;
    emit languageChanged();
}

QString I18n::resolve() const
{
    if (m_language != QLatin1String("auto") && !m_language.isEmpty()) return m_language;
    const auto loc = QLocale::system().name(); // e.g. zh_CN
    if (loc.startsWith(QLatin1String("zh_TW")) || loc.startsWith(QLatin1String("zh_HK"))) return QStringLiteral("zh-TW");
    if (loc.startsWith(QLatin1String("zh"))) return QStringLiteral("zh-CN");
    if (loc.startsWith(QLatin1String("ko"))) return QStringLiteral("ko");
    if (loc.startsWith(QLatin1String("ja"))) return QStringLiteral("ja");
    return QStringLiteral("en");
}

QString I18n::t(const QString &key, const QVariantMap &params) const
{
    const auto lang = resolve();
    auto table = m_messages.value(lang).toObject();
    if (!table.contains(key)) table = m_messages.value(QStringLiteral("en")).toObject();
    auto text = table.value(key).toString();
    if (text.isEmpty()) text = key;
    for (auto it = params.begin(); it != params.end(); ++it)
        text.replace(QStringLiteral("{") + it.key() + QLatin1Char('}'), it.value().toString());
    return text;
}

} // namespace tmon
