#pragma once

#include <QAbstractListModel>
#include <QVariantList>

// One screen of results: a library section, search results, or an album,
// playlist or artist. Items are flat maps (see AppleMusic::itemFromResource)
// so QML reads them as roles and the player takes them as they are.
class Page : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY headerChanged)
    Q_PROPERTY(QString subtitle READ subtitle NOTIFY headerChanged)
    Q_PROPERTY(QString artwork READ artwork NOTIFY headerChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int songCount READ songCount NOTIFY countChanged)
    // "list" for rows, "shelves" for a row of cards per section.
    Q_PROPERTY(QString layout READ layout NOTIFY headerChanged)
    Q_PROPERTY(QStringList sections READ sections NOTIFY countChanged)
    // The album, playlist or artist this page shows; album and playlist
    // songs play from it.
    Q_PROPERTY(QVariantMap container READ container NOTIFY headerChanged)
    Q_PROPERTY(qint64 totalDurationMs READ totalDurationMs NOTIFY countChanged)
    // More than one means the album shows disc headings.
    Q_PROPERTY(int discCount READ discCount NOTIFY countChanged)

public:
    static const QStringList keys;

    explicit Page(const QString &title = {}, QObject *parent = nullptr);

    QString title() const { return m_title; }
    QString subtitle() const { return m_subtitle; }
    QString artwork() const { return m_artwork; }
    bool isLoading() const { return m_pending > 0; }
    QString layout() const { return m_layout; }
    QVariantMap container() const { return m_container; }
    void setContainer(const QVariantMap &container) {
        m_container = container;
        emit headerChanged();
    }
    void setLayout(const QString &layout) {
        m_layout = layout;
        emit headerChanged();
    }
    // Section names in the order they first appear.
    QStringList sections() const;
    QString error() const { return m_error; }

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap get(int row) const;
    // The playable songs on the page, and where a row lands among them, so
    // playing a row queues everything after it.
    Q_INVOKABLE QVariantList songs() const;
    int songCount() const;
    qint64 totalDurationMs() const;
    int discCount() const;
    Q_INVOKABLE int songIndex(int row) const;
    Q_INVOKABLE QVariantList itemsInSection(const QString &section) const;

    void setHeader(const QString &title, const QString &subtitle, const QString &artwork);
    void append(const QVariantList &items);
    void beginRequest();
    void endRequest();
    void setError(const QString &error);

signals:
    void headerChanged();
    void loadingChanged();
    void errorChanged();
    void countChanged();

private:
    QString m_title, m_subtitle, m_artwork, m_error;
    QString m_layout = "list";
    QVariantMap m_container;
    QVariantList m_items;
    int m_pending = 0;
};
