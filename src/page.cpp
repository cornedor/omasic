#include "page.h"

const QStringList Page::keys{"type",       "id",      "playId",  "title",   "subtitle", "album",
                             "artist",     "durationMs",        "artwork", "section",  "explicit",
                             "catalogId",  "isLibrary",         "canEdit", "trackNumber", "discNumber",
                             "year",       "genre",             "description"};

namespace {
// Songs without play parameters (removed from the catalog, not in the
// storefront) are shown but can't be queued.
bool isPlayable(const QVariant &item) {
    const QVariantMap map = item.toMap();
    return map.value("type") == "song" && !map.value("playId").toString().isEmpty();
}
}

Page::Page(const QString &title, QObject *parent) : QAbstractListModel(parent), m_title(title) {}

int Page::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_items.size());
}

QVariant Page::data(const QModelIndex &index, int role) const {
    const int key = role - Qt::UserRole;
    if (!index.isValid() || index.row() >= m_items.size() || key < 0 || key >= keys.size())
        return {};
    return m_items.at(index.row()).toMap().value(keys.at(key));
}

QHash<int, QByteArray> Page::roleNames() const {
    QHash<int, QByteArray> roles;
    for (int i = 0; i < keys.size(); ++i)
        roles.insert(Qt::UserRole + i, keys.at(i).toUtf8());
    return roles;
}

QVariantMap Page::get(int row) const {
    return row >= 0 && row < m_items.size() ? m_items.at(row).toMap() : QVariantMap();
}

QVariantList Page::songs() const {
    QVariantList songs;
    for (const auto &item : m_items)
        if (isPlayable(item))
            songs << item;
    return songs;
}

qint64 Page::totalDurationMs() const {
    qint64 total = 0;
    for (const auto &item : m_items)
        total += item.toMap().value("durationMs").toLongLong();
    return total;
}

int Page::discCount() const {
    int discs = 1;
    for (const auto &item : m_items)
        discs = std::max(discs, item.toMap().value("discNumber").toInt());
    return discs;
}

int Page::songCount() const {
    return int(std::count_if(m_items.begin(), m_items.end(), isPlayable));
}

int Page::songIndex(int row) const {
    if (row < 0 || row >= m_items.size() || !isPlayable(m_items.at(row)))
        return -1;
    int index = 0;
    for (int i = 0; i < row; ++i)
        if (isPlayable(m_items.at(i)))
            ++index;
    return index;
}

QStringList Page::sections() const {
    QStringList sections;
    for (const auto &item : m_items) {
        const QString section = item.toMap().value("section").toString();
        if (!sections.contains(section))
            sections << section;
    }
    return sections;
}

QVariantList Page::itemsInSection(const QString &section) const {
    QVariantList items;
    for (const auto &item : m_items)
        if (item.toMap().value("section").toString() == section)
            items << item;
    return items;
}

void Page::setHeader(const QString &title, const QString &subtitle, const QString &artwork) {
    m_title = title;
    m_subtitle = subtitle;
    m_artwork = artwork;
    emit headerChanged();
}

void Page::append(const QVariantList &items) {
    if (items.isEmpty())
        return;
    beginInsertRows({}, int(m_items.size()), int(m_items.size() + items.size() - 1));
    m_items += items;
    endInsertRows();
    emit countChanged();
}

void Page::beginRequest() {
    if (m_pending++ == 0)
        emit loadingChanged();
}

void Page::endRequest() {
    if (m_pending > 0 && --m_pending == 0)
        emit loadingChanged();
}

void Page::setError(const QString &error) {
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}
