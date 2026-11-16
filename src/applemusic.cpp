#include "applemusic.h"

#include "page.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQmlEngine>
#include <QSet>
#include <QUrlQuery>

AppleMusic::AppleMusic(QNetworkAccessManager *network, QObject *parent, const QString &baseUrl)
    : QObject(parent), m_network(network), m_baseUrl(baseUrl) {}

void AppleMusic::setStorefront(const QString &storefront) {
    if (storefront.isEmpty() || storefront == m_storefront)
        return;
    m_storefront = storefront;
    emit storefrontChanged();
}

QString AppleMusic::artworkUrl(const QString &artwork, int size) {
    QString url = artwork;
    return url.replace("{w}", QString::number(size)).replace("{h}", QString::number(size));
}

QVariantMap AppleMusic::itemFromResource(const QJsonObject &resource, const QString &section) {
    const QString resourceType = resource.value("type").toString();
    const QJsonObject attributes = resource.value("attributes").toObject();
    const QJsonObject playParams = attributes.value("playParams").toObject();

    QString type;
    if (resourceType.endsWith("songs"))
        type = "song";
    else if (resourceType.endsWith("albums"))
        type = "album";
    else if (resourceType.endsWith("playlists"))
        type = "playlist";
    else if (resourceType.endsWith("artists"))
        type = "artist";
    else if (resourceType == "stations")
        type = "station";
    else
        return {};

    // A library song plays through its catalog id when it has one, so it
    // streams like any other song; uploaded or purchased-only songs keep
    // their library id.
    QString playId;
    if ((type == "song" || type == "station") && !playParams.isEmpty())
        playId = playParams.value("catalogId").toString(playParams.value("id").toString());

    QString subtitle = attributes.value("artistName").toString();
    if (type == "playlist")
        subtitle = attributes.value("curatorName").toString();
    else if (type == "station")
        subtitle = attributes.value("stationProviderName").toString(tr("Station"));

    // The catalog id is what library, favorite and rating calls take; a
    // library item only has one when it came from the catalog.
    const QString id = resource.value("id").toString();
    const bool isLibrary = resourceType.startsWith("library-");
    const QString catalogId = isLibrary ? playParams.value("catalogId").toString() : id;

    return {
        {"type", type},
        {"id", id},
        {"catalogId", catalogId},
        {"isLibrary", isLibrary},
        {"canEdit", attributes.value("canEdit").toBool()},
        {"trackNumber", attributes.value("trackNumber").toInt()},
        {"discNumber", attributes.value("discNumber").toInt(1)},
        {"year", attributes.value("releaseDate").toString().left(4)},
        {"genre", attributes.value("genreNames").toArray().first().toString()},
        // Playlists describe themselves; albums have editorial notes.
        {"description", attributes.value("description").toObject().value("short").toString(
                            attributes.value("description").toObject().value("standard").toString(
                                attributes.value("editorialNotes").toObject().value("short").toString()))},
        {"playId", playId},
        {"title", attributes.value("name").toString()},
        {"subtitle", subtitle},
        {"album", attributes.value("albumName").toString()},
        {"artist", attributes.value("artistName").toString()},
        {"durationMs", attributes.value("durationInMillis").toInteger()},
        {"artwork", attributes.value("artwork").toObject().value("url").toString()},
        {"section", section},
        {"explicit", attributes.value("contentRating").toString() == "explicit"},
    };
}

QVariantList AppleMusic::itemsFromResources(const QJsonValue &data, const QString &section) {
    QVariantList items;
    for (const auto &resource : data.toArray()) {
        const QVariantMap item = itemFromResource(resource.toObject(), section);
        if (!item.isEmpty())
            items << item;
    }
    return items;
}

bool AppleMusic::containsItems(const QVariantList &shelf, const QVariantList &items) {
    if (items.isEmpty())
        return false;
    QSet<QString> ids;
    for (const auto &item : shelf)
        ids.insert(item.toMap().value("id").toString());
    for (const auto &item : items)
        if (!ids.contains(item.toMap().value("id").toString()))
            return false;
    return true;
}

QString AppleMusic::recommendationTitle(const QJsonObject &recommendation) {
    // The title is an object ({"stringForDisplay": …}) in current responses.
    const QJsonValue title = recommendation.value("attributes").toObject().value("title");
    return title.isObject() ? title.toObject().value("stringForDisplay").toString() : title.toString();
}

Page *AppleMusic::newPage(const QString &title, const QString &layout) {
    auto *page = new Page(title);
    page->setLayout(layout);
    // The page belongs to the view that shows it.
    QQmlEngine::setObjectOwnership(page, QQmlEngine::JavaScriptOwnership);
    return page;
}

void AppleMusic::request(Page *page, const QString &path, Handler handler) {
    QNetworkRequest request(QUrl(m_baseUrl + path));
    request.setRawHeader("Authorization", "Bearer " + m_developerToken.toUtf8());
    if (!m_userToken.isEmpty())
        request.setRawHeader("Music-User-Token", m_userToken.toUtf8());
    if (m_sendAppleOrigin)
        request.setRawHeader("Origin", "https://music.apple.com");
    request.setTransferTimeout(20000);

    page->beginRequest();
    QPointer<Page> guard(page);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, guard, handler] {
        reply->deleteLater();
        if (!guard)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 401)
            emit developerTokenRejected();

        // An empty library playlist answers 404 rather than an empty list.
        if (status == 404) {
            guard->endRequest();
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Apple Music request failed" << reply->url().path() << status << reply->readAll().left(300);
            guard->setError(status == 403 ? tr("Apple Music refused the request. Sign in again.")
                                          : tr("Couldn't load from Apple Music: %1").arg(reply->errorString()));
            // Carry on with nothing, so a page built from several requests
            // still shows what the others return.
            handler({});
            guard->endRequest();
            return;
        }
        handler(QJsonDocument::fromJson(reply->readAll()).object());
        guard->endRequest();
    });
}

void AppleMusic::fetchAll(Page *page, const QString &path, const QString &section) {
    request(page, path, [this, page, section](const QJsonObject &response) {
        page->append(itemsFromResources(response.value("data"), section));
        const QString next = response.value("next").toString();
        if (!next.isEmpty())
            fetchAll(page, next, section);
    });
}

Page *AppleMusic::library(const QString &section) {
    static const QHash<QString, QString> paths{
        {"recently-added", "/v1/me/library/recently-added?limit=25"},
        {"playlists", "/v1/me/library/playlists?limit=100"},
        {"albums", "/v1/me/library/albums?limit=100"},
        {"artists", "/v1/me/library/artists?limit=100"},
        {"songs", "/v1/me/library/songs?limit=100"},
    };
    const QHash<QString, QString> titles{
        {"recently-added", tr("Recently Added")}, {"playlists", tr("Playlists")}, {"albums", tr("Albums")},
        {"artists", tr("Artists")},               {"songs", tr("Songs")},
    };
    Page *page = newPage(titles.value(section));
    if (paths.contains(section))
        fetchAll(page, paths.value(section));
    return page;
}

Page *AppleMusic::home() {
    Page *page = newPage(tr("Home"), "shelves");
    // One after another, so the shelves keep the same order on every visit.
    request(page, "/v1/me/recent/played?limit=10", [this, page](const QJsonObject &recent) {
        const QVariantList recentItems = itemsFromResources(recent.value("data"), tr("Recently Played"));
        page->append(recentItems);
        request(page, "/v1/me/recommendations?limit=10", [this, page, recentItems](const QJsonObject &recommendations) {
            for (const auto &value : recommendations.value("data").toArray()) {
                const QJsonObject recommendation = value.toObject();
                const QJsonValue contents = recommendation.value("relationships").toObject().value("contents").toObject().value("data");
                const QVariantList items = itemsFromResources(contents, recommendationTitle(recommendation));
                // Apple often recommends "recently played" too, with more of
                // it; once is enough.
                if (!containsItems(items, recentItems))
                    page->append(items);
            }
            request(page, "/v1/me/recent/radio-stations?limit=10", [page](const QJsonObject &stations) {
                page->append(itemsFromResources(stations.value("data"), tr("Recent Stations")));
            });
        });
    });
    return page;
}

Page *AppleMusic::browse() {
    Page *page = newPage(tr("Browse"), "shelves");
    request(page, "/v1/catalog/" + m_storefront + "/charts?types=songs,albums,playlists&limit=25",
            [page](const QJsonObject &response) {
        const QJsonObject results = response.value("results").toObject();
        for (const QString &type : QStringList{"songs", "albums", "playlists"}) {
            for (const auto &chart : results.value(type).toArray())
                page->append(itemsFromResources(chart.toObject().value("data"), chart.toObject().value("name").toString()));
        }
    });
    return page;
}

Page *AppleMusic::search(const QString &term) {
    Page *page = newPage(tr("Search"));
    page->setHeader(tr("Search"), term, {});
    if (term.trimmed().isEmpty())
        return page;

    QUrlQuery query;
    query.addQueryItem("term", term.trimmed());
    query.addQueryItem("types", "songs,albums,artists,playlists,stations");
    query.addQueryItem("limit", "25");
    const QString path = "/v1/catalog/" + m_storefront + "/search?" + query.toString(QUrl::FullyEncoded);
    request(page, path, [page](const QJsonObject &response) {
        const QJsonObject results = response.value("results").toObject();
        const QList<std::pair<QString, QString>> sections{{"songs", tr("Songs")},         {"albums", tr("Albums")},
                                                          {"artists", tr("Artists")},     {"playlists", tr("Playlists")},
                                                          {"stations", tr("Stations")}};
        for (const auto &[key, title] : sections)
            page->append(itemsFromResources(results.value(key).toObject().value("data"), title));
    });
    return page;
}

void AppleMusic::fill(Page *page, const QVariantMap &item) {
    const QString type = item.value("type").toString();
    const QString id = item.value("id").toString();
    page->setHeader(item.value("title").toString(), item.value("subtitle").toString(), item.value("artwork").toString());

    // Library ids carry a prefix (l. albums, p. playlists, r. artists).
    const QString catalog = "/v1/catalog/" + m_storefront;
    if (type == "album" || type == "playlist" || type == "artist")
        page->setContainer(item);
    // An artist is a page of shelves: top songs, then albums and more.
    if (type == "artist")
        page->setLayout("shelves");
    if (type == "album") {
        fetchAll(page, (id.startsWith("l.") ? "/v1/me/library/albums/" : catalog + "/albums/") + id + "/tracks");
    } else if (type == "playlist") {
        fetchAll(page, (id.startsWith("p.") ? "/v1/me/library/playlists/" : catalog + "/playlists/") + id + "/tracks?limit=100");
    } else if (type == "artist" && id.startsWith("r.")) {
        fetchAll(page, "/v1/me/library/artists/" + id + "/albums?limit=100", tr("Albums"));
    } else if (type == "artist") {
        request(page, catalog + "/artists/" + id + "?views=top-songs,full-albums,singles,similar-artists",
                [page](const QJsonObject &response) {
            const QJsonObject views =
                response.value("data").toArray().first().toObject().value("views").toObject();
            page->append(itemsFromResources(views.value("top-songs").toObject().value("data"), tr("Top Songs")));
            page->append(itemsFromResources(views.value("full-albums").toObject().value("data"), tr("Albums")));
            page->append(itemsFromResources(views.value("singles").toObject().value("data"), tr("Singles & EPs")));
            page->append(itemsFromResources(views.value("similar-artists").toObject().value("data"), tr("Similar Artists")));
        });
    }
}

Page *AppleMusic::pageFromItems(const QString &title, const QVariantList &items) {
    Page *page = newPage(title);
    QVariantList withoutSections;
    for (const auto &item : items) {
        QVariantMap map = item.toMap();
        map.remove("section");
        withoutSections << map;
    }
    page->append(withoutSections);
    return page;
}

Page *AppleMusic::open(const QVariantMap &item) {
    Page *page = newPage(item.value("title").toString());
    fill(page, item);
    return page;
}

Page *AppleMusic::openFromTrack(const QVariantMap &track, const QString &type) {
    Page *page = newPage(type == "artist" ? track.value("artist").toString() : track.value("album").toString());
    page->setHeader(page->title(), {}, {});

    // A song or album only knows its album and artist by name; ask the API
    // which ones they are, then show that page. Library-only items ask the
    // library.
    const bool isSong = track.value("type", "song").toString() == "song";
    QString id = isSong ? track.value("playId").toString() : track.value("catalogId").toString();
    if (id.isEmpty())
        id = track.value("id").toString();
    if (id.isEmpty())
        return page;
    const bool isLibrary = id.startsWith("i.") || id.startsWith("l.");
    const QString collection = isSong ? "songs/" : "albums/";
    const QString relationship = type == "artist" ? "artists" : "albums";
    const QString path = (isLibrary ? "/v1/me/library/" : "/v1/catalog/" + m_storefront + "/") + collection + id
                         + "?include=" + relationship;
    request(page, path, [this, page, relationship](const QJsonObject &response) {
        const QJsonArray related = response.value("data").toArray().first().toObject().value("relationships")
                                       .toObject().value(relationship).toObject().value("data").toArray();
        const QVariantMap item = related.isEmpty() ? QVariantMap() : itemFromResource(related.first().toObject());
        if (item.isEmpty()) {
            if (page->error().isEmpty())
                page->setError(tr("Apple Music doesn't know where this track comes from."));
            return;
        }
        fill(page, item);
    });
    return page;
}

QString AppleMusic::resourceType(const QVariantMap &item) {
    static const QHash<QString, QString> types{
        {"song", "songs"}, {"album", "albums"}, {"playlist", "playlists"}, {"artist", "artists"}, {"station", "stations"}};
    return types.value(item.value("type").toString());
}

QJsonObject AppleMusic::playlistTrack(const QVariantMap &item) {
    const QString catalogId = item.value("catalogId").toString();
    if (!catalogId.isEmpty())
        return {{"id", catalogId}, {"type", "songs"}};
    return {{"id", item.value("id").toString()}, {"type", "library-songs"}};
}

void AppleMusic::send(const QByteArray &method, const QString &path, const QByteArray &body,
                      std::function<void(int, const QJsonObject &)> done) {
    QNetworkRequest request(QUrl(m_baseUrl + path));
    request.setRawHeader("Authorization", "Bearer " + m_developerToken.toUtf8());
    request.setRawHeader("Music-User-Token", m_userToken.toUtf8());
    if (m_sendAppleOrigin)
        request.setRawHeader("Origin", "https://music.apple.com");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(20000);
    QNetworkReply *reply = m_network->sendCustomRequest(request, method, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, method, path, done] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        if (status == 401)
            emit developerTokenRejected();
        if (status >= 400 && status != 404)
            qWarning() << "Apple Music request failed" << method << QUrl(path).path() << status << data.left(300);
        done(status, QJsonDocument::fromJson(data).object());
    });
}

void AppleMusic::loadState(const QVariantMap &item) {
    const QString type = resourceType(item);
    const QString catalogId = item.value("catalogId").toString();
    const QString id = item.value("id").toString();
    if (type.isEmpty() || catalogId.isEmpty()) {
        emit stateLoaded(id, {{"known", false}, {"inLibrary", item.value("isLibrary").toBool()}});
        return;
    }
    const QString path = "/v1/catalog/" + m_storefront + "/" + type + "?ids=" + catalogId + "&fields[" + type
                         + "]=inLibrary,inFavorites,personalRating";
    send("GET", path, {}, [this, id, item](int status, const QJsonObject &response) {
        const QJsonObject attributes = response.value("data").toArray().first().toObject().value("attributes").toObject();
        emit stateLoaded(id, {
            {"known", status == 200},
            {"inLibrary", attributes.value("inLibrary").toBool(item.value("isLibrary").toBool())},
            {"inFavorites", attributes.value("inFavorites").toBool()},
            {"rating", attributes.value("personalRating").toInt()},
        });
    });
}

void AppleMusic::addToLibrary(const QVariantMap &item) {
    const QString type = resourceType(item);
    const QString title = item.value("title").toString();
    send("POST", "/v1/me/library?ids[" + type + "]=" + item.value("catalogId").toString(), {},
         [this, title](int status, const QJsonObject &) {
        const bool ok = status >= 200 && status < 300;
        if (ok)
            emit libraryChanged();
        emit actionFinished(ok ? tr("Added %1 to your library").arg(title)
                               : tr("Couldn't add %1 to your library (%2)").arg(title).arg(status), ok);
    });
}

void AppleMusic::setFavorite(const QVariantMap &item, bool favorite) {
    const QString title = item.value("title").toString();
    send(favorite ? "POST" : "DELETE",
         "/v1/me/favorites?ids[" + resourceType(item) + "]=" + item.value("catalogId").toString(), {},
         [this, title, favorite](int status, const QJsonObject &) {
        const bool ok = status >= 200 && status < 300;
        emit actionFinished(!ok ? tr("Couldn't change the favorite (%1)").arg(status)
                            : favorite ? tr("%1 is a favorite").arg(title)
                                       : tr("%1 is no longer a favorite").arg(title), ok);
    });
}

void AppleMusic::setSuggestLess(const QVariantMap &item, bool less) {
    const QString title = item.value("title").toString();
    const QString path = "/v1/me/ratings/" + resourceType(item) + "/" + item.value("catalogId").toString();
    const QByteArray body = less ? QJsonDocument(QJsonObject{{"type", "rating"}, {"attributes", QJsonObject{{"value", -1}}}})
                                       .toJson(QJsonDocument::Compact)
                                 : QByteArray();
    send(less ? "PUT" : "DELETE", path, body, [this, title, less](int status, const QJsonObject &) {
        const bool ok = status >= 200 && status < 300;
        emit actionFinished(!ok ? tr("Couldn't change the suggestion (%1)").arg(status)
                            : less ? tr("Apple Music will suggest less like %1").arg(title)
                                   : tr("Suggestions for %1 are back to normal").arg(title), ok);
    });
}

void AppleMusic::addToPlaylist(const QVariantMap &item, const QVariantMap &playlist) {
    const QString title = item.value("title").toString();
    const QString name = playlist.value("title").toString();
    const QByteArray body = QJsonDocument(QJsonObject{{"data", QJsonArray{playlistTrack(item)}}}).toJson(QJsonDocument::Compact);
    send("POST", "/v1/me/library/playlists/" + playlist.value("id").toString() + "/tracks", body,
         [this, title, name](int status, const QJsonObject &) {
        const bool ok = status >= 200 && status < 300;
        if (ok)
            emit libraryChanged();
        emit actionFinished(ok ? tr("Added %1 to %2").arg(title, name)
                               : tr("Couldn't add %1 to %2 (%3)").arg(title, name).arg(status), ok);
    });
}

void AppleMusic::createPlaylist(const QString &name, const QVariantMap &item) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return;
    QJsonObject playlist{{"attributes", QJsonObject{{"name", trimmed}}}};
    if (item.value("type") == "song")
        playlist.insert("relationships", QJsonObject{{"tracks", QJsonObject{{"data", QJsonArray{playlistTrack(item)}}}}});
    send("POST", "/v1/me/library/playlists", QJsonDocument(playlist).toJson(QJsonDocument::Compact),
         [this, trimmed](int status, const QJsonObject &) {
        const bool ok = status >= 200 && status < 300;
        if (ok)
            emit libraryChanged();
        emit actionFinished(ok ? tr("Created the playlist %1").arg(trimmed)
                               : tr("Couldn't create the playlist %1 (%2)").arg(trimmed).arg(status), ok);
    });
}
