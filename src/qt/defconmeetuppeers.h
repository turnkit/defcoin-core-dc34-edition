// Copyright (c) 2026 The Defcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_DEFCONMEETUPPEERS_H
#define BITCOIN_QT_DEFCONMEETUPPEERS_H

#include <QString>
#include <QVector>

namespace DefconMeetupPeers {

struct Meetup
{
    const char* group;
    const char* city_state;
    const char* website;
    double latitude;
    double longitude;
};

inline const QVector<Meetup>& meetups()
{
    static const QVector<Meetup> meetup_peers{
        {"DC214", "Dallas, Texas", "https://dc214.org", 32.7767, -96.7970},
        {"DC213", "Los Angeles, California", "https://dc213.org", 34.0522, -118.2437},
        {"DC404", "Atlanta, Georgia", "https://dc404.org", 33.7490, -84.3880},
        {"DC702", "Las Vegas, Nevada", "https://dc702.space", 36.1699, -115.1398},
        {"DC408", "San Jose, California", "https://dc408.com", 37.3382, -121.8863},
        {"DC801", "Salt Lake City, Utah", "https://dc801.org", 40.7608, -111.8910},
        {"DC612", "Minneapolis, Minnesota", "https://dc612.org", 44.9778, -93.2650},
        {"DC206", "Seattle, Washington", "https://dc206.org", 47.6062, -122.3321},
        {"DC207", "Portland, Maine", "https://dc207.org", 43.6591, -70.2568},
        {"DC303", "Denver, Colorado", "https://dc303.org", 39.7392, -104.9903},
        {"DC720", "Denver, Colorado", "https://dc720.org", 39.7392, -104.9903},
        {"DC719", "Colorado Springs, Colorado", "https://dc719.org", 38.8339, -104.8214},
        {"DC201", "Jersey City, New Jersey", "https://dc201.org", 40.7178, -74.0431},
        {"DC215", "Philadelphia, Pennsylvania", "https://dc215.org", 39.9526, -75.1652},
        {"DC716", "Buffalo, New York", "https://dc716.org", 42.8864, -78.8784},
        {"DC858", "San Diego, California", "https://dc858.org", 32.7157, -117.1611},
        {"DC949", "Irvine, California", "https://dc949.org", 33.6846, -117.8265},
        {"DC919", "Raleigh, North Carolina", "https://dc919.org", 35.7796, -78.6382},
        {"DC704", "Charlotte, North Carolina", "https://defcon704.org", 35.2271, -80.8431},
        {"DC713", "Houston, Texas", "https://dc713.net", 29.7604, -95.3698},
        {"DC4420", "London, United Kingdom", "https://dc4420.org", 51.5072, -0.1276},
        {"DC151", "Stuttgart, Germany", "https://dc151.org", 48.7758, 9.1829},
        {"DC33", "Paris, France", "https://dc33.org", 48.8566, 2.3522},
        {"DC34", "Madrid, Spain", "https://dc34.org", 40.4168, -3.7038},
        {"DC9723", "Tel Aviv, Israel", "https://dc9723.org", 32.0853, 34.7818},
    };
    return meetup_peers;
}

inline QString groupName(const Meetup& meetup)
{
    return QString::fromLatin1(meetup.group);
}

inline QString cityState(const Meetup& meetup)
{
    return QString::fromUtf8(meetup.city_state);
}

inline QString website(const Meetup& meetup)
{
    return QString::fromLatin1(meetup.website);
}

} // namespace DefconMeetupPeers

#endif // BITCOIN_QT_DEFCONMEETUPPEERS_H
