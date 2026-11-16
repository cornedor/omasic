#pragma once

#include <QColor>
#include <QImage>
#include <QList>
#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

// The colors the visualizer paints with: the three most present hues of the
// playing track's artwork, so every album looks like itself. Covers without
// color (black and white, or none at all) fall back to the theme accent.
class ArtworkPalette : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QColor fallback READ fallback WRITE setFallback NOTIFY fallbackChanged)
    Q_PROPERTY(QColor first READ first NOTIFY changed)
    Q_PROPERTY(QColor second READ second NOTIFY changed)
    Q_PROPERTY(QColor third READ third NOTIFY changed)

public:
    explicit ArtworkPalette(QNetworkAccessManager *network, QObject *parent = nullptr);

    QUrl source() const { return m_source; }
    void setSource(const QUrl &source);
    QColor fallback() const { return m_fallback; }
    void setFallback(const QColor &fallback);
    QColor first() const { return m_colors.value(0); }
    QColor second() const { return m_colors.value(1); }
    QColor third() const { return m_colors.value(2); }

    // Three vivid colors: the image's dominant hues, most present first,
    // filled up with neighbors of the fallback's hue when it has fewer.
    static QList<QColor> extract(const QImage &image, const QColor &fallback);

signals:
    void sourceChanged();
    void fallbackChanged();
    void changed();

private:
    void setImage(const QImage &image);

    QNetworkAccessManager *m_network;
    QNetworkReply *m_reply = nullptr;
    QUrl m_source;
    QColor m_fallback = QColor("#FFD60A");
    QImage m_image;
    QList<QColor> m_colors;
};
