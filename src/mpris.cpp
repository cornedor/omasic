#include "mpris.h"

#include "applemusic.h"
#include "player.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QRegularExpression>

namespace {
const QString objectPath = "/org/mpris/MediaPlayer2";

QString trackPath(const QVariantMap &track) {
    // Object paths allow only [A-Za-z0-9_]; library ids contain dots.
    QString id = track.value("playId").toString();
    id.replace(QRegularExpression("[^A-Za-z0-9_]"), "_");
    return id.isEmpty() ? "/org/mpris/MediaPlayer2/TrackList/NoTrack" : "/org/omasic/track/t" + id;
}
}

Mpris::Mpris(Player *player, QObject *parent) : QObject(parent), m_player(player) {
    new MprisRoot(this);
    auto *adaptor = new MprisPlayer(this);

    connect(player, &Player::stateChanged, this, [this] {
        notify({{"PlaybackStatus", playbackStatus(m_player->state())}});
    });
    connect(player, &Player::indexChanged, this, [this, adaptor] {
        notify({{"Metadata", adaptor->metadata()},
                {"CanGoNext", adaptor->canGoNext()},
                {"CanGoPrevious", adaptor->canGoPrevious()}});
    });
    connect(player, &Player::modesChanged, this, [this, adaptor] {
        notify({{"LoopStatus", adaptor->loopStatus()}, {"Shuffle", adaptor->shuffle()}});
    });
    connect(player, &Player::queueChanged, this, [this, adaptor] {
        notify({{"CanPlay", adaptor->canPlay()}, {"CanPause", adaptor->canPlay()}, {"CanSeek", adaptor->canPlay()}});
    });
    connect(player, &Player::volumeChanged, this, [this] { notify({{"Volume", m_player->volume()}}); });
    connect(player, &Player::seeked, adaptor, [adaptor](double position) {
        emit adaptor->Seeked(qlonglong(position * 1e6));
    });

    QDBusConnection bus = QDBusConnection::sessionBus();
    m_registered = bus.isConnected() && bus.registerObject(objectPath, this)
                   && bus.registerService("org.mpris.MediaPlayer2.omasic");
}

QString Mpris::playbackStatus(const QString &state) {
    static const QStringList playing{"playing", "loading", "waiting", "stalled", "seeking"};
    if (playing.contains(state))
        return "Playing";
    if (state == "paused")
        return "Paused";
    return "Stopped";
}

QString Mpris::loopStatus(int repeat) {
    return repeat == Player::RepeatOne ? "Track" : repeat == Player::RepeatAll ? "Playlist" : "None";
}

int Mpris::repeatFor(const QString &loopStatus) {
    return loopStatus == "Track" ? Player::RepeatOne : loopStatus == "Playlist" ? Player::RepeatAll : Player::RepeatOff;
}

QVariantMap Mpris::metadata(const QVariantMap &track, const QString &artUrl) {
    if (track.isEmpty())
        return {{"mpris:trackid", QVariant::fromValue(QDBusObjectPath(trackPath(track)))}};
    QVariantMap metadata{
        {"mpris:trackid", QVariant::fromValue(QDBusObjectPath(trackPath(track)))},
        {"mpris:length", qlonglong(track.value("durationMs").toLongLong() * 1000)},
        {"xesam:title", track.value("title").toString()},
        {"xesam:artist", QStringList{track.value("artist").toString()}},
        {"xesam:album", track.value("album").toString()},
    };
    if (!artUrl.isEmpty())
        metadata.insert("mpris:artUrl", artUrl);
    return metadata;
}

void Mpris::notify(const QVariantMap &changed) {
    if (!m_registered)
        return;
    QDBusMessage signal = QDBusMessage::createSignal(objectPath, "org.freedesktop.DBus.Properties", "PropertiesChanged");
    signal << QString("org.mpris.MediaPlayer2.Player") << changed << QStringList();
    QDBusConnection::sessionBus().send(signal);
}

MprisRoot::MprisRoot(Mpris *mpris) : QDBusAbstractAdaptor(mpris), m_mpris(mpris) {}

void MprisRoot::Raise() {
    emit m_mpris->raiseRequested();
}

void MprisRoot::Quit() {
    QCoreApplication::quit();
}

MprisPlayer::MprisPlayer(Mpris *mpris) : QDBusAbstractAdaptor(mpris), m_mpris(mpris) {}

QString MprisPlayer::playbackStatus() const {
    return Mpris::playbackStatus(m_mpris->player()->state());
}

QVariantMap MprisPlayer::metadata() const {
    const QVariantMap track = m_mpris->player()->current();
    return Mpris::metadata(track, AppleMusic::artworkUrl(track.value("artwork").toString(), 512));
}

double MprisPlayer::volume() const {
    return m_mpris->player()->volume();
}

void MprisPlayer::setVolume(double volume) {
    m_mpris->player()->setVolume(volume);
}

qlonglong MprisPlayer::position() const {
    return qlonglong(m_mpris->player()->position() * 1e6);
}

QString MprisPlayer::loopStatus() const {
    return Mpris::loopStatus(m_mpris->player()->repeat());
}

void MprisPlayer::setLoopStatus(const QString &status) {
    m_mpris->player()->setRepeat(Mpris::repeatFor(status));
}

bool MprisPlayer::shuffle() const {
    return m_mpris->player()->shuffle();
}

void MprisPlayer::setShuffle(bool shuffle) {
    m_mpris->player()->setShuffle(shuffle);
}

bool MprisPlayer::canGoNext() const {
    return m_mpris->player()->canNext();
}

bool MprisPlayer::canGoPrevious() const {
    return m_mpris->player()->canPrevious();
}

bool MprisPlayer::canPlay() const {
    return !m_mpris->player()->queue().isEmpty() || !m_mpris->player()->station().isEmpty();
}

void MprisPlayer::Next() { m_mpris->player()->next(); }
void MprisPlayer::Previous() { m_mpris->player()->previous(); }
void MprisPlayer::Pause() { m_mpris->player()->pause(); }
void MprisPlayer::PlayPause() { m_mpris->player()->togglePlay(); }
void MprisPlayer::Stop() { m_mpris->player()->stop(); }
void MprisPlayer::Play() { m_mpris->player()->resume(); }

void MprisPlayer::Seek(qlonglong offset) {
    m_mpris->player()->seekBy(double(offset) / 1e6);
}

void MprisPlayer::SetPosition(const QDBusObjectPath &trackId, qlonglong position) {
    // The spec ignores a SetPosition meant for a track that's no longer playing.
    if (trackId.path() != trackPath(m_mpris->player()->current()))
        return;
    m_mpris->player()->seek(double(position) / 1e6);
}
