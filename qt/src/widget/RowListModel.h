#pragma once

#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>

namespace tmon {

class RowListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        LabelRole,
        TokensRole,
        CostRole,
        PercentRole,
        MarkRole,
        DetailRole,
        StatusRole,
        ClientRole,
        ExtraRole,
        PlanRole,
        IconRole,
        PlatformRole,
        StaleRole,
        ColorRole,
        ActivityRole,
        LocalRole
    };
    Q_ENUM(Role)

    explicit RowListModel(QObject *parent = nullptr);
    void setRows(const QVariantList &rows);
    QVariantList rows() const { return m_rows; }
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QVariantList m_rows;
};

} // namespace tmon
