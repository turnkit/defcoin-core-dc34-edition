// Copyright (c) 2011-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <interfaces/node.h>
#include <qt/trafficgraphwidget.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>

#include <net_processing.h>

#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QColor>
#include <QDateTime>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPalette>
#include <QPen>
#include <QSettings>
#include <QSet>
#include <QStringList>
#include <QToolTip>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>

#define DESIRED_SAMPLES         800

#define XMARGIN                 10
#define YMARGIN                 24
#define YBOTTOM_MARGIN          28

namespace {
static constexpr qint64 MAX_REASONABLE_LATENCY_USEC = 10 * 60 * 1000 * 1000;
static constexpr int PAN_POSITION_MAX = 1000;

struct ChartColors {
    QColor rx;
    QColor tx;
    QColor grid;
    QColor border;
    QColor ping;
    QColor background;
    QColor text;
};

ChartColors currentChartColors()
{
    const QPalette palette = QApplication::activeWindow() ? QApplication::activeWindow()->palette() : QApplication::palette();
    const QColor base_surface = palette.color(QPalette::Base);
    const QColor window_surface = palette.color(QPalette::Window);
    const QColor probe_surface = base_surface.isValid() ? base_surface : window_surface;
    const bool dark_surface = probe_surface.lightness() < 128;
    QString theme = GUIUtil::appearanceThemeBaseStyle(GUIUtil::getConfiguredAppearanceTheme());
    if (theme == QStringLiteral("auto") || theme == QStringLiteral("default") || theme.isEmpty()) {
        theme = dark_surface ? QStringLiteral("dark") : QStringLiteral("light");
    }
    if (theme == QStringLiteral("light")) {
        return {QColor("#21883E"), QColor("#0A66C2"), QColor("#B6C1CC"), QColor("#8AA1B8"), QColor("#A35C00"), QColor("#FFFFFF"), QColor("#1A202A")};
    }
    if (theme == QStringLiteral("dark")) {
        return {QColor("#4CFF8B"), QColor("#FF8C4A"), QColor("#5A6878"), QColor("#B7D3F2"), QColor("#FFD24A"), QColor("#000000"), QColor("#EDF3F8")};
    }
    if (theme == QStringLiteral("modern")) {
        return {QColor("#00FA9A"), QColor("#1E90FF"), QColor("#546071"), QColor("#FFFFFF"), QColor("#FF4DB8"), QColor("#05080C"), QColor("#F2F5F8")};
    }
    if (theme == QStringLiteral("34")) {
        return {QColor("#7CFC00"), QColor("#00BFFF"), QColor("#4B5B69"), QColor("#8FC7FF"), QColor("#FF00FF"), QColor("#000000"), QColor("#EEEEEE")};
    }
    if (theme == QStringLiteral("yam")) {
        return {QColor("#00EB00"), QColor("#E03030"), QColor("#D8D8D8"), QColor("#606060"), QColor("#C878FF"), QColor("#000000"), QColor("#CCCCCC")};
    }
    if (dark_surface) {
        return {QColor("#4CFF8B"), QColor("#FF8C4A"), QColor("#5A6878"), QColor("#B7D3F2"), QColor("#FFD24A"), QColor("#000000"), QColor("#EDF3F8")};
    }
    return {QColor("#21883E"), QColor("#0A66C2"), QColor("#B6C1CC"), QColor("#8AA1B8"), QColor("#A35C00"), QColor("#FFFFFF"), QColor("#1A202A")};
}

bool isReasonableLatencyUsec(qint64 ping_usec)
{
    return ping_usec > 0 && ping_usec <= MAX_REASONABLE_LATENCY_USEC;
}

bool isReasonableLatencyMS(float ping_ms)
{
    return ping_ms > 0.0f && ping_ms <= static_cast<float>(MAX_REASONABLE_LATENCY_USEC) / 1000.0f;
}

int fontMetricHorizontalAdvance(const QFontMetrics& metrics, const QString& text)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    return metrics.horizontalAdvance(text);
#else
    return metrics.width(text);
#endif
}

QString formatRateTick(float value, const QString& units)
{
    int precision = 0;
    if (value < 0.1f) {
        precision = 2;
    } else if (value < 10.0f) {
        precision = 1;
    }
    return QStringLiteral("%1 %2").arg(QString::number(value, 'f', precision), units);
}

float niceLatencyAxisMax(float max_value, float minimum_axis)
{
    const float target = std::max(max_value * 1.10f, minimum_axis);
    const float candidates[] = {
        5.0f, 10.0f, 20.0f, 50.0f, 75.0f, 100.0f, 150.0f, 250.0f,
        500.0f, 750.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f,
        30000.0f, 60000.0f
    };
    for (float candidate : candidates) {
        if (target <= candidate) return candidate;
    }
    return target;
}
}

TrafficGraphWidget::TrafficGraphWidget(QWidget *parent) :
    QWidget(parent),
    timer(nullptr),
    repaint_timer(nullptr),
    fMax(0.0f),
    nMins(15),
    m_graph_range_msecs(15 * 60 * 1000),
    vSamples(),
    nLastBytesIn(0),
    nLastBytesOut(0),
    nLastSampleMsecs(0),
    clientModel(nullptr),
    m_show_received(true),
    m_show_sent(true),
    m_show_avg_recent_latency(true),
    m_show_peer_avg_latency(true),
    m_show_jitter(true),
    m_show_moving_averages(false),
    m_show_traffic_moving_averages(false),
    m_show_latency_moving_averages(false),
    m_moving_average_window_mins(5),
    m_show_avg_peer_latency(true),
    m_adaptive_scale(true),
    m_latency_adaptive_scale(true),
    m_algorithmic_scale(false),
    m_latency_algorithmic_scale(false),
    m_show_all_history(false),
    m_animation_paused(false),
    m_latency_hide_worst20(false),
    m_latency_show_only_worst20(false),
    m_scale_percent(100),
    m_pan_percent(PAN_POSITION_MAX),
    m_pan_anchor_wall_msecs(0),
    m_pan_anchor_end_msecs(0)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &TrafficGraphWidget::updateRates);
    repaint_timer = new QTimer(this);
    repaint_timer->setInterval(100);
    connect(repaint_timer, &QTimer::timeout, this, [this] {
        if (!m_animation_paused) update();
    });
    QSettings settings;
    m_show_received = settings.value(QStringLiteral("TrafficGraphShowReceived"), true).toBool();
    m_show_sent = settings.value(QStringLiteral("TrafficGraphShowSent"), true).toBool();
    m_show_avg_recent_latency = settings.value(QStringLiteral("TrafficGraphShowAvgRecentLatency"), true).toBool();
    m_show_peer_avg_latency = settings.value(QStringLiteral("TrafficGraphShowPeerAvgLatency"), true).toBool();
    m_show_jitter = settings.value(QStringLiteral("TrafficGraphShowJitter"), true).toBool();
    m_show_moving_averages = settings.value(QStringLiteral("TrafficGraphShowMovingAverages"), false).toBool();
    m_show_traffic_moving_averages = settings.value(QStringLiteral("TrafficGraphShowTrafficMovingAverages"), m_show_moving_averages).toBool();
    m_show_latency_moving_averages = settings.value(QStringLiteral("TrafficGraphShowLatencyMovingAverages"), m_show_moving_averages).toBool();
    m_moving_average_window_mins = settings.value(QStringLiteral("TrafficGraphMovingAverageWindowMins"), 5).toInt();
    m_algorithmic_scale = settings.value(QStringLiteral("TrafficGraphAlgorithmicScale"), false).toBool();
    m_adaptive_scale = settings.value(QStringLiteral("TrafficGraphTrafficAdaptiveScale"), true).toBool();
    m_latency_algorithmic_scale = settings.value(QStringLiteral("TrafficGraphLatencyAlgorithmicScale"), false).toBool();
    m_latency_adaptive_scale = settings.value(QStringLiteral("TrafficGraphLatencyAdaptiveScale"), true).toBool();
    m_show_all_history = settings.value(QStringLiteral("TrafficGraphShowAllHistory"), false).toBool();
    m_latency_hide_worst20 = settings.value(QStringLiteral("TrafficGraphLatencyHideWorst20"), false).toBool();
    m_latency_show_only_worst20 = settings.value(QStringLiteral("TrafficGraphLatencyShowOnlyWorst20"), false).toBool();
    if (m_latency_show_only_worst20) m_latency_hide_worst20 = false;
    const qint64 saved_range_seconds = settings.value(QStringLiteral("TrafficGraphRangeSeconds"), 15 * 60).toLongLong();
    m_graph_range_msecs = std::max<qint64>(1000, saved_range_seconds * 1000);
    nMins = static_cast<int>((m_graph_range_msecs + 59999) / (60 * 1000));
    m_scale_percent = std::max(1, std::min(settings.value(QStringLiteral("TrafficGraphScalePercent"), 100).toInt(), 100));
    m_pan_percent = std::max(0, std::min(settings.value(QStringLiteral("TrafficGraphPanPercent"), PAN_POSITION_MAX).toInt(), PAN_POSITION_MAX));
    setMouseTracking(true);
}

void TrafficGraphWidget::setClientModel(ClientModel *model)
{
    clientModel = model;
    if(model) {
        nLastBytesIn = model->node().getTotalBytesRecv();
        nLastBytesOut = model->node().getTotalBytesSent();
        nLastSampleMsecs = QDateTime::currentMSecsSinceEpoch();
        if (m_updates_active && vSamples.isEmpty()) {
            QTimer::singleShot(0, this, &TrafficGraphWidget::updateRates);
        }
    }
}

int TrafficGraphWidget::getGraphRangeMins() const
{
    return nMins;
}

qint64 TrafficGraphWidget::graphRangeSeconds() const
{
    return std::max<qint64>(1, graphRangeMsecs() / 1000);
}

void TrafficGraphWidget::setShowAvgPeerLatency(bool show)
{
    setShowAvgRecentLatency(show);
    setShowPeerAvgLatency(show);
    m_show_avg_peer_latency = show;
}

void TrafficGraphWidget::setShowReceived(bool show)
{
    if (m_show_received == show) return;
    m_show_received = show;
    QSettings().setValue(QStringLiteral("TrafficGraphShowReceived"), show);
    update();
}

void TrafficGraphWidget::setShowSent(bool show)
{
    if (m_show_sent == show) return;
    m_show_sent = show;
    QSettings().setValue(QStringLiteral("TrafficGraphShowSent"), show);
    update();
}

void TrafficGraphWidget::setShowAvgRecentLatency(bool show)
{
    if (m_show_avg_recent_latency == show) return;
    m_show_avg_recent_latency = show;
    QSettings().setValue(QStringLiteral("TrafficGraphShowAvgRecentLatency"), show);
    update();
}

void TrafficGraphWidget::setShowPeerAvgLatency(bool show)
{
    if (m_show_peer_avg_latency == show) return;
    m_show_peer_avg_latency = show;
    QSettings().setValue(QStringLiteral("TrafficGraphShowPeerAvgLatency"), show);
    update();
}

void TrafficGraphWidget::setShowJitter(bool show)
{
    if (m_show_jitter == show) return;
    m_show_jitter = show;
    QSettings().setValue(QStringLiteral("TrafficGraphShowJitter"), show);
    update();
}

void TrafficGraphWidget::setShowMovingAverages(bool show)
{
    if (m_show_moving_averages == show && m_show_traffic_moving_averages == show && m_show_latency_moving_averages == show) return;
    m_show_moving_averages = show;
    m_show_traffic_moving_averages = show;
    m_show_latency_moving_averages = show;
    QSettings().setValue(QStringLiteral("TrafficGraphShowMovingAverages"), show);
    QSettings().setValue(QStringLiteral("TrafficGraphShowTrafficMovingAverages"), show);
    QSettings().setValue(QStringLiteral("TrafficGraphShowLatencyMovingAverages"), show);
    update();
}

void TrafficGraphWidget::setShowTrafficMovingAverages(bool show)
{
    if (m_show_traffic_moving_averages == show) return;
    m_show_traffic_moving_averages = show;
    m_show_moving_averages = m_show_traffic_moving_averages || m_show_latency_moving_averages;
    QSettings().setValue(QStringLiteral("TrafficGraphShowTrafficMovingAverages"), show);
    QSettings().setValue(QStringLiteral("TrafficGraphShowMovingAverages"), m_show_moving_averages);
    update();
}

void TrafficGraphWidget::setShowLatencyMovingAverages(bool show)
{
    if (m_show_latency_moving_averages == show) return;
    m_show_latency_moving_averages = show;
    m_show_moving_averages = m_show_traffic_moving_averages || m_show_latency_moving_averages;
    QSettings().setValue(QStringLiteral("TrafficGraphShowLatencyMovingAverages"), show);
    QSettings().setValue(QStringLiteral("TrafficGraphShowMovingAverages"), m_show_moving_averages);
    update();
}

void TrafficGraphWidget::setMovingAverageWindowMins(int mins)
{
    const int bounded_mins = std::max(1, std::min(mins, 240));
    if (m_moving_average_window_mins == bounded_mins) return;
    m_moving_average_window_mins = bounded_mins;
    QSettings().setValue(QStringLiteral("TrafficGraphMovingAverageWindowMins"), bounded_mins);
    update();
}

void TrafficGraphWidget::setAlgorithmicScale(bool enabled)
{
    if (m_algorithmic_scale == enabled && m_latency_algorithmic_scale == enabled) return;
    m_algorithmic_scale = enabled;
    m_latency_algorithmic_scale = enabled;
    QSettings().setValue(QStringLiteral("TrafficGraphAlgorithmicScale"), enabled);
    QSettings().setValue(QStringLiteral("TrafficGraphLatencyAlgorithmicScale"), enabled);
    update();
}

void TrafficGraphWidget::setTrafficAlgorithmicScale(bool enabled)
{
    if (m_algorithmic_scale == enabled) return;
    m_algorithmic_scale = enabled;
    QSettings().setValue(QStringLiteral("TrafficGraphAlgorithmicScale"), enabled);
    update();
}

void TrafficGraphWidget::setLatencyAlgorithmicScale(bool enabled)
{
    if (m_latency_algorithmic_scale == enabled) return;
    m_latency_algorithmic_scale = enabled;
    QSettings().setValue(QStringLiteral("TrafficGraphLatencyAlgorithmicScale"), enabled);
    update();
}

void TrafficGraphWidget::setAdaptiveScale(bool enabled)
{
    if (m_adaptive_scale == enabled && m_latency_adaptive_scale == enabled) return;
    m_adaptive_scale = enabled;
    m_latency_adaptive_scale = enabled;
    QSettings().setValue(QStringLiteral("TrafficGraphTrafficAdaptiveScale"), enabled);
    QSettings().setValue(QStringLiteral("TrafficGraphLatencyAdaptiveScale"), enabled);
    update();
}

void TrafficGraphWidget::setTrafficAdaptiveScale(bool enabled)
{
    if (m_adaptive_scale == enabled) return;
    m_adaptive_scale = enabled;
    QSettings().setValue(QStringLiteral("TrafficGraphTrafficAdaptiveScale"), enabled);
    update();
}

void TrafficGraphWidget::setLatencyAdaptiveScale(bool enabled)
{
    if (m_latency_adaptive_scale == enabled) return;
    m_latency_adaptive_scale = enabled;
    QSettings().setValue(QStringLiteral("TrafficGraphLatencyAdaptiveScale"), enabled);
    update();
}

void TrafficGraphWidget::setShowAllHistory(bool show)
{
    if (m_show_all_history == show) return;
    m_show_all_history = show;
    QSettings().setValue(QStringLiteral("TrafficGraphShowAllHistory"), show);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_visible_range_anchor_msecs = show ? 0 : calculatedVisibleRangeMsecs(now);
    m_pan_anchor_wall_msecs = now;
    m_pan_anchor_end_msecs = unanchoredVisibleEndMsecs(now, m_pan_percent);
    update();
}

void TrafficGraphWidget::setScalePercent(int percent)
{
    const int bounded_percent = std::max(1, std::min(percent, 100));
    if (m_scale_percent == bounded_percent) return;
    m_scale_percent = bounded_percent;
    QSettings().setValue(QStringLiteral("TrafficGraphScalePercent"), bounded_percent);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_visible_range_anchor_msecs = bounded_percent >= 100 ? 0 : calculatedVisibleRangeMsecs(now);
    m_pan_anchor_wall_msecs = now;
    m_pan_anchor_end_msecs = unanchoredVisibleEndMsecs(now, m_pan_percent);
    update();
}

int TrafficGraphWidget::scalePercent() const
{
    return m_scale_percent;
}

bool TrafficGraphWidget::showAllHistory() const
{
    return m_show_all_history;
}

void TrafficGraphWidget::setAnimationPaused(bool paused)
{
    if (m_animation_paused == paused) return;
    m_animation_paused = paused;
    if (!paused) update();
}

bool TrafficGraphWidget::animationPaused() const
{
    return m_animation_paused;
}

void TrafficGraphWidget::setLatencyHideWorst20(bool enabled)
{
    if (enabled && m_latency_show_only_worst20) {
        m_latency_show_only_worst20 = false;
        QSettings().setValue(QStringLiteral("TrafficGraphLatencyShowOnlyWorst20"), false);
    }
    if (m_latency_hide_worst20 == enabled) return;
    m_latency_hide_worst20 = enabled;
    QSettings().setValue(QStringLiteral("TrafficGraphLatencyHideWorst20"), enabled);
    update();
}

void TrafficGraphWidget::setLatencyShowOnlyWorst20(bool enabled)
{
    if (enabled && m_latency_hide_worst20) {
        m_latency_hide_worst20 = false;
        QSettings().setValue(QStringLiteral("TrafficGraphLatencyHideWorst20"), false);
    }
    if (m_latency_show_only_worst20 == enabled) return;
    m_latency_show_only_worst20 = enabled;
    QSettings().setValue(QStringLiteral("TrafficGraphLatencyShowOnlyWorst20"), enabled);
    update();
}

void TrafficGraphWidget::setPanPercent(int percent)
{
    const int bounded_percent = std::max(0, std::min(percent, PAN_POSITION_MAX));
    if (m_pan_percent == bounded_percent) return;
    m_pan_percent = bounded_percent;
    QSettings().setValue(QStringLiteral("TrafficGraphPanPercent"), bounded_percent);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_pan_anchor_wall_msecs = now;
    m_pan_anchor_end_msecs = unanchoredVisibleEndMsecs(now, m_pan_percent);
    update();
}

void TrafficGraphWidget::setUpdatesActive(bool active)
{
    m_updates_active = active;
    if (active) {
        if (!timer->isActive()) {
            timer->start();
        }
        if (!repaint_timer->isActive()) {
            repaint_timer->start();
        }
        // Seed the plot immediately when the tab is opened. The cumulative
        // totals can already be non-zero before the graph has sampled rates.
        if (vSamples.isEmpty()) {
            updateRates();
        }
    } else {
        timer->stop();
        repaint_timer->stop();
    }
}

bool TrafficGraphWidget::hasScrollableHistory() const
{
    if (m_show_all_history || vSamples.isEmpty()) return false;
    const qint64 newest = vSamples.front().timestamp_msecs;
    const qint64 oldest = vSamples.back().timestamp_msecs;
    return newest - oldest > visibleRangeMsecs(QDateTime::currentMSecsSinceEpoch()) + 250;
}

qint64 TrafficGraphWidget::retainedHistoryMsecs() const
{
    if (vSamples.isEmpty()) return 60 * 1000;
    const qint64 newest = vSamples.front().timestamp_msecs;
    const qint64 oldest = vSamples.back().timestamp_msecs;
    return std::max<qint64>(60 * 1000, newest - oldest);
}

QString TrafficGraphWidget::visibleWindowLabel() const
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_show_all_history) return tr("All");

    const qint64 window_end = visibleEndMsecs(now);
    const qint64 window_start = window_end - visibleRangeMsecs(now);
    const QString format = visibleRangeMsecs(now) <= 24 * 60 * 60 * 1000
        ? QStringLiteral("h:mm:ss AP")
        : QStringLiteral("M/d h:mm:ss AP");
    const QString start = QDateTime::fromMSecsSinceEpoch(window_start).toString(format);
    if (m_pan_percent >= PAN_POSITION_MAX - 1 || !hasScrollableHistory()) {
        return tr("%1 - Present").arg(start);
    }
    const QString end = QDateTime::fromMSecsSinceEpoch(window_end).toString(format);
    return tr("%1 - %2").arg(start, end);
}

QString TrafficGraphWidget::exportSamplesCsv() const
{
    QString csv = QStringLiteral("timestamp,received_kbps,sent_kbps,avg_peer_latency_ms,jitter_ms\n");
    QVector<TrafficSample> chronological;
    chronological.reserve(vSamples.size());
    for (auto it = vSamples.crbegin(); it != vSamples.crend(); ++it) {
        chronological.push_back(*it);
    }
    for (const TrafficSample& sample : chronological) {
        csv += QDateTime::fromMSecsSinceEpoch(sample.timestamp_msecs).toString(Qt::ISODateWithMs);
        csv += QStringLiteral(",");
        csv += QString::number(sample.in_rate, 'f', 6);
        csv += QStringLiteral(",");
        csv += QString::number(sample.out_rate, 'f', 6);
        csv += QStringLiteral(",");
        if (sample.avg_peer_latency_valid) csv += QString::number(sample.avg_peer_latency_ms, 'f', 3);
        csv += QStringLiteral(",");
        if (sample.peer_jitter_valid) csv += QString::number(sample.peer_jitter_ms, 'f', 3);
        csv += QStringLiteral("\n");
    }
    return csv;
}

qint64 TrafficGraphWidget::visibleRangeSeconds() const
{
    return std::max<qint64>(1, visibleRangeMsecs(QDateTime::currentMSecsSinceEpoch()) / 1000);
}

QColor TrafficGraphWidget::metricColor(TrafficMetric metric) const
{
    const ChartColors colors = currentChartColors();
    switch (metric) {
    case TrafficMetric::Received:
        return colors.rx;
    case TrafficMetric::Sent:
        return colors.tx;
    case TrafficMetric::AvgRecentLatency:
    {
        const QString theme = GUIUtil::appearanceThemeBaseStyle(GUIUtil::getConfiguredAppearanceTheme());
        if (theme == QStringLiteral("34")) return QColor("#FF4DF8");
        if (theme == QStringLiteral("yam")) return QColor("#C878FF");
        if (theme == QStringLiteral("light")) return QColor("#A35C00");
        return colors.ping;
    }
    case TrafficMetric::PeerAvgLatency:
    {
        const QString theme = GUIUtil::appearanceThemeBaseStyle(GUIUtil::getConfiguredAppearanceTheme());
        if (theme == QStringLiteral("34")) return QColor("#FFB000");
        if (theme == QStringLiteral("yam")) return QColor("#FFD24A");
        if (theme == QStringLiteral("light")) return QColor("#7A4FC2");
        return QColor("#FF8C4A");
    }
    case TrafficMetric::Jitter:
    {
        const QString theme = GUIUtil::appearanceThemeBaseStyle(GUIUtil::getConfiguredAppearanceTheme());
        if (theme == QStringLiteral("34")) return QColor("#7A5CFF");
        if (theme == QStringLiteral("yam")) return QColor("#00D6FF");
        if (theme == QStringLiteral("light")) return QColor("#D13C70");
        return QColor("#00D6FF");
    }
    }
    return colors.border;
}

qint64 TrafficGraphWidget::graphRangeMsecs() const
{
    return std::max<qint64>(1000, m_graph_range_msecs);
}

qint64 TrafficGraphWidget::calculatedVisibleRangeMsecs(qint64 now) const
{
    Q_UNUSED(now);
    if (m_show_all_history && !vSamples.isEmpty()) {
        return retainedHistoryMsecs();
    }
    if (m_scale_percent >= 100) return graphRangeMsecs();
    return std::max<qint64>(1000, graphRangeMsecs() * m_scale_percent / 100);
}

qint64 TrafficGraphWidget::visibleRangeMsecs(qint64 now) const
{
    if (!m_show_all_history && m_scale_percent < 100 && m_visible_range_anchor_msecs > 0) {
        return m_visible_range_anchor_msecs;
    }
    return calculatedVisibleRangeMsecs(now);
}

qint64 TrafficGraphWidget::unanchoredVisibleEndMsecs(qint64 now, int pan_percent) const
{
    if (vSamples.isEmpty()) return now;

    const qint64 newest = std::max(now, vSamples.front().timestamp_msecs);
    const qint64 oldest = vSamples.back().timestamp_msecs;
    const qint64 range = visibleRangeMsecs(now);
    if (m_show_all_history) return oldest + range;
    if (newest - oldest <= range) return oldest + range;

    const qint64 min_end = oldest + range;
    const qint64 max_end = newest;
    return min_end + ((max_end - min_end) * pan_percent / PAN_POSITION_MAX);
}

qint64 TrafficGraphWidget::visibleEndMsecs(qint64 now) const
{
    if (vSamples.isEmpty()) return now;

    const qint64 newest = std::max(now, vSamples.front().timestamp_msecs);
    const qint64 oldest = vSamples.back().timestamp_msecs;
    const qint64 range = visibleRangeMsecs(now);
    if (m_show_all_history) return oldest + range;
    if (newest - oldest <= range) return oldest + range;

    if (m_pan_percent >= PAN_POSITION_MAX - 1) return newest;

    if (m_pan_anchor_wall_msecs <= 0 || m_pan_anchor_end_msecs <= 0) {
        return unanchoredVisibleEndMsecs(now, m_pan_percent);
    }
    return std::min(newest, m_pan_anchor_end_msecs + (now - m_pan_anchor_wall_msecs));
}

QVector<float> niceScaleTicks(float max_value, int preferred_count)
{
    QVector<float> ticks;
    if (max_value <= 0.0f || preferred_count <= 0) return ticks;

    const float raw_step = max_value / static_cast<float>(preferred_count);
    const float magnitude = std::pow(10.0f, std::floor(std::log10(raw_step)));
    const float normalized = raw_step / magnitude;
    float nice_normalized = 10.0f;
    if (normalized <= 1.0f) {
        nice_normalized = 1.0f;
    } else if (normalized <= 2.0f) {
        nice_normalized = 2.0f;
    } else if (normalized <= 5.0f) {
        nice_normalized = 5.0f;
    }
    const float step = nice_normalized * magnitude;
    for (float value = step; value < max_value + step * 0.25f; value += step) {
        ticks.push_back(value);
        if (ticks.size() >= 6) break;
    }
    return ticks;
}

QVector<TrafficGraphWidget::TrafficSample> TrafficGraphWidget::visibleSamples(qint64 now) const
{
    QVector<TrafficSample> samples;
    const qint64 window_end = visibleEndMsecs(now);
    const qint64 min_time = window_end - visibleRangeMsecs(now);
    for (const TrafficSample& sample : vSamples) {
        if (sample.timestamp_msecs > window_end) continue;
        if (sample.timestamp_msecs < min_time) {
            if (!samples.isEmpty()) {
                const TrafficSample newer = samples.last();
                const qint64 span = std::max<qint64>(1, newer.timestamp_msecs - sample.timestamp_msecs);
                const double t = std::max(0.0, std::min(1.0, static_cast<double>(min_time - sample.timestamp_msecs) / static_cast<double>(span)));
                auto lerp = [t](float older, float newer_value) {
                    return static_cast<float>(older + (newer_value - older) * t);
                };
                TrafficSample edge = sample;
                edge.timestamp_msecs = min_time;
                edge.in_rate = lerp(sample.in_rate, newer.in_rate);
                edge.out_rate = lerp(sample.out_rate, newer.out_rate);
                edge.avg_peer_latency_valid = sample.avg_peer_latency_valid && newer.avg_peer_latency_valid;
                edge.peer_jitter_valid = sample.peer_jitter_valid && newer.peer_jitter_valid;
                edge.avg_peer_latency_ms = edge.avg_peer_latency_valid ? lerp(sample.avg_peer_latency_ms, newer.avg_peer_latency_ms) : 0.0f;
                edge.peer_jitter_ms = edge.peer_jitter_valid ? lerp(sample.peer_jitter_ms, newer.peer_jitter_ms) : 0.0f;
                samples.push_back(edge);
            } else {
                TrafficSample edge = sample;
                edge.timestamp_msecs = min_time;
                samples.push_back(edge);
            }
            break;
        }
        samples.push_back(sample);
    }
    return samples;
}

float TrafficGraphWidget::maxVisibleRate(const QVector<TrafficSample>& samples) const
{
    float max_rate = 0.0f;
    for (const TrafficSample& sample : samples) {
        if (m_show_received) max_rate = std::max(max_rate, sample.in_rate);
        if (m_show_sent) max_rate = std::max(max_rate, sample.out_rate);
    }
    return max_rate;
}

std::pair<float, bool> TrafficGraphWidget::averageVisibleLatencyMS(const QVector<TrafficSample>& samples) const
{
    float total = 0.0f;
    int count = 0;
    for (const TrafficSample& sample : samples) {
        if (!sample.avg_peer_latency_valid || !isReasonableLatencyMS(sample.avg_peer_latency_ms)) continue;
        total += sample.avg_peer_latency_ms;
        ++count;
    }
    if (count <= 0) return {0.0f, false};
    return {total / count, true};
}

std::pair<float, bool> TrafficGraphWidget::averageVisibleJitterMS(const QVector<TrafficSample>& samples) const
{
    float total = 0.0f;
    int count = 0;
    for (const TrafficSample& sample : samples) {
        if (!sample.peer_jitter_valid || !isReasonableLatencyMS(sample.peer_jitter_ms)) continue;
        total += sample.peer_jitter_ms;
        ++count;
    }
    if (count <= 0) return {0.0f, false};
    return {total / count, true};
}

QVector<TrafficGraphWidget::TrafficSample> TrafficGraphWidget::latencyFilteredSamples(const QVector<TrafficSample>& samples) const
{
    if ((!m_latency_hide_worst20 && !m_latency_show_only_worst20) || samples.size() < 5) return samples;

    QVector<float> values;
    values.reserve(samples.size());
    for (const TrafficSample& sample : samples) {
        if (sample.avg_peer_latency_valid && isReasonableLatencyMS(sample.avg_peer_latency_ms)) {
            values.push_back(sample.avg_peer_latency_ms);
        }
    }
    if (values.size() < 5) return samples;

    std::sort(values.begin(), values.end());
    const int threshold_index = std::max(0, std::min(values.size() - 1, static_cast<int>(std::floor(values.size() * 0.80))));
    const float threshold = values.at(threshold_index);

    QVector<TrafficSample> filtered;
    filtered.reserve(samples.size());
    for (TrafficSample sample : samples) {
        const bool valid = sample.avg_peer_latency_valid && isReasonableLatencyMS(sample.avg_peer_latency_ms);
        const bool worst = valid && sample.avg_peer_latency_ms >= threshold;
        if (m_latency_hide_worst20 && worst) {
            sample.avg_peer_latency_valid = false;
            sample.peer_jitter_valid = false;
        } else if (m_latency_show_only_worst20 && !worst) {
            sample.avg_peer_latency_valid = false;
            sample.peer_jitter_valid = false;
        }
        filtered.push_back(sample);
    }
    return filtered;
}

std::pair<float, bool> TrafficGraphWidget::averagePeerLatencyMS() const
{
    if (!clientModel) return {0.0f, false};

    interfaces::Node::NodesStats nodes_stats;
    clientModel->node().getNodesStats(nodes_stats);
    qint64 total_usec = 0;
    int count = 0;
    for (const auto& node_stats : nodes_stats) {
        const CNodeStats& stats = std::get<0>(node_stats);
        const qint64 ping_usec = isReasonableLatencyUsec(stats.m_ping_usec) ? stats.m_ping_usec :
            (isReasonableLatencyUsec(stats.m_min_ping_usec) ? stats.m_min_ping_usec : 0);
        if (ping_usec <= 0) continue;
        total_usec += ping_usec;
        ++count;
    }
    if (count <= 0) return {0.0f, false};
    return {static_cast<float>(total_usec) / static_cast<float>(count) / 1000.0f, true};
}

TrafficGraphWidget::PeerLatencySnapshot TrafficGraphWidget::peerLatencySnapshot()
{
    PeerLatencySnapshot snapshot;
    if (!clientModel) return snapshot;

    interfaces::Node::NodesStats nodes_stats;
    clientModel->node().getNodesStats(nodes_stats);
    qint64 total_usec = 0;
    int count = 0;
    float total_jitter_ms = 0.0f;
    int jitter_count = 0;
    QSet<qint64> seen_node_ids;

    for (const auto& node_stats : nodes_stats) {
        const CNodeStats& stats = std::get<0>(node_stats);
        const qint64 ping_usec = isReasonableLatencyUsec(stats.m_ping_usec) ? stats.m_ping_usec :
            (isReasonableLatencyUsec(stats.m_min_ping_usec) ? stats.m_min_ping_usec : 0);
        if (ping_usec <= 0) continue;

        const float latency_ms = static_cast<float>(ping_usec) / 1000.0f;
        total_usec += ping_usec;
        ++count;
        seen_node_ids.insert(static_cast<qint64>(stats.nodeid));

        const auto previous = m_last_peer_latency_ms_by_node.constFind(static_cast<qint64>(stats.nodeid));
        if (previous != m_last_peer_latency_ms_by_node.constEnd() && isReasonableLatencyMS(*previous)) {
            total_jitter_ms += std::abs(latency_ms - *previous);
            ++jitter_count;
        }
        m_last_peer_latency_ms_by_node.insert(static_cast<qint64>(stats.nodeid), latency_ms);
    }

    for (auto it = m_last_peer_latency_ms_by_node.begin(); it != m_last_peer_latency_ms_by_node.end();) {
        if (!seen_node_ids.contains(it.key())) {
            it = m_last_peer_latency_ms_by_node.erase(it);
        } else {
            ++it;
        }
    }

    if (count > 0) {
        snapshot.average_ms = static_cast<float>(total_usec) / static_cast<float>(count) / 1000.0f;
        snapshot.average_valid = true;
    }
    if (jitter_count > 0) {
        snapshot.jitter_ms = total_jitter_ms / jitter_count;
        snapshot.jitter_valid = true;
    }
    return snapshot;
}

void TrafficGraphWidget::trimSamples(qint64 now)
{
    const qint64 history_msecs = std::max<qint64>(24 * 60 * 60 * 1000, graphRangeMsecs() * 2);
    const qint64 min_time = now - history_msecs;
    while (!vSamples.isEmpty() && vSamples.back().timestamp_msecs < min_time) {
        vSamples.pop_back();
    }
}

void TrafficGraphWidget::seedInitialRateSample(qint64 now, quint64 bytes_in, quint64 bytes_out)
{
    if (!vSamples.isEmpty() || (bytes_in == 0 && bytes_out == 0)) return;

    const qint64 seed_span = std::max<qint64>(1000, std::min<qint64>(30000, graphRangeMsecs() / 12));
    vSamples.push_back({
        now - seed_span,
        0.0f,
        0.0f,
        0.0f,
        false,
        0.0f,
        false,
    });
}

float TrafficGraphWidget::normalizedTrafficValue(float value, float graph_max) const
{
    if (graph_max <= 0.0f) return 0.0f;
    const float bounded_value = std::max(0.0f, std::min(value, graph_max));
    if (m_algorithmic_scale) {
        return std::log10(1.0f + bounded_value) / std::log10(1.0f + graph_max);
    }
    return bounded_value / graph_max;
}

void TrafficGraphWidget::paintPath(QPainterPath& path, const QVector<TrafficSample>& samples, bool inbound, qint64 window_end, qint64 range_msecs, float graph_max, bool close_to_baseline) const
{
    if (samples.isEmpty() || graph_max <= 0.0f) return;

    const int h = height() - YMARGIN - YBOTTOM_MARGIN;
    const int w = width() - XMARGIN * 2;
    const int baseline = YMARGIN + h;
    const qint64 min_time = window_end - range_msecs;
    int last_x = XMARGIN + w;
    bool started = false;
    for (const TrafficSample& sample : samples) {
        const qint64 elapsed = std::max<qint64>(0, std::min<qint64>(range_msecs, sample.timestamp_msecs - min_time));
        const int x = XMARGIN + static_cast<int>(w * elapsed / std::max<qint64>(1, range_msecs));
        const float rate = inbound ? sample.in_rate : sample.out_rate;
        const int y = baseline - static_cast<int>(h * normalizedTrafficValue(rate, graph_max));
        if (!started) {
            if (close_to_baseline) {
                path.moveTo(x, baseline);
                path.lineTo(x, y);
            } else {
                path.moveTo(x, y);
            }
        } else {
            path.lineTo(x, y);
        }
        started = true;
        last_x = x;
    }
    if (close_to_baseline) {
        path.lineTo(last_x, baseline);
        path.closeSubpath();
    }
}

void TrafficGraphWidget::paintLatencyPath(QPainter& painter, const QVector<TrafficSample>& samples, qint64 window_end, qint64 range_msecs, bool draw_recent, bool draw_peer_average, bool draw_jitter) const
{
    if ((!draw_recent && !draw_peer_average && !draw_jitter) || samples.isEmpty()) return;

    float max_latency = 0.0f;
    float max_jitter = 0.0f;
    bool has_latency = false;
    bool has_jitter = false;
    for (const TrafficSample& sample : samples) {
        if ((draw_recent || draw_peer_average) && sample.avg_peer_latency_valid && isReasonableLatencyMS(sample.avg_peer_latency_ms)) {
            max_latency = has_latency ? std::max(max_latency, sample.avg_peer_latency_ms) : sample.avg_peer_latency_ms;
            has_latency = true;
        }
        if (draw_jitter && sample.peer_jitter_valid && isReasonableLatencyMS(sample.peer_jitter_ms)) {
            max_jitter = has_jitter ? std::max(max_jitter, sample.peer_jitter_ms) : sample.peer_jitter_ms;
            has_jitter = true;
        }
    }
    if (!has_latency && !has_jitter) return;

    const int h = height() - YMARGIN - YBOTTOM_MARGIN;
    const int w = width() - XMARGIN * 2;
    const qint64 min_time = window_end - range_msecs;
    const float latency_max = m_latency_adaptive_scale
        ? niceLatencyAxisMax(max_latency, 5.0f)
        : niceLatencyAxisMax(max_latency, 50.0f);
    const float jitter_max = m_latency_adaptive_scale
        ? niceLatencyAxisMax(max_jitter, 1.0f)
        : niceLatencyAxisMax(max_jitter, 10.0f);
    auto normalize_latency = [this, latency_max](float value) {
        const float bounded_value = std::max(0.0f, std::min(value, latency_max));
        if (m_latency_algorithmic_scale) {
            return std::log10(1.0f + bounded_value) / std::log10(1.0f + latency_max);
        }
        return bounded_value / latency_max;
    };
    auto normalize_jitter = [this, jitter_max](float value) {
        const float bounded_value = std::max(0.0f, std::min(value, jitter_max));
        if (m_latency_algorithmic_scale) {
            return std::log10(1.0f + bounded_value) / std::log10(1.0f + jitter_max);
        }
        return bounded_value / jitter_max;
    };
    const qint64 expected_gap = std::max<qint64>(timer ? timer->interval() * 3 : 3000, 3000);

    struct LatencyPoint {
        int x;
        int y;
        qint64 timestamp;
    };
    QVector<LatencyPoint> points;
    QVector<LatencyPoint> jitter_points;
    for (auto it = samples.crbegin(); it != samples.crend(); ++it) {
        const TrafficSample& sample = *it;
        const qint64 elapsed = std::max<qint64>(0, std::min<qint64>(range_msecs, sample.timestamp_msecs - min_time));
        const int x = XMARGIN + static_cast<int>(w * elapsed / std::max<qint64>(1, range_msecs));
        if (sample.avg_peer_latency_valid && isReasonableLatencyMS(sample.avg_peer_latency_ms)) {
            const int y = YMARGIN + h - static_cast<int>(h * normalize_latency(sample.avg_peer_latency_ms));
            points.push_back({x, y, sample.timestamp_msecs});
        }
        if (sample.peer_jitter_valid && isReasonableLatencyMS(sample.peer_jitter_ms)) {
            const int jitter_midline = YMARGIN + h / 2;
            const int y = jitter_midline - static_cast<int>((h / 2 - 2) * normalize_jitter(sample.peer_jitter_ms));
            jitter_points.push_back({x, y, sample.timestamp_msecs});
        }
    }
    if (points.isEmpty() && jitter_points.isEmpty()) return;

    const ChartColors colors = currentChartColors();
    const QFontMetrics latency_fm(painter.font());
    const QString low_label = tr("0 ms");
    const QString high_label = tr("%1 ms").arg(QString::number(latency_max, 'f', latency_max < 10.0f ? 1 : 0));
    painter.setPen(QColor(colors.ping.red(), colors.ping.green(), colors.ping.blue(), 190));
    painter.drawText(width() - XMARGIN - fontMetricHorizontalAdvance(latency_fm, high_label), YMARGIN + latency_fm.ascent(), high_label);
    painter.drawText(width() - XMARGIN - fontMetricHorizontalAdvance(latency_fm, low_label), YMARGIN + h - 2, low_label);
    if (draw_jitter && has_jitter) {
        const int jitter_midline = YMARGIN + h / 2;
        const QColor jitter_color = metricColor(TrafficMetric::Jitter);
        QPen jitter_axis_pen(QColor(jitter_color.red(), jitter_color.green(), jitter_color.blue(), 120), 1);
        jitter_axis_pen.setStyle(Qt::DashLine);
        painter.setPen(jitter_axis_pen);
        painter.drawLine(XMARGIN, jitter_midline, width() - XMARGIN, jitter_midline);
        const QString jitter_label = tr("0 jitter");
        painter.setPen(QColor(jitter_color.red(), jitter_color.green(), jitter_color.blue(), 205));
        painter.drawText(width() - XMARGIN - fontMetricHorizontalAdvance(latency_fm, jitter_label),
                         jitter_midline - 3,
                         jitter_label);
    }

    QPen solid_pen(metricColor(TrafficMetric::AvgRecentLatency), 2);
    solid_pen.setStyle(Qt::SolidLine);
    solid_pen.setCapStyle(Qt::RoundCap);
    QPen gap_pen(metricColor(TrafficMetric::AvgRecentLatency), 2);
    gap_pen.setStyle(Qt::DashLine);
    gap_pen.setCapStyle(Qt::RoundCap);
    QPen marker_pen(QColor(colors.ping.red(), colors.ping.green(), colors.ping.blue(), 150), 1);
    marker_pen.setStyle(Qt::DashLine);

    if (draw_recent && points.size() == 1) {
        painter.setPen(solid_pen);
        painter.drawPoint(points.first().x, points.first().y);
    }

    if (draw_recent) {
        for (int i = 1; i < points.size(); ++i) {
            const LatencyPoint& prev = points.at(i - 1);
            const LatencyPoint& current = points.at(i);
            const bool has_gap = current.timestamp - prev.timestamp > expected_gap;
            painter.setPen(has_gap ? gap_pen : solid_pen);
            painter.drawLine(prev.x, prev.y, current.x, current.y);
            if (has_gap) {
                painter.setPen(marker_pen);
                painter.drawLine(current.x, prev.y, current.x, current.y);
            }
        }
    }

    if (draw_peer_average) {
        const auto average_latency = averageVisibleLatencyMS(samples);
        if (average_latency.second) {
            const int avg_y = YMARGIN + h - static_cast<int>(h * normalize_latency(average_latency.first));
            QPen avg_pen(metricColor(TrafficMetric::PeerAvgLatency), 2);
            avg_pen.setStyle(Qt::DotLine);
            avg_pen.setCapStyle(Qt::RoundCap);
            painter.setPen(avg_pen);
            painter.drawLine(XMARGIN, avg_y, width() - XMARGIN, avg_y);
        }
    }

    if (draw_jitter && !jitter_points.isEmpty()) {
        QPen jitter_pen(metricColor(TrafficMetric::Jitter), 2);
        jitter_pen.setStyle(Qt::DashDotLine);
        jitter_pen.setCapStyle(Qt::RoundCap);
        painter.setPen(jitter_pen);
        if (jitter_points.size() == 1) {
            painter.drawPoint(jitter_points.first().x, jitter_points.first().y);
        } else {
            for (int i = 1; i < jitter_points.size(); ++i) {
                painter.drawLine(jitter_points.at(i - 1).x, jitter_points.at(i - 1).y, jitter_points.at(i).x, jitter_points.at(i).y);
            }
        }
    }
}

void TrafficGraphWidget::paintMovingAveragePath(QPainter& painter, const QVector<TrafficSample>& samples, qint64 window_end, qint64 range_msecs, TrafficMetric metric, float graph_max) const
{
    // PeerAvgLatency is already drawn as a visible-window average reference line.
    // Drawing a second moving-average path for it makes the chart look like an
    // unrelated 1000 ms latency series when a single high sample expands the axis.
    if (metric == TrafficMetric::PeerAvgLatency) return;
    const bool latency_metric = metric == TrafficMetric::AvgRecentLatency || metric == TrafficMetric::PeerAvgLatency || metric == TrafficMetric::Jitter;
    if (samples.size() < 2) return;
    if (latency_metric ? !m_show_latency_moving_averages : !m_show_traffic_moving_averages) return;

    QVector<TrafficSample> chronological;
    chronological.reserve(samples.size());
    for (auto it = samples.crbegin(); it != samples.crend(); ++it) {
        chronological.push_back(*it);
    }

    const qint64 window_msecs = static_cast<qint64>(m_moving_average_window_mins) * 60 * 1000;
    const int h = height() - YMARGIN - YBOTTOM_MARGIN;
    const int w = width() - XMARGIN * 2;
    const int baseline = YMARGIN + h;
    const qint64 min_time = window_end - range_msecs;

    float max_latency = 0.0f;
    if (metric == TrafficMetric::AvgRecentLatency || metric == TrafficMetric::PeerAvgLatency || metric == TrafficMetric::Jitter) {
        for (const TrafficSample& sample : chronological) {
            if (sample.timestamp_msecs < min_time || sample.timestamp_msecs > window_end) continue;
            if ((metric == TrafficMetric::AvgRecentLatency || metric == TrafficMetric::PeerAvgLatency) && sample.avg_peer_latency_valid && isReasonableLatencyMS(sample.avg_peer_latency_ms)) {
                max_latency = std::max(max_latency, sample.avg_peer_latency_ms);
            } else if (metric == TrafficMetric::Jitter && sample.peer_jitter_valid && isReasonableLatencyMS(sample.peer_jitter_ms)) {
                max_latency = std::max(max_latency, sample.peer_jitter_ms);
            }
        }
        if (max_latency <= 0.0f) return;
        max_latency = metric == TrafficMetric::Jitter
            ? niceLatencyAxisMax(max_latency, m_latency_adaptive_scale ? 1.0f : 10.0f)
            : niceLatencyAxisMax(max_latency, m_latency_adaptive_scale ? 5.0f : 50.0f);
    }

    QPainterPath path;
    bool started = false;
    for (int i = 0; i < chronological.size(); ++i) {
        const TrafficSample& sample = chronological.at(i);
        if (sample.timestamp_msecs < min_time || sample.timestamp_msecs > window_end) continue;
        float total = 0.0f;
        int count = 0;
        for (int j = i; j >= 0; --j) {
            const TrafficSample& candidate = chronological.at(j);
            if (sample.timestamp_msecs - candidate.timestamp_msecs > window_msecs) break;
            float value = 0.0f;
            bool valid = true;
            switch (metric) {
            case TrafficMetric::Received:
                value = candidate.in_rate;
                break;
            case TrafficMetric::Sent:
                value = candidate.out_rate;
                break;
            case TrafficMetric::AvgRecentLatency:
            case TrafficMetric::PeerAvgLatency:
                valid = candidate.avg_peer_latency_valid && isReasonableLatencyMS(candidate.avg_peer_latency_ms);
                value = candidate.avg_peer_latency_ms;
                break;
            case TrafficMetric::Jitter:
                valid = candidate.peer_jitter_valid && isReasonableLatencyMS(candidate.peer_jitter_ms);
                value = candidate.peer_jitter_ms;
                break;
            }
            if (!valid) continue;
            total += value;
            ++count;
        }
        if (count <= 0) continue;

        const float average = total / count;
        const qint64 elapsed = std::max<qint64>(0, std::min<qint64>(range_msecs, sample.timestamp_msecs - min_time));
        const int x = XMARGIN + static_cast<int>(w * elapsed / std::max<qint64>(1, range_msecs));
        const float normalized = latency_metric
            ? (m_latency_algorithmic_scale ? std::log10(1.0f + average) / std::log10(1.0f + std::max(1.0f, max_latency)) : average / std::max(1.0f, max_latency))
            : normalizedTrafficValue(average, graph_max);
        const int y = latency_metric
            ? (metric == TrafficMetric::Jitter
                ? YMARGIN + h / 2 - static_cast<int>((h / 2 - 2) * normalized)
                : baseline - static_cast<int>(h * normalized))
            : baseline - static_cast<int>(h * normalized);
        if (!started) {
            path.moveTo(x, y);
            started = true;
        } else {
            path.lineTo(x, y);
        }
    }

    if (!started) return;
    QPen pen(metricColor(metric), 1.6);
    pen.setStyle(Qt::DashLine);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawPath(path);
}

void TrafficGraphWidget::paintTimeTicks(QPainter& painter, qint64 window_end, qint64 range_msecs) const
{
    const ChartColors colors = currentChartColors();
    const int w = width() - XMARGIN * 2;
    const int y = height() - 9;
    const int graph_bottom = height() - YBOTTOM_MARGIN;
    painter.setPen(colors.border);
    painter.drawLine(XMARGIN, graph_bottom, width() - XMARGIN, graph_bottom);

    const int tick_count = 4;
    const QFontMetrics fm(painter.font());
    for (int i = 0; i <= tick_count; ++i) {
        const int x = XMARGIN + w * i / tick_count;
        const qint64 tick_time = window_end - (range_msecs * (tick_count - i) / tick_count);
        const bool newest_tick = i == tick_count && qAbs(QDateTime::currentMSecsSinceEpoch() - window_end) < 5000;
        const int seconds_ago = static_cast<int>((range_msecs / 1000) * (tick_count - i) / tick_count);
        const int minutes_ago = seconds_ago / 60;
        QString label;
        if (newest_tick) {
            label = tr("Present");
        } else if (range_msecs <= 10 * 60 * 1000) {
            label = seconds_ago >= 60 ? tr("%1:%2").arg(minutes_ago).arg(seconds_ago % 60, 2, 10, QLatin1Char('0')) : tr("%1 s").arg(seconds_ago);
        } else if (range_msecs <= 60 * 60 * 1000) {
            label = tr("%1 min").arg(minutes_ago);
        } else {
            label = QDateTime::fromMSecsSinceEpoch(tick_time).toString(i == 0 ? QStringLiteral("M/d h:mm AP") : QStringLiteral("h:mm AP"));
        }
        painter.drawLine(x, graph_bottom, x, graph_bottom + 4);
        const int label_width = fontMetricHorizontalAdvance(fm, label);
        const int text_x = std::max(XMARGIN, std::min(width() - XMARGIN - label_width, x - label_width / 2));
        painter.drawText(text_x, y, label);
    }
}

void TrafficGraphWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const ChartColors colors = currentChartColors();
    painter.fillRect(rect(), colors.background);

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 window_end = visibleEndMsecs(now);
    const qint64 range_msecs = visibleRangeMsecs(now);
    const QVector<TrafficSample> samples = visibleSamples(now);
    const QVector<TrafficSample> latency_samples = latencyFilteredSamples(samples);
    fMax = maxVisibleRate(samples);
    const float graph_max = m_adaptive_scale
        ? (fMax > 0.0f ? std::max(0.05f, fMax * 1.10f) : 0.05f)
        : std::max(10.0f, fMax);
    const bool draw_traffic_scale = m_show_received || m_show_sent;

    QColor axisCol(colors.grid);
    int h = height() - YMARGIN - YBOTTOM_MARGIN;
    painter.setPen(axisCol);
    painter.drawLine(XMARGIN, YMARGIN + h, width() - XMARGIN, YMARGIN + h);

    if (draw_traffic_scale) {
        const QString units = tr("KB/s");
        const QFontMetrics scale_metrics(painter.font());
        const QVector<float> ticks = niceScaleTicks(graph_max, 4);
        painter.setPen(axisCol);
        for (const float value : ticks) {
            const int yy = YMARGIN + h - h * normalizedTrafficValue(value, graph_max);
            painter.drawLine(XMARGIN, yy, width() - XMARGIN, yy);
            const QString label = formatRateTick(value, units);
            const int baseline = std::max(YMARGIN + scale_metrics.ascent(),
                                          std::min(YMARGIN + h - 2, yy - 3));
            painter.setPen(colors.text);
            painter.drawText(XMARGIN, baseline, label);
            painter.setPen(axisCol);
        }
    }

    painter.setRenderHint(QPainter::Antialiasing);
    if (samples.empty()) {
        const int baseline = YMARGIN + h;
        if (clientModel) {
            QTimer::singleShot(0, const_cast<TrafficGraphWidget*>(this), &TrafficGraphWidget::updateRates);
        }
        if (m_show_received) {
            painter.setPen(QPen(colors.rx, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawLine(XMARGIN, baseline - 3, width() - XMARGIN, baseline - 3);
        }
        if (m_show_sent) {
            painter.setPen(QPen(colors.tx, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawLine(XMARGIN, baseline - 7, width() - XMARGIN, baseline - 7);
        }
        paintTimeTicks(painter, window_end, range_msecs);
        return;
    }
    QPainterPath received_area;
    QPainterPath received_line;
    QPainterPath sent_area;
    QPainterPath sent_line;
    float received_max_rate = 0.0f;
    float sent_max_rate = 0.0f;
    if (m_show_received) {
        for (const TrafficSample& sample : samples) {
            received_max_rate = std::max(received_max_rate, sample.in_rate);
        }
        paintPath(received_area, samples, true, window_end, range_msecs, graph_max, true);
        paintPath(received_line, samples, true, window_end, range_msecs, graph_max, false);
    }
    if (m_show_sent) {
        for (const TrafficSample& sample : samples) {
            sent_max_rate = std::max(sent_max_rate, sample.out_rate);
        }
        paintPath(sent_area, samples, false, window_end, range_msecs, graph_max, true);
        paintPath(sent_line, samples, false, window_end, range_msecs, graph_max, false);
    }

    if(m_show_received) {
        painter.setPen(QPen(colors.rx, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (received_max_rate <= 0.0001f) {
            const int baseline = YMARGIN + h;
            painter.drawLine(XMARGIN, baseline - 3, width() - XMARGIN, baseline - 3);
        } else if (received_line.elementCount() == 1) {
            const QPainterPath::Element point = received_line.elementAt(0);
            const int baseline = YMARGIN + h;
            const qreal y = std::min<qreal>(point.y, baseline - 3);
            painter.drawLine(XMARGIN, y, width() - XMARGIN, y);
        } else {
            painter.fillPath(received_area, QColor(colors.rx.red(), colors.rx.green(), colors.rx.blue(), 56));
            painter.drawPath(received_line);
        }
    }
    if(m_show_sent) {
        painter.setPen(QPen(colors.tx, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (sent_max_rate <= 0.0001f) {
            const int baseline = YMARGIN + h;
            painter.drawLine(XMARGIN, baseline - 7, width() - XMARGIN, baseline - 7);
        } else if (sent_line.elementCount() == 1) {
            const QPainterPath::Element point = sent_line.elementAt(0);
            const int baseline = YMARGIN + h;
            const qreal y = std::min<qreal>(point.y, baseline - 7);
            painter.drawLine(XMARGIN, y, width() - XMARGIN, y);
        } else {
            painter.fillPath(sent_area, QColor(colors.tx.red(), colors.tx.green(), colors.tx.blue(), 56));
            painter.drawPath(sent_line);
        }
    }
    if (!samples.empty() && (m_show_traffic_moving_averages || m_show_latency_moving_averages)) {
        if (m_show_received) paintMovingAveragePath(painter, samples, window_end, range_msecs, TrafficMetric::Received, graph_max);
        if (m_show_sent) paintMovingAveragePath(painter, samples, window_end, range_msecs, TrafficMetric::Sent, graph_max);
        if (m_show_avg_recent_latency) paintMovingAveragePath(painter, latency_samples, window_end, range_msecs, TrafficMetric::AvgRecentLatency, graph_max);
        if (m_show_peer_avg_latency) paintMovingAveragePath(painter, latency_samples, window_end, range_msecs, TrafficMetric::PeerAvgLatency, graph_max);
        if (m_show_jitter) paintMovingAveragePath(painter, latency_samples, window_end, range_msecs, TrafficMetric::Jitter, graph_max);
    }
    paintLatencyPath(painter, latency_samples, window_end, range_msecs, m_show_avg_recent_latency, m_show_peer_avg_latency, m_show_jitter);
    paintTimeTicks(painter, window_end, range_msecs);
}

void TrafficGraphWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!event) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 window_end = visibleEndMsecs(now);
    const qint64 range_msecs = visibleRangeMsecs(now);
    const QVector<TrafficSample> samples = visibleSamples(now);
    if (samples.isEmpty()) {
        QToolTip::hideText();
        return;
    }

    const int w = width() - XMARGIN * 2;
    if (w <= 0 || event->pos().x() < XMARGIN || event->pos().x() > width() - XMARGIN) {
        QToolTip::hideText();
        return;
    }
    const qint64 min_time = window_end - range_msecs;
    const double fraction = static_cast<double>(event->pos().x() - XMARGIN) / static_cast<double>(w);
    const qint64 target_time = min_time + static_cast<qint64>(range_msecs * fraction);

    const TrafficSample* closest = nullptr;
    qint64 closest_delta = std::numeric_limits<qint64>::max();
    for (const TrafficSample& sample : samples) {
        const qint64 delta = sample.timestamp_msecs > target_time ? sample.timestamp_msecs - target_time : target_time - sample.timestamp_msecs;
        if (delta < closest_delta) {
            closest = &sample;
            closest_delta = delta;
        }
    }
    if (!closest) {
        QToolTip::hideText();
        return;
    }

    QStringList lines;
    lines << QDateTime::fromMSecsSinceEpoch(closest->timestamp_msecs).toString(QStringLiteral("M/d h:mm:ss AP"));
    lines << tr("Received: %1 KB/s").arg(QString::number(closest->in_rate, 'f', closest->in_rate < 10.0f ? 2 : 1));
    lines << tr("Sent: %1 KB/s").arg(QString::number(closest->out_rate, 'f', closest->out_rate < 10.0f ? 2 : 1));
    if (closest->avg_peer_latency_valid) lines << tr("Latency: %1 ms").arg(QString::number(closest->avg_peer_latency_ms, 'f', closest->avg_peer_latency_ms < 10.0f ? 1 : 0));
    if (closest->peer_jitter_valid) lines << tr("Jitter: %1 ms").arg(QString::number(closest->peer_jitter_ms, 'f', closest->peer_jitter_ms < 10.0f ? 1 : 0));
    QToolTip::showText(event->globalPos(), lines.join(QLatin1Char('\n')), this);
}

void TrafficGraphWidget::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    QToolTip::hideText();
}

void TrafficGraphWidget::updateRates()
{
    if(!clientModel) return;

    quint64 bytesIn = clientModel->node().getTotalBytesRecv(),
            bytesOut = clientModel->node().getTotalBytesSent();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 elapsed_msecs = std::max<qint64>(1, nLastSampleMsecs > 0 ? now - nLastSampleMsecs : timer->interval());
    float inRate = (bytesIn - nLastBytesIn) / 1024.0f * 1000 / elapsed_msecs;
    float outRate = (bytesOut - nLastBytesOut) / 1024.0f * 1000 / elapsed_msecs;
    if (vSamples.isEmpty() && (bytesIn > 0 || bytesOut > 0)) {
        const qint64 seed_window_msecs = std::max<qint64>(1000, std::min<qint64>(30000, graphRangeMsecs() / 12));
        inRate = bytesIn / 1024.0f * 1000 / seed_window_msecs;
        outRate = bytesOut / 1024.0f * 1000 / seed_window_msecs;
        seedInitialRateSample(now, bytesIn, bytesOut);
    }
    PeerLatencySnapshot latency;
#if ENABLE_DEFCOIN_FUN_UI
    latency = peerLatencySnapshot();
#endif
    vSamples.push_front({now, inRate, outRate, latency.average_ms, latency.average_valid, latency.jitter_ms, latency.jitter_valid});
    nLastBytesIn = bytesIn;
    nLastBytesOut = bytesOut;
    nLastSampleMsecs = now;

    trimSamples(now);

    const QVector<TrafficSample> visible = visibleSamples(now);
    const QVector<TrafficSample> latency_visible = latencyFilteredSamples(visible);
    fMax = maxVisibleRate(visible);
    const auto visible_average = averageVisibleLatencyMS(latency_visible);
    const auto visible_jitter = averageVisibleJitterMS(latency_visible);
    Q_EMIT latencyStatsChanged(latency.average_ms, latency.average_valid, visible_average.first, visible_average.second, visible_jitter.first, visible_jitter.second);
    if (!m_animation_paused) update();
}

void TrafficGraphWidget::setGraphRangeMins(int mins)
{
    setGraphRangeSeconds(static_cast<qint64>(std::max(1, mins)) * 60);
}

void TrafficGraphWidget::setGraphRangeSeconds(qint64 seconds)
{
    const qint64 bounded_seconds = std::max<qint64>(1, seconds);
    m_graph_range_msecs = bounded_seconds * 1000;
    nMins = static_cast<int>((m_graph_range_msecs + 59999) / (60 * 1000));
    QSettings().setValue(QStringLiteral("TrafficGraphRangeSeconds"), bounded_seconds);
    int msecsPerSample = std::max<int>(250, std::min<int>(1000, graphRangeMsecs() / DESIRED_SAMPLES));
    timer->stop();
    timer->setInterval(msecsPerSample);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_visible_range_anchor_msecs = m_show_all_history ? 0 : calculatedVisibleRangeMsecs(now);
    m_pan_anchor_wall_msecs = now;
    m_pan_anchor_end_msecs = unanchoredVisibleEndMsecs(now, m_pan_percent);
    trimSamples(now);
    fMax = maxVisibleRate(visibleSamples(now));
    if (m_updates_active) {
        timer->start();
        if (repaint_timer && !repaint_timer->isActive()) {
            repaint_timer->start();
        }
        if (vSamples.isEmpty()) {
            QTimer::singleShot(0, this, &TrafficGraphWidget::updateRates);
        }
    }
    update();
}

void TrafficGraphWidget::clear()
{
    timer->stop();

    vSamples.clear();
    m_last_peer_latency_ms_by_node.clear();
    fMax = 0.0f;

    if(clientModel) {
        nLastBytesIn = clientModel->node().getTotalBytesRecv();
        nLastBytesOut = clientModel->node().getTotalBytesSent();
    }
    nLastSampleMsecs = QDateTime::currentMSecsSinceEpoch();
    if (m_updates_active) {
        timer->start();
        if (repaint_timer && !repaint_timer->isActive()) {
            repaint_timer->start();
        }
        QTimer::singleShot(0, this, &TrafficGraphWidget::updateRates);
    }
}
