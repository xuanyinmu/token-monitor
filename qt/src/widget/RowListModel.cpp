#include "widget/RowListModel.h"

namespace tmon {

RowListModel::RowListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void RowListModel::setRows(const QVariantList &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

int RowListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant RowListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) return {};
    const auto row = m_rows.at(index.row()).toMap();
    switch (role) {
    case IdRole: return row.value(QStringLiteral("id"));
    case LabelRole: return row.value(QStringLiteral("label"));
    case TokensRole: return row.value(QStringLiteral("tokens"));
    case CostRole: return row.value(QStringLiteral("cost"));
    case PercentRole: return row.value(QStringLiteral("percent"));
    case MarkRole: return row.value(QStringLiteral("mark"));
    case DetailRole: return row.value(QStringLiteral("detail"));
    case StatusRole: return row.value(QStringLiteral("status"));
    case ClientRole: return row.value(QStringLiteral("client"));
    case ExtraRole: return row.value(QStringLiteral("extra"));
    case PlanRole: return row.value(QStringLiteral("plan"));
    case IconRole: return row.value(QStringLiteral("icon"));
    case PlatformRole: return row.value(QStringLiteral("platform"));
    case StaleRole: return row.value(QStringLiteral("stale"));
    case ColorRole: return row.value(QStringLiteral("color"));
    case ActivityRole: return row.value(QStringLiteral("activity"));
    case LocalRole: return row.value(QStringLiteral("local"));
    default: return {};
    }
}

QHash<int, QByteArray> RowListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {LabelRole, "label"},
        {TokensRole, "tokens"},
        {CostRole, "cost"},
        {PercentRole, "percent"},
        {MarkRole, "mark"},
        {DetailRole, "detail"},
        {StatusRole, "status"},
        {ClientRole, "client"},
        {ExtraRole, "extra"},
        {PlanRole, "plan"},
        {IconRole, "icon"},
        {PlatformRole, "platform"},
        {StaleRole, "stale"},
        {ColorRole, "color"},
        {ActivityRole, "activity"},
        {LocalRole, "local"}
    };
}

} // namespace tmon
