// Copyright (c) 2011-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/bantablemodel.h>

#include <interfaces/node.h>
#include <net_types.h> // For banmap_t

#include <algorithm>
#include <utility>

#include <QDateTime>
#include <QFont>
#include <QHash>
#include <QList>
#include <QModelIndex>
#include <QRegExp>
#include <QVariant>
#include <QVector>

namespace {
QString emptyUnavailable()
{
    return QString();
}

QString banHostKey(const QString& value)
{
    QString key = value.trimmed();
    const int slash = key.indexOf(QLatin1Char('/'));
    if (slash >= 0) key.truncate(slash);
    if (key.startsWith(QLatin1Char('[')) && key.endsWith(QLatin1Char(']'))) {
        key = key.mid(1, key.size() - 2);
    }
    return key;
}

QString banHostKey(const CSubNet& subnet)
{
    return banHostKey(QString::fromStdString(subnet.ToString()));
}

QString alignIPv4EndpointForDisplay(const QString& endpoint)
{
    const QString trimmed = endpoint.trimmed();
    QRegExp ipv4_with_optional_port(QStringLiteral("^(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})(:\\d+)?$"));
    if (ipv4_with_optional_port.exactMatch(trimmed)) {
        return QStringLiteral("%1.%2.%3.%4%5")
            .arg(ipv4_with_optional_port.cap(1).toInt(), 3, 10, QLatin1Char(' '))
            .arg(ipv4_with_optional_port.cap(2).toInt(), 3, 10, QLatin1Char(' '))
            .arg(ipv4_with_optional_port.cap(3).toInt(), 3, 10, QLatin1Char(' '))
            .arg(ipv4_with_optional_port.cap(4).toInt(), 3, 10, QLatin1Char(' '))
            .arg(ipv4_with_optional_port.cap(5));
    }
    return trimmed;
}
}

bool BannedNodeLessThan::operator()(const CCombinedBan& left, const CCombinedBan& right) const
{
    const CCombinedBan* pLeft = &left;
    const CCombinedBan* pRight = &right;

    if (order == Qt::DescendingOrder)
        std::swap(pLeft, pRight);

    switch(column)
    {
    case BanTableModel::Address:
        return pLeft->subnet.ToString().compare(pRight->subnet.ToString()) < 0;
    case BanTableModel::Bantime:
        return pLeft->banEntry.nBanUntil < pRight->banEntry.nBanUntil;
    default:
        return pLeft->subnet.ToString().compare(pRight->subnet.ToString()) < 0;
    }

    return false;
}

// private implementation
class BanTablePriv
{
public:
    /** Local cache of peer information */
    QList<CCombinedBan> cachedBanlist;
    /** Last-seen connected peer row values, keyed by host IP. */
    QHash<QString, QVector<QVariant>> rememberedRows;
    /** Column to sort nodes by (default to unsorted) */
    int sortColumn{-1};
    /** Order (ascending or descending) to sort nodes by */
    Qt::SortOrder sortOrder;

    /** Pull a full list of banned nodes from CNode into our cache */
    void refreshBanlist(interfaces::Node& node)
    {
        banmap_t banMap;
        node.getBanned(banMap);

        cachedBanlist.clear();
        cachedBanlist.reserve(banMap.size());
        for (const auto& entry : banMap)
        {
            CCombinedBan banEntry;
            banEntry.subnet = entry.first;
            banEntry.banEntry = entry.second;
            const auto remembered = rememberedRows.constFind(banHostKey(entry.first));
            if (remembered != rememberedRows.constEnd()) {
                banEntry.displayValues = remembered.value();
                banEntry.hasDisplayValues = true;
            }
            cachedBanlist.append(banEntry);
        }

        if (sortColumn >= 0)
            // sort cachedBanlist (use stable sort to prevent rows jumping around unnecessarily)
            std::stable_sort(cachedBanlist.begin(), cachedBanlist.end(), BannedNodeLessThan(sortColumn, sortOrder));
    }

    int size() const
    {
        return cachedBanlist.size();
    }

    CCombinedBan *index(int idx)
    {
        if (idx >= 0 && idx < cachedBanlist.size())
            return &cachedBanlist[idx];

        return nullptr;
    }
};

BanTableModel::BanTableModel(interfaces::Node& node, QObject* parent) :
    QAbstractTableModel(parent),
    m_node(node)
{
    columns
        << tr("Node ID")
        << tr("Seed")
#if ENABLE_DEFCOIN_FUN_UI
        << tr("Node")
#else
        << tr("IP Address: Port")
#endif
        << tr("Port")
        << tr("FQDN")
        << tr("Domain Alias")
        << tr("Version")
        << tr("Svcs")
        << tr("Avg Ping")
        << tr("Ping Time")
        << tr("Jitter")
        << tr("Traffic Health")
        << tr("Sent")
        << tr("Rec'd")
        << tr("User Agent")
        << tr("UA Count")
        << tr("Geo")
        << tr("City, St")
        << tr("Permissions")
        << tr("Direction")
        << tr("Starting Block")
        << tr("Synced Headers")
        << tr("Synced Blocks")
        << tr("Connection Time")
        << tr("Last Send")
        << tr("Last Receive")
        << tr("Ping Wait")
        << tr("Min. Ping")
        << tr("Time Offset")
        << tr("AS Number")
        << tr("AS Name")
        << tr("AS Hosting Company")
        << tr("Banned Until");
    priv.reset(new BanTablePriv());

    // load initial data
    refresh();
}

BanTableModel::~BanTableModel()
{
    // Intentionally left empty
}

int BanTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return std::max(1, priv->size());
}

int BanTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant BanTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    CCombinedBan *rec = static_cast<CCombinedBan*>(index.internalPointer());

    if (!rec) {
        if (role == Qt::DisplayRole) {
            if (index.column() == Address) return tr("No banned peers.");
            return QVariant();
        } else if (role == Qt::TextAlignmentRole) {
            return QVariant(Qt::AlignCenter | Qt::AlignVCenter);
        } else if (role == Qt::ToolTipRole) {
            return tr("There are currently no banned peers.");
        }
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
        if (rec->hasDisplayValues && index.column() >= 0 && index.column() < rec->displayValues.size() && index.column() != Bantime) {
            const QVariant value = rec->displayValues.at(index.column());
            if (value.isValid() && !value.toString().isEmpty()) {
                if (index.column() == Subversion) {
                    QString user_agent = value.toString().trimmed();
                    while (user_agent.startsWith(QLatin1Char('/'))) user_agent.remove(0, 1);
                    while (user_agent.endsWith(QLatin1Char('/'))) user_agent.chop(1);
                    return user_agent;
                }
                if (index.column() == Address) return alignIPv4EndpointForDisplay(value.toString());
                return value;
            }
        }
        switch(index.column())
        {
        case NetNodeId:
        case Seed:
            return emptyUnavailable();
        case Address:
            return alignIPv4EndpointForDisplay(QString::fromStdString(rec->subnet.ToString()));
        case Port:
        case Fqdn:
        case CustomHostname:
        case Version:
        case Services:
        case AvgPing:
        case Ping:
        case Jitter:
        case PingHealth:
        case Sent:
        case Received:
        case Subversion:
        case UserAgentCount:
        case Country:
        case CityState:
        case Permissions:
        case Direction:
        case StartingBlock:
        case SyncedHeaders:
        case SyncedBlocks:
        case ConnectionTime:
        case LastSend:
        case LastReceive:
        case PingWait:
        case MinPing:
        case TimeOffset:
        case MappedAS:
        case ASName:
        case ASHostingCompany:
            return emptyUnavailable();
        case Bantime:
            QDateTime date = QDateTime::fromMSecsSinceEpoch(0);
            date = date.addSecs(rec->banEntry.nBanUntil);
            return date.toString(Qt::SystemLocaleLongDate);
        }
    } else if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case AvgPing:
        case Ping:
        case Jitter:
        case PingWait:
        case MinPing:
            return data(index, Qt::DisplayRole).toString() == QStringLiteral("N/A")
                ? QVariant(Qt::AlignCenter | Qt::AlignVCenter)
                : QVariant(Qt::AlignRight | Qt::AlignVCenter);
        case Address:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case Fqdn:
        case CustomHostname:
        case CityState:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case NetNodeId:
        case Port:
        case Version:
        case Sent:
        case Received:
        case UserAgentCount:
        case StartingBlock:
        case SyncedHeaders:
        case SyncedBlocks:
        case ConnectionTime:
        case LastSend:
        case LastReceive:
        case TimeOffset:
        case MappedAS:
        case ASName:
        case ASHostingCompany:
        case Bantime:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        case Subversion:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case Country:
        case Seed:
        case Services:
            return QVariant(Qt::AlignCenter | Qt::AlignVCenter);
        default:
            return QVariant(Qt::AlignCenter | Qt::AlignVCenter);
        }
    } else if (role == Qt::ToolTipRole) {
        switch (index.column()) {
        case Address:
            return tr("Banned subnet or host. Rows retain last-seen peer metadata when the peer was visible before it was banned.");
        case Bantime:
            return tr("Time when this ban expires.");
        default:
            return tr("This column mirrors the Connected Peers table. Unknown fields stay blank when the node backend only has subnet and expiration data.");
        }
    } else if (role == Qt::FontRole && index.column() == Address) {
        QFont font(QStringLiteral("Space Mono"));
        font.setStyleHint(QFont::Monospace);
        return font;
    } else if (role == Qt::UserRole && index.column() == Address) {
        return QString::fromStdString(rec->subnet.ToString());
    }

    return QVariant();
}

QVariant BanTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole && section < columns.size())
        {
            return columns[section];
        }
        if (role == Qt::ToolTipRole && section == MappedAS) {
            return tr("Autonomous System Number from the node backend or optional DNS-based IP-to-ASN lookup. This is distinct from service flags and is cached when looked up.");
        }
    }
    return QVariant();
}

Qt::ItemFlags BanTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    return retval;
}

QModelIndex BanTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    CCombinedBan *data = priv->index(row);

    if (data)
        return createIndex(row, column, data);
    if (row == 0 && priv->size() == 0)
        return createIndex(row, column, static_cast<void*>(nullptr));
    return QModelIndex();
}

void BanTableModel::refresh()
{
    Q_EMIT layoutAboutToBeChanged();
    priv->refreshBanlist(m_node);
    Q_EMIT layoutChanged();
}

void BanTableModel::sort(int column, Qt::SortOrder order)
{
    priv->sortColumn = column;
    priv->sortOrder = order;
    refresh();
}

bool BanTableModel::shouldShow()
{
    return true;
}

int BanTableModel::realRowCount() const
{
    return priv->size();
}

void BanTableModel::rememberPeerDisplay(const QString& ip, const QVector<QVariant>& display_values)
{
    const QString key = banHostKey(ip);
    if (key.isEmpty()) return;
    priv->rememberedRows.insert(key, display_values);
}
