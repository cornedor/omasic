#pragma once

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

// Apple Music needs a developer token (a JWT signed with a MusicKit key) on
// every request. The supported way is the user's own MusicKit key from an
// Apple Developer account: Omasic signs a token with it. As an opt-in
// fallback it can use the token music.apple.com's web player ships, which
// Apple doesn't sanction; that one is cached and fetched again before it
// expires.
class DeveloperToken : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString token READ token NOTIFY changed)
    Q_PROPERTY(bool loading READ isLoading NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    // Neither a key nor the web token: the setup screen asks for one.
    Q_PROPERTY(bool needsSetup READ needsSetup NOTIFY changed)
    // Signed with the user's own key, as Apple intends.
    Q_PROPERTY(bool isOfficial READ isOfficial NOTIFY changed)
    Q_PROPERTY(QString teamId READ teamId NOTIFY changed)
    Q_PROPERTY(QString keyId READ keyId NOTIFY changed)
    Q_PROPERTY(QString keyFile READ keyFile NOTIFY changed)
    Q_PROPERTY(bool useWebToken READ useWebToken WRITE setUseWebToken NOTIFY changed)

public:
    DeveloperToken(QNetworkAccessManager *network, const QString &cacheFile, const QString &userAgent,
                   QObject *parent = nullptr);

    QString token() const { return m_token; }
    bool isLoading() const { return m_loading; }
    QString error() const { return m_error; }
    bool hasKey() const { return !m_teamId.isEmpty() && !m_keyId.isEmpty() && !m_keyFile.isEmpty(); }
    bool needsSetup() const { return !hasKey() && !m_useWebToken; }
    bool isOfficial() const { return hasKey(); }
    QString teamId() const { return m_teamId; }
    QString keyId() const { return m_keyId; }
    QString keyFile() const { return m_keyFile; }
    bool useWebToken() const { return m_useWebToken; }

    // Saves the user's MusicKit key details and signs a token with them; an
    // empty team id removes the key.
    Q_INVOKABLE void setKey(const QString &teamId, const QString &keyId, const QString &keyFile);
    void setUseWebToken(bool use);

    // Uses the cached token while it's valid, unless forced (after a 401).
    Q_INVOKABLE void refresh(bool force = false);

    // The script bundles a music.apple.com page loads, most likely first.
    static QStringList scriptPaths(const QString &html);
    // The web player's token in a bundle: the JWT whose issuer is AMPWebPlay.
    static QString findToken(const QString &script);
    static QDateTime expiry(const QString &jwt);
    static bool isUsable(const QString &jwt, const QDateTime &now);
    // What a MusicKit token signs: the base64url header and payload.
    static QByteArray signingInput(const QString &teamId, const QString &keyId, const QDateTime &issued,
                                   const QDateTime &expires);
    // openssl gives an ECDSA signature as DER; a JWT wants r and s as 32
    // bytes each.
    static QByteArray rawSignature(const QByteArray &der);
    // A file path as typed, with ~ for the home directory.
    static QString expandPath(const QString &path);

signals:
    void changed();

private:
    void signWithKey();
    void fetchPage();
    void fetchScripts(QStringList paths);
    void finish(const QString &token, const QString &error);
    QNetworkReply *get(const QUrl &url);

    QNetworkAccessManager *m_network;
    QString m_cacheFile;
    QString m_userAgent;
    QString m_token;
    QString m_error;
    bool m_loading = false;
    QString m_teamId;
    QString m_keyId;
    QString m_keyFile;
    bool m_useWebToken = false;
    QString m_openssl = "openssl";
};
