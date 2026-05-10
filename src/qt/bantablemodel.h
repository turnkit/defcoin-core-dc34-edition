// Copyright (c) 2011-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_BANTABLEMODEL_H
#define BITCOIN_QT_BANTABLEMODEL_H

#include <net.h>

#include <memory>

#include <QAbstractTableModel>
#include <QVariant>
#include <QVector>
#include <QStringList>

class BanTablePriv;

namespace interfaces {
    class Node;
}

struct CCombinedBan {
    CSubNet subnet;
    CBanEntry banEntry;
    QVector<QVariant> displayValues;
    bool hasDisplayValues{false};
};

class BannedNodeLessThan
{
public:
    BannedNodeLessThan(int nColumn, Qt::SortOrder fOrder) :
        column(nColumn), order(fOrder) {}
    bool operator()(const CCombinedBan& left, const CCombinedBan& right) const;

private:
    int column;
    Qt::SortOrder order;
};

/**
   Qt model providing information about connected peers, similar to the
   "getpeerinfo" RPC call. Used by the rpc console UI.
 */
class BanTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit BanTableModel(interfaces::Node& node, QObject* parent);
    ~BanTableModel();
    void startAutoRefresh();
    void stopAutoRefresh();

    enum ColumnIndex {
        NetNodeId = 0,
        Seed = 1,
        Address = 2,
        Port = 3,
        Fqdn = 4,
        CustomHostname = 5,
        Version = 6,
        Services = 7,
        AvgPing = 8,
        Ping = 9,
        Jitter = 10,
        PingHealth = 11,
        Sent = 12,
        Received = 13,
        Subversion = 14,
        UserAgentCount = 15,
        Country = 16,
        CityState = 17,
        Permissions = 18,
        Direction = 19,
        StartingBlock = 20,
        SyncedHeaders = 21,
        SyncedBlocks = 22,
        ConnectionTime = 23,
        LastSend = 24,
        LastReceive = 25,
        PingWait = 26,
        MinPing = 27,
        TimeOffset = 28,
        MappedAS = 29,
        ASName = 30,
        ASHostingCompany = 31,
        Bantime = 32,
        ColumnCount = 33,
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

    bool shouldShow();
    int realRowCount() const;
    void rememberPeerDisplay(const QString& ip, const QVector<QVariant>& display_values);

public Q_SLOTS:
    void refresh();

private:
    interfaces::Node& m_node;
    QStringList columns;
    std::unique_ptr<BanTablePriv> priv;
};

#endif // BITCOIN_QT_BANTABLEMODEL_H
