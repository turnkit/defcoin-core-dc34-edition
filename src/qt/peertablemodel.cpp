// Copyright (c) 2011-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/peertablemodel.h>

#include <qt/guiconstants.h>
#include <qt/guiutil.h>

#include <clientversion.h>
#if ENABLE_DEFCOIN_FUN_UI
#include <qt/defcoindummypeers.h>
#endif
#include <interfaces/node.h>
#include <net_types.h>
#include <netbase.h>
#include <net_processing.h>
#include <version.h>

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include <QAbstractSocket>
#include <QApplication>
#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <QDnsDomainNameRecord>
#include <QDnsLookup>
#include <QDnsTextRecord>
#include <QFont>
#include <QHostAddress>
#include <QHash>
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPalette>
#include <QQueue>
#include <QProcess>
#include <QRegExp>
#include <QSet>
#include <QSettings>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantList>
#include <QVector>

namespace {
static constexpr qint64 DNS_LOOKUP_RETRY_MSECS = 15 * 60 * 1000;
static constexpr qint64 DNS_LOOKUP_REFRESH_MSECS = 24 * 60 * 60 * 1000;
static constexpr int MAX_PARALLEL_LOOKUPS = 4;
static constexpr int MAX_PARALLEL_LOCATION_LOOKUPS = 2;
// ip-api.com free endpoints are currently limited to 45 requests/minute.
// Keep a small margin because the Peer Map WAN lookup may also use the service.
static constexpr int IP_API_MAX_LOCATION_REQUESTS_PER_MINUTE = 40;
static constexpr qint64 IP_API_RATE_WINDOW_MSECS = 60 * 1000;
static constexpr qint64 IP_API_DEFAULT_RATE_RETRY_MSECS = 60 * 1000;
static constexpr qint64 MAX_REASONABLE_PING_USEC = 10 * 60 * 1000 * 1000;
static const QString WAN_LOCATION_LOOKUP_KEY = QStringLiteral("__defcoin_wan_location__");

QString peerLookupKey(const CAddress& address)
{
    return QString::fromStdString(address.ToStringIP());
}

QString peerStatsKey(const CNodeStats& stats)
{
    if (stats.nodeid == -1) return QStringLiteral("local-node");
    const QString endpoint = QString::fromStdString(stats.addr.ToStringIPPort());
    if (!endpoint.isEmpty()) return endpoint;
    const QString named_address = QString::fromStdString(stats.addrName);
    if (!named_address.isEmpty()) return named_address;
    return QString::number(stats.nodeid);
}

QString peerEndpointKey(const CNodeStats& stats)
{
    if (stats.nodeid == -1) return QStringLiteral("local-node");
    const QString endpoint = QString::fromStdString(stats.addr.ToStringIPPort());
    if (!endpoint.isEmpty()) return endpoint;
    const QString address_name = QString::fromStdString(stats.addrName);
    if (!address_name.isEmpty()) return address_name;
    return QString::number(stats.nodeid);
}

QString peerFingerprintText(const CNodeStats& stats)
{
    const QByteArray material = QStringLiteral("%1|%2|%3")
        .arg(stats.nVersion)
        .arg(static_cast<qulonglong>(stats.nServices))
        .arg(QString::fromStdString(stats.cleanSubVer))
        .toUtf8();
    uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char ch : material) {
        hash ^= ch;
        hash *= 1099511628211ULL;
    }
    return QString::number(static_cast<qulonglong>(hash));
}

QString peerUniqueBaseKey(const CNodeStats& stats)
{
    if (stats.nodeid == -1) return QStringLiteral("local-node");
    const QString endpoint = peerEndpointKey(stats);
    if (endpoint.isEmpty()) return QString();
    return endpoint + QLatin1Char('|') + peerFingerprintText(stats);
}

QString seedDomainForAddress(const CAddress& address)
{
    const QString ip = QString::fromStdString(address.ToStringIP());
    if (ip == QLatin1String("66.42.91.225")) return QStringLiteral("seed.defcoin.io");
    if (ip == QLatin1String("73.52.182.204")) return QStringLiteral("seed.defcoin.mikej.tech");
    if (ip == QLatin1String("50.116.19.40")) return QStringLiteral("seed.defcoin.dc903.org");
    return QString();
}

QString displayUserAgent(const std::string& clean_subver)
{
    QString user_agent = QString::fromStdString(clean_subver).trimmed();
    while (user_agent.startsWith(QLatin1Char('/'))) user_agent.remove(0, 1);
    while (user_agent.endsWith(QLatin1Char('/'))) user_agent.chop(1);
    return user_agent;
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

QString cymruAsNameDnsName(const QString& asn)
{
    QString digits = asn.trimmed();
    if (digits.startsWith(QStringLiteral("AS"), Qt::CaseInsensitive)) {
        digits.remove(0, 2);
    }
    bool ok = false;
    const uint value = digits.toUInt(&ok);
    if (!ok || value == 0) return QString();
    return QStringLiteral("AS%1.asn.cymru.com").arg(value);
}

QString hostingCompanyFromASName(const QString& as_name)
{
    QString value = as_name.trimmed();
    if (value.isEmpty()) return QString();
    const int dash = value.indexOf(QStringLiteral(" - "));
    if (dash >= 0) value = value.mid(dash + 3).trimmed();
    const int comma = value.indexOf(QLatin1Char(','));
    if (comma >= 0) value.truncate(comma);
    return value.trimmed();
}

bool shouldLookupPeer(const CAddress& address)
{
    return address.IsValid() && address.IsRoutable() && !address.IsInternal() && !address.IsTor() && !address.IsI2P();
}

bool sameIPv6Subnet(const QHostAddress& address, const QHostAddress& local, int prefix_length)
{
    if (prefix_length < 0 || prefix_length > 128) return false;

    const Q_IPV6ADDR addr_bytes = address.toIPv6Address();
    const Q_IPV6ADDR local_bytes = local.toIPv6Address();
    const int full_bytes = prefix_length / 8;
    const int remaining_bits = prefix_length % 8;

    for (int i = 0; i < full_bytes; ++i) {
        if (addr_bytes[i] != local_bytes[i]) return false;
    }
    if (remaining_bits == 0) return true;

    const quint8 mask = static_cast<quint8>(0xffU << (8 - remaining_bits));
    return (addr_bytes[full_bytes] & mask) == (local_bytes[full_bytes] & mask);
}

bool isInLocalInterfaceSubnet(const QHostAddress& address)
{
    if (address.isNull() || address.protocol() == QAbstractSocket::UnknownNetworkLayerProtocol) return false;

    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        const QNetworkInterface::InterfaceFlags flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning) || (flags & QNetworkInterface::IsLoopBack)) continue;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            const QHostAddress local = entry.ip();
            const int prefix_length = entry.prefixLength();
            if (local.isNull() || local.protocol() != address.protocol() || prefix_length < 0) continue;

            if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                const quint32 mask = prefix_length == 0 ? 0 : 0xffffffffU << (32 - prefix_length);
                if ((address.toIPv4Address() & mask) == (local.toIPv4Address() & mask)) return true;
            } else if (address.protocol() == QAbstractSocket::IPv6Protocol) {
                if (sameIPv6Subnet(address, local, prefix_length)) return true;
            }
        }
    }
    return false;
}

bool isLanPeer(const CAddress& address)
{
    return address.IsRFC1918() || address.IsRFC3927() || address.IsRFC4193() || address.IsRFC4862() ||
           isInLocalInterfaceSubnet(QHostAddress(QString::fromStdString(address.ToStringIP())));
}

bool isReasonablePing(qint64 ping_usec)
{
    return ping_usec > 0 && ping_usec <= MAX_REASONABLE_PING_USEC;
}

bool isDefcoinPrefixedUserAgent(const std::string& user_agent)
{
    QString value = QString::fromStdString(user_agent).trimmed();
    while (value.startsWith(QLatin1Char('/'))) value.remove(0, 1);
    return value.startsWith(QStringLiteral("defcoin"), Qt::CaseInsensitive);
}

qint64 bestPingUsec(const CNodeStats& stats)
{
    if (isReasonablePing(stats.m_ping_usec)) return stats.m_ping_usec;
    if (isReasonablePing(stats.m_min_ping_usec)) return stats.m_min_ping_usec;
    return 0;
}

QString formatPingUsec(qint64 ping_usec)
{
    return isReasonablePing(ping_usec) ? GUIUtil::formatPingTime(ping_usec) : QStringLiteral("N/A");
}

QString normalizedAddressString(const QHostAddress& address)
{
    QString value = address.toString();
    const int zone_index = value.indexOf(QLatin1Char('%'));
    if (zone_index >= 0) {
        value.truncate(zone_index);
    }
    return value.toLower();
}

QString normalizedAddressString(const CAddress& address)
{
    return normalizedAddressString(QHostAddress(QString::fromStdString(address.ToStringIP())));
}

QSet<QString> localInterfaceAddresses()
{
    QSet<QString> addresses;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        const QNetworkInterface::InterfaceFlags flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning) || (flags & QNetworkInterface::IsLoopBack)) continue;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            const QHostAddress address = entry.ip();
            if (address.isNull() || address.protocol() == QAbstractSocket::UnknownNetworkLayerProtocol) continue;
            addresses.insert(normalizedAddressString(address));
        }
    }
    addresses.insert(normalizedAddressString(QHostAddress(QHostAddress::LocalHost)));
    addresses.insert(normalizedAddressString(QHostAddress(QHostAddress::LocalHostIPv6)));
    return addresses;
}

bool isLocalInterfaceAddress(const CAddress& address)
{
    const QString normalized = normalizedAddressString(address);
    return !normalized.isEmpty() && localInterfaceAddresses().contains(normalized);
}

QHostAddress localDisplayAddress()
{
    QHostAddress fallback;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        const QNetworkInterface::InterfaceFlags flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning) || (flags & QNetworkInterface::IsLoopBack)) continue;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            const QHostAddress address = entry.ip();
            if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                const quint32 ip = address.toIPv4Address();
                if ((ip & 0xff000000U) == 0x0a000000U ||
                    (ip & 0xfff00000U) == 0xac100000U ||
                    (ip & 0xffff0000U) == 0xc0a80000U) {
                    return address;
                }
                if (fallback.isNull()) fallback = address;
            } else if (fallback.isNull() && address.protocol() == QAbstractSocket::IPv6Protocol) {
                const Q_IPV6ADDR bytes = address.toIPv6Address();
                const bool link_local = bytes[0] == 0xfe && (bytes[1] & 0xc0) == 0x80;
                if (!link_local) fallback = address;
            }
        }
    }
    return fallback.isNull() ? QHostAddress(QHostAddress::LocalHost) : fallback;
}

QString nodeAddressText(const CNodeStats& stats)
{
    if (stats.nodeid == -1) {
        const QString ip = QString::fromStdString(stats.addr.ToStringIP());
        return ip.isEmpty() ? QObject::tr("localhost") : ip;
    }
    return QString::fromStdString(stats.addr.ToStringIP());
}

QString nodePortText(const CNodeStats& stats)
{
    const QString port = QString::fromStdString(stats.addr.ToStringPort());
    return port.isEmpty() ? QString::number(GetListenPort()) : port;
}

QString localHostDescription()
{
    const QString host = QHostInfo::localHostName().trimmed();
    const QString os = QSysInfo::prettyProductName().trimmed();
    if (!host.isEmpty() && !os.isEmpty()) {
        return QStringLiteral("%1 [%2]").arg(host, os);
    }
    if (!host.isEmpty()) return host;
    if (!os.isEmpty()) return os;
    return QObject::tr("localhost");
}

bool isLanFqdnCandidate(const QString& name)
{
    const QString value = name.trimmed();
    return value.contains(QLatin1Char('.')) && !value.endsWith(QStringLiteral(".local"), Qt::CaseInsensitive);
}

QString geoSortKey(const CAddress& address)
{
    if (isLocalInterfaceAddress(address)) return QStringLiteral("00-local");
    if (isLanPeer(address)) return QStringLiteral("01-lan");
    return QStringLiteral("10-") + peerLookupKey(address);
}

void appendUniquePort(QStringList& ports, const QString& port)
{
    if (!port.isEmpty() && !ports.contains(port)) {
        ports.append(port);
    }
}

void mergePeerStats(CNodeCombinedStats& merged, const CNodeCombinedStats& incoming)
{
    for (const QString& port : incoming.displayPorts) {
        appendUniquePort(merged.displayPorts, port);
    }

    merged.nodeStats.nSendBytes += incoming.nodeStats.nSendBytes;
    merged.nodeStats.nRecvBytes += incoming.nodeStats.nRecvBytes;
    merged.nodeStats.nLastSend = std::max(merged.nodeStats.nLastSend, incoming.nodeStats.nLastSend);
    merged.nodeStats.nLastRecv = std::max(merged.nodeStats.nLastRecv, incoming.nodeStats.nLastRecv);
    merged.nodeStats.nLastTXTime = std::max(merged.nodeStats.nLastTXTime, incoming.nodeStats.nLastTXTime);
    merged.nodeStats.nLastBlockTime = std::max(merged.nodeStats.nLastBlockTime, incoming.nodeStats.nLastBlockTime);
    merged.nodeStats.nServices = ServiceFlags(merged.nodeStats.nServices | incoming.nodeStats.nServices);

    if (merged.nodeStats.cleanSubVer.empty() && !incoming.nodeStats.cleanSubVer.empty()) {
        merged.nodeStats.cleanSubVer = incoming.nodeStats.cleanSubVer;
    }
    if (!merged.fNodeStateStatsAvailable && incoming.fNodeStateStatsAvailable) {
        merged.fNodeStateStatsAvailable = incoming.fNodeStateStatsAvailable;
        merged.nodeStateStats = incoming.nodeStateStats;
    }
    if (isReasonablePing(incoming.nodeStats.m_ping_usec) &&
        (!isReasonablePing(merged.nodeStats.m_ping_usec) || incoming.nodeStats.m_ping_usec < merged.nodeStats.m_ping_usec)) {
        merged.nodeStats.m_ping_usec = incoming.nodeStats.m_ping_usec;
    }
    if (isReasonablePing(incoming.nodeStats.m_min_ping_usec) &&
        (!isReasonablePing(merged.nodeStats.m_min_ping_usec) || incoming.nodeStats.m_min_ping_usec < merged.nodeStats.m_min_ping_usec)) {
        merged.nodeStats.m_min_ping_usec = incoming.nodeStats.m_min_ping_usec;
    }
    if (isReasonablePing(incoming.nodeStats.m_ping_wait_usec) &&
        (!isReasonablePing(merged.nodeStats.m_ping_wait_usec) || incoming.nodeStats.m_ping_wait_usec < merged.nodeStats.m_ping_wait_usec)) {
        merged.nodeStats.m_ping_wait_usec = incoming.nodeStats.m_ping_wait_usec;
    }
}

QString reverseDnsNameForAddress(const QHostAddress& address)
{
    if (address.protocol() == QAbstractSocket::IPv4Protocol) {
        QStringList octets = address.toString().split('.');
        if (octets.size() != 4) return QString();
        std::reverse(octets.begin(), octets.end());
        return octets.join('.') + QStringLiteral(".in-addr.arpa");
    }

    if (address.protocol() == QAbstractSocket::IPv6Protocol) {
        const Q_IPV6ADDR bytes = address.toIPv6Address();
        QStringList nibbles;
        nibbles.reserve(32);
        for (int i = 15; i >= 0; --i) {
            nibbles << QString::number(bytes[i] & 0x0f, 16);
            nibbles << QString::number((bytes[i] >> 4) & 0x0f, 16);
        }
        return nibbles.join('.') + QStringLiteral(".ip6.arpa");
    }

    return QString();
}

QString cymruDnsNameForAddress(const QHostAddress& address)
{
    if (address.protocol() == QAbstractSocket::IPv4Protocol) {
        QStringList octets = address.toString().split('.');
        if (octets.size() != 4) return QString();
        std::reverse(octets.begin(), octets.end());
        return octets.join('.') + QStringLiteral(".origin.asn.cymru.com");
    }

    if (address.protocol() == QAbstractSocket::IPv6Protocol) {
        const Q_IPV6ADDR bytes = address.toIPv6Address();
        QStringList nibbles;
        nibbles.reserve(32);
        for (int i = 15; i >= 0; --i) {
            nibbles << QString::number(bytes[i] & 0x0f, 16);
            nibbles << QString::number((bytes[i] >> 4) & 0x0f, 16);
        }
        return nibbles.join('.') + QStringLiteral(".origin6.asn.cymru.com");
    }

    return QString();
}

QString normalizeDnsName(QString name)
{
    if (name.endsWith('.')) name.chop(1);
    return name;
}

QString countryFlag(const QString& country_code)
{
    const QString code = country_code.toUpper();
    if (code.size() != 2) return QString();
    const ushort first = code.at(0).unicode();
    const ushort second = code.at(1).unicode();
    if (first < 'A' || first > 'Z' || second < 'A' || second > 'Z') return QString();

    QString flag;
    const uint first_symbol = 0x1F1E6 + first - 'A';
    const uint second_symbol = 0x1F1E6 + second - 'A';
    flag.append(QChar::highSurrogate(first_symbol));
    flag.append(QChar::lowSurrogate(first_symbol));
    flag.append(QChar::highSurrogate(second_symbol));
    flag.append(QChar::lowSurrogate(second_symbol));
    return flag;
}

QString abbreviatedServices(ServiceFlags services)
{
    QStringList flags;
    if (services & NODE_NETWORK) flags << QStringLiteral("N");
    if (services & NODE_GETUTXO) flags << QStringLiteral("G");
    if (services & NODE_BLOOM) flags << QStringLiteral("B");
    if (services & NODE_WITNESS) flags << QStringLiteral("W");
    if (services & NODE_COMPACT_FILTERS) flags << QStringLiteral("CF");
    if (services & NODE_NETWORK_LIMITED) flags << QStringLiteral("NL");
    if (services & NODE_MWEB) flags << QStringLiteral("M");
    if (services & NODE_MWEB_LIGHT_CLIENT) flags << QStringLiteral("MLC");
    return flags.isEmpty() ? QStringLiteral("-") : flags.join(QStringLiteral(" "));
}

QString permissionsText(NetPermissionFlags permissions)
{
    if (permissions == PF_NONE) return QStringLiteral("N/A");

    QStringList permission_strings;
    for (const auto& permission : NetPermissions::ToStrings(permissions)) {
        permission_strings.append(QString::fromStdString(permission));
    }
    return permission_strings.join(QStringLiteral(" & "));
}

QString durationSince(qint64 timestamp)
{
    if (timestamp <= 0) return QStringLiteral("never");
    return GUIUtil::formatDurationStr(QDateTime::currentSecsSinceEpoch() - timestamp);
}

QString nodeStateHeight(bool available, int height)
{
    if (!available || height < 0) return QStringLiteral("Unknown");
    return QString::number(height);
}

QString pingHealthSparkline(const QVector<qint64>& samples)
{
    if (samples.isEmpty()) return QString();

    qint64 low = samples.constFirst();
    qint64 high = samples.constFirst();
    for (const qint64 sample : samples) {
        low = std::min(low, sample);
        high = std::max(high, sample);
    }

    static const QString levels = QString::fromUtf8("▁▂▃▄▅▆▇█");
    QString out;
    out.reserve(samples.size());
    const qint64 span = std::max<qint64>(1, high - low);
    for (const qint64 sample : samples) {
        const int idx = std::min(7, static_cast<int>((sample - low) * 7 / span));
        out.append(levels.at(idx));
    }
    return out;
}

bool isBannedPeerAddress(const CAddress& address, const banmap_t& bans)
{
    for (const auto& ban : bans) {
        if (ban.first.Match(address)) return true;
    }
    return false;
}

int addressFamilySortRank(const CAddress& address)
{
    if (address.IsIPv4()) return 0;
    if (address.IsIPv6()) return 1;
    return 2;
}

bool endpointLessThan(const CAddress& left, const CAddress& right)
{
    const int left_rank = addressFamilySortRank(left);
    const int right_rank = addressFamilySortRank(right);
    if (left_rank != right_rank) return left_rank < right_rank;

    const std::vector<unsigned char> left_bytes = left.GetAddrBytes();
    const std::vector<unsigned char> right_bytes = right.GetAddrBytes();
    if (left_bytes != right_bytes) return left_bytes < right_bytes;
    return left.GetPort() < right.GetPort();
}
} // namespace

bool NodeLessThan::operator()(const CNodeCombinedStats &left, const CNodeCombinedStats &right) const
{
    const CNodeStats *pLeft = &(left.nodeStats);
    const CNodeStats *pRight = &(right.nodeStats);

    if (order == Qt::DescendingOrder)
        std::swap(pLeft, pRight);

    switch(column)
    {
    case PeerTableModel::NetNodeId:
        return pLeft->nodeid < pRight->nodeid;
    case PeerTableModel::Address:
        return endpointLessThan(pLeft->addr, pRight->addr);
    case PeerTableModel::Port:
        return pLeft->addr.GetPort() < pRight->addr.GetPort();
    case PeerTableModel::Fqdn:
        return seedDomainForAddress(pLeft->addr).compare(seedDomainForAddress(pRight->addr)) < 0;
    case PeerTableModel::CustomHostname:
        return seedDomainForAddress(pLeft->addr).compare(seedDomainForAddress(pRight->addr)) < 0;
    case PeerTableModel::Version:
        return pLeft->nVersion < pRight->nVersion;
    case PeerTableModel::Services:
        return pLeft->nServices < pRight->nServices;
    case PeerTableModel::Subversion:
        return pLeft->cleanSubVer.compare(pRight->cleanSubVer) < 0;
    case PeerTableModel::AvgPing:
    case PeerTableModel::Ping:
    case PeerTableModel::Jitter:
    case PeerTableModel::PingHealth:
        return pLeft->m_min_ping_usec < pRight->m_min_ping_usec;
    case PeerTableModel::Sent:
        return pLeft->nSendBytes < pRight->nSendBytes;
    case PeerTableModel::Received:
        return pLeft->nRecvBytes < pRight->nRecvBytes;
    case PeerTableModel::Direction:
        return pLeft->fInbound < pRight->fInbound;
    case PeerTableModel::Country:
        return geoSortKey(pLeft->addr).compare(geoSortKey(pRight->addr)) < 0;
    case PeerTableModel::CityState:
        return pLeft->addr.ToStringIP().compare(pRight->addr.ToStringIP()) < 0;
    case PeerTableModel::ASName:
    case PeerTableModel::ASHostingCompany:
        return pLeft->addr.ToStringIP().compare(pRight->addr.ToStringIP()) < 0;
    case PeerTableModel::Seed:
        return seedDomainForAddress(pLeft->addr).compare(seedDomainForAddress(pRight->addr)) < 0;
    case PeerTableModel::UniqueId:
        return peerUniqueBaseKey(*pLeft).compare(peerUniqueBaseKey(*pRight)) < 0;
    }

    return false;
}

// private implementation
class PeerTablePriv
{
public:
    /** Local cache of peer information */
    QList<CNodeCombinedStats> cachedNodeStats;
    /** Column to sort nodes by (default to unsorted) */
    int sortColumn{-1};
    /** Order (ascending or descending) to sort nodes by */
    Qt::SortOrder sortOrder;
    /** Index of rows by node ID */
    std::map<NodeId, int> mapNodeRows;
    struct PeerLookup {
        QString fqdn;
        QString netbios_name;
        QString country_code;
        QString mapped_as;
        QString as_name;
        QString as_company;
        QString city;
        QString state;
        double latitude{0.0};
        double longitude{0.0};
        bool coordinates_populated{false};
        bool fqdn_pending{false};
        bool netbios_pending{false};
        bool country_pending{false};
        bool as_name_pending{false};
        bool location_pending{false};
        qint64 next_fqdn_lookup{0};
        qint64 next_netbios_lookup{0};
        qint64 next_country_lookup{0};
        qint64 next_as_name_lookup{0};
        qint64 next_location_lookup{0};
        bool location_populated{false};
    };
    QHash<QString, PeerLookup> peer_lookup_cache;
    QHash<QString, CNodeCombinedStats> seen_node_stats;
    QHash<NodeId, QVector<qint64>> ping_history;
    QHash<NodeId, QVector<qint64>> average_ping_history;
    QHash<NodeId, qint64> last_ping_usec;
    QHash<QString, int> stable_unique_ids;
    QHash<QString, NodeId> active_unique_id_owners;
    int next_unique_id{2};
    int ping_health_sample_limit{24};
    int current_active_peer_count{0};
    bool show_connected_peers{true};
    bool show_inactive_peers{false};
    bool only_defcoin_user_agents{true};
    bool geolocation_enabled{true};
    bool mapped_as_lookup_enabled{false};
    bool demo_peers_enabled{false};
    QQueue<QString> fqdn_queue;
    QQueue<QString> netbios_queue;
    QQueue<QString> country_queue;
    QQueue<QString> location_queue;
    QSet<QString> queued_fqdn;
    QSet<QString> queued_netbios;
    QSet<QString> queued_country;
    QSet<QString> queued_location;
    int active_fqdn_lookups{0};
    int active_netbios_lookups{0};
    int active_country_lookups{0};
    int active_location_lookups{0};
    QQueue<qint64> location_request_timestamps;
    qint64 location_rate_pause_until{0};
    QTimer* location_rate_timer{nullptr};
    QNetworkAccessManager* location_manager{nullptr};

    void loadStableUniqueIds()
    {
#if ENABLE_DEFCOIN_FUN_UI
        const QByteArray raw = QSettings().value(QStringLiteral("PeerTableStableUniqueIds")).toByteArray();
        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (!doc.isObject()) return;

        int max_id = 1;
        const QJsonObject object = doc.object();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            const int value = it.value().toInt();
            if (value <= 1) continue;
            stable_unique_ids.insert(it.key(), value);
            max_id = std::max(max_id, value);
        }
        next_unique_id = std::max(next_unique_id, max_id + 1);
#endif
    }

    void saveStableUniqueIds() const
    {
#if ENABLE_DEFCOIN_FUN_UI
        QJsonObject object;
        for (auto it = stable_unique_ids.constBegin(); it != stable_unique_ids.constEnd(); ++it) {
            object.insert(it.key(), it.value());
        }
        QSettings().setValue(QStringLiteral("PeerTableStableUniqueIds"),
                             QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
#endif
    }

    int uniqueIdForNode(const CNodeCombinedStats& stats)
    {
        if (stats.nodeStats.nodeid == -1) return 1;
        QString key = peerUniqueBaseKey(stats.nodeStats);
        if (key.isEmpty()) key = QString::number(stats.nodeStats.nodeid);

        if (stats.fActive) {
            auto owner = active_unique_id_owners.find(key);
            if (owner == active_unique_id_owners.end()) {
                active_unique_id_owners.insert(key, stats.nodeStats.nodeid);
            } else if (owner.value() != stats.nodeStats.nodeid) {
                key += QStringLiteral("#%1").arg(stats.nodeStats.nodeid);
            }
        }

        auto existing = stable_unique_ids.find(key);
        if (existing != stable_unique_ids.end()) return existing.value();

        int candidate = stats.nodeStats.nodeid >= 0 && stats.nodeStats.nodeid < std::numeric_limits<int>::max() - 2
            ? static_cast<int>(stats.nodeStats.nodeid) + 2
            : next_unique_id;
        QSet<int> used;
        for (auto it = stable_unique_ids.constBegin(); it != stable_unique_ids.constEnd(); ++it) {
            used.insert(it.value());
        }
        while (candidate <= 1 || used.contains(candidate)) {
            candidate = next_unique_id++;
        }
        stable_unique_ids.insert(key, candidate);
        next_unique_id = std::max(next_unique_id, candidate + 1);
        saveStableUniqueIds();
        return candidate;
    }

    void populateDemoPeers()
    {
        cachedNodeStats.clear();
        seen_node_stats.clear();
        mapNodeRows.clear();
        current_active_peer_count = 0;

#if !ENABLE_DEFCOIN_FUN_UI
        return;
#else
        const auto& peers = DefcoinDemoPeers::peers();
        cachedNodeStats.reserve(peers.size());
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const qint64 tick = QDateTime::currentMSecsSinceEpoch() / 1000;

        for (int i = 0; i < peers.size(); ++i) {
            const DefcoinDemoPeers::Peer& peer = peers.at(i);
            const QString ip = DefcoinDemoPeers::ip(peer);
            const CService service = LookupNumeric(ip.toStdString(), peer.port);
            if (!service.IsValid()) continue;

            CNodeCombinedStats stats;
            stats.nodeStats.nodeid = 1000 + i;
            stats.nodeStats.addr = CAddress(service, ServiceFlags(NODE_NETWORK | NODE_BLOOM | NODE_WITNESS));
            stats.nodeStats.nServices = ServiceFlags(NODE_NETWORK | NODE_BLOOM | NODE_WITNESS);
            stats.nodeStats.addrName = stats.nodeStats.addr.ToStringIPPort();
            stats.nodeStats.addrLocal = QStringLiteral("demo.local:%1").arg(peer.port).toStdString();
            stats.nodeStats.nVersion = PROTOCOL_VERSION;
            stats.nodeStats.cleanSubVer = "/DefcoinCore:demo/";
            stats.nodeStats.fInbound = (i % 5) == 0;
            stats.nodeStats.m_manual_connection = false;
            stats.nodeStats.nStartingHeight = 2315000 + i;
            stats.nodeStats.nSendBytes = peer.sent;
            stats.nodeStats.nRecvBytes = peer.received;
            stats.nodeStats.nLastSend = now - (10 + (i % 8) * 7);
            stats.nodeStats.nLastRecv = now - (8 + (i % 6) * 5);
            stats.nodeStats.nLastTXTime = stats.nodeStats.nLastSend;
            stats.nodeStats.nLastBlockTime = stats.nodeStats.nLastRecv;
            stats.nodeStats.nTimeConnected = now - (600 + i * 37);
            stats.nodeStats.nTimeOffset = 0;
            stats.nodeStats.m_permissionFlags = PF_NONE;
            stats.nodeStats.m_legacyWhitelisted = false;
            stats.nodeStats.minFeeFilter = 0;
            stats.nodeStats.m_network = "demo";
            stats.nodeStats.m_mapped_as = 0;
            stats.nodeStats.m_conn_type_string = stats.nodeStats.fInbound ? "inbound" : "outbound";
            const qint64 jitter_ms = ((tick + i * 7) % 11) - 5;
            const qint64 ping_ms = std::max<qint64>(1, peer.ping_ms + jitter_ms);
            stats.nodeStats.m_ping_usec = ping_ms * 1000;
            stats.nodeStats.m_ping_wait_usec = 0;
            stats.nodeStats.m_min_ping_usec = std::max<qint64>(1, peer.ping_ms - 7) * 1000;
            appendUniquePort(stats.displayPorts, QString::number(peer.port));
            stats.fNodeStateStatsAvailable = false;
            stats.fActive = true;
            ++current_active_peer_count;

            PeerLookup& lookup = peer_lookup_cache[ip];
            lookup.fqdn = DefcoinDemoPeers::fqdn(peer);
            lookup.city.clear();
            lookup.state.clear();
            const QString city_state = DefcoinDemoPeers::cityState(peer);
            const int comma = city_state.indexOf(QStringLiteral(", "));
            if (comma > 0) {
                lookup.city = city_state.left(comma);
                lookup.state = city_state.mid(comma + 2);
            } else {
                lookup.city = city_state;
            }
            lookup.latitude = peer.latitude;
            lookup.longitude = peer.longitude;
            lookup.coordinates_populated = true;
            lookup.location_populated = true;
            lookup.location_pending = false;
            lookup.fqdn_pending = false;
            lookup.country_pending = false;
            lookup.mapped_as = QStringLiteral("demo");
            lookup.as_name = QStringLiteral("Demo network");
            lookup.as_company = QStringLiteral("Demo network");

            QVector<qint64>& history = ping_history[stats.nodeStats.nodeid];
            history.append(ping_ms);
            while (history.size() > 256) history.removeFirst();
            QVector<qint64>& average_history = average_ping_history[stats.nodeStats.nodeid];
            average_history.append(ping_ms);
            while (average_history.size() > 256) average_history.removeFirst();
            last_ping_usec[stats.nodeStats.nodeid] = ping_ms * 1000;

            seen_node_stats.insert(peerStatsKey(stats.nodeStats), stats);
            mapNodeRows.insert(std::pair<NodeId, int>(stats.nodeStats.nodeid, cachedNodeStats.size()));
            cachedNodeStats.append(stats);
        }
#endif
    }

    /** Pull a full list of peers from vNodes into our cache */
    void refreshPeers(interfaces::Node& node)
    {
        if (demo_peers_enabled) {
            populateDemoPeers();
            return;
        }

        {
            cachedNodeStats.clear();
            current_active_peer_count = 0;
            QSet<QString> active_peer_keys;
            QSet<QString> active_peer_endpoints;
            QHash<QString, int> active_peer_rows;
            active_unique_id_owners.clear();
            const QSet<QString> local_ips = localInterfaceAddresses();
            banmap_t ban_map;
            node.getBanned(ban_map);
            for (auto it = seen_node_stats.begin(); it != seen_node_stats.end();) {
                if (it.key() != QStringLiteral("local-node") && isBannedPeerAddress(it->nodeStats.addr, ban_map)) {
                    it = seen_node_stats.erase(it);
                } else {
                    ++it;
                }
            }

            interfaces::Node::NodesStats nodes_stats;
            node.getNodesStats(nodes_stats);
            cachedNodeStats.reserve(nodes_stats.size());
            for (const auto& node_stats : nodes_stats)
            {
                CNodeCombinedStats stats;
                stats.nodeStats = std::get<0>(node_stats);
                stats.fNodeStateStatsAvailable = std::get<1>(node_stats);
                stats.nodeStateStats = std::get<2>(node_stats);
                stats.fActive = true;
                const QString peer_key = peerStatsKey(stats.nodeStats);
                if (isBannedPeerAddress(stats.nodeStats.addr, ban_map)) {
                    if (!peer_key.isEmpty()) seen_node_stats.remove(peer_key);
                    continue;
                }
                if (only_defcoin_user_agents && !isDefcoinPrefixedUserAgent(stats.nodeStats.cleanSubVer)) {
                    continue;
                }
                const QString normalized_peer_ip = normalizedAddressString(stats.nodeStats.addr);
                if (!normalized_peer_ip.isEmpty() && local_ips.contains(normalized_peer_ip)) {
                    continue;
                }
                appendUniquePort(stats.displayPorts, nodePortText(stats.nodeStats));
                if (!peer_key.isEmpty() && seen_node_stats.contains(peer_key)) {
                    for (const QString& port : seen_node_stats.value(peer_key).displayPorts) {
                        appendUniquePort(stats.displayPorts, port);
                    }
                }
                if (show_connected_peers && !peer_key.isEmpty() && active_peer_rows.contains(peer_key)) {
                    CNodeCombinedStats& merged_stats = cachedNodeStats[active_peer_rows.value(peer_key)];
                    mergePeerStats(merged_stats, stats);
                    seen_node_stats.insert(peer_key, merged_stats);
                    active_peer_keys.insert(peer_key);
                    continue;
                }
                if (!peer_key.isEmpty() && active_peer_keys.contains(peer_key)) {
                    seen_node_stats.insert(peer_key, stats);
                    continue;
                }
                ++current_active_peer_count;
#if ENABLE_DEFCOIN_FUN_UI
                const qint64 health_ping_usec = bestPingUsec(stats.nodeStats);
                if (health_ping_usec > 0) {
                    QVector<qint64>& history = ping_history[stats.nodeStats.nodeid];
                    history.append(health_ping_usec / 1000);
                    while (history.size() > 256) {
                        history.removeFirst();
                    }
                }
#endif
                const qint64 sample_ping_usec = isReasonablePing(stats.nodeStats.m_ping_usec) ? stats.nodeStats.m_ping_usec : 0;
                if (sample_ping_usec > 0) {
                    QVector<qint64>& average_history = average_ping_history[stats.nodeStats.nodeid];
                    if (average_history.isEmpty() || last_ping_usec.value(stats.nodeStats.nodeid) != sample_ping_usec) {
                        average_history.append(sample_ping_usec / 1000);
                    }
                    last_ping_usec[stats.nodeStats.nodeid] = sample_ping_usec;
                    while (average_history.size() > 256) {
                        average_history.removeFirst();
                    }
                }
                if (!peer_key.isEmpty()) {
                    active_peer_keys.insert(peer_key);
                    const QString endpoint = peerEndpointKey(stats.nodeStats);
                    if (!endpoint.isEmpty()) active_peer_endpoints.insert(endpoint);
                    seen_node_stats.insert(peer_key, stats);
                }
                if (show_connected_peers) {
                    if (!peer_key.isEmpty()) active_peer_rows.insert(peer_key, cachedNodeStats.size());
                    cachedNodeStats.append(stats);
                    uniqueIdForNode(cachedNodeStats.last());
                }
            }

            if (only_defcoin_user_agents) {
                for (auto it = seen_node_stats.begin(); it != seen_node_stats.end();) {
                    if (it.key() != QStringLiteral("local-node") && !isDefcoinPrefixedUserAgent(it->nodeStats.cleanSubVer)) {
                        it = seen_node_stats.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            const bool show_local_node = QSettings().value(QStringLiteral("ShowLocalNodeInPeers"), true).toBool();
            if (show_connected_peers && show_local_node) {
                const QHostAddress local_address = localDisplayAddress();
                const QString local_ip = local_address.toString();
                const CService local_service = LookupNumeric(local_ip.toStdString(), GetListenPort());
                CNodeCombinedStats local_stats;
                local_stats.nodeStats.nodeid = -1;
                local_stats.nodeStats.addr = CAddress(local_service, ServiceFlags(NODE_NETWORK | NODE_BLOOM | NODE_WITNESS));
                local_stats.nodeStats.nServices = ServiceFlags(NODE_NETWORK | NODE_BLOOM | NODE_WITNESS);
                local_stats.nodeStats.nLastSend = 0;
                local_stats.nodeStats.nLastRecv = 0;
                local_stats.nodeStats.nLastTXTime = 0;
                local_stats.nodeStats.nLastBlockTime = 0;
                local_stats.nodeStats.nTimeConnected = QDateTime::currentSecsSinceEpoch();
                local_stats.nodeStats.nTimeOffset = 0;
                local_stats.nodeStats.addrName = local_stats.nodeStats.addr.ToStringIPPort();
                local_stats.nodeStats.nVersion = PROTOCOL_VERSION;
                local_stats.nodeStats.cleanSubVer = FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, std::vector<std::string>());
                local_stats.nodeStats.fInbound = false;
                local_stats.nodeStats.m_manual_connection = false;
                local_stats.nodeStats.nStartingHeight = 0;
                local_stats.nodeStats.nSendBytes = 0;
                local_stats.nodeStats.nRecvBytes = 0;
                local_stats.nodeStats.m_permissionFlags = PF_NONE;
                local_stats.nodeStats.m_legacyWhitelisted = false;
                local_stats.nodeStats.m_ping_usec = 0;
                local_stats.nodeStats.m_ping_wait_usec = 0;
                local_stats.nodeStats.m_min_ping_usec = 0;
                local_stats.nodeStats.minFeeFilter = 0;
                local_stats.nodeStats.addrLocal = local_stats.nodeStats.addr.ToStringIPPort();
                local_stats.nodeStats.m_network = "local";
                local_stats.nodeStats.m_mapped_as = 0;
                local_stats.nodeStats.m_conn_type_string = "local";
                appendUniquePort(local_stats.displayPorts, QString::number(GetListenPort()));
                local_stats.fNodeStateStatsAvailable = false;
                local_stats.fActive = true;
                active_peer_keys.insert(QStringLiteral("local-node"));
                seen_node_stats.insert(QStringLiteral("local-node"), local_stats);
                cachedNodeStats.prepend(local_stats);
                uniqueIdForNode(cachedNodeStats.first());
            } else {
                seen_node_stats.remove(QStringLiteral("local-node"));
            }

            if (show_inactive_peers) {
                for (auto it = seen_node_stats.constBegin(); it != seen_node_stats.constEnd(); ++it) {
                    if (active_peer_keys.contains(it.key())) continue;
                    const QString endpoint = peerEndpointKey(it.value().nodeStats);
                    if (!endpoint.isEmpty() && active_peer_endpoints.contains(endpoint)) continue;
                    if (it.key() != QStringLiteral("local-node") && isBannedPeerAddress(it->nodeStats.addr, ban_map)) continue;
                    CNodeCombinedStats inactive_stats = it.value();
                    inactive_stats.fActive = false;
                    cachedNodeStats.append(inactive_stats);
                    uniqueIdForNode(cachedNodeStats.last());
                }
            }
        }

        if (sortColumn >= 0)
            // sort cacheNodeStats (use stable sort to prevent rows jumping around unnecessarily)
            std::stable_sort(cachedNodeStats.begin(), cachedNodeStats.end(), NodeLessThan(sortColumn, sortOrder));

        // build index map
        mapNodeRows.clear();
        int row = 0;
        for (const CNodeCombinedStats& stats : cachedNodeStats)
            mapNodeRows.insert(std::pair<NodeId, int>(stats.nodeStats.nodeid, row++));
    }

    int size() const
    {
        return cachedNodeStats.size();
    }

    CNodeCombinedStats *index(int idx)
    {
        if (idx >= 0 && idx < cachedNodeStats.size())
            return &cachedNodeStats[idx];

        return nullptr;
    }

    QString fqdnForPeer(const CAddress& address) const
    {
        if (isLocalInterfaceAddress(address)) {
            return QStringLiteral("[NA: LAN]");
        }
        const auto it = peer_lookup_cache.constFind(peerLookupKey(address));
        const QString reverse_name = it == peer_lookup_cache.constEnd() ? QString() : it->fqdn;
        if (isLanPeer(address)) {
            return isLanFqdnCandidate(reverse_name) ? reverse_name : QStringLiteral("[NA: LAN]");
        }
        if (!reverse_name.isEmpty()) return reverse_name;
        return isLanPeer(address) ? QStringLiteral("[NA: LAN]") : QString();
    }

    QString customHostnameForPeer(const CAddress& address) const
    {
        if (isLocalInterfaceAddress(address)) return localHostDescription();
        const QString seed_domain = seedDomainForAddress(address);
        if (!seed_domain.isEmpty()) return seed_domain;
        if (!isLanPeer(address)) return QString();

        const auto it = peer_lookup_cache.constFind(peerLookupKey(address));
        if (it == peer_lookup_cache.constEnd()) return QString();
        if (!it->netbios_name.isEmpty()) return it->netbios_name;
        if (!it->fqdn.isEmpty() && !isLanFqdnCandidate(it->fqdn)) return it->fqdn;
        return QString();
    }

    QString countryForPeer(const CAddress& address) const
    {
        if (isLocalInterfaceAddress(address)) return QString::fromUtf8("🏠");
        if (isLanPeer(address)) return QStringLiteral("LAN");
        const auto it = peer_lookup_cache.constFind(peerLookupKey(address));
        if (it == peer_lookup_cache.constEnd()) return QString();
        return countryFlag(it->country_code);
    }

    QString mappedASForPeer(const CAddress& address) const
    {
        if (isLocalInterfaceAddress(address) || isLanPeer(address)) return QString();
        const auto it = peer_lookup_cache.constFind(peerLookupKey(address));
        if (it == peer_lookup_cache.constEnd()) return QString();
        return it->mapped_as;
    }

    QString asNameForPeer(const CAddress& address) const
    {
        if (isLocalInterfaceAddress(address) || isLanPeer(address)) return QString();
        const auto it = peer_lookup_cache.constFind(peerLookupKey(address));
        if (it == peer_lookup_cache.constEnd()) return QString();
        return it->as_name;
    }

    QString asCompanyForPeer(const CAddress& address) const
    {
        if (isLocalInterfaceAddress(address) || isLanPeer(address)) return QString();
        const auto it = peer_lookup_cache.constFind(peerLookupKey(address));
        if (it == peer_lookup_cache.constEnd()) return QString();
        return it->as_company;
    }

    QString geoMarkerForPeer(const CAddress& address) const
    {
        if (isLocalInterfaceAddress(address)) return QStringLiteral("local");
        if (isLanPeer(address)) return QStringLiteral("lan");
        return QStringLiteral("country");
    }

    QString cityStateForPeer(const CAddress& address) const
    {
        const QString key = (isLocalInterfaceAddress(address) || isLanPeer(address)) ? WAN_LOCATION_LOOKUP_KEY : peerLookupKey(address);
        const auto it = peer_lookup_cache.constFind(key);
        if (it == peer_lookup_cache.constEnd() || !it->location_populated) return QString();

        QStringList parts;
        if (!it->city.isEmpty()) parts << it->city;
        if (!it->state.isEmpty() && it->state != it->city) parts << it->state;
        return parts.join(QStringLiteral(", "));
    }

    QVariant latitudeForPeer(const CAddress& address) const
    {
        const QString key = (isLocalInterfaceAddress(address) || isLanPeer(address)) ? WAN_LOCATION_LOOKUP_KEY : peerLookupKey(address);
        const auto it = peer_lookup_cache.constFind(key);
        if (it == peer_lookup_cache.constEnd() || !it->coordinates_populated) return QVariant();
        return it->latitude;
    }

    QVariant longitudeForPeer(const CAddress& address) const
    {
        const QString key = (isLocalInterfaceAddress(address) || isLanPeer(address)) ? WAN_LOCATION_LOOKUP_KEY : peerLookupKey(address);
        const auto it = peer_lookup_cache.constFind(key);
        if (it == peer_lookup_cache.constEnd() || !it->coordinates_populated) return QVariant();
        return it->longitude;
    }

    QString pingHealthForNode(NodeId nodeid) const
    {
        QVector<qint64> samples = ping_history.value(nodeid);
        if (samples.size() > ping_health_sample_limit) {
            samples = samples.mid(samples.size() - ping_health_sample_limit);
        }
        return pingHealthSparkline(samples);
    }

    QVariantList pingHistoryForNode(NodeId nodeid) const
    {
        QVector<qint64> samples = ping_history.value(nodeid);
        if (samples.size() > ping_health_sample_limit) {
            samples = samples.mid(samples.size() - ping_health_sample_limit);
        }

        QVariantList values;
        values.reserve(samples.size());
        for (const qint64 sample : samples) {
            values << sample;
        }
        return values;
    }

    qint64 averagePingForNode(NodeId nodeid) const
    {
        const QVector<qint64> samples = average_ping_history.value(nodeid);
        if (samples.isEmpty()) return 0;

        qint64 total = 0;
        for (const qint64 sample : samples) total += sample;
        return total / samples.size();
    }

    qint64 jitterForNode(NodeId nodeid) const
    {
        const QVector<qint64> samples = average_ping_history.value(nodeid);
        if (samples.size() < 2) return -1;

        qint64 total_delta = 0;
        int delta_count = 0;
        for (int i = 1; i < samples.size(); ++i) {
            total_delta += qAbs(samples.at(i) - samples.at(i - 1));
            ++delta_count;
        }
        if (delta_count <= 0) return -1;
        return total_delta / delta_count;
    }

    int userAgentCount(const std::string& user_agent) const
    {
        int count = 0;
        const QString display_agent = displayUserAgent(user_agent);
        for (const CNodeCombinedStats& stats : cachedNodeStats) {
#if ENABLE_DEFCOIN_FUN_UI
            if (stats.fActive && displayUserAgent(stats.nodeStats.cleanSubVer) == display_agent) ++count;
#else
            if (stats.fActive && stats.nodeStats.nodeid != -1 && displayUserAgent(stats.nodeStats.cleanSubVer) == display_agent) ++count;
#endif
        }
        return count;
    }

    NodeId displayNodeId(const CNodeStats& stats)
    {
        return stats.nodeid;
    }

    int activePeerCount() const
    {
        return current_active_peer_count;
    }

    int seenPeerCount() const
    {
        int count = 0;
        for (auto it = seen_node_stats.constBegin(); it != seen_node_stats.constEnd(); ++it) {
            if (it.key() != QLatin1String("local-node")) ++count;
        }
        return count;
    }
};

PeerTableModel::PeerTableModel(interfaces::Node& node, QObject* parent) :
    QAbstractTableModel(parent),
    m_node(node),
    timer(nullptr)
{
#if ENABLE_DEFCOIN_FUN_UI
    default_columns << tr("Node ID") << tr("IP Address: Port") << tr("Port") << tr("FQDN") << tr("Domain Alias") << tr("Version") << tr("Svcs") << tr("Avg Ping") << tr("Ping Time") << tr("Jitter") << tr("Traffic Health") << tr("Sent") << tr("Rec'd") << tr("User Agent") << tr("UA Count") << tr("Geo") << tr("City, St")
                    << tr("Permissions") << tr("Direction") << tr("Starting Block") << tr("Synced Headers") << tr("Synced Blocks") << tr("Connection Time") << tr("Last Send") << tr("Last Receive")
                    << tr("Ping Wait") << tr("Min. Ping") << tr("Time Offset") << tr("AS Number") << tr("AS Name") << tr("AS Hosting Company") << tr("Seed") << tr("UniqID");
#else
    default_columns << tr("Node ID") << tr("IP Address: Port") << tr("Port") << tr("FQDN") << tr("Domain Alias") << tr("Version") << tr("Svcs") << tr("Avg Ping") << tr("Ping Time") << tr("Jitter") << tr("Traffic Health") << tr("Sent") << tr("Rec'd") << tr("User Agent") << tr("UA Count") << tr("Geo") << tr("City, St")
                    << tr("Permissions") << tr("Direction") << tr("Starting Block") << tr("Synced Headers") << tr("Synced Blocks") << tr("Connection Time") << tr("Last Send") << tr("Last Receive")
                    << tr("Ping Wait") << tr("Min. Ping") << tr("Time Offset") << tr("AS Number") << tr("AS Name") << tr("AS Hosting Company") << tr("Seed");
#endif
    columns = default_columns;
    priv.reset(new PeerTablePriv());
    priv->loadStableUniqueIds();
    priv->show_connected_peers = QSettings().value(QStringLiteral("PeerTableShowConnectedPeers"), true).toBool();
    priv->show_inactive_peers = QSettings().value(QStringLiteral("PeerTableShowInactivePeers"), true).toBool();
    priv->only_defcoin_user_agents = QSettings().value(QStringLiteral("OnlyDefcoinUserAgents"), DEFAULT_DEFCOIN_USER_AGENT_FILTER).toBool();
#if ENABLE_DEFCOIN_FUN_UI
    priv->geolocation_enabled = QSettings().value(QStringLiteral("PeerTableGetLocation"), true).toBool();
    priv->mapped_as_lookup_enabled = QSettings().value(QStringLiteral("PeerTableLookupMappedAS"), false).toBool();
#else
    priv->geolocation_enabled = false;
    priv->mapped_as_lookup_enabled = false;
#endif
    priv->location_manager = new QNetworkAccessManager(this);
    priv->location_rate_timer = new QTimer(this);
    priv->location_rate_timer->setSingleShot(true);
    connect(priv->location_rate_timer, &QTimer::timeout, this, &PeerTableModel::processPeerLookupQueues);

    // set up timer for auto refresh
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &PeerTableModel::refresh);
    timer->setInterval(MODEL_UPDATE_DELAY);

    // load initial data
    refresh();
}

PeerTableModel::~PeerTableModel()
{
    // Intentionally left empty
}

QString PeerTableModel::columnTitle(int column) const
{
    if (column < 0 || column >= columns.size()) return QString();
    return columns.at(column);
}

QString PeerTableModel::defaultColumnTitle(int column) const
{
    if (column < 0 || column >= default_columns.size()) return QString();
    return default_columns.at(column);
}

void PeerTableModel::setColumnTitle(int column, const QString& title)
{
    if (column < 0 || column >= columns.size()) return;
    const QString trimmed_title = title.trimmed();
    const QString new_title = trimmed_title.isEmpty() ? defaultColumnTitle(column) : trimmed_title;
    if (columns[column] == new_title) return;
    columns[column] = new_title;
    Q_EMIT headerDataChanged(Qt::Horizontal, column, column);
}

void PeerTableModel::resetColumnTitles()
{
    columns = default_columns;
    if (!columns.isEmpty()) {
        Q_EMIT headerDataChanged(Qt::Horizontal, 0, columns.size() - 1);
    }
}

void PeerTableModel::setPingHealthSampleLimit(int limit)
{
    const int bounded_limit = std::max(4, std::min(limit, 360));
    if (priv->ping_health_sample_limit == bounded_limit) return;
    priv->ping_health_sample_limit = bounded_limit;
    if (rowCount(QModelIndex()) > 0) {
        Q_EMIT dataChanged(index(0, PingHealth, QModelIndex()), index(rowCount(QModelIndex()) - 1, PingHealth, QModelIndex()), {Qt::DisplayRole, Qt::ToolTipRole, Qt::UserRole});
    }
}

void PeerTableModel::setShowConnectedPeers(bool show)
{
    if (priv->show_connected_peers == show) return;
    priv->show_connected_peers = show;
    refresh();
}

bool PeerTableModel::showConnectedPeers() const
{
    return priv->show_connected_peers;
}

void PeerTableModel::setShowInactivePeers(bool show)
{
    if (priv->show_inactive_peers == show) return;
    priv->show_inactive_peers = show;
    refresh();
}

bool PeerTableModel::showInactivePeers() const
{
    return priv->show_inactive_peers;
}

void PeerTableModel::setOnlyDefcoinUserAgents(bool only_defcoin)
{
    if (priv->only_defcoin_user_agents == only_defcoin) return;
    priv->only_defcoin_user_agents = only_defcoin;
    refresh();
}

bool PeerTableModel::onlyDefcoinUserAgents() const
{
    return priv->only_defcoin_user_agents;
}

void PeerTableModel::setGeolocationEnabled(bool enabled)
{
    if (priv->geolocation_enabled == enabled) return;
    priv->geolocation_enabled = enabled;

    if (!enabled) {
        priv->location_queue.clear();
        priv->queued_location.clear();
        for (auto it = priv->peer_lookup_cache.begin(); it != priv->peer_lookup_cache.end(); ++it) {
            it->location_pending = false;
        }
        if (rowCount(QModelIndex()) > 0) {
            Q_EMIT dataChanged(index(0, CityState, QModelIndex()), index(rowCount(QModelIndex()) - 1, CityState, QModelIndex()), {Qt::DisplayRole, Qt::ToolTipRole});
        }
        return;
    }

    refresh();
}

bool PeerTableModel::geolocationEnabled() const
{
    return priv->geolocation_enabled;
}

void PeerTableModel::setMappedASLookupEnabled(bool enabled)
{
    if (priv->mapped_as_lookup_enabled == enabled) return;
    priv->mapped_as_lookup_enabled = enabled;
    QSettings().setValue(QStringLiteral("PeerTableLookupMappedAS"), enabled);
    if (!enabled && !priv->geolocation_enabled) {
        priv->country_queue.clear();
        priv->queued_country.clear();
        for (auto it = priv->peer_lookup_cache.begin(); it != priv->peer_lookup_cache.end(); ++it) {
            it->country_pending = false;
            it->as_name_pending = false;
        }
    }
    if (rowCount(QModelIndex()) > 0) {
        Q_EMIT dataChanged(index(0, MappedAS, QModelIndex()), index(rowCount(QModelIndex()) - 1, ASHostingCompany, QModelIndex()), {Qt::DisplayRole, Qt::ToolTipRole});
    }
    refresh();
}

bool PeerTableModel::mappedASLookupEnabled() const
{
    return priv->mapped_as_lookup_enabled;
}

void PeerTableModel::prioritizeLookupsForNode(NodeId nodeid)
{
    if (!priv) return;
    const int row = getRowByNodeId(nodeid);
    if (row < 0) return;

    const CNodeCombinedStats* stats = priv->index(row);
    if (!stats) return;

    const bool lan_peer = isLanPeer(stats->nodeStats.addr);
    const bool local_or_lan_peer = lan_peer || isLocalInterfaceAddress(stats->nodeStats.addr);
    const bool public_peer_lookup = shouldLookupPeer(stats->nodeStats.addr) && !lan_peer;
    const QString key = peerLookupKey(stats->nodeStats.addr);
    const QHostAddress address(key);
    if (key.isEmpty() || address.isNull()) return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    PeerTablePriv::PeerLookup& lookup = priv->peer_lookup_cache[key];

    if ((priv->geolocation_enabled || priv->mapped_as_lookup_enabled) &&
        public_peer_lookup &&
        !lookup.country_pending &&
        now >= lookup.next_country_lookup &&
        !cymruDnsNameForAddress(address).isEmpty()) {
        priv->country_queue.removeAll(key);
        priv->queued_country.insert(key);
        priv->country_queue.prepend(key);
    }

    if (priv->geolocation_enabled) {
        const QString location_key = local_or_lan_peer ? WAN_LOCATION_LOOKUP_KEY : key;
        PeerTablePriv::PeerLookup& location_lookup = priv->peer_lookup_cache[location_key];
        if (!location_lookup.location_pending &&
            !location_lookup.location_populated &&
            now >= location_lookup.next_location_lookup) {
            priv->location_queue.removeAll(location_key);
            priv->queued_location.insert(location_key);
            priv->location_queue.prepend(location_key);
        }
    }

    processPeerLookupQueues();
}

void PeerTableModel::setDemoPeersEnabled(bool enabled)
{
    if (priv->demo_peers_enabled == enabled) return;
    Q_EMIT layoutAboutToBeChanged();
    priv->demo_peers_enabled = enabled;
    priv->refreshPeers(m_node);
    Q_EMIT layoutChanged();
}

bool PeerTableModel::demoPeersEnabled() const
{
    return priv->demo_peers_enabled;
}

int PeerTableModel::activePeerCount() const
{
    return priv->activePeerCount();
}

int PeerTableModel::seenPeerCount() const
{
    return priv->seenPeerCount();
}

void PeerTableModel::startAutoRefresh()
{
    timer->start();
}

void PeerTableModel::stopAutoRefresh()
{
    timer->stop();
}

int PeerTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int PeerTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant PeerTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    CNodeCombinedStats *rec = static_cast<CNodeCombinedStats*>(index.internalPointer());

    if (role == PeerAddressRole) {
        return QString::fromStdString(rec->nodeStats.addr.ToStringIP());
    } else if (role == PeerPortsRole) {
        return rec->displayPorts.isEmpty() ? nodePortText(rec->nodeStats) : rec->displayPorts.join(QStringLiteral(", "));
    } else if (role == PeerLatitudeRole) {
        return priv->latitudeForPeer(rec->nodeStats.addr);
    } else if (role == PeerLongitudeRole) {
        return priv->longitudeForPeer(rec->nodeStats.addr);
    } else if (role == PeerIsLanRole) {
        return isLanPeer(rec->nodeStats.addr);
    } else if (role == PeerIsLocalRole) {
        return isLocalInterfaceAddress(rec->nodeStats.addr);
    } else if (role == PeerUserAgentRole) {
        return displayUserAgent(rec->nodeStats.cleanSubVer);
    } else if (role == PeerSentBytesRole) {
        return static_cast<qulonglong>(rec->nodeStats.nSendBytes);
    } else if (role == PeerReceivedBytesRole) {
        return static_cast<qulonglong>(rec->nodeStats.nRecvBytes);
    } else if (role == PeerCityStateRole) {
        return priv->cityStateForPeer(rec->nodeStats.addr);
    } else if (role == PeerActualNodeIdRole) {
        return (qint64)rec->nodeStats.nodeid;
    } else if (role == PeerUniqueIdRole) {
        return priv->uniqueIdForNode(*rec);
    } else if (role == PeerFingerprintRole) {
        return peerFingerprintText(rec->nodeStats);
    }

    if (role == Qt::DisplayRole) {
        switch(index.column())
        {
        case NetNodeId:
            return (qint64)priv->displayNodeId(rec->nodeStats);
        case UniqueId:
            return priv->uniqueIdForNode(*rec);
        case Address:
#if ENABLE_DEFCOIN_FUN_UI
            return alignIPv4EndpointForDisplay(nodeAddressText(rec->nodeStats));
#else
            return alignIPv4EndpointForDisplay(QString::fromStdString(rec->nodeStats.addrName));
#endif
        case Port:
            return rec->displayPorts.isEmpty() ? nodePortText(rec->nodeStats) : rec->displayPorts.join(QStringLiteral(", "));
        case Fqdn:
            return priv->fqdnForPeer(rec->nodeStats.addr);
        case CustomHostname:
            return priv->customHostnameForPeer(rec->nodeStats.addr);
        case Version:
            return rec->nodeStats.nVersion > 0 ? QString::number(rec->nodeStats.nVersion) : QString();
        case Services:
            return abbreviatedServices(rec->nodeStats.nServices);
        case Subversion:
            return displayUserAgent(rec->nodeStats.cleanSubVer);
        case AvgPing:
        {
            const qint64 average_ms = priv->averagePingForNode(rec->nodeStats.nodeid);
            return average_ms > 0 ? GUIUtil::formatPingTime(average_ms * 1000) : QStringLiteral("N/A");
        }
        case Ping:
            return formatPingUsec(rec->nodeStats.m_ping_usec);
        case Jitter:
        {
            const qint64 jitter_ms = priv->jitterForNode(rec->nodeStats.nodeid);
            return jitter_ms >= 0 ? GUIUtil::formatPingTime(jitter_ms * 1000) : QStringLiteral("N/A");
        }
        case PingHealth:
            return priv->pingHealthForNode(rec->nodeStats.nodeid);
        case Sent:
            return GUIUtil::formatBytes(rec->nodeStats.nSendBytes);
        case Received:
            return GUIUtil::formatBytes(rec->nodeStats.nRecvBytes);
        case UserAgentCount:
            return priv->userAgentCount(rec->nodeStats.cleanSubVer);
        case Country:
            return priv->countryForPeer(rec->nodeStats.addr);
        case CityState:
            return priv->cityStateForPeer(rec->nodeStats.addr);
        case Seed:
            return seedDomainForAddress(rec->nodeStats.addr).isEmpty() ? QString() : QString::fromUtf8("✓");
        case Permissions:
            return permissionsText(rec->nodeStats.m_permissionFlags);
        case Direction:
            if (rec->nodeStats.nodeid < 0) return tr("Local");
            return rec->nodeStats.fInbound ? tr("Inbound") : tr("Outbound");
        case StartingBlock:
            return rec->nodeStats.nStartingHeight;
        case SyncedHeaders:
            return nodeStateHeight(rec->fNodeStateStatsAvailable, rec->nodeStateStats.nSyncHeight);
        case SyncedBlocks:
            return nodeStateHeight(rec->fNodeStateStatsAvailable, rec->nodeStateStats.nCommonHeight);
        case ConnectionTime:
            return durationSince(rec->nodeStats.nTimeConnected);
        case LastSend:
            return durationSince(rec->nodeStats.nLastSend);
        case LastReceive:
            return durationSince(rec->nodeStats.nLastRecv);
        case PingWait:
            return formatPingUsec(rec->nodeStats.m_ping_wait_usec);
        case MinPing:
            return formatPingUsec(rec->nodeStats.m_min_ping_usec);
        case TimeOffset:
            return GUIUtil::formatTimeOffset(rec->nodeStats.nTimeOffset);
        case MappedAS:
        {
            if (rec->nodeStats.m_mapped_as != 0) return QString::number(rec->nodeStats.m_mapped_as);
            const QString mapped_as = priv->mappedASForPeer(rec->nodeStats.addr);
            return mapped_as.isEmpty() ? tr("N/A") : mapped_as;
        }
        case ASName:
        {
            const QString as_name = priv->asNameForPeer(rec->nodeStats.addr);
            return as_name.isEmpty() ? tr("N/A") : as_name;
        }
        case ASHostingCompany:
        {
            const QString as_company = priv->asCompanyForPeer(rec->nodeStats.addr);
            return as_company.isEmpty() ? tr("N/A") : as_company;
        }
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
        case Fqdn:
        case CustomHostname:
        case CityState:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case Address:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case NetNodeId:
        case UniqueId:
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
    } else if (role == Qt::FontRole && index.column() == Address) {
        QFont font(QStringLiteral("Space Mono"));
        font.setStyleHint(QFont::Monospace);
        const int peer_table_size = QSettings().value(QStringLiteral("connectedPeersPanelFontSize"), QApplication::font().pointSize()).toInt();
        font.setPointSize(std::max(8, std::min(28, peer_table_size)));
        return font;
    } else if (role == Qt::BackgroundRole && rec->nodeStats.nodeid == -1) {
        const QColor base = QApplication::palette().color(QPalette::Base);
        return base.lightness() < 128 ? QColor(12, 19, 27) : QColor(232, 236, 241);
    } else if (role == Qt::ForegroundRole && !rec->fActive) {
        return QColor(128, 128, 128);
    } else if (role == Qt::ForegroundRole && index.column() == PingHealth) {
        const qint64 ping_ms = bestPingUsec(rec->nodeStats) / 1000;
        if (ping_ms <= 0) return QVariant();
        if (ping_ms < 50) return QColor(49, 209, 88);
        if (ping_ms < 150) return QColor(255, 159, 10);
        return QColor(255, 69, 58);
    } else if (role == Qt::ToolTipRole && index.column() == PingHealth) {
        return tr("Traffic Health shows recent ping samples for this peer. Heartbeat markers update on each refresh; lower values mean lower latency.");
    } else if (role == Qt::ToolTipRole && index.column() == UniqueId) {
        return tr("DC34 Edition session identifier. It reuses the same number for the same IP address, port, and node fingerprint when possible.");
    } else if (role == Qt::ToolTipRole && index.column() == Jitter) {
        return tr("Ping jitter is the average change between recent ping samples. Lower jitter means the connection is more consistent.");
    } else if (role == Qt::ToolTipRole && index.column() == Fqdn) {
        return tr("Reverse DNS for this peer. [NA: LAN] is shown when no local DNS name is available.");
    } else if (role == Qt::ToolTipRole && index.column() == CustomHostname) {
        return tr("Seed domain, custom host label, or LAN device name discovered through local naming tools.");
    } else if (role == Qt::ToolTipRole && index.column() == Seed) {
        const QString seed_domain = seedDomainForAddress(rec->nodeStats.addr);
        return seed_domain.isEmpty() ? tr("This peer is not one of the configured seed domains.") : tr("Configured seed domain: %1").arg(seed_domain);
    } else if (role == Qt::ToolTipRole && index.column() == CityState) {
        return tr("Best-effort city and state/region from Lookup Geo. Public peers use their IP address; local and LAN rows use this node's public WAN address.");
    } else if (role == Qt::ToolTipRole && index.column() == MappedAS) {
        return tr("Autonomous System Number from the node backend or optional DNS-based IP-to-ASN lookup. Lookups are cached and never sent for local or LAN addresses.");
    } else if (role == Qt::ToolTipRole && index.column() == ASName) {
        return tr("Autonomous System name from optional DNS-based IP-to-ASN lookup. Lookups are cached and never sent for local or LAN addresses.");
    } else if (role == Qt::ToolTipRole && index.column() == ASHostingCompany) {
        return tr("Best-effort hosting company extracted from the Autonomous System name. Lookups are cached and never sent for local or LAN addresses.");
    } else if (role == Qt::ToolTipRole && index.column() == Country) {
        if (priv->geoMarkerForPeer(rec->nodeStats.addr) == QLatin1String("local")) return tr("This row is the local Defcoin node.");
        if (priv->geoMarkerForPeer(rec->nodeStats.addr) == QLatin1String("lan")) return tr("This peer is on the local area network.");
        return tr("Country or region inferred from DNS-based IP-to-ASN data. Lookups are cached and throttled.");
    } else if (role == Qt::UserRole && index.column() == PingHealth) {
        return priv->pingHistoryForNode(rec->nodeStats.nodeid);
    } else if (role == Qt::UserRole && index.column() == Country) {
        return priv->geoMarkerForPeer(rec->nodeStats.addr);
    }

    return QVariant();
}

QVariant PeerTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole && section < columns.size())
        {
            return columns[section];
        }
        if (role == Qt::TextAlignmentRole) {
            return QVariant(Qt::AlignCenter | Qt::AlignVCenter);
        }
        if (role == Qt::ToolTipRole) {
            switch (section) {
            case UniqueId:
                return tr("DC34 Edition session identifier derived from endpoint plus node fingerprint. Node ID remains the backend connection ID.");
            case Address:
                return tr("Peer network endpoint. Direction is shown in its own column.");
            case Port:
                return tr("TCP service port used by this peer.");
            case Fqdn:
                return tr("Fully qualified domain name from reverse DNS, or LAN when no local DNS name is available.");
            case CustomHostname:
                return tr("Seed domain, custom host label, or LAN device name discovered through local naming tools.");
            case Services:
                return tr("Service flags: N=NETWORK, NL=NETWORK_LIMITED, G=GETUTXO, B=BLOOM, W=WITNESS, CF=COMPACT_FILTERS. MWEB service bits may appear on inherited Litecoin peers but are not Defcoin mainnet features.");
            case PingHealth:
                return tr("Traffic Health heartbeat. Lower values mean lower latency; color reflects current recent ping.");
            case Jitter:
                return tr("Average change between recent ping samples. Lower jitter means steadier latency.");
            case Country:
                return tr("Country or region inferred from DNS-based IP-to-ASN data. Lookups are cached and throttled.");
            case CityState:
                return tr("City and state/region lookup. Public peers use their IP address; local and LAN rows use this node's public WAN address.");
            case MappedAS:
                return tr("Autonomous System Number from the node backend or optional DNS-based IP-to-ASN lookup. This is distinct from the node service flags and is cached when looked up.");
            case ASName:
                return tr("Autonomous System name from optional DNS-based IP-to-ASN lookup. Lookups are cached and never sent for local or LAN addresses.");
            case ASHostingCompany:
                return tr("Best-effort hosting company extracted from the Autonomous System name. Lookups are cached and never sent for local or LAN addresses.");
            case UserAgentCount:
                return tr("Number of currently connected peers reporting this same user agent string.");
            case Seed:
                return tr("Shows a checkmark when the peer address matches a configured seed domain.");
            }
        }
    }

    return QVariant();
}

Qt::ItemFlags PeerTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    return retval;
}

QModelIndex PeerTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (column < 0 || column >= columnCount(QModelIndex())) return QModelIndex();
    CNodeCombinedStats *data = priv->index(row);

    if (data)
        return createIndex(row, column, data);
    return QModelIndex();
}

const CNodeCombinedStats *PeerTableModel::getNodeStats(int idx)
{
    return priv->index(idx);
}

void PeerTableModel::refresh()
{
    Q_EMIT layoutAboutToBeChanged();
    priv->refreshPeers(m_node);
    Q_EMIT layoutChanged();
    if (!priv->demo_peers_enabled) {
        schedulePeerLookups();
    }
}

int PeerTableModel::getRowByNodeId(NodeId nodeid)
{
    std::map<NodeId, int>::iterator it = priv->mapNodeRows.find(nodeid);
    if (it == priv->mapNodeRows.end())
        return -1;

    return it->second;
}

void PeerTableModel::sort(int column, Qt::SortOrder order)
{
    priv->sortColumn = column;
    priv->sortOrder = order;
    refresh();
}

void PeerTableModel::schedulePeerLookups()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const CNodeCombinedStats& stats : priv->cachedNodeStats) {
        const bool lan_peer = isLanPeer(stats.nodeStats.addr);
        const bool local_or_lan_peer = lan_peer || isLocalInterfaceAddress(stats.nodeStats.addr);
        const bool public_peer_lookup = shouldLookupPeer(stats.nodeStats.addr) && !lan_peer;
        const bool lookup_fqdn = public_peer_lookup || lan_peer;
        if (!lookup_fqdn) continue;

        const QString key = peerLookupKey(stats.nodeStats.addr);
        const QHostAddress address(key);
        if (address.isNull()) continue;

        PeerTablePriv::PeerLookup& lookup = priv->peer_lookup_cache[key];
        if (!lookup.fqdn_pending && !priv->queued_fqdn.contains(key) && now >= lookup.next_fqdn_lookup) {
            priv->queued_fqdn.insert(key);
            priv->fqdn_queue.enqueue(key);
        }
        if (lan_peer && !lookup.netbios_pending && !priv->queued_netbios.contains(key) && now >= lookup.next_netbios_lookup) {
            priv->queued_netbios.insert(key);
            priv->netbios_queue.enqueue(key);
        }
        const bool needs_cymru_lookup = priv->geolocation_enabled || priv->mapped_as_lookup_enabled;
        if (needs_cymru_lookup && public_peer_lookup && !lookup.country_pending && !priv->queued_country.contains(key) && now >= lookup.next_country_lookup && !cymruDnsNameForAddress(address).isEmpty()) {
            priv->queued_country.insert(key);
            priv->country_queue.enqueue(key);
        }
        if (priv->geolocation_enabled) {
            const QString location_key = local_or_lan_peer ? WAN_LOCATION_LOOKUP_KEY : key;
            PeerTablePriv::PeerLookup& location_lookup = priv->peer_lookup_cache[location_key];
            const bool should_location_lookup = public_peer_lookup || local_or_lan_peer;
            if (should_location_lookup && !location_lookup.location_pending && !location_lookup.location_populated && !priv->queued_location.contains(location_key) && now >= location_lookup.next_location_lookup) {
                priv->queued_location.insert(location_key);
                priv->location_queue.enqueue(location_key);
            }
        }
    }
    processPeerLookupQueues();
}

void PeerTableModel::scheduleLocationQueueRetry(qint64 delay_msecs)
{
    if (!priv || !priv->location_rate_timer) return;

    const int delay = static_cast<int>(std::max<qint64>(1000, std::min<qint64>(delay_msecs, IP_API_DEFAULT_RATE_RETRY_MSECS)));
    const int remaining = priv->location_rate_timer->remainingTime();
    if (!priv->location_rate_timer->isActive() || remaining < 0 || remaining > delay) {
        priv->location_rate_timer->start(delay);
    }
}

void PeerTableModel::processPeerLookupQueues()
{
    while (priv->active_fqdn_lookups < MAX_PARALLEL_LOOKUPS && !priv->fqdn_queue.isEmpty()) {
        const QString key = priv->fqdn_queue.dequeue();
        priv->queued_fqdn.remove(key);
        PeerTablePriv::PeerLookup& lookup = priv->peer_lookup_cache[key];
        if (lookup.fqdn_pending) continue;

        const QString ptr_name = reverseDnsNameForAddress(QHostAddress(key));
        if (ptr_name.isEmpty()) {
            lookup.next_fqdn_lookup = QDateTime::currentMSecsSinceEpoch() + DNS_LOOKUP_RETRY_MSECS;
            continue;
        }

        lookup.fqdn_pending = true;
        ++priv->active_fqdn_lookups;
        QDnsLookup* dns = new QDnsLookup(QDnsLookup::PTR, ptr_name, this);
        connect(dns, &QDnsLookup::finished, this, [this, dns, key] {
            PeerTablePriv::PeerLookup& lookup = priv->peer_lookup_cache[key];
            lookup.fqdn_pending = false;
            --priv->active_fqdn_lookups;

            if (dns->error() == QDnsLookup::NoError && !dns->pointerRecords().isEmpty()) {
                lookup.fqdn = normalizeDnsName(dns->pointerRecords().constFirst().value());
                lookup.next_fqdn_lookup = QDateTime::currentMSecsSinceEpoch() + DNS_LOOKUP_REFRESH_MSECS;
            } else {
                lookup.next_fqdn_lookup = QDateTime::currentMSecsSinceEpoch() + DNS_LOOKUP_RETRY_MSECS;
            }

            dns->deleteLater();
            notifyLookupChanged(key, Fqdn);
            notifyLookupChanged(key, CustomHostname);
            processPeerLookupQueues();
        });
        dns->lookup();
    }

    while (priv->active_netbios_lookups < MAX_PARALLEL_LOCATION_LOOKUPS && !priv->netbios_queue.isEmpty()) {
        const QString key = priv->netbios_queue.dequeue();
        priv->queued_netbios.remove(key);
        PeerTablePriv::PeerLookup& lookup = priv->peer_lookup_cache[key];
        if (lookup.netbios_pending) continue;

        lookup.netbios_pending = true;
        ++priv->active_netbios_lookups;
        QProcess* process = new QProcess(this);
#ifdef Q_OS_WIN
        process->setProgram(QStringLiteral("nbtstat.exe"));
        process->setArguments(QStringList{QStringLiteral("-A"), key});
#elif defined(Q_OS_MAC)
        process->setProgram(QStringLiteral("/usr/bin/smbutil"));
        process->setArguments(QStringList{QStringLiteral("status"), QStringLiteral("-ae"), key});
#else
        process->setProgram(QStringLiteral("nmblookup"));
        process->setArguments(QStringList{QStringLiteral("-A"), key});
#endif
        process->setProcessChannelMode(QProcess::MergedChannels);
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, process, key](int, QProcess::ExitStatus) {
            PeerTablePriv::PeerLookup& lookup = priv->peer_lookup_cache[key];
            lookup.netbios_pending = false;
            --priv->active_netbios_lookups;
            const QString output = QString::fromLocal8Bit(process->readAll()).trimmed();
            QString server_name;
            const QStringList output_lines = output.split(QRegExp(QStringLiteral("[\\r\\n]+")), QString::SkipEmptyParts);
            for (const QString& line : output_lines) {
                if (line.trimmed().startsWith(QStringLiteral("Server"), Qt::CaseInsensitive) && line.contains(QLatin1Char(':'))) {
                    server_name = line.section(QLatin1Char(':'), 1).trimmed();
                    break;
                }
                const QString simplified = line.simplified();
                if (simplified.contains(QStringLiteral("<00>"), Qt::CaseInsensitive) &&
                    simplified.contains(QStringLiteral("UNIQUE"), Qt::CaseInsensitive)) {
                    server_name = simplified.section(QLatin1Char(' '), 0, 0).trimmed();
                    break;
                }
            }
            if (!server_name.isEmpty()) {
                lookup.netbios_name = server_name;
                lookup.next_netbios_lookup = QDateTime::currentMSecsSinceEpoch() + DNS_LOOKUP_REFRESH_MSECS;
            } else {
                lookup.next_netbios_lookup = QDateTime::currentMSecsSinceEpoch() + DNS_LOOKUP_RETRY_MSECS;
            }
            process->deleteLater();
            notifyLookupChanged(key, CustomHostname);
            processPeerLookupQueues();
        });
        connect(process, &QProcess::errorOccurred, this, [this, process, key](QProcess::ProcessError) {
            PeerTablePriv::PeerLookup& lookup = priv->peer_lookup_cache[key];
            lookup.netbios_pending = false;
            --priv->active_netbios_lookups;
            lookup.next_netbios_lookup = QDateTime::currentMSecsSinceEpoch() + DNS_LOOKUP_RETRY_MSECS;
            process->deleteLater();
            notifyLookupChanged(key, CustomHostname);
            processPeerLookupQueues();
        });
        process->start();
    }

    while (priv->active_country_lookups < MAX_PARALLEL_LOOKUPS && !priv->country_queue.isEmpty()) {
        const QString key = priv->country_queue.dequeue();
        priv->queued_country.remove(key);
        PeerTablePriv::PeerLookup& lookup = priv->peer_lookup_cache[key];
        if (lookup.country_pending) continue;

        const QString cymru_name = cymruDnsNameForAddress(QHostAddress(key));
        if (cymru_name.isEmpty()) continue;

        lookup.country_pending = true;
        ++priv->active_country_lookups;
        QDnsLookup* dns = new QDnsLookup(QDnsLookup::TXT, cymru_name, this);
        connect(dns, &QDnsLookup::finished, this, [this, dns, key] {
            PeerTablePriv::PeerLookup& lookup = priv->peer_lookup_cache[key];
            lookup.country_pending = false;
            --priv->active_country_lookups;

            if (dns->error() == QDnsLookup::NoError && !dns->textRecords().isEmpty() && !dns->textRecords().constFirst().values().isEmpty()) {
                QByteArray txt_bytes;
                for (const QByteArray& value : dns->textRecords().constFirst().values()) {
                    txt_bytes += value;
                }
                const QString txt = QString::fromUtf8(txt_bytes);
                const QStringList parts = txt.split('|');
                if (priv->mapped_as_lookup_enabled && !parts.isEmpty()) {
                    const QString asn = parts.at(0).trimmed();
                    lookup.mapped_as = (asn.isEmpty() || asn == QLatin1String("0")) ? QString() : asn;
                }
                if (parts.size() >= 3) {
                    lookup.country_code = parts.at(2).trimmed().toUpper();
                }
                lookup.next_country_lookup = QDateTime::currentMSecsSinceEpoch() + DNS_LOOKUP_REFRESH_MSECS;
            } else {
                lookup.next_country_lookup = QDateTime::currentMSecsSinceEpoch() + DNS_LOOKUP_RETRY_MSECS;
            }

            const QString as_name_query = priv->mapped_as_lookup_enabled && lookup.as_name.isEmpty() && !lookup.mapped_as.isEmpty() && !lookup.as_name_pending && QDateTime::currentMSecsSinceEpoch() >= lookup.next_as_name_lookup
                ? cymruAsNameDnsName(lookup.mapped_as)
                : QString();
            if (!as_name_query.isEmpty()) {
                lookup.as_name_pending = true;
                QDnsLookup* as_dns = new QDnsLookup(QDnsLookup::TXT, as_name_query, this);
                connect(as_dns, &QDnsLookup::finished, this, [this, as_dns, key] {
                    PeerTablePriv::PeerLookup& lookup = priv->peer_lookup_cache[key];
                    lookup.as_name_pending = false;
                    if (as_dns->error() == QDnsLookup::NoError && !as_dns->textRecords().isEmpty() && !as_dns->textRecords().constFirst().values().isEmpty()) {
                        QByteArray txt_bytes;
                        for (const QByteArray& value : as_dns->textRecords().constFirst().values()) {
                            txt_bytes += value;
                        }
                        const QStringList parts = QString::fromUtf8(txt_bytes).split('|');
                        if (parts.size() >= 5) {
                            lookup.as_name = parts.at(4).trimmed();
                            lookup.as_company = hostingCompanyFromASName(lookup.as_name);
                            lookup.next_as_name_lookup = QDateTime::currentMSecsSinceEpoch() + DNS_LOOKUP_REFRESH_MSECS;
                        } else {
                            lookup.next_as_name_lookup = QDateTime::currentMSecsSinceEpoch() + DNS_LOOKUP_RETRY_MSECS;
                        }
                    } else {
                        lookup.next_as_name_lookup = QDateTime::currentMSecsSinceEpoch() + DNS_LOOKUP_RETRY_MSECS;
                    }
                    as_dns->deleteLater();
                    notifyLookupChanged(key, ASName);
                    notifyLookupChanged(key, ASHostingCompany);
                    processPeerLookupQueues();
                });
                as_dns->lookup();
            }

            dns->deleteLater();
            notifyLookupChanged(key, Country);
            notifyLookupChanged(key, MappedAS);
            notifyLookupChanged(key, ASName);
            notifyLookupChanged(key, ASHostingCompany);
            processPeerLookupQueues();
        });
        dns->lookup();
    }

    while (priv->geolocation_enabled && priv->active_location_lookups < MAX_PARALLEL_LOCATION_LOOKUPS && !priv->location_queue.isEmpty()) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        while (!priv->location_request_timestamps.isEmpty() &&
               priv->location_request_timestamps.head() <= now - IP_API_RATE_WINDOW_MSECS) {
            priv->location_request_timestamps.dequeue();
        }
        if (now < priv->location_rate_pause_until) {
            scheduleLocationQueueRetry(priv->location_rate_pause_until - now);
            break;
        }
        if (priv->location_request_timestamps.size() >= IP_API_MAX_LOCATION_REQUESTS_PER_MINUTE) {
            scheduleLocationQueueRetry(priv->location_request_timestamps.head() + IP_API_RATE_WINDOW_MSECS - now);
            break;
        }

        const QString key = priv->location_queue.dequeue();
        priv->queued_location.remove(key);
        PeerTablePriv::PeerLookup& lookup = priv->peer_lookup_cache[key];
        if (lookup.location_pending || lookup.location_populated || !priv->location_manager) continue;

        QUrl url;
        url.setScheme(QStringLiteral("http"));
        url.setHost(QStringLiteral("ip-api.com"));
        url.setPath(key == WAN_LOCATION_LOOKUP_KEY ? QStringLiteral("/json/") : QStringLiteral("/json/") + key);
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("fields"), QStringLiteral("status,message,countryCode,regionName,city,lat,lon"));
        url.setQuery(query);

        lookup.location_pending = true;
        ++priv->active_location_lookups;
        priv->location_request_timestamps.enqueue(now);
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Defcoin Core"));
        QNetworkReply* reply = priv->location_manager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, key] {
            PeerTablePriv::PeerLookup& lookup = priv->peer_lookup_cache[key];
            lookup.location_pending = false;
            --priv->active_location_lookups;

            bool rate_limited = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 429;
            bool ok = false;
            const int remaining = QString::fromLatin1(reply->rawHeader("X-Rl")).toInt(&ok);
            if (ok && remaining <= 0) rate_limited = true;
            ok = false;
            const int ttl_seconds = QString::fromLatin1(reply->rawHeader("X-Ttl")).toInt(&ok);
            const qint64 ttl_msecs = ok && ttl_seconds > 0 ? static_cast<qint64>(ttl_seconds) * 1000 : IP_API_DEFAULT_RATE_RETRY_MSECS;
            if (rate_limited) {
                priv->location_rate_pause_until = std::max(priv->location_rate_pause_until, QDateTime::currentMSecsSinceEpoch() + ttl_msecs);
            }

            if (reply->error() == QNetworkReply::NoError) {
                const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
                if (document.isObject()) {
                    const QJsonObject object = document.object();
                    const QString status = object.value(QStringLiteral("status")).toString();
                    const bool success = status.isEmpty() ? object.value(QStringLiteral("success")).toBool(true) : status == QLatin1String("success");
                    if (success) {
                        lookup.city = object.value(QStringLiteral("city")).toString().trimmed();
                        lookup.state = object.value(QStringLiteral("regionName")).toString().trimmed();
                        if (lookup.state.isEmpty()) lookup.state = object.value(QStringLiteral("region")).toString().trimmed();
                        QJsonValue latitude = object.value(QStringLiteral("lat"));
                        QJsonValue longitude = object.value(QStringLiteral("lon"));
                        if (!latitude.isDouble()) latitude = object.value(QStringLiteral("latitude"));
                        if (!longitude.isDouble()) longitude = object.value(QStringLiteral("longitude"));
                        lookup.latitude = latitude.toDouble();
                        lookup.longitude = longitude.toDouble();
                        lookup.coordinates_populated = latitude.isDouble() && longitude.isDouble();
                        if (lookup.country_code.isEmpty()) {
                            lookup.country_code = object.value(QStringLiteral("countryCode")).toString().trimmed().toUpper();
                            if (lookup.country_code.isEmpty()) {
                                lookup.country_code = object.value(QStringLiteral("country_code")).toString().trimmed().toUpper();
                            }
                        }
                        lookup.location_populated = !lookup.city.isEmpty() || !lookup.state.isEmpty();
                    }
                }
            }
            if (rate_limited && !lookup.location_populated) {
                lookup.next_location_lookup = priv->location_rate_pause_until;
                if (!priv->queued_location.contains(key)) {
                    priv->queued_location.insert(key);
                    priv->location_queue.enqueue(key);
                }
                scheduleLocationQueueRetry(std::max<qint64>(1000, priv->location_rate_pause_until - QDateTime::currentMSecsSinceEpoch()));
            } else if (!lookup.location_populated) {
                lookup.next_location_lookup = QDateTime::currentMSecsSinceEpoch() + DNS_LOOKUP_RETRY_MSECS;
            }

            reply->deleteLater();
            notifyLookupChanged(key, CityState);
            processPeerLookupQueues();
        });
    }
}

void PeerTableModel::notifyLookupChanged(const QString& peer, int column)
{
    for (int row = 0; row < priv->cachedNodeStats.size(); ++row) {
        const CAddress& address = priv->cachedNodeStats.at(row).nodeStats.addr;
        const bool wan_match = peer == WAN_LOCATION_LOOKUP_KEY && (isLocalInterfaceAddress(address) || isLanPeer(address));
        if (wan_match || peerLookupKey(address) == peer) {
            Q_EMIT dataChanged(index(row, column, QModelIndex()), index(row, column, QModelIndex()));
        }
    }
}
