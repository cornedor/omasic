// Omasic: an Apple Music player for Omarchy. The window, library and search
// are native Qt Quick; a hidden web view runs Apple's MusicKit JS for sign-in
// and DRM playback.

#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QIcon>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlNetworkAccessManagerFactory>
#include <QQuickStyle>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QWebEngineScript>
#include <QtWebEngineQuick/QQuickWebEngineProfile>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include "applemusic.h"
#include "developertoken.h"
#include "mpris.h"
#include "outputs.h"
#include "page.h"
#include "player.h"
#include "theme.h"
#include "translator.h"

namespace {
// Artwork is fetched by QML; keep it on disk so the library doesn't
// download every cover again on each start.
class CachingNetworkFactory : public QQmlNetworkAccessManagerFactory {
public:
    QNetworkAccessManager *create(QObject *parent) override {
        auto *network = new QNetworkAccessManager(parent);
        auto *cache = new QNetworkDiskCache(network);
        cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/artwork");
        cache->setMaximumCacheSize(200 * 1024 * 1024);
        network->setCache(cache);
        return network;
    }
};

// Qt WebEngine exposes WebAuthn, but its capability queries never resolve,
// and Apple's sign-in page waits on them before showing the password step.
// Without the API the page uses password and two-factor sign-in instead.
QWebEngineScript passkeyRemoval() {
    QWebEngineScript script;
    script.setName("omasic-no-passkeys");
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(true);
    script.setSourceCode("delete window.PublicKeyCredential;");
    return script;
}
}

int main(int argc, char *argv[]) {
    // Chromium reads its flags once, before the web engine starts.
    const QString widevine = Player::findWidevine(Player::widevineCandidates());
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", Player::chromiumFlags(widevine, qgetenv("QTWEBENGINE_CHROMIUM_FLAGS")));
    QtWebEngineQuick::initialize();

    QGuiApplication app(argc, argv);
    app.setApplicationName("omasic");
    app.setOrganizationName("omasic");
    app.setApplicationVersion("0.1.0");

    // Matches the window to omasic.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("omasic");
    app.setWindowIcon(QIcon::fromTheme("omasic"));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;

    // One persistent profile keeps the Apple ID session between runs.
    QQuickWebEngineProfile webProfile("omasic");
    webProfile.setPersistentCookiesPolicy(QQuickWebEngineProfile::ForcePersistentCookies);
    // Apple's sign-in page breaks on the QtWebEngine token in the user agent;
    // present the page as the Chrome it is.
    QString userAgent = webProfile.httpUserAgent();
    userAgent.remove(QRegularExpression(R"(QtWebEngine/\S+ )"));
    webProfile.setHttpUserAgent(userAgent);
    QMetaObject::invokeMethod(webProfile.property("userScripts").value<QObject *>(), "insert",
                              Q_ARG(QWebEngineScript, passkeyRemoval()));

    QNetworkAccessManager network;
    const QString cacheDirectory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    DeveloperToken developerToken(&network, cacheDirectory + "/developer-token", userAgent);
    AppleMusic music(&network);
    Player player(widevine);
    Mpris mpris(&player);
    AudioOutputs outputs(app.applicationName());
    outputs.refresh();
    // Each track can open a new stream on the system default; move it as it
    // starts when another device is chosen.
    QObject::connect(&player, &Player::stateChanged, &app, [&] {
        if (player.isPlaying() && !outputs.selected().isEmpty())
            outputs.apply();
    });

    QObject::connect(&developerToken, &DeveloperToken::changed, &app, [&] {
        const bool isWebToken = !developerToken.isOfficial();
        music.setDeveloperToken(developerToken.token());
        music.setSendAppleOrigin(isWebToken);
        player.setDeveloperToken(developerToken.token(), isWebToken);
    });
    QObject::connect(&player, &Player::sessionChanged, &app, [&](const QString &token, const QString &storefront) {
        music.setUserToken(token);
        music.setStorefront(storefront);
    });
    // A refused token has expired or been rotated by Apple. Fetch it again,
    // but not on every failing request in a burst.
    QElapsedTimer sinceRefresh;
    QObject::connect(&music, &AppleMusic::developerTokenRejected, &app, [&] {
        if (sinceRefresh.isValid() && sinceRefresh.elapsed() < 60000)
            return;
        sinceRefresh.start();
        developerToken.refresh(true);
    });

    // Pages are handed to QML by the music client, never created there.
    qmlRegisterAnonymousType<Page>("Omasic", 1);

    CachingNetworkFactory networkFactory;
    QQmlApplicationEngine engine;
    engine.setNetworkAccessManagerFactory(&networkFactory);
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("webProfile", &webProfile);
    engine.rootContext()->setContextProperty("developerToken", &developerToken);
    engine.rootContext()->setContextProperty("music", &music);
    engine.rootContext()->setContextProperty("player", &player);
    engine.rootContext()->setContextProperty("mpris", &mpris);
    engine.rootContext()->setContextProperty("outputs", &outputs);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    developerToken.refresh();
    return app.exec();
}
