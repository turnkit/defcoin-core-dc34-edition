// Copyright (c) 2011-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/rpcconsole.h>
#include <qt/forms/ui_debugwindow.h>

#include <qt/bantablemodel.h>
#include <qt/clientmodel.h>
#if ENABLE_DEFCOIN_FUN_UI
#include <qt/defconmeetuppeers.h>
#include <qt/defcoindummypeers.h>
#endif
#include <qt/guiconstants.h>
#include <qt/platformstyle.h>
#include <qt/walletmodel.h>
#include <chainparams.h>
#include <clientversion.h>
#include <interfaces/node.h>
#include <netbase.h>
#include <net_processing.h>
#include <rpc/server.h>
#include <rpc/client.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <util/threadnames.h>

#include <univalue.h>

#ifdef ENABLE_WALLET
#include <wallet/db.h>
#include <wallet/wallet.h>
#endif

#include <QFont>
#include <QFontMetrics>
#include <QAbstractSocket>
#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QClipboard>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QComboBox>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QHash>
#include <QHideEvent>
#include <QKeyEvent>
#include <QHeaderView>
#include <QImage>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLayout>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QFrame>
#include <QNetworkAccessManager>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QRegExp>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
#include <QScrollArea>
#include <QSettings>
#include <QSet>
#include <QSharedPointer>
#include <QShortcut>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSplitter>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTabBar>
#include <QTabWidget>
#include <QTableView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QTcpSocket>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QTime>
#include <QTimer>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QVariantList>
#include <QVector>
#include <QVector3D>
#include <QWheelEvent>
#include <QDir>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <utility>

const int CONSOLE_HISTORY = 50;
const int INITIAL_TRAFFIC_GRAPH_MINS = 15;
const QSize FONT_RANGE(4, 40);
const char fontSizeSettingsKey[] = "consoleFontSize";
const char peerTableHeaderStateKey[] = "PeerTableHeaderState";
const char peerTableColumnTitlesKey[] = "PeerTableColumnTitles";
const char peerTablePingHealthBaseWidthKey[] = "PeerTablePingHealthBaseWidth";
const char peerPanelSplitterStateKey[] = "PeerPanelSplitterState";
const char peerTableCompactRowsKey[] = "PeerTableCompactRows";

const struct {
    const char *url;
    const char *source;
} ICON_MAPPING[] = {
    {"cmd-request", ":/icons/tx_input"},
    {"cmd-reply", ":/icons/tx_output"},
    {"cmd-error", ":/icons/tx_output"},
    {"misc", ":/icons/tx_inout"},
    {nullptr, nullptr}
};

namespace {

// don't add private key handling cmd's to the history
const QStringList historyFilter = QStringList()
    << "importprivkey"
    << "importmulti"
    << "sethdseed"
    << "signmessagewithprivkey"
    << "signrawtransactionwithkey"
    << "walletpassphrase"
    << "walletpassphrasechange"
    << "encryptwallet";

static constexpr int TRAFFIC_RANGE_SLIDER_MIN = 1;
static constexpr int TRAFFIC_RANGE_SLIDER_MAX = 100;
static constexpr int TRAFFIC_WINDOW_SLIDER_MIN = 0;
static constexpr int TRAFFIC_WINDOW_SLIDER_MAX = 1000;
static constexpr int TRAFFIC_WINDOW_SLIDER_ALL = TRAFFIC_WINDOW_SLIDER_MAX + 1;
static constexpr qint64 TRAFFIC_WINDOW_SECONDS_MIN = 1;
static constexpr qint64 TRAFFIC_WINDOW_SECONDS_MAX = 24 * 60 * 60;

QString debugLogFilePath()
{
    return GUIUtil::boostPathToQString(GetDataDir() / "debug.log");
}

QString compactDurationText(qint64 seconds)
{
    seconds = std::max<qint64>(0, seconds);
    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    const qint64 secs = seconds % 60;
    QStringList parts;
    const qint64 days = hours / 24;
    const qint64 rem_hours = hours % 24;
    if (days > 0) parts << QStringLiteral("%1d").arg(days);
    if (rem_hours > 0) parts << QStringLiteral("%1h").arg(rem_hours);
    if (minutes > 0) parts << QStringLiteral("%1m").arg(minutes);
    parts << QStringLiteral("%1s").arg(secs);
    return parts.join(QString());
}

qint64 trafficWindowSecondsForSliderValue(int value)
{
    if (value >= TRAFFIC_WINDOW_SLIDER_ALL) return TRAFFIC_WINDOW_SECONDS_MAX;

    const int bounded_value = std::max(TRAFFIC_WINDOW_SLIDER_MIN, std::min(value, TRAFFIC_WINDOW_SLIDER_MAX));
    const double normalized = static_cast<double>(bounded_value - TRAFFIC_WINDOW_SLIDER_MIN) /
        static_cast<double>(std::max(1, TRAFFIC_WINDOW_SLIDER_MAX - TRAFFIC_WINDOW_SLIDER_MIN));
    const double log_min = std::log(static_cast<double>(TRAFFIC_WINDOW_SECONDS_MIN));
    const double log_max = std::log(static_cast<double>(TRAFFIC_WINDOW_SECONDS_MAX));
    return std::max<qint64>(TRAFFIC_WINDOW_SECONDS_MIN,
        std::min<qint64>(TRAFFIC_WINDOW_SECONDS_MAX, static_cast<qint64>(std::llround(std::exp(log_min + (log_max - log_min) * normalized)))));
}

int trafficWindowSliderValueForSeconds(qint64 seconds)
{
    const qint64 bounded_seconds = std::max<qint64>(TRAFFIC_WINDOW_SECONDS_MIN, std::min<qint64>(seconds, TRAFFIC_WINDOW_SECONDS_MAX));
    const double log_min = std::log(static_cast<double>(TRAFFIC_WINDOW_SECONDS_MIN));
    const double log_max = std::log(static_cast<double>(TRAFFIC_WINDOW_SECONDS_MAX));
    const double normalized = (std::log(static_cast<double>(bounded_seconds)) - log_min) / std::max(0.000001, log_max - log_min);
    return std::max(TRAFFIC_WINDOW_SLIDER_MIN,
        std::min(TRAFFIC_WINDOW_SLIDER_MAX, TRAFFIC_WINDOW_SLIDER_MIN + static_cast<int>(std::llround(normalized * (TRAFFIC_WINDOW_SLIDER_MAX - TRAFFIC_WINDOW_SLIDER_MIN)))));
}

bool parseCompactDurationText(const QString& text, qint64& seconds_out)
{
    QString value = text.trimmed().toLower();
    if (value.isEmpty()) return false;

    bool plain_number = false;
    const qint64 plain_seconds = value.toLongLong(&plain_number);
    if (plain_number && plain_seconds > 0) {
        seconds_out = plain_seconds;
        return true;
    }

    QRegExp token(QStringLiteral("(\\d+)\\s*(d|day|days|h|hr|hrs|hour|hours|m|min|mins|minute|minutes|s|sec|secs|second|seconds)"));
    int pos = 0;
    qint64 total = 0;
    bool found = false;
    while ((pos = token.indexIn(value, pos)) != -1) {
        const qint64 amount = token.cap(1).toLongLong();
        const QString unit = token.cap(2);
        if (unit.startsWith(QLatin1Char('d'))) {
            total += amount * 24 * 3600;
        } else if (unit.startsWith(QLatin1Char('h'))) {
            total += amount * 3600;
        } else if (unit.startsWith(QLatin1Char('m'))) {
            total += amount * 60;
        } else {
            total += amount;
        }
        pos += token.matchedLength();
        found = true;
    }

    if (!found || total <= 0) return false;
    seconds_out = total;
    return true;
}

class TrafficWindowSlider : public QSlider
{
public:
    explicit TrafficWindowSlider(QWidget* parent = nullptr) : QSlider(Qt::Horizontal, parent)
    {
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        connect(this, &QSlider::valueChanged, this, [this] {
            if (m_drag_mode == DragMode::None) {
                rememberCenterRatio(viewportCenterRatio());
            }
        });
    }

    void setWindowPercentChangedCallback(std::function<void(int)> callback)
    {
        m_window_percent_changed = std::move(callback);
    }

    int windowPercent() const
    {
        return m_window_percent;
    }

    bool isDraggingViewport() const
    {
        return m_drag_mode != DragMode::None;
    }

    void setWindowPercent(int percent)
    {
        const int bounded_percent = std::max(1, std::min(percent, 100));
        const double center_ratio = m_have_center_anchor ? m_center_anchor_ratio : viewportCenterRatio();
        if (m_window_percent == bounded_percent) {
            QSignalBlocker blocker(this);
            setPanFromCenterRatio(center_ratio);
            rememberCenterRatio(center_ratio);
            update();
            return;
        }
        m_window_percent = bounded_percent;
        QSignalBlocker blocker(this);
        setPanFromCenterRatio(center_ratio);
        rememberCenterRatio(center_ratio);
        update();
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            const QRectF viewport = viewportRect();
            if (viewport.isValid()) {
                constexpr qreal edge_hit_width = 30.0;
                if (std::abs(event->pos().x() - viewport.left()) <= edge_hit_width) {
                    m_drag_mode = DragMode::ResizeLeft;
                    setCursor(Qt::SizeHorCursor);
                    event->accept();
                    return;
                }
                if (std::abs(event->pos().x() - viewport.right()) <= edge_hit_width) {
                    m_drag_mode = DragMode::ResizeRight;
                    setCursor(Qt::SizeHorCursor);
                    event->accept();
                    return;
                }
                if (viewport.contains(event->pos())) {
                    m_drag_mode = DragMode::Move;
                    m_drag_offset = event->pos().x() - viewport.left();
                    setCursor(Qt::ClosedHandCursor);
                    event->accept();
                    return;
                }
            }
        }
        QSlider::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (m_drag_mode == DragMode::None) {
            updateHoverCursor(event->pos());
            event->accept();
            return;
        }

        const QRectF track = trackRect();
        if (track.width() <= 1.0) {
            event->accept();
            return;
        }

        QRectF viewport = viewportRect();
        if (!viewport.isValid()) {
            event->accept();
            return;
        }

        if (m_drag_mode == DragMode::Move) {
            const qreal new_left = std::max(track.left(), std::min(event->pos().x() - m_drag_offset, track.right() - viewport.width()));
            setPanFromViewportLeft(track, new_left, viewport.width());
            rememberCenterRatio(centerRatioForViewport(track, new_left, new_left + viewport.width()));
        } else if (m_drag_mode == DragMode::ResizeLeft) {
            const qreal right = viewport.right();
            const qreal new_left = std::max(track.left(), std::min<qreal>(event->pos().x(), right - 2.0));
            setWindowFromViewport(track, new_left, right);
        } else if (m_drag_mode == DragMode::ResizeRight) {
            const qreal left = viewport.left();
            const qreal new_right = std::min(track.right(), std::max<qreal>(event->pos().x(), left + 2.0));
            setWindowFromViewport(track, left, new_right);
        }
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (m_drag_mode != DragMode::None) {
            m_drag_mode = DragMode::None;
            updateHoverCursor(event->pos());
            event->accept();
            return;
        }
        QSlider::mouseReleaseEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        if (m_drag_mode == DragMode::None) unsetCursor();
        QSlider::leaveEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QSlider::resizeEvent(event);
        if (m_have_center_anchor) {
            QSignalBlocker blocker(this);
            setPanFromCenterRatio(m_center_anchor_ratio);
        }
        update();
    }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        const QRectF track = trackRect();
        if (track.width() <= 1.0) return;
        const QRectF viewport = viewportRect();

        QColor line_color = palette().color(QPalette::Dark);
        line_color.setAlpha(isEnabled() ? 220 : 120);
        QColor fill = palette().highlight().color();
        fill.setAlpha(isEnabled() ? 230 : 96);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(line_color, 5.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(track.left(), track.center().y()), QPointF(track.right(), track.center().y()));
        painter.setBrush(fill);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(viewport, 3.0, 3.0);
        if (viewport.width() >= 12.0) {
            QColor handle_color = palette().highlightedText().color();
            handle_color.setAlpha(isEnabled() ? 190 : 90);
            painter.setPen(QPen(handle_color, 2.2, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(QPointF(viewport.left() + 5.0, viewport.top() + 2.0), QPointF(viewport.left() + 5.0, viewport.bottom() - 2.0));
            painter.drawLine(QPointF(viewport.left() + 9.0, viewport.top() + 2.0), QPointF(viewport.left() + 9.0, viewport.bottom() - 2.0));
            painter.drawLine(QPointF(viewport.right() - 9.0, viewport.top() + 2.0), QPointF(viewport.right() - 9.0, viewport.bottom() - 2.0));
            painter.drawLine(QPointF(viewport.right() - 5.0, viewport.top() + 2.0), QPointF(viewport.right() - 5.0, viewport.bottom() - 2.0));
        }

        painter.setPen(QPen(fill, 1.2));
        const int now_x = static_cast<int>(track.right()) + 5;
        painter.drawLine(now_x, static_cast<int>(track.center().y()) - 7, now_x, static_cast<int>(track.center().y()) + 7);
    }

private:
    enum class DragMode {
        None,
        Move,
        ResizeLeft,
        ResizeRight,
    };

    void updateHoverCursor(const QPoint& pos)
    {
        const QRectF viewport = viewportRect();
        if (!viewport.isValid()) {
            unsetCursor();
            return;
        }

        constexpr qreal edge_hit_width = 30.0;
        if (std::abs(pos.x() - viewport.left()) <= edge_hit_width ||
            std::abs(pos.x() - viewport.right()) <= edge_hit_width) {
            setCursor(Qt::SizeHorCursor);
        } else if (viewport.contains(pos)) {
            setCursor(m_drag_mode == DragMode::Move ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
        } else {
            unsetCursor();
        }
    }

    QRectF trackRect() const
    {
        QStyleOptionSlider option;
        const_cast<TrafficWindowSlider*>(this)->initStyleOption(&option);
        const QRect groove = style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderGroove, this);
        const QRect handle = style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
        return groove.adjusted(handle.width() / 2, -6, -handle.width() / 2, 6);
    }

    QRectF viewportRect() const
    {
        const QRectF track = trackRect();
        if (track.width() <= 1.0) return QRectF();
        const double fraction = std::max(0.01, std::min(1.0, static_cast<double>(m_window_percent) / 100.0));
        const double viewport_width = std::max(2.0, track.width() * fraction);
        const double denominator = std::max(1, maximum() - minimum());
        const double t = static_cast<double>(value() - minimum()) / denominator;
        const double left = track.left() + (track.width() - viewport_width) * t;
        return QRectF(left, track.center().y() - 9.0, viewport_width, 18.0);
    }

    double viewportCenterRatio() const
    {
        if (m_window_percent >= 100 && m_have_center_anchor) {
            return m_center_anchor_ratio;
        }
        const QRectF track = trackRect();
        if (track.width() <= 1.0) {
            return m_have_center_anchor ? m_center_anchor_ratio : 1.0;
        }
        const QRectF viewport = viewportRect();
        if (!viewport.isValid()) {
            return m_have_center_anchor ? m_center_anchor_ratio : 1.0;
        }
        return centerRatioForViewport(track, viewport.left(), viewport.right());
    }

    double centerRatioForViewport(const QRectF& track, qreal left, qreal right) const
    {
        if (track.width() <= 1.0) return m_have_center_anchor ? m_center_anchor_ratio : 1.0;
        const double center = (static_cast<double>(left + right) / 2.0 - track.left()) / track.width();
        return std::max(0.0, std::min(1.0, center));
    }

    void rememberCenterRatio(double ratio)
    {
        m_center_anchor_ratio = std::max(0.0, std::min(1.0, ratio));
        m_have_center_anchor = true;
    }

    void setPanFromViewportLeft(const QRectF& track, qreal left, qreal viewport_width)
    {
        const qreal travel = std::max<qreal>(1.0, track.width() - viewport_width);
        const int new_value = minimum() + static_cast<int>(std::llround((left - track.left()) / travel * (maximum() - minimum())));
        setValue(std::max(minimum(), std::min(maximum(), new_value)));
    }

    void setPanFromCenterRatio(double center_ratio)
    {
        const QRectF track = trackRect();
        if (track.width() <= 1.0) return;
        const double fraction = std::max(0.01, std::min(1.0, static_cast<double>(m_window_percent) / 100.0));
        const qreal viewport_width = std::max<qreal>(2.0, track.width() * fraction);
        if (viewport_width >= track.width() - 1.0) {
            setValue(maximum());
            return;
        }
        const qreal center_x = track.left() + track.width() * std::max(0.0, std::min(1.0, center_ratio));
        const qreal left = std::max(track.left(), std::min(center_x - viewport_width / 2.0, track.right() - viewport_width));
        setPanFromViewportLeft(track, left, viewport_width);
    }

    void setWindowFromViewport(const QRectF& track, qreal left, qreal right)
    {
        const qreal width = std::max<qreal>(2.0, right - left);
        const int new_percent = std::max(1, std::min(100, static_cast<int>(std::llround(width / track.width() * 100.0))));
        if (new_percent != m_window_percent) {
            m_window_percent = new_percent;
        }
        setPanFromViewportLeft(track, left, width);
        rememberCenterRatio(centerRatioForViewport(track, left, right));
        if (m_window_percent_changed) m_window_percent_changed(m_window_percent);
        update();
    }

    int m_window_percent{100};
    double m_center_anchor_ratio{1.0};
    bool m_have_center_anchor{false};
    DragMode m_drag_mode{DragMode::None};
    qreal m_drag_offset{0.0};
    std::function<void(int)> m_window_percent_changed;
};

QModelIndexList selectedRowsInColumn(QTableView* table, const QModelIndex& anchor)
{
    QModelIndexList rows;
    if (!table || !table->model() || !anchor.isValid()) return rows;

    if (table->selectionModel()) {
        rows = table->selectionModel()->selectedRows(anchor.column());
        if (rows.isEmpty()) {
            QSet<int> seen_rows;
            for (const QModelIndex& selected : table->selectionModel()->selectedIndexes()) {
                if (!selected.isValid() || seen_rows.contains(selected.row())) continue;
                rows << table->model()->index(selected.row(), anchor.column());
                seen_rows.insert(selected.row());
            }
        }
    }
    if (rows.isEmpty()) rows << anchor;

    std::sort(rows.begin(), rows.end(), [](const QModelIndex& left, const QModelIndex& right) {
        return left.row() < right.row();
    });
    return rows;
}

QString selectedColumnText(QTableView* table, const QModelIndex& anchor)
{
    QStringList values;
    for (const QModelIndex& row_index : selectedRowsInColumn(table, anchor)) {
        values << row_index.data(Qt::DisplayRole).toString().replace(QLatin1Char('\n'), QStringLiteral(", "));
    }
    return values.join(QLatin1Char('\n'));
}

class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(QWidget* parent = nullptr, int margin = 0, int hSpacing = 8, int vSpacing = 4)
        : QLayout(parent), m_hspace(hSpacing), m_vspace(vSpacing)
    {
        setContentsMargins(margin, margin, margin, margin);
    }

    ~FlowLayout() override
    {
        QLayoutItem* item;
        while ((item = takeAt(0)) != nullptr) delete item;
    }

    void addItem(QLayoutItem* item) override { m_items.append(item); }
    int count() const override { return m_items.size(); }
    QLayoutItem* itemAt(int index) const override { return m_items.value(index); }
    QLayoutItem* takeAt(int index) override
    {
        if (index < 0 || index >= m_items.size()) return nullptr;
        return m_items.takeAt(index);
    }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return doLayout(QRect(0, 0, width, 0), true); }
    QSize sizeHint() const override
    {
        QSize size;
        int width = 0;
        int height = 0;
        for (QLayoutItem* item : m_items) {
            if (!item || (item->widget() && !item->widget()->isVisible())) continue;
            const QSize hint = item->sizeHint();
            if (width > 0) width += m_hspace;
            width += hint.width();
            height = std::max(height, hint.height());
        }
        const QMargins margins = contentsMargins();
        size = QSize(width + margins.left() + margins.right(), height + margins.top() + margins.bottom());
        return size.expandedTo(minimumSize());
    }
    QSize minimumSize() const override
    {
        QSize size;
        for (QLayoutItem* item : m_items) {
            size = size.expandedTo(item->minimumSize());
        }
        const QMargins margins = contentsMargins();
        size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
        return size;
    }
    void setGeometry(const QRect& rect) override
    {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }

private:
    int doLayout(const QRect& rect, bool testOnly) const
    {
        const QMargins margins = contentsMargins();
        const QRect effective = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
        int x = effective.x();
        int y = effective.y();
        int lineHeight = 0;

        for (QLayoutItem* item : m_items) {
            if (!item || (item->widget() && !item->widget()->isVisible())) continue;
            const QSize hint = item->sizeHint();
            const int nextX = x + hint.width() + m_hspace;
            if (nextX - m_hspace > effective.right() && lineHeight > 0) {
                x = effective.x();
                y += lineHeight + m_vspace;
                lineHeight = 0;
            }
            if (!testOnly) item->setGeometry(QRect(QPoint(x, y), hint));
            x += hint.width() + m_hspace;
            lineHeight = std::max(lineHeight, hint.height());
        }
        return y + lineHeight - rect.y() + margins.bottom();
    }

    QList<QLayoutItem*> m_items;
    int m_hspace{8};
    int m_vspace{4};
};

int layoutItemHeightForWidth(QLayoutItem* item, int width);

int layoutHeightForWidth(QLayout* layout, int width)
{
    if (!layout) return 0;
    width = std::max(1, width);

    const QMargins margins = layout->contentsMargins();
    const int inner_width = std::max(1, width - margins.left() - margins.right());

    if (QBoxLayout* box = qobject_cast<QBoxLayout*>(layout)) {
        int visible_items = 0;
        int total_stretch = 0;
        int fixed_width = 0;
        for (int i = 0; i < box->count(); ++i) {
            QLayoutItem* item = box->itemAt(i);
            if (!item) continue;
            QWidget* widget = item->widget();
            if (widget && !widget->isVisible()) continue;
            ++visible_items;
            total_stretch += std::max(0, box->stretch(i));
            if (box->direction() == QBoxLayout::LeftToRight || box->direction() == QBoxLayout::RightToLeft) {
                if (box->stretch(i) == 0) fixed_width += item->sizeHint().width();
            }
        }

        const int spacing_total = std::max(0, visible_items - 1) * std::max(0, box->spacing());
        if (box->direction() == QBoxLayout::TopToBottom || box->direction() == QBoxLayout::BottomToTop) {
            int height = margins.top() + margins.bottom() + spacing_total;
            for (int i = 0; i < box->count(); ++i) {
                QLayoutItem* item = box->itemAt(i);
                if (!item) continue;
                QWidget* widget = item->widget();
                if (widget && !widget->isVisible()) continue;
                height += layoutItemHeightForWidth(item, inner_width);
            }
            return height;
        }

        int max_height = 0;
        const int stretch_width = std::max(1, inner_width - fixed_width - spacing_total);
        for (int i = 0; i < box->count(); ++i) {
            QLayoutItem* item = box->itemAt(i);
            if (!item) continue;
            QWidget* widget = item->widget();
            if (widget && !widget->isVisible()) continue;
            int item_width = item->sizeHint().width();
            if (total_stretch > 0 && box->stretch(i) > 0) {
                item_width = std::max(item->minimumSize().width(), stretch_width * box->stretch(i) / total_stretch);
            } else if (total_stretch == 0 && visible_items > 0) {
                item_width = std::max(item->minimumSize().width(), (inner_width - spacing_total) / visible_items);
            }
            max_height = std::max(max_height, layoutItemHeightForWidth(item, item_width));
        }
        return max_height + margins.top() + margins.bottom();
    }

    if (layout->hasHeightForWidth()) {
        return layout->heightForWidth(width);
    }

    return layout->sizeHint().height();
}

int layoutItemHeightForWidth(QLayoutItem* item, int width)
{
    if (!item) return 0;
    width = std::max(1, width);
    if (QLayout* child_layout = item->layout()) {
        return layoutHeightForWidth(child_layout, width);
    }
    if (QWidget* widget = item->widget()) {
        if (!widget->isVisible()) return 0;
        if (QLayout* widget_layout = widget->layout()) {
            const QMargins margins = widget->contentsMargins();
            return layoutHeightForWidth(widget_layout, std::max(1, width - margins.left() - margins.right())) + margins.top() + margins.bottom();
        }
    }
    if (item->hasHeightForWidth()) {
        return item->heightForWidth(width);
    }
    return item->sizeHint().height();
}

int banColumnForPeerColumn(int peer_column)
{
    switch (peer_column) {
    case PeerTableModel::NetNodeId: return BanTableModel::NetNodeId;
    case PeerTableModel::Address: return BanTableModel::Address;
    case PeerTableModel::Port: return BanTableModel::Port;
    case PeerTableModel::Fqdn: return BanTableModel::Fqdn;
    case PeerTableModel::CustomHostname: return BanTableModel::CustomHostname;
    case PeerTableModel::Version: return BanTableModel::Version;
    case PeerTableModel::Services: return BanTableModel::Services;
    case PeerTableModel::AvgPing: return BanTableModel::AvgPing;
    case PeerTableModel::Ping: return BanTableModel::Ping;
    case PeerTableModel::Jitter: return BanTableModel::Jitter;
    case PeerTableModel::PingHealth: return BanTableModel::PingHealth;
    case PeerTableModel::Sent: return BanTableModel::Sent;
    case PeerTableModel::Received: return BanTableModel::Received;
    case PeerTableModel::Subversion: return BanTableModel::Subversion;
    case PeerTableModel::UserAgentCount: return BanTableModel::UserAgentCount;
    case PeerTableModel::Country: return BanTableModel::Country;
    case PeerTableModel::CityState: return BanTableModel::CityState;
    case PeerTableModel::Permissions: return BanTableModel::Permissions;
    case PeerTableModel::Direction: return BanTableModel::Direction;
    case PeerTableModel::StartingBlock: return BanTableModel::StartingBlock;
    case PeerTableModel::SyncedHeaders: return BanTableModel::SyncedHeaders;
    case PeerTableModel::SyncedBlocks: return BanTableModel::SyncedBlocks;
    case PeerTableModel::ConnectionTime: return BanTableModel::ConnectionTime;
    case PeerTableModel::LastSend: return BanTableModel::LastSend;
    case PeerTableModel::LastReceive: return BanTableModel::LastReceive;
    case PeerTableModel::PingWait: return BanTableModel::PingWait;
    case PeerTableModel::MinPing: return BanTableModel::MinPing;
    case PeerTableModel::TimeOffset: return BanTableModel::TimeOffset;
    case PeerTableModel::MappedAS: return BanTableModel::MappedAS;
    case PeerTableModel::ASName: return BanTableModel::ASName;
    case PeerTableModel::ASHostingCompany: return BanTableModel::ASHostingCompany;
    case PeerTableModel::Seed: return BanTableModel::Seed;
    default: return -1;
    }
}

bool isPrivateIPv4(quint32 ip)
{
    return (ip & 0xff000000) == 0x0a000000 ||      // 10.0.0.0/8
           (ip & 0xfff00000) == 0xac100000 ||      // 172.16.0.0/12
           (ip & 0xffff0000) == 0xc0a80000;        // 192.168.0.0/16
}

QString ipv4String(quint32 ip)
{
    return QStringLiteral("%1.%2.%3.%4")
        .arg((ip >> 24) & 0xff)
        .arg((ip >> 16) & 0xff)
        .arg((ip >> 8) & 0xff)
        .arg(ip & 0xff);
}

QSet<QString> localIPv4Strings()
{
    QSet<QString> addresses;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        const QNetworkInterface::InterfaceFlags flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning) || (flags & QNetworkInterface::IsLoopBack)) continue;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            const QHostAddress address = entry.ip();
            if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                addresses.insert(address.toString());
            }
        }
    }
    addresses.insert(QStringLiteral("127.0.0.1"));
    return addresses;
}

QVector<quint16> localDiscoveryPorts()
{
    QVector<quint16> ports;
    ports << Params().GetDefaultPort() << 10332 << 9333;
    std::sort(ports.begin(), ports.end());
    ports.erase(std::unique(ports.begin(), ports.end()), ports.end());
    return ports;
}

QListWidgetItem* makePeerColumnItem(PeerTableModel* model, QHeaderView* header, int column, int default_width)
{
    QListWidgetItem* item = new QListWidgetItem(model->columnTitle(column));
    item->setData(Qt::UserRole, column);
    item->setData(Qt::UserRole + 1, header->isSectionHidden(column) ? default_width : header->sectionSize(column));
    item->setCheckState(header->isSectionHidden(column) ? Qt::Unchecked : Qt::Checked);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable);
    item->setToolTip(QObject::tr("Default width: %1 px").arg(default_width));
    return item;
}

QLabel* makePanelHeading(const QString& text)
{
    QLabel* label = new QLabel(text);
    QFont heading_font = label->font();
    heading_font.setBold(true);
    label->setFont(heading_font);
    label->setTextFormat(Qt::PlainText);
    return label;
}

QFrame* makeMetricSwatch(QWidget* parent)
{
    QFrame* swatch = new QFrame(parent);
    swatch->setFixedSize(44, 6);
    swatch->setFrameShape(QFrame::NoFrame);
    swatch->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return swatch;
}

void setMetricSwatchColor(QWidget* swatch, const QColor& color)
{
    if (!swatch) return;
    swatch->setStyleSheet(QStringLiteral("QFrame { background-color: %1; border: 0; border-radius: 2px; }").arg(color.name()));
}

int fontMetricHorizontalAdvance(const QFontMetrics& metrics, const QString& text)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    return metrics.horizontalAdvance(text);
#else
    return metrics.width(text);
#endif
}

void setPlainTextEditTabStop(QPlainTextEdit* edit, const QFontMetrics& metrics, const QString& text)
{
    if (!edit) return;
    const int width = fontMetricHorizontalAdvance(metrics, text);
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    edit->setTabStopDistance(width);
#else
    edit->setTabStopWidth(width);
#endif
}

void resizeTextControlWidget(QWidget* controls, const QFont& font)
{
    if (!controls) return;

    QFont button_font = font;
    button_font.setPointSize(std::max(10, font.pointSize() + 1));
    button_font.setBold(true);
    const QFontMetrics metrics(button_font);
    const int button_height = std::max(32, metrics.height() + 12);
    const int button_width = std::max(36, fontMetricHorizontalAdvance(metrics, QStringLiteral("A+")) + 18);
    const int pair_height = button_height + 8;
    const bool controls_are_pair = controls->objectName() == QStringLiteral("nodeTextControlPair");
    const int controls_height = controls_are_pair ? pair_height : pair_height + 10;

    controls->setMinimumHeight(controls_height);
    controls->setMaximumHeight(controls_height);
    controls->setSizePolicy(controls->sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);
    const QList<QWidget*> pairs = controls->findChildren<QWidget*>(QStringLiteral("nodeTextControlPair"));
    for (QWidget* pair : pairs) {
        pair->setMinimumHeight(pair_height);
        pair->setMaximumHeight(pair_height);
        pair->setSizePolicy(pair->sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);
    }
    const QList<QPushButton*> buttons = controls->findChildren<QPushButton*>();
    for (QPushButton* button : buttons) {
        if (!button) continue;
        button->setFont(button_font);
        button->setFixedSize(button_width, button_height);
        button->updateGeometry();
    }
    controls->updateGeometry();
}

class TrafficHealthDelegate final : public QStyledItemDelegate
{
public:
    explicit TrafficHealthDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (index.column() != PeerTableModel::PingHealth) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        painter->save();
        const QRect r = option.rect.adjusted(4, 2, -4, -2);
        painter->fillRect(option.rect, option.palette.base());

        const QVariantList raw_samples = index.data(Qt::UserRole).toList();
        QVector<qint64> samples;
        samples.reserve(raw_samples.size());
        for (const QVariant& value : raw_samples) {
            const qint64 sample = value.toLongLong();
            if (sample > 0) samples.push_back(sample);
        }
        if (samples.isEmpty() || r.width() <= 4 || r.height() <= 4) {
            painter->restore();
            return;
        }

        qint64 low = samples.constFirst();
        qint64 high = samples.constFirst();
        for (const qint64 sample : samples) {
            low = std::min(low, sample);
            high = std::max(high, sample);
        }
        const qint64 span = high - low;
        const qreal step = samples.size() > 1 ? static_cast<qreal>(r.width()) / static_cast<qreal>(samples.size() - 1) : 0.0;
        const qint64 animation_tick = QDateTime::currentMSecsSinceEpoch() / 150;
        const int live_phase = static_cast<int>((animation_tick + index.row()) % 4);
        const int traveling_spark = samples.isEmpty() ? -1 : static_cast<int>((animation_tick + index.row() * 5) % samples.size());

        auto sample_color = [](qint64 ms) {
            if (ms < 50) return QColor(49, 209, 88);
            if (ms < 150) return QColor(255, 159, 10);
            return QColor(255, 69, 58);
        };

        painter->setRenderHint(QPainter::Antialiasing, true);
        QColor grid = option.palette.mid().color();
        grid.setAlpha(70);
        painter->setPen(QPen(grid, 1.0));
        painter->drawLine(QPointF(r.left(), r.center().y()), QPointF(r.right(), r.center().y()));

        QPainterPath heartbeat;
        for (int i = 0; i < samples.size(); ++i) {
            const qreal x = r.left() + i * step;
            const qreal normalized = span > 0 ? static_cast<qreal>(samples.at(i) - low) / static_cast<qreal>(span) : 0.5;
            const qreal y = r.bottom() - normalized * r.height();
            if (i == 0) {
                heartbeat.moveTo(x, y);
            } else {
                const qreal prev_x = r.left() + (i - 1) * step;
                const qreal mid_x = (prev_x + x) / 2.0;
                heartbeat.cubicTo(mid_x, heartbeat.currentPosition().y(), mid_x, y, x, y);
            }

            const QColor color = sample_color(samples.at(i));
            const bool last_sample = i == samples.size() - 1;
            const int sample_phase = static_cast<int>((animation_tick + i + index.row()) % 4);
            QColor tick_color = color;
            if (i == traveling_spark) {
                tick_color = tick_color.lighter(145);
            } else if (sample_phase == 3) {
                tick_color.setAlpha(150);
            }
            const qreal active_width = (i == traveling_spark || (last_sample && live_phase == 0)) ? 2.8 : (sample_phase == 1 ? 1.8 : 1.2);
            QPen tick_pen(tick_color, active_width);
            tick_pen.setCapStyle(Qt::RoundCap);
            painter->setPen(tick_pen);
            const qreal request_height = std::max<qreal>(5.0, r.height() * (sample_phase == 0 ? 0.76 : 0.58));
            const qreal response_height = std::max<qreal>(4.0, r.height() * (sample_phase == 2 ? 0.56 : 0.36));
            painter->drawLine(QPointF(x, y - request_height / 2.0), QPointF(x, y + request_height / 2.0));
            if (step > 5.0) {
                const qreal response_x = std::min<qreal>(r.right(), x + std::max<qreal>(2.0, step * 0.18));
                QColor response_color = color;
                if (i == traveling_spark || sample_phase == 2) response_color = response_color.lighter(135);
                else response_color.setAlpha(155);
                painter->setPen(QPen(response_color, (i == traveling_spark || sample_phase == 2) ? 2.4 : 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter->drawLine(QPointF(response_x, y - response_height / 2.0), QPointF(response_x, y + response_height / 2.0));
                QPainterPath hop;
                hop.moveTo(x, y);
                hop.quadTo((x + response_x) / 2.0, y - request_height * 0.34, response_x, y);
                if (i == traveling_spark || sample_phase == 1) {
                    painter->setPen(QPen(color, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                }
                painter->drawPath(hop);
            }
        }

        QPen path_pen(sample_color(samples.constLast()), 1.8);
        path_pen.setCapStyle(Qt::RoundCap);
        path_pen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(path_pen);
        painter->drawPath(heartbeat);
        painter->setBrush(sample_color(samples.constLast()));
        painter->setPen(Qt::NoPen);
        const qreal last_x = r.left() + (samples.size() - 1) * step;
        const qreal last_normalized = span > 0 ? static_cast<qreal>(samples.constLast() - low) / static_cast<qreal>(span) : 0.5;
        const qreal last_y = r.bottom() - last_normalized * r.height();
        painter->drawEllipse(QPointF(last_x, last_y), live_phase == 3 ? 2.0 : 3.4, live_phase == 3 ? 2.0 : 3.4);
        painter->restore();
    }
};

class GeoIconDelegate final : public QStyledItemDelegate
{
public:
    explicit GeoIconDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (index.column() != PeerTableModel::Country) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        const QString marker = index.data(Qt::UserRole).toString();
        if (marker != QLatin1String("local") && marker != QLatin1String("lan")) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        painter->save();
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        opt.text.clear();
        opt.icon = QIcon();
        QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

        painter->setRenderHint(QPainter::Antialiasing, true);
        const int side = std::max(16, std::min(option.rect.width() - 8, option.rect.height() - 5));
        QRectF box(0, 0, side, side);
        box.moveCenter(option.rect.center());
        box.adjust(0.5, 0.5, -0.5, -0.5);

        QColor base = marker == QLatin1String("local") ? QColor(49, 209, 88) : QColor(52, 199, 255);
        QColor stroke = option.palette.text().color();
        QColor fill = base;
        fill.setAlpha(option.state & QStyle::State_Selected ? 235 : 180);
        stroke.setAlpha(option.state & QStyle::State_Selected ? 235 : 190);

        painter->setPen(QPen(stroke, 1.1));
        painter->setBrush(fill);
        painter->drawRoundedRect(box, 3.0, 3.0);

        QColor glyph = option.palette.base().color();
        if (glyph.lightness() < 100) {
            glyph = QColor(12, 24, 32);
        }
        glyph.setAlpha(225);
        painter->setPen(QPen(glyph, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);

        if (marker == QLatin1String("local")) {
            QPainterPath roof;
            roof.moveTo(box.left() + side * 0.22, box.top() + side * 0.52);
            roof.lineTo(box.center().x(), box.top() + side * 0.27);
            roof.lineTo(box.right() - side * 0.22, box.top() + side * 0.52);
            painter->drawPath(roof);
            QRectF house(box.left() + side * 0.31, box.top() + side * 0.50, side * 0.38, side * 0.28);
            painter->drawRect(house);
            painter->drawLine(QPointF(box.center().x(), house.bottom()), QPointF(box.center().x(), house.top() + side * 0.15));
        } else {
            const QPointF top(box.center().x(), box.top() + side * 0.30);
            const QPointF left(box.left() + side * 0.30, box.bottom() - side * 0.30);
            const QPointF right(box.right() - side * 0.30, box.bottom() - side * 0.30);
            painter->drawLine(top, left);
            painter->drawLine(top, right);
            painter->drawLine(left, right);

            painter->setBrush(glyph);
            painter->setPen(Qt::NoPen);
            const qreal node_radius = std::max<qreal>(1.9, side * 0.095);
            painter->drawEllipse(top, node_radius, node_radius);
            painter->drawEllipse(left, node_radius, node_radius);
            painter->drawEllipse(right, node_radius, node_radius);

            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(glyph, 1.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            QRectF switch_box(box.left() + side * 0.31, box.top() + side * 0.17, side * 0.38, side * 0.14);
            painter->drawRoundedRect(switch_box, 1.5, 1.5);
        }

        painter->restore();
    }
};

class ByteAmountDelegate final : public QStyledItemDelegate
{
public:
    explicit ByteAmountDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const QString text = index.data(Qt::DisplayRole).toString().trimmed();
        QRegExp byte_value(QStringLiteral("^([0-9]+(?:\\.[0-9]+)?)\\s*([A-Za-z]+)$"));
        if (byte_value.indexIn(text) != 0) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        opt.text.clear();
        QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

        const QString number = byte_value.cap(1);
        const QString unit = byte_value.cap(2);
        const QFontMetrics fm(option.font);
        const int unit_width = fontMetricHorizontalAdvance(fm, QStringLiteral("MB"));
        const int number_width = fontMetricHorizontalAdvance(fm, number);
        const QRect text_rect = option.rect.adjusted(4, 0, -6, 0);
        QRect unit_rect(text_rect.right() - unit_width + 1, text_rect.top(), unit_width, text_rect.height());
        QRect number_rect(unit_rect.left() - number_width - fontMetricHorizontalAdvance(fm, QStringLiteral(" ")), text_rect.top(), number_width, text_rect.height());

        painter->save();
        painter->setFont(option.font);
        painter->setPen(opt.palette.color((option.state & QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text));
        painter->drawText(number_rect, Qt::AlignRight | Qt::AlignVCenter, number);
        painter->drawText(unit_rect, Qt::AlignRight | Qt::AlignVCenter, unit);
        painter->restore();
    }
};

class UserAgentPieWidget final : public QWidget
{
public:
    explicit UserAgentPieWidget(QWidget* parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(340, 142);
        setMaximumHeight(166);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    }

    void setCounts(const QVector<QPair<QString, int>>& counts, int total)
    {
        m_counts = counts;
        m_total = total;
        updateGeometry();
        update();
    }

    QSize sizeHint() const override
    {
        return QSize(560, 154);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QColor text_color = palette().color(QPalette::WindowText);
        const QRect content = rect().adjusted(6, 0, -6, -2);
        if (m_counts.isEmpty() || m_total <= 0) {
            painter.setPen(text_color);
            painter.drawText(content, Qt::AlignHCenter | Qt::AlignVCenter, tr("Active node user agents: none"));
            return;
        }

        const QVector<QColor> colors{
            QColor("#00FA9A"), QColor("#1E90FF"), QColor("#FFD700"), QColor("#FF8C00"),
            QColor("#C71585"), QColor("#9400D3"), QColor("#7CFC00"), QColor("#808080")
        };
        const QFontMetrics fm(font());
        const int title_height = fm.height() + 2;
        const int legend_rows = std::min(m_counts.size(), 7);
        const int legend_height = legend_rows * (fm.height() + 1) + 2;
        const int diameter = std::max(68, std::min(content.width() - 24, content.height() - title_height - legend_height - 2));
        const QRect pie_rect(content.left() + (content.width() - diameter) / 2, content.top() + title_height + 1, diameter, diameter);

        painter.setPen(text_color);
        painter.drawText(QRect(content.left(), content.top(), content.width(), title_height), Qt::AlignHCenter | Qt::AlignVCenter, tr("Active Node User Agents"));

        int start_angle = 90 * 16;
        for (int i = 0; i < m_counts.size(); ++i) {
            const int span_angle = -static_cast<int>(std::round(360.0 * 16.0 * m_counts.at(i).second / m_total));
            painter.setBrush(colors.at(i % colors.size()));
            painter.setPen(QPen(palette().color(QPalette::Window), 1));
            painter.drawPie(pie_rect, start_angle, span_angle);
            start_angle += span_angle;
        }

        int legend_width = 0;
        for (int i = 0; i < m_counts.size() && i < 7; ++i) {
            const double percent = 100.0 * m_counts.at(i).second / m_total;
            const QString agent = fm.elidedText(m_counts.at(i).first, Qt::ElideMiddle, std::max(80, content.width() - 96));
            const QString label = tr("%1  %2 (%3%)").arg(agent).arg(m_counts.at(i).second).arg(QString::number(percent, 'f', 0));
            legend_width = std::max(legend_width, 16 + fontMetricHorizontalAdvance(fm, label));
        }
        const int legend_x = content.left() + std::max(0, (content.width() - legend_width) / 2);
        int y = pie_rect.bottom() + fm.ascent() + 4;
        for (int i = 0; i < m_counts.size() && i < 7 && y + fm.descent() <= content.bottom(); ++i) {
            const QColor swatch = colors.at(i % colors.size());
            painter.setBrush(swatch);
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(QRect(legend_x, y - fm.ascent() + 2, 10, 10), 2, 2);
            painter.setPen(text_color);
            const double percent = 100.0 * m_counts.at(i).second / m_total;
            const QString agent = fm.elidedText(m_counts.at(i).first, Qt::ElideMiddle, std::max(80, content.width() - 96));
            painter.drawText(legend_x + 16, y, tr("%1  %2 (%3%)").arg(agent).arg(m_counts.at(i).second).arg(QString::number(percent, 'f', 0)));
            y += fm.height() + 1;
        }
    }

private:
    QVector<QPair<QString, int>> m_counts;
    int m_total{0};
};

#if ENABLE_DEFCOIN_FUN_UI
namespace {
struct PeerMapNode
{
    QString ip;
    QString city_state;
    QString map_label;
    QString user_agent;
    QString website;
    double latitude{0.0};
    double longitude{0.0};
    bool has_coordinates{false};
    bool is_local{false};
    bool is_lan{false};
    quint64 sent{0};
    quint64 received{0};
};

struct PeerMapTrafficPulse
{
    quint64 sent_delta{0};
    quint64 received_delta{0};
    qint64 timestamp_msecs{0};
};

enum class PeerMapDataSource {
    RealNodes = 0,
    DefconMeetups = 1,
    DemoLocations = 2,
};

constexpr double GLOBE_PI = 3.14159265358979323846;

double globeDegToRad(double degrees)
{
    return degrees * GLOBE_PI / 180.0;
}

double globeRadToDeg(double radians)
{
    return radians * 180.0 / GLOBE_PI;
}

double normalizeLongitude(double longitude)
{
    while (longitude < -180.0) longitude += 360.0;
    while (longitude > 180.0) longitude -= 360.0;
    return longitude;
}

double boundedUnit(double value)
{
    return std::max(-1.0, std::min(1.0, value));
}

double globeDistance(double lat_a, double lon_a, double lat_b, double lon_b)
{
    const double phi_a = globeDegToRad(lat_a);
    const double phi_b = globeDegToRad(lat_b);
    const double delta_phi = globeDegToRad(lat_b - lat_a);
    const double delta_lambda = globeDegToRad(lon_b - lon_a);
    const double h = std::sin(delta_phi / 2.0) * std::sin(delta_phi / 2.0) +
                     std::cos(phi_a) * std::cos(phi_b) *
                     std::sin(delta_lambda / 2.0) * std::sin(delta_lambda / 2.0);
    return 2.0 * std::atan2(std::sqrt(h), std::sqrt(std::max(0.0, 1.0 - h)));
}

double clampDouble(double value, double low, double high)
{
    return std::max(low, std::min(high, value));
}

double smoothStep(double value)
{
    const double t = clampDouble(value, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

double interpolateLongitude(double from, double to, double progress)
{
    return normalizeLongitude(from + normalizeLongitude(to - from) * progress);
}

QColor blendColor(const QColor& a, const QColor& b, double progress)
{
    const double t = clampDouble(progress, 0.0, 1.0);
    return QColor(
        static_cast<int>(a.red() + (b.red() - a.red()) * t),
        static_cast<int>(a.green() + (b.green() - a.green()) * t),
        static_cast<int>(a.blue() + (b.blue() - a.blue()) * t),
        static_cast<int>(a.alpha() + (b.alpha() - a.alpha()) * t));
}

class PeerGlobeCanvas final : public QWidget
{
public:
    struct ViewState {
        double center_latitude{20.0};
        double center_longitude{-95.0};
        double camera_zoom{1.0};
        double zoom_in{5.00};
        double zoom_out{1.55};
        double spin_velocity_latitude{0.0};
        double spin_velocity_longitude{0.0};
        int route_index{0};
        int route_elapsed_ms{0};
        double phase{0.0};
    };

    explicit PeerGlobeCanvas(QWidget* parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(420, 320);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        setCursor(Qt::OpenHandCursor);
        QSettings settings;
        m_zoom_in_default = settings.value(QStringLiteral("PeerMapZoomedInMax"), 5.00).toDouble();
        m_zoom_out_default = settings.value(QStringLiteral("PeerMapZoomedOutMax"), 1.55).toDouble();
        m_zoom_in = clampDouble(m_zoom_in_default, 1.15, 5.0);
        m_zoom_out = clampDouble(m_zoom_out_default, 0.72, 2.2);
        m_animation_timer.setInterval(TIMER_INTERVAL_MS);
        connect(&m_animation_timer, &QTimer::timeout, this, [this] {
            advanceCamera();
            update();
        });
    }

    void setNodes(const QVector<PeerMapNode>& nodes)
    {
        const bool had_route = !m_route.isEmpty();
        const int old_focus = focusedNodeIndex();
        const QString preferred_ip = old_focus >= 0 && old_focus < m_nodes.size() ? m_nodes.at(old_focus).ip : QString();
        const ViewState previous_state = viewState();

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        QSet<QString> active_ips;
        for (const PeerMapNode& node : nodes) {
            active_ips.insert(node.ip);
            const auto previous = m_last_traffic_totals.constFind(node.ip);
            if (previous != m_last_traffic_totals.constEnd()) {
                const quint64 sent_delta = node.sent >= previous->sent_delta ? node.sent - previous->sent_delta : 0;
                const quint64 received_delta = node.received >= previous->received_delta ? node.received - previous->received_delta : 0;
                if (sent_delta > 0 || received_delta > 0) {
                    PeerMapTrafficPulse pulse;
                    pulse.sent_delta = sent_delta;
                    pulse.received_delta = received_delta;
                    pulse.timestamp_msecs = now;
                    m_recent_traffic.insert(node.ip, pulse);
                }
            }
            PeerMapTrafficPulse total_snapshot;
            total_snapshot.sent_delta = node.sent;
            total_snapshot.received_delta = node.received;
            total_snapshot.timestamp_msecs = now;
            m_last_traffic_totals.insert(node.ip, total_snapshot);
        }
        for (auto it = m_last_traffic_totals.begin(); it != m_last_traffic_totals.end();) {
            if (!active_ips.contains(it.key())) {
                it = m_last_traffic_totals.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = m_recent_traffic.begin(); it != m_recent_traffic.end();) {
            if (!active_ips.contains(it.key()) || now - it->timestamp_msecs > TRAFFIC_PULSE_DECAY_MS) {
                it = m_recent_traffic.erase(it);
            } else {
                ++it;
            }
        }

        m_nodes = nodes;
        assignDisplayCoordinates();
        const QString signature = routeSignature();
        if (signature != m_route_signature) {
            m_route_signature = signature;
            rebuildRoute(!had_route, preferred_ip);
            if (had_route) {
                setViewState(previous_state);
            }
        }
        update();
    }

    ViewState viewState() const
    {
        ViewState state;
        state.center_latitude = m_center_latitude;
        state.center_longitude = m_center_longitude;
        state.camera_zoom = m_camera_zoom;
        state.zoom_in = m_zoom_in;
        state.zoom_out = m_zoom_out;
        state.spin_velocity_latitude = m_spin_velocity_latitude;
        state.spin_velocity_longitude = m_spin_velocity_longitude;
        state.route_index = m_route_index;
        state.route_elapsed_ms = m_route_elapsed_ms;
        state.phase = m_phase;
        return state;
    }

    void setViewState(const ViewState& state)
    {
        m_center_latitude = clampDouble(state.center_latitude, -82.0, 82.0);
        m_center_longitude = normalizeLongitude(state.center_longitude);
        m_camera_zoom = clampDouble(state.camera_zoom, 0.72, 5.0);
        m_zoom_in = clampDouble(state.zoom_in, 1.15, 5.0);
        m_zoom_out = clampDouble(state.zoom_out, 0.72, 2.2);
        m_spin_velocity_latitude = state.spin_velocity_latitude;
        m_spin_velocity_longitude = state.spin_velocity_longitude;
        m_route_index = m_route.isEmpty() ? 0 : std::max(0, std::min(state.route_index, m_route.size() - 1));
        m_route_elapsed_ms = std::max(0, std::min(state.route_elapsed_ms, NODE_HOLD_MS + NODE_TRANSITION_MS));
        m_phase = state.phase;
        update();
    }

    void adjustZoom(double factor)
    {
        adjustSessionZoom(factor);
    }

    void nudgeView(double latitude_delta, double longitude_delta)
    {
        m_center_latitude = clampDouble(m_center_latitude + latitude_delta, -82.0, 82.0);
        m_center_longitude = normalizeLongitude(m_center_longitude + longitude_delta);
        m_spin_velocity_latitude = latitude_delta * 0.025;
        m_spin_velocity_longitude = longitude_delta * 0.025;
        m_user_spin_pause_ms = 1800;
        update();
    }

    QVector<int> routeOrder() const
    {
        return m_route;
    }

    int focusedNodeIndex() const
    {
        if (m_route.isEmpty()) return -1;
        return m_route.value(m_route_index % m_route.size(), -1);
    }

    void focusNodeIndex(int node_index, double speed_multiplier = 2.5)
    {
        const int route_row = m_route.indexOf(node_index);
        if (route_row < 0) return;
        m_route_index = route_row;
        m_route_elapsed_ms = 0;
        m_speed_multiplier = clampDouble(speed_multiplier, 1.0, 3.5);
        m_speed_boost_ticks = static_cast<int>((NODE_HOLD_MS + NODE_TRANSITION_MS) / TIMER_INTERVAL_MS / m_speed_multiplier);
        if (m_focus_changed_callback) {
            m_focus_changed_callback(focusedNodeIndex());
        }
        setFocus(Qt::MouseFocusReason);
        update();
    }

    void setFocusChangedCallback(std::function<void(int)> callback)
    {
        m_focus_changed_callback = std::move(callback);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        updateProjectionBasis();

        const QRectF content = rect().adjusted(14, 12, -14, -12);
        const qreal base_diameter = std::max<qreal>(40.0, std::min(content.width(), content.height()));
        const qreal diameter = base_diameter * m_camera_zoom;
        const QRectF globe_rect(content.center().x() - diameter / 2.0,
                                content.center().y() - diameter / 2.0,
                                diameter, diameter);

        paintGlobe(painter, globe_rect);
        paintGlobeGrid(painter, globe_rect);
        paintCountryBorders(painter, globe_rect);
        paintRoutes(painter, globe_rect);
        paintNodes(painter, globe_rect);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(event);
            return;
        }
        setFocus(Qt::MouseFocusReason);
        m_dragging = true;
        m_last_drag_pos = event->pos();
        m_spin_velocity_latitude = 0.0;
        m_spin_velocity_longitude = 0.0;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (!m_dragging) {
            QWidget::mouseMoveEvent(event);
            return;
        }
        const QPoint delta = event->pos() - m_last_drag_pos;
        m_last_drag_pos = event->pos();
        const double lon_delta = -delta.x() * 0.16 / std::max(0.8, m_camera_zoom);
        const double lat_delta = delta.y() * 0.12 / std::max(0.8, m_camera_zoom);
        m_center_longitude = normalizeLongitude(m_center_longitude + lon_delta);
        m_center_latitude = clampDouble(m_center_latitude + lat_delta, -82.0, 82.0);
        m_spin_velocity_longitude = lon_delta;
        m_spin_velocity_latitude = lat_delta;
        m_user_spin_pause_ms = 2400;
        update();
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton || !m_dragging) {
            QWidget::mouseReleaseEvent(event);
            return;
        }
        m_dragging = false;
        m_user_spin_pause_ms = 3000;
        setCursor(Qt::OpenHandCursor);
        event->accept();
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
            adjustSessionZoom(1.08);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Minus || event->key() == Qt::Key_Underscore) {
            adjustSessionZoom(1.0 / 1.08);
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        const int delta = event->angleDelta().y();
        if (delta == 0) {
            QWidget::wheelEvent(event);
            return;
        }
        adjustSessionZoom(std::pow(1.0014, static_cast<double>(delta)));
        event->accept();
    }

    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
        updateProjectionBasis();
        if (!m_animation_timer.isActive()) {
            m_animation_timer.start();
        }
    }

    void hideEvent(QHideEvent* event) override
    {
        m_animation_timer.stop();
        QWidget::hideEvent(event);
    }

private:
    struct DisplayPoint {
        double latitude{0.0};
        double longitude{0.0};
    };

    struct GeoLine {
        QVector<QPointF> points;
    };

    static constexpr int TIMER_INTERVAL_MS = 33;
    static constexpr int NODE_HOLD_MS = 13500;
    static constexpr int NODE_TRANSITION_MS = 17000;
    static constexpr int TRAFFIC_PULSE_DECAY_MS = 4500;

    QString routeSignature() const
    {
        QStringList parts;
        parts.reserve(m_nodes.size());
        for (int i = 0; i < m_nodes.size(); ++i) {
            const PeerMapNode& node = m_nodes.at(i);
            const DisplayPoint point = m_display_points.value(i);
            parts << QStringLiteral("%1|%2|%3|%4|%5|%6")
                         .arg(node.ip)
                         .arg(node.is_local ? QLatin1String("L") : (node.is_lan ? QLatin1String("N") : QLatin1String("P")))
                         .arg(QString::number(point.latitude, 'f', 4))
                         .arg(QString::number(point.longitude, 'f', 4))
                         .arg(node.city_state)
                         .arg(node.map_label);
        }
        return parts.join(QString(QChar(0x1f)));
    }

    static void appendGeoLine(QVector<GeoLine>& lines, const QJsonArray& coordinates)
    {
        GeoLine line;
        line.points.reserve(coordinates.size());
        for (const QJsonValue& coordinate_value : coordinates) {
            const QJsonArray coordinate = coordinate_value.toArray();
            if (coordinate.size() < 2) continue;
            line.points.push_back(QPointF(coordinate.at(0).toDouble(), coordinate.at(1).toDouble()));
        }
        if (line.points.size() > 1) {
            lines.push_back(line);
        }
    }

    static QVector<GeoLine> loadCountryBorders()
    {
        QVector<GeoLine> lines;
        QFile file(QStringLiteral(":/geo/country_borders_110m"));
        if (!file.open(QIODevice::ReadOnly)) {
            return lines;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const QJsonArray features = doc.object().value(QStringLiteral("features")).toArray();
        for (const QJsonValue& feature_value : features) {
            const QJsonObject geometry = feature_value.toObject().value(QStringLiteral("geometry")).toObject();
            const QString type = geometry.value(QStringLiteral("type")).toString();
            const QJsonArray coordinates = geometry.value(QStringLiteral("coordinates")).toArray();
            if (type == QLatin1String("LineString")) {
                appendGeoLine(lines, coordinates);
            } else if (type == QLatin1String("MultiLineString")) {
                for (const QJsonValue& line_value : coordinates) {
                    appendGeoLine(lines, line_value.toArray());
                }
            }
        }
        return lines;
    }

    static const QVector<GeoLine>& countryBorders()
    {
        static const QVector<GeoLine> lines = loadCountryBorders();
        return lines;
    }

    void updateProjectionBasis() const
    {
        const double lat = globeDegToRad(m_center_latitude);
        const double lon = globeDegToRad(m_center_longitude);
        m_cached_view_center = surfaceVector(m_center_latitude, m_center_longitude).normalized();
        m_cached_view_east = QVector3D(-std::sin(lon), 0.0, std::cos(lon)).normalized();
        m_cached_view_north = QVector3D(-std::sin(lat) * std::cos(lon),
                                        std::cos(lat),
                                        -std::sin(lat) * std::sin(lon)).normalized();
        m_projection_basis_valid = true;
    }

    static const QImage& nightTexture()
    {
        static const QImage texture = QImage(QStringLiteral(":/geo/black_marble_2016_3km")).convertToFormat(QImage::Format_RGB32);
        if (!texture.isNull()) return texture;
        static const QImage fallback = QImage(QStringLiteral(":/geo/black_marble_2016_global_7km")).convertToFormat(QImage::Format_RGB32);
        return fallback;
    }

    static const QImage& dayTexture()
    {
        static const QImage texture = QImage(QStringLiteral(":/geo/blue_marble_200405_3x5400x2700")).convertToFormat(QImage::Format_RGB32);
        if (!texture.isNull()) return texture;
        static const QImage fallback = QImage(QStringLiteral(":/geo/blue_marble_200405_3x5400x2700_jpg")).convertToFormat(QImage::Format_RGB32);
        return fallback;
    }

    static QVector3D surfaceVector(double latitude, double longitude)
    {
        const double lat = globeDegToRad(latitude);
        const double lon = globeDegToRad(longitude);
        return QVector3D(std::cos(lat) * std::cos(lon),
                         std::sin(lat),
                         std::cos(lat) * std::sin(lon));
    }

    static DisplayPoint vectorToDisplayPoint(const QVector3D& vector)
    {
        const QVector3D normal = vector.normalized();
        DisplayPoint point;
        point.latitude = globeRadToDeg(std::asin(boundedUnit(normal.y())));
        point.longitude = normalizeLongitude(globeRadToDeg(std::atan2(normal.z(), normal.x())));
        return point;
    }

    void assignDisplayCoordinates()
    {
        m_display_points.clear();
        m_display_points.reserve(m_nodes.size());
        int lan_index = 0;
        for (const PeerMapNode& node : m_nodes) {
            DisplayPoint point;
            if (node.has_coordinates) {
                point.latitude = node.latitude;
                point.longitude = node.longitude;
            } else if (node.is_local) {
                point.latitude = 18.0;
                point.longitude = -100.0;
            } else if (node.is_lan) {
                point.latitude = 18.0 + (lan_index % 4) * 3.0;
                point.longitude = -94.0 + (lan_index / 4) * 5.0;
                ++lan_index;
            } else {
                const uint hash = qHash(node.ip);
                point.latitude = -55.0 + static_cast<double>(hash % 11000U) / 100.0;
                point.longitude = -180.0 + static_cast<double>((hash / 11000U) % 36000U) / 100.0;
            }
            point.latitude = std::max(-80.0, std::min(80.0, point.latitude));
            point.longitude = normalizeLongitude(point.longitude);
            m_display_points.push_back(point);
        }
    }

    void rebuildRoute(bool reset_camera = true, const QString& preferred_ip = QString())
    {
        m_route.clear();
        if (m_nodes.isEmpty()) return;

        QVector<int> remaining;
        remaining.reserve(m_nodes.size());
        for (int i = 0; i < m_nodes.size(); ++i) {
            remaining.push_back(i);
        }

        int current = 0;
        for (int i = 0; i < remaining.size(); ++i) {
            if (m_nodes.at(i).is_local) {
                current = i;
                break;
            }
        }
        m_route.push_back(current);
        remaining.removeOne(current);

        while (!remaining.isEmpty()) {
            int best_pos = 0;
            double best_distance = std::numeric_limits<double>::max();
            const DisplayPoint from = m_display_points.value(current);
            for (int pos = 0; pos < remaining.size(); ++pos) {
                const int candidate = remaining.at(pos);
                const DisplayPoint to = m_display_points.value(candidate);
                const double distance = globeDistance(from.latitude, from.longitude, to.latitude, to.longitude);
                if (distance < best_distance) {
                    best_distance = distance;
                    best_pos = pos;
                }
            }
            current = remaining.takeAt(best_pos);
            m_route.push_back(current);
        }
        int preferred_route = 0;
        if (!preferred_ip.isEmpty()) {
            for (int route_pos = 0; route_pos < m_route.size(); ++route_pos) {
                const int node_index = m_route.at(route_pos);
                if (node_index >= 0 && node_index < m_nodes.size() && m_nodes.at(node_index).ip == preferred_ip) {
                    preferred_route = route_pos;
                    break;
                }
            }
        }
        m_route_index = preferred_route;
        m_route_elapsed_ms = reset_camera ? 0 : std::min(m_route_elapsed_ms, NODE_HOLD_MS + NODE_TRANSITION_MS - TIMER_INTERVAL_MS);
        if (reset_camera && !m_route.isEmpty()) {
            const DisplayPoint first = m_display_points.value(m_route.first());
            m_center_latitude = first.latitude;
            m_center_longitude = first.longitude;
            m_camera_zoom = m_zoom_out;
        }
        if (m_focus_changed_callback) {
            m_focus_changed_callback(focusedNodeIndex());
        }
    }

    void adjustSessionZoom(double factor)
    {
        m_camera_zoom = clampDouble(m_camera_zoom * factor, 0.72, 5.0);
        const double midpoint = (m_zoom_in + m_zoom_out) / 2.0;
        if (m_camera_zoom >= midpoint || (m_route_elapsed_ms < NODE_HOLD_MS && !m_route.isEmpty())) {
            m_zoom_in = m_camera_zoom;
        } else {
            m_zoom_out = m_camera_zoom;
        }
        m_user_spin_pause_ms = 1600;
        update();
    }

    void advanceCamera()
    {
        if (m_dragging) {
            m_phase += 0.030;
            return;
        }
        if (std::abs(m_spin_velocity_longitude) > 0.002 || std::abs(m_spin_velocity_latitude) > 0.002) {
            m_center_longitude = normalizeLongitude(m_center_longitude + m_spin_velocity_longitude);
            m_center_latitude = clampDouble(m_center_latitude + m_spin_velocity_latitude, -82.0, 82.0);
            m_spin_velocity_longitude *= 0.94;
            m_spin_velocity_latitude *= 0.94;
        }
        if (m_user_spin_pause_ms > 0) {
            m_user_spin_pause_ms = std::max(0, m_user_spin_pause_ms - TIMER_INTERVAL_MS);
            m_phase += 0.030;
            if (m_phase > 1000.0) m_phase = 0.0;
            return;
        }

        if (m_route.isEmpty()) {
            m_center_longitude = normalizeLongitude(m_center_longitude + 0.08);
            m_camera_zoom = 1.0;
        } else if (m_route.size() == 1) {
            const DisplayPoint target = m_display_points.value(m_route.first());
            m_center_latitude += (target.latitude - m_center_latitude) * 0.06;
            m_center_longitude = interpolateLongitude(m_center_longitude, target.longitude, 0.06);
            m_camera_zoom += (m_zoom_in - m_camera_zoom) * 0.05;
        } else {
            const int current_route = m_route_index % m_route.size();
            const int next_route = (current_route + 1) % m_route.size();
            const DisplayPoint from = m_display_points.value(m_route.value(current_route));
            const DisplayPoint to = m_display_points.value(m_route.value(next_route));

            if (m_route_elapsed_ms < NODE_HOLD_MS) {
                m_center_latitude += (from.latitude - m_center_latitude) * 0.08;
                m_center_longitude = interpolateLongitude(m_center_longitude, from.longitude, 0.08);
                m_camera_zoom += (m_zoom_in - m_camera_zoom) * 0.06;
            } else {
                const double t = smoothStep(static_cast<double>(m_route_elapsed_ms - NODE_HOLD_MS) / NODE_TRANSITION_MS);
                const double center_t = t < 0.5
                    ? smoothStep(t * 2.0) * 0.35
                    : 0.35 + smoothStep((t - 0.5) * 2.0) * 0.65;
                const double spin_influence = (1.0 - t) * 8.0;
                const double target_latitude = clampDouble(to.latitude + m_spin_velocity_latitude * spin_influence, -82.0, 82.0);
                const double target_longitude = normalizeLongitude(to.longitude + m_spin_velocity_longitude * spin_influence);
                m_center_latitude = from.latitude + (target_latitude - from.latitude) * center_t;
                m_center_longitude = interpolateLongitude(from.longitude, target_longitude, center_t);
                m_camera_zoom = m_zoom_in - std::sin(GLOBE_PI * t) * std::max(0.05, m_zoom_in - m_zoom_out);
                m_spin_velocity_longitude *= 0.995;
                m_spin_velocity_latitude *= 0.995;
            }

            const double active_multiplier = (m_speed_boost_ticks > 0) ? m_speed_multiplier : 1.0;
            m_route_elapsed_ms += static_cast<int>(TIMER_INTERVAL_MS * active_multiplier);
            if (m_speed_boost_ticks > 0) {
                --m_speed_boost_ticks;
                if (m_speed_boost_ticks == 0) m_speed_multiplier = 1.0;
            }
            if (m_route_elapsed_ms > NODE_HOLD_MS + NODE_TRANSITION_MS) {
                m_route_index = next_route;
                m_route_elapsed_ms = 0;
                if (m_focus_changed_callback) {
                    m_focus_changed_callback(focusedNodeIndex());
                }
            }
        }

        m_phase += 0.030;
        if (m_phase > 1000.0) m_phase = 0.0;
    }

    QVector3D sunVector() const
    {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        const int day = now.date().dayOfYear();
        const QTime time = now.time();
        const double hour = time.hour() + time.minute() / 60.0 + time.second() / 3600.0;
        const double declination = 23.44 * std::sin(2.0 * GLOBE_PI * (static_cast<double>(day) - 81.0) / 365.0);
        const double subsolar_longitude = normalizeLongitude(180.0 - hour * 15.0);
        return surfaceVector(declination, subsolar_longitude);
    }

    void viewBasis(QVector3D& center, QVector3D& east, QVector3D& north) const
    {
        if (m_projection_basis_valid) {
            center = m_cached_view_center;
            east = m_cached_view_east;
            north = m_cached_view_north;
            return;
        }
        const double lat = globeDegToRad(m_center_latitude);
        const double lon = globeDegToRad(m_center_longitude);
        center = surfaceVector(m_center_latitude, m_center_longitude).normalized();
        east = QVector3D(-std::sin(lon), 0.0, std::cos(lon)).normalized();
        north = QVector3D(-std::sin(lat) * std::cos(lon),
                          std::cos(lat),
                          -std::sin(lat) * std::sin(lon)).normalized();
    }

    static QColor sampleTextureRgb(const QImage& texture, double latitude, double longitude, const QColor& fallback)
    {
        if (texture.isNull()) {
            return fallback;
        }
        const int map_height = std::min(texture.height(), texture.width() / 2);
        const int x = static_cast<int>(clampDouble((normalizeLongitude(longitude) + 180.0) / 360.0, 0.0, 0.999999) * texture.width());
        const int y = static_cast<int>(clampDouble((90.0 - latitude) / 180.0, 0.0, 0.999999) * map_height);
        const QRgb* line = reinterpret_cast<const QRgb*>(texture.constScanLine(y));
        const QRgb sampled = line[x];
        return QColor(qRed(sampled), qGreen(sampled), qBlue(sampled));
    }

    QColor sampleNightTexture(double latitude, double longitude) const
    {
        QColor sampled = sampleTextureRgb(nightTexture(), latitude, longitude, QColor(3, 8, 18));
        return QColor(std::min(255, static_cast<int>(sampled.red() * 0.90)),
                      std::min(255, static_cast<int>(sampled.green() * 0.95)),
                      std::min(255, static_cast<int>(sampled.blue() * 1.05)));
    }

    QColor sampleDayTexture(double latitude, double longitude) const
    {
        QColor sampled = sampleTextureRgb(dayTexture(), latitude, longitude, QColor(35, 125, 170));
        return QColor(std::min(255, static_cast<int>(sampled.red() * 1.04)),
                      std::min(255, static_cast<int>(sampled.green() * 1.06)),
                      std::min(255, static_cast<int>(sampled.blue() * 1.10)));
    }

    void paintGlobe(QPainter& painter, const QRectF& globe_rect)
    {
        const qreal pixel_ratio = devicePixelRatioF();
        const bool moving = m_dragging ||
                            std::abs(m_spin_velocity_longitude) > 0.002 ||
                            std::abs(m_spin_velocity_latitude) > 0.002 ||
                            m_user_spin_pause_ms > 0 ||
                            (m_route.size() > 1 && m_route_elapsed_ms >= NODE_HOLD_MS);
        const qreal max_raster =
#ifdef Q_OS_WIN
            moving ? 520.0 : 700.0;
#else
            moving ? 760.0 : 1250.0;
#endif
        const int image_size = std::max(420, static_cast<int>(std::min<qreal>(max_raster, globe_rect.width() * pixel_ratio)));
        const int lat_key = static_cast<int>(std::llround(m_center_latitude * 20.0));
        const int lon_key = static_cast<int>(std::llround(normalizeLongitude(m_center_longitude) * 20.0));
        const int sun_key = QDateTime::currentDateTimeUtc().toSecsSinceEpoch() / 600;
        const bool cache_hit = !moving &&
                               !m_cached_globe_image.isNull() &&
                               m_cached_globe_size == QSize(image_size, image_size) &&
                               m_cached_globe_lat_key == lat_key &&
                               m_cached_globe_lon_key == lon_key &&
                               m_cached_globe_sun_key == sun_key;
        if (cache_hit) {
            painter.save();
            QPainterPath clip_path;
            clip_path.addEllipse(globe_rect);
            painter.setClipPath(clip_path);
            painter.drawImage(globe_rect.adjusted(-1.2, -1.2, 1.2, 1.2), m_cached_globe_image);
            painter.restore();
            QColor rim = palette().highlight().color();
            rim.setAlpha(220);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(rim, 1.4));
            painter.drawEllipse(globe_rect);
            return;
        }

        QImage globe(image_size, image_size, QImage::Format_ARGB32);
        globe.fill(Qt::transparent);
        const double radius = image_size / 2.0 - 0.5;
        const QPointF center_point(image_size / 2.0, image_size / 2.0);
        const QVector3D sun = sunVector();
        QVector3D view_center;
        QVector3D view_east;
        QVector3D view_north;
        viewBasis(view_center, view_east, view_north);

        for (int y = 0; y < image_size; ++y) {
            QRgb* line = reinterpret_cast<QRgb*>(globe.scanLine(y));
            for (int x = 0; x < image_size; ++x) {
                const double nx = (x - center_point.x()) / radius;
                const double ny = (y - center_point.y()) / radius;
                const double dist2 = nx * nx + ny * ny;
                if (dist2 > 1.012) continue;

                const double nz = std::sqrt(std::max(0.0, 1.0 - std::min(1.0, dist2)));
                QVector3D normal = view_east * nx + view_north * (-ny) + view_center * nz;
                normal.normalize();
                const DisplayPoint point = vectorToDisplayPoint(normal);
                const double daylight = QVector3D::dotProduct(normal, sun);

                QColor day_color = sampleDayTexture(point.latitude, point.longitude);
                const double day_light = clampDouble(daylight * 0.65 + 0.42, 0.0, 1.0);
                day_color = blendColor(QColor(18, 70, 105), day_color, day_light);
                QColor night_color = sampleNightTexture(point.latitude, point.longitude);
                night_color = blendColor(QColor(2, 7, 17), night_color, 0.82);
                const double day_mix = smoothStep((daylight + 0.13) / 0.30);
                QColor color = blendColor(night_color, day_color, day_mix);
                const double terminator = 1.0 - std::abs(daylight) / 0.14;
                if (terminator > 0.0) {
                    color = blendColor(color, QColor(74, 154, 184), clampDouble(terminator, 0.0, 1.0) * 0.18);
                }

                if (dist2 > 0.995) {
                    color = blendColor(color, QColor(8, 20, 38), clampDouble((dist2 - 0.995) / 0.017, 0.0, 1.0) * 0.45);
                }
                line[x] = color.rgba();
            }
        }

        painter.save();
        QPainterPath clip_path;
        clip_path.addEllipse(globe_rect);
        painter.setClipPath(clip_path);
        painter.drawImage(globe_rect.adjusted(-1.2, -1.2, 1.2, 1.2), globe);
        painter.restore();
        m_cached_globe_image = globe;
        m_cached_globe_size = QSize(image_size, image_size);
        m_cached_globe_lat_key = lat_key;
        m_cached_globe_lon_key = lon_key;
        m_cached_globe_sun_key = sun_key;
        QColor rim = palette().highlight().color();
        rim.setAlpha(220);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(rim, 1.4));
        painter.drawEllipse(globe_rect);
    }

    void paintGlobeGrid(QPainter& painter, const QRectF& globe_rect)
    {
        painter.save();
        QPainterPath clip_path;
        clip_path.addEllipse(globe_rect);
        painter.setClipPath(clip_path);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(Qt::NoBrush);

        QColor minor_color(88, 160, 202, 62);
        QColor major_color(122, 198, 226, 92);
        QPen minor_pen(minor_color, 0.72, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        QPen major_pen(major_color, 0.95, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        minor_pen.setCosmetic(true);
        major_pen.setCosmetic(true);

        auto draw_geo_line = [&](const QVector<QPointF>& geo_points, const QPen& pen) {
            QPainterPath path;
            bool path_started = false;
            QPointF previous_projected;
            painter.setPen(pen);

            for (const QPointF& geo : geo_points) {
                bool visible = false;
                const QPointF projected = project(geo.y(), geo.x(), globe_rect, &visible);
                const bool too_far = path_started && QLineF(previous_projected, projected).length() > globe_rect.width() * 0.18;
                if (visible && !too_far) {
                    if (!path_started) {
                        path.moveTo(projected);
                        path_started = true;
                    } else {
                        path.lineTo(projected);
                    }
                    previous_projected = projected;
                } else {
                    if (!path.isEmpty()) {
                        painter.drawPath(path);
                        path = QPainterPath();
                    }
                    path_started = false;
                    previous_projected = projected;
                }
            }

            if (!path.isEmpty()) {
                painter.drawPath(path);
            }
        };

        for (int latitude = -75; latitude <= 75; latitude += 15) {
            QVector<QPointF> points;
            points.reserve(241);
            for (double longitude = -180.0; longitude <= 180.0; longitude += 1.5) {
                points.push_back(QPointF(longitude, latitude));
            }
            const bool major = latitude == 0 || std::abs(latitude) == 60;
            draw_geo_line(points, major ? major_pen : minor_pen);
        }

        for (int longitude = -180; longitude < 180; longitude += 15) {
            QVector<QPointF> points;
            points.reserve(115);
            for (double latitude = -85.0; latitude <= 85.0; latitude += 1.5) {
                points.push_back(QPointF(longitude, latitude));
            }
            const bool major = longitude == 0 || std::abs(longitude) == 90 || longitude == -180;
            draw_geo_line(points, major ? major_pen : minor_pen);
        }

        painter.restore();
    }

    QPointF project(double latitude, double longitude, const QRectF& globe_rect, bool* visible = nullptr) const
    {
        QVector3D view_center;
        QVector3D view_east;
        QVector3D view_north;
        viewBasis(view_center, view_east, view_north);
        const QVector3D normal = surfaceVector(latitude, longitude).normalized();
        const double x = QVector3D::dotProduct(normal, view_east);
        const double y = QVector3D::dotProduct(normal, view_north);
        const double z = QVector3D::dotProduct(normal, view_center);
        if (visible) *visible = z > -0.05;
        const double radius = globe_rect.width() / 2.0;
        return QPointF(globe_rect.center().x() + x * radius,
                       globe_rect.center().y() - y * radius);
    }

    void paintCountryBorders(QPainter& painter, const QRectF& globe_rect)
    {
        painter.save();
        QPainterPath clip_path;
        clip_path.addEllipse(globe_rect);
        painter.setClipPath(clip_path);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QColor border_color = palette().highlight().color();
        border_color = blendColor(border_color, QColor(225, 240, 245), 0.48);
        border_color.setAlpha(185);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(border_color, 0.85));

        for (const GeoLine& line : countryBorders()) {
            QPainterPath path;
            bool path_started = false;
            QPointF previous_geo;
            bool has_previous = false;
            for (const QPointF& geo : line.points) {
                if (has_previous && std::abs(normalizeLongitude(geo.x() - previous_geo.x())) > 90.0) {
                    path_started = false;
                }
                bool visible = false;
                const QPointF p = project(geo.y(), geo.x(), globe_rect, &visible);
                if (visible) {
                    if (!path_started) {
                        path.moveTo(p);
                        path_started = true;
                    } else {
                        path.lineTo(p);
                    }
                } else {
                    path_started = false;
                }
                previous_geo = geo;
                has_previous = true;
            }
            if (!path.isEmpty()) {
                painter.drawPath(path);
            }
        }
        painter.restore();
    }

    QPainterPath buildSurfacePath(const DisplayPoint& from, const DisplayPoint& to, const QRectF& globe_rect, bool* any_visible = nullptr) const
    {
        QPainterPath path;
        const QVector3D a = surfaceVector(from.latitude, from.longitude).normalized();
        const QVector3D b = surfaceVector(to.latitude, to.longitude).normalized();
        const double omega = std::acos(boundedUnit(QVector3D::dotProduct(a, b)));
        const double sin_omega = std::sin(omega);
        const int steps = std::max(16, std::min(56, static_cast<int>(globe_rect.width() / 18.0)));
        bool started = false;
        bool visible_any = false;
        QPointF previous;

        for (int i = 0; i <= steps; ++i) {
            const double t = static_cast<double>(i) / steps;
            QVector3D point;
            if (std::abs(sin_omega) < 1e-6) {
                point = (a * (1.0 - t) + b * t).normalized();
            } else {
                point = (a * (std::sin((1.0 - t) * omega) / sin_omega) +
                         b * (std::sin(t * omega) / sin_omega)).normalized();
            }
            const DisplayPoint geo = vectorToDisplayPoint(point);
            bool visible = false;
            const QPointF projected = project(geo.latitude, geo.longitude, globe_rect, &visible);
            const bool jump = started && QLineF(previous, projected).length() > globe_rect.width() * 0.20;
            if (visible && !jump) {
                if (!started) {
                    path.moveTo(projected);
                    started = true;
                } else {
                    path.lineTo(projected);
                }
                previous = projected;
                visible_any = true;
            } else {
                started = false;
                previous = projected;
            }
        }

        if (any_visible) *any_visible = visible_any;
        return path;
    }

    void paintRoutes(QPainter& painter, const QRectF& globe_rect)
    {
        if (m_route.size() < 2) return;

        const QColor sent_color("#1E90FF");
        const QColor recv_color("#00FA9A");
        painter.save();
        QPainterPath clip_path;
        clip_path.addEllipse(globe_rect);
        painter.setClipPath(clip_path);

        const int local_node = localNodeIndex();
        if (local_node >= 0) {
            for (int i = 0; i < m_nodes.size(); ++i) {
                if (i == local_node) continue;
                paintMeasuredPeerLine(painter, globe_rect, local_node, i, sent_color, recv_color);
            }
            painter.restore();
            return;
        }

        bool illustrative_mode = true;
        bool defcon_mode = !m_nodes.isEmpty();
        bool demo_mode = !m_nodes.isEmpty();
        for (const PeerMapNode& node : m_nodes) {
            if (node.user_agent != QLatin1String("DC34 Groups")) {
                defcon_mode = false;
            }
            if (node.user_agent != QLatin1String("/DefcoinCore:demo/")) {
                demo_mode = false;
            }
            if (node.user_agent != QLatin1String("DC34 Groups") && node.user_agent != QLatin1String("/DefcoinCore:demo/")) {
                illustrative_mode = false;
                break;
            }
        }
        if (defcon_mode || demo_mode) {
            painter.restore();
            return;
        }
        if (!illustrative_mode) {
            painter.restore();
            return;
        }

        // Presentation/demo data does not contain measured peer traffic. Keep it
        // lightweight and visibly illustrative rather than drawing thick links.
        for (int i = 1; i < m_route.size(); ++i) {
            const int a = m_route.at(i - 1);
            const int b = m_route.at(i);
            const DisplayPoint from = m_display_points.value(a);
            const DisplayPoint to = m_display_points.value(b);
            bool visible = false;
            const QPainterPath path = buildSurfacePath(from, to, globe_rect, &visible);
            if (!visible || path.isEmpty()) continue;

            QColor route_color = (i % 2) ? recv_color : sent_color;
            route_color.setAlpha(42);
            QPen route_pen(route_color, 0.45, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            route_pen.setCosmetic(true);
            painter.setPen(route_pen);
            painter.drawPath(path);

            const qreal pulse = std::fmod(m_phase * 0.34 + i * 0.17, 1.0);
            QPointF pulse_point = path.pointAtPercent(pulse);
            route_color.setAlpha(200);
            painter.setBrush(route_color);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(pulse_point, 1.25, 1.25);
        }
        painter.restore();
    }

    int localNodeIndex() const
    {
        for (int i = 0; i < m_nodes.size(); ++i) {
            if (m_nodes.at(i).is_local) return i;
        }
        return -1;
    }

    static qreal trafficLineWidth(quint64 bytes, qreal freshness)
    {
        if (bytes == 0 || freshness <= 0.0) return 0.0;
        const qreal scaled = std::log10(static_cast<qreal>(bytes) + 1.0);
        return 0.55 + std::min<qreal>(0.65, scaled * 0.14) * freshness;
    }

    void paintMeasuredPeerLine(QPainter& painter, const QRectF& globe_rect, int local_node, int peer_node, const QColor& sent_color, const QColor& recv_color)
    {
        const DisplayPoint local = m_display_points.value(local_node);
        const DisplayPoint peer = m_display_points.value(peer_node);
        bool visible = false;
        const QPainterPath path = buildSurfacePath(local, peer, globe_rect, &visible);
        if (!visible || path.isEmpty()) return;

        QColor base_color = blendColor(sent_color, recv_color, 0.54);
        base_color.setAlpha(56);
        QPen base_pen(base_color, 0.55, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        base_pen.setCosmetic(true);
        painter.setPen(base_pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);

        const PeerMapNode& peer_node_data = m_nodes.at(peer_node);
        const PeerMapTrafficPulse pulse = m_recent_traffic.value(peer_node_data.ip);
        const qint64 age = pulse.timestamp_msecs > 0 ? QDateTime::currentMSecsSinceEpoch() - pulse.timestamp_msecs : TRAFFIC_PULSE_DECAY_MS + 1;
        const qreal freshness = pulse.timestamp_msecs > 0 ? clampDouble(1.0 - static_cast<qreal>(age) / TRAFFIC_PULSE_DECAY_MS, 0.0, 1.0) : 0.0;

        auto draw_active_line = [&](const QColor& color, quint64 bytes) {
            const qreal width = trafficLineWidth(bytes, freshness);
            if (width <= 0.0) return;
            QColor active = color;
            active.setAlpha(static_cast<int>(95 + 130 * freshness));
            QPen pen(active, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            pen.setCosmetic(true);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
        };

        // Received bytes are traffic from the peer to this node; sent bytes are
        // traffic from this node to the peer.
        draw_active_line(recv_color, pulse.received_delta);
        draw_active_line(sent_color, pulse.sent_delta);

        const qreal outbound = std::fmod(m_phase * 0.36 + peer_node * 0.09, 1.0);
        const qreal inbound = std::fmod(m_phase * 0.33 + peer_node * 0.13 + 0.5, 1.0);
        QColor sent_spark = sent_color;
        QColor recv_spark = recv_color;
        sent_spark.setAlpha(freshness > 0.0 && pulse.sent_delta > 0 ? 240 : 145);
        recv_spark.setAlpha(freshness > 0.0 && pulse.received_delta > 0 ? 240 : 145);
        painter.setPen(Qt::NoPen);
        painter.setBrush(sent_spark);
        painter.drawEllipse(path.pointAtPercent(outbound), 1.2 + freshness * 0.5, 1.2 + freshness * 0.5);
        painter.setBrush(recv_spark);
        painter.drawEllipse(path.pointAtPercent(1.0 - inbound), 1.2 + freshness * 0.5, 1.2 + freshness * 0.5);
    }

    void paintNodes(QPainter& painter, const QRectF& globe_rect)
    {
        const QColor text_color = palette().color(QPalette::WindowText);
        QFont label_font = font();
        label_font.setPointSize(std::max(8, label_font.pointSize() - 1));
        painter.setFont(label_font);
        const int focused_node = focusedNodeIndex();

        for (int i = 0; i < m_nodes.size(); ++i) {
            bool visible = false;
            const DisplayPoint point = m_display_points.value(i);
            const QPointF p = project(point.latitude, point.longitude, globe_rect, &visible);
            if (!visible) continue;

            const PeerMapNode& node = m_nodes.at(i);
            QColor marker = node.is_local ? QColor("#FFD700") : (node.is_lan ? QColor("#87CEFA") : QColor("#00FA9A"));
            const quint64 activity = node.sent + node.received;
            const bool focused = i == focused_node;
            const qreal radius = std::min<qreal>(focused ? 10.0 : 7.0, (focused ? 5.8 : 4.0) + std::log10(static_cast<double>(std::max<quint64>(1, activity))) * 0.7);
            marker.setAlpha(240);
            painter.setBrush(marker);
            painter.setPen(QPen(focused ? QColor(255, 255, 255, 220) : QColor(0, 0, 0, 180), focused ? 1.5 : 1.0));
            painter.drawEllipse(p, radius, radius);

            const bool show_label = focused || node.is_local || node.is_lan || m_camera_zoom > 1.32;
            if (!show_label) continue;
            const QString label = !node.map_label.isEmpty() ? node.map_label :
                (node.city_state.isEmpty() ? (node.is_local ? tr("Self") : node.ip) : node.city_state);
            QRectF label_rect(p.x() + radius + 4, p.y() - 9, 190, 18);
            painter.setPen(QColor(0, 0, 0, 160));
            painter.drawText(label_rect.translated(1, 1), Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics(label_font).elidedText(label, Qt::ElideRight, static_cast<int>(label_rect.width())));
            painter.setPen(text_color);
            painter.drawText(label_rect, Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics(label_font).elidedText(label, Qt::ElideRight, static_cast<int>(label_rect.width())));
        }
    }

    QVector<PeerMapNode> m_nodes;
    QVector<DisplayPoint> m_display_points;
    QVector<int> m_route;
    QHash<QString, PeerMapTrafficPulse> m_last_traffic_totals;
    QHash<QString, PeerMapTrafficPulse> m_recent_traffic;
    QString m_route_signature;
    QImage m_cached_globe_image;
    QSize m_cached_globe_size;
    int m_cached_globe_lat_key{std::numeric_limits<int>::min()};
    int m_cached_globe_lon_key{std::numeric_limits<int>::min()};
    qint64 m_cached_globe_sun_key{std::numeric_limits<qint64>::min()};
    int m_route_index{0};
    int m_route_elapsed_ms{0};
    double m_center_latitude{20.0};
    double m_center_longitude{-95.0};
    double m_camera_zoom{1.0};
    double m_zoom_in_default{5.00};
    double m_zoom_out_default{1.55};
    double m_zoom_in{5.00};
    double m_zoom_out{1.55};
    double m_phase{0.0};
    double m_speed_multiplier{1.0};
    int m_speed_boost_ticks{0};
    bool m_dragging{false};
    QPoint m_last_drag_pos;
    double m_spin_velocity_latitude{0.0};
    double m_spin_velocity_longitude{0.0};
    int m_user_spin_pause_ms{0};
    QTimer m_animation_timer;
    std::function<void(int)> m_focus_changed_callback;
    mutable QVector3D m_cached_view_center;
    mutable QVector3D m_cached_view_east;
    mutable QVector3D m_cached_view_north;
    mutable bool m_projection_basis_valid{false};
};

class PeerMapFullscreenDialog final : public QDialog
{
public:
    explicit PeerMapFullscreenDialog(QWidget* parent = nullptr) : QDialog(parent)
    {
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_DeleteOnClose, false);
        setFocusPolicy(Qt::StrongFocus);
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        m_globe = new PeerGlobeCanvas(this);
        m_globe->installEventFilter(this);
        layout->addWidget(m_globe);

        m_title = new QLabel(tr("Defcoin Peer Map"), this);
        QFont title_font = m_title->font();
        title_font.setPointSize(std::max(18, title_font.pointSize() + 8));
        title_font.setBold(true);
        m_title->setFont(title_font);
        m_title->setStyleSheet(QStringLiteral("QLabel { color: rgba(235,245,255,210); background: rgba(0,0,0,50); padding: 7px 18px; border-radius: 7px; }"));
        m_title->setAttribute(Qt::WA_TransparentForMouseEvents);

        m_footer = new QLabel(tr("Traffic between remote peers is illustrated; this node only measures its own peer connections.<br>Press Space to hide on-screen text. Press Esc to return. Drag to spin. Use + / - to adjust zoom."), this);
        m_footer->setTextFormat(Qt::RichText);
        m_footer->setWordWrap(true);
        m_footer->setAlignment(Qt::AlignCenter);
        m_footer->setStyleSheet(QStringLiteral("QLabel { color: rgba(235,245,255,170); background: rgba(0,0,0,60); padding: 6px 12px; border-radius: 5px; }"));
        m_footer->setAttribute(Qt::WA_TransparentForMouseEvents);

        m_nav_controls = new QWidget(this);
        m_nav_controls->setObjectName(QStringLiteral("peerMapFullscreenNav"));
        m_nav_controls->setStyleSheet(QStringLiteral(
            "QWidget#peerMapFullscreenNav { background: rgba(0,0,0,42); border: 1px solid rgba(180,220,255,70); border-radius: 8px; }"
            "QToolButton { color: rgba(230,244,255,210); background: rgba(20,35,55,92); border: 1px solid rgba(180,220,255,80); border-radius: 4px; padding: 2px; font-weight: 700; }"
            "QToolButton:hover { background: rgba(55,105,150,132); border-color: rgba(210,240,255,135); }"));
        QGridLayout* nav_layout = new QGridLayout(m_nav_controls);
        nav_layout->setContentsMargins(6, 6, 6, 6);
        nav_layout->setSpacing(4);
        auto nav_button = [this](const QString& text, const QString& tooltip) {
            QToolButton* button = new QToolButton(m_nav_controls);
            button->setText(text);
            button->setToolTip(tooltip);
            button->setFixedSize(30, 28);
            button->setAutoRaise(false);
            return button;
        };
        m_zoom_in_button = nav_button(QStringLiteral("+"), tr("Zoom in"));
        m_zoom_out_button = nav_button(QStringLiteral("-"), tr("Zoom out"));
        m_nav_up_button = nav_button(QString::fromUtf8("↑"), tr("Nudge map north"));
        m_nav_left_button = nav_button(QString::fromUtf8("←"), tr("Nudge map west"));
        m_nav_right_button = nav_button(QString::fromUtf8("→"), tr("Nudge map east"));
        m_nav_down_button = nav_button(QString::fromUtf8("↓"), tr("Nudge map south"));
        nav_layout->addWidget(m_zoom_in_button, 0, 1);
        nav_layout->addWidget(m_nav_up_button, 1, 1);
        nav_layout->addWidget(m_nav_left_button, 2, 0);
        nav_layout->addWidget(m_nav_right_button, 2, 2);
        nav_layout->addWidget(m_nav_down_button, 3, 1);
        nav_layout->addWidget(m_zoom_out_button, 4, 1);
        connect(m_zoom_in_button, &QToolButton::clicked, this, [this] { if (m_globe) m_globe->adjustZoom(1.08); });
        connect(m_zoom_out_button, &QToolButton::clicked, this, [this] { if (m_globe) m_globe->adjustZoom(1.0 / 1.08); });
        connect(m_nav_up_button, &QToolButton::clicked, this, [this] { if (m_globe) m_globe->nudgeView(7.0, 0.0); });
        connect(m_nav_down_button, &QToolButton::clicked, this, [this] { if (m_globe) m_globe->nudgeView(-7.0, 0.0); });
        connect(m_nav_left_button, &QToolButton::clicked, this, [this] { if (m_globe) m_globe->nudgeView(0.0, -10.0); });
        connect(m_nav_right_button, &QToolButton::clicked, this, [this] { if (m_globe) m_globe->nudgeView(0.0, 10.0); });

    }

    void setNodes(const QVector<PeerMapNode>& nodes, const PeerGlobeCanvas::ViewState& state)
    {
        if (!m_globe) return;
        m_globe->setNodes(nodes);
        m_globe->setViewState(state);
        m_globe->setFocus(Qt::OtherFocusReason);
        placeOverlays();
    }

    PeerGlobeCanvas::ViewState viewState() const
    {
        return m_globe ? m_globe->viewState() : PeerGlobeCanvas::ViewState{};
    }

    void setDemoToggleCallback(std::function<void()> callback)
    {
        m_demo_toggle_callback = std::move(callback);
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QDialog::resizeEvent(event);
        placeOverlays();
    }

    void showEvent(QShowEvent* event) override
    {
        QDialog::showEvent(event);
        if (m_globe) {
            m_globe->setFocus(Qt::OtherFocusReason);
        }
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_globe && event->type() == QEvent::KeyPress) {
            if (handleFullScreenKey(static_cast<QKeyEvent*>(event))) return true;
        }
        return QDialog::eventFilter(watched, event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (handleFullScreenKey(event)) return;
        if (m_globe) {
            m_globe->setFocus(Qt::OtherFocusReason);
        }
        QDialog::keyPressEvent(event);
    }

private:
    bool handleFullScreenKey(QKeyEvent* event)
    {
        if (event->key() == Qt::Key_Escape) {
            close();
            event->accept();
            return true;
        }
        if (event->key() == Qt::Key_Space) {
            m_overlay_state = (m_overlay_state + 1) % 3;
            applyOverlayState();
            event->accept();
            return true;
        }
        if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
            if (m_globe) m_globe->adjustZoom(1.08);
            event->accept();
            return true;
        }
        if (event->key() == Qt::Key_Minus || event->key() == Qt::Key_Underscore) {
            if (m_globe) m_globe->adjustZoom(1.0 / 1.08);
            event->accept();
            return true;
        }
        return false;
    }

    void applyOverlayState()
    {
        if (m_title) m_title->setVisible(m_overlay_state != 2);
        if (m_footer) m_footer->setVisible(m_overlay_state == 0);
        if (m_nav_controls) m_nav_controls->setVisible(m_overlay_state == 0);
        placeOverlays();
    }

    void placeOverlays()
    {
        if (m_title) {
            m_title->adjustSize();
            m_title->move((width() - m_title->width()) / 2, 24);
            m_title->raise();
        }
        if (m_footer) {
            const int max_width = std::min(width() - 80, 980);
            m_footer->setFixedWidth(std::max(300, max_width));
            m_footer->adjustSize();
            m_footer->move((width() - m_footer->width()) / 2, height() - m_footer->height() - 28);
            m_footer->raise();
        }
        if (m_nav_controls) {
            m_nav_controls->adjustSize();
            m_nav_controls->move(width() - m_nav_controls->width() - 26, std::max(82, height() / 2 - m_nav_controls->height() / 2));
            m_nav_controls->raise();
        }
    }

    QLabel* m_title{nullptr};
    QLabel* m_footer{nullptr};
    QWidget* m_nav_controls{nullptr};
    QToolButton* m_zoom_in_button{nullptr};
    QToolButton* m_zoom_out_button{nullptr};
    QToolButton* m_nav_up_button{nullptr};
    QToolButton* m_nav_left_button{nullptr};
    QToolButton* m_nav_right_button{nullptr};
    QToolButton* m_nav_down_button{nullptr};
    PeerGlobeCanvas* m_globe{nullptr};
    QString m_demo_key_buffer;
    int m_overlay_state{0};
    std::function<void()> m_demo_toggle_callback;
};

class PeerMapWidget final : public QWidget
{
public:
    explicit PeerMapWidget(QWidget* parent = nullptr) : QWidget(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
        QHBoxLayout* root = new QHBoxLayout(this);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(8);

        m_splitter = new QSplitter(Qt::Horizontal, this);
        m_splitter->setChildrenCollapsible(false);
        root->addWidget(m_splitter);

        m_sidebar = new QWidget(this);
        m_sidebar->setObjectName(QStringLiteral("peerMapSidebar"));
        m_sidebar->setMinimumWidth(360);
        m_sidebar->setMaximumWidth(500);
        QVBoxLayout* sidebar_layout = new QVBoxLayout(m_sidebar);
        sidebar_layout->setContentsMargins(6, 4, 6, 4);
        sidebar_layout->setSpacing(5);

        QHBoxLayout* sidebar_header = new QHBoxLayout();
        sidebar_header->setContentsMargins(0, 0, 0, 0);
        m_collapse_button = new QToolButton(m_sidebar);
        m_collapse_button->setObjectName(QStringLiteral("panelCollapseButton"));
        m_collapse_button->setAutoRaise(false);
        m_collapse_button->setFixedSize(34, 34);
        m_collapse_button->setArrowType(Qt::NoArrow);
        QFont collapse_font = m_collapse_button->font();
        collapse_font.setPointSize(std::max(13, collapse_font.pointSize() + 2));
        collapse_font.setBold(true);
        m_collapse_button->setFont(collapse_font);
        m_collapse_button->setText(QString::fromUtf8("◄"));
        m_collapse_button->setToolTip(tr("Collapse node list"));
        m_sidebar_title = new QLabel(tr("Traveling Found Node Locations"), m_sidebar);
        QFont title_font = m_sidebar_title->font();
        title_font.setBold(true);
        m_sidebar_title->setFont(title_font);
        sidebar_header->addWidget(m_sidebar_title);
        sidebar_header->addStretch(1);
        sidebar_header->addWidget(m_collapse_button);
        sidebar_layout->addLayout(sidebar_header);

        m_lookup_locations = new QCheckBox(tr("Lookup Peer Locations"), m_sidebar);
        m_lookup_locations->setChecked(QSettings().value(QStringLiteral("PeerTableGetLocation"), true).toBool());
        m_lookup_locations->setToolTip(tr("Populate city/state and coordinates. Public peers use their IP address; local and LAN rows use this node's public WAN address."));
        m_lookup_locations->hide();
        m_wan_location_manager = new QNetworkAccessManager(this);

        m_source_panel = new QWidget(m_sidebar);
        QVBoxLayout* source_layout = new QVBoxLayout(m_source_panel);
        source_layout->setContentsMargins(0, 2, 0, 2);
        source_layout->setSpacing(4);
        m_source_group = new QButtonGroup(m_source_panel);
        m_real_nodes_radio = new QRadioButton(tr("Show connected peers"), m_source_panel);
        m_defcon_meetups_radio = new QRadioButton(tr("Show DC34 meetup cities"), m_source_panel);
        m_demo_locations_radio = new QRadioButton(tr("Popular Data Centers Around the World"), m_source_panel);
        m_source_group->addButton(m_real_nodes_radio, static_cast<int>(PeerMapDataSource::RealNodes));
        m_source_group->addButton(m_defcon_meetups_radio, static_cast<int>(PeerMapDataSource::DefconMeetups));
        m_source_group->addButton(m_demo_locations_radio, static_cast<int>(PeerMapDataSource::DemoLocations));
        m_real_nodes_radio->setChecked(true);
        m_defcon_meetups_radio->setToolTip(tr("Use a curated presentation list of DC34 Groups and meetup cities."));
        m_demo_locations_radio->setToolTip(tr("Use a curated global list of 200 major data-center markets for route and rendering tests."));
        source_layout->addWidget(m_real_nodes_radio);
        source_layout->addWidget(m_defcon_meetups_radio);
        source_layout->addWidget(m_demo_locations_radio);
        sidebar_layout->addWidget(m_source_panel);

        m_node_list = new QTableWidget(m_sidebar);
        m_node_list->setColumnCount(3);
        m_node_list->setHorizontalHeaderLabels(QStringList() << tr("Route") << tr("IP") << tr("City, St"));
        m_node_list->verticalHeader()->hide();
        m_node_list->horizontalHeader()->setStretchLastSection(false);
        m_node_list->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_node_list->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
        m_node_list->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
        m_node_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_node_list->setSelectionMode(QAbstractItemView::SingleSelection);
        m_node_list->setSelectionBehavior(QAbstractItemView::SelectRows);
        sidebar_layout->addWidget(m_node_list, 1);
        m_node_list->installEventFilter(this);

        m_globe_panel = new QWidget(this);
        QVBoxLayout* globe_layout = new QVBoxLayout(m_globe_panel);
        globe_layout->setContentsMargins(0, 0, 0, 0);
        globe_layout->setSpacing(4);
        QHBoxLayout* globe_actions = new QHBoxLayout();
        globe_actions->setContentsMargins(0, 0, 0, 0);
        globe_actions->addStretch(1);
        m_fullscreen_button = new QToolButton(m_globe_panel);
        m_fullscreen_button->setAutoRaise(true);
        m_fullscreen_button->setText(QString::fromUtf8("⛶"));
        m_fullscreen_button->setToolTip(tr("Open the animated globe full screen"));
        globe_actions->addWidget(m_fullscreen_button);
        globe_layout->addLayout(globe_actions);

        m_globe = new PeerGlobeCanvas(m_globe_panel);
        m_globe->installEventFilter(this);
        m_globe->setFocusChangedCallback([this](int node_index) {
            highlightFocusedNode(node_index);
        });
        globe_layout->addWidget(m_globe, 1);
        m_globe_layout = globe_layout;
        m_splitter->addWidget(m_sidebar);
        m_splitter->addWidget(m_globe_panel);
        m_splitter->setStretchFactor(0, 0);
        m_splitter->setStretchFactor(1, 1);
        const QByteArray splitter_state = QSettings().value(QStringLiteral("PeerMapSplitterState")).toByteArray();
        if (!splitter_state.isEmpty()) {
            m_splitter->restoreState(splitter_state);
        } else {
            m_splitter->setSizes(QList<int>() << 420 << 980);
        }

        connect(m_collapse_button, &QToolButton::clicked, this, [this] { setSidebarCollapsed(!m_sidebar_collapsed); });
        connect(m_fullscreen_button, &QToolButton::clicked, this, [this] { showFullScreenGlobe(); });
        connect(m_real_nodes_radio, &QRadioButton::toggled, this, [this](bool checked) {
            if (checked) setDataSource(PeerMapDataSource::RealNodes);
        });
        connect(m_defcon_meetups_radio, &QRadioButton::toggled, this, [this](bool checked) {
            if (checked) setDataSource(PeerMapDataSource::DefconMeetups);
        });
        connect(m_demo_locations_radio, &QRadioButton::toggled, this, [this](bool checked) {
            if (checked) setDataSource(PeerMapDataSource::DemoLocations);
        });
        connect(m_splitter, &QSplitter::splitterMoved, this, [this] {
            QSettings().setValue(QStringLiteral("PeerMapSplitterState"), m_splitter->saveState());
        });
        connect(m_node_list, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
            const int node_index = m_sidebar_route_nodes.value(row, -1);
            if (node_index >= 0) m_globe->focusNodeIndex(node_index, 2.5);
        });
        connect(m_lookup_locations, &QCheckBox::toggled, this, [this](bool checked) {
            QSettings().setValue(QStringLiteral("PeerTableGetLocation"), checked);
            if (!checked) {
                m_wan_city_state.clear();
                m_wan_location_valid = false;
                m_wan_location_pending = false;
            }
            if (m_peer_model) m_peer_model->setGeolocationEnabled(checked);
            refreshFromModel();
        });

        m_refresh_timer.setInterval(3000);
        connect(&m_refresh_timer, &QTimer::timeout, this, [this] { refreshFromModel(); });
        m_refresh_timer.start();

        m_sidebar_collapsed = QSettings().value(QStringLiteral("PeerMapSidebarCollapsed"), true).toBool();
        setSidebarCollapsed(m_sidebar_collapsed);
    }

    void setPeerModel(PeerTableModel* model)
    {
        if (m_peer_model == model) return;
        m_peer_model = model;
        if (!m_peer_model) {
            refreshFromModel();
            return;
        }

        m_peer_model->setGeolocationEnabled(m_lookup_locations->isChecked());
        connect(m_peer_model, &QAbstractItemModel::dataChanged, this, [this] { refreshFromModel(); });
        connect(m_peer_model, &QAbstractItemModel::layoutChanged, this, [this] { refreshFromModel(); });
        connect(m_peer_model, &QObject::destroyed, this, [this] {
            m_peer_model = nullptr;
            refreshFromModel();
        });
        refreshFromModel();
    }

    void setDemoMode(bool enabled)
    {
        m_demo_mode = enabled;
        setDataSource(enabled ? PeerMapDataSource::DemoLocations : PeerMapDataSource::RealNodes);
    }

    bool demoMode() const
    {
        return m_demo_mode;
    }

    void resetToRealNodes()
    {
        m_demo_key_buffer.clear();
        setDataSource(PeerMapDataSource::RealNodes);
    }

    void setDemoToggleCallback(std::function<void()> callback)
    {
        m_demo_toggle_callback = std::move(callback);
    }

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        QWidget::keyPressEvent(event);
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        return QWidget::eventFilter(watched, event);
    }

private:
    void setDataSource(PeerMapDataSource source)
    {
        if (m_data_source == source && m_current_nodes.size() > 0) return;
        m_data_source = source;
        m_demo_mode = (source == PeerMapDataSource::DemoLocations);
        m_node_list_default_widths_applied = false;
        if (m_source_group) {
            const QSignalBlocker blocker(m_source_group);
            if (QAbstractButton* button = m_source_group->button(static_cast<int>(source))) {
                button->setChecked(true);
            }
        }
        if (m_lookup_locations) m_lookup_locations->hide();
        refreshFromModel();
    }

    void cycleDataSource()
    {
        switch (m_data_source) {
        case PeerMapDataSource::RealNodes:
            setDataSource(PeerMapDataSource::DefconMeetups);
            break;
        case PeerMapDataSource::DefconMeetups:
            setDataSource(PeerMapDataSource::DemoLocations);
            break;
        case PeerMapDataSource::DemoLocations:
            setDataSource(PeerMapDataSource::RealNodes);
            break;
        }
    }

    void toggleDemoMode()
    {
        if (m_demo_toggle_callback) {
            m_demo_toggle_callback();
        } else {
            cycleDataSource();
        }
    }

    void setSidebarCollapsed(bool collapsed)
    {
        m_sidebar_collapsed = collapsed;
        QSettings().setValue(QStringLiteral("PeerMapSidebarCollapsed"), collapsed);
        m_sidebar_title->setVisible(!collapsed);
        if (m_lookup_locations) m_lookup_locations->hide();
        if (m_source_panel) m_source_panel->setVisible(!collapsed);
        m_node_list->setVisible(!collapsed);
        m_collapse_button->setArrowType(Qt::NoArrow);
        m_collapse_button->setText(collapsed ? QString::fromUtf8("►") : QString::fromUtf8("◄"));
        m_collapse_button->setToolTip(collapsed ? tr("Show node list") : tr("Collapse node list"));
        m_sidebar->setMaximumWidth(collapsed ? 72 : 500);
        m_sidebar->setMinimumWidth(collapsed ? 72 : 320);
        if (m_splitter && collapsed) {
            m_splitter->setSizes(QList<int>() << 72 << std::max(560, width() - 72));
        }
    }

    void showFullScreenGlobe()
    {
        if (m_fullscreen_dialog || !m_globe) return;
        m_fullscreen_dialog = new PeerMapFullscreenDialog(this);
        m_fullscreen_dialog->setDemoToggleCallback([this] { toggleDemoMode(); });
        m_fullscreen_dialog->setNodes(m_current_nodes, m_globe->viewState());
        connect(m_fullscreen_dialog, &QDialog::finished, this, [this] {
            if (m_globe && m_fullscreen_dialog) {
                m_globe->setViewState(m_fullscreen_dialog->viewState());
                m_globe->setFocus(Qt::OtherFocusReason);
            }
            m_fullscreen_dialog->deleteLater();
            m_fullscreen_dialog = nullptr;
        });
        m_fullscreen_dialog->showFullScreen();
        m_fullscreen_dialog->activateWindow();
        m_fullscreen_dialog->raise();
    }

    QVector<PeerMapNode> demoNodes() const
    {
        QVector<PeerMapNode> nodes;
        const auto& locations = DefcoinDemoPeers::peers();
        nodes.reserve(locations.size());
        for (const DefcoinDemoPeers::Peer& location : locations) {
            PeerMapNode node;
            node.ip = DefcoinDemoPeers::dataCenter(location);
            node.city_state = DefcoinDemoPeers::cityState(location);
            node.map_label = node.ip;
            node.user_agent = QStringLiteral("/DefcoinCore:demo/");
            node.website = DefcoinDemoPeers::ip(location);
            node.has_coordinates = true;
            node.latitude = location.latitude;
            node.longitude = location.longitude;
            node.sent = location.sent;
            node.received = location.received;
            nodes.push_back(node);
        }
        return nodes;
    }

    QVector<PeerMapNode> defconMeetupNodes() const
    {
        QVector<PeerMapNode> nodes;
        const auto& meetups = DefconMeetupPeers::meetups();
        nodes.reserve(meetups.size());
        for (const DefconMeetupPeers::Meetup& meetup : meetups) {
            PeerMapNode node;
            node.ip = DefconMeetupPeers::groupName(meetup);
            node.city_state = DefconMeetupPeers::cityState(meetup);
            node.map_label = tr("%1, %2").arg(node.ip, node.city_state);
            node.user_agent = QStringLiteral("DC34 Groups");
            node.website = DefconMeetupPeers::website(meetup);
            node.has_coordinates = true;
            node.latitude = meetup.latitude;
            node.longitude = meetup.longitude;
            const uint seed = qHash(node.ip);
            node.sent = 50000 + (seed % 85000);
            node.received = 120000 + ((seed / 17) % 210000);
            nodes.push_back(node);
        }
        return nodes;
    }

    void maybeRefreshWanLocation()
    {
        if (!m_lookup_locations->isChecked() || m_wan_location_valid || m_wan_location_pending || !m_wan_location_manager) return;
        QUrl url(QStringLiteral("http://ip-api.com/json/"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("fields"), QStringLiteral("success,query,city,region,lat,lon"));
        url.setQuery(query);
        QNetworkReply* reply = m_wan_location_manager->get(QNetworkRequest(url));
        m_wan_location_pending = true;
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            m_wan_location_pending = false;
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) return;
            const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            const QJsonObject object = doc.object();
            if (object.value(QStringLiteral("success")).toBool()) {
                const QJsonValue latitude = object.value(QStringLiteral("lat"));
                const QJsonValue longitude = object.value(QStringLiteral("lon"));
                if (latitude.isDouble() && longitude.isDouble()) {
                    QStringList parts;
                    const QString city = object.value(QStringLiteral("city")).toString().trimmed();
                    const QString state = object.value(QStringLiteral("region")).toString().trimmed();
                    if (!city.isEmpty()) parts << city;
                    if (!state.isEmpty() && state != city) parts << state;
                    m_wan_city_state = parts.join(QStringLiteral(", "));
                    m_wan_latitude = latitude.toDouble();
                    m_wan_longitude = longitude.toDouble();
                    m_wan_location_valid = !m_wan_city_state.isEmpty();
                    if (m_wan_location_valid) refreshFromModel();
                }
            }
        });
    }

    void refreshFromModel()
    {
        if (m_data_source == PeerMapDataSource::DemoLocations || m_data_source == PeerMapDataSource::DefconMeetups) {
            const QVector<PeerMapNode> nodes = (m_data_source == PeerMapDataSource::DefconMeetups) ? defconMeetupNodes() : demoNodes();
            m_current_nodes = nodes;
            m_globe->setNodes(nodes);
            if (m_fullscreen_dialog) {
                m_fullscreen_dialog->setNodes(nodes, m_fullscreen_dialog->viewState());
            }
            populateNodeList(nodes);
            highlightFocusedNode(m_globe->focusedNodeIndex());
            return;
        }

        QVector<PeerMapNode> nodes;
        if (m_peer_model) {
            const int rows = m_peer_model->rowCount(QModelIndex());
            nodes.reserve(rows);
            for (int row = 0; row < rows; ++row) {
                const QModelIndex idx = m_peer_model->index(row, PeerTableModel::Address, QModelIndex());
                if (!idx.isValid()) continue;

                PeerMapNode node;
                node.ip = idx.data(PeerTableModel::PeerAddressRole).toString();
                node.city_state = idx.data(PeerTableModel::PeerCityStateRole).toString();
                node.user_agent = idx.data(PeerTableModel::PeerUserAgentRole).toString();
                node.is_local = idx.data(PeerTableModel::PeerIsLocalRole).toBool();
                node.is_lan = idx.data(PeerTableModel::PeerIsLanRole).toBool();
                node.sent = idx.data(PeerTableModel::PeerSentBytesRole).toULongLong();
                node.received = idx.data(PeerTableModel::PeerReceivedBytesRole).toULongLong();
                const QVariant lat = idx.data(PeerTableModel::PeerLatitudeRole);
                const QVariant lon = idx.data(PeerTableModel::PeerLongitudeRole);
                node.has_coordinates = lat.isValid() && lon.isValid();
                if (node.has_coordinates) {
                    node.latitude = lat.toDouble();
                    node.longitude = lon.toDouble();
                }
                if ((node.is_local || node.is_lan) && !node.has_coordinates) {
                    if (m_wan_location_valid) {
                        node.has_coordinates = true;
                        node.latitude = m_wan_latitude;
                        node.longitude = m_wan_longitude;
                        if (node.city_state.isEmpty()) node.city_state = m_wan_city_state;
                    } else {
                        maybeRefreshWanLocation();
                    }
                }
                if (!node.has_coordinates) {
                    continue;
                }
                if (node.is_local && node.city_state.isEmpty()) {
                    node.city_state = tr("Self");
                } else if (node.is_lan && node.city_state.isEmpty()) {
                    node.city_state = tr("LAN");
                } else if (node.city_state.isEmpty()) {
                    continue;
                }
                nodes.push_back(node);
            }
        }

        m_current_nodes = nodes;
        m_globe->setNodes(nodes);
        if (m_fullscreen_dialog) {
            m_fullscreen_dialog->setNodes(nodes, m_fullscreen_dialog->viewState());
        }
        populateNodeList(nodes);
        highlightFocusedNode(m_globe->focusedNodeIndex());
    }

    void populateNodeList(const QVector<PeerMapNode>& nodes)
    {
        const int old_route_width = m_node_list->columnWidth(0);
        const int old_ip_width = m_node_list->columnWidth(1);
        const int old_city_width = m_node_list->columnWidth(2);
        const int old_scroll = m_node_list->horizontalScrollBar() ? m_node_list->horizontalScrollBar()->value() : 0;
        m_sidebar_route_nodes = m_globe->routeOrder();
        const QString source_column = m_data_source == PeerMapDataSource::RealNodes ? tr("IP") :
                                      (m_data_source == PeerMapDataSource::DefconMeetups ? tr("Group") : tr("Data Center"));
        m_node_list->setHorizontalHeaderLabels(QStringList() << tr("Route")
                                                             << source_column
                                                             << tr("City, St"));
        m_sidebar_title->setText(m_data_source == PeerMapDataSource::RealNodes ? tr("Traveling Found Node Locations") :
                                 (m_data_source == PeerMapDataSource::DefconMeetups ? tr("DC34 Meetup Route") : tr("Popular Data Centers Around the World")));
        m_node_list->setRowCount(m_sidebar_route_nodes.size());
        for (int row = 0; row < m_sidebar_route_nodes.size(); ++row) {
            const int node_index = m_sidebar_route_nodes.at(row);
            const PeerMapNode node = nodes.value(node_index);

            QTableWidgetItem* route_item = new QTableWidgetItem(QString::number(row + 1));
            QTableWidgetItem* ip_item = new QTableWidgetItem(node.ip);
            QTableWidgetItem* city_item = new QTableWidgetItem(node.city_state);
            ip_item->setToolTip(node.ip);
            city_item->setToolTip(node.website.isEmpty() ? node.city_state : tr("%1\n%2").arg(node.city_state, node.website));
            route_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            ip_item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            city_item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            route_item->setFlags(route_item->flags() & ~Qt::ItemIsEditable);
            ip_item->setFlags(ip_item->flags() & ~Qt::ItemIsEditable);
            city_item->setFlags(city_item->flags() & ~Qt::ItemIsEditable);
            m_node_list->setItem(row, 0, route_item);
            m_node_list->setItem(row, 1, ip_item);
            m_node_list->setItem(row, 2, city_item);
        }
        if (!m_node_list_default_widths_applied) {
            m_node_list->resizeColumnToContents(0);
            if (m_data_source == PeerMapDataSource::RealNodes) {
                const int ip_width = std::max(150, std::min(260, fontMetricHorizontalAdvance(m_node_list->fontMetrics(), QStringLiteral("2600:3c00::f03c:92ff")) + 22));
                m_node_list->setColumnWidth(1, ip_width);
            } else {
                m_node_list->resizeColumnToContents(1);
                m_node_list->setColumnWidth(1, std::min(260, std::max(130, m_node_list->columnWidth(1) + 12)));
            }
            m_node_list->resizeColumnToContents(2);
            m_node_list->setColumnWidth(2, std::min(260, std::max(130, m_node_list->columnWidth(2) + 12)));
            m_node_list_default_widths_applied = true;
        } else {
            m_node_list->setColumnWidth(0, old_route_width);
            m_node_list->setColumnWidth(1, old_ip_width);
            m_node_list->setColumnWidth(2, old_city_width);
        }
        if (m_node_list->horizontalScrollBar()) {
            m_node_list->horizontalScrollBar()->setValue(old_scroll);
        }
    }

    void highlightFocusedNode(int node_index)
    {
        if (!m_node_list || node_index < 0) {
            if (m_node_list) m_node_list->clearSelection();
            return;
        }

        const int route_row = m_sidebar_route_nodes.indexOf(node_index);
        if (route_row < 0) {
            m_node_list->clearSelection();
            return;
        }
        m_node_list->setCurrentCell(route_row, 0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        // Keep the highlighted row current without stealing the user's scrollbar position.
    }

    PeerTableModel* m_peer_model{nullptr};
    QSplitter* m_splitter{nullptr};
    QWidget* m_sidebar{nullptr};
    QLabel* m_sidebar_title{nullptr};
    QToolButton* m_collapse_button{nullptr};
    QCheckBox* m_lookup_locations{nullptr};
    QWidget* m_source_panel{nullptr};
    QButtonGroup* m_source_group{nullptr};
    QRadioButton* m_real_nodes_radio{nullptr};
    QRadioButton* m_defcon_meetups_radio{nullptr};
    QRadioButton* m_demo_locations_radio{nullptr};
    QNetworkAccessManager* m_wan_location_manager{nullptr};
    QTableWidget* m_node_list{nullptr};
    QWidget* m_globe_panel{nullptr};
    QVBoxLayout* m_globe_layout{nullptr};
    QToolButton* m_fullscreen_button{nullptr};
    PeerGlobeCanvas* m_globe{nullptr};
    PeerMapFullscreenDialog* m_fullscreen_dialog{nullptr};
    QTimer m_refresh_timer;
    QVector<int> m_sidebar_route_nodes;
    QString m_demo_key_buffer;
    QString m_wan_city_state;
    double m_wan_latitude{0.0};
    double m_wan_longitude{0.0};
    bool m_wan_location_pending{false};
    bool m_wan_location_valid{false};
    bool m_sidebar_collapsed{false};
    bool m_node_list_default_widths_applied{false};
    bool m_demo_mode{false};
    PeerMapDataSource m_data_source{PeerMapDataSource::RealNodes};
    QVector<PeerMapNode> m_current_nodes;
    std::function<void()> m_demo_toggle_callback;
};
} // namespace
#endif // ENABLE_DEFCOIN_FUN_UI

QString adaptiveHeaderDisplayText(QString text)
{
    text = text.trimmed();
    if (text == QObject::tr("Node ID") ||
        text == QStringLiteral("NodeId") ||
        text == QStringLiteral("NodeID")) {
        return QObject::tr("Node\nID");
    }
    return text;
}

QColor themedTableGridColor(const QPalette& fallback_palette)
{
    const QString theme = GUIUtil::appearanceThemeBaseStyle(
        GUIUtil::normalizeAppearanceTheme(QSettings().value(QStringLiteral("appearanceTheme")).toString()));
    if (theme == QStringLiteral("light")) return QColor(QStringLiteral("#aebccc"));
#if ENABLE_DEFCOIN_FUN_UI
    if (theme == QStringLiteral("34")) return QColor(QStringLiteral("#2b5a60"));
    if (theme == QStringLiteral("modern")) return QColor(176, 198, 222, 150);
    if (theme == QStringLiteral("YaM") || theme == QStringLiteral("yam")) return QColor(QStringLiteral("#222222"));
#endif
    if (theme == QStringLiteral("dark") || theme == QStringLiteral("auto")) return QColor(QStringLiteral("#52677f"));
    return fallback_palette.mid().color();
}

class AdaptiveHeaderView final : public QHeaderView
{
public:
    explicit AdaptiveHeaderView(Qt::Orientation orientation, QWidget* parent = nullptr)
        : QHeaderView(orientation, parent)
    {
        setProperty("defcoinAdaptiveHeader", true);
        setDefaultAlignment(Qt::AlignCenter);
        setSectionsClickable(true);
        connect(this, &QHeaderView::sectionResized, this, [this] { refreshHeight(); });
    }

    void refreshHeight()
    {
        if (orientation() != Qt::Horizontal || !model()) return;

        QFont header_font = font();
        header_font.setBold(true);
        const QFontMetrics metrics(header_font);
        const int vertical_pad = property("defcoinCompactRows").toBool() ? 4 : 8;
        int height = metrics.height() + vertical_pad;
        for (int visual = 0; visual < count(); ++visual) {
            const int logical = logicalIndex(visual);
            if (isSectionHidden(logical)) continue;
            const QString text = adaptiveHeaderDisplayText(model()->headerData(logical, Qt::Horizontal, Qt::DisplayRole).toString());
            const int sort_room = isSortIndicatorShown() && sortIndicatorSection() == logical ? 14 : 0;
            const bool fits = fontMetricHorizontalAdvance(metrics, text) <= std::max(8, sectionSize(logical) - 10 - sort_room);
            const int explicit_lines = text.count(QLatin1Char('\n')) + 1;
            const int line_count = std::max(explicit_lines, fits ? 1 : 2);
            height = std::max(height, line_count * metrics.height() + vertical_pad);
        }
        setMinimumHeight(height);
        setMaximumHeight(height);
    }

protected:
    void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override
    {
        if (!rect.isValid()) return;

        painter->save();
        const QPalette pal = palette();
        painter->fillRect(rect, pal.button());
        QColor border = themedTableGridColor(pal);
        border.setAlpha(255);
        painter->setPen(QPen(border, 1.2));
        const QRect border_rect = rect.adjusted(0, 0, -1, -1);

        QFont header_font = font();
        header_font.setBold(true);
        painter->setFont(header_font);
        painter->setPen(pal.buttonText().color());

        const QString text = model() ? adaptiveHeaderDisplayText(model()->headerData(logicalIndex, Qt::Horizontal, Qt::DisplayRole).toString()) : QString();
        const int sort_room = isSortIndicatorShown() && sortIndicatorSection() == logicalIndex ? 14 : 0;
        const int vertical_margin = property("defcoinCompactRows").toBool() ? 1 : 2;
        const QRect text_rect = rect.adjusted(4, vertical_margin, -4 - sort_room, -vertical_margin);
        QTextOption option(Qt::AlignCenter);
        option.setWrapMode(fontMetricHorizontalAdvance(QFontMetrics(header_font), text) <= text_rect.width() ? QTextOption::NoWrap : QTextOption::WordWrap);
        painter->drawText(text_rect, text, option);

        if (isSortIndicatorShown() && sortIndicatorSection() == logicalIndex) {
            const int arrow_w = 8;
            const int cx = rect.right() - arrow_w - 3;
            const int cy = rect.center().y();
            QPolygon arrow;
            if (sortIndicatorOrder() == Qt::AscendingOrder) {
                arrow << QPoint(cx, cy + 3) << QPoint(cx + arrow_w, cy + 3) << QPoint(cx + arrow_w / 2, cy - 3);
            } else {
                arrow << QPoint(cx, cy - 3) << QPoint(cx + arrow_w, cy - 3) << QPoint(cx + arrow_w / 2, cy + 3);
            }
            painter->setBrush(pal.buttonText());
            painter->setPen(Qt::NoPen);
            painter->drawPolygon(arrow);
        }

        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(border, 1.2));
        painter->drawRect(border_rect);
        painter->drawLine(border_rect.left(), border_rect.top(), border_rect.left(), border_rect.bottom());
        painter->drawLine(border_rect.right(), border_rect.top(), border_rect.right(), border_rect.bottom());
        painter->drawLine(border_rect.left(), border_rect.top(), border_rect.right(), border_rect.top());
        painter->drawLine(border_rect.left(), border_rect.bottom(), border_rect.right(), border_rect.bottom());

        painter->restore();
    }
};

void refreshAdaptiveHeader(QTableView* table)
{
    if (!table) return;
    QHeaderView* header = table->horizontalHeader();
    if (header && header->property("defcoinAdaptiveHeader").toBool()) {
        static_cast<AdaptiveHeaderView*>(header)->refreshHeight();
    }
}

void clampWindowToAvailableScreen(QWidget* window, const QSize& default_size, const QSize& minimum_size)
{
    if (!window) return;

    window->setMinimumSize(minimum_size);

    QScreen* screen = nullptr;
    const QPoint window_center = window->frameGeometry().center();
    for (QScreen* candidate : QGuiApplication::screens()) {
        if (candidate && candidate->availableGeometry().contains(window_center)) {
            screen = candidate;
            break;
        }
    }
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) {
        window->resize(default_size.expandedTo(minimum_size));
        return;
    }

    const QRect available = screen->availableGeometry().adjusted(24, 24, -24, -24);
    const QSize effective_minimum(std::min(minimum_size.width(), available.width()),
                                  std::min(minimum_size.height(), available.height()));
    window->setMinimumSize(effective_minimum);
    QSize target_size = window->size();
    const bool restored_geometry_too_large =
        target_size.width() > static_cast<int>(available.width() * 0.84) ||
        target_size.height() > static_cast<int>(available.height() * 0.84);
    if (restored_geometry_too_large) {
        target_size = default_size;
    }

    const QSize bounded_size = target_size
        .boundedTo(available.size())
        .expandedTo(effective_minimum);
    window->resize(bounded_size);

    QRect frame = window->frameGeometry();
    if (!available.contains(frame)) {
        frame.moveCenter(available.center());
        if (frame.left() < available.left()) frame.moveLeft(available.left());
        if (frame.top() < available.top()) frame.moveTop(available.top());
        if (frame.right() > available.right()) frame.moveRight(available.right());
        if (frame.bottom() > available.bottom()) frame.moveBottom(available.bottom());
        window->move(frame.topLeft());
    }
}

}

/* Object for executing console RPC commands in a separate thread.
*/
class RPCExecutor : public QObject
{
    Q_OBJECT
public:
    explicit RPCExecutor(interfaces::Node& node) : m_node(node) {}

public Q_SLOTS:
    void request(const QString &command, const WalletModel* wallet_model);

Q_SIGNALS:
    void reply(int category, const QString &command);

private:
    interfaces::Node& m_node;
};

/** Class for handling RPC timers
 * (used for e.g. re-locking the wallet after a timeout)
 */
class QtRPCTimerBase: public QObject, public RPCTimerBase
{
    Q_OBJECT
public:
    QtRPCTimerBase(std::function<void()>& _func, int64_t millis):
        func(_func)
    {
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, [this]{ func(); });
        timer.start(millis);
    }
    ~QtRPCTimerBase() {}
private:
    QTimer timer;
    std::function<void()> func;
};

class QtRPCTimerInterface: public RPCTimerInterface
{
public:
    ~QtRPCTimerInterface() {}
    const char *Name() override { return "Qt"; }
    RPCTimerBase* NewTimer(std::function<void()>& func, int64_t millis) override
    {
        return new QtRPCTimerBase(func, millis);
    }
};


#include <qt/rpcconsole.moc>

/**
 * Split shell command line into a list of arguments and optionally execute the command(s).
 * Aims to emulate \c bash and friends.
 *
 * - Command nesting is possible with parenthesis; for example: validateaddress(getnewaddress())
 * - Arguments are delimited with whitespace or comma
 * - Extra whitespace at the beginning and end and between arguments will be ignored
 * - Text can be "double" or 'single' quoted
 * - The backslash \c \ is used as escape character
 *   - Outside quotes, any character can be escaped
 *   - Within double quotes, only escape \c " and backslashes before a \c " or another backslash
 *   - Within single quotes, no escaping is possible and no special interpretation takes place
 *
 * @param[in]    node    optional node to execute command on
 * @param[out]   strResult   stringified result from the executed command(chain)
 * @param[in]    strCommand  Command line to split
 * @param[in]    fExecute    set true if you want the command to be executed
 * @param[out]   pstrFilteredOut  Command line, filtered to remove any sensitive data
 */

bool RPCConsole::RPCParseCommandLine(interfaces::Node* node, std::string &strResult, const std::string &strCommand, const bool fExecute, std::string * const pstrFilteredOut, const WalletModel* wallet_model)
{
    std::vector< std::vector<std::string> > stack;
    stack.push_back(std::vector<std::string>());

    enum CmdParseState
    {
        STATE_EATING_SPACES,
        STATE_EATING_SPACES_IN_ARG,
        STATE_EATING_SPACES_IN_BRACKETS,
        STATE_ARGUMENT,
        STATE_SINGLEQUOTED,
        STATE_DOUBLEQUOTED,
        STATE_ESCAPE_OUTER,
        STATE_ESCAPE_DOUBLEQUOTED,
        STATE_COMMAND_EXECUTED,
        STATE_COMMAND_EXECUTED_INNER
    } state = STATE_EATING_SPACES;
    std::string curarg;
    UniValue lastResult;
    unsigned nDepthInsideSensitive = 0;
    size_t filter_begin_pos = 0, chpos;
    std::vector<std::pair<size_t, size_t>> filter_ranges;

    auto add_to_current_stack = [&](const std::string& strArg) {
        if (stack.back().empty() && (!nDepthInsideSensitive) && historyFilter.contains(QString::fromStdString(strArg), Qt::CaseInsensitive)) {
            nDepthInsideSensitive = 1;
            filter_begin_pos = chpos;
        }
        // Make sure stack is not empty before adding something
        if (stack.empty()) {
            stack.push_back(std::vector<std::string>());
        }
        stack.back().push_back(strArg);
    };

    auto close_out_params = [&]() {
        if (nDepthInsideSensitive) {
            if (!--nDepthInsideSensitive) {
                assert(filter_begin_pos);
                filter_ranges.push_back(std::make_pair(filter_begin_pos, chpos));
                filter_begin_pos = 0;
            }
        }
        stack.pop_back();
    };

    std::string strCommandTerminated = strCommand;
    if (strCommandTerminated.back() != '\n')
        strCommandTerminated += "\n";
    for (chpos = 0; chpos < strCommandTerminated.size(); ++chpos)
    {
        char ch = strCommandTerminated[chpos];
        switch(state)
        {
            case STATE_COMMAND_EXECUTED_INNER:
            case STATE_COMMAND_EXECUTED:
            {
                bool breakParsing = true;
                switch(ch)
                {
                    case '[': curarg.clear(); state = STATE_COMMAND_EXECUTED_INNER; break;
                    default:
                        if (state == STATE_COMMAND_EXECUTED_INNER)
                        {
                            if (ch != ']')
                            {
                                // append char to the current argument (which is also used for the query command)
                                curarg += ch;
                                break;
                            }
                            if (curarg.size() && fExecute)
                            {
                                // if we have a value query, query arrays with index and objects with a string key
                                UniValue subelement;
                                if (lastResult.isArray())
                                {
                                    for(char argch: curarg)
                                        if (!IsDigit(argch))
                                            throw std::runtime_error("Invalid result query");
                                    subelement = lastResult[atoi(curarg.c_str())];
                                }
                                else if (lastResult.isObject())
                                    subelement = find_value(lastResult, curarg);
                                else
                                    throw std::runtime_error("Invalid result query"); //no array or object: abort
                                lastResult = subelement;
                            }

                            state = STATE_COMMAND_EXECUTED;
                            break;
                        }
                        // don't break parsing when the char is required for the next argument
                        breakParsing = false;

                        // pop the stack and return the result to the current command arguments
                        close_out_params();

                        // don't stringify the json in case of a string to avoid doublequotes
                        if (lastResult.isStr())
                            curarg = lastResult.get_str();
                        else
                            curarg = lastResult.write(2);

                        // if we have a non empty result, use it as stack argument otherwise as general result
                        if (curarg.size())
                        {
                            if (stack.size())
                                add_to_current_stack(curarg);
                            else
                                strResult = curarg;
                        }
                        curarg.clear();
                        // assume eating space state
                        state = STATE_EATING_SPACES;
                }
                if (breakParsing)
                    break;
            }
            case STATE_ARGUMENT: // In or after argument
            case STATE_EATING_SPACES_IN_ARG:
            case STATE_EATING_SPACES_IN_BRACKETS:
            case STATE_EATING_SPACES: // Handle runs of whitespace
                switch(ch)
            {
                case '"': state = STATE_DOUBLEQUOTED; break;
                case '\'': state = STATE_SINGLEQUOTED; break;
                case '\\': state = STATE_ESCAPE_OUTER; break;
                case '(': case ')': case '\n':
                    if (state == STATE_EATING_SPACES_IN_ARG)
                        throw std::runtime_error("Invalid Syntax");
                    if (state == STATE_ARGUMENT)
                    {
                        if (ch == '(' && stack.size() && stack.back().size() > 0)
                        {
                            if (nDepthInsideSensitive) {
                                ++nDepthInsideSensitive;
                            }
                            stack.push_back(std::vector<std::string>());
                        }

                        // don't allow commands after executed commands on baselevel
                        if (!stack.size())
                            throw std::runtime_error("Invalid Syntax");

                        add_to_current_stack(curarg);
                        curarg.clear();
                        state = STATE_EATING_SPACES_IN_BRACKETS;
                    }
                    if ((ch == ')' || ch == '\n') && stack.size() > 0)
                    {
                        if (fExecute) {
                            // Convert argument list to JSON objects in method-dependent way,
                            // and pass it along with the method name to the dispatcher.
                            UniValue params = RPCConvertValues(stack.back()[0], std::vector<std::string>(stack.back().begin() + 1, stack.back().end()));
                            std::string method = stack.back()[0];
                            std::string uri;
#ifdef ENABLE_WALLET
                            if (wallet_model) {
                                QByteArray encodedName = QUrl::toPercentEncoding(wallet_model->getWalletName());
                                uri = "/wallet/"+std::string(encodedName.constData(), encodedName.length());
                            }
#endif
                            assert(node);
                            lastResult = node->executeRpc(method, params, uri);
                        }

                        state = STATE_COMMAND_EXECUTED;
                        curarg.clear();
                    }
                    break;
                case ' ': case ',': case '\t':
                    if(state == STATE_EATING_SPACES_IN_ARG && curarg.empty() && ch == ',')
                        throw std::runtime_error("Invalid Syntax");

                    else if(state == STATE_ARGUMENT) // Space ends argument
                    {
                        add_to_current_stack(curarg);
                        curarg.clear();
                    }
                    if ((state == STATE_EATING_SPACES_IN_BRACKETS || state == STATE_ARGUMENT) && ch == ',')
                    {
                        state = STATE_EATING_SPACES_IN_ARG;
                        break;
                    }
                    state = STATE_EATING_SPACES;
                    break;
                default: curarg += ch; state = STATE_ARGUMENT;
            }
                break;
            case STATE_SINGLEQUOTED: // Single-quoted string
                switch(ch)
            {
                case '\'': state = STATE_ARGUMENT; break;
                default: curarg += ch;
            }
                break;
            case STATE_DOUBLEQUOTED: // Double-quoted string
                switch(ch)
            {
                case '"': state = STATE_ARGUMENT; break;
                case '\\': state = STATE_ESCAPE_DOUBLEQUOTED; break;
                default: curarg += ch;
            }
                break;
            case STATE_ESCAPE_OUTER: // '\' outside quotes
                curarg += ch; state = STATE_ARGUMENT;
                break;
            case STATE_ESCAPE_DOUBLEQUOTED: // '\' in double-quoted text
                if(ch != '"' && ch != '\\') curarg += '\\'; // keep '\' for everything but the quote and '\' itself
                curarg += ch; state = STATE_DOUBLEQUOTED;
                break;
        }
    }
    if (pstrFilteredOut) {
        if (STATE_COMMAND_EXECUTED == state) {
            assert(!stack.empty());
            close_out_params();
        }
        *pstrFilteredOut = strCommand;
        for (auto i = filter_ranges.rbegin(); i != filter_ranges.rend(); ++i) {
            pstrFilteredOut->replace(i->first, i->second - i->first, "(…)");
        }
    }
    switch(state) // final state
    {
        case STATE_COMMAND_EXECUTED:
            if (lastResult.isStr())
                strResult = lastResult.get_str();
            else
                strResult = lastResult.write(2);
        case STATE_ARGUMENT:
        case STATE_EATING_SPACES:
            return true;
        default: // ERROR to end in one of the other states
            return false;
    }
}

void RPCExecutor::request(const QString &command, const WalletModel* wallet_model)
{
    try
    {
        std::string result;
        std::string executableCommand = command.toStdString() + "\n";

        // Catch the console-only-help command before RPC call is executed and reply with help text as-if a RPC reply.
        if(executableCommand == "help-console\n") {
            Q_EMIT reply(RPCConsole::CMD_REPLY, QString(("\n"
                "This console accepts RPC commands using the standard syntax.\n"
                "   example:    getblockhash 0\n\n"

                "This console can also accept RPC commands using the parenthesized syntax.\n"
                "   example:    getblockhash(0)\n\n"

                "Commands may be nested when specified with the parenthesized syntax.\n"
                "   example:    getblock(getblockhash(0) 1)\n\n"

                "A space or a comma can be used to delimit arguments for either syntax.\n"
                "   example:    getblockhash 0\n"
                "               getblockhash,0\n\n"

                "Named results can be queried with a non-quoted key string in brackets using the parenthesized syntax.\n"
                "   example:    getblock(getblockhash(0) 1)[tx]\n\n"

                "Results without keys can be queried with an integer in brackets using the parenthesized syntax.\n"
                "   example:    getblock(getblockhash(0),1)[tx][0]\n\n")));
            return;
        }
        if (!RPCConsole::RPCExecuteCommandLine(m_node, result, executableCommand, nullptr, wallet_model)) {
            Q_EMIT reply(RPCConsole::CMD_ERROR, QString("Parse error: unbalanced ' or \""));
            return;
        }

        Q_EMIT reply(RPCConsole::CMD_REPLY, QString::fromStdString(result));
    }
    catch (UniValue& objError)
    {
        try // Nice formatting for standard-format error
        {
            int code = find_value(objError, "code").get_int();
            std::string message = find_value(objError, "message").get_str();
            Q_EMIT reply(RPCConsole::CMD_ERROR, QString::fromStdString(message) + " (code " + QString::number(code) + ")");
        }
        catch (const std::runtime_error&) // raised when converting to invalid type, i.e. missing code or message
        {   // Show raw JSON object
            Q_EMIT reply(RPCConsole::CMD_ERROR, QString::fromStdString(objError.write()));
        }
    }
    catch (const std::exception& e)
    {
        Q_EMIT reply(RPCConsole::CMD_ERROR, QString("Error: ") + QString::fromStdString(e.what()));
    }
}

RPCConsole::RPCConsole(interfaces::Node& node, const PlatformStyle *_platformStyle, QWidget *parent) :
    QWidget(parent),
    m_node(node),
    ui(new Ui::RPCConsole),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);
    if (layout()) layout()->setSizeConstraint(QLayout::SetNoConstraint);
    setupTrafficStatsSplitter();
    ui->peerWidget->setHorizontalHeader(new AdaptiveHeaderView(Qt::Horizontal, ui->peerWidget));
    ui->banlistWidget->setHorizontalHeader(new AdaptiveHeaderView(Qt::Horizontal, ui->banlistWidget));
    if (ui->verticalLayout_6) {
        ui->verticalLayout_6->setContentsMargins(4, 4, 4, 4);
        ui->verticalLayout_6->setSpacing(3);
    }
    if (ui->verticalLayout_7) {
        ui->verticalLayout_7->setContentsMargins(6, 4, 6, 4);
        ui->verticalLayout_7->setSpacing(5);
        ui->verticalLayout_7->setSizeConstraint(QLayout::SetDefaultConstraint);
    }
    if (ui->splitter) {
        ui->splitter->setChildrenCollapsible(true);
        ui->splitter->setMinimumSize(0, 0);
    }
    if (ui->widget_2) {
        ui->widget_2->setMinimumSize(0, 0);
        ui->widget_2->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    }
    if (ui->widget_1) {
        ui->widget_1->setMinimumSize(0, 0);
        ui->widget_1->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    }
    if (ui->splitter && ui->widget_1) {
        const int widget_index = ui->splitter->indexOf(ui->widget_1);
        if (widget_index >= 0) {
            peerLeftScrollArea = new QScrollArea(ui->splitter);
            peerLeftScrollArea->setObjectName(QStringLiteral("peerLeftScrollArea"));
            peerLeftScrollArea->setFrameShape(QFrame::NoFrame);
            peerLeftScrollArea->setWidgetResizable(true);
            peerLeftScrollArea->setMinimumSize(0, 0);
            peerLeftScrollArea->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
            peerLeftScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            peerLeftScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            ui->widget_1->setParent(nullptr);
            peerLeftScrollArea->setWidget(ui->widget_1);
            ui->splitter->insertWidget(widget_index, peerLeftScrollArea);
        }
    }
    if (ui->horizontalLayoutPeerView) {
        ui->horizontalLayoutPeerView->setContentsMargins(0, 0, 0, 0);
        ui->horizontalLayoutPeerView->setSpacing(5);
    }
    if (ui->horizontalLayoutPeerPingDensity) {
        ui->horizontalLayoutPeerPingDensity->setContentsMargins(0, 0, 0, 0);
        ui->horizontalLayoutPeerPingDensity->setSpacing(6);
        if (ui->verticalLayout_7) {
            for (int i = 0; i < ui->verticalLayout_7->count(); ++i) {
                QLayoutItem* item = ui->verticalLayout_7->itemAt(i);
                if (item && item->layout() == ui->horizontalLayoutPeerPingDensity) {
                    QLayoutItem* density_item = ui->verticalLayout_7->takeAt(i);
                    ui->verticalLayout_7->addItem(density_item);
                    break;
                }
            }
        }
    }
    ui->peerServices->setWordWrap(true);
    ui->label_4->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->peerServices->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->peerServices->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    QSettings settings;
    const QSize default_console_size(1240, 640);
    if (!restoreGeometry(settings.value("RPCConsoleWindowGeometry").toByteArray())) {
        resize(default_console_size);
    }
    clampWindowToAvailableScreen(this, default_console_size, QSize(640, 400));
    restorePeerPanelLayout();
    connect(ui->splitter, &QSplitter::splitterMoved, this, &RPCConsole::savePeerPanelLayout);
    connect(ui->splitter, &QSplitter::splitterMoved, this, &RPCConsole::updatePeerOverviewGeometry);
    connect(ui->splitter, &QSplitter::splitterMoved, this, &RPCConsole::alignPeerDetailPane);

    QChar nonbreaking_hyphen(8209);
    ui->dataDir->setToolTip(ui->dataDir->toolTip().arg(QString(nonbreaking_hyphen) + "datadir"));
    ui->blocksDir->setToolTip(ui->blocksDir->toolTip().arg(QString(nonbreaking_hyphen) + "blocksdir"));
    ui->openDebugLogfileButton->setToolTip(ui->openDebugLogfileButton->toolTip().arg(PACKAGE_NAME));
    ui->labelDebugLogfile->setAlignment(Qt::AlignCenter);
    ui->openDebugLogfileButton->setMinimumHeight(32);
    ui->openDebugLogfileButton->setMinimumWidth(96);
    ui->labelDebugLogfile->hide();
    ui->openDebugLogfileButton->hide();

    debugLogTab = new QWidget(this);
    debugLogTab->setObjectName(QStringLiteral("tab_debug_log"));
    QVBoxLayout* debug_log_layout = new QVBoxLayout(debugLogTab);
    debug_log_layout->setContentsMargins(14, 14, 14, 14);
    debug_log_layout->setSpacing(10);
    QPushButton* open_log_file_button = new QPushButton(tr("Open Full Log"), debugLogTab);
    open_log_file_button->setObjectName(QStringLiteral("openLogFileButton"));
    open_log_file_button->setToolTip(ui->openDebugLogfileButton->toolTip());
    open_log_file_button->setMinimumHeight(32);
    open_log_file_button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    if (platformStyle->getImagesOnButtons()) {
        open_log_file_button->setIcon(platformStyle->SingleColorIcon(":/icons/export"));
    }
    bool session_offset_ok = false;
    const QVariant session_start = QCoreApplication::instance()->property("defcoinDebugLogSessionStartSize");
    debugLogSessionStartSize = session_start.toLongLong(&session_offset_ok);
    if (!session_offset_ok) {
        QFile debug_log_file(debugLogFilePath());
        debugLogSessionStartSize = debug_log_file.exists() ? debug_log_file.size() : 0;
    }
    debugLogLastSize = -1;
    QLabel* debug_log_info = new QLabel(tr("Showing debug.log records written since this app launch. Open Full Log opens the complete debug.log in the system log viewer."), debugLogTab);
    debug_log_info->setWordWrap(true);
    debug_log_info->setTextInteractionFlags(Qt::TextSelectableByMouse);
    debugLogView = new QPlainTextEdit(debugLogTab);
    debugLogView->setObjectName(QStringLiteral("debugLogView"));
    debugLogView->setReadOnly(true);
    debugLogView->setLineWrapMode(QPlainTextEdit::NoWrap);
    debugLogView->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    QFont debug_log_font = GUIUtil::fixedPitchFont();
    debug_log_font.setPointSize(std::max(10, debug_log_font.pointSize()));
    debugLogView->setFont(debug_log_font);
    debugLogView->setPlaceholderText(tr("debug.log is not available yet."));
    debugLogView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    debugLogSearchEdit = new QLineEdit(debugLogTab);
    debugLogSearchEdit->setObjectName(QStringLiteral("debugLogSearchEdit"));
    debugLogSearchEdit->setPlaceholderText(tr("Search debug log"));
    debugLogSearchEdit->setClearButtonEnabled(true);
    debugLogSearchEdit->setMinimumWidth(fontMetricHorizontalAdvance(QFontMetrics(font()), tr("Search debug log")) + 120);
    QPushButton* debug_search_previous = new QPushButton(tr("Previous"), debugLogTab);
    QPushButton* debug_search_next = new QPushButton(tr("Next"), debugLogTab);
    debug_search_previous->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    debug_search_next->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    auto find_debug_text = [this](QTextDocument::FindFlags flags) {
        if (!debugLogView || !debugLogSearchEdit) return;
        const QString needle = debugLogSearchEdit->text();
        if (needle.trimmed().isEmpty()) return;
        if (debugLogView->find(needle, flags)) return;
        QTextCursor cursor = debugLogView->textCursor();
        cursor.movePosition(flags.testFlag(QTextDocument::FindBackward) ? QTextCursor::End : QTextCursor::Start);
        debugLogView->setTextCursor(cursor);
        debugLogView->find(needle, flags);
    };
    QShortcut* debug_find_shortcut = new QShortcut(QKeySequence::Find, debugLogTab);
    debug_find_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(debug_find_shortcut, &QShortcut::activated, debugLogTab, [this] {
        if (!debugLogSearchEdit) return;
        debugLogSearchEdit->setFocus();
        debugLogSearchEdit->selectAll();
    });
    connect(debugLogSearchEdit, &QLineEdit::returnPressed, debugLogTab, [find_debug_text] {
        find_debug_text(QTextDocument::FindFlags());
    });
    connect(debug_search_previous, &QPushButton::clicked, debugLogTab, [find_debug_text] {
        find_debug_text(QTextDocument::FindFlags(QTextDocument::FindBackward));
    });
    connect(debug_search_next, &QPushButton::clicked, debugLogTab, [find_debug_text] {
        find_debug_text(QTextDocument::FindFlags());
    });
    debugLogRefreshTimer = new QTimer(this);
    debugLogRefreshTimer->setInterval(2000);
    connect(debugLogRefreshTimer, &QTimer::timeout, this, [this] { refreshDebugLogView(false); });
    QHBoxLayout* debug_log_toolbar = new QHBoxLayout();
    debug_log_toolbar->setContentsMargins(0, 0, 0, 0);
    debug_log_toolbar->setSpacing(8);
    debug_log_toolbar->addWidget(open_log_file_button);
    debug_log_toolbar->addStretch();
    debug_log_toolbar->addWidget(debugLogSearchEdit);
    debug_log_toolbar->addWidget(debug_search_previous);
    debug_log_toolbar->addWidget(debug_search_next);
    debug_log_layout->addLayout(debug_log_toolbar);
    debug_log_layout->addWidget(debug_log_info);
    debug_log_layout->addWidget(debugLogView, 1);
    const int info_tab_index = ui->tabWidget->indexOf(ui->tab_info);
    ui->tabWidget->insertTab(info_tab_index + 1, debugLogTab, tr("Debug Log File"));
    connect(open_log_file_button, &QPushButton::clicked, this, &RPCConsole::on_openDebugLogfileButton_clicked);

    if (platformStyle->getImagesOnButtons()) {
        ui->openDebugLogfileButton->setIcon(platformStyle->SingleColorIcon(":/icons/export"));
    }
    ui->clearButton->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
    ui->fontBiggerButton->setIcon(platformStyle->SingleColorIcon(":/icons/fontbigger"));
    ui->fontSmallerButton->setIcon(platformStyle->SingleColorIcon(":/icons/fontsmaller"));
    ui->fontBiggerButton->hide();
    ui->fontSmallerButton->hide();

    // Install event filter for up and down arrow
    ui->lineEdit->installEventFilter(this);
    ui->lineEdit->setMaxLength(16 * 1024 * 1024);
    ui->messagesWidget->installEventFilter(this);

    connect(ui->clearButton, &QPushButton::clicked, this, &RPCConsole::clear);
    connect(ui->btnClearTrafficGraph, &QPushButton::clicked, this, [this] {
        QSettings settings;
        const int default_font_size = QApplication::font().pointSize();
        settings.setValue(QStringLiteral("mainWindowFontSize"), default_font_size);
        settings.setValue(QStringLiteral("nodeWindowFontSize"), default_font_size);
        settings.setValue(QStringLiteral("connectedPeersPanelFontSize"), default_font_size);
        settings.setValue(fontSizeSettingsKey, default_font_size);
        settings.remove(QStringLiteral("pingToolFontSize"));
        settings.remove(QStringLiteral("traceRouteFontSize"));
        nodeWindowFontSize = default_font_size;
        connectedPeersPanelFontSize = default_font_size;
        applyNodeWindowFont();
        applyConnectedPeersPanelFont();
        setFontSize(default_font_size);
        for (QWidget* top_level : QApplication::topLevelWidgets()) {
            if (!top_level || top_level == this) continue;
            if (top_level->metaObject()->indexOfMethod("refreshTextSizeSettings()") >= 0) {
                QMetaObject::invokeMethod(top_level, "refreshTextSizeSettings", Qt::QueuedConnection);
            }
        }
        ui->trafficGraph->clear();
        ui->trafficGraph->setGraphRangeSeconds(INITIAL_TRAFFIC_GRAPH_MINS * 60);
        ui->trafficGraph->setPanPercent(1000);
        if (trafficPanSlider) {
            QSignalBlocker blocker(trafficPanSlider);
            trafficPanSlider->setValue(1000);
        }
        if (trafficAllHistoryCheckBox) {
            QSignalBlocker blocker(trafficAllHistoryCheckBox);
            trafficAllHistoryCheckBox->setChecked(false);
        }
        if (ui->sldGraphRange) {
            QSignalBlocker blocker(ui->sldGraphRange);
            ui->sldGraphRange->setValue(TRAFFIC_RANGE_SLIDER_MAX);
        }
        if (trafficWindowSlider) {
            QSignalBlocker blocker(trafficWindowSlider);
            trafficWindowSlider->setValue(trafficWindowSliderValueForSeconds(INITIAL_TRAFFIC_GRAPH_MINS * 60));
        }
        lastTrafficSlider = ui->sldGraphRange;
        setTrafficGraphScale(TRAFFIC_RANGE_SLIDER_MAX, true);
    });
    const QString traffic_slider_style = QStringLiteral(
        "QSlider::groove:horizontal { height: 6px; border-radius: 3px; background: #303842; }"
        "QSlider::sub-page:horizontal { height: 6px; border-radius: 3px; background: #5ea9ff; }"
        "QSlider::handle:horizontal { width: 18px; height: 18px; margin: -7px 0; border-radius: 9px; background: #e6edf5; border: 1px solid #52677f; }"
        "QSlider::groove:horizontal:disabled { background: #d9dde2; }"
        "QSlider::sub-page:horizontal:disabled { background: #d9dde2; }"
        "QSlider::handle:horizontal:disabled { background: #f6f8fa; border: 1px solid #c8d0d8; }");
    ui->sldGraphRange->setStyleSheet(traffic_slider_style);
    ui->sldGraphRange->setToolTip(tr("Scale the visible network traffic window. Use lower values for a tighter detail view."));
    ui->sldGraphRange->setMinimumWidth(160);
    ui->sldGraphRange->setRange(TRAFFIC_RANGE_SLIDER_MIN, TRAFFIC_RANGE_SLIDER_MAX);
    ui->sldGraphRange->setTickPosition(QSlider::TicksBelow);
    ui->sldGraphRange->setTickInterval(10);
    ui->sldGraphRange->setSingleStep(1);
    ui->sldGraphRange->setPageStep(10);
    ui->sldGraphRange->setMinimumWidth(160);
    ui->sldGraphRange->installEventFilter(this);
    ui->trafficGraph->installEventFilter(this);
    ui->lblGraphRange->installEventFilter(this);
    ui->lblGraphRange->setCursor(Qt::PointingHandCursor);
    {
        QSignalBlocker blocker(ui->sldGraphRange);
        const int saved_scale = std::max(TRAFFIC_RANGE_SLIDER_MIN, std::min(settings.value(QStringLiteral("TrafficGraphScalePercent"), TRAFFIC_RANGE_SLIDER_MAX).toInt(), TRAFFIC_RANGE_SLIDER_MAX));
        ui->sldGraphRange->setValue(saved_scale);
    }
    lastTrafficSlider = ui->sldGraphRange;
    if (ui->horizontalLayout_2) {
        ui->horizontalLayout_2->setSpacing(8);
        ui->horizontalLayout_2->removeWidget(ui->sldGraphRange);
        ui->horizontalLayout_2->removeWidget(ui->lblGraphRange);
        ui->horizontalLayout_2->removeWidget(ui->btnClearTrafficGraph);

        QLabel* traffic_scale_label = new QLabel(tr("Scale"), this);
        traffic_scale_label->setObjectName(QStringLiteral("trafficScaleLabel"));
        traffic_scale_label->setToolTip(ui->sldGraphRange->toolTip());
        ui->lblGraphRange->setToolTip(ui->sldGraphRange->toolTip());
        QWidget* traffic_zoom_widget = new QWidget(this);
        traffic_zoom_widget->setObjectName(QStringLiteral("trafficZoomControl"));
        QHBoxLayout* traffic_zoom_layout = new QHBoxLayout(traffic_zoom_widget);
        traffic_zoom_layout->setContentsMargins(0, 0, 0, 0);
        traffic_zoom_layout->setSpacing(6);
        traffic_zoom_layout->addWidget(traffic_scale_label);
        traffic_zoom_layout->addWidget(ui->sldGraphRange, 1);
        traffic_zoom_layout->addWidget(ui->lblGraphRange);

        QLabel* traffic_window_label = new QLabel(tr("Window"), this);
        traffic_window_label->setObjectName(QStringLiteral("trafficWindowLabel"));
        traffic_window_label->setToolTip(tr("Choose the amount of time represented by the full graph. The final detent shows all retained history."));
        trafficWindowSlider = new QSlider(Qt::Horizontal, this);
        trafficWindowSlider->setObjectName(QStringLiteral("trafficWindowSlider"));
        trafficWindowSlider->setRange(TRAFFIC_WINDOW_SLIDER_MIN, TRAFFIC_WINDOW_SLIDER_ALL);
        trafficWindowSlider->setTickPosition(QSlider::TicksBelow);
        trafficWindowSlider->setTickInterval(100);
        trafficWindowSlider->setSingleStep(10);
        trafficWindowSlider->setPageStep(100);
        trafficWindowSlider->setMinimumWidth(160);
        trafficWindowSlider->setStyleSheet(traffic_slider_style);
        trafficWindowSlider->setToolTip(traffic_window_label->toolTip());
        trafficWindowSlider->installEventFilter(this);
        const bool saved_all_history = settings.value(QStringLiteral("TrafficGraphShowAllHistory"), false).toBool();
        {
            QSignalBlocker blocker(trafficWindowSlider);
            const qint64 saved_window_seconds = settings.value(QStringLiteral("TrafficGraphRangeSeconds"), INITIAL_TRAFFIC_GRAPH_MINS * 60).toLongLong();
            trafficWindowSlider->setValue(saved_all_history
                ? TRAFFIC_WINDOW_SLIDER_ALL
                : trafficWindowSliderValueForSeconds(saved_window_seconds));
        }
        trafficWindowDurationLabel = new QLabel(compactDurationText(std::max<qint64>(1, ui->trafficGraph->graphRangeSeconds())), this);
        trafficWindowDurationLabel->setObjectName(QStringLiteral("trafficWindowDurationLabel"));
        trafficWindowDurationLabel->setAlignment(Qt::AlignCenter);
        trafficWindowDurationLabel->setCursor(Qt::PointingHandCursor);
        trafficWindowDurationLabel->setFixedWidth(fontMetricHorizontalAdvance(QFontMetrics(font()), QStringLiteral("365d23h59m59s")) + 22);
        trafficWindowDurationLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        trafficWindowDurationLabel->setToolTip(tr("Double-click to type a custom graph window such as 30s, 15m, 1h, or 1d2h."));
        trafficWindowDurationLabel->installEventFilter(this);
        QWidget* traffic_window_widget = new QWidget(this);
        traffic_window_widget->setObjectName(QStringLiteral("trafficWindowControl"));
        QHBoxLayout* traffic_window_layout = new QHBoxLayout(traffic_window_widget);
        traffic_window_layout->setContentsMargins(0, 0, 0, 0);
        traffic_window_layout->setSpacing(6);
        traffic_window_layout->addWidget(traffic_window_label);
        traffic_window_layout->addWidget(trafficWindowSlider, 1);
        traffic_window_layout->addWidget(trafficWindowDurationLabel);
        auto make_traffic_button = [this](const QString& text, const QString& tooltip) {
            QPushButton* button = new QPushButton(text, this);
            button->setToolTip(tooltip);
            button->setAutoDefault(false);
            const int width = std::max(54, fontMetricHorizontalAdvance(QFontMetrics(button->font()), text) + 28);
            button->setMinimumWidth(36);
            button->setMaximumWidth(width);
            return button;
        };
        QPushButton* traffic_5m_button = make_traffic_button(tr("5m"), tr("Show a five minute network traffic window."));
        QPushButton* traffic_15m_button = make_traffic_button(tr("15m"), tr("Show a fifteen minute network traffic window."));
        QPushButton* traffic_1h_button = make_traffic_button(tr("1h"), tr("Show a one hour network traffic window."));
        QPushButton* traffic_all_button = make_traffic_button(tr("All"), tr("Show all retained network traffic history."));
#if ENABLE_DEFCOIN_FUN_UI
        QPushButton* traffic_pause_button = make_traffic_button(tr("Pause"), tr("Pause or resume graph animation. Data collection continues while paused."));
        const int pause_width = std::max(fontMetricHorizontalAdvance(QFontMetrics(traffic_pause_button->font()), tr("Resume")),
                                         fontMetricHorizontalAdvance(QFontMetrics(traffic_pause_button->font()), tr("Pause"))) + 40;
        traffic_pause_button->setMinimumWidth(std::max(82, pause_width));
        traffic_pause_button->setMaximumWidth(std::max(82, pause_width));
        QPushButton* traffic_png_button = make_traffic_button(tr("PNG"), tr("Export the current network traffic graph as a PNG image."));
        QPushButton* traffic_csv_button = make_traffic_button(tr("CSV"), tr("Export retained network traffic samples as CSV."));
#endif
        QWidget* traffic_controls_widget = new QWidget(this);
        traffic_controls_widget->setObjectName(QStringLiteral("trafficBottomControls"));
        FlowLayout* traffic_controls_layout = new FlowLayout(traffic_controls_widget, 0, 8, 4);
        traffic_controls_layout->addWidget(traffic_zoom_widget);
        traffic_controls_layout->addWidget(traffic_window_widget);
        traffic_controls_layout->addWidget(traffic_5m_button);
        traffic_controls_layout->addWidget(traffic_15m_button);
        traffic_controls_layout->addWidget(traffic_1h_button);
        traffic_controls_layout->addWidget(traffic_all_button);
#if ENABLE_DEFCOIN_FUN_UI
        traffic_controls_layout->addWidget(traffic_pause_button);
        traffic_controls_layout->addWidget(traffic_png_button);
        traffic_controls_layout->addWidget(traffic_csv_button);
        traffic_controls_layout->addWidget(ui->btnClearTrafficGraph);
#else
        ui->btnClearTrafficGraph->hide();
#endif
        ui->horizontalLayout_2->addWidget(traffic_controls_widget, 1);
        auto apply_window_seconds = [this](int seconds) {
            if (trafficWindowSlider) trafficWindowSlider->setValue(trafficWindowSliderValueForSeconds(seconds));
            ui->trafficGraph->setShowAllHistory(false);
            ui->trafficGraph->setGraphRangeSeconds(seconds);
            setTrafficGraphScale(ui->sldGraphRange->value(), true);
        };
        connect(traffic_5m_button, &QPushButton::clicked, this, [apply_window_seconds] { apply_window_seconds(5 * 60); });
        connect(traffic_15m_button, &QPushButton::clicked, this, [apply_window_seconds] { apply_window_seconds(15 * 60); });
        connect(traffic_1h_button, &QPushButton::clicked, this, [apply_window_seconds] { apply_window_seconds(60 * 60); });
        connect(traffic_all_button, &QPushButton::clicked, this, [this] {
            if (trafficWindowSlider) trafficWindowSlider->setValue(TRAFFIC_WINDOW_SLIDER_ALL);
            ui->trafficGraph->setShowAllHistory(true);
            setTrafficGraphScale(ui->sldGraphRange->value(), true);
        });
#if ENABLE_DEFCOIN_FUN_UI
        connect(traffic_pause_button, &QPushButton::clicked, this, [this, traffic_pause_button] {
            const bool pause = !ui->trafficGraph->animationPaused();
            ui->trafficGraph->setAnimationPaused(pause);
            traffic_pause_button->setText(pause ? tr("Resume") : tr("Pause"));
        });
        connect(traffic_png_button, &QPushButton::clicked, this, [this] {
            QString filename = GUIUtil::getSaveFileName(this, tr("Export Network Traffic Graph"), QString(), tr("PNG Image (*.png)"), nullptr);
            if (filename.isEmpty()) return;
            ui->trafficGraph->grab().save(filename, "PNG");
        });
        connect(traffic_csv_button, &QPushButton::clicked, this, [this] {
            QString filename = GUIUtil::getSaveFileName(this, tr("Export Network Traffic Samples"), QString(), tr("CSV File (*.csv)"), nullptr);
            if (filename.isEmpty()) return;
            QFile file(filename);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.write(ui->trafficGraph->exportSamplesCsv().toUtf8());
            }
        });
#endif
    }
    for (QFrame* line : {ui->line, ui->line_2, ui->lineAvgLatency}) {
        if (!line) continue;
        line->setFrameShape(QFrame::NoFrame);
        line->setFixedSize(44, 6);
        line->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    setMetricSwatchColor(ui->line, ui->trafficGraph->metricColor(TrafficGraphWidget::TrafficMetric::Received));
    setMetricSwatchColor(ui->line_2, ui->trafficGraph->metricColor(TrafficGraphWidget::TrafficMetric::Sent));
    setMetricSwatchColor(ui->lineAvgLatency, ui->trafficGraph->metricColor(TrafficGraphWidget::TrafficMetric::AvgRecentLatency));
    if (ui->horizontalLayout_4) ui->horizontalLayout_4->setSpacing(10);
    if (ui->horizontalLayout_5) ui->horizontalLayout_5->setSpacing(10);
    if (ui->horizontalLayoutAvgLatency) ui->horizontalLayoutAvgLatency->setSpacing(10);
    installNodeTextControls();
    trafficPanSlider = new TrafficWindowSlider(this);
    trafficPanSlider->setObjectName(QStringLiteral("trafficPanSlider"));
    trafficPanSlider->setRange(0, 1000);
    trafficPanSlider->setValue(std::max(0, std::min(settings.value(QStringLiteral("TrafficGraphPanPercent"), 1000).toInt(), 1000)));
    trafficPanSlider->setTickPosition(QSlider::TicksBelow);
    trafficPanSlider->setTickInterval(250);
    trafficPanSlider->setSingleStep(10);
    trafficPanSlider->setPageStep(100);
    trafficPanSlider->setMinimumWidth(220);
    trafficPanSlider->installEventFilter(this);
    trafficPanSlider->setStyleSheet(traffic_slider_style);
    trafficPanSlider->setToolTip(tr("Move the visible traffic window through retained history. Drag the blue rectangle to pan; drag either edge to resize the visible window."));
    static_cast<TrafficWindowSlider*>(trafficPanSlider)->setWindowPercentChangedCallback([this](int percent) {
        if (!ui->sldGraphRange) return;
        if (trafficWindowSlider && trafficWindowSlider->value() >= TRAFFIC_WINDOW_SLIDER_ALL) {
            QSignalBlocker blocker(trafficWindowSlider);
            trafficWindowSlider->setValue(trafficWindowSliderValueForSeconds(std::max<qint64>(1, ui->trafficGraph->graphRangeSeconds())));
            ui->trafficGraph->setShowAllHistory(false);
        }
        lastTrafficSlider = trafficPanSlider;
        ui->sldGraphRange->setValue(std::max(ui->sldGraphRange->minimum(), std::min(ui->sldGraphRange->maximum(), percent)));
    });
    trafficPanWidget = new QWidget(this);
    trafficPanWidget->setObjectName(QStringLiteral("trafficPanWidget"));
    QHBoxLayout* traffic_pan_layout = new QHBoxLayout(trafficPanWidget);
    traffic_pan_layout->setContentsMargins(0, 0, 0, 0);
    traffic_pan_layout->setSpacing(8);
    QLabel* traffic_pan_label = new QLabel(tr("Timeline"), this);
    traffic_pan_label->setObjectName(QStringLiteral("trafficPanLabel"));
    traffic_pan_label->setToolTip(trafficPanSlider->toolTip());
    trafficWindowValueLabel = new QLabel(tr("Present"), this);
    trafficWindowValueLabel->setObjectName(QStringLiteral("trafficWindowValueLabel"));
    trafficWindowValueLabel->setFixedWidth(fontMetricHorizontalAdvance(QFontMetrics(font()), tr("11:43:00 AM - 11:43:00 AM")) + 64);
    trafficWindowValueLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    trafficWindowValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    trafficWindowValueLabel->setToolTip(trafficPanSlider->toolTip());
    traffic_pan_layout->addWidget(traffic_pan_label);
    traffic_pan_layout->addWidget(trafficPanSlider, 1);
    traffic_pan_layout->addWidget(trafficWindowValueLabel);
    ui->verticalLayout_4->insertWidget(1, trafficPanWidget);
    connect(trafficPanSlider, &QSlider::valueChanged, this, [this](int percent) {
        lastTrafficSlider = trafficPanSlider;
        ui->trafficGraph->setPanPercent(percent);
        setTrafficGraphScale(ui->sldGraphRange->value(), false);
    });
    if (trafficWindowSlider) {
        connect(trafficWindowSlider, &QSlider::valueChanged, this, [this](int value) {
            lastTrafficSlider = trafficWindowSlider;
            const bool all_history = value >= TRAFFIC_WINDOW_SLIDER_ALL;
            ui->trafficGraph->setShowAllHistory(all_history);
            if (!all_history) {
                ui->trafficGraph->setGraphRangeSeconds(trafficWindowSecondsForSliderValue(value));
            }
            setTrafficGraphScale(ui->sldGraphRange->value(), true);
        });
    }
#if ENABLE_DEFCOIN_FUN_UI
    peerMapWidget = new PeerMapWidget(this);
    peerMapWidget->installEventFilter(this);
    static_cast<PeerMapWidget*>(peerMapWidget)->setDemoToggleCallback([this] { togglePeerDemoMode(); });
    const int peers_tab_index = ui->tabWidget->indexOf(ui->tab_peers);
    ui->tabWidget->insertTab(peers_tab_index + 1, peerMapWidget, tr("Peer Map"));
#endif

#if ENABLE_DEFCOIN_FUN_UI
    ui->groupBox->setTitle(tr("Network Statistics"));
    ui->groupBox->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    ui->groupBox->setMinimumWidth(300);
    ui->groupBox->setMaximumWidth(360);
    ui->verticalLayout_5->removeItem(ui->verticalSpacer_4);
    ui->label_16->hide();
    ui->label_17->hide();
    ui->chkAvgPeerLatency->setText(tr("Sample Avg Latency"));
    ui->chkAvgPeerLatency->setToolTip(tr("At each chart sample, this is the average internal Defcoin ping time across currently connected peers. It changes sample by sample."));
    trafficReceivedCheckBox = new QCheckBox(tr("Received"), this);
    trafficSentCheckBox = new QCheckBox(tr("Sent"), this);
    trafficAvgRecentLatencyCheckBox = ui->chkAvgPeerLatency;
    trafficPeerAvgLatencyCheckBox = new QCheckBox(tr("Visible Window Avg"), this);
    trafficJitterCheckBox = new QCheckBox(tr("Jitter"), this);
    trafficMovingAverageCheckBox = new QCheckBox(tr("Moving Averages"), this);
    QCheckBox* trafficTotalsAdaptiveScaleCheckBox = new QCheckBox(tr("Adaptive Scale"), this);
    QCheckBox* trafficTotalsLogScaleCheckBox = new QCheckBox(tr("Display Logarithmically"), this);
    QCheckBox* trafficLatencyMovingAverageCheckBox = new QCheckBox(tr("Moving Averages"), this);
    QCheckBox* trafficLatencyAdaptiveScaleCheckBox = new QCheckBox(tr("Adaptive Scale"), this);
    QCheckBox* trafficLatencyLogScaleCheckBox = new QCheckBox(tr("Display Logarithmically"), this);
    QCheckBox* trafficLatencyHideWorst20CheckBox = new QCheckBox(tr("Hide worst 20%"), this);
    QCheckBox* trafficLatencyShowWorst20CheckBox = new QCheckBox(tr("Show only worst 20%"), this);
    trafficTotalsAdaptiveScaleCheckBox->setObjectName(QStringLiteral("trafficTotalsAdaptiveScale"));
    trafficTotalsLogScaleCheckBox->setObjectName(QStringLiteral("trafficTotalsLogScale"));
    trafficLatencyMovingAverageCheckBox->setObjectName(QStringLiteral("trafficLatencyMovingAverage"));
    trafficLatencyAdaptiveScaleCheckBox->setObjectName(QStringLiteral("trafficLatencyAdaptiveScale"));
    trafficLatencyLogScaleCheckBox->setObjectName(QStringLiteral("trafficLatencyLogScale"));
    trafficLatencyHideWorst20CheckBox->setObjectName(QStringLiteral("trafficLatencyHideWorst20"));
    trafficLatencyShowWorst20CheckBox->setObjectName(QStringLiteral("trafficLatencyShowOnlyWorst20"));
    trafficAllHistoryCheckBox = new QCheckBox(tr("All"), this);
    trafficAllHistoryCheckBox->setObjectName(QStringLiteral("trafficAllHistory"));
    trafficAllHistoryCheckBox->setToolTip(tr("Show all traffic history collected while the Node window has been open."));
    trafficReceivedCheckBox->setChecked(QSettings().value(QStringLiteral("TrafficGraphShowReceived"), true).toBool());
    trafficSentCheckBox->setChecked(QSettings().value(QStringLiteral("TrafficGraphShowSent"), true).toBool());
    trafficAvgRecentLatencyCheckBox->setChecked(QSettings().value(QStringLiteral("TrafficGraphShowAvgRecentLatency"), true).toBool());
    trafficPeerAvgLatencyCheckBox->setChecked(QSettings().value(QStringLiteral("TrafficGraphShowPeerAvgLatency"), true).toBool());
    trafficJitterCheckBox->setChecked(QSettings().value(QStringLiteral("TrafficGraphShowJitter"), true).toBool());
    trafficMovingAverageCheckBox->setChecked(QSettings().value(QStringLiteral("TrafficGraphShowTrafficMovingAverages"), QSettings().value(QStringLiteral("TrafficGraphShowMovingAverages"), false).toBool()).toBool());
    trafficTotalsAdaptiveScaleCheckBox->setChecked(QSettings().value(QStringLiteral("TrafficGraphTrafficAdaptiveScale"), true).toBool());
    trafficTotalsLogScaleCheckBox->setChecked(QSettings().value(QStringLiteral("TrafficGraphAlgorithmicScale"), false).toBool());
    trafficLatencyMovingAverageCheckBox->setChecked(QSettings().value(QStringLiteral("TrafficGraphShowLatencyMovingAverages"), QSettings().value(QStringLiteral("TrafficGraphShowMovingAverages"), false).toBool()).toBool());
    trafficLatencyAdaptiveScaleCheckBox->setChecked(QSettings().value(QStringLiteral("TrafficGraphLatencyAdaptiveScale"), true).toBool());
    trafficLatencyLogScaleCheckBox->setChecked(QSettings().value(QStringLiteral("TrafficGraphLatencyAlgorithmicScale"), false).toBool());
    trafficLatencyHideWorst20CheckBox->setChecked(QSettings().value(QStringLiteral("TrafficGraphLatencyHideWorst20"), false).toBool());
    trafficLatencyShowWorst20CheckBox->setChecked(QSettings().value(QStringLiteral("TrafficGraphLatencyShowOnlyWorst20"), false).toBool());
    trafficAllHistoryCheckBox->setChecked(QSettings().value(QStringLiteral("TrafficGraphShowAllHistory"), false).toBool());
    for (QCheckBox* stat_box : {trafficReceivedCheckBox, trafficSentCheckBox, trafficAvgRecentLatencyCheckBox, trafficPeerAvgLatencyCheckBox, trafficJitterCheckBox, trafficMovingAverageCheckBox, trafficTotalsAdaptiveScaleCheckBox, trafficTotalsLogScaleCheckBox, trafficLatencyMovingAverageCheckBox, trafficLatencyAdaptiveScaleCheckBox, trafficLatencyLogScaleCheckBox, trafficLatencyHideWorst20CheckBox, trafficLatencyShowWorst20CheckBox}) {
        stat_box->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    }
    trafficReceivedCheckBox->setToolTip(tr("Show or hide received traffic on the chart."));
    trafficSentCheckBox->setToolTip(tr("Show or hide sent traffic on the chart."));
    trafficAvgRecentLatencyCheckBox->setToolTip(tr("Show or hide the per-sample average of internal Defcoin peer ping responses."));
    trafficPeerAvgLatencyCheckBox->setToolTip(tr("Show or hide one reference line representing the average of all visible latency samples in the current graph window."));
    trafficJitterCheckBox->setToolTip(tr("Show or hide variation between recent internal Defcoin peer ping times."));
    trafficMovingAverageCheckBox->setToolTip(tr("Show dashed moving-average lines for received and sent traffic."));
    trafficLatencyMovingAverageCheckBox->setToolTip(tr("Show dashed moving-average lines for latency metrics."));
    trafficTotalsAdaptiveScaleCheckBox->setToolTip(tr("Automatically scales the vertical axis to visible traffic data."));
    trafficLatencyAdaptiveScaleCheckBox->setToolTip(tr("Automatically scales the vertical axis to visible latency data."));
    trafficTotalsLogScaleCheckBox->setToolTip(tr("Use a logarithmic vertical scale for traffic."));
    trafficLatencyLogScaleCheckBox->setToolTip(tr("Use a logarithmic vertical scale for latency."));
    trafficLatencyHideWorst20CheckBox->setToolTip(tr("Hide the slowest 20% of latency samples from the latency overlay so routine network quality is easier to see at a glance. Adaptive scaling is recalculated from the remaining samples."));
    trafficLatencyShowWorst20CheckBox->setToolTip(tr("Show only the slowest 20% of latency samples. This highlights spikes and routing trouble, but it does not mean the whole connection is that slow."));
    trafficAllHistoryCheckBox->hide();
    ui->line->hide();
    ui->line_2->hide();
    ui->lineAvgLatency->hide();
    ui->horizontalLayout_4->removeWidget(ui->line);
    ui->horizontalLayout_5->removeWidget(ui->line_2);
    ui->horizontalLayoutAvgLatency->removeWidget(ui->lineAvgLatency);
    ui->horizontalLayout_4->setSpacing(10);
    ui->horizontalLayout_5->setSpacing(10);
    ui->horizontalLayoutAvgLatency->setSpacing(10);
    trafficReceivedSwatch = makeMetricSwatch(this);
    trafficSentSwatch = makeMetricSwatch(this);
    trafficAvgRecentLatencySwatch = makeMetricSwatch(this);
    trafficPeerAvgLatencySwatch = makeMetricSwatch(this);
    trafficJitterSwatch = makeMetricSwatch(this);
    ui->horizontalLayout_4->insertWidget(0, trafficReceivedSwatch);
    ui->horizontalLayout_4->insertWidget(1, trafficReceivedCheckBox);
    ui->horizontalLayout_5->insertWidget(0, trafficSentSwatch);
    ui->horizontalLayout_5->insertWidget(1, trafficSentCheckBox);
    ui->horizontalLayoutAvgLatency->insertWidget(0, trafficAvgRecentLatencySwatch);
    QLabel* traffic_totals_heading = makePanelHeading(tr("Traffic Totals"));
    traffic_totals_heading->setWordWrap(true);
    ui->verticalLayout_5->insertWidget(0, traffic_totals_heading);
    trafficAvgLatencyLabel = new QLabel(tr("N/A"), this);
    trafficAvgLatencyLabel->setMinimumWidth(54);
    trafficAvgLatencyLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->horizontalLayoutAvgLatency->addWidget(trafficAvgLatencyLabel);
    trafficPeerAvgLatencyLabel = new QLabel(tr("N/A"), this);
    trafficPeerAvgLatencyLabel->setMinimumWidth(54);
    trafficPeerAvgLatencyLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QHBoxLayout* peer_avg_latency_layout = new QHBoxLayout();
    peer_avg_latency_layout->setContentsMargins(0, 0, 0, 0);
    peer_avg_latency_layout->setSpacing(8);
    peer_avg_latency_layout->addWidget(trafficPeerAvgLatencySwatch);
    peer_avg_latency_layout->addWidget(trafficPeerAvgLatencyCheckBox);
    peer_avg_latency_layout->addStretch();
    peer_avg_latency_layout->addWidget(trafficPeerAvgLatencyLabel);
    ui->verticalLayout_5->insertLayout(7, peer_avg_latency_layout);
    trafficJitterLabel = new QLabel(tr("N/A"), this);
    trafficJitterLabel->setMinimumWidth(54);
    trafficJitterLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QHBoxLayout* jitter_layout = new QHBoxLayout();
    jitter_layout->setContentsMargins(0, 0, 0, 0);
    jitter_layout->setSpacing(8);
    jitter_layout->addWidget(trafficJitterSwatch);
    jitter_layout->addWidget(trafficJitterCheckBox);
    jitter_layout->addStretch();
    jitter_layout->addWidget(trafficJitterLabel);
    ui->verticalLayout_5->insertLayout(8, jitter_layout);
    QVBoxLayout* traffic_totals_options_layout = new QVBoxLayout();
    traffic_totals_options_layout->setContentsMargins(0, 0, 0, 0);
    traffic_totals_options_layout->setSpacing(8);
    traffic_totals_options_layout->addWidget(trafficMovingAverageCheckBox);
    traffic_totals_options_layout->addWidget(trafficTotalsAdaptiveScaleCheckBox);
    traffic_totals_options_layout->addWidget(trafficTotalsLogScaleCheckBox);
    ui->verticalLayout_5->insertLayout(3, traffic_totals_options_layout);
    QFrame* latency_separator = new QFrame(this);
    latency_separator->setFrameShape(QFrame::HLine);
    latency_separator->setFrameShadow(QFrame::Sunken);
    ui->verticalLayout_5->insertWidget(4, latency_separator);
    QLabel* latency_heading = makePanelHeading(tr("Latency"));
    latency_heading->setWordWrap(true);
    ui->verticalLayout_5->insertWidget(5, latency_heading);
    QVBoxLayout* latency_options_layout = new QVBoxLayout();
    latency_options_layout->setContentsMargins(0, 0, 0, 0);
    latency_options_layout->setSpacing(8);
    latency_options_layout->addWidget(trafficLatencyMovingAverageCheckBox);
    latency_options_layout->addWidget(trafficLatencyAdaptiveScaleCheckBox);
    latency_options_layout->addWidget(trafficLatencyLogScaleCheckBox);
    latency_options_layout->addWidget(trafficLatencyHideWorst20CheckBox);
    latency_options_layout->addWidget(trafficLatencyShowWorst20CheckBox);
    ui->verticalLayout_5->insertLayout(10, latency_options_layout);
    QHBoxLayout* moving_average_layout = new QHBoxLayout();
    moving_average_layout->setContentsMargins(0, 0, 0, 0);
    moving_average_layout->addWidget(new QLabel(tr("Average window:"), this));
    QSpinBox* moving_average_window = new QSpinBox(this);
    moving_average_window->setObjectName(QStringLiteral("trafficMovingAverageWindow"));
    moving_average_window->setRange(1, 240);
    moving_average_window->setValue(QSettings().value(QStringLiteral("TrafficGraphMovingAverageWindowMins"), 5).toInt());
    moving_average_window->setSuffix(tr(" min"));
    moving_average_window->setToolTip(tr("Moving-average window used for enabled chart metrics."));
    moving_average_layout->addWidget(moving_average_window);
    moving_average_layout->addStretch();
    ui->verticalLayout_5->insertLayout(11, moving_average_layout);
    ui->verticalLayout_5->addItem(ui->verticalSpacer_4);
    updateTrafficMetricSwatches();
    connect(trafficReceivedCheckBox, &QCheckBox::toggled, ui->trafficGraph, &TrafficGraphWidget::setShowReceived);
    connect(trafficSentCheckBox, &QCheckBox::toggled, ui->trafficGraph, &TrafficGraphWidget::setShowSent);
    connect(trafficAvgRecentLatencyCheckBox, &QCheckBox::toggled, ui->trafficGraph, &TrafficGraphWidget::setShowAvgRecentLatency);
    connect(trafficPeerAvgLatencyCheckBox, &QCheckBox::toggled, ui->trafficGraph, &TrafficGraphWidget::setShowPeerAvgLatency);
    connect(trafficJitterCheckBox, &QCheckBox::toggled, ui->trafficGraph, &TrafficGraphWidget::setShowJitter);
    connect(trafficMovingAverageCheckBox, &QCheckBox::toggled, ui->trafficGraph, &TrafficGraphWidget::setShowTrafficMovingAverages);
    connect(trafficTotalsAdaptiveScaleCheckBox, &QCheckBox::toggled, ui->trafficGraph, &TrafficGraphWidget::setTrafficAdaptiveScale);
    connect(trafficTotalsLogScaleCheckBox, &QCheckBox::toggled, ui->trafficGraph, &TrafficGraphWidget::setTrafficAlgorithmicScale);
    connect(trafficLatencyMovingAverageCheckBox, &QCheckBox::toggled, ui->trafficGraph, &TrafficGraphWidget::setShowLatencyMovingAverages);
    connect(trafficLatencyAdaptiveScaleCheckBox, &QCheckBox::toggled, ui->trafficGraph, &TrafficGraphWidget::setLatencyAdaptiveScale);
    connect(trafficLatencyLogScaleCheckBox, &QCheckBox::toggled, ui->trafficGraph, &TrafficGraphWidget::setLatencyAlgorithmicScale);
    connect(trafficLatencyHideWorst20CheckBox, &QCheckBox::toggled, this, [this, trafficLatencyShowWorst20CheckBox](bool checked) {
        if (checked && trafficLatencyShowWorst20CheckBox->isChecked()) trafficLatencyShowWorst20CheckBox->setChecked(false);
        ui->trafficGraph->setLatencyHideWorst20(checked);
    });
    connect(trafficLatencyShowWorst20CheckBox, &QCheckBox::toggled, this, [this, trafficLatencyHideWorst20CheckBox](bool checked) {
        if (checked && trafficLatencyHideWorst20CheckBox->isChecked()) trafficLatencyHideWorst20CheckBox->setChecked(false);
        ui->trafficGraph->setLatencyShowOnlyWorst20(checked);
    });
    connect(trafficAllHistoryCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        if (trafficWindowSlider) {
            QSignalBlocker blocker(trafficWindowSlider);
            trafficWindowSlider->setValue(checked ? TRAFFIC_WINDOW_SLIDER_ALL : trafficWindowSliderValueForSeconds(INITIAL_TRAFFIC_GRAPH_MINS * 60));
        }
        ui->trafficGraph->setShowAllHistory(checked);
        setTrafficGraphScale(ui->sldGraphRange->value());
    });
    connect(moving_average_window, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), ui->trafficGraph, &TrafficGraphWidget::setMovingAverageWindowMins);
    setTrafficGraphScale(ui->sldGraphRange->value());
    connect(ui->trafficGraph, &TrafficGraphWidget::latencyStatsChanged, this, [this](float recent, bool recent_valid, float average, bool average_valid, float jitter, bool jitter_valid) {
        if (trafficAvgLatencyLabel) {
            trafficAvgLatencyLabel->setText(recent_valid ? tr("%1 ms").arg(QString::number(recent, 'f', recent < 10.0f ? 1 : 0)) : tr("N/A"));
        }
        if (trafficPeerAvgLatencyLabel) {
            trafficPeerAvgLatencyLabel->setText(average_valid ? tr("%1 ms").arg(QString::number(average, 'f', average < 10.0f ? 1 : 0)) : tr("N/A"));
        }
        if (trafficJitterLabel) {
            trafficJitterLabel->setText(jitter_valid ? tr("%1 ms").arg(QString::number(jitter, 'f', jitter < 10.0f ? 1 : 0)) : tr("N/A"));
        }
    });
#else
    ui->groupBox->setTitle(tr("Totals"));
    ui->trafficGraph->setShowReceived(true);
    ui->trafficGraph->setShowSent(true);
    ui->trafficGraph->setPanPercent(1000);
    if (trafficPanSlider) {
        trafficPanSlider->setValue(1000);
    }
    if (trafficAllHistoryCheckBox) trafficAllHistoryCheckBox->hide();
    if (ui->chkAvgPeerLatency) ui->chkAvgPeerLatency->hide();
    if (ui->lineAvgLatency) ui->lineAvgLatency->hide();
    ui->trafficGraph->setShowAvgRecentLatency(false);
    ui->trafficGraph->setShowPeerAvgLatency(false);
    ui->trafficGraph->setShowJitter(false);
    ui->trafficGraph->setShowMovingAverages(false);
    setTrafficGraphScale(ui->sldGraphRange->value());
#endif

    // disable the wallet selector by default
    ui->WalletSelector->setVisible(false);
    ui->WalletSelectorLabel->setVisible(false);

    // set library version labels
#ifdef ENABLE_WALLET
    ui->berkeleyDBVersion->setText(QString::fromStdString(BerkeleyDatabaseVersion()));
#else
    ui->label_berkeleyDBVersion->hide();
    ui->berkeleyDBVersion->hide();
#endif
    // Register RPC timer interface
    rpcTimerInterface = new QtRPCTimerInterface();
    // avoid accidentally overwriting an existing, non QTThread
    // based timer interface
    m_node.rpcSetTimerInterfaceIfUnset(rpcTimerInterface);

    ui->trafficGraph->setGraphRangeSeconds(settings.value(QStringLiteral("TrafficGraphRangeSeconds"), INITIAL_TRAFFIC_GRAPH_MINS * 60).toLongLong());
    setTrafficGraphScale(ui->sldGraphRange->value());

    ui->detailWidget->hide();
    ui->peerHeading->setText(tr("Select a peer to view detailed information."));
    ui->verticalLayout_8->setContentsMargins(8, 8, 8, 8);
    ui->verticalLayout_8->setSpacing(8);
    ui->verticalLayout_8->setAlignment(Qt::AlignTop);
    peerDetailTopSpacer = new QWidget(this);
    peerDetailTopSpacer->setObjectName(QStringLiteral("peerDetailTopSpacer"));
    peerDetailTopSpacer->setFixedHeight(0);
    ui->verticalLayout_8->insertWidget(0, peerDetailTopSpacer);
    QWidget* peer_detail_header = new QWidget(this);
    peer_detail_header->setObjectName(QStringLiteral("peerDetailHeader"));
    QHBoxLayout* peer_detail_header_layout = new QHBoxLayout(peer_detail_header);
    peer_detail_header_layout->setContentsMargins(0, 0, 0, 0);
    peer_detail_header_layout->setSpacing(6);
    ui->verticalLayout_8->removeWidget(ui->peerHeading);
    peer_detail_header_layout->addWidget(ui->peerHeading, 1);
    peerDetailCollapseButton = new QToolButton(peer_detail_header);
    peerDetailCollapseButton->setObjectName(QStringLiteral("panelCollapseButton"));
    peerDetailCollapseButton->setAutoRaise(false);
    peerDetailCollapseButton->setFixedSize(34, 34);
    peerDetailCollapseButton->setIconSize(QSize(14, 14));
    peerDetailCollapseButton->setArrowType(Qt::NoArrow);
    QFont peer_collapse_font = peerDetailCollapseButton->font();
    peer_collapse_font.setPointSize(std::max(14, peer_collapse_font.pointSize() + 3));
    peer_collapse_font.setBold(true);
    peerDetailCollapseButton->setFont(peer_collapse_font);
    peerDetailCollapseButton->setText(QString::fromUtf8("▶"));
    peerDetailCollapseButton->setToolTip(tr("Collapse peer inspector"));
    peer_detail_header_layout->addWidget(peerDetailCollapseButton, 0, Qt::AlignTop | Qt::AlignRight);
    ui->verticalLayout_8->insertWidget(1, peer_detail_header);
    connect(peerDetailCollapseButton, &QToolButton::clicked, this, [this] {
        const bool collapsed = ui->scrollArea && ui->scrollArea->isVisible();
        setPeerDetailPanelCollapsed(collapsed);
    });
    if (QSettings().value(QStringLiteral("PeerDetailPanelCollapsed"), false).toBool()) {
        QTimer::singleShot(0, this, [this] { setPeerDetailPanelCollapsed(true); });
    }
    peerPingButton = new QPushButton(tr("Ping"), this);
    peerPingButton->setEnabled(false);
    peerPingButton->setToolTip(tr("Run the system ping tool for the selected peer in a separate window."));
    ui->verticalLayout_8->addWidget(peerPingButton);
    connect(peerPingButton, &QPushButton::clicked, this, &RPCConsole::showPingDialog);
    peerTraceRouteButton = new QPushButton(tr("Trace Route"), this);
    peerTraceRouteButton->setEnabled(false);
    peerTraceRouteButton->setToolTip(tr("Run the system traceroute tool for the selected peer in a separate window."));
    ui->verticalLayout_8->addWidget(peerTraceRouteButton);
    connect(peerTraceRouteButton, &QPushButton::clicked, this, &RPCConsole::showTraceRouteDialog);
    if (ui->scrollArea) {
        ui->scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    }
    if (ui->detailWidget) {
        ui->detailWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    }
    if (QGridLayout* detail_grid = qobject_cast<QGridLayout*>(ui->detailWidget->layout())) {
        detail_grid->setVerticalSpacing(1);
        detail_grid->setHorizontalSpacing(12);
        detail_grid->setContentsMargins(8, 4, 8, 4);
        auto add_detail_row = [this, detail_grid](int row, const QString& title, const QString& object_name, const QString& tooltip = QString()) {
            QLabel* key = new QLabel(title, ui->detailWidget);
            QLabel* value = new QLabel(tr("N/A"), ui->detailWidget);
            value->setObjectName(object_name);
            value->setWordWrap(true);
            value->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
            value->setOpenExternalLinks(true);
            value->setCursor(Qt::IBeamCursor);
            if (!tooltip.isEmpty()) {
                key->setToolTip(tooltip);
                value->setToolTip(tooltip);
            }
            detail_grid->addWidget(key, row, 0);
            detail_grid->addWidget(value, row, 1);
        };
#if ENABLE_DEFCOIN_FUN_UI
        add_detail_row(18, tr("UniqID"), QStringLiteral("peerInspectorUniqueId"), tr("DC34 Edition session identifier that reuses the same number for the same IP address, port, and node fingerprint when possible."));
        add_detail_row(19, tr("Fingerprint"), QStringLiteral("peerInspectorFingerprint"), tr("Best-effort node fingerprint derived from protocol version, services, and user agent. It is not cryptographic identity."));
        add_detail_row(20, tr("Node"), QStringLiteral("peerInspectorNode"));
        add_detail_row(21, tr("Port"), QStringLiteral("peerInspectorPort"));
        add_detail_row(22, tr("FQDN"), QStringLiteral("peerInspectorFqdn"), tr("Reverse DNS for this peer. [NA: LAN] is shown when no local DNS name is available."));
        add_detail_row(23, tr("Domain Alias"), QStringLiteral("peerInspectorCustomHostname"), tr("Seed domain, custom host label, or LAN device name discovered through local naming tools."));
        add_detail_row(24, tr("Seed"), QStringLiteral("peerInspectorSeed"));
        add_detail_row(25, tr("Geo"), QStringLiteral("peerInspectorGeo"));
        add_detail_row(26, tr("City, St"), QStringLiteral("peerInspectorCityState"), tr("Best-effort location. Public peers use their IP address; local and LAN rows use this node's public WAN address."));
        add_detail_row(27, tr("AS Number"), QStringLiteral("peerInspectorMappedAS"), tr("Autonomous System Number from the node backend or optional DNS-based IP-to-ASN lookup. This is distinct from service flags."));
        add_detail_row(28, tr("AS Name"), QStringLiteral("peerInspectorASName"), tr("Autonomous System name from optional DNS-based IP-to-ASN lookup."));
        add_detail_row(29, tr("AS Hosting Company"), QStringLiteral("peerInspectorASHost"), tr("Best-effort hosting company extracted from the Autonomous System name."));
        add_detail_row(30, tr("UA Count"), QStringLiteral("peerInspectorUaCount"));
        add_detail_row(31, tr("Location Source"), QStringLiteral("peerInspectorLocationSource"), tr("Lookup sources are documented in Help under Peer lookup sources."));
#else
        ui->peerAvgPingLabel->hide();
        ui->peerAvgPing->hide();
        ui->peerMappedASLabel->hide();
        ui->peerMappedAS->hide();
#endif
    }
    ui->peerViewLayout->addItem(tr("Default"));
    ui->peerViewLayout->addItem(tr("Compact"));
    ui->peerViewLayout->addItem(tr("Wide"));
    ui->peerViewLayout->addItem(tr("Saved"));
    ui->peerViewLayout->setCurrentIndex(0);
    QPushButton* peerAutoSizeColumnsButton = new QPushButton(tr("Auto-size Columns"), this);
    peerAutoSizeColumnsButton->setObjectName(QStringLiteral("peerAutoSizeColumnsButton"));
    peerAutoSizeColumnsButton->setToolTip(tr("Resize the Connected and Banned Peers columns to fit the current visible data."));
    ui->horizontalLayoutPeerView->insertWidget(6, peerAutoSizeColumnsButton);
    peerBanButton = new QPushButton(tr("Ban Peer"), this);
    peerBanButton->setToolTip(tr("Ban the selected peer. Use only for peers you do not want this node to reconnect to."));
    peerBanButton->setEnabled(false);
    ui->horizontalLayoutPeerView->insertWidget(7, peerBanButton);
    peerStatsBannerLabel = new QLabel(this);
    peerStatsBannerLabel->setTextFormat(Qt::RichText);
    peerStatsBannerLabel->setWordWrap(true);
    peerStatsBannerLabel->setToolTip(tr("Current Defcoin Peers counts active rows. Seen Since Open counts unique peers observed while this Node window has been open; enable Show inactive peers to display disconnected peers still in that count."));
    peerShowConnectedPeersCheckBox = new QCheckBox(tr("Show connected peers"), this);
    peerShowConnectedPeersCheckBox->setChecked(settings.value(QStringLiteral("PeerTableShowConnectedPeers"), true).toBool());
    peerShowConnectedPeersCheckBox->setToolTip(tr("Show or hide the Connected peers panel. Hide it when you want to focus on banned peers."));
    peerShowInactiveCheckBox = new QCheckBox(tr("Show inactive peers"), this);
    peerShowInactiveCheckBox->setChecked(settings.value(QStringLiteral("PeerTableShowInactivePeers"), true).toBool());
    peerShowInactiveCheckBox->setToolTip(tr("Show peers that were seen while this Node window was open but are not currently connected."));
    peerShowBannedPeersCheckBox = new QCheckBox(tr("Show banned peers"), this);
    peerShowBannedPeersCheckBox->setChecked(settings.value(QStringLiteral("PeerTableShowBannedPeers"), true).toBool());
    peerShowBannedPeersCheckBox->setToolTip(tr("Show or hide the Banned peers panel so the connected peer table can use more space."));
    peerLocalDiscoveryCheckBox = new QCheckBox(tr("Discover local network nodes"), this);
    peerLocalDiscoveryCheckBox->setChecked(settings.value(QStringLiteral("DiscoverLocalNetworkNodes"), true).toBool());
    peerLocalDiscoveryCheckBox->setToolTip(tr("Probe the local network for Defcoin nodes. macOS may ask for Local Network permission when this is enabled."));
    peerOnlyDefcoinUserAgentsCheckBox = new QCheckBox(tr("Only accept Defcoin prefixed User Agents"), this);
    const bool only_defcoin_user_agents_overridden = gArgs.IsArgSet("-onlydefcoinua");
    peerOnlyDefcoinUserAgentsCheckBox->setChecked(only_defcoin_user_agents_overridden ?
        gArgs.GetBoolArg("-onlydefcoinua", DEFAULT_DEFCOIN_USER_AGENT_FILTER) :
        settings.value(QStringLiteral("OnlyDefcoinUserAgents"), DEFAULT_DEFCOIN_USER_AGENT_FILTER).toBool());
    peerOnlyDefcoinUserAgentsCheckBox->setEnabled(!only_defcoin_user_agents_overridden);
    peerOnlyDefcoinUserAgentsCheckBox->setToolTip(only_defcoin_user_agents_overridden ?
        tr("This setting is currently controlled by the -onlydefcoinua startup option.") :
        tr("Disconnect peers whose advertised user agent does not start with Defcoin, ignoring case."));
    SetOnlyDefcoinUserAgents(peerOnlyDefcoinUserAgentsCheckBox->isChecked());
    peerGetLocationCheckBox = new QCheckBox(tr("Lookup Geo"), this);
    peerGetLocationCheckBox->setChecked(settings.value(QStringLiteral("PeerTableGetLocation"), true).toBool());
    peerGetLocationCheckBox->setToolTip(tr("Look up best-effort city and state/region. Public peers use their IP address; local and LAN rows use this node's public WAN address. Turning this off stops new lookups but keeps cached results for this Node window."));
    peerGetLocationCheckBox->setMinimumWidth(fontMetricHorizontalAdvance(QFontMetrics(peerGetLocationCheckBox->font()), tr("Lookup Geo")) + 54);
#if !ENABLE_DEFCOIN_FUN_UI
    peerGetLocationCheckBox->setChecked(false);
    peerGetLocationCheckBox->hide();
#endif
    peerLookupMappedASCheckBox = new QCheckBox(tr("Lookup Mapped AS"), this);
    peerLookupMappedASCheckBox->setChecked(settings.value(QStringLiteral("PeerTableLookupMappedAS"), false).toBool());
    peerLookupMappedASCheckBox->setToolTip(tr("Look up public peer Autonomous System Numbers using DNS-based IP-to-ASN data. Local and LAN addresses are never submitted. Turning this off stops new lookups but keeps cached results for this Node window."));
    peerLookupMappedASCheckBox->setMinimumWidth(fontMetricHorizontalAdvance(QFontMetrics(peerLookupMappedASCheckBox->font()), tr("Lookup Mapped AS")) + 58);
#if !ENABLE_DEFCOIN_FUN_UI
    peerLookupMappedASCheckBox->setChecked(false);
    peerLookupMappedASCheckBox->hide();
#endif
    peerUserAgentSummaryLabel = new QLabel(this);
    peerUserAgentSummaryLabel->setTextFormat(Qt::RichText);
    peerUserAgentSummaryLabel->setWordWrap(true);
    peerUserAgentSummaryLabel->setToolTip(tr("User agent counts are calculated from the peers currently shown in the Peers table."));
    peerUserAgentSummaryLabel->hide();
    peerUserAgentChartWidget = new UserAgentPieWidget(this);
    peerUserAgentChartWidget->setToolTip(tr("Pie chart of currently displayed remote peers grouped by user agent. The local node row is shown for context but is not counted as a peer."));
    QWidget* peer_overview = new QWidget(this);
    peer_overview->setObjectName(QStringLiteral("peerOverview"));
    peer_overview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QHBoxLayout* peer_overview_layout = new QHBoxLayout(peer_overview);
    peer_overview_layout->setContentsMargins(0, 0, 0, 0);
    peer_overview_layout->setSpacing(10);
    QWidget* peer_overview_text_widget = new QWidget(peer_overview);
    peer_overview_text_widget->setObjectName(QStringLiteral("peerOverviewTextColumn"));
    peer_overview_text_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QVBoxLayout* peer_overview_text = new QVBoxLayout(peer_overview_text_widget);
    peer_overview_text->setContentsMargins(0, 0, 0, 0);
    peer_overview_text->setSpacing(ENABLE_DEFCOIN_FUN_UI ? 8 : 4);
    QLayoutItem* peer_view_controls_item = ui->verticalLayout_7->takeAt(0);
    QLayout* peer_view_controls_layout = peer_view_controls_item ? peer_view_controls_item->layout() : nullptr;
    if (peer_view_controls_layout) {
        const QList<QWidget*> peer_controls{
            ui->peerViewLayoutLabel,
            ui->peerViewLayout,
            ui->peerEditViewButton,
            ui->peerSaveViewButton,
            ui->peerLoadViewButton,
            ui->peerResetViewButton,
            peerAutoSizeColumnsButton,
            peerBanButton
        };
        for (QWidget* control : peer_controls) {
            if (control) peer_view_controls_layout->removeWidget(control);
        }
        delete peer_view_controls_item;
        FlowLayout* peer_view_grid = new FlowLayout(nullptr, 0, 8, 4);
        const int peer_control_height = std::max(34, QFontMetrics(font()).height() + 14);
        ui->peerViewLayoutLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        ui->peerViewLayoutLabel->setFixedHeight(peer_control_height);
        ui->peerViewLayout->setFixedHeight(peer_control_height);
        GUIUtil::PolishComboBox(ui->peerViewLayout, 130);
        for (QWidget* control : {ui->peerEditViewButton, ui->peerSaveViewButton, ui->peerLoadViewButton, ui->peerResetViewButton, peerAutoSizeColumnsButton, peerBanButton}) {
            if (!control) continue;
            control->setFixedHeight(peer_control_height);
            control->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        }
        peer_view_grid->addWidget(ui->peerViewLayoutLabel);
        peer_view_grid->addWidget(ui->peerViewLayout);
        peer_view_grid->addWidget(ui->peerEditViewButton);
        peer_view_grid->addWidget(ui->peerSaveViewButton);
        peer_view_grid->addWidget(ui->peerLoadViewButton);
        peer_view_grid->addWidget(ui->peerResetViewButton);
        peer_view_grid->addWidget(peerAutoSizeColumnsButton);
        peer_view_grid->addWidget(peerBanButton);
        peer_overview_text->addLayout(peer_view_grid);
    } else {
        delete peer_view_controls_item;
    }
    peerStatsBannerLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    peer_overview_text->addWidget(peerStatsBannerLabel);
    FlowLayout* peer_filter_row = new FlowLayout(nullptr, 0, ENABLE_DEFCOIN_FUN_UI ? 14 : 12, ENABLE_DEFCOIN_FUN_UI ? 6 : 4);
    peer_filter_row->setContentsMargins(0, 0, 0, 0);
    peer_filter_row->addWidget(peerShowConnectedPeersCheckBox);
    peer_filter_row->addWidget(peerShowInactiveCheckBox);
    peer_filter_row->addWidget(peerShowBannedPeersCheckBox);
    peer_filter_row->addWidget(peerLocalDiscoveryCheckBox);
    peer_filter_row->addWidget(peerOnlyDefcoinUserAgentsCheckBox);
#if ENABLE_DEFCOIN_FUN_UI
    peer_filter_row->addWidget(peerGetLocationCheckBox);
    peer_filter_row->addWidget(peerLookupMappedASCheckBox);
#endif
    peer_overview_text->addLayout(peer_filter_row);
    peer_overview_layout->addWidget(peer_overview_text_widget, 1);
    peer_overview_layout->setAlignment(peer_overview_text_widget, Qt::AlignTop);
#if ENABLE_DEFCOIN_FUN_UI
    peer_overview_layout->addWidget(peerUserAgentChartWidget, 2);
    peer_overview_layout->setAlignment(peerUserAgentChartWidget, Qt::AlignTop | Qt::AlignHCenter);
#else
    peerUserAgentChartWidget->hide();
#endif
    ui->verticalLayout_7->insertWidget(0, peer_overview);
    QTimer::singleShot(0, this, &RPCConsole::updatePeerOverviewGeometry);
    QWidget* connected_peers_header = new QWidget(this);
    connectedPeersHeaderWidget = connected_peers_header;
    connected_peers_header->setObjectName(QStringLiteral("connectedPeersHeader"));
    QHBoxLayout* connected_peers_header_layout = new QHBoxLayout(connected_peers_header);
    connected_peers_header_layout->setContentsMargins(0, 0, 0, 0);
    connected_peers_header_layout->setSpacing(8);
    connectedPeersLabel = new QLabel(tr("Connected peers"), connected_peers_header);
    connectedPeersLabel->setObjectName(QStringLiteral("connectedPeersLabel"));
    connectedPeersLabel->setStyleSheet(QStringLiteral("font-weight: 600;"));
    connected_peers_header_layout->addWidget(connectedPeersLabel);
    connected_peers_header_layout->addStretch(1);
    QToolButton* connected_reset_button = new QToolButton(connected_peers_header);
    connected_reset_button->setText(QString::fromUtf8("↺"));
    connected_reset_button->setToolTip(tr("Reset peers view"));
    connected_reset_button->setAutoRaise(true);
    connected_peers_header_layout->addWidget(connected_reset_button, 0, Qt::AlignRight | Qt::AlignVCenter);
    connect(connected_reset_button, &QToolButton::clicked, this, [this] {
        QSettings settings;
        settings.remove(peerTableHeaderStateKey);
        settings.remove(peerTableColumnTitlesKey);
        settings.remove(peerTablePingHealthBaseWidthKey);
        settings.remove(peerPanelSplitterStateKey);
        settings.remove(QStringLiteral("pingToolFontSize"));
        settings.remove(QStringLiteral("traceRouteFontSize"));
        if (clientModel && clientModel->getPeerTableModel()) {
            clientModel->getPeerTableModel()->resetColumnTitles();
        }
        ui->peerWidget->horizontalHeader()->setSectionsMovable(false);
        if (ui->peerViewLayout) ui->peerViewLayout->setCurrentIndex(0);
        peerColumnsUserResized = false;
        connectedPeersPanelFontSize = nodeWindowFontSize > 0 ? nodeWindowFontSize : QApplication::font().pointSize();
        settings.setValue(QStringLiteral("connectedPeersPanelFontSize"), connectedPeersPanelFontSize);
        applyConnectedPeersPanelFont();
        resetPeerPanelLayout();
        resizePeerTableColumnsToContents();
        if (ui->peerWidget && ui->peerWidget->model()) {
            ui->peerWidget->sortByColumn(PeerTableModel::NetNodeId, Qt::AscendingOrder);
            ui->peerWidget->horizontalHeader()->setSortIndicator(PeerTableModel::NetNodeId, Qt::AscendingOrder);
        }
        if (ui->banlistWidget && ui->banlistWidget->model()) {
            ui->banlistWidget->sortByColumn(BanTableModel::NetNodeId, Qt::AscendingOrder);
            ui->banlistWidget->horizontalHeader()->setSortIndicator(BanTableModel::NetNodeId, Qt::AscendingOrder);
        }
        syncBanTableColumnsToPeerTable();
    });
    QToolButton* connected_popout_button = new QToolButton(connected_peers_header);
    connected_popout_button->setText(QString::fromUtf8("↗"));
    connected_popout_button->setToolTip(tr("Pop out Connected Peers table"));
    connected_popout_button->setAutoRaise(true);
    connected_peers_header_layout->addWidget(connected_popout_button, 0, Qt::AlignRight | Qt::AlignVCenter);
    connect(connected_popout_button, &QToolButton::clicked, this, [this] {
        if (!ui->peerWidget->model()) return;
        QDialog* dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(tr("Connected Peers"));
        dialog->resize(1100, 560);
        QVBoxLayout* layout = new QVBoxLayout(dialog);
        QTableView* table = new QTableView(dialog);
        table->setModel(ui->peerWidget->model());
#if ENABLE_DEFCOIN_FUN_UI
        table->setItemDelegateForColumn(PeerTableModel::PingHealth, new TrafficHealthDelegate(table));
        table->setItemDelegateForColumn(PeerTableModel::Country, new GeoIconDelegate(table));
#endif
        table->setWordWrap(true);
        table->setSortingEnabled(true);
        table->verticalHeader()->hide();
        table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table->setFont(ui->peerWidget->font());
        table->horizontalHeader()->setFont(ui->peerWidget->horizontalHeader()->font());
        layout->addWidget(table);
        QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
        dialog->show();
    });
    peerPanelTextControls = createTextControlWidget(
        tr("Make Connected and Banned Peers text smaller"),
        tr("Make Connected and Banned Peers text larger"),
        this,
        [this](int delta) { adjustConnectedPeersPanelFontSize(delta); });
    peerPanelTextControls->setVisible(!QSettings().value(QStringLiteral("HideTextResizeControls"), false).toBool());
    connected_peers_header_layout->addWidget(peerPanelTextControls, 0, Qt::AlignRight | Qt::AlignVCenter);
    ui->verticalLayout_7->insertWidget(1, connected_peers_header);
    updatePeerTableVisibility();
    QTimer::singleShot(0, this, &RPCConsole::alignPeerDetailPane);
    ui->peerPingDensity->setObjectName(QStringLiteral("peerPingDensityWheel"));
    peerPingHealthBaseWidth = settings.value(peerTablePingHealthBaseWidthKey, 140).toInt();
    ui->peerPingDensity->setRange(1, 60);
    ui->peerPingDensity->setValue(settings.value(QStringLiteral("PeerPingDensity"), 20).toInt());
    ui->peerPingDensity->setToolTip(tr("Adjust Traffic Health history density"));
    ui->peerPingDensity->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 14px; border: 1px solid #263342; border-radius: 7px; background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #18222c, stop:0.5 #3b4650, stop:1 #0b0f14); }"
        "QSlider::handle:horizontal { width: 18px; margin: -6px 0; border: 1px solid #596879; border-radius: 5px; background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #252f3a, stop:0.5 #596572, stop:1 #252f3a); }"
        "QSlider::sub-page:horizontal, QSlider::add-page:horizontal { background: transparent; }"));
    ui->peerPingDensityMinus->setStyleSheet(QStringLiteral("color: #98a5b4; font-size: 20px;"));
    ui->peerPingDensityPlus->setStyleSheet(QStringLiteral("color: #98a5b4; font-size: 20px;"));
    peerPingDensityMetricLabel = new QLabel(this);
    peerPingDensityMetricLabel->setToolTip(tr("Approximate Traffic Health history currently visible in the column."));
    peerTrafficHealthLegendLabel = new QLabel(this);
    peerTrafficHealthLegendLabel->setTextFormat(Qt::RichText);
    peerTrafficHealthLegendLabel->setToolTip(tr("Traffic Health colors are relative to recent ping latency: green is fast, amber is moderate, red is slow."));
    const int density_height = std::max(30, QFontMetrics(font()).height() + 12);
    ui->peerPingDensity->setMinimumHeight(density_height);
    peerPingDensityMetricLabel->setMinimumHeight(density_height);
    peerPingDensityMetricLabel->setAlignment(Qt::AlignVCenter);
    peerTrafficHealthLegendLabel->setMinimumHeight(density_height);
    peerTrafficHealthLegendLabel->setAlignment(Qt::AlignVCenter);
    const int density_insert = std::max(0, ui->horizontalLayoutPeerPingDensity->count() - 1);
    ui->horizontalLayoutPeerPingDensity->insertWidget(density_insert, peerPingDensityMetricLabel);
    ui->horizontalLayoutPeerPingDensity->insertWidget(density_insert + 1, peerTrafficHealthLegendLabel);
#if !ENABLE_DEFCOIN_FUN_UI
    ui->peerPingDensity->hide();
    ui->peerPingDensityMinus->hide();
    ui->peerPingDensityPlus->hide();
    if (peerPingDensityMetricLabel) peerPingDensityMetricLabel->hide();
    if (peerTrafficHealthLegendLabel) peerTrafficHealthLegendLabel->hide();
#else
    const bool show_density_controls = ui->tabWidget->currentWidget() == ui->tab_peers;
    ui->peerPingDensity->setVisible(show_density_controls);
    ui->peerPingDensityMinus->setVisible(show_density_controls);
    ui->peerPingDensityPlus->setVisible(show_density_controls);
    if (peerPingDensityMetricLabel) peerPingDensityMetricLabel->setVisible(show_density_controls);
    if (peerTrafficHealthLegendLabel) peerTrafficHealthLegendLabel->setVisible(show_density_controls);
#endif
    connect(ui->peerViewLayout, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &RPCConsole::applyPeerViewPreset);
    connect(ui->peerEditViewButton, &QPushButton::clicked, this, &RPCConsole::editPeerViewLayout);
    connect(ui->peerSaveViewButton, &QPushButton::clicked, this, &RPCConsole::savePeerViewLayout);
    connect(ui->peerLoadViewButton, &QPushButton::clicked, this, &RPCConsole::loadPeerViewLayout);
    connect(ui->peerResetViewButton, &QPushButton::clicked, this, &RPCConsole::resetPeerViewLayout);
    connect(peerAutoSizeColumnsButton, &QPushButton::clicked, this, [this] {
        peerColumnsUserResized = false;
        resizePeerTableColumnsToContents();
        syncBanTableColumnsToPeerTable();
    });
    connect(peerShowConnectedPeersCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        QSettings().setValue(QStringLiteral("PeerTableShowConnectedPeers"), checked);
        if (clientModel && clientModel->getPeerTableModel()) {
            clientModel->getPeerTableModel()->setShowConnectedPeers(checked);
        }
        updatePeerTableVisibility();
        updatePeerSummaryStats();
        resizePeerTableColumnsToContents();
    });
    connect(peerShowInactiveCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        QSettings().setValue(QStringLiteral("PeerTableShowInactivePeers"), checked);
        if (clientModel && clientModel->getPeerTableModel()) {
            clientModel->getPeerTableModel()->setShowInactivePeers(checked);
        }
        updatePeerTableVisibility();
        updatePeerSummaryStats();
        resizePeerTableColumnsToContents();
    });
    connect(peerLocalDiscoveryCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        QSettings().setValue(QStringLiteral("DiscoverLocalNetworkNodes"), checked);
        if (checked) {
            startLocalPeerDiscovery();
        } else {
            stopLocalPeerDiscovery();
        }
    });
    connect(peerShowBannedPeersCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        QSettings().setValue(QStringLiteral("PeerTableShowBannedPeers"), checked);
        showOrHideBanTableIfRequired();
    });
    connect(peerOnlyDefcoinUserAgentsCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        QSettings().setValue(QStringLiteral("OnlyDefcoinUserAgents"), checked);
        SetOnlyDefcoinUserAgents(checked);
        if (clientModel && clientModel->getPeerTableModel()) {
            clientModel->getPeerTableModel()->setOnlyDefcoinUserAgents(checked);
        }
        if (checked && clientModel) {
            interfaces::Node::NodesStats nodes_stats;
            clientModel->node().getNodesStats(nodes_stats);
            for (const auto& node_stats : nodes_stats) {
                const CNodeStats& stats = std::get<0>(node_stats);
                QString user_agent = QString::fromStdString(stats.cleanSubVer);
                while (user_agent.startsWith(QLatin1Char('/'))) user_agent.remove(0, 1);
                if (!user_agent.startsWith(QStringLiteral("defcoin"), Qt::CaseInsensitive) && stats.nodeid >= 0) {
                    clientModel->node().disconnectById(stats.nodeid);
                }
            }
        }
        updatePeerSummaryStats();
        resizePeerTableColumnsToContents();
    });
    connect(peerGetLocationCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        QSettings().setValue(QStringLiteral("PeerTableGetLocation"), checked);
        if (clientModel && clientModel->getPeerTableModel()) {
            clientModel->getPeerTableModel()->setGeolocationEnabled(checked);
        }
        resizePeerTableColumnsToContents();
    });
    connect(peerLookupMappedASCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        QSettings().setValue(QStringLiteral("PeerTableLookupMappedAS"), checked);
        if (clientModel && clientModel->getPeerTableModel()) {
            clientModel->getPeerTableModel()->setMappedASLookupEnabled(checked);
            if (checked) {
                for (NodeId nodeid : cachedNodeids) {
                    clientModel->getPeerTableModel()->prioritizeLookupsForNode(nodeid);
                }
                clientModel->getPeerTableModel()->refresh();
                ui->peerWidget->viewport()->update();
            }
        }
        resizePeerTableColumnsToContents();
    });
    connect(ui->peerPingDensity, &QSlider::valueChanged, this, [this](int value) {
        QSettings().setValue(QStringLiteral("PeerPingDensity"), value);
        updatePeerPingHealthDensity();
    });
#if ENABLE_DEFCOIN_FUN_UI
    peerTrafficHealthRepaintTimer = new QTimer(this);
    peerTrafficHealthRepaintTimer->setInterval(180);
    connect(peerTrafficHealthRepaintTimer, &QTimer::timeout, this, [this] {
        if (ui->tabWidget->currentWidget() == ui->tab_peers && ui->peerWidget && ui->peerWidget->isVisible()) {
            ui->peerWidget->viewport()->update();
            if (ui->banlistWidget && ui->banlistWidget->isVisible()) ui->banlistWidget->viewport()->update();
        }
    });
#endif

    consoleFontSize = settings.value(fontSizeSettingsKey, QFont().pointSize()).toInt();
    nodeWindowFontSize = settings.value(QStringLiteral("nodeWindowFontSize"), font().pointSize()).toInt();
    connectedPeersPanelFontSize = settings.value(QStringLiteral("connectedPeersPanelFontSize"), nodeWindowFontSize).toInt();
    setNodeWindowFontSize(nodeWindowFontSize);
    clear();

    GUIUtil::handleCloseWindowShortcut(this);
}

RPCConsole::~RPCConsole()
{
    QSettings settings;
    settings.setValue("RPCConsoleWindowGeometry", saveGeometry());
    savePeerPanelLayout();
    m_node.rpcUnsetTimerInterface(rpcTimerInterface);
    delete rpcTimerInterface;
    delete ui;
}

void RPCConsole::installNodeTextControls()
{
    if (QTabBar* tab_bar = ui->tabWidget->tabBar()) {
        const int min_height = std::max(34, QFontMetrics(tab_bar->font()).height() + 14);
        tab_bar->setMinimumHeight(min_height);
        tab_bar->setExpanding(false);
    }
    QWidget* controls = createTextControlWidget(
        tr("Make all Node window text smaller"),
        tr("Make all Node window text larger"),
        this,
        [this](int delta) { adjustNodeWindowFontSize(delta); });
    controls->setMinimumHeight(40);
    controls->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QWidget* header_row = new QWidget(this);
    header_row->setObjectName(QStringLiteral("nodeTextControls"));
    QHBoxLayout* header_layout = new QHBoxLayout(header_row);
    header_layout->setContentsMargins(0, 4, 0, 6);
    header_layout->setSpacing(0);
    header_layout->addStretch(1);
    header_layout->addWidget(controls, 0, Qt::AlignRight | Qt::AlignVCenter);
    nodeTextControls = header_row;
    nodeTextControls->setVisible(!QSettings().value(QStringLiteral("HideTextResizeControls"), false).toBool());
    ui->verticalLayout_2->insertWidget(1, header_row);
}

QWidget* RPCConsole::createTextControlWidget(const QString& smaller_tooltip, const QString& bigger_tooltip, const QObject* receiver, const std::function<void(int)>& adjust_callback)
{
    QWidget* controls = new QWidget();
    controls->setObjectName(QStringLiteral("nodeTextControlPair"));
    QHBoxLayout* layout = new QHBoxLayout(controls);
    layout->setContentsMargins(2, 0, 2, 0);
    layout->setSpacing(4);

    QPushButton* smaller = new QPushButton(tr("A-"), controls);
    QPushButton* bigger = new QPushButton(tr("A+"), controls);
    smaller->setToolTip(smaller_tooltip);
    bigger->setToolTip(bigger_tooltip);
    smaller->setFixedSize(46, 30);
    bigger->setFixedSize(46, 30);
    QFont button_font = smaller->font();
    button_font.setPointSize(std::max(12, button_font.pointSize() + 2));
    button_font.setBold(true);
    smaller->setFont(button_font);
    bigger->setFont(button_font);
    const QString button_style = QStringLiteral(
        "QPushButton { color: #8dbfff; background: rgba(15, 24, 34, 185);"
        " border: 1px solid rgba(141, 191, 255, 190); border-radius: 4px;"
        " padding: 0 2px; }"
        "QPushButton:hover { background: rgba(34, 56, 78, 220); border-color: #b7d6ff; }"
        "QPushButton:pressed { background: rgba(7, 15, 24, 235); }");
    smaller->setStyleSheet(button_style);
    bigger->setStyleSheet(button_style);
    layout->addWidget(smaller);
    layout->addWidget(bigger);

    connect(smaller, &QPushButton::clicked, receiver, [adjust_callback] { adjust_callback(-1); });
    connect(bigger, &QPushButton::clicked, receiver, [adjust_callback] { adjust_callback(1); });
    return controls;
}

void RPCConsole::updateTrafficMetricSwatches()
{
    setMetricSwatchColor(trafficReceivedSwatch, ui->trafficGraph->metricColor(TrafficGraphWidget::TrafficMetric::Received));
    setMetricSwatchColor(trafficSentSwatch, ui->trafficGraph->metricColor(TrafficGraphWidget::TrafficMetric::Sent));
    setMetricSwatchColor(trafficAvgRecentLatencySwatch, ui->trafficGraph->metricColor(TrafficGraphWidget::TrafficMetric::AvgRecentLatency));
    setMetricSwatchColor(trafficPeerAvgLatencySwatch, ui->trafficGraph->metricColor(TrafficGraphWidget::TrafficMetric::PeerAvgLatency));
    setMetricSwatchColor(trafficJitterSwatch, ui->trafficGraph->metricColor(TrafficGraphWidget::TrafficMetric::Jitter));
}

void RPCConsole::setupTrafficStatsSplitter()
{
    if (!ui->horizontalLayout_3 || trafficStatsSplitter) return;

    int graph_index = -1;
    int stats_index = -1;
    for (int i = 0; i < ui->horizontalLayout_3->count(); ++i) {
        QLayoutItem* item = ui->horizontalLayout_3->itemAt(i);
        if (!item || !item->layout()) continue;
        if (item->layout() == ui->verticalLayout_4) graph_index = i;
        if (item->layout() == ui->verticalLayout) stats_index = i;
    }
    if (graph_index < 0 || stats_index < 0) return;

    QLayoutItem* first = nullptr;
    QLayoutItem* second = nullptr;
    if (graph_index > stats_index) {
        first = ui->horizontalLayout_3->takeAt(graph_index);
        second = ui->horizontalLayout_3->takeAt(stats_index);
    } else {
        first = ui->horizontalLayout_3->takeAt(stats_index);
        second = ui->horizontalLayout_3->takeAt(graph_index);
    }

    QLayout* graph_layout = nullptr;
    QLayout* stats_layout = nullptr;
    for (QLayoutItem* item : {first, second}) {
        if (!item) continue;
        if (item->layout() == ui->verticalLayout_4) graph_layout = item->layout();
        if (item->layout() == ui->verticalLayout) stats_layout = item->layout();
    }
    if (!graph_layout || !stats_layout) return;
    graph_layout->setParent(nullptr);
    stats_layout->setParent(nullptr);
    // The QLayoutItem wrappers returned by takeAt() can own the child layout
    // pointer on some Qt builds. Deleting them before reparenting the layouts
    // below leaves QWidget::setLayout() with a dangling pointer at launch.

    trafficGraphPanel = new QWidget(ui->tab_nettraffic);
    trafficGraphPanel->setObjectName(QStringLiteral("trafficGraphPanel"));
    trafficGraphPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    trafficGraphPanel->setMinimumWidth(180);
    trafficGraphPanel->setLayout(graph_layout);

    trafficStatsPanel = new QWidget(ui->tab_nettraffic);
    trafficStatsPanel->setObjectName(QStringLiteral("trafficStatsPanel"));
    trafficStatsPanel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);
    QVBoxLayout* stats_outer = new QVBoxLayout(trafficStatsPanel);
    stats_outer->setContentsMargins(0, 0, 0, 0);
    stats_outer->setSpacing(4);
    trafficStatsCollapseButton = new QToolButton(trafficStatsPanel);
    trafficStatsCollapseButton->setObjectName(QStringLiteral("panelCollapseButton"));
    trafficStatsCollapseButton->setAutoRaise(false);
    trafficStatsCollapseButton->setFixedSize(34, 34);
    trafficStatsCollapseButton->setIconSize(QSize(14, 14));
    trafficStatsCollapseButton->setArrowType(Qt::NoArrow);
    QFont traffic_collapse_font = trafficStatsCollapseButton->font();
    traffic_collapse_font.setPointSize(std::max(14, traffic_collapse_font.pointSize() + 3));
    traffic_collapse_font.setBold(true);
    trafficStatsCollapseButton->setFont(traffic_collapse_font);
    trafficStatsCollapseButton->setText(QString::fromUtf8("▶"));
    trafficStatsCollapseButton->setToolTip(tr("Collapse network statistics"));
    stats_outer->addWidget(trafficStatsCollapseButton, 0, Qt::AlignRight);
    stats_outer->addLayout(stats_layout);

    trafficStatsSplitter = new QSplitter(Qt::Horizontal, ui->tab_nettraffic);
    trafficStatsSplitter->setObjectName(QStringLiteral("trafficStatsSplitter"));
    trafficStatsSplitter->setChildrenCollapsible(false);
    trafficStatsSplitter->addWidget(trafficGraphPanel);
    trafficStatsSplitter->addWidget(trafficStatsPanel);
    trafficStatsSplitter->setStretchFactor(0, 1);
    trafficStatsSplitter->setStretchFactor(1, 0);
    ui->horizontalLayout_3->addWidget(trafficStatsSplitter);
    const QByteArray state = QSettings().value(QStringLiteral("TrafficStatsSplitterState")).toByteArray();
    if (!state.isEmpty()) {
        trafficStatsSplitter->restoreState(state);
    } else {
        trafficStatsSplitter->setSizes(QList<int>() << 960 << 210);
    }
    connect(trafficStatsSplitter, &QSplitter::splitterMoved, this, [this] {
        QSettings().setValue(QStringLiteral("TrafficStatsSplitterState"), trafficStatsSplitter->saveState());
    });
    connect(trafficStatsCollapseButton, &QToolButton::clicked, this, [this] {
        setTrafficStatsPanelCollapsed(!trafficStatsPanelCollapsed);
    });
    setTrafficStatsPanelCollapsed(QSettings().value(QStringLiteral("TrafficStatsPanelCollapsed"), false).toBool());
}

void RPCConsole::setTrafficStatsPanelCollapsed(bool collapsed)
{
    if (!trafficStatsPanel || !ui->groupBox) return;
    trafficStatsPanelCollapsed = collapsed;
    QSettings().setValue(QStringLiteral("TrafficStatsPanelCollapsed"), collapsed);
    ui->groupBox->setVisible(!collapsed);
    if (trafficStatsCollapseButton) {
        trafficStatsCollapseButton->setArrowType(Qt::NoArrow);
        trafficStatsCollapseButton->setFixedSize(34, 34);
        trafficStatsCollapseButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        QFont arrow_font = trafficStatsCollapseButton->font();
        arrow_font.setPointSize(std::max(13, font().pointSize()));
        arrow_font.setBold(true);
        trafficStatsCollapseButton->setFont(arrow_font);
        trafficStatsCollapseButton->setText(collapsed ? QString::fromUtf8("◀") : QString::fromUtf8("▶"));
        trafficStatsCollapseButton->setToolTip(collapsed ? tr("Show network statistics") : tr("Collapse network statistics"));
        if (trafficStatsPanel->layout()) {
            trafficStatsPanel->layout()->setAlignment(trafficStatsCollapseButton, collapsed ? (Qt::AlignHCenter | Qt::AlignTop) : (Qt::AlignRight | Qt::AlignTop));
        }
    }
    const int collapsed_width = 58;
#if ENABLE_DEFCOIN_FUN_UI
    const int expanded_width = std::min(360, std::max(300, ui->groupBox->sizeHint().width() + 24));
#else
    const int expanded_width = std::min(300, std::max(220, ui->groupBox->sizeHint().width() + 20));
#endif
    trafficStatsPanel->setMinimumWidth(collapsed ? collapsed_width : 220);
    trafficStatsPanel->setMaximumWidth(collapsed ? collapsed_width : expanded_width);
    if (trafficStatsSplitter) {
        const int total = std::max(1, trafficStatsSplitter->width());
        trafficStatsSplitter->setSizes(QList<int>() << std::max(240, total - (collapsed ? collapsed_width : trafficStatsPanel->maximumWidth())) << (collapsed ? collapsed_width : trafficStatsPanel->maximumWidth()));
    }
}

void RPCConsole::setPeerDetailPanelCollapsed(bool collapsed)
{
    if (!ui->widget_2) return;
    QSettings().setValue(QStringLiteral("PeerDetailPanelCollapsed"), collapsed);
    if (ui->peerHeading) ui->peerHeading->setVisible(!collapsed);
    if (ui->scrollArea) ui->scrollArea->setVisible(!collapsed);
    if (peerPingButton) peerPingButton->setVisible(!collapsed);
    if (peerTraceRouteButton) peerTraceRouteButton->setVisible(!collapsed);
    if (peerDetailCollapseButton) {
        peerDetailCollapseButton->setArrowType(Qt::NoArrow);
        peerDetailCollapseButton->setFixedSize(34, 34);
        peerDetailCollapseButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        QFont arrow_font = peerDetailCollapseButton->font();
        arrow_font.setPointSize(std::max(13, font().pointSize()));
        arrow_font.setBold(true);
        peerDetailCollapseButton->setFont(arrow_font);
        peerDetailCollapseButton->setText(collapsed ? QString::fromUtf8("◀") : QString::fromUtf8("▶"));
        peerDetailCollapseButton->setToolTip(collapsed ? tr("Show peer inspector") : tr("Collapse peer inspector"));
        if (peerDetailCollapseButton->parentWidget() && peerDetailCollapseButton->parentWidget()->layout()) {
            peerDetailCollapseButton->parentWidget()->layout()->setAlignment(peerDetailCollapseButton, collapsed ? (Qt::AlignHCenter | Qt::AlignTop) : (Qt::AlignRight | Qt::AlignTop));
        }
    }
    const int collapsed_width = 58;
    const int expanded_width = 300;
    ui->widget_2->setMinimumWidth(collapsed ? collapsed_width : 240);
    ui->widget_2->setMaximumWidth(collapsed ? collapsed_width : 340);
    if (ui->splitter) {
        const int total = std::max(1, ui->splitter->width());
        ui->splitter->setSizes(QList<int>() << std::max(220, total - (collapsed ? collapsed_width : expanded_width)) << (collapsed ? collapsed_width : expanded_width));
    }
    updatePeerInspectorHeight();
}

void RPCConsole::resetNodeVisualState()
{
    QSettings settings;
    settings.remove(QStringLiteral("mainWindowFontSize"));
    settings.remove(QStringLiteral("nodeWindowFontSize"));
    settings.remove(QStringLiteral("connectedPeersPanelFontSize"));
    settings.remove(fontSizeSettingsKey);
    settings.remove(QStringLiteral("pingToolFontSize"));
    settings.remove(QStringLiteral("traceRouteFontSize"));
    settings.remove(QStringLiteral("PeerPingDensity"));
    settings.remove(QStringLiteral("TrafficGraphShowReceived"));
    settings.remove(QStringLiteral("TrafficGraphShowSent"));
    settings.remove(QStringLiteral("TrafficGraphShowAvgRecentLatency"));
    settings.remove(QStringLiteral("TrafficGraphShowPeerAvgLatency"));
    settings.remove(QStringLiteral("TrafficGraphShowJitter"));
    settings.remove(QStringLiteral("TrafficGraphShowMovingAverages"));
    settings.remove(QStringLiteral("TrafficGraphShowTrafficMovingAverages"));
    settings.remove(QStringLiteral("TrafficGraphShowLatencyMovingAverages"));
    settings.remove(QStringLiteral("TrafficGraphMovingAverageWindowMins"));
    settings.remove(QStringLiteral("TrafficGraphAlgorithmicScale"));
    settings.remove(QStringLiteral("TrafficGraphLatencyAlgorithmicScale"));
    settings.remove(QStringLiteral("TrafficGraphLatencyAdaptiveScale"));
    settings.remove(QStringLiteral("TrafficGraphShowAllHistory"));
    settings.remove(QStringLiteral("TrafficGraphScalePercent"));
    settings.remove(QStringLiteral("TrafficGraphRangeSeconds"));
    settings.remove(QStringLiteral("TrafficGraphPanPercent"));
    settings.remove(QStringLiteral("TrafficStatsSplitterState"));
    settings.remove(QStringLiteral("TrafficStatsPanelCollapsed"));
    settings.remove(QStringLiteral("PeerMapSplitterState"));
    settings.remove(QStringLiteral("PeerMapSidebarCollapsed"));
    settings.remove(QStringLiteral("PeerDetailPanelCollapsed"));
    settings.setValue(QStringLiteral("PeerTableShowConnectedPeers"), true);
    settings.setValue(QStringLiteral("PeerTableShowInactivePeers"), true);
    settings.setValue(QStringLiteral("PeerTableShowBannedPeers"), true);
    settings.remove(QStringLiteral("PeerTableGetLocation"));
    settings.remove(QStringLiteral("PeerTableLookupMappedAS"));

    ui->peerPingDensity->setValue(20);
    if (peerShowConnectedPeersCheckBox) peerShowConnectedPeersCheckBox->setChecked(true);
    if (peerShowInactiveCheckBox) peerShowInactiveCheckBox->setChecked(true);
#if ENABLE_DEFCOIN_FUN_UI
    if (peerGetLocationCheckBox) peerGetLocationCheckBox->setChecked(true);
#else
    if (peerGetLocationCheckBox) peerGetLocationCheckBox->setChecked(false);
#endif
    if (peerLookupMappedASCheckBox) peerLookupMappedASCheckBox->setChecked(false);
    const int default_font_size = QApplication::font().pointSize();
    nodeWindowFontSize = default_font_size;
    connectedPeersPanelFontSize = default_font_size;
    applyNodeWindowFont();
    applyConnectedPeersPanelFont();
    setFontSize(default_font_size);
    settings.setValue(QStringLiteral("nodeWindowFontSize"), nodeWindowFontSize);
    settings.setValue(QStringLiteral("connectedPeersPanelFontSize"), connectedPeersPanelFontSize);
    settings.setValue(QStringLiteral("mainWindowFontSize"), default_font_size);
    for (QWidget* top_level : QApplication::topLevelWidgets()) {
        if (!top_level || top_level == this) continue;
        if (top_level->metaObject()->indexOfMethod("refreshTextSizeSettings()") >= 0) {
            QMetaObject::invokeMethod(top_level, "refreshTextSizeSettings", Qt::QueuedConnection);
        }
    }
    relayoutAfterNodeFontChange();
    if (trafficReceivedCheckBox) trafficReceivedCheckBox->setChecked(true);
    if (trafficSentCheckBox) trafficSentCheckBox->setChecked(true);
    if (trafficAvgRecentLatencyCheckBox) trafficAvgRecentLatencyCheckBox->setChecked(true);
    if (trafficPeerAvgLatencyCheckBox) trafficPeerAvgLatencyCheckBox->setChecked(true);
    if (trafficJitterCheckBox) trafficJitterCheckBox->setChecked(true);
    if (trafficMovingAverageCheckBox) trafficMovingAverageCheckBox->setChecked(false);
    if (trafficAllHistoryCheckBox) trafficAllHistoryCheckBox->setChecked(false);
    ui->trafficGraph->setGraphRangeSeconds(INITIAL_TRAFFIC_GRAPH_MINS * 60);
    if (ui->sldGraphRange) ui->sldGraphRange->setValue(TRAFFIC_RANGE_SLIDER_MAX);
    if (trafficWindowSlider) trafficWindowSlider->setValue(trafficWindowSliderValueForSeconds(INITIAL_TRAFFIC_GRAPH_MINS * 60));
    if (trafficPanSlider) trafficPanSlider->setValue(1000);
    setTrafficGraphScale(TRAFFIC_RANGE_SLIDER_MAX, true);
    if (QSpinBox* window = findChild<QSpinBox*>(QStringLiteral("trafficMovingAverageWindow"))) window->setValue(5);
    if (QCheckBox* adaptive = findChild<QCheckBox*>(QStringLiteral("trafficTotalsAdaptiveScale"))) adaptive->setChecked(true);
    if (QCheckBox* algorithmic = findChild<QCheckBox*>(QStringLiteral("trafficTotalsLogScale"))) algorithmic->setChecked(false);
    if (QCheckBox* moving = findChild<QCheckBox*>(QStringLiteral("trafficLatencyMovingAverage"))) moving->setChecked(false);
    if (QCheckBox* adaptive = findChild<QCheckBox*>(QStringLiteral("trafficLatencyAdaptiveScale"))) adaptive->setChecked(true);
    if (QCheckBox* algorithmic = findChild<QCheckBox*>(QStringLiteral("trafficLatencyLogScale"))) algorithmic->setChecked(false);
    if (QCheckBox* hide = findChild<QCheckBox*>(QStringLiteral("trafficLatencyHideWorst20"))) hide->setChecked(false);
    if (QCheckBox* show = findChild<QCheckBox*>(QStringLiteral("trafficLatencyShowOnlyWorst20"))) show->setChecked(false);
    resetPeerMapDataSource();
}

bool RPCConsole::handlePeerDemoKey(QKeyEvent* event)
{
    Q_UNUSED(event);
    return false;
}

void RPCConsole::setPeerDemoMode(bool enabled)
{
    peerDemoMode = enabled;
    if (clientModel && clientModel->getPeerTableModel()) {
        clientModel->getPeerTableModel()->setDemoPeersEnabled(enabled);
    }
#if ENABLE_DEFCOIN_FUN_UI
    if (peerMapWidget) {
        static_cast<PeerMapWidget*>(peerMapWidget)->setDemoMode(enabled);
    }
#endif
    updatePeerSummaryStats();
}

void RPCConsole::togglePeerDemoMode()
{
    setPeerDemoMode(!peerDemoMode);
}

void RPCConsole::resetPeerMapDataSource()
{
    peerDemoKeyBuffer.clear();
    peerDemoMode = false;
    if (clientModel && clientModel->getPeerTableModel()) {
        clientModel->getPeerTableModel()->setDemoPeersEnabled(false);
    }
#if ENABLE_DEFCOIN_FUN_UI
    if (peerMapWidget) {
        static_cast<PeerMapWidget*>(peerMapWidget)->resetToRealNodes();
    }
#endif
    updatePeerSummaryStats();
}

bool RPCConsole::eventFilter(QObject* obj, QEvent *event)
{
    if ((obj == ui->sldGraphRange || obj == trafficPanSlider || obj == trafficWindowSlider) && event->type() == QEvent::MouseButtonPress) {
        lastTrafficSlider = qobject_cast<QSlider*>(obj);
    }
    if ((obj == ui->lblGraphRange || obj == trafficWindowDurationLabel) && event->type() == QEvent::MouseButtonDblClick) {
        promptTrafficGraphDuration();
        return true;
    }
    if (((ui->peerWidget && obj == ui->peerWidget->horizontalHeader()->viewport()) ||
         (ui->banlistWidget && obj == ui->banlistWidget->horizontalHeader()->viewport())) &&
        event->type() == QEvent::MouseButtonDblClick) {
        QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
        if (mouse_event->pos().x() <= 12) {
            peerColumnsUserResized = false;
            resizePeerTableColumnsToContents();
            syncBanTableColumnsToPeerTable();
            return true;
        }
    }
    if (obj == ui->trafficGraph && event->type() == QEvent::Wheel) {
        QWheelEvent* wheel_event = static_cast<QWheelEvent*>(event);
        const int delta = wheel_event->angleDelta().y();
        if (delta != 0) {
            if (wheel_event->modifiers() & (Qt::MetaModifier | Qt::AltModifier | Qt::ControlModifier)) {
                const int step = delta > 0 ? 1 : -1;
                ui->sldGraphRange->setValue(std::max(ui->sldGraphRange->minimum(), std::min(ui->sldGraphRange->maximum(), ui->sldGraphRange->value() + step)));
                lastTrafficSlider = ui->sldGraphRange;
            } else if (trafficPanSlider && trafficPanSlider->isEnabled()) {
                const int step = delta > 0 ? trafficPanSlider->singleStep() : -trafficPanSlider->singleStep();
                trafficPanSlider->setValue(std::max(trafficPanSlider->minimum(), std::min(trafficPanSlider->maximum(), trafficPanSlider->value() + step)));
                lastTrafficSlider = trafficPanSlider;
            }
            return true;
        }
    }
    if (obj == ui->peerWidget->viewport() && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
        if (!ui->peerWidget->indexAt(mouse_event->pos()).isValid()) {
            clearSelectedNode();
        }
    }
    if(event->type() == QEvent::KeyPress) // Special key handling
    {
        QKeyEvent *keyevt = static_cast<QKeyEvent*>(event);
        if ((obj == ui->sldGraphRange || obj == trafficPanSlider || obj == trafficWindowSlider) && handleTrafficSliderKey(keyevt)) {
            return true;
        }
        if (handlePeerDemoKey(keyevt)) {
            return true;
        }
        int key = keyevt->key();
        Qt::KeyboardModifiers mod = keyevt->modifiers();
        switch(key)
        {
        case Qt::Key_Up: if(obj == ui->lineEdit) { browseHistory(-1); return true; } break;
        case Qt::Key_Down: if(obj == ui->lineEdit) { browseHistory(1); return true; } break;
        case Qt::Key_PageUp: /* pass paging keys to messages widget */
        case Qt::Key_PageDown:
            if(obj == ui->lineEdit)
            {
                QApplication::postEvent(ui->messagesWidget, new QKeyEvent(*keyevt));
                return true;
            }
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            // forward these events to lineEdit
            if(obj == autoCompleter->popup()) {
                QApplication::postEvent(ui->lineEdit, new QKeyEvent(*keyevt));
                autoCompleter->popup()->hide();
                return true;
            }
            break;
        default:
            // Typing in messages widget brings focus to line edit, and redirects key there
            // Exclude most combinations and keys that emit no text, except paste shortcuts
            if(obj == ui->messagesWidget && (
                  (!mod && !keyevt->text().isEmpty() && key != Qt::Key_Tab) ||
                  ((mod & Qt::ControlModifier) && key == Qt::Key_V) ||
                  ((mod & Qt::ShiftModifier) && key == Qt::Key_Insert)))
            {
                ui->lineEdit->setFocus();
                QApplication::postEvent(ui->lineEdit, new QKeyEvent(*keyevt));
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void RPCConsole::setClientModel(ClientModel *model, int bestblock_height, int64_t bestblock_date, double verification_progress)
{
    clientModel = model;

    bool wallet_enabled{false};
#ifdef ENABLE_WALLET
    wallet_enabled = WalletModel::isWalletEnabled();
#endif // ENABLE_WALLET
    if (model && !wallet_enabled) {
        // Show warning, for example if this is a prerelease version
        connect(model, &ClientModel::alertsChanged, this, &RPCConsole::updateAlerts);
        updateAlerts(model->getStatusBarWarnings());
    }

    ui->trafficGraph->setClientModel(model);
    if (model && clientModel->getPeerTableModel() && clientModel->getBanTableModel()) {
        // Keep up to date with client
        setNumConnections(model->getNumConnections());
        connect(model, &ClientModel::numConnectionsChanged, this, &RPCConsole::setNumConnections);

        setNumBlocks(bestblock_height, QDateTime::fromTime_t(bestblock_date), verification_progress, false);
        connect(model, &ClientModel::numBlocksChanged, this, &RPCConsole::setNumBlocks);

        updateNetworkState();
        connect(model, &ClientModel::networkActiveChanged, this, &RPCConsole::setNetworkActive);

        interfaces::Node& node = clientModel->node();
        updateTrafficStats(node.getTotalBytesRecv(), node.getTotalBytesSent());
        connect(model, &ClientModel::bytesChanged, this, &RPCConsole::updateTrafficStats);

        connect(model, &ClientModel::mempoolSizeChanged, this, &RPCConsole::setMempoolSize);

        // set up peer table
        ui->peerWidget->setModel(model->getPeerTableModel());
#if ENABLE_DEFCOIN_FUN_UI
        if (peerMapWidget) {
            static_cast<PeerMapWidget*>(peerMapWidget)->setPeerModel(model->getPeerTableModel());
            static_cast<PeerMapWidget*>(peerMapWidget)->setDemoMode(peerDemoMode);
        }
#endif
        model->getPeerTableModel()->setDemoPeersEnabled(peerDemoMode);
        model->getPeerTableModel()->setShowConnectedPeers(peerShowConnectedPeersCheckBox && peerShowConnectedPeersCheckBox->isChecked());
        model->getPeerTableModel()->setShowInactivePeers(peerShowInactiveCheckBox && peerShowInactiveCheckBox->isChecked());
        model->getPeerTableModel()->setOnlyDefcoinUserAgents(peerOnlyDefcoinUserAgentsCheckBox && peerOnlyDefcoinUserAgentsCheckBox->isChecked());
        model->getPeerTableModel()->setGeolocationEnabled(peerGetLocationCheckBox && peerGetLocationCheckBox->isChecked());
        model->getPeerTableModel()->setMappedASLookupEnabled(peerLookupMappedASCheckBox && peerLookupMappedASCheckBox->isChecked());
#if ENABLE_DEFCOIN_FUN_UI
        ui->peerWidget->setItemDelegateForColumn(PeerTableModel::PingHealth, new TrafficHealthDelegate(ui->peerWidget));
        ui->peerWidget->setItemDelegateForColumn(PeerTableModel::Country, new GeoIconDelegate(ui->peerWidget));
#endif
        ui->peerWidget->setItemDelegateForColumn(PeerTableModel::Sent, new ByteAmountDelegate(ui->peerWidget));
        ui->peerWidget->setItemDelegateForColumn(PeerTableModel::Received, new ByteAmountDelegate(ui->peerWidget));
        loadPeerColumnTitles();
        ui->peerWidget->verticalHeader()->hide();
        ui->peerWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->peerWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->peerWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
        ui->peerWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        ui->peerWidget->setWordWrap(true);
        ui->peerWidget->installEventFilter(this);
        ui->peerWidget->viewport()->installEventFilter(this);
        ui->peerWidget->horizontalHeader()->setStretchLastSection(false);
        ui->peerWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        ui->peerWidget->horizontalHeader()->setSectionsMovable(false);
        ui->peerWidget->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
        ui->peerWidget->horizontalHeader()->viewport()->installEventFilter(this);
        refreshAdaptiveHeader(ui->peerWidget);
        ui->peerWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        connect(ui->peerWidget->horizontalHeader(), &QHeaderView::sectionResized, this, [this](int logical_index, int, int new_size) {
            if (peerColumnsAutoResizing) return;
            peerColumnsUserResized = true;
            if (logical_index == PeerTableModel::PingHealth) {
                peerPingHealthBaseWidth = std::max(48, new_size);
                updatePeerPingHealthDensity();
            } else {
                stretchPeerPingHealthColumn();
            }
        });
        connect(ui->peerWidget->horizontalHeader(), &QHeaderView::sectionHandleDoubleClicked, this, [this](int logical_index) {
            if (logical_index < 0 || logical_index >= ui->peerWidget->model()->columnCount(QModelIndex())) return;
            peerColumnsAutoResizing = true;
            setPeerColumnWidthToDefault(logical_index);
            peerColumnsAutoResizing = false;
            peerColumnsUserResized = true;
            refreshAdaptiveHeader(ui->peerWidget);
            syncBanTableColumnsToPeerTable();
        });
        connect(model->getPeerTableModel(), &PeerTableModel::dataChanged, this, [this] {
            updatePeerTableVisibility();
            resizePeerTableColumnsToContents();
            applyPeerTableRowHeights(peerCompactRowHeights);
        });
        const QByteArray saved_peer_view = QSettings().value(peerTableHeaderStateKey).toByteArray();
        if (!saved_peer_view.isEmpty() && ui->peerWidget->horizontalHeader()->restoreState(saved_peer_view)) {
            peerColumnsUserResized = true;
            peerPingHealthBaseWidth = QSettings().value(peerTablePingHealthBaseWidthKey, std::max(48, ui->peerWidget->columnWidth(PeerTableModel::PingHealth))).toInt();
            peerCompactRowHeights = QSettings().value(peerTableCompactRowsKey, false).toBool();
            applyPeerTableRowHeights(peerCompactRowHeights);
            ui->peerViewLayout->setCurrentIndex(3);
#if !ENABLE_DEFCOIN_FUN_UI
            applyDefaultPeerColumnVisibility();
#endif
            ensurePeerDirectionColumnPlacement();
        } else {
            resizePeerTableColumnsToContents();
        }
        stretchPeerPingHealthColumn();
        updatePeerPingHealthDensity();
        updatePeerSummaryStats();

        // create peer table context menu actions
        QAction* disconnectAction = new QAction(tr("&Disconnect"), this);
        QAction* banAction1h      = new QAction(tr("Ban for") + " " + tr("1 &hour"), this);
        QAction* banAction24h     = new QAction(tr("Ban for") + " " + tr("1 &day"), this);
        QAction* banAction7d      = new QAction(tr("Ban for") + " " + tr("1 &week"), this);
        QAction* banAction365d    = new QAction(tr("Ban for") + " " + tr("1 &year"), this);
        QAction* copyPeerCellAction = new QAction(tr("&Copy cell"), this);

        // create peer table context menu
        peersTableContextMenu = new QMenu(this);
        peersTableContextMenu->addAction(copyPeerCellAction);
        peersTableContextMenu->addSeparator();
        peersTableContextMenu->addAction(disconnectAction);
        peersTableContextMenu->addAction(banAction1h);
        peersTableContextMenu->addAction(banAction24h);
        peersTableContextMenu->addAction(banAction7d);
        peersTableContextMenu->addAction(banAction365d);

        if (peerBanButton) {
            QMenu* ban_button_menu = new QMenu(peerBanButton);
            ban_button_menu->addAction(banAction1h);
            ban_button_menu->addAction(banAction24h);
            ban_button_menu->addAction(banAction7d);
            ban_button_menu->addAction(banAction365d);
            peerBanButton->setMenu(ban_button_menu);
        }

        connect(banAction1h, &QAction::triggered, [this] { banSelectedNode(60 * 60); });
        connect(banAction24h, &QAction::triggered, [this] { banSelectedNode(60 * 60 * 24); });
        connect(banAction7d, &QAction::triggered, [this] { banSelectedNode(60 * 60 * 24 * 7); });
        connect(banAction365d, &QAction::triggered, [this] { banSelectedNode(60 * 60 * 24 * 365); });
        connect(copyPeerCellAction, &QAction::triggered, this, [this] {
            const QModelIndex index = ui->peerWidget->currentIndex();
            if (!index.isValid()) return;
            QApplication::clipboard()->setText(selectedColumnText(ui->peerWidget, index));
        });

        // peer table context menu signals
        connect(ui->peerWidget, &QTableView::customContextMenuRequested, this, &RPCConsole::showPeersTableContextMenu);
        connect(ui->peerWidget, &QTableView::doubleClicked, this, [this](const QModelIndex& index) {
            if (!index.isValid()) return;
            const QString text = index.data(Qt::DisplayRole).toString().replace(QLatin1Char('\n'), QStringLiteral(", "));
            QApplication::clipboard()->setText(text);
        });
        connect(disconnectAction, &QAction::triggered, this, &RPCConsole::disconnectSelectedNode);

        // peer table signal handling - update peer details when selecting new node
        connect(ui->peerWidget->selectionModel(), &QItemSelectionModel::selectionChanged, this, &RPCConsole::peerSelected);
        // peer table signal handling - update peer details when new nodes are added to the model
        connect(model->getPeerTableModel(), &PeerTableModel::layoutChanged, this, &RPCConsole::peerLayoutChanged);
        // peer table signal handling - cache selected node ids
        connect(model->getPeerTableModel(), &PeerTableModel::layoutAboutToBeChanged, this, &RPCConsole::peerLayoutAboutToChange);

        // set up ban table
        ui->banlistWidget->setModel(model->getBanTableModel());
        ui->banlistWidget->verticalHeader()->hide();
        ui->banlistWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->banlistWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->banlistWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
        ui->banlistWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        ui->banlistWidget->setWordWrap(true);
        ui->banlistWidget->setItemDelegateForColumn(BanTableModel::Sent, new ByteAmountDelegate(ui->banlistWidget));
        ui->banlistWidget->setItemDelegateForColumn(BanTableModel::Received, new ByteAmountDelegate(ui->banlistWidget));
        ui->banlistWidget->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
        ui->banlistWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        ui->banlistWidget->horizontalHeader()->setStretchLastSection(false);
        ui->banlistWidget->horizontalHeader()->viewport()->installEventFilter(this);
        refreshAdaptiveHeader(ui->banlistWidget);
        resizeBanTableColumnsForFont();
        syncBanTableColumnsToPeerTable();

        // create ban table context menu action
        QAction* unbanAction = new QAction(tr("&Unban"), this);
        QAction* copyBanCellAction = new QAction(tr("&Copy cell"), this);

        // create ban table context menu
        banTableContextMenu = new QMenu(this);
        banTableContextMenu->addAction(copyBanCellAction);
        banTableContextMenu->addSeparator();
        banTableContextMenu->addAction(unbanAction);

        // ban table context menu signals
        connect(ui->banlistWidget, &QTableView::customContextMenuRequested, this, &RPCConsole::showBanTableContextMenu);
        connect(unbanAction, &QAction::triggered, this, &RPCConsole::unbanSelectedNode);
        connect(copyBanCellAction, &QAction::triggered, this, [this] {
            const QModelIndex index = ui->banlistWidget->currentIndex();
            if (!index.isValid()) return;
            QApplication::clipboard()->setText(selectedColumnText(ui->banlistWidget, index));
        });

        connect(ui->banlistWidget->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection& selected, const QItemSelection&) {
            if (selected.indexes().isEmpty()) return;
            updateBannedPeerDetail(selected.indexes().constFirst());
        });
        connect(ui->banlistWidget, &QTableView::doubleClicked, this, [this](const QModelIndex& index) {
            if (!index.isValid()) return;
            const QString text = index.data(Qt::DisplayRole).toString().replace(QLatin1Char('\n'), QStringLiteral(", "));
            QApplication::clipboard()->setText(text);
        });
        // ban table signal handling - ensure ban table is shown or hidden (if empty)
        connect(model->getBanTableModel(), &BanTableModel::layoutChanged, this, [this] {
            showOrHideBanTableIfRequired();
            resizeBanTableColumnsForFont();
            syncBanTableColumnsToPeerTable();
        });
        showOrHideBanTableIfRequired();

        // Provide initial values
        ui->clientVersion->setText(model->formatFullVersion());
        ui->clientUserAgent->setText(model->formatSubVersion());
        ui->dataDir->setText(model->dataDir());
        ui->blocksDir->setText(model->blocksDir());
        ui->startupTime->setText(model->formatClientStartupTime());
        ui->networkName->setText(QString::fromStdString(Params().NetworkIDString()));

        //Setup autocomplete and attach it
        QStringList wordList;
        std::vector<std::string> commandList = m_node.listRpcCommands();
        for (size_t i = 0; i < commandList.size(); ++i)
        {
            wordList << commandList[i].c_str();
            wordList << ("help " + commandList[i]).c_str();
        }

        wordList << "help-console";
        wordList.sort();
        autoCompleter = new QCompleter(wordList, this);
        autoCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
        // ui->lineEdit is initially disabled because running commands is only
        // possible from now on.
        ui->lineEdit->setEnabled(true);
        ui->lineEdit->setCompleter(autoCompleter);
        autoCompleter->popup()->installEventFilter(this);
        // Start thread to execute RPC commands.
        startExecutor();
        if (isVisible()) {
            model->getPeerTableModel()->startAutoRefresh();
            ui->trafficGraph->setUpdatesActive(true);
        } else {
            model->getPeerTableModel()->stopAutoRefresh();
            ui->trafficGraph->setUpdatesActive(false);
        }
        if (peerLocalDiscoveryCheckBox && peerLocalDiscoveryCheckBox->isChecked()) {
            startLocalPeerDiscovery();
        }
    }
    if (!model) {
        // Client model is being set to 0, this means shutdown() is about to be called.
        thread.quit();
        thread.wait();
    }
}

#ifdef ENABLE_WALLET
void RPCConsole::addWallet(WalletModel * const walletModel)
{
    // use name for text and wallet model for internal data object (to allow to move to a wallet id later)
    ui->WalletSelector->addItem(walletModel->getDisplayName(), QVariant::fromValue(walletModel));
    if (ui->WalletSelector->count() == 2 && !isVisible()) {
        // First wallet added, set to default so long as the window isn't presently visible (and potentially in use)
        ui->WalletSelector->setCurrentIndex(1);
    }
    if (ui->WalletSelector->count() > 2) {
        ui->WalletSelector->setVisible(true);
        ui->WalletSelectorLabel->setVisible(true);
    }
}

void RPCConsole::removeWallet(WalletModel * const walletModel)
{
    ui->WalletSelector->removeItem(ui->WalletSelector->findData(QVariant::fromValue(walletModel)));
    if (ui->WalletSelector->count() == 2) {
        ui->WalletSelector->setVisible(false);
        ui->WalletSelectorLabel->setVisible(false);
    }
}
#endif

static QString categoryClass(int category)
{
    switch(category)
    {
    case RPCConsole::CMD_REQUEST:  return "cmd-request"; break;
    case RPCConsole::CMD_REPLY:    return "cmd-reply"; break;
    case RPCConsole::CMD_ERROR:    return "cmd-error"; break;
    default:                       return "misc";
    }
}

void RPCConsole::fontBigger()
{
    setFontSize(consoleFontSize+1);
}

void RPCConsole::fontSmaller()
{
    setFontSize(consoleFontSize-1);
}

void RPCConsole::setFontSize(int newSize)
{
    QSettings settings;

    //don't allow an insane font size
    if (newSize < FONT_RANGE.width() || newSize > FONT_RANGE.height())
        return;

    // temp. store the console content
    QString str = ui->messagesWidget->toHtml();

    // replace font tags size in current content
    str.replace(QString("font-size:%1pt").arg(consoleFontSize), QString("font-size:%1pt").arg(newSize));
    str.remove(QRegExp(QStringLiteral("\\swidth=\"\\d+\"")));

    // store the new font size
    consoleFontSize = newSize;
    settings.setValue(fontSizeSettingsKey, consoleFontSize);

    // Reset measured icon sizes and stylesheet before re-adding existing content.
    QScrollBar* scroll_bar = ui->messagesWidget->verticalScrollBar();
    const float oldPosFactor = scroll_bar->maximum() > 0 ? static_cast<float>(scroll_bar->value()) / scroll_bar->maximum() : 1.0f;
    updateConsoleFontMetrics();
    QFont console_font = GUIUtil::fixedPitchFont();
    console_font.setPointSize(consoleFontSize);
    ui->messagesWidget->setFont(console_font);
    ui->lineEdit->setFont(console_font);
    ui->messagesWidget->setHtml(str);
    scroll_bar->setValue(oldPosFactor * scroll_bar->maximum());
}

void RPCConsole::refreshTextSizeSettings()
{
    QSettings settings;
    const int default_font_size = QApplication::font().pointSize();
    consoleFontSize = std::max(FONT_RANGE.width(), std::min(FONT_RANGE.height(), settings.value(fontSizeSettingsKey, default_font_size).toInt()));
    nodeWindowFontSize = std::max(FONT_RANGE.width(), std::min(FONT_RANGE.height(), settings.value(QStringLiteral("nodeWindowFontSize"), default_font_size).toInt()));
    connectedPeersPanelFontSize = std::max(FONT_RANGE.width(), std::min(FONT_RANGE.height(), settings.value(QStringLiteral("connectedPeersPanelFontSize"), nodeWindowFontSize).toInt()));

    applyNodeWindowFont();
    applyConnectedPeersPanelFont();
    setFontSize(consoleFontSize);
    relayoutAfterNodeFontChange();
    alignPeerDetailPane();
    updateTrafficMetricSwatches();
    if (peerUserAgentChartWidget) peerUserAgentChartWidget->updateGeometry();
    QTimer::singleShot(0, this, [this] {
        applyPeerTableRowHeights(peerCompactRowHeights);
        resizeConnectedPeersPanelColumnsForFont();
        resizeBanTableColumnsForFont();
        syncBanTableColumnsToPeerTable();
        updatePeerPingHealthDensity();
        updatePeerOverviewGeometry();
    });
}

void RPCConsole::updatePeerOverviewGeometry()
{
    QWidget* peer_overview = findChild<QWidget*>(QStringLiteral("peerOverview"));
    if (!peer_overview || !peer_overview->layout()) return;

    int available_width = 0;
    if (ui->splitter) {
        const int left_index = peerLeftScrollArea ? ui->splitter->indexOf(peerLeftScrollArea) : ui->splitter->indexOf(ui->widget_1);
        const QList<int> splitter_sizes = ui->splitter->sizes();
        if (left_index >= 0 && left_index < splitter_sizes.size()) {
            available_width = splitter_sizes.at(left_index);
        }
    }
    if (available_width <= 0) {
        available_width = peer_overview->width();
    }
    if (available_width <= 0) {
        available_width = peer_overview->contentsRect().width();
    }
    if (available_width <= 0 && peer_overview->parentWidget()) {
        available_width = peer_overview->parentWidget()->contentsRect().width();
    }
    if (available_width <= 0 && peerLeftScrollArea && peerLeftScrollArea->viewport()) {
        available_width = peerLeftScrollArea->viewport()->contentsRect().width();
    }
    if (available_width <= 0 && ui->widget_1) {
        available_width = ui->widget_1->contentsRect().width();
        if (ui->verticalLayout_7) {
            const QMargins margins = ui->verticalLayout_7->contentsMargins();
            available_width -= margins.left() + margins.right();
        }
    }
    available_width = std::max(1, available_width);
    if (available_width < 280) {
        QTimer::singleShot(0, this, &RPCConsole::updatePeerOverviewGeometry);
        return;
    }

    QLayout* overview_layout = peer_overview->layout();
    peer_overview->setMinimumHeight(0);
    peer_overview->setMaximumHeight(QWIDGETSIZE_MAX);
    if (peer_overview->width() != available_width) {
        peer_overview->resize(available_width, peer_overview->height());
    }
    overview_layout->invalidate();
    overview_layout->activate();

    const QMargins margins = overview_layout->contentsMargins();
    const int inner_width = std::max(1, available_width - margins.left() - margins.right());
    QWidget* text_column = nullptr;
    QWidget* chart_column = nullptr;
    for (QWidget* child : peer_overview->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (!child || !child->isVisible()) continue;
        if (child->objectName() == QStringLiteral("peerOverviewTextColumn")) {
            text_column = child;
        } else {
            chart_column = child;
        }
    }

    const int layout_spacing = qobject_cast<QBoxLayout*>(overview_layout) ? qobject_cast<QBoxLayout*>(overview_layout)->spacing() : 0;
    int chart_width = 0;
    if (chart_column && chart_column->isVisible()) {
        chart_width = std::min(chart_column->sizeHint().width(), std::max(220, inner_width / 3));
    }
    const int text_width = std::max(1, inner_width - chart_width - (chart_width > 0 ? layout_spacing : 0));

    int required_height = overview_layout->hasHeightForWidth()
        ? overview_layout->heightForWidth(available_width)
        : layoutHeightForWidth(overview_layout, available_width);
    if (text_column) {
        if (QLayout* text_layout = text_column->layout()) {
            required_height = std::max(required_height, layoutHeightForWidth(text_layout, text_width));
        } else {
            required_height = std::max(required_height, text_column->sizeHint().height());
        }
    }
    if (chart_column && chart_column->isVisible()) {
        required_height = std::max(required_height, chart_column->sizeHint().height());
    }
    required_height = std::max(required_height, overview_layout->minimumSize().height());
    const int final_height = std::max(required_height + margins.top() + margins.bottom(), 1) + 4;

    peer_overview->setMinimumHeight(final_height);
    peer_overview->setMaximumHeight(final_height);
    peer_overview->updateGeometry();
    if (peerLeftScrollArea) peerLeftScrollArea->updateGeometry();
}

void RPCConsole::applyNodeWindowFont()
{
    QFont f = font();
    f.setPointSize(nodeWindowFontSize);
    setFont(f);

    const QList<QWidget*> widgets = findChildren<QWidget*>();
    for (QWidget* widget : widgets) {
        if (!widget) continue;
        if (nodeTextControls && (widget == nodeTextControls || nodeTextControls->isAncestorOf(widget))) continue;
        if (peerPanelTextControls && (widget == peerPanelTextControls || peerPanelTextControls->isAncestorOf(widget))) continue;
        if (widget->objectName() == QStringLiteral("panelCollapseButton")) continue;
        widget->setFont(f);
    }

    if (QTabBar* tab_bar = ui->tabWidget->tabBar()) {
        tab_bar->setMinimumHeight(std::max(34, QFontMetrics(f).height() + 14));
    }
    resizeTextControlWidget(nodeTextControls, f);
    ui->trafficGraph->setFont(f);
    if (trafficWindowValueLabel) {
        trafficWindowValueLabel->setFixedWidth(fontMetricHorizontalAdvance(QFontMetrics(f), tr("11:43:00 AM - 11:43:00 AM")) + 64);
    }
    if (trafficWindowDurationLabel) {
        trafficWindowDurationLabel->setFixedWidth(fontMetricHorizontalAdvance(QFontMetrics(f), QStringLiteral("365d23h59m59s")) + 22);
    }
#if ENABLE_DEFCOIN_FUN_UI
    const int stats_width = std::min(360, std::max(300, fontMetricHorizontalAdvance(QFontMetrics(f), tr("Display Logarithmically")) + 96));
    ui->groupBox->setMinimumWidth(stats_width);
    ui->groupBox->setMaximumWidth(360);
#else
    ui->groupBox->setMinimumWidth(std::max(220, fontMetricHorizontalAdvance(QFontMetrics(f), tr("Received 999.99 MB")) + 112));
#endif
    const int checkbox_height = std::max(24, QFontMetrics(f).height() + 10);
    for (QCheckBox* box : {peerShowConnectedPeersCheckBox, peerShowInactiveCheckBox, peerShowBannedPeersCheckBox, peerLocalDiscoveryCheckBox, peerOnlyDefcoinUserAgentsCheckBox, peerGetLocationCheckBox, peerLookupMappedASCheckBox}) {
        if (!box) continue;
        box->setMinimumHeight(checkbox_height);
        box->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);
        box->updateGeometry();
    }
    const int peer_control_height = std::max(34, QFontMetrics(f).height() + 14);
    if (ui->peerViewLayoutLabel) {
        ui->peerViewLayoutLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        ui->peerViewLayoutLabel->setFixedHeight(peer_control_height);
    }
    if (ui->peerViewLayout) {
        ui->peerViewLayout->setFixedHeight(peer_control_height);
        GUIUtil::PolishComboBox(ui->peerViewLayout, 130);
    }
    for (QPushButton* button : {ui->peerEditViewButton, ui->peerSaveViewButton, ui->peerLoadViewButton, ui->peerResetViewButton, peerBanButton}) {
        if (!button) continue;
        button->setFixedHeight(peer_control_height);
        button->updateGeometry();
    }
    if (QPushButton* auto_size_button = findChild<QPushButton*>(QStringLiteral("peerAutoSizeColumnsButton"))) {
        auto_size_button->setFixedHeight(peer_control_height);
        auto_size_button->updateGeometry();
    }
    updatePeerOverviewGeometry();
}

void RPCConsole::applyPeerTableRowHeights(bool compact)
{
    peerCompactRowHeights = compact;
    QFont table_font = font();
    table_font.setPointSize(connectedPeersPanelFontSize > 0 ? connectedPeersPanelFontSize : nodeWindowFontSize);
    const QFontMetrics metrics(table_font);
    const int row_height = compact ? std::max(18, metrics.height() + 2) : std::max(20, metrics.height() + 8);

    for (QTableView* table : {ui->peerWidget, ui->banlistWidget}) {
        if (!table) continue;
        table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        table->setFont(table_font);
        table->verticalHeader()->setFont(table_font);
        table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        table->verticalHeader()->setMinimumSectionSize(row_height);
        table->verticalHeader()->setDefaultSectionSize(row_height);
        if (QHeaderView* horizontal = table->horizontalHeader()) {
            horizontal->setFont(table_font);
            horizontal->setProperty("defcoinCompactRows", compact);
        }
        refreshAdaptiveHeader(table);
        table->updateGeometry();
        table->viewport()->update();
    }
}

void RPCConsole::applyConnectedPeersPanelFont()
{
    QFont f = font();
    f.setPointSize(connectedPeersPanelFontSize > 0 ? connectedPeersPanelFontSize : nodeWindowFontSize);

    QFont heading_font = f;
    heading_font.setBold(true);
    if (connectedPeersLabel) connectedPeersLabel->setFont(heading_font);
    resizeTextControlWidget(peerPanelTextControls, f);
    if (QWidget* header = findChild<QWidget*>(QStringLiteral("connectedPeersHeader"))) {
        const QFontMetrics metrics(f);
        const int controls_height = peerPanelTextControls ? peerPanelTextControls->sizeHint().height() : 0;
        const int header_height = std::max(metrics.height() + 10, controls_height + 8);
        header->setFont(f);
        header->setMinimumHeight(header_height);
        header->setMaximumHeight(header_height);
        header->updateGeometry();
    }
    if (ui->peerWidget) {
        ui->peerWidget->setFont(f);
        ui->peerWidget->horizontalHeader()->setFont(f);
        ui->peerWidget->verticalHeader()->setFont(f);
        ui->peerWidget->updateGeometry();
    }
    if (ui->banHeading) {
        QFont ban_heading_font = f;
        ban_heading_font.setBold(true);
        ui->banHeading->setFont(ban_heading_font);
        const int heading_height = std::max(22, QFontMetrics(ban_heading_font).height() + 8);
        ui->banHeading->setMinimumHeight(heading_height);
        ui->banHeading->setMaximumHeight(heading_height);
    }
    if (ui->banlistWidget) {
        ui->banlistWidget->setFont(f);
        ui->banlistWidget->horizontalHeader()->setFont(f);
        ui->banlistWidget->verticalHeader()->setFont(f);
        ui->banlistWidget->updateGeometry();
    }
    applyPeerTableRowHeights(peerCompactRowHeights);
    resizeConnectedPeersPanelColumnsForFont();
    resizeBanTableColumnsForFont();
    syncBanTableColumnsToPeerTable();
}

void RPCConsole::resizeConnectedPeersPanelColumnsForFont()
{
    if (!ui->peerWidget || !ui->peerWidget->model()) return;

    const bool previous_auto_resizing = peerColumnsAutoResizing;
    peerColumnsAutoResizing = true;
    resizeTableColumnsTightly(ui->peerWidget);
    applyPeerTableRowHeights(peerCompactRowHeights);
    refreshAdaptiveHeader(ui->peerWidget);
    ui->peerWidget->horizontalHeader()->viewport()->update();
    ui->peerWidget->viewport()->update();
    peerColumnsAutoResizing = previous_auto_resizing;
}

void RPCConsole::resizeBanTableColumnsForFont()
{
    if (!ui->banlistWidget || !ui->banlistWidget->model()) return;

    resizeTableColumnsTightly(ui->banlistWidget);
    applyPeerTableRowHeights(peerCompactRowHeights);
    refreshAdaptiveHeader(ui->banlistWidget);
    ui->banlistWidget->horizontalHeader()->viewport()->update();
    ui->banlistWidget->viewport()->update();
}

void RPCConsole::relayoutAfterNodeFontChange()
{
    const QFont node_font = font();
    const QFontMetrics node_metrics(node_font);
    QFont connected_font = node_font;
    connected_font.setPointSize(connectedPeersPanelFontSize > 0 ? connectedPeersPanelFontSize : nodeWindowFontSize);
    const QFontMetrics connected_metrics(connected_font);

    if (ui->banlistWidget) {
        ui->banlistWidget->setFont(connected_font);
        ui->banlistWidget->horizontalHeader()->setFont(connected_font);
        ui->banlistWidget->verticalHeader()->setFont(connected_font);
        if (ui->banlistWidget->model()) {
            resizeBanTableColumnsForFont();
        }
        ui->banlistWidget->updateGeometry();
    }

    if (ui->banHeading) {
        QFont ban_heading_font = connected_font;
        ban_heading_font.setBold(true);
        ui->banHeading->setFont(ban_heading_font);
        ui->banHeading->setMinimumHeight(std::max(22, connected_metrics.height() + 8));
        ui->banHeading->setMaximumHeight(std::max(22, connected_metrics.height() + 8));
        ui->banHeading->updateGeometry();
    }

    if (trafficAvgLatencyLabel) {
        trafficAvgLatencyLabel->setMinimumWidth(fontMetricHorizontalAdvance(node_metrics, tr("9999 ms")) + 12);
    }
    if (trafficPeerAvgLatencyLabel) {
        trafficPeerAvgLatencyLabel->setMinimumWidth(fontMetricHorizontalAdvance(node_metrics, tr("9999 ms")) + 12);
    }
    if (trafficJitterLabel) {
        trafficJitterLabel->setMinimumWidth(fontMetricHorizontalAdvance(node_metrics, tr("9999 ms")) + 12);
    }
    if (ui->lblGraphRange) {
        ui->lblGraphRange->setMinimumWidth(fontMetricHorizontalAdvance(node_metrics, tr("100% All 12h59m59s")) + 24);
    }
    if (trafficWindowValueLabel) {
        trafficWindowValueLabel->setMinimumWidth(fontMetricHorizontalAdvance(node_metrics, tr("11:43:00 AM - 11:43:00 AM")) + 24);
    }
    if (ui->groupBox) {
#if ENABLE_DEFCOIN_FUN_UI
        int stats_width = fontMetricHorizontalAdvance(node_metrics, tr("Display Logarithmically")) + 96;
        stats_width = std::max(stats_width, fontMetricHorizontalAdvance(node_metrics, tr("Visible Avg Latency 9999 ms")) + 96);
        stats_width = std::min(360, std::max(300, stats_width));
        ui->groupBox->setMinimumWidth(stats_width);
        ui->groupBox->setMaximumWidth(stats_width);
        ui->groupBox->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
#else
        const int stats_width = fontMetricHorizontalAdvance(node_metrics, tr("Received 999.99 MB")) + 112;
        ui->groupBox->setMinimumWidth(std::max(220, stats_width));
#endif
        ui->groupBox->adjustSize();
        ui->groupBox->updateGeometry();
    }
    updatePeerOverviewGeometry();
    if (ui->trafficGraph) {
        ui->trafficGraph->updateGeometry();
        ui->trafficGraph->update();
    }

    resizeConnectedPeersPanelColumnsForFont();
    updatePeerPingHealthDensity();
    alignPeerDetailPane();

    if (ui->detailWidget) {
        ui->detailWidget->setFont(node_font);
        ui->detailWidget->adjustSize();
        ui->detailWidget->updateGeometry();
    }
    if (ui->peerHeading) {
        ui->peerHeading->setFont(node_font);
        ui->peerHeading->adjustSize();
        ui->peerHeading->updateGeometry();
    }
    if (peerUserAgentChartWidget) {
        peerUserAgentChartWidget->setFont(node_font);
        peerUserAgentChartWidget->updateGeometry();
        peerUserAgentChartWidget->update();
    }

    if (ui->tabWidget) ui->tabWidget->updateGeometry();
    if (layout()) layout()->activate();
    updateGeometry();

    applyPeerTableRowHeights(peerCompactRowHeights);
    QTimer::singleShot(0, this, [this] {
        updatePeerOverviewGeometry();
        if (ui->peerWidget && ui->peerWidget->model()) {
            resizeConnectedPeersPanelColumnsForFont();
            applyPeerTableRowHeights(peerCompactRowHeights);
            syncBanTableColumnsToPeerTable();
        }
        if (layout()) layout()->activate();
    });
}

void RPCConsole::setNodeWindowFontSize(int newSize)
{
    if (newSize < FONT_RANGE.width() || newSize > FONT_RANGE.height())
        return;

    const int old_node_size = nodeWindowFontSize > 0 ? nodeWindowFontSize : font().pointSize();
    const int old_peer_size = connectedPeersPanelFontSize > 0 ? connectedPeersPanelFontSize : old_node_size;
    const int peer_delta = newSize - old_node_size;
    nodeWindowFontSize = newSize;
    connectedPeersPanelFontSize = std::max(FONT_RANGE.width(), std::min(FONT_RANGE.height(), old_peer_size + peer_delta));
    applyNodeWindowFont();
    applyConnectedPeersPanelFont();
    setFontSize(newSize);
    QSettings().setValue(QStringLiteral("nodeWindowFontSize"), nodeWindowFontSize);
    QSettings().setValue(QStringLiteral("connectedPeersPanelFontSize"), connectedPeersPanelFontSize);
    if (QSettings().value(QStringLiteral("LinkWalletTextSizes"), true).toBool()) {
        QSettings().setValue(QStringLiteral("mainWindowFontSize"), nodeWindowFontSize);
        for (QWidget* top_level : QApplication::topLevelWidgets()) {
            if (!top_level || top_level == this) continue;
            if (top_level->metaObject()->indexOfMethod("refreshTextSizeSettings()") >= 0) {
                QMetaObject::invokeMethod(top_level, "refreshTextSizeSettings", Qt::QueuedConnection);
            }
        }
    }
    relayoutAfterNodeFontChange();
    if (peerUserAgentChartWidget) peerUserAgentChartWidget->updateGeometry();
    alignPeerDetailPane();
    updateTrafficMetricSwatches();
    QTimer::singleShot(0, this, [this] {
        if (!ui->peerWidget->model()) return;
        applyPeerTableRowHeights(peerCompactRowHeights);
        stretchPeerPingHealthColumn();
        updatePeerPingHealthDensity();
        syncBanTableColumnsToPeerTable();
    });
}

void RPCConsole::adjustNodeWindowFontSize(int delta)
{
    setNodeWindowFontSize(nodeWindowFontSize + delta);
}

void RPCConsole::setConnectedPeersPanelFontSize(int newSize)
{
    if (newSize < FONT_RANGE.width() || newSize > FONT_RANGE.height())
        return;

    connectedPeersPanelFontSize = newSize;
    applyConnectedPeersPanelFont();
    QSettings().setValue(QStringLiteral("connectedPeersPanelFontSize"), connectedPeersPanelFontSize);
    relayoutAfterNodeFontChange();
    updatePeerPingHealthDensity();
}

void RPCConsole::adjustConnectedPeersPanelFontSize(int delta)
{
    setConnectedPeersPanelFontSize(connectedPeersPanelFontSize + delta);
}

void RPCConsole::clear(bool clearHistory)
{
    ui->messagesWidget->clear();
    if(clearHistory)
    {
        history.clear();
        historyPtr = 0;
    }
    ui->lineEdit->clear();
    ui->lineEdit->setFocus();

    updateConsoleFontMetrics();

#ifdef Q_OS_MAC
    QString clsKey = "(⌘)-L";
#else
    QString clsKey = "Ctrl-L";
#endif

    message(CMD_REPLY, (tr("Welcome to the %1 RPC console.").arg(PACKAGE_NAME) + "<br>" +
                        tr("Use up and down arrows to navigate history, and %1 to clear screen.").arg("<b>"+clsKey+"</b>") + "<br>" +
                        tr("Type %1 for an overview of available commands.").arg("<b>help</b>") + "<br>" +
                        tr("For more information on using this console type %1.").arg("<b>help-console</b>") +
                        "<br><span class=\"secwarning\"><br>" +
                        tr("WARNING: Scammers have been active, telling users to type commands here, stealing their wallet contents. Do not use this console without fully understanding the ramifications of a command.") +
                        "</span>"),
                        true);
}

void RPCConsole::keyPressEvent(QKeyEvent *event)
{
    if(windowType() != Qt::Widget && event->key() == Qt::Key_Escape)
    {
        close();
    }
}

bool RPCConsole::handleTrafficSliderKey(QKeyEvent* event)
{
    if (!event || (event->key() != Qt::Key_Left && event->key() != Qt::Key_Right)) return false;
    QSlider* primary = lastTrafficSlider ? lastTrafficSlider : ui->sldGraphRange;
    QSlider* alternate = primary == ui->sldGraphRange ? trafficPanSlider : ui->sldGraphRange;
    if (primary == trafficWindowSlider) alternate = trafficPanSlider;
    QSlider* target = (event->modifiers() & Qt::MetaModifier) ? alternate : primary;
    if (!target || !target->isEnabled()) return false;

    const int direction = event->key() == Qt::Key_Right ? 1 : -1;
    const int step = std::max(1, target->singleStep());
    const int next = std::max(target->minimum(), std::min(target->maximum(), target->value() + direction * step));
    target->setValue(next);
    target->setFocus(Qt::OtherFocusReason);
    return true;
}

void RPCConsole::promptTrafficGraphDuration()
{
    if (!ui->trafficGraph || !ui->lblGraphRange) return;

    const QString current = compactDurationText(ui->trafficGraph->visibleRangeSeconds());
    bool ok = false;
    const QString text = QInputDialog::getText(
        this,
        tr("Network Traffic Window"),
        tr("Visible duration"),
        QLineEdit::Normal,
        current,
        &ok);
    if (!ok) return;

    qint64 seconds = 0;
    if (!parseCompactDurationText(text, seconds) || seconds <= 0) {
        QMessageBox::warning(this, tr("Network Traffic Window"), tr("Enter a duration such as 15m, 1h30m, or 90s."));
        return;
    }

    if (trafficAllHistoryCheckBox) {
        QSignalBlocker blocker(trafficAllHistoryCheckBox);
        trafficAllHistoryCheckBox->setChecked(false);
    }
    if (trafficWindowSlider) {
        QSignalBlocker blocker(trafficWindowSlider);
        trafficWindowSlider->setValue(trafficWindowSliderValueForSeconds(seconds));
    }

    const int scale = ui->sldGraphRange ? std::max(TRAFFIC_RANGE_SLIDER_MIN, std::min(ui->sldGraphRange->value(), TRAFFIC_RANGE_SLIDER_MAX)) : TRAFFIC_RANGE_SLIDER_MAX;
    ui->trafficGraph->setShowAllHistory(false);
    ui->trafficGraph->setGraphRangeSeconds(std::max<qint64>(1, std::min<qint64>(seconds, 365 * 24 * 60 * 60)));
    trafficScaleDisplayedSeconds = -1;
    setTrafficGraphScale(scale, true);
}

void RPCConsole::message(int category, const QString &message, bool html)
{
    QTime time = QTime::currentTime();
    QString timeString = time.toString();
    QString out;
    out += "<table class=\"console-message\"><tr><td class=\"time\">" + timeString + "</td>";
    out += "<td class=\"icon\"><img src=\"" + categoryClass(category) + "\"></td>";
    out += "<td class=\"message " + categoryClass(category) + "\">";
    if(html)
        out += message;
    else
        out += GUIUtil::HtmlEscape(message, false);
    out += "</td></tr></table>";
    ui->messagesWidget->append(out);
}

int RPCConsole::consoleTimeColumnWidth() const
{
    QFont time_font = GUIUtil::fixedPitchFont();
    time_font.setPointSize(consoleFontSize);
    QFontMetrics metrics(time_font);
    return fontMetricHorizontalAdvance(metrics, QStringLiteral("88:88:88")) + std::max(6, fontMetricHorizontalAdvance(metrics, QStringLiteral(" ")));
}

int RPCConsole::consoleIconColumnWidth() const
{
    QFont icon_font = GUIUtil::fixedPitchFont();
    icon_font.setPointSize(consoleFontSize);
    const int icon_size = std::max(10, QFontMetrics(icon_font).height() + 2);
    return icon_size + std::max(6, icon_size / 3);
}

QString RPCConsole::consoleMessageStyleSheet() const
{
    QFontInfo fixedFontInfo(GUIUtil::fixedPitchFont());
    const QString font_size = QStringLiteral("%1pt").arg(consoleFontSize);
    return QString(
        "table.console-message { border-collapse: collapse; margin: 0; padding: 0; width: 100%; } "
        "td.time { color: #808080; font-family: %1; font-size: %2; width: %3px; min-width: %3px; max-width: %3px; padding: 0 4px 0 0; white-space: nowrap; vertical-align: top; } "
        "td.icon { width: %4px; min-width: %4px; max-width: %4px; padding: 0 4px; vertical-align: top; } "
        "td.message { font-family: %1; font-size: %2; white-space: pre-wrap; vertical-align: top; } "
        "td.cmd-request { color: #006060; } "
        "td.cmd-error { color: red; } "
        ".secwarning { color: red; } "
        "b { color: #006060; } "
    ).arg(fixedFontInfo.family(), font_size)
     .arg(consoleTimeColumnWidth())
     .arg(consoleIconColumnWidth());
}

void RPCConsole::updateConsoleFontMetrics()
{
    QFont icon_font = GUIUtil::fixedPitchFont();
    icon_font.setPointSize(consoleFontSize);
    const int icon_size = std::max(10, QFontMetrics(icon_font).height() + 2);
    for (int i = 0; ICON_MAPPING[i].url; ++i) {
        ui->messagesWidget->document()->addResource(
            QTextDocument::ImageResource,
            QUrl(ICON_MAPPING[i].url),
            platformStyle->SingleColorImage(ICON_MAPPING[i].source).scaled(QSize(icon_size, icon_size), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }
    ui->messagesWidget->document()->setDefaultStyleSheet(consoleMessageStyleSheet());
}

void RPCConsole::updateNetworkState()
{
    QString connections = QString::number(clientModel->getNumConnections()) + " (";
    connections += tr("In:") + " " + QString::number(clientModel->getNumConnections(CONNECTIONS_IN)) + " / ";
    connections += tr("Out:") + " " + QString::number(clientModel->getNumConnections(CONNECTIONS_OUT)) + ")";

    if(!clientModel->node().getNetworkActive()) {
        connections += " (" + tr("Network activity disabled") + ")";
    }

    ui->numberOfConnections->setText(connections);
}

void RPCConsole::setNumConnections(int count)
{
    if (!clientModel)
        return;

    updateNetworkState();
}

void RPCConsole::setNetworkActive(bool networkActive)
{
    updateNetworkState();
}

void RPCConsole::setNumBlocks(int count, const QDateTime& blockDate, double nVerificationProgress, bool headers)
{
    if (!headers) {
        ui->numberOfBlocks->setText(QString::number(count));
        ui->lastBlockTime->setText(blockDate.toString());
    }
}

void RPCConsole::setMempoolSize(long numberOfTxs, size_t dynUsage)
{
    ui->mempoolNumberTxs->setText(QString::number(numberOfTxs));

    if (dynUsage < 1000000)
        ui->mempoolSize->setText(QString::number(dynUsage/1000.0, 'f', 2) + " KB");
    else
        ui->mempoolSize->setText(QString::number(dynUsage/1000000.0, 'f', 2) + " MB");
}

void RPCConsole::on_lineEdit_returnPressed()
{
    QString cmd = ui->lineEdit->text();

    if(!cmd.isEmpty())
    {
        std::string strFilteredCmd;
        try {
            std::string dummy;
            if (!RPCParseCommandLine(nullptr, dummy, cmd.toStdString(), false, &strFilteredCmd)) {
                // Failed to parse command, so we cannot even filter it for the history
                throw std::runtime_error("Invalid command line");
            }
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Error", QString("Error: ") + QString::fromStdString(e.what()));
            return;
        }

        ui->lineEdit->clear();

        cmdBeforeBrowsing = QString();

#ifdef ENABLE_WALLET
        WalletModel* wallet_model = ui->WalletSelector->currentData().value<WalletModel*>();

        if (m_last_wallet_model != wallet_model) {
            if (wallet_model) {
                message(CMD_REQUEST, tr("Executing command using \"%1\" wallet").arg(wallet_model->getWalletName()));
            } else {
                message(CMD_REQUEST, tr("Executing command without any wallet"));
            }
            m_last_wallet_model = wallet_model;
        }
#endif

        message(CMD_REQUEST, QString::fromStdString(strFilteredCmd));
        Q_EMIT cmdRequest(cmd, m_last_wallet_model);

        cmd = QString::fromStdString(strFilteredCmd);

        // Remove command, if already in history
        history.removeOne(cmd);
        // Append command to history
        history.append(cmd);
        // Enforce maximum history size
        while(history.size() > CONSOLE_HISTORY)
            history.removeFirst();
        // Set pointer to end of history
        historyPtr = history.size();

        // Scroll console view to end
        scrollToEnd();
    }
}

void RPCConsole::browseHistory(int offset)
{
    // store current text when start browsing through the history
    if (historyPtr == history.size()) {
        cmdBeforeBrowsing = ui->lineEdit->text();
    }

    historyPtr += offset;
    if(historyPtr < 0)
        historyPtr = 0;
    if(historyPtr > history.size())
        historyPtr = history.size();
    QString cmd;
    if(historyPtr < history.size())
        cmd = history.at(historyPtr);
    else if (!cmdBeforeBrowsing.isNull()) {
        cmd = cmdBeforeBrowsing;
    }
    ui->lineEdit->setText(cmd);
}

void RPCConsole::startExecutor()
{
    RPCExecutor *executor = new RPCExecutor(m_node);
    executor->moveToThread(&thread);

    // Replies from executor object must go to this object
    connect(executor, &RPCExecutor::reply, this, static_cast<void (RPCConsole::*)(int, const QString&)>(&RPCConsole::message));

    // Requests from this object must go to executor
    connect(this, &RPCConsole::cmdRequest, executor, &RPCExecutor::request);

    // Make sure executor object is deleted in its own thread
    connect(&thread, &QThread::finished, executor, &RPCExecutor::deleteLater);

    // Default implementation of QThread::run() simply spins up an event loop in the thread,
    // which is what we want.
    thread.start();
    QTimer::singleShot(0, executor, []() {
        util::ThreadRename("qt-rpcconsole");
    });
}

void RPCConsole::on_tabWidget_currentChanged(int index)
{
    if (ui->tabWidget->widget(index) == ui->tab_console) {
        ui->lineEdit->setFocus();
    } else if (ui->tabWidget->widget(index) == ui->tab_nettraffic) {
        ui->trafficGraph->setUpdatesActive(true);
        ui->trafficGraph->updateRates();
        setTrafficGraphScale(ui->sldGraphRange->value());
        QTimer::singleShot(0, this, [this] { ui->tabWidget->tabBar()->update(); });
        QTimer::singleShot(150, this, [this] { ui->tabWidget->tabBar()->update(); });
    } else if (ui->tabWidget->widget(index) == debugLogTab) {
        refreshDebugLogView(true);
        if (debugLogRefreshTimer && !debugLogRefreshTimer->isActive()) {
            debugLogRefreshTimer->start();
        }
    } else if (ui->tabWidget->widget(index) == ui->tab_peers) {
        QTimer::singleShot(0, this, &RPCConsole::updatePeerOverviewGeometry);
        QTimer::singleShot(50, this, &RPCConsole::updatePeerOverviewGeometry);
        QTimer::singleShot(0, this, &RPCConsole::alignPeerDetailPane);
        QTimer::singleShot(50, this, &RPCConsole::alignPeerDetailPane);
        QTimer::singleShot(0, this, [this] {
            showOrHideBanTableIfRequired();
            if (ui->peerWidget && ui->peerWidget->model()) {
                refreshAdaptiveHeader(ui->peerWidget);
            }
            if (ui->banlistWidget && ui->banlistWidget->model()) {
                refreshAdaptiveHeader(ui->banlistWidget);
            }
            applyPeerTableRowHeights(peerCompactRowHeights);
        });
    }
#if ENABLE_DEFCOIN_FUN_UI
    const bool show_density_controls = ui->tabWidget->widget(index) == ui->tab_peers;
    ui->peerPingDensity->setVisible(show_density_controls);
    ui->peerPingDensityMinus->setVisible(show_density_controls);
    ui->peerPingDensityPlus->setVisible(show_density_controls);
    if (peerPingDensityMetricLabel) peerPingDensityMetricLabel->setVisible(show_density_controls);
    if (peerTrafficHealthLegendLabel) peerTrafficHealthLegendLabel->setVisible(show_density_controls);
#endif
    if (ui->tabWidget->widget(index) != debugLogTab && debugLogRefreshTimer) {
        debugLogRefreshTimer->stop();
    }
    updateTrafficMetricSwatches();
}

void RPCConsole::on_openDebugLogfileButton_clicked()
{
    GUIUtil::openDebugLogfile();
}

void RPCConsole::refreshDebugLogView(bool force)
{
    if (!debugLogView) return;

    const QString path = debugLogFilePath();
    QFile file(path);
    if (!file.exists()) {
        debugLogLastSize = -1;
        debugLogView->setPlainText(tr("debug.log does not exist yet:\n%1").arg(path));
        return;
    }
    const qint64 file_size = file.size();
    if (!force && file_size == debugLogLastSize) return;
    debugLogLastSize = file_size;

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        debugLogView->setPlainText(tr("Unable to read debug.log:\n%1").arg(path));
        return;
    }

    qint64 session_offset = debugLogSessionStartSize;
    if (session_offset < 0 || session_offset > file_size) {
        session_offset = 0;
        debugLogSessionStartSize = 0;
    }
    if (session_offset > 0) {
        file.seek(session_offset);
    }
    const QByteArray bytes = file.readAll();
    QString text = QString::fromLocal8Bit(bytes);
    if (text.trimmed().isEmpty()) {
        text = tr("No debug.log records have been written since this app launch.");
    }

    QScrollBar* scroll_bar = debugLogView->verticalScrollBar();
    const bool was_at_bottom = scroll_bar->value() >= scroll_bar->maximum() - 4;
    const int previous_scroll = scroll_bar->value();
    const int previous_cursor = debugLogView->textCursor().position();
    debugLogView->setPlainText(text);
    if (was_at_bottom || force) {
        debugLogView->moveCursor(QTextCursor::End);
    } else {
        QTextCursor cursor = debugLogView->textCursor();
        cursor.setPosition(std::max(0, std::min(previous_cursor, debugLogView->document()->characterCount() - 1)));
        debugLogView->setTextCursor(cursor);
        scroll_bar->setValue(std::min(previous_scroll, scroll_bar->maximum()));
    }
}

void RPCConsole::scrollToEnd()
{
    QScrollBar *scrollbar = ui->messagesWidget->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

void RPCConsole::on_sldGraphRange_valueChanged(int value)
{
    lastTrafficSlider = ui->sldGraphRange;
    setTrafficGraphScale(value, true);
}

void RPCConsole::setTrafficGraphScale(int scale_percent, bool refresh_duration)
{
    const int effective_percent = std::max(TRAFFIC_RANGE_SLIDER_MIN, std::min(scale_percent, TRAFFIC_RANGE_SLIDER_MAX));
    ui->trafficGraph->setScalePercent(effective_percent);
    const bool all_history = trafficWindowSlider && trafficWindowSlider->value() >= TRAFFIC_WINDOW_SLIDER_ALL;
    ui->trafficGraph->setShowAllHistory(all_history);
    if (trafficAllHistoryCheckBox && trafficAllHistoryCheckBox->isChecked() != all_history) {
        QSignalBlocker blocker(trafficAllHistoryCheckBox);
        trafficAllHistoryCheckBox->setChecked(all_history);
    }
    if (refresh_duration || all_history || trafficScaleDisplayedSeconds < 0 || trafficScaleDisplayedPercent != effective_percent || trafficScaleDisplayedAllHistory != all_history) {
        trafficScaleDisplayedSeconds = ui->trafficGraph->visibleRangeSeconds();
        trafficScaleDisplayedPercent = effective_percent;
        trafficScaleDisplayedAllHistory = all_history;
    }
    const QString visible_duration = compactDurationText(trafficScaleDisplayedSeconds);
    ui->lblGraphRange->setText(all_history
        ? tr("%1%").arg(effective_percent)
        : tr("%1%").arg(effective_percent));
    if (trafficWindowDurationLabel) {
        trafficWindowDurationLabel->setText(all_history ? tr("All %1").arg(visible_duration) : compactDurationText(ui->trafficGraph->graphRangeSeconds()));
    }
    if (trafficPanSlider) {
        const bool can_pan = !all_history && ui->trafficGraph->hasScrollableHistory();
        TrafficWindowSlider* timeline_slider = static_cast<TrafficWindowSlider*>(trafficPanSlider);
        const int target_window_percent = all_history ? 100 : effective_percent;
        if (!timeline_slider->isDraggingViewport() && timeline_slider->windowPercent() != target_window_percent) {
            timeline_slider->setWindowPercent(target_window_percent);
        }
        ui->trafficGraph->setPanPercent(trafficPanSlider->value());
        trafficPanSlider->setToolTip(can_pan
            ? tr("Move the visible traffic window through retained history at the selected scale. Drag the blue rectangle to pan; drag either edge to resize the visible window.")
            : tr("Drag either edge of the blue rectangle to resize the visible window. Panning becomes active when older history exists outside the graph."));
        trafficPanSlider->setEnabled(true);
        trafficPanSlider->update();
        if (trafficWindowValueLabel) {
            trafficWindowValueLabel->setText(ui->trafficGraph->visibleWindowLabel());
            trafficWindowValueLabel->setToolTip(trafficPanSlider->toolTip());
        }
    }
    ui->sldGraphRange->update();
}

void RPCConsole::updateTrafficStats(quint64 totalBytesIn, quint64 totalBytesOut)
{
    ui->lblBytesIn->setText(GUIUtil::formatBytes(totalBytesIn));
    ui->lblBytesOut->setText(GUIUtil::formatBytes(totalBytesOut));
    setTrafficGraphScale(ui->sldGraphRange->value(), false);
}

void RPCConsole::peerSelected(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected);

    if (!clientModel || !clientModel->getPeerTableModel() || selected.indexes().isEmpty())
        return;

    const CNodeCombinedStats *stats = clientModel->getPeerTableModel()->getNodeStats(selected.indexes().first().row());
    if (stats) {
        if (peerBanButton) peerBanButton->setEnabled(stats->nodeStats.nodeid >= 0);
        updateNodeDetail(stats);
    }
}

void RPCConsole::peerLayoutAboutToChange()
{
    QModelIndexList selected = ui->peerWidget->selectionModel()->selectedIndexes();
    cachedNodeids.clear();
    for(int i = 0; i < selected.size(); i++)
    {
        const CNodeCombinedStats *stats = clientModel->getPeerTableModel()->getNodeStats(selected.at(i).row());
        cachedNodeids.append(stats->nodeStats.nodeid);
    }
}

void RPCConsole::peerLayoutChanged()
{
    if (!clientModel || !clientModel->getPeerTableModel())
        return;

    resizePeerTableColumnsToContents();
    applyPeerTableRowHeights(peerCompactRowHeights);
    updatePeerTableVisibility();
    updatePeerSummaryStats();

    const CNodeCombinedStats *stats = nullptr;
    bool fUnselect = false;
    bool fReselect = false;

    if (cachedNodeids.empty()) // no node selected yet
        return;

    // find the currently selected row
    int selectedRow = -1;
    QModelIndexList selectedModelIndex = ui->peerWidget->selectionModel()->selectedIndexes();
    if (!selectedModelIndex.isEmpty()) {
        selectedRow = selectedModelIndex.first().row();
    }

    // check if our detail node has a row in the table (it may not necessarily
    // be at selectedRow since its position can change after a layout change)
    int detailNodeRow = clientModel->getPeerTableModel()->getRowByNodeId(cachedNodeids.first());

    if (detailNodeRow < 0)
    {
        // detail node disappeared from table (node disconnected)
        fUnselect = true;
    }
    else
    {
        if (detailNodeRow != selectedRow)
        {
            // detail node moved position
            fUnselect = true;
            fReselect = true;
        }

        // get fresh stats on the detail node.
        stats = clientModel->getPeerTableModel()->getNodeStats(detailNodeRow);
    }

    if (fUnselect && selectedRow >= 0) {
        clearSelectedNode();
    }

    if (fReselect)
    {
        for(int i = 0; i < cachedNodeids.size(); i++)
        {
            ui->peerWidget->selectRow(clientModel->getPeerTableModel()->getRowByNodeId(cachedNodeids.at(i)));
        }
    }

    if (stats)
        updateNodeDetail(stats);
}

int RPCConsole::peerColumnDefaultWidth(int column) const
{
    if (!clientModel || !clientModel->getPeerTableModel() || !ui->peerWidget->model()) return 80;

    PeerTableModel* model = clientModel->getPeerTableModel();
    QFont sample_font = ui->peerWidget->font();
    if (column == PeerTableModel::Address) {
        sample_font = QFont(QStringLiteral("Space Mono"));
        sample_font.setStyleHint(QFont::Monospace);
        sample_font.setPointSize(ui->peerWidget->font().pointSize());
    }
    const QFontMetrics metrics(sample_font);
    QStringList samples;
    auto addHeaderSamples = [&samples](const QString& title) {
        const QString trimmed = title.trimmed();
        if (trimmed.isEmpty()) return;
        QString widest_part;
        for (const QString& part : trimmed.split(QRegExp(QStringLiteral("[\\s:;\\-/\\.]+")), QString::SkipEmptyParts)) {
            if (part.size() > widest_part.size()) widest_part = part;
        }
        samples << (widest_part.isEmpty() ? trimmed : widest_part);
    };
    addHeaderSamples(model->columnTitle(column));
    addHeaderSamples(model->defaultColumnTitle(column));
    switch (column) {
    case PeerTableModel::NetNodeId: samples << QStringLiteral("9999"); break;
    case PeerTableModel::UniqueId: samples << QStringLiteral("9999"); break;
    case PeerTableModel::Address:
        samples << QStringLiteral("255.255.255.255:10332")
                << QStringLiteral("192.168.100.122:1337")
                << QStringLiteral("[2600:3c00::f03c:92ff:fe17:805d]:10332")
                << QStringLiteral("[fe80:1234:4567:8901:0202:b3ff:fe1e:8329]:10332");
        break;
    case PeerTableModel::Port: samples << QStringLiteral("65535") << QStringLiteral("10332"); break;
    case PeerTableModel::Fqdn: samples << QStringLiteral("66.42.91.225.vultrusercontent.com") << QStringLiteral("c-73-52-182-204.hsd1.ut.comcast.net") << QStringLiteral("[NA: LAN]"); break;
    case PeerTableModel::CustomHostname: samples << QStringLiteral("seed.defcoin.io") << QStringLiteral("Davids-Mac-mini.local [macOS 26.4]") << QStringLiteral("WINDOWS11"); break;
    case PeerTableModel::Version: samples << QStringLiteral("70017"); break;
    case PeerTableModel::Services: samples << QStringLiteral("N B W CF"); break;
    case PeerTableModel::AvgPing:
    case PeerTableModel::Ping:
    case PeerTableModel::Jitter: samples << QStringLiteral("999 ms"); break;
    case PeerTableModel::PingHealth: samples << QStringLiteral("Traffic Health"); break;
    case PeerTableModel::Sent:
    case PeerTableModel::Received: samples << QStringLiteral("999 KB") << QStringLiteral("1.23 MB"); break;
    case PeerTableModel::Subversion: samples << QStringLiteral("DefcoinCore:1.0.0") << QStringLiteral("Defcoin Core:0.21.5.4"); break;
    case PeerTableModel::UserAgentCount: samples << QStringLiteral("99"); break;
    case PeerTableModel::Country: samples << QString::fromUtf8("🇺🇸"); break;
    case PeerTableModel::CityState: samples << QStringLiteral("Austin, Texas") << QStringLiteral("New York, New York"); break;
    case PeerTableModel::Permissions: samples << QStringLiteral("NETWORK & BLOOM & WITNESS"); break;
    case PeerTableModel::Direction: samples << tr("Outbound") << tr("Inbound"); break;
    case PeerTableModel::StartingBlock:
    case PeerTableModel::SyncedHeaders:
    case PeerTableModel::SyncedBlocks: samples << QStringLiteral("9999999"); break;
    case PeerTableModel::ConnectionTime:
    case PeerTableModel::LastSend:
    case PeerTableModel::LastReceive: samples << QStringLiteral("99 h 59 m"); break;
    case PeerTableModel::PingWait:
    case PeerTableModel::MinPing: samples << QStringLiteral("999 ms"); break;
    case PeerTableModel::TimeOffset: samples << QStringLiteral("-999 s"); break;
    case PeerTableModel::MappedAS: samples << QStringLiteral("999999"); break;
    case PeerTableModel::ASName: samples << QStringLiteral("OVH, FR") << QStringLiteral("GOOGLE - Google LLC, US"); break;
    case PeerTableModel::ASHostingCompany: samples << QStringLiteral("Google LLC") << QStringLiteral("OVH"); break;
    case PeerTableModel::Seed: samples << QString::fromUtf8("✓"); break;
    }

    for (int row = 0; row < std::min(model->rowCount(QModelIndex()), 25); ++row) {
        samples << model->data(model->index(row, column, QModelIndex()), Qt::DisplayRole).toString();
    }

    int width = 0;
    for (QString sample : samples) {
        const QStringList lines = sample.split(QLatin1Char('\n'));
        for (const QString& line : lines) {
            width = std::max(width, fontMetricHorizontalAdvance(metrics, line));
        }
    }

    const int padding = fontMetricHorizontalAdvance(metrics, QStringLiteral("—")) + 6;
    width += padding;

    const int em = std::max(6, fontMetricHorizontalAdvance(metrics, QStringLiteral("M")));
    auto clamp = [](int value, int min_value, int max_value) {
        return std::max(min_value, std::min(value, max_value));
    };

    switch (column) {
    case PeerTableModel::NetNodeId: return clamp(width + 8, 5 * em, 6 * em);
    case PeerTableModel::UniqueId: return clamp(width + 8, 5 * em, 7 * em);
    case PeerTableModel::Address: return clamp(width + std::max(44, em * 3), 24 * em, 120 * em);
    case PeerTableModel::Port: return clamp(width, 5 * em, 8 * em);
    case PeerTableModel::Fqdn: return clamp(width, 10 * em, 28 * em);
    case PeerTableModel::CustomHostname: return clamp(width, 12 * em, 26 * em);
    case PeerTableModel::Version: return clamp(width, 6 * em, 10 * em);
    case PeerTableModel::Services: return clamp(width, 5 * em, 9 * em);
    case PeerTableModel::AvgPing:
    case PeerTableModel::Ping:
    case PeerTableModel::Jitter: return clamp(width, 6 * em, 12 * em);
    case PeerTableModel::PingHealth: return std::max(peerPingHealthBaseWidth, clamp(width, 10 * em, 30 * em));
    case PeerTableModel::Sent: return clamp(width, 6 * em, 10 * em);
    case PeerTableModel::Received: return clamp(width, 7 * em, 11 * em);
    case PeerTableModel::Subversion: return clamp(width, 15 * em, 28 * em);
    case PeerTableModel::UserAgentCount: return clamp(width, 4 * em, 8 * em);
    case PeerTableModel::Country: return clamp(width, 3 * em, 5 * em);
    case PeerTableModel::CityState: return clamp(width, 12 * em, 23 * em);
    case PeerTableModel::Seed: return clamp(width, 3 * em, 6 * em);
    case PeerTableModel::MappedAS: return clamp(width, 7 * em, 12 * em);
    case PeerTableModel::ASName: return clamp(width, 10 * em, 24 * em);
    case PeerTableModel::ASHostingCompany: return clamp(width, 10 * em, 22 * em);
    case PeerTableModel::Direction: return clamp(width, 8 * em, 12 * em);
    default: return clamp(width, 8 * em, 24 * em);
    }
}

int RPCConsole::tightTableColumnWidth(QTableView* table, int column) const
{
    if (!table || !table->model() || column < 0 || column >= table->model()->columnCount(QModelIndex())) return 80;

    const QFontMetrics cell_metrics(table->font());
    QFont header_font = table->horizontalHeader() ? table->horizontalHeader()->font() : table->font();
    header_font.setBold(true);
    const QFontMetrics header_metrics(header_font);
    const int em = std::max(6, fontMetricHorizontalAdvance(cell_metrics, QStringLiteral("M")));
    const bool is_peer_model = qobject_cast<PeerTableModel*>(table->model()) != nullptr;
    const bool is_ban_model = qobject_cast<BanTableModel*>(table->model()) != nullptr;
    const bool address_column = (is_peer_model && column == PeerTableModel::Address) ||
                                (is_ban_model && column == BanTableModel::Address);
    const int padding = address_column
        ? (peerCompactRowHeights ? std::max(36, em * 3) : std::max(52, em * 4))
        : (peerCompactRowHeights ? std::max(6, em) : std::max(10, em * 2));
    const int sort_room = table->horizontalHeader() && table->horizontalHeader()->isSortIndicatorShown() &&
                          table->horizontalHeader()->sortIndicatorSection() == column ? 18 : 0;

    int header_width = 0;
    const QString header_text = adaptiveHeaderDisplayText(table->model()->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString());
    const QStringList header_lines = header_text.split(QLatin1Char('\n'));
    for (const QString& line : header_lines) {
        const QStringList tokens = line.split(QRegExp(QStringLiteral("[\\s:;\\-/\\.]+")), QString::SkipEmptyParts);
        if (tokens.isEmpty()) {
            header_width = std::max(header_width, fontMetricHorizontalAdvance(header_metrics, line.trimmed()));
        } else {
            for (const QString& token : tokens) {
                header_width = std::max(header_width, fontMetricHorizontalAdvance(header_metrics, token));
            }
        }
    }

    int content_width = 0;
    const int row_count = table->model()->rowCount(QModelIndex());
    for (int row = 0; row < row_count; ++row) {
        const QModelIndex cell = table->model()->index(row, column, QModelIndex());
        const QString value = table->model()->data(cell, Qt::DisplayRole).toString();
        QFont cell_font = table->font();
        const QVariant font_role = table->model()->data(cell, Qt::FontRole);
        if (font_role.canConvert<QFont>()) cell_font = qvariant_cast<QFont>(font_role);
        const QFontMetrics metrics(cell_font);
        const QStringList lines = value.split(QLatin1Char('\n'));
        for (const QString& line : lines) {
            const QString measured_line = address_column ? line : line.trimmed();
            content_width = std::max(content_width, fontMetricHorizontalAdvance(metrics, measured_line));
        }
        if (address_column) {
            const QString raw_address = table->model()->data(cell, is_peer_model ? PeerTableModel::PeerAddressRole : Qt::UserRole).toString();
            const QString raw_ports = is_peer_model ? table->model()->data(cell, PeerTableModel::PeerPortsRole).toString() : QString();
            if (!raw_address.isEmpty()) {
                QString endpoint = raw_address;
                if (endpoint.contains(QLatin1Char(':')) && !endpoint.startsWith(QLatin1Char('['))) {
                    endpoint = QStringLiteral("[%1]").arg(endpoint);
                }
                if (!raw_ports.isEmpty()) {
                    endpoint += QStringLiteral(":%1").arg(raw_ports.split(QLatin1Char(',')).first().trimmed());
                }
                content_width = std::max(content_width, fontMetricHorizontalAdvance(metrics, endpoint));
            }
        }
    }

    int width = std::max(header_width + sort_room, content_width) + padding;
    if (address_column) {
        // QFontMetrics can undercount punctuation-heavy IPv6 endpoints by a
        // few pixels across platforms, especially with bracketed IPv6 plus
        // fixed-width dotted IPv4 alignment. Keep the guard endpoint-specific
        // so compact auto-size stays tight without clipping brackets or ports.
        width += peerCompactRowHeights ? std::max(34, em * 3) : std::max(46, em * 4);
    }
    if (column == PeerTableModel::PingHealth || column == BanTableModel::PingHealth) {
        width = std::max(width, 80);
    }
    return std::max(28, width);
}

void RPCConsole::resizeTableColumnsTightly(QTableView* table)
{
    if (!table || !table->model()) return;
    const int column_count = table->model()->columnCount(QModelIndex());
    for (int column = 0; column < column_count; ++column) {
        if (table->isColumnHidden(column)) continue;
        table->setColumnWidth(column, tightTableColumnWidth(table, column));
    }
    if (table == ui->peerWidget || table == ui->banlistWidget) {
        int table_width = table->frameWidth() * 2 + 4;
        for (int column = 0; column < column_count; ++column) {
            if (!table->isColumnHidden(column)) table_width += table->columnWidth(column);
        }
        if (table->verticalScrollBar() && table->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff) {
            table_width += table->verticalScrollBar()->sizeHint().width();
        }
        table->setMaximumWidth(std::max(220, table_width));
    }
    refreshAdaptiveHeader(table);
    table->horizontalHeader()->viewport()->update();
    table->viewport()->update();
}

void RPCConsole::setPeerColumnWidthToDefault(int column)
{
    if (!ui->peerWidget || !ui->peerWidget->model() || column < 0 || column >= ui->peerWidget->model()->columnCount(QModelIndex())) return;
    const int width = tightTableColumnWidth(ui->peerWidget, column);
    if (column == PeerTableModel::PingHealth) {
        peerPingHealthBaseWidth = width;
    }
    ui->peerWidget->setColumnWidth(column, width);
}

void RPCConsole::resizePeerTableColumnsToContents()
{
    if (peerColumnsUserResized || !ui->peerWidget->model()) return;

    peerColumnsAutoResizing = true;
    resizeTableColumnsTightly(ui->peerWidget);
    ensurePeerDirectionColumnPlacement();
    refreshAdaptiveHeader(ui->peerWidget);
    syncBanTableColumnsToPeerTable();
    peerColumnsAutoResizing = false;
    updatePeerPingHealthDensity();
}

void RPCConsole::applyPeerViewPreset(int preset_index)
{
    if (!ui->peerWidget->model()) return;

    if (preset_index == 3) {
        loadPeerViewLayout();
        return;
    }

    ui->peerWidget->horizontalHeader()->setSectionsMovable(false);
    peerColumnsAutoResizing = true;
    peerCompactRowHeights = preset_index == 1;
    if (preset_index == 1) {
        applyDefaultPeerColumnVisibility();
        ui->peerWidget->setColumnWidth(PeerTableModel::NetNodeId, 52);
        ui->peerWidget->setColumnWidth(PeerTableModel::Direction, 74);
        ui->peerWidget->setColumnWidth(PeerTableModel::Address, peerColumnDefaultWidth(PeerTableModel::Address));
        ui->peerWidget->setColumnWidth(PeerTableModel::Port, 54);
        ui->peerWidget->setColumnWidth(PeerTableModel::Fqdn, 140);
        ui->peerWidget->setColumnWidth(PeerTableModel::CustomHostname, 150);
        ui->peerWidget->setColumnWidth(PeerTableModel::Version, 66);
        ui->peerWidget->setColumnWidth(PeerTableModel::Services, 52);
        ui->peerWidget->setColumnWidth(PeerTableModel::AvgPing, 68);
        ui->peerWidget->setColumnWidth(PeerTableModel::Ping, 76);
        ui->peerWidget->setColumnWidth(PeerTableModel::Jitter, 62);
        peerPingHealthBaseWidth = 140;
        ui->peerWidget->setColumnWidth(PeerTableModel::Sent, 60);
        ui->peerWidget->setColumnWidth(PeerTableModel::Received, 68);
        ui->peerWidget->setColumnWidth(PeerTableModel::Subversion, tightTableColumnWidth(ui->peerWidget, PeerTableModel::Subversion));
        ui->peerWidget->setColumnWidth(PeerTableModel::UserAgentCount, 60);
        ui->peerWidget->setColumnWidth(PeerTableModel::Country, 42);
        ui->peerWidget->setColumnWidth(PeerTableModel::CityState, 126);
        stretchPeerPingHealthColumn();
        peerColumnsUserResized = true;
    } else if (preset_index == 2) {
#if ENABLE_DEFCOIN_FUN_UI
        const int column_count = ui->peerWidget->model()->columnCount(QModelIndex());
        for (int column = 0; column < column_count; ++column) {
            ui->peerWidget->setColumnHidden(column, false);
        }
        ui->peerWidget->setColumnWidth(PeerTableModel::NetNodeId, 58);
        ui->peerWidget->setColumnWidth(PeerTableModel::Direction, 84);
        ui->peerWidget->setColumnWidth(PeerTableModel::Address, peerColumnDefaultWidth(PeerTableModel::Address));
        ui->peerWidget->setColumnWidth(PeerTableModel::Port, 62);
        ui->peerWidget->setColumnWidth(PeerTableModel::Fqdn, 240);
        ui->peerWidget->setColumnWidth(PeerTableModel::CustomHostname, 220);
        ui->peerWidget->setColumnWidth(PeerTableModel::Version, 82);
        ui->peerWidget->setColumnWidth(PeerTableModel::Services, 72);
        ui->peerWidget->setColumnWidth(PeerTableModel::AvgPing, 86);
        ui->peerWidget->setColumnWidth(PeerTableModel::Ping, 96);
        ui->peerWidget->setColumnWidth(PeerTableModel::Jitter, 82);
        peerPingHealthBaseWidth = 150;
        ui->peerWidget->setColumnWidth(PeerTableModel::PingHealth, 150);
        ui->peerWidget->setColumnWidth(PeerTableModel::Sent, 88);
        ui->peerWidget->setColumnWidth(PeerTableModel::Received, 88);
        ui->peerWidget->setColumnWidth(PeerTableModel::Subversion, 220);
        ui->peerWidget->setColumnWidth(PeerTableModel::UserAgentCount, 70);
        ui->peerWidget->setColumnWidth(PeerTableModel::Country, 54);
        ui->peerWidget->setColumnWidth(PeerTableModel::CityState, 160);
        ui->peerWidget->setColumnWidth(PeerTableModel::Permissions, 112);
        ui->peerWidget->setColumnWidth(PeerTableModel::Direction, 84);
        ui->peerWidget->setColumnWidth(PeerTableModel::StartingBlock, 108);
        ui->peerWidget->setColumnWidth(PeerTableModel::SyncedHeaders, 112);
        ui->peerWidget->setColumnWidth(PeerTableModel::SyncedBlocks, 108);
        ui->peerWidget->setColumnWidth(PeerTableModel::ConnectionTime, 112);
        ui->peerWidget->setColumnWidth(PeerTableModel::LastSend, 88);
        ui->peerWidget->setColumnWidth(PeerTableModel::LastReceive, 100);
        ui->peerWidget->setColumnWidth(PeerTableModel::PingWait, 88);
        ui->peerWidget->setColumnWidth(PeerTableModel::MinPing, 82);
        ui->peerWidget->setColumnWidth(PeerTableModel::TimeOffset, 88);
        ui->peerWidget->setColumnWidth(PeerTableModel::MappedAS, 82);
        ui->peerWidget->setColumnWidth(PeerTableModel::ASName, 180);
        ui->peerWidget->setColumnWidth(PeerTableModel::ASHostingCompany, 160);
        ui->peerWidget->setColumnWidth(PeerTableModel::Seed, 46);
        peerColumnsUserResized = true;
#else
        applyDefaultPeerColumnVisibility();
        ui->peerWidget->setColumnWidth(PeerTableModel::NetNodeId, 58);
        ui->peerWidget->setColumnWidth(PeerTableModel::Direction, 84);
        ui->peerWidget->setColumnWidth(PeerTableModel::Address, peerColumnDefaultWidth(PeerTableModel::Address));
        ui->peerWidget->setColumnWidth(PeerTableModel::Ping, 96);
        ui->peerWidget->setColumnWidth(PeerTableModel::Sent, 88);
        ui->peerWidget->setColumnWidth(PeerTableModel::Received, 88);
        ui->peerWidget->setColumnWidth(PeerTableModel::Subversion, 220);
        peerColumnsUserResized = true;
#endif
    } else {
        applyDefaultPeerColumnVisibility();
        peerColumnsUserResized = false;
        resizePeerTableColumnsToContents();
    }
    ensurePeerDirectionColumnPlacement();
    peerColumnsAutoResizing = false;
    applyPeerTableRowHeights(peerCompactRowHeights);
    syncBanTableColumnsToPeerTable();
    updatePeerPingHealthDensity();
}

void RPCConsole::savePeerViewLayout()
{
    if (!ui->peerWidget->model()) return;
    QSettings settings;
    settings.setValue(peerTableHeaderStateKey, ui->peerWidget->horizontalHeader()->saveState());
    settings.setValue(peerTablePingHealthBaseWidthKey, peerPingHealthBaseWidth);
    settings.setValue(peerTableCompactRowsKey, peerCompactRowHeights);
    savePeerColumnTitles();
    ui->peerViewLayout->setCurrentIndex(3);
}

void RPCConsole::loadPeerViewLayout()
{
    if (!ui->peerWidget->model()) return;
    const QByteArray saved_peer_view = QSettings().value(peerTableHeaderStateKey).toByteArray();
    if (saved_peer_view.isEmpty() || !ui->peerWidget->horizontalHeader()->restoreState(saved_peer_view)) {
        QMessageBox::information(this, tr("No saved peer view"), tr("No saved peer view layout is available yet."));
        ui->peerViewLayout->setCurrentIndex(0);
        return;
    }
    peerColumnsUserResized = true;
    peerPingHealthBaseWidth = QSettings().value(peerTablePingHealthBaseWidthKey, std::max(48, ui->peerWidget->columnWidth(PeerTableModel::PingHealth))).toInt();
    peerCompactRowHeights = QSettings().value(peerTableCompactRowsKey, false).toBool();
    ui->peerViewLayout->setCurrentIndex(3);
#if !ENABLE_DEFCOIN_FUN_UI
    applyDefaultPeerColumnVisibility();
#endif
    ensurePeerDirectionColumnPlacement();
    applyPeerTableRowHeights(peerCompactRowHeights);
    stretchPeerPingHealthColumn();
    updatePeerPingHealthDensity();
    syncBanTableColumnsToPeerTable();
}

void RPCConsole::restorePeerPanelLayout()
{
    const QByteArray state = QSettings().value(peerPanelSplitterStateKey).toByteArray();
    if (!state.isEmpty() && ui->splitter->restoreState(state)) {
        const QList<int> sizes = ui->splitter->sizes();
        const bool collapsed = QSettings().value(QStringLiteral("PeerDetailPanelCollapsed"), false).toBool();
        if (collapsed || sizes.size() < 2 || (sizes.at(1) >= 240 && sizes.at(1) <= 340)) {
            return;
        }
    }
    resetPeerPanelLayout();
}

void RPCConsole::savePeerPanelLayout()
{
    QSettings().setValue(peerPanelSplitterStateKey, ui->splitter->saveState());
}

void RPCConsole::resetPeerPanelLayout()
{
    QSettings().remove(peerPanelSplitterStateKey);
    const int width = std::max(1, ui->splitter->width());
    if (ui->tabWidget->currentWidget() == ui->tab_peers && width < 760) {
        ui->splitter->setSizes(QList<int>() << width << 0);
        if (ui->widget_2) ui->widget_2->setVisible(false);
        return;
    }
    if (ui->widget_2) ui->widget_2->setVisible(true);
    const int detail_width = std::min(320, std::max(280, width / 5));
    ui->splitter->setSizes(QList<int>() << std::max(220, width - detail_width) << detail_width);
}

void RPCConsole::resetPeerViewLayout()
{
    QSettings settings;
    settings.remove(peerTableHeaderStateKey);
    settings.remove(peerTableColumnTitlesKey);
    settings.remove(peerTablePingHealthBaseWidthKey);
    settings.remove(peerTableCompactRowsKey);
    settings.remove(peerPanelSplitterStateKey);
    resetNodeVisualState();
    if (clientModel && clientModel->getPeerTableModel()) {
        clientModel->getPeerTableModel()->resetColumnTitles();
    }
    ui->peerWidget->horizontalHeader()->setSectionsMovable(false);
    ui->peerViewLayout->setCurrentIndex(0);
    peerColumnsUserResized = false;
    peerCompactRowHeights = false;
    applyDefaultPeerColumnVisibility();
    resetPeerPanelLayout();
    resizePeerTableColumnsToContents();
    ensurePeerDirectionColumnPlacement();
    if (ui->peerWidget && ui->peerWidget->model()) {
        ui->peerWidget->sortByColumn(PeerTableModel::NetNodeId, Qt::AscendingOrder);
        ui->peerWidget->horizontalHeader()->setSortIndicator(PeerTableModel::NetNodeId, Qt::AscendingOrder);
    }
    if (ui->banlistWidget && ui->banlistWidget->model()) {
        ui->banlistWidget->sortByColumn(BanTableModel::NetNodeId, Qt::AscendingOrder);
        ui->banlistWidget->horizontalHeader()->setSortIndicator(BanTableModel::NetNodeId, Qt::AscendingOrder);
    }
    applyPeerTableRowHeights(peerCompactRowHeights);
    syncBanTableColumnsToPeerTable();
}

void RPCConsole::editPeerViewLayout()
{
    if (!ui->peerWidget->model()) return;
    PeerTableModel* model = clientModel ? clientModel->getPeerTableModel() : nullptr;
    if (!model) return;

    QHeaderView* header = ui->peerWidget->horizontalHeader();
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Edit Peer View"));
    dialog.resize(620, 520);

    QVBoxLayout* root = new QVBoxLayout(&dialog);
    QLabel* intro = new QLabel(tr("Select columns, drag them into the order you want, then adjust the selected column title and width."));
    intro->setWordWrap(true);
    root->addWidget(intro);

    QListWidget* list = new QListWidget(&dialog);
    list->setDragDropMode(QAbstractItemView::InternalMove);
    list->setDefaultDropAction(Qt::MoveAction);
    for (int visual = 0; visual < header->count(); ++visual) {
        const int logical = header->logicalIndex(visual);
        if (logical >= 0 && logical < model->columnCount(QModelIndex())) {
#if !ENABLE_DEFCOIN_FUN_UI
            const bool standard_peer_column =
                logical == PeerTableModel::NetNodeId ||
                logical == PeerTableModel::Direction ||
                logical == PeerTableModel::Address ||
                logical == PeerTableModel::Ping ||
                logical == PeerTableModel::Sent ||
                logical == PeerTableModel::Received ||
                logical == PeerTableModel::Subversion;
            if (!standard_peer_column) continue;
#endif
            list->addItem(makePeerColumnItem(model, header, logical, peerColumnDefaultWidth(logical)));
        }
    }
    root->addWidget(list, 1);

    QGridLayout* editor = new QGridLayout();
    QLineEdit* title_edit = new QLineEdit(&dialog);
    QSpinBox* width_spin = new QSpinBox(&dialog);
    QCheckBox* compact_rows = new QCheckBox(tr("Compact table cell size"), &dialog);
    compact_rows->setToolTip(tr("Use tighter Connected Peers and Banned Peers row heights and column padding in this saved view."));
    compact_rows->setChecked(peerCompactRowHeights);
    width_spin->setRange(36, 600);
    width_spin->setSuffix(tr(" px"));
    editor->addWidget(new QLabel(tr("Header")), 0, 0);
    editor->addWidget(title_edit, 0, 1);
    editor->addWidget(new QLabel(tr("Width")), 1, 0);
    editor->addWidget(width_spin, 1, 1);
    root->addLayout(editor);
    root->addWidget(compact_rows);

    QHBoxLayout* move_layout = new QHBoxLayout();
    QPushButton* select_all_button = new QPushButton(tr("Select All"), &dialog);
    QPushButton* select_none_button = new QPushButton(tr("Select None"), &dialog);
    QPushButton* up_button = new QPushButton(tr("Move Up"), &dialog);
    QPushButton* down_button = new QPushButton(tr("Move Down"), &dialog);
    move_layout->addWidget(select_all_button);
    move_layout->addWidget(select_none_button);
    move_layout->addStretch();
    move_layout->addWidget(up_button);
    move_layout->addWidget(down_button);
    root->addLayout(move_layout);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    root->addWidget(buttons);

    auto load_selected_item = [list, title_edit, width_spin] {
        QListWidgetItem* item = list->currentItem();
        const bool enabled = item != nullptr;
        title_edit->setEnabled(enabled);
        width_spin->setEnabled(enabled);
        if (!enabled) return;
        title_edit->setText(item->text());
        width_spin->setValue(item->data(Qt::UserRole + 1).toInt());
    };
    connect(list, &QListWidget::currentItemChanged, &dialog, [load_selected_item](QListWidgetItem*, QListWidgetItem*) {
        load_selected_item();
    });
    connect(title_edit, &QLineEdit::textEdited, &dialog, [list](const QString& text) {
        if (QListWidgetItem* item = list->currentItem()) item->setText(text);
    });
    connect(width_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), &dialog, [list](int value) {
        if (QListWidgetItem* item = list->currentItem()) item->setData(Qt::UserRole + 1, value);
    });
    connect(select_all_button, &QPushButton::clicked, &dialog, [list] {
        for (int row = 0; row < list->count(); ++row) {
            list->item(row)->setCheckState(Qt::Checked);
        }
    });
    connect(select_none_button, &QPushButton::clicked, &dialog, [list] {
        for (int row = 0; row < list->count(); ++row) {
            list->item(row)->setCheckState(Qt::Unchecked);
        }
    });
    connect(up_button, &QPushButton::clicked, &dialog, [list] {
        const int row = list->currentRow();
        if (row <= 0) return;
        QListWidgetItem* item = list->takeItem(row);
        list->insertItem(row - 1, item);
        list->setCurrentItem(item);
    });
    connect(down_button, &QPushButton::clicked, &dialog, [list] {
        const int row = list->currentRow();
        if (row < 0 || row >= list->count() - 1) return;
        QListWidgetItem* item = list->takeItem(row);
        list->insertItem(row + 1, item);
        list->setCurrentItem(item);
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (list->count() > 0) list->setCurrentRow(0);
    load_selected_item();

    if (dialog.exec() != QDialog::Accepted) return;

    header->setSectionsMovable(true);
    peerColumnsAutoResizing = true;
    for (int row = 0; row < list->count(); ++row) {
        QListWidgetItem* item = list->item(row);
        const int logical = item->data(Qt::UserRole).toInt();
        ui->peerWidget->setColumnHidden(logical, item->checkState() != Qt::Checked);
    }
    for (int row = 0; row < list->count(); ++row) {
        QListWidgetItem* item = list->item(row);
        const int logical = item->data(Qt::UserRole).toInt();
        const int current_visual = header->visualIndex(logical);
        if (current_visual >= 0 && current_visual != row) {
            header->moveSection(current_visual, row);
        }
    }
    for (int row = 0; row < list->count(); ++row) {
        QListWidgetItem* item = list->item(row);
        const int logical = item->data(Qt::UserRole).toInt();
        ui->peerWidget->setColumnWidth(logical, item->data(Qt::UserRole + 1).toInt());
        if (logical == PeerTableModel::PingHealth) {
            peerPingHealthBaseWidth = item->data(Qt::UserRole + 1).toInt();
        }
        model->setColumnTitle(logical, item->text());
    }
    peerColumnsAutoResizing = false;
    peerColumnsUserResized = true;
    peerCompactRowHeights = compact_rows->isChecked();
    applyPeerTableRowHeights(peerCompactRowHeights);
    stretchPeerPingHealthColumn();
    updatePeerPingHealthDensity();
    syncBanTableColumnsToPeerTable();
    savePeerViewLayout();
    QTimer::singleShot(0, this, [this] {
        if (!ui->peerWidget->model()) return;
        stretchPeerPingHealthColumn();
        refreshAdaptiveHeader(ui->peerWidget);
        syncBanTableColumnsToPeerTable();
        ui->peerWidget->horizontalHeader()->viewport()->update();
        ui->peerWidget->viewport()->update();
        ui->peerWidget->update();
    });
}

void RPCConsole::loadPeerColumnTitles()
{
    if (!clientModel || !clientModel->getPeerTableModel()) return;

    const QStringList titles = QSettings().value(peerTableColumnTitlesKey).toStringList();
    PeerTableModel* model = clientModel->getPeerTableModel();
    if (!titles.isEmpty() && titles.size() != model->columnCount(QModelIndex())) {
        QSettings().remove(peerTableColumnTitlesKey);
        model->resetColumnTitles();
        return;
    }
    for (int column = 0; column < std::min(titles.size(), model->columnCount(QModelIndex())); ++column) {
        QString title = titles.at(column);
        if (title == QStringLiteral("Node\nID") ||
            title == QStringLiteral("Node ID") ||
            title == QStringLiteral("NodeID") ||
            title == QStringLiteral("NodeId") ||
            title == QStringLiteral("Node/Service") ||
            title == QStringLiteral("Node:port") ||
            title == QStringLiteral("IP Address:Port") ||
            title == QStringLiteral("Custom\nHostname") ||
            title == QStringLiteral("Custom Hostname") ||
            title == QStringLiteral("UA\nCount") ||
            title == QStringLiteral("City,\nState") ||
            title == QStringLiteral("Received")) {
            title = model->defaultColumnTitle(column);
        }
        model->setColumnTitle(column, title);
    }
}

void RPCConsole::savePeerColumnTitles()
{
    if (!clientModel || !clientModel->getPeerTableModel()) return;

    QStringList titles;
    PeerTableModel* model = clientModel->getPeerTableModel();
    for (int column = 0; column < model->columnCount(QModelIndex()); ++column) {
        titles << model->columnTitle(column);
    }
    QSettings().setValue(peerTableColumnTitlesKey, titles);
}

void RPCConsole::applyDefaultPeerColumnVisibility()
{
    if (!ui->peerWidget->model()) return;
    const int column_count = ui->peerWidget->model()->columnCount(QModelIndex());
    for (int column = 0; column < column_count; ++column) {
#if ENABLE_DEFCOIN_FUN_UI
        ui->peerWidget->setColumnHidden(column, column >= PeerTableModel::Permissions && column != PeerTableModel::Direction);
#else
        const bool standard_visible =
            column == PeerTableModel::NetNodeId ||
            column == PeerTableModel::Direction ||
            column == PeerTableModel::Address ||
            column == PeerTableModel::Ping ||
            column == PeerTableModel::Sent ||
            column == PeerTableModel::Received ||
            column == PeerTableModel::Subversion;
        ui->peerWidget->setColumnHidden(column, !standard_visible);
#endif
    }
    ensurePeerDirectionColumnPlacement();
}

void RPCConsole::ensurePeerDirectionColumnPlacement()
{
    if (!ui->peerWidget || !ui->peerWidget->model()) return;
    if (ui->peerWidget->isColumnHidden(PeerTableModel::Direction) || ui->peerWidget->isColumnHidden(PeerTableModel::Address)) return;

    QHeaderView* header = ui->peerWidget->horizontalHeader();
    if (!header) return;
    const int direction_visual = header->visualIndex(PeerTableModel::Direction);
    const int address_visual = header->visualIndex(PeerTableModel::Address);
    if (direction_visual >= 0 && address_visual >= 0 && direction_visual > address_visual) {
        header->moveSection(direction_visual, address_visual);
    }
}

void RPCConsole::syncBanTableColumnsToPeerTable()
{
    if (!ui->peerWidget || !ui->peerWidget->model() || !ui->banlistWidget || !ui->banlistWidget->model()) return;

    QHeaderView* peer_header = ui->peerWidget->horizontalHeader();
    QHeaderView* ban_header = ui->banlistWidget->horizontalHeader();
    if (!peer_header || !ban_header) return;

    const int peer_column_count = ui->peerWidget->model()->columnCount(QModelIndex());
    for (int peer_column = 0; peer_column < peer_column_count; ++peer_column) {
        const int ban_column = banColumnForPeerColumn(peer_column);
        if (ban_column < 0 || ban_column >= BanTableModel::ColumnCount) continue;
        ui->banlistWidget->setColumnHidden(ban_column, ui->peerWidget->isColumnHidden(peer_column));
        if (!ui->peerWidget->isColumnHidden(peer_column)) {
            ui->banlistWidget->setColumnWidth(ban_column, ui->peerWidget->columnWidth(peer_column));
        }
    }
    ui->banlistWidget->setColumnHidden(BanTableModel::Bantime, false);

    QVector<int> desired_order;
    for (int visual = 0; visual < peer_header->count(); ++visual) {
        const int peer_column = peer_header->logicalIndex(visual);
        const int ban_column = banColumnForPeerColumn(peer_column);
        if (ban_column >= 0 && ban_column < BanTableModel::ColumnCount) desired_order << ban_column;
    }
    desired_order << BanTableModel::Bantime;
    for (int target_visual = 0; target_visual < desired_order.size(); ++target_visual) {
        const int logical = desired_order.at(target_visual);
        const int current_visual = ban_header->visualIndex(logical);
        if (current_visual >= 0 && current_visual != target_visual) {
            ban_header->moveSection(current_visual, target_visual);
        }
    }

    refreshAdaptiveHeader(ui->banlistWidget);
    applyPeerTableRowHeights(peerCompactRowHeights);
}

void RPCConsole::stretchPeerPingHealthColumn()
{
#if !ENABLE_DEFCOIN_FUN_UI
    return;
#endif
    if (!ui->peerWidget->model()) return;

    const bool previous_auto_resizing = peerColumnsAutoResizing;
    peerColumnsAutoResizing = true;

    if (ui->peerWidget->isColumnHidden(PeerTableModel::PingHealth)) {
        const bool saved_layout_active = ui->peerViewLayout && ui->peerViewLayout->currentIndex() == 3;
        if (!previous_auto_resizing && peerColumnsUserResized && saved_layout_active) {
            peerColumnsAutoResizing = previous_auto_resizing;
            return;
        }

        peerColumnsAutoResizing = previous_auto_resizing;
        return;
    }

    int used_width = 0;
    const int column_count = ui->peerWidget->model()->columnCount(QModelIndex());
    for (int column = 0; column < column_count; ++column) {
        if (column == PeerTableModel::PingHealth || ui->peerWidget->isColumnHidden(column)) continue;
        used_width += ui->peerWidget->columnWidth(column);
    }
    const int available = ui->peerWidget->viewport()->width() - used_width - 8;
    const int target = std::max(peerPingHealthBaseWidth, available);
    if (std::abs(ui->peerWidget->columnWidth(PeerTableModel::PingHealth) - target) > 1) {
        ui->peerWidget->setColumnWidth(PeerTableModel::PingHealth, target);
    }
    peerColumnsAutoResizing = previous_auto_resizing;
}

void RPCConsole::alignPeerDetailPane()
{
    if (peerDetailTopSpacer && peerDetailTopSpacer->height() != 0) {
        peerDetailTopSpacer->setFixedHeight(0);
    }
    if (QSettings().value(QStringLiteral("PeerDetailPanelCollapsed"), false).toBool()) {
        setPeerDetailPanelCollapsed(true);
        return;
    }
    if (ui->splitter && ui->widget_2) {
        const int available_width = ui->splitter->width();
        const bool hide_detail_for_tiny_width =
            ui->tabWidget->currentWidget() == ui->tab_peers &&
            available_width > 0 &&
            available_width < 760;
        ui->widget_2->setVisible(!hide_detail_for_tiny_width);
        if (!hide_detail_for_tiny_width) {
            const QList<int> sizes = ui->splitter->sizes();
            const bool needs_initial_size = sizes.size() < 2 || sizes.at(1) <= 0;
            if (needs_initial_size || sizes.at(1) > 340) {
                const int detail_width = std::min(320, std::max(280, available_width / 5));
                ui->splitter->setSizes(QList<int>{std::max(220, available_width - detail_width), detail_width});
            }
        }
    }
    updatePeerInspectorHeight();
}

void RPCConsole::updatePeerInspectorHeight()
{
    if (!ui->scrollArea || !ui->detailWidget) return;

    const int desired_height = ui->detailWidget->isVisible()
        ? ui->detailWidget->sizeHint().height() + 2 * ui->scrollArea->frameWidth() + 10
        : 180;
    const int button_height =
        (peerPingButton && peerPingButton->isVisible() ? peerPingButton->sizeHint().height() + 8 : 0) +
        (peerTraceRouteButton && peerTraceRouteButton->isVisible() ? peerTraceRouteButton->sizeHint().height() + 8 : 0);
    const int heading_height = ui->peerHeading ? ui->peerHeading->sizeHint().height() + 8 : 0;
    const int available_height = std::max(180, height() - heading_height - button_height - 80);
    const int target_height = std::min(desired_height, available_height);
    ui->scrollArea->setMinimumHeight(target_height);
    ui->scrollArea->setMaximumHeight(target_height);
    ui->scrollArea->updateGeometry();
}

void RPCConsole::updatePeerPingHealthDensity()
{
#if !ENABLE_DEFCOIN_FUN_UI
    if (peerPingDensityMetricLabel) peerPingDensityMetricLabel->hide();
    if (peerTrafficHealthLegendLabel) peerTrafficHealthLegendLabel->hide();
    return;
#endif
    if (!clientModel || !clientModel->getPeerTableModel() || !ui->peerWidget->model()) return;

    const int visible_seconds = std::max(1, std::min(60, ui->peerPingDensity->value()));
    const int sample_limit = std::max(4, static_cast<int>(std::ceil((visible_seconds * 1000.0) / MODEL_UPDATE_DELAY)));
    clientModel->getPeerTableModel()->setPingHealthSampleLimit(sample_limit);
    if (peerPingDensityMetricLabel) {
        const double seconds = (static_cast<double>(sample_limit) * MODEL_UPDATE_DELAY) / 1000.0;
        peerPingDensityMetricLabel->setText(tr("Visible: ~%1s").arg(QString::number(seconds, 'f', seconds < 10.0 ? 1 : 0)));
    }
    if (peerTrafficHealthLegendLabel) {
        peerTrafficHealthLegendLabel->setText(tr("<span style=\"color:#31d158\">fast</span> / <span style=\"color:#ff9f0a\">moderate</span> / <span style=\"color:#ff453a\">slow</span>"));
    }
}

void RPCConsole::updatePeerSummaryStats()
{
    if (!clientModel || !clientModel->getPeerTableModel()) return;

    PeerTableModel* model = clientModel->getPeerTableModel();
    const int current_peers = model->activePeerCount();
    const int seen_peers = model->seenPeerCount();
    QHash<QString, int> user_agent_counts;
    int active_node_count = 0;
    seenPeerKeys.remove(QStringLiteral("local-node"));

    const int displayed_peers = model->rowCount(QModelIndex());
    for (int row = 0; row < displayed_peers; ++row) {
        const CNodeCombinedStats* stats = model->getNodeStats(row);
        if (!stats) continue;
        if (stats->nodeStats.nodeid != -1) {
            const QString peer_key = QString::fromStdString(stats->nodeStats.addr.ToStringIPPort());
            if (!peer_key.isEmpty()) {
                seenPeerKeys.insert(peer_key);
            }
        }
        if (!stats->fActive) continue;

        ++active_node_count;
        QString user_agent = QString::fromStdString(stats->nodeStats.cleanSubVer).trimmed();
        while (user_agent.startsWith(QLatin1Char('/'))) {
            user_agent.remove(0, 1);
        }
        while (user_agent.endsWith(QLatin1Char('/'))) {
            user_agent.chop(1);
        }
        if (user_agent.isEmpty()) {
            user_agent = tr("Unknown");
        }
        user_agent_counts[user_agent] += 1;
    }

#if !ENABLE_DEFCOIN_FUN_UI
    active_node_count = current_peers;
    user_agent_counts.clear();
    for (int row = 0; row < displayed_peers; ++row) {
        const CNodeCombinedStats* stats = model->getNodeStats(row);
        if (!stats || stats->nodeStats.nodeid == -1) continue;
        const QString peer_key = QString::fromStdString(stats->nodeStats.addr.ToStringIPPort());
        if (!peer_key.isEmpty()) {
            seenPeerKeys.insert(peer_key);
        }
        if (!stats->fActive) continue;
        QString user_agent = QString::fromStdString(stats->nodeStats.cleanSubVer);
        while (user_agent.startsWith(QLatin1Char('/'))) user_agent.remove(0, 1);
        while (user_agent.endsWith(QLatin1Char('/'))) user_agent.chop(1);
        if (user_agent.isEmpty()) {
            user_agent = tr("Unknown");
        }
        user_agent_counts[user_agent] += 1;
    }
#endif

    if (peerStatsBannerLabel) {
        peerStatsBannerLabel->setText(tr("<b>Current Defcoin Peers:</b> %1 &nbsp;&nbsp; <b>Seen Since Open:</b> %2 <span style=\"color:#7f8c8d;\">(resets when Node window closes)</span>")
                                      .arg(current_peers)
                                      .arg(seen_peers));
    }

    if (!peerUserAgentSummaryLabel) return;

    QVector<QPair<QString, int>> user_agents;
    user_agents.reserve(user_agent_counts.size());
    for (auto it = user_agent_counts.constBegin(); it != user_agent_counts.constEnd(); ++it) {
        user_agents.append(qMakePair(it.key(), it.value()));
    }
    std::sort(user_agents.begin(), user_agents.end(), [](const QPair<QString, int>& left, const QPair<QString, int>& right) {
        if (left.second != right.second) return left.second > right.second;
        return left.first < right.first;
    });

    QStringList parts;
    QVector<QPair<QString, int>> displayed_user_agents;
    int other_count = 0;
    for (int i = 0; i < user_agents.size(); ++i) {
        if (i >= 7) {
            other_count += user_agents.at(i).second;
            continue;
        }
        displayed_user_agents.append(user_agents.at(i));
        const double percent = active_node_count > 0 ? (100.0 * user_agents.at(i).second / active_node_count) : 0.0;
        parts << tr("%1: %2 (%3%)").arg(user_agents.at(i).first).arg(user_agents.at(i).second).arg(QString::number(percent, 'f', 0));
    }
    if (other_count > 0) {
        const double percent = active_node_count > 0 ? (100.0 * other_count / active_node_count) : 0.0;
        parts << tr("Others: %1 (%2%)").arg(other_count).arg(QString::number(percent, 'f', 0));
        displayed_user_agents.append(qMakePair(tr("Others"), other_count));
    }

    peerUserAgentSummaryLabel->setText(parts.isEmpty() ? tr("<b>User Agents:</b> none") : tr("<b>User Agents:</b> %1").arg(parts.join(QStringLiteral(" &nbsp; | &nbsp; "))));
    if (peerUserAgentChartWidget) {
        static_cast<UserAgentPieWidget*>(peerUserAgentChartWidget)->setCounts(displayed_user_agents, active_node_count);
    }
}

void RPCConsole::updatePeerTableVisibility()
{
    const bool show_connected = peerShowConnectedPeersCheckBox && peerShowConnectedPeersCheckBox->isChecked();
    const bool show_inactive = peerShowInactiveCheckBox && peerShowInactiveCheckBox->isChecked();
    const bool show_table = show_connected || show_inactive;

    if (connectedPeersHeaderWidget) connectedPeersHeaderWidget->setVisible(show_table);
    if (ui->peerWidget) ui->peerWidget->setVisible(show_table);

    bool has_active_rows = false;
    bool has_inactive_rows = false;
    if (clientModel && clientModel->getPeerTableModel()) {
        PeerTableModel* model = clientModel->getPeerTableModel();
        const int rows = model->rowCount(QModelIndex());
        for (int row = 0; row < rows; ++row) {
            const CNodeCombinedStats* stats = model->getNodeStats(row);
            if (!stats) continue;
            if (stats->fActive) has_active_rows = true;
            else has_inactive_rows = true;
        }
    }

    if (connectedPeersLabel) {
        if (show_connected && show_inactive && has_inactive_rows) {
            connectedPeersLabel->setText(tr("Connected peers & inactive peers"));
        } else if (!show_connected && show_inactive) {
            connectedPeersLabel->setText(tr("Inactive peers"));
        } else if (show_inactive && !has_active_rows && has_inactive_rows) {
            connectedPeersLabel->setText(tr("Inactive peers"));
        } else {
            connectedPeersLabel->setText(tr("Connected peers"));
        }
    }

    if (ui->verticalLayout_7) {
        ui->verticalLayout_7->setAlignment(Qt::AlignTop);
        ui->verticalLayout_7->invalidate();
        ui->verticalLayout_7->activate();
    }
    updateGeometry();
}

void RPCConsole::startLocalPeerDiscovery()
{
    if (localPeerDiscoveryTimer || !clientModel) return;
    if (Params().NetworkIDString() != "main") return;
    if (!QSettings().value(QStringLiteral("DiscoverLocalNetworkNodes"), true).toBool()) return;

    localPeerDiscoveryTimer = new QTimer(this);
    localPeerDiscoveryTimer->setInterval(30 * 1000);
    connect(localPeerDiscoveryTimer, &QTimer::timeout, this, &RPCConsole::runLocalPeerDiscoveryScan);
    localPeerDiscoveryTimer->start();
    QTimer::singleShot(3000, this, &RPCConsole::runLocalPeerDiscoveryScan);
}

void RPCConsole::stopLocalPeerDiscovery()
{
    if (localPeerDiscoveryTimer) {
        localPeerDiscoveryTimer->stop();
        localPeerDiscoveryTimer->deleteLater();
        localPeerDiscoveryTimer = nullptr;
    }
    localPeerProbeQueue.clear();
    localPeerProbeQueued.clear();
}

void RPCConsole::runLocalPeerDiscoveryScan()
{
    if (!QSettings().value(QStringLiteral("DiscoverLocalNetworkNodes"), true).toBool()) {
        stopLocalPeerDiscovery();
        return;
    }
    if (!clientModel || !clientModel->node().getNetworkActive()) return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        const QNetworkInterface::InterfaceFlags flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning) || (flags & QNetworkInterface::IsLoopBack)) continue;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                enqueueLocalPeerSubnet(entry.ip(), entry.netmask(), now);
            }
        }
    }

    QProcess* arp = new QProcess(this);
    connect(arp, &QProcess::errorOccurred, this, [arp](QProcess::ProcessError) {
        arp->deleteLater();
    });
    connect(arp, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, arp, now](int, QProcess::ExitStatus) {
        const QString output = QString::fromLocal8Bit(arp->readAllStandardOutput());
        QRegExp ip_expr(QStringLiteral("\\((\\d+\\.\\d+\\.\\d+\\.\\d+)\\)"));
        const QSet<QString> local_ips = localIPv4Strings();
        int pos = 0;
        while ((pos = ip_expr.indexIn(output, pos)) != -1) {
            const QString ip = ip_expr.cap(1);
            pos += ip_expr.matchedLength();
            if (local_ips.contains(ip)) continue;
            const QHostAddress address(ip);
            if (address.protocol() == QAbstractSocket::IPv4Protocol && isPrivateIPv4(address.toIPv4Address())) {
                for (const quint16 port : localDiscoveryPorts()) {
                    enqueueLocalPeerCandidate(ip, port, now);
                }
            }
        }
        arp->deleteLater();
        processLocalPeerProbeQueue();
    });
    arp->start(QStringLiteral("/usr/sbin/arp"), QStringList{QStringLiteral("-an")});

    processLocalPeerProbeQueue();
}

void RPCConsole::enqueueLocalPeerSubnet(const QHostAddress& address, const QHostAddress& netmask, qint64 now)
{
    const quint32 local = address.toIPv4Address();
    if (!isPrivateIPv4(local)) return;

    const quint32 mask = netmask.toIPv4Address();
    const QSet<QString> local_ips = localIPv4Strings();
    const QVector<quint16> ports = localDiscoveryPorts();
    auto enqueue_ip = [this, &local_ips, &ports, now](quint32 ip) {
        const QString host = ipv4String(ip);
        if (local_ips.contains(host)) return;
        for (const quint16 port : ports) {
            enqueueLocalPeerCandidate(host, port, now);
        }
    };
    auto enqueue_24 = [enqueue_ip](quint32 base) {
        for (quint32 host = 1; host <= 254; ++host) {
            const quint32 ip = base | host;
            enqueue_ip(ip);
        }
    };

    if (mask != 0 && ((~mask) & 0xffffffffU) <= 255U) {
        const quint32 network = local & mask;
        const quint32 broadcast = network | (~mask);
        for (quint32 ip = network + 1; ip < broadcast; ++ip) {
            enqueue_ip(ip);
        }
        return;
    }

    enqueue_24(local & 0xffffff00U);

    if ((local & 0xffff0000U) == 0xc0a80000U) {
        const quint32 rotating_base = 0xc0a80000U | (static_cast<quint32>(localPeerRotatingSubnet) << 8);
        if (rotating_base != (local & 0xffffff00U)) {
            enqueue_24(rotating_base);
        }
        ++localPeerRotatingSubnet;
    }
}

void RPCConsole::enqueueLocalPeerCandidate(const QString& host, quint16 port, qint64 now)
{
    const QString endpoint = QStringLiteral("%1:%2").arg(host).arg(port);
    if (localPeerProbeQueued.contains(endpoint)) return;
    if (localPeerNextProbeMsecs.value(endpoint, 0) > now) return;
    localPeerProbeQueued.insert(endpoint);
    localPeerProbeQueue.enqueue(endpoint);
}

void RPCConsole::processLocalPeerProbeQueue()
{
    static constexpr int MAX_ACTIVE_LOCAL_PEER_PROBES = 16;
    while (activeLocalPeerProbes < MAX_ACTIVE_LOCAL_PEER_PROBES && !localPeerProbeQueue.isEmpty()) {
        const QString endpoint = localPeerProbeQueue.dequeue();
        localPeerProbeQueued.remove(endpoint);
        probeLocalPeer(endpoint);
    }
}

void RPCConsole::probeLocalPeer(const QString& endpoint)
{
    const int split = endpoint.lastIndexOf(QLatin1Char(':'));
    if (split <= 0) return;
    const QString host = endpoint.left(split);
    const quint16 port = endpoint.mid(split + 1).toUShort();
    if (port == 0) return;

    ++activeLocalPeerProbes;
    QTcpSocket* socket = new QTcpSocket(this);
    QTimer* timeout = new QTimer(socket);
    timeout->setSingleShot(true);
    QSharedPointer<bool> finished(new bool(false));

    auto finish = [this, endpoint, socket, timeout, finished](bool success) {
        if (*finished) return;
        *finished = true;
        timeout->stop();
        socket->abort();
        socket->deleteLater();
        localPeerNextProbeMsecs[endpoint] = QDateTime::currentMSecsSinceEpoch() + (success ? 5 * 60 * 1000 : 10 * 60 * 1000);
        activeLocalPeerProbes = std::max(0, activeLocalPeerProbes - 1);
        if (success) {
            tryLocalPeerConnection(endpoint);
        }
        processLocalPeerProbeQueue();
    };

    connect(socket, &QTcpSocket::connected, this, [finish] { finish(true); });
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error), this, [finish](QAbstractSocket::SocketError) { finish(false); });
    connect(timeout, &QTimer::timeout, this, [finish] { finish(false); });

    socket->connectToHost(host, port);
    timeout->start(1500);
}

void RPCConsole::tryLocalPeerConnection(const QString& endpoint)
{
    try {
        UniValue params(UniValue::VARR);
        params.push_back(endpoint.toStdString());
        params.push_back("add");
        m_node.executeRpc("addnode", params, "");
    } catch (...) {
        try {
            UniValue params(UniValue::VARR);
            params.push_back(endpoint.toStdString());
            params.push_back("onetry");
            m_node.executeRpc("addnode", params, "");
        } catch (...) {
            // The TCP probe was successful but the node may reject a duplicate
            // or transient manual connection attempt. The normal peer table
            // will show whether it connected.
        }
    }
}

void RPCConsole::showTraceRouteDialog()
{
    if (peerTraceRouteTarget.isEmpty()) return;

    QDialog* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Trace Route to %1").arg(peerTraceRouteTarget));
#ifdef Q_OS_WIN
    dialog->resize(1180, 720);
#else
    dialog->resize(1220, 720);
#endif

    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QHBoxLayout* heading_layout = new QHBoxLayout();
    QLabel* heading = new QLabel(tr("Trace Route to %1").arg(peerTraceRouteTarget), dialog);
    QFont heading_font = heading->font();
    heading_font.setBold(true);
    heading->setFont(heading_font);
    heading_layout->addWidget(heading);
    heading_layout->addStretch(1);
    layout->addLayout(heading_layout);

    QPlainTextEdit* output = new QPlainTextEdit(dialog);
    output->setReadOnly(true);
    output->setLineWrapMode(QPlainTextEdit::NoWrap);
    output->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
#ifdef Q_OS_WIN
    const int default_trace_font_size = std::max(10, GUIUtil::fixedPitchFont().pointSize() + 1);
#else
    const int default_trace_font_size = std::max(12, GUIUtil::fixedPitchFont().pointSize() + 1);
#endif
    auto current_trace_font_size = [default_trace_font_size]() {
        return std::max(FONT_RANGE.width(), std::min(QSettings().value(QStringLiteral("traceRouteFontSize"), default_trace_font_size).toInt(), FONT_RANGE.height()));
    };
    auto apply_trace_font = [output](int size) {
        const int bounded_size = std::max(FONT_RANGE.width(), std::min(size, FONT_RANGE.height()));
        QFont trace_font = GUIUtil::fixedPitchFont();
        trace_font.setPointSize(bounded_size);
        output->setFont(trace_font);
        setPlainTextEditTabStop(output, QFontMetrics(trace_font), QStringLiteral("    "));
        QSettings().setValue(QStringLiteral("traceRouteFontSize"), bounded_size);
    };
    QWidget* trace_text_controls = createTextControlWidget(
        tr("Make traceroute text smaller"),
        tr("Make traceroute text larger"),
        dialog,
        [apply_trace_font, current_trace_font_size](int delta) { apply_trace_font(current_trace_font_size() + delta); });
    heading_layout->addWidget(trace_text_controls, 0, Qt::AlignRight | Qt::AlignVCenter);
    apply_trace_font(current_trace_font_size());
    output->appendPlainText(tr("Starting traceroute..."));
    layout->addWidget(output, 1);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);

    QProcess* process = new QProcess(dialog);
    QPointer<QProcess> process_ptr(process);
    QPointer<QPlainTextEdit> output_ptr(output);
    connect(dialog, &QObject::destroyed, process, [process_ptr] {
        if (process_ptr && process_ptr->state() != QProcess::NotRunning) {
            process_ptr->kill();
            process_ptr->waitForFinished(1000);
        }
    });
    connect(process, &QProcess::readyReadStandardOutput, process, [process_ptr, output_ptr] {
        if (!process_ptr || !output_ptr) return;
        output_ptr->moveCursor(QTextCursor::End);
        output_ptr->insertPlainText(QString::fromLocal8Bit(process_ptr->readAllStandardOutput()));
        output_ptr->moveCursor(QTextCursor::End);
    });
    connect(process, &QProcess::readyReadStandardError, process, [process_ptr, output_ptr] {
        if (!process_ptr || !output_ptr) return;
        output_ptr->moveCursor(QTextCursor::End);
        output_ptr->insertPlainText(QString::fromLocal8Bit(process_ptr->readAllStandardError()));
        output_ptr->moveCursor(QTextCursor::End);
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), process, [output_ptr](int exit_code, QProcess::ExitStatus) {
        if (!output_ptr) return;
        output_ptr->appendPlainText(QObject::tr("\nTraceroute finished with exit code %1. This window will remain open until you close it.").arg(exit_code));
    });
    connect(process, &QProcess::errorOccurred, process, [output_ptr](QProcess::ProcessError) {
        if (!output_ptr) return;
        output_ptr->appendPlainText(QObject::tr("Unable to start the system traceroute tool."));
    });

    const bool ipv6 = peerTraceRouteTarget.contains(QLatin1Char(':'));
#ifdef Q_OS_WIN
    Q_UNUSED(ipv6);
    const QString tool = QStringLiteral("tracert.exe");
    const QStringList arguments{peerTraceRouteTarget};
#elif defined(Q_OS_MAC)
    const QString tool = ipv6 && QFileInfo::exists(QStringLiteral("/usr/sbin/traceroute6")) ? QStringLiteral("/usr/sbin/traceroute6") : QStringLiteral("/usr/sbin/traceroute");
    const QStringList arguments{peerTraceRouteTarget};
#else
    QString tool = QStringLiteral("traceroute");
    if (QFileInfo::exists(QStringLiteral("/usr/bin/traceroute"))) {
        tool = QStringLiteral("/usr/bin/traceroute");
    }
    QStringList arguments;
    if (ipv6) arguments << QStringLiteral("-6");
    arguments << peerTraceRouteTarget;
#endif
    process->start(tool, arguments);
    dialog->show();
}

void RPCConsole::showPingDialog()
{
    if (peerTraceRouteTarget.isEmpty()) return;

    QDialog* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Ping %1").arg(peerTraceRouteTarget));
    dialog->resize(900, 520);

    QVBoxLayout* layout = new QVBoxLayout(dialog);
    QHBoxLayout* heading_layout = new QHBoxLayout();
    QLabel* heading = new QLabel(tr("Ping %1").arg(peerTraceRouteTarget), dialog);
    QFont heading_font = heading->font();
    heading_font.setBold(true);
    heading->setFont(heading_font);
    heading_layout->addWidget(heading);
    heading_layout->addStretch(1);
    layout->addLayout(heading_layout);

    QPlainTextEdit* output = new QPlainTextEdit(dialog);
    output->setReadOnly(true);
    output->setLineWrapMode(QPlainTextEdit::NoWrap);
    output->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    const int default_ping_font_size = std::max(12, GUIUtil::fixedPitchFont().pointSize() + 1);
    auto current_ping_font_size = [default_ping_font_size]() {
        return std::max(FONT_RANGE.width(), std::min(QSettings().value(QStringLiteral("pingToolFontSize"), default_ping_font_size).toInt(), FONT_RANGE.height()));
    };
    auto apply_ping_font = [output](int size) {
        const int bounded_size = std::max(FONT_RANGE.width(), std::min(size, FONT_RANGE.height()));
        QFont ping_font = GUIUtil::fixedPitchFont();
        ping_font.setPointSize(bounded_size);
        output->setFont(ping_font);
        setPlainTextEditTabStop(output, QFontMetrics(ping_font), QStringLiteral("    "));
        QSettings().setValue(QStringLiteral("pingToolFontSize"), bounded_size);
    };
    QWidget* ping_text_controls = createTextControlWidget(
        tr("Make ping text smaller"),
        tr("Make ping text larger"),
        dialog,
        [apply_ping_font, current_ping_font_size](int delta) { apply_ping_font(current_ping_font_size() + delta); });
    heading_layout->addWidget(ping_text_controls, 0, Qt::AlignRight | Qt::AlignVCenter);
    apply_ping_font(current_ping_font_size());
    output->appendPlainText(tr("Starting ping..."));
    layout->addWidget(output, 1);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);

    QProcess* process = new QProcess(dialog);
    QPointer<QProcess> process_ptr(process);
    QPointer<QPlainTextEdit> output_ptr(output);
    connect(dialog, &QObject::destroyed, process, [process_ptr] {
        if (process_ptr && process_ptr->state() != QProcess::NotRunning) {
            process_ptr->kill();
            process_ptr->waitForFinished(1000);
        }
    });
    connect(process, &QProcess::readyReadStandardOutput, process, [process_ptr, output_ptr] {
        if (!process_ptr || !output_ptr) return;
        output_ptr->moveCursor(QTextCursor::End);
        output_ptr->insertPlainText(QString::fromLocal8Bit(process_ptr->readAllStandardOutput()));
        output_ptr->moveCursor(QTextCursor::End);
    });
    connect(process, &QProcess::readyReadStandardError, process, [process_ptr, output_ptr] {
        if (!process_ptr || !output_ptr) return;
        output_ptr->moveCursor(QTextCursor::End);
        output_ptr->insertPlainText(QString::fromLocal8Bit(process_ptr->readAllStandardError()));
        output_ptr->moveCursor(QTextCursor::End);
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), process, [output_ptr](int exit_code, QProcess::ExitStatus) {
        if (!output_ptr) return;
        output_ptr->appendPlainText(QObject::tr("\nPing finished with exit code %1. This window will remain open until you close it.").arg(exit_code));
    });
    connect(process, &QProcess::errorOccurred, process, [output_ptr](QProcess::ProcessError) {
        if (!output_ptr) return;
        output_ptr->appendPlainText(QObject::tr("Unable to start the system ping tool."));
    });

    const bool ipv6 = peerTraceRouteTarget.contains(QLatin1Char(':'));
#ifdef Q_OS_WIN
    Q_UNUSED(ipv6);
    const QString tool = QStringLiteral("ping.exe");
    const QStringList arguments{QStringLiteral("-n"), QStringLiteral("5"), peerTraceRouteTarget};
#elif defined(Q_OS_MAC)
    const QString tool = ipv6 && QFileInfo::exists(QStringLiteral("/sbin/ping6")) ? QStringLiteral("/sbin/ping6") : QStringLiteral("/sbin/ping");
    const QStringList arguments{QStringLiteral("-c"), QStringLiteral("5"), peerTraceRouteTarget};
#else
    QString tool = QStringLiteral("ping");
    if (QFileInfo::exists(QStringLiteral("/usr/bin/ping"))) {
        tool = QStringLiteral("/usr/bin/ping");
    }
    QStringList arguments;
    if (ipv6) arguments << QStringLiteral("-6");
    arguments << QStringLiteral("-c") << QStringLiteral("5") << peerTraceRouteTarget;
#endif
    process->start(tool, arguments);
    dialog->show();
}

void RPCConsole::updateNodeDetail(const CNodeCombinedStats *stats)
{
    auto set_detail_value = [this](const char* name, const QString& value) {
        if (QLabel* label = findChild<QLabel*>(QString::fromLatin1(name))) {
            label->setText(value.isEmpty() ? tr("N/A") : value);
        }
    };
    auto set_detail_row_visible = [](QWidget* label, QWidget* value, bool visible) {
        if (label) label->setVisible(visible);
        if (value) value->setVisible(visible);
    };

    // update the detail ui with latest node information
    QString peerAddrDetails;
    if (stats->nodeStats.nodeid == -1) {
        peerAddrDetails = tr("Local node");
    } else {
        peerAddrDetails = QString::fromStdString(stats->nodeStats.addrName);
        QString node_id_line = tr("(node id: %1)").arg(QString::number(stats->nodeStats.nodeid));
        node_id_line.replace(QLatin1Char(' '), QStringLiteral("&nbsp;"));
        peerAddrDetails += "<br /><span style=\"white-space: nowrap;\">" + node_id_line + "</span>";
    }
    if (!stats->nodeStats.addrLocal.empty())
        peerAddrDetails += "<br />" + tr("via %1").arg(QString::fromStdString(stats->nodeStats.addrLocal));
    ui->peerHeading->setText(peerAddrDetails);
    QString mapped_as_from_model;
    if (clientModel && clientModel->getPeerTableModel()) {
        const int row = clientModel->getPeerTableModel()->getRowByNodeId(stats->nodeStats.nodeid);
        PeerTableModel* peer_model = clientModel->getPeerTableModel();
        if ((peerGetLocationCheckBox && peerGetLocationCheckBox->isChecked()) ||
            (peerLookupMappedASCheckBox && peerLookupMappedASCheckBox->isChecked())) {
            peer_model->prioritizeLookupsForNode(stats->nodeStats.nodeid);
        }
        auto model_text = [peer_model, row](int column) {
            return row >= 0 ? peer_model->data(peer_model->index(row, column, QModelIndex()), Qt::DisplayRole).toString() : QString();
        };
        mapped_as_from_model = model_text(PeerTableModel::MappedAS);
#if ENABLE_DEFCOIN_FUN_UI
        set_detail_value("peerInspectorUniqueId", row >= 0 ? peer_model->data(peer_model->index(row, PeerTableModel::NetNodeId, QModelIndex()), PeerTableModel::PeerUniqueIdRole).toString() : QString());
        set_detail_value("peerInspectorFingerprint", row >= 0 ? peer_model->data(peer_model->index(row, PeerTableModel::NetNodeId, QModelIndex()), PeerTableModel::PeerFingerprintRole).toString() : QString());
        set_detail_value("peerInspectorNode", model_text(PeerTableModel::Address));
        set_detail_value("peerInspectorPort", model_text(PeerTableModel::Port));
        set_detail_value("peerInspectorFqdn", model_text(PeerTableModel::Fqdn));
        set_detail_value("peerInspectorCustomHostname", model_text(PeerTableModel::CustomHostname));
        set_detail_value("peerInspectorSeed", model_text(PeerTableModel::Seed).isEmpty() ? tr("No") : tr("Yes"));
        set_detail_value("peerInspectorGeo", model_text(PeerTableModel::Country));
        const QString city_state = model_text(PeerTableModel::CityState);
        set_detail_value("peerInspectorCityState", city_state);
        set_detail_value("peerInspectorMappedAS", mapped_as_from_model);
        set_detail_value("peerInspectorASName", model_text(PeerTableModel::ASName));
        set_detail_value("peerInspectorASHost", model_text(PeerTableModel::ASHostingCompany));
        set_detail_value("peerInspectorUaCount", model_text(PeerTableModel::UserAgentCount));
        set_detail_value("peerInspectorLocationSource",
            (city_state.isEmpty() ? tr("Unavailable or lookup disabled.") : tr("Cached best-effort IP lookup.")) +
            QStringLiteral(" <a href=\"https://ip-api.com/docs\">Geo</a> / <a href=\"https://www.team-cymru.com/ip-asn-mapping\">AS</a>"));
        ui->peerAvgPing->setText(model_text(PeerTableModel::AvgPing).isEmpty() ? tr("N/A") : model_text(PeerTableModel::AvgPing));
#endif
    }
    QString peer_services = GUIUtil::formatServicesStr(stats->nodeStats.nServices).trimmed();
    peer_services.replace(QLatin1Char('\n'), QLatin1Char(' '));
    peer_services = peer_services.simplified();
    ui->peerServices->setText(peer_services);
    const bool has_services = !peer_services.trimmed().isEmpty() && peer_services != tr("None");
    ui->label_4->setVisible(has_services);
    ui->peerServices->setVisible(has_services);
    ui->peerServices->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->peerServices->setSizePolicy(QSizePolicy::Expanding, has_services ? QSizePolicy::Minimum : QSizePolicy::Ignored);
    peerTraceRouteTarget = QString::fromStdString(stats->nodeStats.addr.ToStringIP());
    if (peerPingButton) {
        const bool ping_enabled = stats->nodeStats.nodeid >= 0 && !peerTraceRouteTarget.isEmpty();
        peerPingButton->setEnabled(ping_enabled);
        peerPingButton->setText(ping_enabled ? tr("Ping %1").arg(peerTraceRouteTarget) : tr("Ping"));
    }
    if (peerTraceRouteButton) {
        const bool trace_enabled = stats->nodeStats.nodeid >= 0 && !peerTraceRouteTarget.isEmpty();
        peerTraceRouteButton->setEnabled(trace_enabled);
        peerTraceRouteButton->setText(trace_enabled ? tr("Trace Route to %1").arg(peerTraceRouteTarget) : tr("Trace Route"));
    }
    ui->peerLastSend->setText(stats->nodeStats.nLastSend ? GUIUtil::formatDurationStr(GetSystemTimeInSeconds() - stats->nodeStats.nLastSend) : tr("never"));
    ui->peerLastRecv->setText(stats->nodeStats.nLastRecv ? GUIUtil::formatDurationStr(GetSystemTimeInSeconds() - stats->nodeStats.nLastRecv) : tr("never"));
    ui->peerBytesSent->setText(GUIUtil::formatBytes(stats->nodeStats.nSendBytes));
    ui->peerBytesRecv->setText(GUIUtil::formatBytes(stats->nodeStats.nRecvBytes));
    ui->peerConnTime->setText(GUIUtil::formatDurationStr(GetSystemTimeInSeconds() - stats->nodeStats.nTimeConnected));
    ui->peerPingTime->setText(GUIUtil::formatPingTime(stats->nodeStats.m_ping_usec));
    set_detail_row_visible(ui->peerPingWaitLabel, ui->peerPingWait, true);
    ui->peerPingWait->setText(stats->nodeStats.m_ping_wait_usec > 0 ? GUIUtil::formatPingTime(stats->nodeStats.m_ping_wait_usec) : tr("N/A"));
    ui->peerMinPing->setText(GUIUtil::formatPingTime(stats->nodeStats.m_min_ping_usec));
    ui->timeoffset->setText(GUIUtil::formatTimeOffset(stats->nodeStats.nTimeOffset));
    ui->peerVersion->setText(QString::number(stats->nodeStats.nVersion));
    QString node_user_agent = QString::fromStdString(stats->nodeStats.cleanSubVer).trimmed();
    while (node_user_agent.startsWith(QLatin1Char('/'))) node_user_agent.remove(0, 1);
    while (node_user_agent.endsWith(QLatin1Char('/'))) node_user_agent.chop(1);
    ui->peerSubversion->setText(node_user_agent.isEmpty() ? tr("N/A") : node_user_agent);
    ui->peerDirection->setText(stats->nodeStats.fInbound ? tr("Inbound") : tr("Outbound"));
    ui->peerHeight->setText(QString::number(stats->nodeStats.nStartingHeight));
    const bool has_permissions = stats->nodeStats.m_permissionFlags != PF_NONE;
    set_detail_row_visible(ui->label_30, ui->peerPermissions, has_permissions);
    if (has_permissions) {
        QStringList permissions;
        for (const auto& permission : NetPermissions::ToStrings(stats->nodeStats.m_permissionFlags)) {
            permissions.append(QString::fromStdString(permission));
        }
        ui->peerPermissions->setText(permissions.join(" & "));
    } else {
        ui->peerPermissions->setText(tr("N/A"));
    }
    set_detail_row_visible(ui->peerMappedASLabel, ui->peerMappedAS, false);

    // This check fails for example if the lock was busy and
    // nodeStateStats couldn't be fetched.
    if (stats->fNodeStateStatsAvailable) {
        // Sync height is init to -1
        if (stats->nodeStateStats.nSyncHeight > -1)
            ui->peerSyncHeight->setText(QString("%1").arg(stats->nodeStateStats.nSyncHeight));
        else
            ui->peerSyncHeight->setText(tr("Unknown"));

        // Common height is init to -1
        if (stats->nodeStateStats.nCommonHeight > -1)
            ui->peerCommonHeight->setText(QString("%1").arg(stats->nodeStateStats.nCommonHeight));
        else
            ui->peerCommonHeight->setText(tr("Unknown"));
    }

    ui->detailWidget->show();
    updatePeerInspectorHeight();
}

void RPCConsole::updateBannedPeerDetail(const QModelIndex& selected_index)
{
    if (!clientModel || !clientModel->getBanTableModel() || !selected_index.isValid()) return;

    if (ui->peerWidget && ui->peerWidget->selectionModel()) {
        ui->peerWidget->selectionModel()->clearSelection();
    }
    cachedNodeids.clear();
    if (peerBanButton) peerBanButton->setEnabled(false);

    BanTableModel* ban_model = clientModel->getBanTableModel();
    const int row = selected_index.row();
    auto model_text = [ban_model, row](int column, int role = Qt::DisplayRole) {
        return ban_model->data(ban_model->index(row, column, QModelIndex()), role).toString().trimmed();
    };
    auto set_detail_value = [this](const char* name, const QString& value) {
        if (QLabel* label = findChild<QLabel*>(QString::fromLatin1(name))) {
            label->setText(value.trimmed().isEmpty() ? tr("N/A") : value.trimmed());
        }
    };
    auto set_detail_row_visible = [](QWidget* label, QWidget* value, bool visible) {
        if (label) label->setVisible(visible);
        if (value) value->setVisible(visible);
    };

    QString node = model_text(BanTableModel::Address);
    node.remove(QRegExp(QStringLiteral("^[↑↓]\\s*")));
    const QString subnet = model_text(BanTableModel::Address, Qt::UserRole);
    QString trace_target = node;
    if (trace_target.contains(QLatin1Char('/'))) trace_target = trace_target.section(QLatin1Char('/'), 0, 0);
    const QString banned_until = model_text(BanTableModel::Bantime);
    ui->peerHeading->setText(tr("Banned peer<br />%1<br />Banned until %2").arg(node.isEmpty() ? subnet : node, banned_until));

#if ENABLE_DEFCOIN_FUN_UI
    set_detail_value("peerInspectorNode", node.isEmpty() ? subnet : node);
    set_detail_value("peerInspectorPort", model_text(BanTableModel::Port));
    set_detail_value("peerInspectorFqdn", model_text(BanTableModel::Fqdn));
    set_detail_value("peerInspectorCustomHostname", model_text(BanTableModel::CustomHostname));
    set_detail_value("peerInspectorSeed", model_text(BanTableModel::Seed).isEmpty() ? tr("No") : tr("Yes"));
    set_detail_value("peerInspectorGeo", model_text(BanTableModel::Country));
    const QString city_state = model_text(BanTableModel::CityState);
    set_detail_value("peerInspectorCityState", city_state);
    set_detail_value("peerInspectorMappedAS", model_text(BanTableModel::MappedAS));
    set_detail_value("peerInspectorASName", model_text(BanTableModel::ASName));
    set_detail_value("peerInspectorASHost", model_text(BanTableModel::ASHostingCompany));
    set_detail_value("peerInspectorUaCount", model_text(BanTableModel::UserAgentCount));
    set_detail_value("peerInspectorLocationSource",
        (city_state.isEmpty() ? tr("Unavailable or lookup disabled.") : tr("Cached best-effort IP lookup.")) +
        QStringLiteral(" <a href=\"https://ip-api.com/docs\">Geo</a> / <a href=\"https://www.team-cymru.com/ip-asn-mapping\">AS</a>"));
#endif

    QString services = model_text(BanTableModel::Services);
    services.replace(QLatin1Char('\n'), QLatin1Char(' '));
    services = services.simplified();
    ui->peerServices->setText(services);
    const bool has_services = !services.isEmpty() && services != tr("N/A") && services != QStringLiteral("-");
    ui->label_4->setVisible(has_services);
    ui->peerServices->setVisible(has_services);
    ui->peerServices->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->peerServices->setSizePolicy(QSizePolicy::Expanding, has_services ? QSizePolicy::Minimum : QSizePolicy::Ignored);

    const QString permissions = model_text(BanTableModel::Permissions);
    const bool has_permissions = !permissions.isEmpty() && permissions != tr("N/A") && permissions != QStringLiteral("-");
    set_detail_row_visible(ui->label_30, ui->peerPermissions, has_permissions);
    ui->peerPermissions->setText(has_permissions ? permissions : tr("N/A"));
    ui->peerDirection->setText(model_text(BanTableModel::Direction).isEmpty() ? tr("N/A") : model_text(BanTableModel::Direction));
    ui->peerVersion->setText(model_text(BanTableModel::Version).isEmpty() ? tr("N/A") : model_text(BanTableModel::Version));
    ui->peerSubversion->setText(model_text(BanTableModel::Subversion).isEmpty() ? tr("N/A") : model_text(BanTableModel::Subversion));
    ui->peerHeight->setText(model_text(BanTableModel::StartingBlock).isEmpty() ? tr("N/A") : model_text(BanTableModel::StartingBlock));
    ui->peerSyncHeight->setText(model_text(BanTableModel::SyncedHeaders).isEmpty() ? tr("N/A") : model_text(BanTableModel::SyncedHeaders));
    ui->peerCommonHeight->setText(model_text(BanTableModel::SyncedBlocks).isEmpty() ? tr("N/A") : model_text(BanTableModel::SyncedBlocks));
    ui->peerConnTime->setText(model_text(BanTableModel::ConnectionTime).isEmpty() ? tr("N/A") : model_text(BanTableModel::ConnectionTime));
    ui->peerLastSend->setText(model_text(BanTableModel::LastSend).isEmpty() ? tr("N/A") : model_text(BanTableModel::LastSend));
    ui->peerLastRecv->setText(model_text(BanTableModel::LastReceive).isEmpty() ? tr("N/A") : model_text(BanTableModel::LastReceive));
    ui->peerBytesSent->setText(model_text(BanTableModel::Sent).isEmpty() ? tr("N/A") : model_text(BanTableModel::Sent));
    ui->peerBytesRecv->setText(model_text(BanTableModel::Received).isEmpty() ? tr("N/A") : model_text(BanTableModel::Received));
#if ENABLE_DEFCOIN_FUN_UI
    ui->peerAvgPing->setText(model_text(BanTableModel::AvgPing).isEmpty() ? tr("N/A") : model_text(BanTableModel::AvgPing));
#endif
    ui->peerPingTime->setText(model_text(BanTableModel::Ping).isEmpty() ? tr("N/A") : model_text(BanTableModel::Ping));
    const QString ping_wait = model_text(BanTableModel::PingWait);
    const bool has_ping_wait = !ping_wait.isEmpty() && ping_wait != tr("N/A") && ping_wait != QStringLiteral("-");
    set_detail_row_visible(ui->peerPingWaitLabel, ui->peerPingWait, has_ping_wait);
    ui->peerPingWait->setText(has_ping_wait ? ping_wait : tr("N/A"));
    ui->peerMinPing->setText(model_text(BanTableModel::MinPing).isEmpty() ? tr("N/A") : model_text(BanTableModel::MinPing));
    ui->timeoffset->setText(model_text(BanTableModel::TimeOffset).isEmpty() ? tr("N/A") : model_text(BanTableModel::TimeOffset));
    set_detail_row_visible(ui->peerMappedASLabel, ui->peerMappedAS, false);

    peerTraceRouteTarget = trace_target;
    if (peerPingButton) {
        const bool ping_enabled = !peerTraceRouteTarget.isEmpty() && peerTraceRouteTarget != tr("N/A");
        peerPingButton->setEnabled(ping_enabled);
        peerPingButton->setText(ping_enabled ? tr("Ping %1").arg(peerTraceRouteTarget) : tr("Ping"));
    }
    if (peerTraceRouteButton) {
        const bool trace_enabled = !peerTraceRouteTarget.isEmpty() && peerTraceRouteTarget != tr("N/A");
        peerTraceRouteButton->setEnabled(trace_enabled);
        peerTraceRouteButton->setText(trace_enabled ? tr("Trace Route to %1").arg(peerTraceRouteTarget) : tr("Trace Route"));
    }
    ui->detailWidget->show();
    updatePeerInspectorHeight();
}

void RPCConsole::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updatePeerOverviewGeometry();
    stretchPeerPingHealthColumn();
    updatePeerPingHealthDensity();
    alignPeerDetailPane();
    QTimer::singleShot(0, this, &RPCConsole::updatePeerOverviewGeometry);
}

void RPCConsole::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::ActivationChange ||
        event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::ApplicationPaletteChange) {
        QTimer::singleShot(0, this, [this] {
            if (ui->tabWidget && ui->tabWidget->currentWidget() == ui->tab_nettraffic) {
                ui->tabWidget->tabBar()->update();
            }
        });
    }
}

void RPCConsole::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    resetPeerMapDataSource();
    QTimer::singleShot(0, this, &RPCConsole::relayoutAfterNodeFontChange);
    QTimer::singleShot(0, this, &RPCConsole::updatePeerOverviewGeometry);
    QTimer::singleShot(50, this, &RPCConsole::updatePeerOverviewGeometry);
    QTimer::singleShot(150, this, &RPCConsole::updatePeerOverviewGeometry);
    QTimer::singleShot(0, this, &RPCConsole::alignPeerDetailPane);
    QTimer::singleShot(150, this, &RPCConsole::alignPeerDetailPane);

    if (!clientModel || !clientModel->getPeerTableModel())
        return;

    // start PeerTableModel auto refresh
    clientModel->getPeerTableModel()->startAutoRefresh();
    if (peerTrafficHealthRepaintTimer) peerTrafficHealthRepaintTimer->start();
    ui->trafficGraph->setUpdatesActive(true);
}

void RPCConsole::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    resetPeerMapDataSource();

    if (!clientModel || !clientModel->getPeerTableModel())
        return;

    clientModel->getPeerTableModel()->stopAutoRefresh();
    if (peerTrafficHealthRepaintTimer) peerTrafficHealthRepaintTimer->stop();
    ui->trafficGraph->setUpdatesActive(false);
}

void RPCConsole::showPeersTableContextMenu(const QPoint& point)
{
    QModelIndex index = ui->peerWidget->indexAt(point);
    if (index.isValid()) {
        QItemSelectionModel* selection = ui->peerWidget->selectionModel();
        if (selection && !selection->isSelected(index)) {
            selection->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
        if (selection) selection->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        if (!peersTableContextMenu->actions().isEmpty()) {
            peersTableContextMenu->actions().first()->setText(selectedRowsInColumn(ui->peerWidget, index).size() > 1 ? tr("&Copy cells") : tr("&Copy cell"));
        }
        peersTableContextMenu->exec(QCursor::pos());
    }
}

void RPCConsole::showBanTableContextMenu(const QPoint& point)
{
    QModelIndex index = ui->banlistWidget->indexAt(point);
    if (index.isValid()) {
        QItemSelectionModel* selection = ui->banlistWidget->selectionModel();
        if (selection && !selection->isSelected(index)) {
            selection->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
        if (selection) selection->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        if (!banTableContextMenu->actions().isEmpty()) {
            banTableContextMenu->actions().first()->setText(selectedRowsInColumn(ui->banlistWidget, index).size() > 1 ? tr("&Copy cells") : tr("&Copy cell"));
        }
        banTableContextMenu->exec(QCursor::pos());
    }
}

void RPCConsole::disconnectSelectedNode()
{
    // Get selected peer addresses
    QList<QModelIndex> nodes = GUIUtil::getEntryData(ui->peerWidget, PeerTableModel::NetNodeId);
    for(int i = 0; i < nodes.count(); i++)
    {
        // Get currently selected peer address
        NodeId id = nodes.at(i).data(PeerTableModel::PeerActualNodeIdRole).toLongLong();
        if (id < 0) continue;
        // Find the node, disconnect it and clear the selected node
        if(m_node.disconnectById(id))
            clearSelectedNode();
    }
}

void RPCConsole::banSelectedNode(int bantime)
{
    if (!clientModel)
        return;

    // Get selected peer addresses
    QList<QModelIndex> nodes = GUIUtil::getEntryData(ui->peerWidget, PeerTableModel::NetNodeId);
    for(int i = 0; i < nodes.count(); i++)
    {
        // Get currently selected peer address
        NodeId id = nodes.at(i).data(PeerTableModel::PeerActualNodeIdRole).toLongLong();
        if (id < 0) continue;

        // Get currently selected peer address
        int detailNodeRow = clientModel->getPeerTableModel()->getRowByNodeId(id);
        if (detailNodeRow < 0) return;

        // Find possible nodes, ban it and clear the selected node
        const CNodeCombinedStats *stats = clientModel->getPeerTableModel()->getNodeStats(detailNodeRow);
        if (stats) {
            PeerTableModel* peer_model = clientModel->getPeerTableModel();
            auto peer_text = [peer_model, detailNodeRow](int column) {
                return peer_model->data(peer_model->index(detailNodeRow, column, QModelIndex()), Qt::DisplayRole);
            };
            QVector<QVariant> display_values(BanTableModel::ColumnCount);
            display_values[BanTableModel::NetNodeId] = peer_text(PeerTableModel::NetNodeId);
            display_values[BanTableModel::Seed] = peer_text(PeerTableModel::Seed);
            display_values[BanTableModel::Address] = peer_text(PeerTableModel::Address);
            display_values[BanTableModel::Port] = peer_text(PeerTableModel::Port);
            display_values[BanTableModel::Fqdn] = peer_text(PeerTableModel::Fqdn);
            display_values[BanTableModel::CustomHostname] = peer_text(PeerTableModel::CustomHostname);
            display_values[BanTableModel::Version] = peer_text(PeerTableModel::Version);
            display_values[BanTableModel::Services] = peer_text(PeerTableModel::Services);
            display_values[BanTableModel::AvgPing] = peer_text(PeerTableModel::AvgPing);
            display_values[BanTableModel::Ping] = peer_text(PeerTableModel::Ping);
            display_values[BanTableModel::Jitter] = peer_text(PeerTableModel::Jitter);
            display_values[BanTableModel::PingHealth] = peer_text(PeerTableModel::PingHealth);
            display_values[BanTableModel::Sent] = peer_text(PeerTableModel::Sent);
            display_values[BanTableModel::Received] = peer_text(PeerTableModel::Received);
            display_values[BanTableModel::Subversion] = peer_text(PeerTableModel::Subversion);
            display_values[BanTableModel::UserAgentCount] = peer_text(PeerTableModel::UserAgentCount);
            display_values[BanTableModel::Country] = peer_text(PeerTableModel::Country);
            display_values[BanTableModel::CityState] = peer_text(PeerTableModel::CityState);
            display_values[BanTableModel::Permissions] = peer_text(PeerTableModel::Permissions);
            display_values[BanTableModel::Direction] = peer_text(PeerTableModel::Direction);
            display_values[BanTableModel::StartingBlock] = peer_text(PeerTableModel::StartingBlock);
            display_values[BanTableModel::SyncedHeaders] = peer_text(PeerTableModel::SyncedHeaders);
            display_values[BanTableModel::SyncedBlocks] = peer_text(PeerTableModel::SyncedBlocks);
            display_values[BanTableModel::ConnectionTime] = peer_text(PeerTableModel::ConnectionTime);
            display_values[BanTableModel::LastSend] = peer_text(PeerTableModel::LastSend);
            display_values[BanTableModel::LastReceive] = peer_text(PeerTableModel::LastReceive);
            display_values[BanTableModel::PingWait] = peer_text(PeerTableModel::PingWait);
            display_values[BanTableModel::MinPing] = peer_text(PeerTableModel::MinPing);
            display_values[BanTableModel::TimeOffset] = peer_text(PeerTableModel::TimeOffset);
            display_values[BanTableModel::MappedAS] = peer_text(PeerTableModel::MappedAS);
            display_values[BanTableModel::ASName] = peer_text(PeerTableModel::ASName);
            display_values[BanTableModel::ASHostingCompany] = peer_text(PeerTableModel::ASHostingCompany);
            clientModel->getBanTableModel()->rememberPeerDisplay(QString::fromStdString(stats->nodeStats.addr.ToStringIP()), display_values);
            m_node.ban(stats->nodeStats.addr, bantime);
            m_node.disconnectByAddress(stats->nodeStats.addr);
        }
    }
    clearSelectedNode();
    clientModel->getBanTableModel()->refresh();
    clientModel->getPeerTableModel()->refresh();
}

void RPCConsole::unbanSelectedNode()
{
    if (!clientModel)
        return;

    const QList<QModelIndex> nodes = GUIUtil::getEntryData(ui->banlistWidget, BanTableModel::Address);
    QStringList subnets;
    for (int i = 0; i < nodes.count(); ++i) {
        QString strNode = nodes.at(i).data(Qt::UserRole).toString();
        if (strNode.isEmpty()) strNode = nodes.at(i).data().toString();
        if (!strNode.isEmpty() && !subnets.contains(strNode)) subnets << strNode;
    }

    bool changed = false;
    for (const QString& strNode : subnets) {
        CSubNet possibleSubnet;

        LookupSubNet(strNode.toStdString(), possibleSubnet);
        if (possibleSubnet.IsValid() && m_node.unban(possibleSubnet)) {
            changed = true;
        }
    }

    if (changed) {
        clientModel->getBanTableModel()->refresh();
        if (clientModel->getPeerTableModel()) clientModel->getPeerTableModel()->refresh();
        syncBanTableColumnsToPeerTable();
    }
}

void RPCConsole::clearSelectedNode()
{
    ui->peerWidget->selectionModel()->clearSelection();
    cachedNodeids.clear();
    if (peerBanButton) peerBanButton->setEnabled(false);
    peerTraceRouteTarget.clear();
    if (peerPingButton) {
        peerPingButton->setText(tr("Ping"));
        peerPingButton->setToolTip(tr("Select a peer to run ping."));
        peerPingButton->setEnabled(false);
    }
    if (peerTraceRouteButton) {
        peerTraceRouteButton->setText(tr("Trace Route"));
        peerTraceRouteButton->setEnabled(false);
    }
    ui->detailWidget->hide();
    ui->peerHeading->setText(tr("Select a peer to view detailed information."));
}

void RPCConsole::showOrHideBanTableIfRequired()
{
    if (!clientModel)
        return;

    BanTableModel* ban_model = clientModel->getBanTableModel();
    bool visible = ban_model && ban_model->shouldShow() && ban_model->realRowCount() > 0;
    if (peerShowBannedPeersCheckBox) {
        visible = visible && peerShowBannedPeersCheckBox->isChecked();
    }
    ui->banlistWidget->setVisible(visible);
    ui->banHeading->setVisible(visible);
    if (ui->verticalLayout_7) {
        ui->verticalLayout_7->setAlignment(Qt::AlignTop);
        const int overview_index = ui->verticalLayout_7->indexOf(findChild<QWidget*>(QStringLiteral("peerOverview")));
        const int connected_header_index = connectedPeersHeaderWidget ? ui->verticalLayout_7->indexOf(connectedPeersHeaderWidget) : -1;
        const int peer_index = ui->verticalLayout_7->indexOf(ui->peerWidget);
        int density_index = -1;
        for (int i = 0; i < ui->verticalLayout_7->count(); ++i) {
            QLayoutItem* item = ui->verticalLayout_7->itemAt(i);
            if (item && item->layout() == ui->horizontalLayoutPeerPingDensity) {
                density_index = i;
                break;
            }
        }
        if (density_index >= 0 && density_index < ui->verticalLayout_7->count() - 1) {
            QLayoutItem* density_item = ui->verticalLayout_7->takeAt(density_index);
            ui->verticalLayout_7->addItem(density_item);
            density_index = ui->verticalLayout_7->count() - 1;
        }
        if (density_index >= 0) {
            ui->verticalLayout_7->setAlignment(ui->horizontalLayoutPeerPingDensity, Qt::AlignRight | Qt::AlignBottom);
        }
        const int ban_heading_index = ui->verticalLayout_7->indexOf(ui->banHeading);
        const int ban_index = ui->verticalLayout_7->indexOf(ui->banlistWidget);
        if (overview_index >= 0) ui->verticalLayout_7->setStretch(overview_index, 0);
        if (connected_header_index >= 0) ui->verticalLayout_7->setStretch(connected_header_index, 0);
        if (peer_index >= 0) ui->verticalLayout_7->setStretch(peer_index, visible ? 2 : 1);
        if (density_index >= 0) ui->verticalLayout_7->setStretch(density_index, 0);
        if (ban_heading_index >= 0) ui->verticalLayout_7->setStretch(ban_heading_index, 0);
        if (ban_index >= 0) ui->verticalLayout_7->setStretch(ban_index, visible ? 1 : 0);
        ui->verticalLayout_7->invalidate();
        ui->verticalLayout_7->activate();
    }
    updateGeometry();
}

void RPCConsole::setTabFocus(enum TabTypes tabType)
{
    ui->tabWidget->setCurrentIndex(int(tabType));
}

void RPCConsole::captureUiScreenshots(const QString& dir)
{
    QDir capture_dir(dir);
    capture_dir.mkpath(QStringLiteral("."));

    struct TabShot {
        TabTypes tab;
        const char* filename;
    };

    const std::vector<TabShot> shots{
        {TabTypes::INFO, "node-information.png"},
        {TabTypes::DEBUG_LOG, "node-debug-log-file.png"},
        {TabTypes::CONSOLE, "node-console.png"},
        {TabTypes::GRAPH, "node-network-traffic.png"},
        {TabTypes::PEERS, "node-peers.png"},
#if ENABLE_DEFCOIN_FUN_UI
        {TabTypes::PEERS_MAP, "node-peers-map.png"},
#endif
    };

    int delay_ms = 0;
    for (const TabShot& shot : shots) {
        QTimer::singleShot(delay_ms, this, [this, shot] {
            setTabFocus(shot.tab);
            GUIUtil::bringToFront(this);
            updateGeometry();
            repaint();
        });
        delay_ms += 220;
        QTimer::singleShot(delay_ms, this, [this, capture_dir, shot] {
            grab().save(capture_dir.filePath(QString::fromUtf8(shot.filename)));
        });
        delay_ms += 180;
    }
}

QString RPCConsole::tabTitle(TabTypes tab_type) const
{
    return ui->tabWidget->tabText(int(tab_type));
}

QKeySequence RPCConsole::tabShortcut(TabTypes tab_type) const
{
    switch (tab_type) {
    case TabTypes::INFO: return QKeySequence(Qt::CTRL + Qt::Key_I);
    case TabTypes::DEBUG_LOG: return QKeySequence(Qt::CTRL + Qt::Key_L);
    case TabTypes::CONSOLE: return QKeySequence(Qt::CTRL + Qt::Key_T);
    case TabTypes::GRAPH: return QKeySequence(Qt::CTRL + Qt::Key_N);
    case TabTypes::PEERS: return QKeySequence(Qt::CTRL + Qt::Key_P);
    case TabTypes::PEERS_MAP: return QKeySequence(Qt::CTRL + Qt::Key_M);
    } // no default case, so the compiler can warn about missing cases

    assert(false);
}

void RPCConsole::updateAlerts(const QString& warnings)
{
    this->ui->label_alerts->setVisible(!warnings.isEmpty());
    this->ui->label_alerts->setText(warnings);
}
