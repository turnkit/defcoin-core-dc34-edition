// Copyright (c) 2011-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TRAFFICGRAPHWIDGET_H
#define BITCOIN_QT_TRAFFICGRAPHWIDGET_H

#include <QWidget>
#include <QColor>
#include <QHash>
#include <QQueue>
#include <QString>
#include <QVector>

#include <utility>

class ClientModel;

QT_BEGIN_NAMESPACE
class QPaintEvent;
class QMouseEvent;
class QTimer;
QT_END_NAMESPACE

class TrafficGraphWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TrafficGraphWidget(QWidget *parent = nullptr);
    enum class TrafficMetric {
        Received,
        Sent,
        AvgRecentLatency,
        PeerAvgLatency,
        Jitter,
    };
    void setClientModel(ClientModel *model);
    int getGraphRangeMins() const;
    qint64 graphRangeSeconds() const;
    void setShowAvgPeerLatency(bool show);
    void setShowReceived(bool show);
    void setShowSent(bool show);
    void setShowAvgRecentLatency(bool show);
    void setShowPeerAvgLatency(bool show);
    void setShowJitter(bool show);
    void setShowMovingAverages(bool show);
    void setShowTrafficMovingAverages(bool show);
    void setShowLatencyMovingAverages(bool show);
    void setMovingAverageWindowMins(int mins);
    void setAlgorithmicScale(bool enabled);
    void setTrafficAlgorithmicScale(bool enabled);
    void setLatencyAlgorithmicScale(bool enabled);
    void setAdaptiveScale(bool enabled);
    void setTrafficAdaptiveScale(bool enabled);
    void setLatencyAdaptiveScale(bool enabled);
    void setShowAllHistory(bool show);
    void setScalePercent(int percent);
    int scalePercent() const;
    bool showAllHistory() const;
    void setAnimationPaused(bool paused);
    bool animationPaused() const;
    void setPanPercent(int percent);
    void setUpdatesActive(bool active);
    void setLatencyHideWorst20(bool enabled);
    void setLatencyShowOnlyWorst20(bool enabled);
    QColor metricColor(TrafficMetric metric) const;
    bool hasScrollableHistory() const;
    QString visibleWindowLabel() const;
    qint64 visibleRangeSeconds() const;
    qint64 retainedHistoryMsecs() const;
    QString exportSamplesCsv() const;

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

public Q_SLOTS:
    void updateRates();
    void setGraphRangeMins(int mins);
    void setGraphRangeSeconds(qint64 seconds);
    void clear();

Q_SIGNALS:
    void latencyStatsChanged(float recent_latency_ms, bool recent_latency_valid, float peer_average_latency_ms, bool peer_average_latency_valid, float jitter_ms, bool jitter_valid);

private:
    struct TrafficSample {
        qint64 timestamp_msecs;
        float in_rate;
        float out_rate;
        float avg_peer_latency_ms;
        bool avg_peer_latency_valid;
        float peer_jitter_ms;
        bool peer_jitter_valid;
    };

    struct PeerLatencySnapshot {
        float average_ms{0.0f};
        bool average_valid{false};
        float jitter_ms{0.0f};
        bool jitter_valid{false};
    };

    void paintPath(QPainterPath& path, const QVector<TrafficSample>& samples, bool inbound, qint64 window_end, qint64 range_msecs, float graph_max, bool close_to_baseline) const;
    void paintLatencyPath(QPainter& painter, const QVector<TrafficSample>& samples, qint64 window_end, qint64 range_msecs, bool draw_recent, bool draw_peer_average, bool draw_jitter) const;
    void paintMovingAveragePath(QPainter& painter, const QVector<TrafficSample>& samples, qint64 window_end, qint64 range_msecs, TrafficMetric metric, float graph_max) const;
    void paintTimeTicks(QPainter& painter, qint64 window_end, qint64 range_msecs) const;
    QVector<TrafficSample> visibleSamples(qint64 now) const;
    qint64 unanchoredVisibleEndMsecs(qint64 now, int pan_percent) const;
    float maxVisibleRate(const QVector<TrafficSample>& samples) const;
    std::pair<float, bool> averageVisibleLatencyMS(const QVector<TrafficSample>& samples) const;
    std::pair<float, bool> averageVisibleJitterMS(const QVector<TrafficSample>& samples) const;
    std::pair<float, bool> averagePeerLatencyMS() const;
    PeerLatencySnapshot peerLatencySnapshot();
    QVector<TrafficSample> latencyFilteredSamples(const QVector<TrafficSample>& samples) const;
    qint64 graphRangeMsecs() const;
    qint64 calculatedVisibleRangeMsecs(qint64 now) const;
    qint64 visibleRangeMsecs(qint64 now) const;
    qint64 visibleEndMsecs(qint64 now) const;
    float normalizedTrafficValue(float value, float graph_max) const;
    void trimSamples(qint64 now);
    void seedInitialRateSample(qint64 now, quint64 bytes_in, quint64 bytes_out);

    QTimer *timer;
    QTimer *repaint_timer;
    float fMax;
    int nMins;
    qint64 m_graph_range_msecs;
    QQueue<TrafficSample> vSamples;
    quint64 nLastBytesIn;
    quint64 nLastBytesOut;
    qint64 nLastSampleMsecs;
    ClientModel *clientModel;
    bool m_show_received;
    bool m_show_sent;
    bool m_show_avg_recent_latency;
    bool m_show_peer_avg_latency;
    bool m_show_jitter;
    bool m_show_moving_averages;
    bool m_show_traffic_moving_averages;
    bool m_show_latency_moving_averages;
    int m_moving_average_window_mins;
    bool m_show_avg_peer_latency;
    bool m_adaptive_scale;
    bool m_latency_adaptive_scale;
    bool m_algorithmic_scale;
    bool m_latency_algorithmic_scale;
    bool m_show_all_history;
    bool m_animation_paused{false};
    bool m_latency_hide_worst20{false};
    bool m_latency_show_only_worst20{false};
    int m_scale_percent;
    int m_pan_percent;
    qint64 m_pan_anchor_wall_msecs{0};
    qint64 m_pan_anchor_end_msecs{0};
    qint64 m_visible_range_anchor_msecs{0};
    bool m_updates_active{false};
    QHash<qint64, float> m_last_peer_latency_ms_by_node;
};

#endif // BITCOIN_QT_TRAFFICGRAPHWIDGET_H
