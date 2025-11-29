#include "chatmessagemodel.h"
#include "chatfilterproxymodel.h"
#include <QModelIndex>

ChatFilterProxyModel::ChatFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    qDebug() << "[ProxyModel] Создан прокси-фильтр для истории чата";
}

bool ChatFilterProxyModel::filterAcceptsRow(int source_row,
                                            const QModelIndex &source_parent) const
{
    QModelIndex sourceIndex =
        sourceModel()->index(source_row, 0, source_parent);
    if (!sourceIndex.isValid()) {
        return false;
    }

    ChatMessage msg =
        sourceModel()->data(sourceIndex, Qt::UserRole).value<ChatMessage>();
    const QRegularExpression& regex = filterRegularExpression();

    // Если фильтр не задан или пустой — показываем все строки
    if (!regex.isValid() || regex.pattern().isEmpty()) {
        return true;
    }

    // Совпадение по тексту сообщения
    bool matched = regex.match(msg.payload).hasMatch();
    return matched;
}
