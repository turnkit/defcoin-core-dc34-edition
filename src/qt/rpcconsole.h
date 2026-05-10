// Copyright (c) 2011-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_RPCCONSOLE_H
#define BITCOIN_QT_RPCCONSOLE_H

#include <qt/guiutil.h>
#include <qt/peertablemodel.h>

#include <clientversion.h>
#include <net.h>

#include <functional>

#include <QWidget>
#include <QCompleter>
#include <QHash>
#include <QHostAddress>
#include <QQueue>
#include <QSet>
#include <QThread>

class ClientModel;
class PlatformStyle;
class RPCTimerInterface;
class WalletModel;

namespace interfaces {
    class Node;
}

namespace Ui {
    class RPCConsole;
}

QT_BEGIN_NAMESPACE
class QMenu;
class QItemSelection;
class QCheckBox;
class QKeyEvent;
class QLabel;
class QPushButton;
class QPlainTextEdit;
class QScrollArea;
class QLineEdit;
class QTimer;
class QSlider;
class QSplitter;
class QToolButton;
QT_END_NAMESPACE

/** Local Bitcoin RPC console. */
class RPCConsole: public QWidget
{
    Q_OBJECT

public:
    explicit RPCConsole(interfaces::Node& node, const PlatformStyle *platformStyle, QWidget *parent);
    ~RPCConsole();

    static bool RPCParseCommandLine(interfaces::Node* node, std::string &strResult, const std::string &strCommand, bool fExecute, std::string * const pstrFilteredOut = nullptr, const WalletModel* wallet_model = nullptr);
    static bool RPCExecuteCommandLine(interfaces::Node& node, std::string &strResult, const std::string &strCommand, std::string * const pstrFilteredOut = nullptr, const WalletModel* wallet_model = nullptr) {
        return RPCParseCommandLine(&node, strResult, strCommand, true, pstrFilteredOut, wallet_model);
    }

    void setClientModel(ClientModel *model = nullptr, int bestblock_height = 0, int64_t bestblock_date = 0, double verification_progress = 0.0);
    void addWallet(WalletModel * const walletModel);
    void removeWallet(WalletModel* const walletModel);

    enum MessageClass {
        MC_ERROR,
        MC_DEBUG,
        CMD_REQUEST,
        CMD_REPLY,
        CMD_ERROR
    };

    enum class TabTypes {
        INFO,
        DEBUG_LOG,
        CONSOLE,
        GRAPH,
        PEERS,
        PEERS_MAP
    };

    std::vector<TabTypes> tabs() const
    {
#if ENABLE_DEFCOIN_FUN_UI
        return {TabTypes::INFO, TabTypes::DEBUG_LOG, TabTypes::CONSOLE, TabTypes::GRAPH, TabTypes::PEERS, TabTypes::PEERS_MAP};
#else
        return {TabTypes::INFO, TabTypes::DEBUG_LOG, TabTypes::CONSOLE, TabTypes::GRAPH, TabTypes::PEERS};
#endif
    }

    QString tabTitle(TabTypes tab_type) const;
    QKeySequence tabShortcut(TabTypes tab_type) const;
    void captureUiScreenshots(const QString& dir);

protected:
    virtual bool eventFilter(QObject* obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent *) override;

private Q_SLOTS:
    void on_lineEdit_returnPressed();
    void on_tabWidget_currentChanged(int index);
    /** open the debug.log from the current datadir */
    void on_openDebugLogfileButton_clicked();
    /** change the time range of the network traffic graph */
    void on_sldGraphRange_valueChanged(int value);
    /** update traffic statistics */
    void updateTrafficStats(quint64 totalBytesIn, quint64 totalBytesOut);
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    /** Show custom context menu on Peers tab */
    void showPeersTableContextMenu(const QPoint& point);
    /** Show custom context menu on Bans tab */
    void showBanTableContextMenu(const QPoint& point);
    /** Hides ban table if no bans are present */
    void showOrHideBanTableIfRequired();
    /** clear the selected node */
    void clearSelectedNode();

public Q_SLOTS:
    void clear(bool clearHistory = true);
    void fontBigger();
    void fontSmaller();
    void setFontSize(int newSize);
    /** Append the message to the message widget */
    void message(int category, const QString &msg) { message(category, msg, false); }
    void message(int category, const QString &message, bool html);
    /** Set number of connections shown in the UI */
    void setNumConnections(int count);
    /** Reload text size settings after Preferences changes */
    void refreshTextSizeSettings();
    /** Set network state shown in the UI */
    void setNetworkActive(bool networkActive);
    /** Set number of blocks and last block date shown in the UI */
    void setNumBlocks(int count, const QDateTime& blockDate, double nVerificationProgress, bool headers);
    /** Set size (number of transactions and memory usage) of the mempool in the UI */
    void setMempoolSize(long numberOfTxs, size_t dynUsage);
    /** Go forward or back in history */
    void browseHistory(int offset);
    /** Scroll console view to end */
    void scrollToEnd();
    /** Handle selection of peer in peers list */
    void peerSelected(const QItemSelection &selected, const QItemSelection &deselected);
    /** Handle selection caching before update */
    void peerLayoutAboutToChange();
    /** Handle updated peer information */
    void peerLayoutChanged();
    /** Disconnect a selected node on the Peers tab */
    void disconnectSelectedNode();
    /** Ban a selected node on the Peers tab */
    void banSelectedNode(int bantime);
    /** Unban a selected node on the Bans tab */
    void unbanSelectedNode();
    /** set which tab has the focus (is visible) */
    void setTabFocus(enum TabTypes tabType);
    void setNodeWindowFontSize(int newSize);

Q_SIGNALS:
    // For RPC command executor
    void cmdRequest(const QString &command, const WalletModel* wallet_model);

private:
    void startExecutor();
    void setTrafficGraphScale(int scale_percent, bool refresh_duration = true);
    void resizePeerTableColumnsToContents();
    void applyPeerViewPreset(int preset_index);
    void savePeerViewLayout();
    void loadPeerViewLayout();
    void resetPeerViewLayout();
    void editPeerViewLayout();
    void restorePeerPanelLayout();
    void savePeerPanelLayout();
    void resetPeerPanelLayout();
    void loadPeerColumnTitles();
    void savePeerColumnTitles();
    void applyDefaultPeerColumnVisibility();
    void ensurePeerDirectionColumnPlacement();
    void syncBanTableColumnsToPeerTable();
    void stretchPeerPingHealthColumn();
    void updatePeerPingHealthDensity();
    void updatePeerSummaryStats();
    void updatePeerTableVisibility();
    void installNodeTextControls();
    QWidget* createTextControlWidget(const QString& smaller_tooltip, const QString& bigger_tooltip, const QObject* receiver, const std::function<void(int)>& adjust_callback);
    void applyNodeWindowFont();
    void applyConnectedPeersPanelFont();
    void applyPeerTableRowHeights(bool compact);
    void resizeConnectedPeersPanelColumnsForFont();
    void resizeBanTableColumnsForFont();
    int tightTableColumnWidth(QTableView* table, int column) const;
    void resizeTableColumnsTightly(QTableView* table);
    void relayoutAfterNodeFontChange();
    void updatePeerOverviewGeometry();
    void updateTrafficMetricSwatches();
    void resetNodeVisualState();
    void alignPeerDetailPane();
    int peerColumnDefaultWidth(int column) const;
    void setPeerColumnWidthToDefault(int column);
    void updateConsoleFontMetrics();
    void adjustNodeWindowFontSize(int delta);
    void setConnectedPeersPanelFontSize(int newSize);
    void adjustConnectedPeersPanelFontSize(int delta);
    QString consoleMessageStyleSheet() const;
    int consoleTimeColumnWidth() const;
    int consoleIconColumnWidth() const;
    void startLocalPeerDiscovery();
    void stopLocalPeerDiscovery();
    void runLocalPeerDiscoveryScan();
    void enqueueLocalPeerSubnet(const QHostAddress& address, const QHostAddress& netmask, qint64 now);
    void enqueueLocalPeerCandidate(const QString& host, quint16 port, qint64 now);
    void processLocalPeerProbeQueue();
    void probeLocalPeer(const QString& endpoint);
    void tryLocalPeerConnection(const QString& endpoint);
    void showTraceRouteDialog();
    void showPingDialog();
    void updatePeerInspectorHeight();
    void refreshDebugLogView(bool force = false);
    bool handleTrafficSliderKey(QKeyEvent* event);
    void setupTrafficStatsSplitter();
    void setTrafficStatsPanelCollapsed(bool collapsed);
    void setPeerDetailPanelCollapsed(bool collapsed);
    void promptTrafficGraphDuration();
    bool handlePeerDemoKey(QKeyEvent* event);
    void setPeerDemoMode(bool enabled);
    void togglePeerDemoMode();
    void resetPeerMapDataSource();
    /** show detailed information on ui about selected node */
    void updateNodeDetail(const CNodeCombinedStats *stats);
    void updateBannedPeerDetail(const QModelIndex& index);

    enum ColumnWidths
    {
        BANSUBNET_COLUMN_WIDTH = 200,
        BANTIME_COLUMN_WIDTH = 250
    };

    interfaces::Node& m_node;
    Ui::RPCConsole* const ui;
    ClientModel *clientModel = nullptr;
    QStringList history;
    int historyPtr = 0;
    QString cmdBeforeBrowsing;
    QList<NodeId> cachedNodeids;
    const PlatformStyle* const platformStyle;
    RPCTimerInterface *rpcTimerInterface = nullptr;
    QMenu *peersTableContextMenu = nullptr;
    QMenu *banTableContextMenu = nullptr;
    bool peerColumnsUserResized = false;
    bool peerColumnsAutoResizing = false;
    int peerPingHealthBaseWidth = 140;
    int consoleFontSize = 0;
    int nodeWindowFontSize = 0;
    QLabel* trafficAvgLatencyLabel = nullptr;
    QLabel* trafficPeerAvgLatencyLabel = nullptr;
    QLabel* trafficJitterLabel = nullptr;
    QCheckBox* trafficReceivedCheckBox = nullptr;
    QCheckBox* trafficSentCheckBox = nullptr;
    QCheckBox* trafficAvgRecentLatencyCheckBox = nullptr;
    QCheckBox* trafficPeerAvgLatencyCheckBox = nullptr;
    QCheckBox* trafficJitterCheckBox = nullptr;
    QCheckBox* trafficMovingAverageCheckBox = nullptr;
    QCheckBox* trafficAllHistoryCheckBox = nullptr;
    QSlider* trafficPanSlider = nullptr;
    QSlider* trafficWindowSlider = nullptr;
    QSlider* lastTrafficSlider = nullptr;
    QWidget* trafficPanWidget = nullptr;
    QLabel* trafficWindowValueLabel = nullptr;
    QLabel* trafficWindowDurationLabel = nullptr;
    qint64 trafficScaleDisplayedSeconds = -1;
    int trafficScaleDisplayedPercent = -1;
    bool trafficScaleDisplayedAllHistory = false;
    QWidget* debugLogTab = nullptr;
    QPlainTextEdit* debugLogView = nullptr;
    QLineEdit* debugLogSearchEdit = nullptr;
    QTimer* debugLogRefreshTimer = nullptr;
    qint64 debugLogLastSize = -1;
    qint64 debugLogSessionStartSize = 0;
    QWidget* trafficReceivedSwatch = nullptr;
    QWidget* trafficSentSwatch = nullptr;
    QWidget* trafficAvgRecentLatencySwatch = nullptr;
    QWidget* trafficPeerAvgLatencySwatch = nullptr;
    QWidget* trafficJitterSwatch = nullptr;
    QSplitter* trafficStatsSplitter = nullptr;
    QWidget* trafficGraphPanel = nullptr;
    QWidget* trafficStatsPanel = nullptr;
    QToolButton* trafficStatsCollapseButton = nullptr;
    bool trafficStatsPanelCollapsed = false;
    QWidget* peerDetailTopSpacer = nullptr;
    QWidget* nodeTextControls = nullptr;
    QWidget* peerPanelTextControls = nullptr;
    QWidget* connectedPeersHeaderWidget = nullptr;
    QScrollArea* peerLeftScrollArea = nullptr;
    QLabel* connectedPeersLabel = nullptr;
    QLabel* peerStatsBannerLabel = nullptr;
    QCheckBox* peerShowConnectedPeersCheckBox = nullptr;
    QCheckBox* peerShowInactiveCheckBox = nullptr;
    QCheckBox* peerOnlyDefcoinUserAgentsCheckBox = nullptr;
    QCheckBox* peerLocalDiscoveryCheckBox = nullptr;
    QCheckBox* peerShowBannedPeersCheckBox = nullptr;
    QCheckBox* peerGetLocationCheckBox = nullptr;
    QCheckBox* peerLookupMappedASCheckBox = nullptr;
    QLabel* peerUserAgentSummaryLabel = nullptr;
    QLabel* peerPingDensityMetricLabel = nullptr;
    QLabel* peerTrafficHealthLegendLabel = nullptr;
    QWidget* peerUserAgentChartWidget = nullptr;
    QWidget* peerMapWidget = nullptr;
    QTimer* peerTrafficHealthRepaintTimer = nullptr;
    QPushButton* peerBanButton = nullptr;
    QToolButton* peerDetailCollapseButton = nullptr;
    QPushButton* peerPingButton = nullptr;
    QPushButton* peerTraceRouteButton = nullptr;
    QString peerTraceRouteTarget;
    int connectedPeersPanelFontSize = 0;
    bool peerCompactRowHeights = false;
    QSet<QString> seenPeerKeys;
    QTimer* localPeerDiscoveryTimer = nullptr;
    QQueue<QString> localPeerProbeQueue;
    QSet<QString> localPeerProbeQueued;
    QHash<QString, qint64> localPeerNextProbeMsecs;
    int activeLocalPeerProbes = 0;
    quint8 localPeerRotatingSubnet = 0;
    QString peerDemoKeyBuffer;
    bool peerDemoMode = false;
    QCompleter *autoCompleter = nullptr;
    QThread thread;
    WalletModel* m_last_wallet_model{nullptr};

    /** Update UI with latest network info from model. */
    void updateNetworkState();

private Q_SLOTS:
    void updateAlerts(const QString& warnings);
};

#endif // BITCOIN_QT_RPCCONSOLE_H
