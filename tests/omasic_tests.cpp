#include <QDBusObjectPath>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSettings>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "applemusic.h"
#include "developertoken.h"
#include "mpris.h"
#include "outputs.h"
#include "page.h"
#include "player.h"
#include "theme.h"
#include "translator.h"

namespace {
QString jwt(const QJsonObject &payload) {
    const auto encode = [](const QByteArray &data) {
        return data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    };
    return encode("{\"alg\":\"ES256\",\"typ\":\"JWT\"}") + "." + encode(QJsonDocument(payload).toJson(QJsonDocument::Compact))
           + ".c2lnbmF0dXJl";
}

QJsonObject resource(const QByteArray &json) {
    return QJsonDocument::fromJson(json).object();
}

QVariantMap song(const QString &playId, const QString &title = "Song") {
    return {{"type", "song"}, {"playId", playId}, {"title", title}, {"durationMs", 200000}};
}
}

class OmasicTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        // Player keeps the volume in QSettings; keep tests out of the user's.
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName("omasic-tests");
        QCoreApplication::setApplicationName("omasic-tests");
    }

    void scriptPathsPutTheEntryBundleFirst() {
        const QString html = "<link href=\"/assets/vendor~a1.js\"><script src=\"/assets/index~B8RKPfmaCI.js\"></script> <script src=\"/assets/vendor~a1.js\"></script>";
        QCOMPARE(DeveloperToken::scriptPaths(html), QStringList({"/assets/index~B8RKPfmaCI.js", "/assets/vendor~a1.js"}));
    }

    void findTokenPicksTheWebPlayerToken() {
        const QString other = jwt({{"iss", "M62YD85FTQ"}, {"exp", 1799948708}});
        const QString web = jwt({{"iss", "AMPWebPlay"}, {"exp", 1799948708}});
        const QString script = "const a=\"" + other + "\",b=\"" + web + "\";";
        QCOMPARE(DeveloperToken::findToken(script), web);
        QVERIFY(DeveloperToken::findToken("const a=\"" + other + "\";").isEmpty());
    }

    void tokenIsRefreshedADayBeforeItExpires() {
        const QDateTime now = QDateTime::fromSecsSinceEpoch(1800000000, QTimeZone::UTC);
        QVERIFY(DeveloperToken::isUsable(jwt({{"iss", "AMPWebPlay"}, {"exp", 1800000000 + 2 * 86400}}), now));
        QVERIFY(!DeveloperToken::isUsable(jwt({{"iss", "AMPWebPlay"}, {"exp", 1800000000 + 3600}}), now));
        QVERIFY(!DeveloperToken::isUsable("not a token", now));
    }

    void cachedTokenIsUsedWithoutTheNetwork() {
        QTemporaryDir dir;
        const QString token = jwt({{"iss", "AMPWebPlay"}, {"exp", QDateTime::currentSecsSinceEpoch() + 10 * 86400}});
        QFile cache(dir.filePath("developer-token"));
        QVERIFY(cache.open(QIODevice::WriteOnly));
        cache.write(token.toUtf8());
        cache.close();

        // The web player's token is only used once the user opts in.
        DeveloperToken developerToken(nullptr, dir.filePath("developer-token"), "test");
        developerToken.refresh();
        QVERIFY(developerToken.token().isEmpty());
        developerToken.setUseWebToken(true);
        QCOMPARE(developerToken.token(), token);
        QVERIFY(!developerToken.isLoading());
        QVERIFY(!developerToken.isOfficial());
        QSettings().clear();
    }

    void rawSignatureUnpacksDer() {
        // r with a leading zero (high bit set), s shorter than 32 bytes.
        QByteArray r(32, '\x81'), s(31, '\x22');
        QByteArray der;
        der += char(0x30);
        der += char(2 + 33 + 2 + 31);
        der += char(0x02); der += char(33); der += '\0'; der += r;
        der += char(0x02); der += char(31); der += s;
        const QByteArray raw = DeveloperToken::rawSignature(der);
        QCOMPARE(raw.size(), 64);
        QCOMPARE(raw.left(32), r);
        QCOMPARE(raw.mid(32), QByteArray(1, '\0') + s);
        QVERIFY(DeveloperToken::rawSignature("nonsense").isEmpty());
    }

    void keySignsATokenOpensslVerifies() {
        if (QStandardPaths::findExecutable("openssl").isEmpty())
            QSKIP("needs openssl");
        QTemporaryDir dir;
        const QString key = dir.filePath("AuthKey.p8"), pub = dir.filePath("pub.pem");
        QCOMPARE(QProcess::execute("openssl", {"genpkey", "-algorithm", "EC", "-pkeyopt", "ec_paramgen_curve:P-256",
                                               "-out", key}), 0);
        QCOMPARE(QProcess::execute("openssl", {"pkey", "-in", key, "-pubout", "-out", pub}), 0);

        DeveloperToken token(nullptr, dir.filePath("cache"), "test");
        token.setKey("TEAM123456", "KEY7890123", key);
        QTRY_VERIFY(!token.token().isEmpty());
        QVERIFY(token.isOfficial());

        const QStringList parts = token.token().split('.');
        QCOMPARE(parts.size(), 3);
        const QJsonObject header = QJsonDocument::fromJson(QByteArray::fromBase64(parts[0].toLatin1(), QByteArray::Base64UrlEncoding)).object();
        const QJsonObject payload = QJsonDocument::fromJson(QByteArray::fromBase64(parts[1].toLatin1(), QByteArray::Base64UrlEncoding)).object();
        QCOMPARE(header.value("alg").toString(), QString("ES256"));
        QCOMPARE(header.value("kid").toString(), QString("KEY7890123"));
        QCOMPARE(payload.value("iss").toString(), QString("TEAM123456"));
        QVERIFY(DeveloperToken::isUsable(token.token(), QDateTime::currentDateTimeUtc()));

        // Back to DER, and let openssl check the signature with the public key.
        const QByteArray raw = QByteArray::fromBase64(parts[2].toLatin1(), QByteArray::Base64UrlEncoding);
        QCOMPARE(raw.size(), 64);
        auto integer = [](QByteArray value) {
            while (value.size() > 1 && value[0] == '\0' && !(uchar(value[1]) & 0x80)) value.remove(0, 1);
            if (uchar(value[0]) & 0x80) value.prepend('\0');
            return QByteArray(1, 0x02) + char(value.size()) + value;
        };
        const QByteArray body = integer(raw.left(32)) + integer(raw.mid(32));
        QFile signature(dir.filePath("sig.der"));
        QVERIFY(signature.open(QIODevice::WriteOnly));
        signature.write(QByteArray(1, 0x30) + char(body.size()) + body);
        signature.close();
        QFile input(dir.filePath("input"));
        QVERIFY(input.open(QIODevice::WriteOnly));
        input.write((parts[0] + "." + parts[1]).toLatin1());
        input.close();
        QCOMPARE(QProcess::execute("openssl", {"dgst", "-sha256", "-verify", pub, "-signature", signature.fileName(),
                                               input.fileName()}), 0);
        token.setKey("", "", "");
        QSettings().clear();
    }

    void noKeyAndNoOptInNeedsSetup() {
        QTemporaryDir dir;
        DeveloperToken token(nullptr, dir.filePath("cache"), "test");
        QVERIFY(token.needsSetup());
        token.refresh();
        QVERIFY(token.token().isEmpty());
        QCOMPARE(DeveloperToken::expandPath("~/key.p8"), QDir::homePath() + "/key.p8");
    }

    void catalogSongBecomesAnItem() {
        const QVariantMap item = AppleMusic::itemFromResource(resource("{ \"id\": \"1239489069\", \"type\": \"songs\", \"attributes\": {\"name\": \"Karma Police\", \"artistName\": \"Radiohead\", \"albumName\": \"OK Computer\", \"durationInMillis\": 261000, \"contentRating\": \"explicit\", \"artwork\": {\"url\": \"https://example.com/{w}x{h}bb.jpg\"}, \"playParams\": {\"id\": \"1239489069\", \"kind\": \"song\"}}}"), "Songs");
        QCOMPARE(item.value("type").toString(), QString("song"));
        QCOMPARE(item.value("playId").toString(), QString("1239489069"));
        QCOMPARE(item.value("subtitle").toString(), QString("Radiohead"));
        QCOMPARE(item.value("album").toString(), QString("OK Computer"));
        QCOMPARE(item.value("durationMs").toLongLong(), 261000);
        QCOMPARE(item.value("section").toString(), QString("Songs"));
        QVERIFY(item.value("explicit").toBool());
    }

    void librarySongPlaysThroughItsCatalogId() {
        const QVariantMap withCatalog = AppleMusic::itemFromResource(resource("{ \"id\": \"i.abc\", \"type\": \"library-songs\", \"attributes\": {\"name\": \"A\", \"playParams\": {\"id\": \"i.abc\", \"kind\": \"song\", \"isLibrary\": true, \"catalogId\": \"123\"}}}"));
        QCOMPARE(withCatalog.value("playId").toString(), QString("123"));

        const QVariantMap uploaded = AppleMusic::itemFromResource(resource("{ \"id\": \"i.def\", \"type\": \"library-songs\", \"attributes\": {\"name\": \"B\", \"playParams\": {\"id\": \"i.def\", \"kind\": \"song\", \"isLibrary\": true}}}"));
        QCOMPARE(uploaded.value("playId").toString(), QString("i.def"));

        const QVariantMap unavailable = AppleMusic::itemFromResource(resource("{ \"id\": \"i.ghi\", \"type\": \"library-songs\", \"attributes\": {\"name\": \"C\"}}"));
        QVERIFY(unavailable.value("playId").toString().isEmpty());
    }

    void playlistShowsItsCuratorAndUnknownTypesAreSkipped() {
        const QVariantMap playlist = AppleMusic::itemFromResource(resource("{ \"id\": \"pl.1\", \"type\": \"playlists\", \"attributes\": {\"name\": \"New Music Daily\", \"curatorName\": \"Apple Music\"}}"));
        QCOMPARE(playlist.value("type").toString(), QString("playlist"));
        QCOMPARE(playlist.value("subtitle").toString(), QString("Apple Music"));
        QVERIFY(AppleMusic::itemFromResource(resource("{\"id\": \"mv.1\", \"type\": \"music-videos\", \"attributes\": {}}")).isEmpty());
    }

    void artworkUrlFillsTheSize() {
        QCOMPARE(AppleMusic::artworkUrl("https://example.com/{w}x{h}bb.jpg", 88), QString("https://example.com/88x88bb.jpg"));
    }

    void pageQueuesOnlyPlayableSongs() {
        Page page;
        page.append({song("1"), QVariantMap{{"type", "album"}, {"id", "a"}}, song(""), song("2")});
        QCOMPARE(page.songCount(), 2);
        QCOMPARE(page.songs().size(), 2);
        QCOMPARE(page.songIndex(0), 0);
        QCOMPARE(page.songIndex(1), -1);
        QCOMPARE(page.songIndex(2), -1);
        QCOMPARE(page.songIndex(3), 1);
    }

    void playSendsTheQueueToThePage() {
        Player player("/widevine.so");
        QSignalSpy commands(&player, &Player::command);
        player.play({song("1"), song("2"), song("3")}, 1);
        QCOMPARE(commands.size(), 1);
        QCOMPARE(commands.at(0).at(0).toString(), QString("play"));
        const QVariantMap args = commands.at(0).at(1).toMap();
        QCOMPARE(args.value("ids").toStringList(), QStringList({"1", "2", "3"}));
        QCOMPARE(args.value("index").toInt(), 1);
        QCOMPARE(player.current().value("playId").toString(), QString("2"));
    }

    void nowPlayingFollowsWhatMusicKitPlays() {
        Player player("/widevine.so");
        player.play({song("1"), song("2"), song("3")}, 0);
        player.reportNowPlaying(song("3", "Third"));
        QCOMPARE(player.index(), 2);
        QCOMPARE(player.current().value("title").toString(), QString("Third"));
    }

    void queueFollowsMusicKitsOrder() {
        Player player("/widevine.so");
        player.play({song("1"), song("2"), song("3")}, 0);
        // Shuffled by MusicKit.
        player.reportQueue({song("1"), song("3"), song("2")}, 1);
        QCOMPARE(player.queue().at(1).toMap().value("playId").toString(), QString("3"));
        QCOMPARE(player.index(), 1);
        // MusicKit reports an empty queue before it has loaded the new one.
        player.reportQueue({}, -1);
        QCOMPARE(player.queue().size(), 3);
    }

    void stationOnlyGoesForward() {
        Player player("/widevine.so");
        QSignalSpy commands(&player, &Player::command);
        player.playStation({{"type", "station"}, {"id", "ra.1"}, {"playId", "ra.1"}, {"title", "Radio"}});
        QCOMPARE(commands.last().at(0).toString(), QString("station"));
        QCOMPARE(commands.last().at(1).toMap().value("id").toString(), QString("ra.1"));
        QVERIFY(player.canNext());
        QVERIFY(!player.canPrevious());
        // Playing songs leaves the station.
        player.play({song("1")}, 0);
        QVERIFY(player.station().isEmpty());
        QVERIFY(!player.canNext());
    }

    void repeatCyclesOffAllOne() {
        Player player("/widevine.so");
        QSignalSpy commands(&player, &Player::command);
        player.cycleRepeat();
        QCOMPARE(player.repeat(), int(Player::RepeatAll));
        QCOMPARE(commands.last().at(1).toMap().value("mode").toInt(), int(Player::RepeatAll));
        player.cycleRepeat();
        QCOMPARE(player.repeat(), int(Player::RepeatOne));
        player.cycleRepeat();
        QCOMPARE(player.repeat(), int(Player::RepeatOff));
    }

    void stationBecomesAnItem() {
        const QVariantMap item = AppleMusic::itemFromResource(resource("{\"id\": \"ra.123\", \"type\": \"stations\", "
            "\"attributes\": {\"name\": \"Chill\", \"playParams\": {\"id\": \"ra.123\", \"kind\": \"radioStation\"}}}"));
        QCOMPARE(item.value("type").toString(), QString("station"));
        QCOMPARE(item.value("playId").toString(), QString("ra.123"));
    }

    void recommendationTitleIsReadFromItsObject() {
        QCOMPARE(AppleMusic::recommendationTitle(resource("{\"attributes\": {\"title\": {\"stringForDisplay\": \"Made for You\"}}}")),
                 QString("Made for You"));
        QCOMPARE(AppleMusic::recommendationTitle(resource("{\"attributes\": {\"title\": \"Plain\"}}")), QString("Plain"));
    }

    void containsItemsChecksEveryId() {
        const QVariantList recent{QVariantMap{{"id", "1"}}, QVariantMap{{"id", "2"}}};
        const QVariantList longer{QVariantMap{{"id", "2"}}, QVariantMap{{"id", "1"}}, QVariantMap{{"id", "3"}}};
        QVERIFY(AppleMusic::containsItems(longer, recent));
        QVERIFY(!AppleMusic::containsItems(recent, longer));
        QVERIFY(!AppleMusic::containsItems(longer, {}));
    }

    void libraryItemsKeepTheirCatalogId() {
        const QVariantMap album = AppleMusic::itemFromResource(resource("{\"id\": \"l.abc\", \"type\": \"library-albums\", "
            "\"attributes\": {\"name\": \"A\", \"playParams\": {\"id\": \"l.abc\", \"isLibrary\": true, \"catalogId\": \"999\"}}}"));
        QCOMPARE(album.value("catalogId").toString(), QString("999"));
        QVERIFY(album.value("isLibrary").toBool());
        const QVariantMap playlist = AppleMusic::itemFromResource(resource("{\"id\": \"p.1\", \"type\": \"library-playlists\", "
            "\"attributes\": {\"name\": \"Mine\", \"canEdit\": true}}"));
        QVERIFY(playlist.value("catalogId").toString().isEmpty());
        QVERIFY(playlist.value("canEdit").toBool());
        const QVariantMap catalog = AppleMusic::itemFromResource(resource("{\"id\": \"42\", \"type\": \"albums\", \"attributes\": {}}"));
        QCOMPARE(catalog.value("catalogId").toString(), QString("42"));
    }

    void itemsMapToApiTypesAndPlaylistTracks() {
        QCOMPARE(AppleMusic::resourceType({{"type", "song"}}), QString("songs"));
        QCOMPARE(AppleMusic::resourceType({{"type", "station"}}), QString("stations"));
        QCOMPARE(AppleMusic::playlistTrack({{"catalogId", "1"}, {"id", "i.1"}}).value("type").toString(), QString("songs"));
        const QJsonObject libraryOnly = AppleMusic::playlistTrack({{"catalogId", ""}, {"id", "i.2"}});
        QCOMPARE(libraryOnly.value("type").toString(), QString("library-songs"));
        QCOMPARE(libraryOnly.value("id").toString(), QString("i.2"));
    }

    void queueDescriptorsNameWhatToQueue() {
        QCOMPARE(Player::queueDescriptor(song("7")), QVariantMap({{"song", "7"}}));
        QCOMPARE(Player::queueDescriptor({{"type", "album"}, {"id", "l.1"}, {"catalogId", "55"}}), QVariantMap({{"album", "55"}}));
        QCOMPARE(Player::queueDescriptor({{"type", "playlist"}, {"id", "p.9"}, {"catalogId", ""}}), QVariantMap({{"playlist", "p.9"}}));
        QVERIFY(Player::queueDescriptor({{"type", "station"}, {"id", "ra.1"}}).isEmpty());
        QVERIFY(Player::queueDescriptor(song("")).isEmpty());
    }

    void playNextAndAddToQueueInsert() {
        Player player("/widevine.so");
        QSignalSpy commands(&player, &Player::command);
        player.playNext(song("7"));
        QCOMPARE(commands.last().at(0).toString(), QString("insert"));
        QCOMPARE(commands.last().at(1).toMap().value("where").toString(), QString("next"));
        player.addToQueue(song("8"));
        QCOMPARE(commands.last().at(1).toMap().value("where").toString(), QString("later"));
        QCOMPARE(commands.last().at(1).toMap().value("descriptor").toMap().value("song").toString(), QString("8"));
    }

    void songsFromAnAlbumQueueTheAlbum() {
        const QVariantMap album{{"type", "album"}, {"id", "l.1"}, {"catalogId", "100"}};
        QCOMPARE(Player::containerQueue(album, song("7"), 2), QVariantMap({{"album", "100"}, {"startWith", "7"}}));
        const QVariantMap mine{{"type", "playlist"}, {"id", "p.1"}, {"catalogId", ""}};
        QCOMPARE(Player::containerQueue(mine, song("7"), 2), QVariantMap({{"playlist", "p.1"}, {"startWith", 2}}));
        QVERIFY(Player::containerQueue({}, song("7"), 2).isEmpty());
        QVERIFY(Player::containerQueue({{"type", "artist"}, {"id", "1"}}, song("7"), 2).isEmpty());

        Player player("/widevine.so");
        QSignalSpy commands(&player, &Player::command);
        player.play({song("6"), song("7")}, 1, album);
        QCOMPARE(commands.last().at(1).toMap().value("container").toMap().value("album").toString(), QString("100"));
    }

    void autoplayAndQualityAreSentAndRemembered() {
        {
            Player player("/widevine.so");
            QSignalSpy commands(&player, &Player::command);
            player.setAutoplay(true);
            QCOMPARE(commands.last().at(0).toString(), QString("autoplay"));
            QVERIFY(commands.last().at(1).toMap().value("on").toBool());
            player.setBitrate(64);
            QCOMPARE(commands.last().at(1).toMap().value("kbps").toInt(), 64);
            // Anything but Standard is High.
            player.setBitrate(128);
            QCOMPARE(player.bitrate(), 256);
            player.setBitrate(64);
        }
        Player restarted("/widevine.so");
        QVERIFY(restarted.autoplay());
        QCOMPARE(restarted.bitrate(), 64);
        // MusicKit turning autoplay off is shown, not saved.
        restarted.reportAutoplay(false);
        QVERIFY(!restarted.autoplay());
        QVERIFY(Player("/widevine.so").autoplay());
        QSettings().clear();
    }

    void pageGroupsItemsBySection() {
        Page page;
        QVariantMap first = song("1"), second = song("2"), third = song("3");
        first["section"] = "B";
        second["section"] = "A";
        third["section"] = "B";
        page.append({first, second, third});
        QCOMPARE(page.sections(), QStringList({"B", "A"}));
        QCOMPARE(page.itemsInSection("B").size(), 2);
        QCOMPARE(page.itemsInSection("A").first().toMap().value("playId").toString(), QString("2"));
    }

    void outputsParseSinksAndFindOwnStreams() {
        const QVariantList devices = AudioOutputs::parseSinks(
            "[{\"name\": \"speakers\", \"description\": \"Speakers\"}, {\"name\": \"usb\"}]", "usb");
        QCOMPARE(devices.size(), 2);
        QCOMPARE(devices.at(0).toMap().value("description").toString(), QString("Speakers"));
        QVERIFY(!devices.at(0).toMap().value("isDefault").toBool());
        QCOMPARE(devices.at(1).toMap().value("description").toString(), QString("usb"));
        QVERIFY(devices.at(1).toMap().value("isDefault").toBool());

        const QList<int> streams = AudioOutputs::streamsFor(
            "[{\"index\": 5, \"properties\": {\"application.name\": \"Firefox\"}},"
            " {\"index\": 9, \"properties\": {\"application.name\": \"omasic\"}}]", "omasic");
        QCOMPARE(streams, QList<int>({9}));
    }

    void outputsReportAMissingPactl() {
        AudioOutputs outputs("omasic", "/nonexistent/pactl");
        outputs.refresh();
        QTRY_VERIFY(!outputs.error().isEmpty());
    }

    void mprisLoopStatusMapsRepeat() {
        QCOMPARE(Mpris::loopStatus(Player::RepeatOne), QString("Track"));
        QCOMPARE(Mpris::loopStatus(Player::RepeatAll), QString("Playlist"));
        QCOMPARE(Mpris::loopStatus(Player::RepeatOff), QString("None"));
        QCOMPARE(Mpris::repeatFor("Track"), int(Player::RepeatOne));
        QCOMPARE(Mpris::repeatFor("None"), int(Player::RepeatOff));
    }

    void previousRestartsATrackThatIsUnderway() {
        Player player("/widevine.so");
        player.play({song("1"), song("2")}, 1);
        QSignalSpy commands(&player, &Player::command);

        player.reportTime(42, 200);
        player.previous();
        QCOMPARE(commands.last().at(0).toString(), QString("seek"));
        QCOMPARE(commands.last().at(1).toMap().value("seconds").toDouble(), 0.0);

        player.reportTime(1, 200);
        player.previous();
        QCOMPARE(commands.last().at(0).toString(), QString("previous"));
    }

    void timeJumpsAreReportedAsSeeks() {
        Player player("/widevine.so");
        QSignalSpy seeks(&player, &Player::seeked);
        player.reportTime(1, 200);
        player.reportTime(2, 200);
        QCOMPARE(seeks.size(), 0);
        player.reportTime(60, 200);
        QCOMPARE(seeks.size(), 1);
    }

    void volumeIsClamped() {
        Player player("/widevine.so");
        player.setVolume(1.5);
        QCOMPARE(player.volume(), 1.0);
        player.setVolume(-1);
        QCOMPARE(player.volume(), 0.0);
    }

    void cancelledSignInIsNotAnError() {
        Player player("/widevine.so");
        player.reportError("signIn", "AUTHORIZATION_ERROR: Unauthorized, user cancelled");
        QVERIFY(player.error().isEmpty());
        player.reportError("playback", "CONTENT_RESTRICTED");
        QVERIFY(player.error().contains("CONTENT_RESTRICTED"));
    }

    void authorizedWithoutAUserTokenIsNotSignedIn() {
        Player player("/widevine.so");
        player.reportAuthorization({{"authorized", true}, {"userToken", ""}, {"storefront", "nl"}});
        QVERIFY(!player.isAuthorized());
        player.reportAuthorization({{"authorized", true}, {"userToken", "token"}, {"storefront", "nl"}});
        QVERIFY(player.isAuthorized());
    }

    void missingWidevineIsReported() {
        Player player({});
        QVERIFY(!player.setupError().isEmpty());
    }

    void widevineIsFoundInTheFirstInstalledLocation() {
        QTemporaryDir dir;
        const QStringList candidates = Player::widevineCandidates(dir.path(), dir.path() + "/home");
        QVERIFY(Player::findWidevine(candidates).isEmpty());

        const QString installed = candidates.at(1);
        QVERIFY(QDir().mkpath(QFileInfo(installed).absolutePath()));
        QFile file(installed);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QCOMPARE(Player::findWidevine(candidates), QFileInfo(installed).canonicalFilePath());
    }

    void chromiumFlagsKeepTheUsersOwn() {
        QCOMPARE(Player::chromiumFlags("/w.so", "--foo"),
                 QByteArray("--foo --widevine-path=/w.so --autoplay-policy=no-user-gesture-required"));
        QCOMPARE(Player::chromiumFlags({}, {}), QByteArray("--autoplay-policy=no-user-gesture-required"));
    }

    void pageCarriesTheTokenAsAStringLiteral() {
        const QString templateHtml = "<script>/" "*QWEBCHANNEL*" "/</script><script>const t = \"%DEVELOPER_TOKEN%\";</script>";
        const QString html = Player::buildPage(templateHtml, "var QWebChannel;", "abc\"def");
        QCOMPARE(html, QString("<script>var QWebChannel;</script><script>const t = \"abc\\\"def\";</script>"));
    }

    void mprisDescribesTheTrack() {
        const QVariantMap metadata = Mpris::metadata(
            {{"playId", "i.abc"}, {"title", "T"}, {"artist", "A"}, {"album", "B"}, {"durationMs", 261000}},
            "https://example.com/512x512bb.jpg");
        QCOMPARE(metadata.value("mpris:trackid").value<QDBusObjectPath>().path(), QString("/org/omasic/track/ti_abc"));
        QCOMPARE(metadata.value("mpris:length").toLongLong(), 261000000LL);
        QCOMPARE(metadata.value("xesam:artist").toStringList(), QStringList({"A"}));
        QCOMPARE(metadata.value("mpris:artUrl").toString(), QString("https://example.com/512x512bb.jpg"));
        QCOMPARE(Mpris::playbackStatus("waiting"), QString("Playing"));
        QCOMPARE(Mpris::playbackStatus("paused"), QString("Paused"));
        QCOMPARE(Mpris::playbackStatus("completed"), QString("Stopped"));
    }

    void themeFallsBackToADarkWindow() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#ffd60a"));
        QCOMPARE(theme.accentForeground(), QString("black"));
        QVERIFY(theme.dark());
    }

    void themeFollowsALightPalette() {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.filePath("theme/colors.toml"),
              "mode = \"light\"\naccent = \"#3760bf\"\nbackground = \"#e9ecf5\"\nforeground = \"#080808\"\n"
              "red = \"#ad3459\"\n");
        Theme theme(dir.path());
        QVERIFY(!theme.dark());
        QCOMPARE(theme.accent(), QString("#3760bf"));
        QCOMPARE(theme.accentForeground(), QString("white"));
        QCOMPARE(theme.background(), QString("#e9ecf5"));
        QCOMPARE(theme.removed(), QString("#ad3459"));
    }

    void themeFollowsHyprlandRounding() {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish() {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile() {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", "{\"Save\": \"Guardar\", \"Open\": \"Abrir\"}");
        write(dir.path() + "/pt-BR.json", "{\"Save\": \"Salvar\"}");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms() {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", "{\"%n clips\": {\"one\": \"%n clip\", \"other\": \"%n clips\"}}");
        write(dir.path() + "/ru.json", "{\"%n clips\": {\"one\": \"%n клип\", \"few\": \"%n клипа\", \"many\": \"%n клипов\", \"other\": \"%n клипа\"}}");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 22), QString("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

    void dutchCatalogCoversEveryString() {
        // Every source string in the app has a Dutch translation.
        QFile catalog(QFINDTESTDATA("../src/i18n/nl.json"));
        QVERIFY(catalog.open(QIODevice::ReadOnly));
        const QJsonObject strings = QJsonDocument::fromJson(catalog.readAll()).object();
        const QRegularExpression call("(?:qsTr|tr)\\(\"((?:[^\"\\\\]|\\\\.)*)\"(?:\\s*\\n\\s*\"((?:[^\"\\\\]|\\\\.)*)\")?");
        const QDir source(QFINDTESTDATA("../src"));
        for (const auto &name : source.entryList({"*.qml", "*.cpp"})) {
            QFile file(source.filePath(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            for (auto it = call.globalMatch(QString::fromUtf8(file.readAll())); it.hasNext();) {
                const auto match = it.next();
                const QString text = (match.captured(1) + match.captured(2)).replace("\\\"", "\"");
                QVERIFY2(strings.contains(text), qPrintable(name + ": " + text));
            }
        }
    }

private:
    static void write(const QString &path, const QByteArray &contents) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(OmasicTests)
#include "omasic_tests.moc"
