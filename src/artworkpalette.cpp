#include "artworkpalette.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtMath>
#include <algorithm>
#include <array>

namespace {
constexpr int HueBuckets = 12;
constexpr int Colors = 3;

// Colors read as light on a dark field: keep the hue, lift the rest.
QColor vivid(qreal hue, qreal saturation) {
    hue = hue - std::floor(hue);
    return QColor::fromHsvF(float(hue), float(qBound(0.55, saturation, 0.9)), 1.0f);
}
}

ArtworkPalette::ArtworkPalette(QNetworkAccessManager *network, QObject *parent)
    : QObject(parent), m_network(network), m_colors(extract({}, m_fallback)) {}

void ArtworkPalette::setSource(const QUrl &source) {
    if (source == m_source)
        return;
    m_source = source;
    emit sourceChanged();

    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (!m_source.isValid() || m_source.isEmpty()) {
        setImage({});
        return;
    }
    QNetworkReply *reply = m_network->get(QNetworkRequest(m_source));
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_reply = nullptr;
        // An unreadable cover gives an empty image: the fallback colors.
        setImage(QImage::fromData(reply->readAll()));
    });
}

void ArtworkPalette::setFallback(const QColor &fallback) {
    if (fallback == m_fallback)
        return;
    m_fallback = fallback;
    emit fallbackChanged();
    setImage(m_image);
}

void ArtworkPalette::setImage(const QImage &image) {
    m_image = image;
    const QList<QColor> colors = extract(m_image, m_fallback);
    if (colors == m_colors)
        return;
    m_colors = colors;
    emit changed();
}

QList<QColor> ArtworkPalette::extract(const QImage &image, const QColor &fallback) {
    struct Bucket {
        qreal weight = 0, x = 0, y = 0, saturation = 0;
    };
    std::array<Bucket, HueBuckets> buckets;
    qreal total = 0;

    const QImage small = image.isNull() ? image
                                        : image.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                              .convertToFormat(QImage::Format_RGB32);
    for (int y = 0; y < small.height(); ++y) {
        for (int x = 0; x < small.width(); ++x) {
            const QColor pixel = small.pixelColor(x, y);
            const qreal hue = pixel.hsvHueF();
            // Grays and near-blacks have no hue worth showing.
            const qreal weight = hue < 0 ? 0 : pixel.hsvSaturationF() * pixel.hsvSaturationF() * pixel.valueF();
            if (weight < 0.04)
                continue;
            // The hue is averaged as an angle, so reds either side of 0 meet.
            Bucket &bucket = buckets[int(hue * HueBuckets) % HueBuckets];
            bucket.weight += weight;
            bucket.x += weight * std::cos(hue * 2 * M_PI);
            bucket.y += weight * std::sin(hue * 2 * M_PI);
            bucket.saturation += weight * pixel.hsvSaturationF();
            total += weight;
        }
    }

    QList<QColor> colors;
    std::array<int, HueBuckets> order;
    for (int i = 0; i < HueBuckets; ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) { return buckets[a].weight > buckets[b].weight; });
    QList<int> taken;
    for (int index : order) {
        const Bucket &bucket = buckets[index];
        // A hue has to cover a real part of the colored area to count.
        if (colors.size() == Colors || bucket.weight < total * 0.06 || total < 4)
            break;
        // Neighboring buckets are the same color twice.
        const bool adjacent = std::any_of(taken.begin(), taken.end(), [index](int other) {
            const int distance = std::abs(index - other);
            return std::min(distance, HueBuckets - distance) < 2;
        });
        if (adjacent)
            continue;
        taken << index;
        colors << vivid(std::atan2(bucket.y, bucket.x) / (2 * M_PI), bucket.saturation / bucket.weight);
    }

    // Fewer than three hues: surround the main one (or the theme accent)
    // with its neighbors on the color wheel.
    const QColor base = colors.isEmpty() ? fallback : colors.first();
    const qreal baseHue = base.hsvHueF() < 0 ? 0.13 : base.hsvHueF();
    const qreal baseSaturation = base.hsvHueF() < 0 ? 0.7 : base.hsvSaturationF();
    if (colors.isEmpty())
        colors << vivid(baseHue, baseSaturation);
    static const qreal shifts[] = {0.11, -0.14};
    for (int i = 0; colors.size() < Colors; ++i)
        colors << vivid(baseHue + shifts[i % 2], baseSaturation);
    return colors;
}
