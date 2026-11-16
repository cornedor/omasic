#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// Playback through MusicKit JS in a hidden web view. Apple Music streams are
// DRM-protected, so only a browser engine with Widevine can play them; this
// class holds the player's state and queue, sends the page commands over a
// QWebChannel (the `command` signal), and takes its reports (the report*
// methods, called from JavaScript). MusicKit owns the queue: what it reports
// is what the window shows, so shuffling and stations stay in step.
class Player : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString pageHtml READ pageHtml NOTIFY pageHtmlChanged)
    // Where the hidden page says it's from: music.apple.com for the web
    // player's token (the only place Apple accepts it), localhost otherwise.
    Q_PROPERTY(QString pageBaseUrl READ pageBaseUrl NOTIFY pageHtmlChanged)
    Q_PROPERTY(bool ready READ isReady NOTIFY readyChanged)
    Q_PROPERTY(bool authorized READ isAuthorized NOTIFY authorizationChanged)
    Q_PROPERTY(bool signingIn READ isSigningIn NOTIFY signingInChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY stateChanged)
    Q_PROPERTY(bool buffering READ isBuffering NOTIFY stateChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY positionChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(QVariantList queue READ queue NOTIFY queueChanged)
    Q_PROPERTY(int index READ index NOTIFY indexChanged)
    Q_PROPERTY(QVariantMap current READ current NOTIFY indexChanged)
    Q_PROPERTY(QVariantMap station READ station NOTIFY queueChanged)
    Q_PROPERTY(bool canNext READ canNext NOTIFY indexChanged)
    Q_PROPERTY(bool canPrevious READ canPrevious NOTIFY indexChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY modesChanged)
    Q_PROPERTY(int repeat READ repeat WRITE setRepeat NOTIFY modesChanged)
    // When the queue runs out, keep playing similar music (Apple's ∞).
    Q_PROPERTY(bool autoplay READ autoplay WRITE setAutoplay NOTIFY settingsChanged)
    // Stream quality in kbps: 256 (High) or 64 (Standard).
    Q_PROPERTY(int bitrate READ bitrate WRITE setBitrate NOTIFY settingsChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString setupError READ setupError NOTIFY readyChanged)

public:
    // MusicKit's repeat modes.
    enum Repeat { RepeatOff = 0, RepeatOne = 1, RepeatAll = 2 };
    Q_ENUM(Repeat)

    explicit Player(const QString &widevinePath, QObject *parent = nullptr);

    QString pageHtml() const { return m_pageHtml; }
    QString pageBaseUrl() const { return m_pageBaseUrl; }
    bool isReady() const { return m_ready; }
    bool isAuthorized() const { return m_authorized; }
    bool isSigningIn() const { return m_signingIn; }
    QString state() const { return m_state; }
    bool isPlaying() const { return m_state == "playing"; }
    bool isBuffering() const;
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    double volume() const { return m_volume; }
    QVariantList queue() const { return m_queue; }
    int index() const { return m_index; }
    QVariantMap current() const;
    QVariantMap station() const { return m_station; }
    bool canNext() const;
    bool canPrevious() const;
    bool shuffle() const { return m_shuffle; }
    int repeat() const { return m_repeat; }
    bool autoplay() const { return m_autoplay; }
    int bitrate() const { return m_bitrate; }
    QString error() const { return m_error; }
    QString setupError() const { return m_setupError; }
    QString userToken() const { return m_userToken; }
    QString storefront() const { return m_storefront; }

    void setDeveloperToken(const QString &token, bool isWebToken);
    void setVolume(double volume);
    void setShuffle(bool shuffle);
    void setRepeat(int repeat);
    void setAutoplay(bool autoplay);
    void setBitrate(int bitrate);

    // Plays tracks from index. With the album or playlist they're from,
    // MusicKit plays that, so Apple's Recently Played lists it.
    Q_INVOKABLE void play(const QVariantList &tracks, int index, const QVariantMap &container = {});
    // What the page needs to queue a container from a song: {album: id,
    // startWith: song id or index}, or nothing to queue the songs.
    static QVariantMap containerQueue(const QVariantMap &container, const QVariantMap &track, int index);
    Q_INVOKABLE void playStation(const QVariantMap &station);
    // Puts a song, album or playlist right after the current track, or at
    // the end of the queue; with nothing playing, it plays it.
    Q_INVOKABLE void playNext(const QVariantMap &item);
    Q_INVOKABLE void addToQueue(const QVariantMap &item);
    // How MusicKit names an item to queue: {song: id}, {album: id}, ….
    static QVariantMap queueDescriptor(const QVariantMap &item);
    Q_INVOKABLE void jump(int index);
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void seekBy(double seconds);
    Q_INVOKABLE void cycleRepeat();
    Q_INVOKABLE void signIn();
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void clearError();

    // Reports from the page.
    Q_INVOKABLE void reportReady(const QVariantMap &info);
    Q_INVOKABLE void reportAuthorization(const QVariantMap &info);
    Q_INVOKABLE void reportState(const QString &state);
    Q_INVOKABLE void reportTime(double position, double duration);
    Q_INVOKABLE void reportQueue(const QVariantList &items, int position);
    Q_INVOKABLE void reportNowPlaying(const QVariantMap &item);
    Q_INVOKABLE void reportError(const QString &context, const QString &message);
    Q_INVOKABLE void reportAutoplay(bool enabled);

    // Where Widevine may be installed for this architecture, most likely first.
    static QStringList widevineCandidates(const QString &root = "/", const QString &home = {});
    static QString findWidevine(const QStringList &candidates);
    // Chromium flags for the web view, kept alongside any the user set.
    static QByteArray chromiumFlags(const QString &widevinePath, const QByteArray &existing);
    // The bridge page with the developer token and qwebchannel.js filled in.
    static QString buildPage(const QString &templateHtml, const QString &channelScript, const QString &token);

signals:
    // A command for the page: play, station, insert, jump, resume, pause, stop, next,
    // previous, seek, volume, shuffle, repeat, autoplay, bitrate, signIn,
    // signOut.
    void command(const QString &name, const QVariantMap &args);
    void pageHtmlChanged();
    void readyChanged();
    // The session (user token, storefront) changed. Emitted before
    // authorizationChanged, so the API client has the token before QML
    // bindings react and load the library.
    void sessionChanged(const QString &userToken, const QString &storefront);
    void authorizationChanged();
    void signingInChanged();
    void stateChanged();
    void positionChanged();
    // The position jumped (a seek, a new track), for MPRIS.
    void seeked(double position);
    void volumeChanged();
    void queueChanged();
    void indexChanged();
    void modesChanged();
    void settingsChanged();
    void errorChanged();

private:
    void setIndex(int index);
    void setError(const QString &error);
    void setSigningIn(bool signingIn);
    void clearQueue();

    QString m_widevinePath;
    QString m_pageHtml;
    QString m_pageBaseUrl = "https://localhost/";
    bool m_ready = false;
    bool m_authorized = false;
    bool m_signingIn = false;
    QString m_userToken;
    QString m_storefront;
    QString m_state = "none";
    double m_position = 0;
    double m_duration = 0;
    double m_volume = 1;
    QVariantList m_queue;
    int m_index = -1;
    QVariantMap m_nowPlaying;
    QVariantMap m_station;
    bool m_shuffle = false;
    int m_repeat = RepeatOff;
    bool m_autoplay = false;
    int m_bitrate = 256;
    QString m_error;
    QString m_setupError;
};
