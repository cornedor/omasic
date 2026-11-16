#include "outputs.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>

AudioOutputs::AudioOutputs(const QString &applicationName, const QString &pactl, QObject *parent)
    : QObject(parent), m_applicationName(applicationName), m_pactl(pactl) {
    m_selected = QSettings().value("outputDevice").toString();
}

QVariantList AudioOutputs::parseSinks(const QByteArray &json, const QString &defaultSink) {
    QVariantList devices;
    for (const auto &value : QJsonDocument::fromJson(json).array()) {
        const QJsonObject sink = value.toObject();
        const QString name = sink.value("name").toString();
        if (name.isEmpty())
            continue;
        devices << QVariantMap{
            {"name", name},
            {"description", sink.value("description").toString(name)},
            {"isDefault", name == defaultSink},
        };
    }
    return devices;
}

QList<int> AudioOutputs::streamsFor(const QByteArray &json, const QString &applicationName) {
    QList<int> streams;
    for (const auto &value : QJsonDocument::fromJson(json).array()) {
        const QJsonObject input = value.toObject();
        // The web engine's audio runs in a helper process, but it carries the
        // application's name.
        if (input.value("properties").toObject().value("application.name").toString() == applicationName)
            streams << input.value("index").toInt();
    }
    return streams;
}

void AudioOutputs::run(const QStringList &arguments, std::function<void(const QByteArray &)> done) {
    auto *process = new QProcess(this);
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            setError(tr("Choosing an output needs pactl (the libpulse package)."));
        process->deleteLater();
    });
    connect(process, &QProcess::finished, this, [this, process, arguments, done](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        if (status != QProcess::NormalExit || code != 0) {
            qWarning() << "pactl failed" << arguments << code << process->readAllStandardError().left(300);
            return;
        }
        setError({});
        done(process->readAllStandardOutput());
    });
    process->start(m_pactl, arguments);
}

void AudioOutputs::refresh() {
    run({"get-default-sink"}, [this](const QByteArray &defaultSink) {
        const QString name = QString::fromUtf8(defaultSink).trimmed();
        run({"-f", "json", "list", "sinks"}, [this, name](const QByteArray &json) {
            const QVariantList devices = parseSinks(json, name);
            if (devices == m_devices)
                return;
            m_devices = devices;
            emit devicesChanged();
        });
    });
}

void AudioOutputs::select(const QString &name) {
    if (name != m_selected) {
        m_selected = name;
        QSettings().setValue("outputDevice", m_selected);
        emit selectedChanged();
    }
    apply();
}

void AudioOutputs::apply() {
    run({"-f", "json", "list", "sink-inputs"}, [this](const QByteArray &json) {
        const QString target = m_selected.isEmpty() ? "@DEFAULT_SINK@" : m_selected;
        for (int stream : streamsFor(json, m_applicationName))
            run({"move-sink-input", QString::number(stream), target}, [](const QByteArray &) {});
    });
}

void AudioOutputs::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}
