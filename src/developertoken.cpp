#include "developertoken.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QProcess>
#include <QSaveFile>
#include <QSettings>

namespace {
const QUrl pageUrl("https://music.apple.com/us/browse");

QJsonObject payload(const QString &jwt) {
    const QStringList parts = jwt.split('.');
    if (parts.size() != 3)
        return {};
    const QByteArray json = QByteArray::fromBase64(parts[1].toLatin1(), QByteArray::Base64UrlEncoding);
    return QJsonDocument::fromJson(json).object();
}
}

DeveloperToken::DeveloperToken(QNetworkAccessManager *network, const QString &cacheFile, const QString &userAgent,
                               QObject *parent)
    : QObject(parent), m_network(network), m_cacheFile(cacheFile), m_userAgent(userAgent) {
    QSettings settings;
    m_teamId = settings.value("developer/teamId").toString();
    m_keyId = settings.value("developer/keyId").toString();
    m_keyFile = settings.value("developer/keyFile").toString();
    m_useWebToken = settings.value("developer/useWebToken", false).toBool();
}

void DeveloperToken::setKey(const QString &teamId, const QString &keyId, const QString &keyFile) {
    m_teamId = teamId.trimmed();
    m_keyId = keyId.trimmed();
    m_keyFile = m_teamId.isEmpty() ? QString() : keyFile.trimmed();
    if (m_teamId.isEmpty())
        m_keyId.clear();
    QSettings settings;
    settings.setValue("developer/teamId", m_teamId);
    settings.setValue("developer/keyId", m_keyId);
    settings.setValue("developer/keyFile", m_keyFile);
    m_token.clear();
    emit changed();
    refresh(true);
}

void DeveloperToken::setUseWebToken(bool use) {
    if (use == m_useWebToken)
        return;
    m_useWebToken = use;
    QSettings().setValue("developer/useWebToken", m_useWebToken);
    if (!hasKey())
        m_token.clear();
    emit changed();
    refresh();
}

QString DeveloperToken::expandPath(const QString &path) {
    const QString trimmed = path.trimmed();
    if (trimmed == "~" || trimmed.startsWith("~/"))
        return QDir::homePath() + trimmed.mid(1);
    return trimmed;
}

QByteArray DeveloperToken::signingInput(const QString &teamId, const QString &keyId, const QDateTime &issued,
                                        const QDateTime &expires) {
    const auto encode = [](const QJsonObject &object) {
        return QJsonDocument(object).toJson(QJsonDocument::Compact)
            .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    };
    const QJsonObject header{{"alg", "ES256"}, {"kid", keyId}};
    const QJsonObject payload{{"iss", teamId}, {"iat", issued.toSecsSinceEpoch()}, {"exp", expires.toSecsSinceEpoch()}};
    return encode(header) + "." + encode(payload);
}

QByteArray DeveloperToken::rawSignature(const QByteArray &der) {
    // SEQUENCE { INTEGER r, INTEGER s }, each integer big-endian and
    // possibly padded with a leading zero.
    if (der.size() < 8 || uchar(der[0]) != 0x30)
        return {};
    int pos = 2;
    if (uchar(der[1]) & 0x80)
        pos = 2 + (uchar(der[1]) & 0x7f);
    QByteArray raw;
    for (int i = 0; i < 2; ++i) {
        if (pos + 2 > der.size() || der[pos] != 0x02)
            return {};
        const int length = uchar(der[pos + 1]);
        pos += 2;
        if (pos + length > der.size())
            return {};
        QByteArray integer = der.mid(pos, length);
        pos += length;
        while (integer.size() > 32 && integer.startsWith('\0'))
            integer.remove(0, 1);
        if (integer.size() > 32)
            return {};
        raw += QByteArray(32 - integer.size(), '\0') + integer;
    }
    return raw;
}

void DeveloperToken::signWithKey() {
    const QString keyPath = expandPath(m_keyFile);
    if (!QFileInfo(keyPath).isReadable()) {
        finish({}, tr("Can't read the MusicKit key at %1.").arg(keyPath));
        return;
    }
    // Apple accepts developer tokens for up to six months.
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QByteArray input = signingInput(m_teamId, m_keyId, now, now.addDays(180));

    m_loading = true;
    m_error.clear();
    emit changed();
    auto *process = new QProcess(this);
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        finish({}, tr("Signing the MusicKit token needs the openssl command."));
    });
    connect(process, &QProcess::finished, this, [this, process, input](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        const QByteArray signature = rawSignature(process->readAllStandardOutput());
        if (status != QProcess::NormalExit || code != 0 || signature.isEmpty()) {
            finish({}, tr("Couldn't sign with the MusicKit key: %1")
                           .arg(QString::fromUtf8(process->readAllStandardError()).trimmed()));
            return;
        }
        finish(QString::fromLatin1(input + "." + signature.toBase64(QByteArray::Base64UrlEncoding
                                                                       | QByteArray::OmitTrailingEquals)), {});
    });
    process->start(m_openssl, {"dgst", "-sha256", "-sign", keyPath});
    process->write(input);
    process->closeWriteChannel();
}

QStringList DeveloperToken::scriptPaths(const QString &html) {
    static const QRegularExpression script(R"re(/assets/[A-Za-z0-9_~.-]+\.js)re");
    QStringList paths;
    for (auto it = script.globalMatch(html); it.hasNext();)
        paths << it.next().captured(0);
    paths.removeDuplicates();
    // The token lives in the entry bundle; try it before the lazy chunks.
    std::stable_partition(paths.begin(), paths.end(),
                          [](const QString &path) { return path.startsWith("/assets/index"); });
    return paths;
}

QString DeveloperToken::findToken(const QString &script) {
    static const QRegularExpression jwt(R"re(eyJ[A-Za-z0-9_-]{10,}\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+)re");
    for (auto it = jwt.globalMatch(script); it.hasNext();) {
        const QString candidate = it.next().captured(0);
        // The bundle carries several tokens; only the web player's one is
        // accepted by the public API.
        if (payload(candidate).value("iss").toString() == "AMPWebPlay")
            return candidate;
    }
    return {};
}

QDateTime DeveloperToken::expiry(const QString &jwt) {
    const qint64 exp = payload(jwt).value("exp").toInteger();
    return exp > 0 ? QDateTime::fromSecsSinceEpoch(exp, QTimeZone::UTC) : QDateTime();
}

bool DeveloperToken::isUsable(const QString &jwt, const QDateTime &now) {
    const QDateTime expires = expiry(jwt);
    // A day of margin, so a session that runs overnight doesn't hit the edge.
    return expires.isValid() && now.addDays(1) < expires;
}

void DeveloperToken::refresh(bool force) {
    if (m_loading)
        return;
    if (hasKey()) {
        signWithKey();
        return;
    }
    if (!m_useWebToken) {
        // Nothing to use yet; the setup screen says what's needed.
        finish({}, {});
        return;
    }
    if (!force) {
        QFile cache(m_cacheFile);
        if (cache.open(QIODevice::ReadOnly)) {
            const QString cached = QString::fromUtf8(cache.readAll()).trimmed();
            if (isUsable(cached, QDateTime::currentDateTimeUtc())) {
                finish(cached, {});
                return;
            }
        }
    }
    m_loading = true;
    m_error.clear();
    emit changed();
    fetchPage();
}

QNetworkReply *DeveloperToken::get(const QUrl &url) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, m_userAgent);
    request.setTransferTimeout(20000);
    return m_network->get(request);
}

void DeveloperToken::fetchPage() {
    QNetworkReply *reply = get(pageUrl);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            finish({}, tr("Couldn't reach music.apple.com: %1").arg(reply->errorString()));
            return;
        }
        const QStringList paths = scriptPaths(QString::fromUtf8(reply->readAll()));
        if (paths.isEmpty()) {
            finish({}, tr("music.apple.com changed: no player script found."));
            return;
        }
        fetchScripts(paths);
    });
}

void DeveloperToken::fetchScripts(QStringList paths) {
    if (paths.isEmpty()) {
        finish({}, tr("music.apple.com changed: no player token found."));
        return;
    }
    const QString path = paths.takeFirst();
    QNetworkReply *reply = get(pageUrl.resolved(QUrl(path)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, paths] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            finish({}, tr("Couldn't reach music.apple.com: %1").arg(reply->errorString()));
            return;
        }
        const QString token = findToken(QString::fromUtf8(reply->readAll()));
        if (token.isEmpty()) {
            fetchScripts(paths);
            return;
        }
        QDir().mkpath(QFileInfo(m_cacheFile).absolutePath());
        QSaveFile cache(m_cacheFile);
        if (cache.open(QIODevice::WriteOnly)) {
            cache.write(token.toUtf8());
            cache.commit();
        }
        finish(token, {});
    });
}

void DeveloperToken::finish(const QString &token, const QString &error) {
    m_loading = false;
    m_error = error;
    if (!token.isEmpty() || !error.isEmpty() || needsSetup())
        m_token = token;
    emit changed();
}
