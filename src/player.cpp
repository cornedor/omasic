#include "player.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSettings>
#include <QSysInfo>
#include <cmath>

Player::Player(const QString &widevinePath, QObject *parent) : QObject(parent), m_widevinePath(widevinePath) {
    QSettings settings;
    m_volume = std::clamp(settings.value("volume", 1.0).toDouble(), 0.0, 1.0);
    m_autoplay = settings.value("autoplay", false).toBool();
    m_bitrate = settings.value("bitrate", 256).toInt() == 64 ? 64 : 256;
    if (m_widevinePath.isEmpty())
        m_setupError = tr("Widevine isn't installed, and Apple Music can't play without it. "
                          "Install the widevine package and restart Omasic.");
}

bool Player::isBuffering() const {
    static const QStringList states{"loading", "waiting", "stalled", "seeking"};
    return states.contains(m_state);
}

QVariantMap Player::current() const {
    // What MusicKit says is playing wins; the queue entry fills in before
    // its first report.
    if (!m_nowPlaying.isEmpty())
        return m_nowPlaying;
    return m_index >= 0 && m_index < m_queue.size() ? m_queue.at(m_index).toMap() : QVariantMap();
}

bool Player::canNext() const {
    return !m_station.isEmpty() || m_index + 1 < m_queue.size() || (m_repeat == RepeatAll && !m_queue.isEmpty());
}

bool Player::canPrevious() const {
    // A station only goes forward.
    return m_station.isEmpty() && !m_queue.isEmpty();
}

void Player::setDeveloperToken(const QString &token, bool isWebToken) {
    m_pageBaseUrl = isWebToken ? "https://music.apple.com/" : "https://localhost/";
    QFile templateFile(":/player.html");
    QFile channelFile(":/qtwebchannel/qwebchannel.js");
    if (token.isEmpty() || !templateFile.open(QIODevice::ReadOnly) || !channelFile.open(QIODevice::ReadOnly))
        return;
    const QString html = buildPage(QString::fromUtf8(templateFile.readAll()),
                                   QString::fromUtf8(channelFile.readAll()), token);
    if (html == m_pageHtml)
        return;
    m_pageHtml = html;
    if (m_ready) {
        m_ready = false;
        emit readyChanged();
    }
    emit pageHtmlChanged();
}

QString Player::buildPage(const QString &templateHtml, const QString &channelScript, const QString &token) {
    QString html = templateHtml;
    // JSON-quoted, so the token is a string literal whatever it contains.
    const QString quoted = QString::fromUtf8(QJsonDocument(QJsonArray{token}).toJson(QJsonDocument::Compact));
    html.replace("/*QWEBCHANNEL*/", channelScript);
    html.replace("\"%DEVELOPER_TOKEN%\"", quoted.mid(1, quoted.size() - 2));
    return html;
}

void Player::setVolume(double volume) {
    volume = std::clamp(volume, 0.0, 1.0);
    if (qFuzzyCompare(volume + 1, m_volume + 1))
        return;
    m_volume = volume;
    QSettings().setValue("volume", m_volume);
    emit command("volume", {{"value", m_volume}});
    emit volumeChanged();
}

void Player::setShuffle(bool shuffle) {
    if (shuffle == m_shuffle)
        return;
    m_shuffle = shuffle;
    emit command("shuffle", {{"on", m_shuffle}});
    emit modesChanged();
}

void Player::setRepeat(int repeat) {
    repeat = std::clamp(repeat, int(RepeatOff), int(RepeatAll));
    if (repeat == m_repeat)
        return;
    m_repeat = repeat;
    emit command("repeat", {{"mode", m_repeat}});
    emit modesChanged();
    emit indexChanged();
}

void Player::setAutoplay(bool autoplay) {
    if (autoplay == m_autoplay)
        return;
    m_autoplay = autoplay;
    QSettings().setValue("autoplay", m_autoplay);
    emit command("autoplay", {{"on", m_autoplay}});
    emit settingsChanged();
}

void Player::setBitrate(int bitrate) {
    bitrate = bitrate == 64 ? 64 : 256;
    if (bitrate == m_bitrate)
        return;
    m_bitrate = bitrate;
    QSettings().setValue("bitrate", m_bitrate);
    emit command("bitrate", {{"kbps", m_bitrate}});
    emit settingsChanged();
}

void Player::reportAutoplay(bool enabled) {
    // MusicKit can turn autoplay off itself (for a station, say); show what
    // it does, but keep the saved choice for the next queue.
    if (enabled == m_autoplay)
        return;
    m_autoplay = enabled;
    emit settingsChanged();
}

void Player::cycleRepeat() {
    // Off, all, one, like Apple's own players.
    setRepeat(m_repeat == RepeatOff ? RepeatAll : m_repeat == RepeatAll ? RepeatOne : RepeatOff);
}

QVariantMap Player::containerQueue(const QVariantMap &container, const QVariantMap &track, int index) {
    const QString type = container.value("type").toString();
    if (type != "album" && type != "playlist")
        return {};
    // A catalog container names its songs by catalog id, so start by id,
    // which MusicKit finds whatever it skips. A library-only playlist names
    // them by library id; start by position there.
    const QString catalogId = container.value("catalogId").toString();
    if (!catalogId.isEmpty())
        return {{type, catalogId}, {"startWith", track.value("playId").toString()}};
    return {{type, container.value("id").toString()}, {"startWith", index}};
}

void Player::play(const QVariantList &tracks, int index, const QVariantMap &container) {
    if (tracks.isEmpty() || index < 0 || index >= tracks.size())
        return;
    QStringList ids;
    for (const auto &track : tracks)
        ids << track.toMap().value("playId").toString();
    m_station.clear();
    m_nowPlaying.clear();
    m_queue = tracks;
    m_index = -1;
    emit queueChanged();
    setIndex(index);
    m_position = 0;
    m_duration = tracks.at(index).toMap().value("durationMs").toDouble() / 1000;
    emit positionChanged();
    emit command("play", {{"ids", ids}, {"index", index}, {"container", containerQueue(container, tracks.at(index).toMap(), index)}});
}

void Player::playStation(const QVariantMap &station) {
    QString id = station.value("playId").toString();
    if (id.isEmpty())
        id = station.value("id").toString();
    if (id.isEmpty())
        return;
    m_station = station;
    m_nowPlaying.clear();
    m_queue.clear();
    m_index = -1;
    emit queueChanged();
    emit indexChanged();
    emit command("station", {{"id", id}});
}

QVariantMap Player::queueDescriptor(const QVariantMap &item) {
    const QString type = item.value("type").toString();
    if (type == "song") {
        const QString id = item.value("playId").toString();
        return id.isEmpty() ? QVariantMap() : QVariantMap{{"song", id}};
    }
    if (type != "album" && type != "playlist")
        return {};
    // Library albums and playlists play through their catalog version when
    // they have one, like library songs.
    const QString catalogId = item.value("catalogId").toString();
    return {{type, catalogId.isEmpty() ? item.value("id").toString() : catalogId}};
}

void Player::playNext(const QVariantMap &item) {
    const QVariantMap descriptor = queueDescriptor(item);
    if (descriptor.isEmpty())
        return;
    m_station.clear();
    emit command("insert", {{"descriptor", descriptor}, {"where", "next"}});
}

void Player::addToQueue(const QVariantMap &item) {
    const QVariantMap descriptor = queueDescriptor(item);
    if (descriptor.isEmpty())
        return;
    m_station.clear();
    emit command("insert", {{"descriptor", descriptor}, {"where", "later"}});
}

void Player::jump(int index) {
    if (index < 0 || index >= m_queue.size())
        return;
    emit command("jump", {{"index", index}});
}

void Player::togglePlay() {
    if (isPlaying())
        pause();
    else
        resume();
}

void Player::pause() {
    emit command("pause", {});
}

void Player::resume() {
    if (m_queue.isEmpty() && m_station.isEmpty())
        return;
    emit command("resume", {});
}

void Player::stop() {
    emit command("stop", {});
}

void Player::next() {
    if (canNext())
        emit command("next", {});
}

void Player::previous() {
    if (!canPrevious())
        return;
    // Like most players: a few seconds into a track, "previous" restarts it.
    if (m_position > 3 || m_index <= 0)
        seek(0);
    else
        emit command("previous", {});
}

void Player::seek(double seconds) {
    if (m_queue.isEmpty())
        return;
    const double limit = m_duration > 0 ? m_duration : seconds;
    emit command("seek", {{"seconds", std::clamp(seconds, 0.0, limit)}});
}

void Player::seekBy(double seconds) {
    seek(m_position + seconds);
}

void Player::signIn() {
    if (!m_ready || m_signingIn)
        return;
    setSigningIn(true);
    emit command("signIn", {});
}

void Player::signOut() {
    stop();
    emit command("signOut", {});
}

void Player::clearError() {
    setError({});
}

void Player::reportReady(const QVariantMap &info) {
    if (!info.value("widevine").toBool() && m_setupError.isEmpty())
        m_setupError = tr("This web engine can't play protected audio (Widevine didn't load from %1).")
                           .arg(m_widevinePath);
    const QString error = info.value("error").toString();
    if (!error.isEmpty())
        m_setupError = tr("Apple Music's player didn't start: %1").arg(error);
    m_ready = error.isEmpty();
    emit readyChanged();
    reportAuthorization(info);
    // The page starts with MusicKit's defaults; bring it to the user's.
    emit command("volume", {{"value", m_volume}});
    emit command("autoplay", {{"on", m_autoplay}});
    emit command("bitrate", {{"kbps", m_bitrate}});
}

void Player::reportAuthorization(const QVariantMap &info) {
    const QString token = info.value("userToken").toString();
    // MusicKit can report itself authorized before it has restored the user
    // token, and the library can't be read without that token.
    const bool authorized = info.value("authorized").toBool() && !token.isEmpty();
    const QString storefront = info.value("storefront").toString();
    setSigningIn(false);
    if (authorized == m_authorized && token == m_userToken && storefront == m_storefront)
        return;
    m_authorized = authorized;
    m_userToken = token;
    m_storefront = storefront;
    emit sessionChanged(m_userToken, m_storefront);
    if (!authorized)
        clearQueue();
    emit authorizationChanged();
}

void Player::reportState(const QString &state) {
    if (state == m_state)
        return;
    m_state = state;
    emit stateChanged();
}

void Player::reportTime(double position, double duration) {
    if (!std::isfinite(position) || !std::isfinite(duration))
        return;
    // Playback advances about a second per report; anything else is a seek.
    const bool jumped = std::abs(position - m_position) > 2.5;
    m_position = position;
    if (duration > 0)
        m_duration = duration;
    emit positionChanged();
    if (jumped)
        emit seeked(position);
}

void Player::reportQueue(const QVariantList &items, int position) {
    // An empty report comes before MusicKit has loaded what it was given;
    // keep showing the queue the user picked until then.
    if (items.isEmpty() && m_station.isEmpty())
        return;
    if (items != m_queue) {
        m_queue = items;
        emit queueChanged();
    }
    if (position != m_index) {
        m_index = position;
        emit indexChanged();
    }
}

void Player::reportNowPlaying(const QVariantMap &item) {
    m_nowPlaying = item;
    const QString id = item.value("playId").toString();
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue.at(i).toMap().value("playId").toString() == id) {
            m_index = i;
            break;
        }
    }
    if (item.value("durationMs").toDouble() > 0)
        m_duration = item.value("durationMs").toDouble() / 1000;
    emit indexChanged();
}

void Player::reportError(const QString &context, const QString &message) {
    if (context == "signIn") {
        setSigningIn(false);
        // Closing the sign-in window isn't an error worth showing.
        if (message.contains("cancel", Qt::CaseInsensitive))
            return;
    }
    setError(context == "signIn" ? tr("Sign-in failed: %1").arg(message)
                                 : tr("Playback failed: %1").arg(message));
}

void Player::setIndex(int index) {
    if (index == m_index)
        return;
    m_index = index;
    emit indexChanged();
}

void Player::clearQueue() {
    m_queue.clear();
    m_station.clear();
    m_nowPlaying.clear();
    m_index = -1;
    emit queueChanged();
    emit indexChanged();
}

void Player::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}

void Player::setSigningIn(bool signingIn) {
    if (signingIn == m_signingIn)
        return;
    m_signingIn = signingIn;
    emit signingInChanged();
}

QStringList Player::widevineCandidates(const QString &root, const QString &home) {
    const QString arch = QSysInfo::currentCpuArchitecture() == "arm64" ? "linux_arm64" : "linux_x64";
    const QString user = home.isEmpty() ? QDir::homePath() : home;
    const QString base = root.endsWith('/') ? root.chopped(1) : root;
    return {
        // The widevine package (Asahi's arm64 build, or the x86_64 AUR one).
        base + "/opt/WidevineCdm/chromium/_platform_specific/" + arch + "/libwidevinecdm.so",
        base + "/usr/lib/chromium/WidevineCdm/_platform_specific/" + arch + "/libwidevinecdm.so",
        // Google Chrome ships its own copy on x86_64.
        base + "/opt/google/chrome/WidevineCdm/_platform_specific/" + arch + "/libwidevinecdm.so",
        user + "/.local/lib/libwidevinecdm.so",
    };
}

QString Player::findWidevine(const QStringList &candidates) {
    for (const auto &path : candidates) {
        const QFileInfo info(path);
        if (info.isFile())
            return info.canonicalFilePath();
    }
    return {};
}

QByteArray Player::chromiumFlags(const QString &widevinePath, const QByteArray &existing) {
    QByteArrayList flags;
    if (!existing.trimmed().isEmpty())
        flags << existing.trimmed();
    if (!widevinePath.isEmpty())
        flags << "--widevine-path=" + widevinePath.toUtf8();
    // Commands come from the app, not from a click inside the page.
    flags << "--autoplay-policy=no-user-gesture-required";
    return flags.join(' ');
}
