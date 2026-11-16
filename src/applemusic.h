#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QVariantMap>

#include <functional>

class Page;
class QNetworkAccessManager;

// The Apple Music REST API (api.music.apple.com): catalog search, the user's
// library, and the tracks of an album, playlist or artist. Every call returns
// a Page that fills itself as responses arrive, following `next` links until
// the list is complete.
class AppleMusic : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString storefront READ storefront NOTIFY storefrontChanged)

public:
    explicit AppleMusic(QNetworkAccessManager *network, QObject *parent = nullptr,
                        const QString &baseUrl = "https://api.music.apple.com");

    QString storefront() const { return m_storefront; }
    void setDeveloperToken(const QString &token) { m_developerToken = token; }
    void setUserToken(const QString &token) { m_userToken = token; }
    // The web player's token is only accepted from Apple's own site; a
    // token signed with the user's own key needs no such header.
    void setSendAppleOrigin(bool send) { m_sendAppleOrigin = send; }
    void setStorefront(const QString &storefront);

    // section: recently-added, playlists, albums, artists, songs.
    Q_INVOKABLE Page *library(const QString &section);
    Q_INVOKABLE Page *search(const QString &term);
    // Recently played, Apple's recommendations and recent stations.
    Q_INVOKABLE Page *home();
    // The storefront's charts.
    Q_INVOKABLE Page *browse();
    // Opens an album, playlist or artist item from another page.
    Q_INVOKABLE Page *open(const QVariantMap &item);
    // A list page of items another page already has ("Show all").
    Q_INVOKABLE Page *pageFromItems(const QString &title, const QVariantList &items);
    // Opens the album or artist ("album", "artist") a playing track is from.
    Q_INVOKABLE Page *openFromTrack(const QVariantMap &track, const QString &type);

    // The item menu's actions. Each reports back with actionFinished.
    // Whether an item is in the library, a favorite, or rated down; answers
    // with stateLoaded.
    Q_INVOKABLE void loadState(const QVariantMap &item);
    Q_INVOKABLE void addToLibrary(const QVariantMap &item);
    Q_INVOKABLE void setFavorite(const QVariantMap &item, bool favorite);
    // Apple's "Suggest Less": a song rated down.
    Q_INVOKABLE void setSuggestLess(const QVariantMap &item, bool less);
    Q_INVOKABLE void addToPlaylist(const QVariantMap &item, const QVariantMap &playlist);
    Q_INVOKABLE void createPlaylist(const QString &name, const QVariantMap &item);

    // The API's resource type for an item ("songs", "albums", …).
    static QString resourceType(const QVariantMap &item);
    // A song as a playlist track: catalog songs by catalog id, songs only in
    // the library by library id.
    static QJsonObject playlistTrack(const QVariantMap &item);

    Q_INVOKABLE static QString artworkUrl(const QString &artwork, int size);
    static QVariantMap itemFromResource(const QJsonObject &resource, const QString &section = {});
    static QVariantList itemsFromResources(const QJsonValue &data, const QString &section = {});
    static QString recommendationTitle(const QJsonObject &recommendation);
    // Whether a shelf holds every one of the items (by id).
    static bool containsItems(const QVariantList &shelf, const QVariantList &items);

signals:
    void storefrontChanged();
    // The developer token was refused; it has expired or Apple rotated it.
    void developerTokenRejected();
    void stateLoaded(const QString &id, const QVariantMap &state);
    // A library change, so library pages know to reload.
    void libraryChanged();
    void actionFinished(const QString &message, bool ok);

private:
    using Handler = std::function<void(const QJsonObject &)>;

    Page *newPage(const QString &title, const QString &layout = "list");
    // Loads an album's or playlist's tracks, or an artist's songs and albums.
    void fill(Page *page, const QVariantMap &item);
    // Calls the handler with the response, or with an empty object after
    // showing the error on the page.
    void request(Page *page, const QString &path, Handler handler);
    // A request outside a page: the item menu's reads and writes.
    void send(const QByteArray &method, const QString &path, const QByteArray &body,
              std::function<void(int status, const QJsonObject &response)> done);
    // Fetches a list endpoint and every page after it.
    void fetchAll(Page *page, const QString &path, const QString &section = {});

    QNetworkAccessManager *m_network;
    QString m_baseUrl;
    QString m_developerToken;
    QString m_userToken;
    QString m_storefront = "us";
    bool m_sendAppleOrigin = false;
};
