#pragma once

#include <QList>
#include <QObject>
#include <QVariantList>

#include <functional>

// Where the sound goes. Playback happens inside Qt WebEngine, which always
// plays to the system's default output, so Omasic moves its own streams to
// the chosen device afterwards with pactl (PipeWire's PulseAudio interface).
class AudioOutputs : public QObject {
    Q_OBJECT
    // [{name, description, isDefault}], for the device menu.
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    // The chosen device's name; empty follows the system default.
    Q_PROPERTY(QString selected READ selected NOTIFY selectedChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit AudioOutputs(const QString &applicationName, const QString &pactl = "pactl", QObject *parent = nullptr);

    QVariantList devices() const { return m_devices; }
    QString selected() const { return m_selected; }
    QString error() const { return m_error; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void select(const QString &name);
    // Moves this app's streams to the chosen device.
    Q_INVOKABLE void apply();

    static QVariantList parseSinks(const QByteArray &json, const QString &defaultSink);
    static QList<int> streamsFor(const QByteArray &json, const QString &applicationName);

signals:
    void devicesChanged();
    void selectedChanged();
    void errorChanged();

private:
    void run(const QStringList &arguments, std::function<void(const QByteArray &)> done);
    void setError(const QString &error);

    QString m_applicationName;
    QString m_pactl;
    QVariantList m_devices;
    QString m_selected;
    QString m_error;
};
