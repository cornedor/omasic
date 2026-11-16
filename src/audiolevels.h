#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QVector>

// What the visualizer hears: how loud the bass, the mids and the highs of
// Omasic's own playback are, and a pulse on every beat. Playback is
// DRM-protected inside the web engine, so the samples come from PipeWire:
// pw-cat records the application's own stream, never the microphone.
class AudioLevels : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(qreal bass READ bass NOTIFY changed)
    Q_PROPERTY(qreal mid READ mid NOTIFY changed)
    Q_PROPERTY(qreal high READ high NOTIFY changed)
    Q_PROPERTY(qreal beat READ beat NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    static constexpr int SampleRate = 44100;
    static constexpr int WindowSize = 1024;

    struct Bands {
        float bass = 0, mid = 0, high = 0;
    };

    explicit AudioLevels(const QString &applicationName, const QString &pactl = "pactl",
                         const QString &pwCat = "pw-cat", QObject *parent = nullptr);
    ~AudioLevels() override;

    bool active() const { return m_active; }
    void setActive(bool active);
    qreal bass() const { return m_bass; }
    qreal mid() const { return m_mid; }
    qreal high() const { return m_high; }
    qreal beat() const { return m_beat; }
    QString error() const { return m_error; }

    // Looks for the stream again; each track can open a new one.
    void reconnect();

    // The mean magnitude of the bass, mid and high frequencies in a window of
    // WindowSize mono samples.
    static Bands measure(const QVector<float> &samples);
    // Feeds one measurement into the levels: each band is scaled to its own
    // recent peak, so the result doesn't depend on the volume, and rises
    // faster than it falls.
    void advance(const Bands &bands, qreal seconds);

signals:
    void activeChanged();
    void changed();
    void errorChanged();

private:
    void findStream();
    void record(int stream);
    void stopRecording();
    void readSamples();
    void setError(const QString &error);

    QString m_applicationName, m_pactl, m_pwCat, m_error;
    bool m_active = false;
    QProcess *m_recorder = nullptr;
    QByteArray m_pending;
    QVector<float> m_window;
    QTimer m_tick, m_retry;
    QElapsedTimer m_sinceSamples;
    Bands m_peak;
    qreal m_bass = 0, m_mid = 0, m_high = 0, m_beat = 0;
    qreal m_bassAverage = 0, m_sinceBeat = 1;
};
