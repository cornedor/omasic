#include "audiolevels.h"

#include <QDebug>
#include <QtMath>
#include <algorithm>
#include <complex>
#include <cstring>

#include "outputs.h"

namespace {
constexpr int TickMs = 16;
// Quieter than this is silence, not music to scale up.
constexpr float NoiseFloor = 1e-4f;

// In-place radix-2 FFT; the size is a power of two.
void transform(QVector<std::complex<float>> &data) {
    const int size = data.size();
    for (int i = 1, j = 0; i < size; ++i) {
        int bit = size >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(data[i], data[j]);
    }
    for (int length = 2; length <= size; length <<= 1) {
        const float angle = -2 * float(M_PI) / length;
        const std::complex<float> step(std::cos(angle), std::sin(angle));
        for (int start = 0; start < size; start += length) {
            std::complex<float> twiddle(1, 0);
            for (int k = 0; k < length / 2; ++k) {
                const auto even = data[start + k];
                const auto odd = data[start + k + length / 2] * twiddle;
                data[start + k] = even + odd;
                data[start + k + length / 2] = even - odd;
                twiddle *= step;
            }
        }
    }
}

float meanMagnitude(const QVector<std::complex<float>> &spectrum, float fromHz, float toHz) {
    const float binHz = float(AudioLevels::SampleRate) / AudioLevels::WindowSize;
    const int from = std::max(1, int(fromHz / binHz));
    const int to = std::min(int(AudioLevels::WindowSize / 2), int(toHz / binHz) + 1);
    float sum = 0;
    for (int bin = from; bin < to; ++bin)
        sum += std::abs(spectrum[bin]);
    return sum / std::max(1, to - from) / (AudioLevels::WindowSize / 4);
}

// Moves a level towards its target, quickly up and slowly down, so the
// picture breathes with the music instead of flickering.
qreal follow(qreal level, qreal target, qreal seconds) {
    const qreal rate = target > level ? 30 : 4;
    return level + (target - level) * (1 - std::exp(-rate * seconds));
}
}

AudioLevels::AudioLevels(const QString &applicationName, const QString &pactl, const QString &pwCat, QObject *parent)
    : QObject(parent), m_applicationName(applicationName), m_pactl(pactl), m_pwCat(pwCat), m_window(WindowSize, 0) {
    m_tick.setInterval(TickMs);
    connect(&m_tick, &QTimer::timeout, this, [this] {
        // A paused stream sends nothing; don't keep reacting to its last note.
        if (m_sinceSamples.isValid() && m_sinceSamples.elapsed() > 150) {
            m_window.fill(0);
            m_sinceSamples.invalidate();
        }
        advance(measure(m_window), TickMs / 1000.0);
    });
    // No stream yet (paused, or between tracks): look again shortly.
    m_retry.setInterval(1000);
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &AudioLevels::findStream);
}

AudioLevels::~AudioLevels() {
    if (!m_recorder)
        return;
    m_recorder->disconnect(this);
    m_recorder->kill();
    m_recorder->waitForFinished(500);
}

void AudioLevels::setActive(bool active) {
    if (active == m_active)
        return;
    m_active = active;
    if (m_active) {
        m_tick.start();
        findStream();
    } else {
        m_tick.stop();
        m_retry.stop();
        stopRecording();
        m_window.fill(0);
        m_bass = m_mid = m_high = m_beat = 0;
        emit changed();
    }
    emit activeChanged();
}

void AudioLevels::reconnect() {
    if (m_active && !m_recorder)
        findStream();
}

AudioLevels::Bands AudioLevels::measure(const QVector<float> &samples) {
    QVector<std::complex<float>> spectrum(WindowSize);
    const int count = std::min<int>(WindowSize, samples.size());
    for (int i = 0; i < count; ++i) {
        const float hann = 0.5f - 0.5f * std::cos(2 * float(M_PI) * i / (WindowSize - 1));
        spectrum[i] = samples[i] * hann;
    }
    transform(spectrum);
    return {meanMagnitude(spectrum, 30, 180), meanMagnitude(spectrum, 180, 2500), meanMagnitude(spectrum, 2500, 14000)};
}

void AudioLevels::advance(const Bands &bands, qreal seconds) {
    // Each band's recent peak fades over a few seconds, so a quiet passage
    // after a loud one comes back up.
    const float fade = std::exp(-float(seconds) / 6);
    const auto scale = [fade](float value, float &peak) {
        peak = std::max({value, peak * fade, NoiseFloor});
        return value < NoiseFloor ? 0.0 : qreal(value / peak);
    };
    const qreal bass = scale(bands.bass, m_peak.bass);
    m_bass = follow(m_bass, bass, seconds);
    m_mid = follow(m_mid, scale(bands.mid, m_peak.mid), seconds);
    m_high = follow(m_high, scale(bands.high, m_peak.high), seconds);

    // A beat is bass that jumps well above its own recent average.
    m_sinceBeat += seconds;
    if (bass > 0.35 && bass > m_bassAverage * 1.5 && m_sinceBeat > 0.18) {
        m_beat = 1;
        m_sinceBeat = 0;
    } else {
        m_beat *= std::exp(-5 * seconds);
    }
    m_bassAverage += (bass - m_bassAverage) * (1 - std::exp(-2 * seconds));
    emit changed();
}

void AudioLevels::findStream() {
    if (!m_active || m_recorder)
        return;
    auto *process = new QProcess(this);
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            setError(tr("The visualizer needs pactl (the libpulse package) to find the music."));
        process->deleteLater();
    });
    connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        if (!m_active || m_recorder)
            return;
        const QList<int> streams = status == QProcess::NormalExit && code == 0
                                       ? AudioOutputs::streamsFor(process->readAllStandardOutput(), m_applicationName)
                                       : QList<int>();
        if (streams.isEmpty())
            m_retry.start();
        else
            record(streams.last());
    });
    process->start(m_pactl, {"-f", "json", "list", "sink-inputs"});
}

void AudioLevels::record(int stream) {
    m_recorder = new QProcess(this);
    m_pending.clear();
    connect(m_recorder, &QProcess::readyReadStandardOutput, this, &AudioLevels::readSamples);
    connect(m_recorder, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            setError(tr("The visualizer needs pw-cat (the pipewire package) to hear the music."));
            stopRecording();
        }
    });
    connect(m_recorder, &QProcess::finished, this, [this] {
        // The stream ended with its track; wait for the next one.
        stopRecording();
        if (m_active)
            m_retry.start();
    });
    // Without a reconnect the recorder stops when its stream goes away,
    // rather than moving on to the default source: the microphone.
    m_recorder->start(m_pwCat, {"--record", "--raw", "--format", "f32", "--rate", QString::number(SampleRate),
                                "--channels", "1", "--latency", "20ms", "--target", QString::number(stream),
                                "-P", "{ node.dont-reconnect = true node.name = omasic-visualizer }", "-"});
}

void AudioLevels::stopRecording() {
    if (!m_recorder)
        return;
    QProcess *recorder = m_recorder;
    m_recorder = nullptr;
    recorder->disconnect(this);
    if (recorder->state() != QProcess::NotRunning) {
        recorder->terminate();
        connect(recorder, &QProcess::finished, recorder, &QObject::deleteLater);
    } else {
        recorder->deleteLater();
    }
    m_window.fill(0);
}

void AudioLevels::readSamples() {
    setError({});
    m_sinceSamples.start();
    m_pending += m_recorder->readAllStandardOutput();
    const int count = m_pending.size() / int(sizeof(float));
    if (count == 0)
        return;
    // Only the newest window matters.
    const int keep = std::min(count, WindowSize);
    const char *newest = m_pending.constData() + (count - keep) * sizeof(float);
    if (keep < WindowSize)
        std::memmove(m_window.data(), m_window.constData() + keep, (WindowSize - keep) * sizeof(float));
    std::memcpy(m_window.data() + WindowSize - keep, newest, keep * sizeof(float));
    m_pending.remove(0, count * int(sizeof(float)));
}

void AudioLevels::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}
