#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace tmon {

class I18n : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString resolvedLanguage READ resolvedLanguage NOTIFY languageChanged)
public:
    explicit I18n(QObject *parent = nullptr);
    void load(const QJsonObject &messages);
    QString language() const { return m_language; }
    QString resolvedLanguage() const { return resolve(); }
    void setLanguage(const QString &language);
    Q_INVOKABLE QString t(const QString &key, const QVariantMap &params = {}) const;

signals:
    void languageChanged();

private:
    QString resolve() const;
    QJsonObject m_messages;
    QString m_language = QStringLiteral("auto");
};

} // namespace tmon
