// Copyright (c) 2011-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_PEERTABLEMODEL_H
#define BITCOIN_QT_PEERTABLEMODEL_H

#include <net_processing.h> // For CNodeStateStats
#include <net.h>

#include <memory>

#include <QAbstractTableModel>
#include <QStringList>

class PeerTablePriv;

namespace interfaces {
class Node;
}

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

struct CNodeCombinedStats {
    CNodeStats nodeStats;
    CNodeStateStats nodeStateStats;
    QStringList displayPorts;
    bool fNodeStateStatsAvailable;
    bool fActive{true};
};

class NodeLessThan
{
public:
    NodeLessThan(int nColumn, Qt::SortOrder fOrder) :
        column(nColumn), order(fOrder) {}
    bool operator()(const CNodeCombinedStats &left, const CNodeCombinedStats &right) const;

private:
    int column;
    Qt::SortOrder order;
};

/**
   Qt model providing information about connected peers, similar to the
   "getpeerinfo" RPC call. Used by the rpc console UI.
 */
class PeerTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit PeerTableModel(interfaces::Node& node, QObject* parent);
    ~PeerTableModel();
    const CNodeCombinedStats *getNodeStats(int idx);
    int getRowByNodeId(NodeId nodeid);
    void setPingHealthSampleLimit(int limit);
    void setShowConnectedPeers(bool show);
    bool showConnectedPeers() const;
    void setShowInactivePeers(bool show);
    bool showInactivePeers() const;
    void setOnlyDefcoinUserAgents(bool only_defcoin);
    bool onlyDefcoinUserAgents() const;
    void setGeolocationEnabled(bool enabled);
    bool geolocationEnabled() const;
    void setMappedASLookupEnabled(bool enabled);
    bool mappedASLookupEnabled() const;
    void prioritizeLookupsForNode(NodeId nodeid);
    void setDemoPeersEnabled(bool enabled);
    bool demoPeersEnabled() const;
    int activePeerCount() const;
    int seenPeerCount() const;
    QString columnTitle(int column) const;
    QString defaultColumnTitle(int column) const;
    void setColumnTitle(int column, const QString& title);
    void resetColumnTitles();
    void startAutoRefresh();
    void stopAutoRefresh();

    enum ColumnIndex {
        NetNodeId = 0,
        Address = 1,
        Port = 2,
        Fqdn = 3,
        CustomHostname = 4,
        Version = 5,
        Services = 6,
        AvgPing = 7,
        Ping = 8,
        Jitter = 9,
        PingHealth = 10,
        Sent = 11,
        Received = 12,
        Subversion = 13,
        UserAgentCount = 14,
        Country = 15,
        CityState = 16,
        Permissions = 17,
        Direction = 18,
        StartingBlock = 19,
        SyncedHeaders = 20,
        SyncedBlocks = 21,
        ConnectionTime = 22,
        LastSend = 23,
        LastReceive = 24,
        PingWait = 25,
        MinPing = 26,
        TimeOffset = 27,
        MappedAS = 28,
        ASName = 29,
        ASHostingCompany = 30,
        Seed = 31,
        UniqueId = 32,
        ColumnCount = 33,
    };

    enum DataRole {
        PeerAddressRole = Qt::UserRole + 100,
        PeerPortsRole,
        PeerLatitudeRole,
        PeerLongitudeRole,
        PeerIsLanRole,
        PeerIsLocalRole,
        PeerUserAgentRole,
        PeerSentBytesRole,
        PeerReceivedBytesRole,
        PeerCityStateRole,
        PeerActualNodeIdRole,
        PeerUniqueIdRole,
        PeerFingerprintRole,
    };

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    void sort(int column, Qt::SortOrder order) override;
    /*@}*/

public Q_SLOTS:
    void refresh();

private:
    void schedulePeerLookups();
    void processPeerLookupQueues();
    void scheduleLocationQueueRetry(qint64 delay_msecs);
    void notifyLookupChanged(const QString& peer, int column);

    interfaces::Node& m_node;
    QStringList columns;
    QStringList default_columns;
    std::unique_ptr<PeerTablePriv> priv;
    QTimer *timer;
};

#endif // BITCOIN_QT_PEERTABLEMODEL_H
