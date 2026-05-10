// Copyright (c) 2026 The Defcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_DEFCOINDUMMYPEERS_H
#define BITCOIN_QT_DEFCOINDUMMYPEERS_H

#include <QString>
#include <QVector>

namespace DefcoinDemoPeers {

struct Peer
{
    const char* city;
    const char* ip;
    const char* data_center;
    double latitude;
    double longitude;
    int port;
    int ping_ms;
    quint64 sent;
    quint64 received;
};

inline const QVector<Peer>& peers()
{
    static const QVector<Peer> demo_peers{
        {"Ashburn, Virginia", "203.0.113.10", "Ashburn VA Internet Exchange", 39.0438, -77.4874, 1337, 18, 42000, 110000},
        {"Reston, Virginia", "203.0.113.11", "Reston VA Campus", 38.9586, -77.3570, 1337, 25, 49919, 122853},
        {"Manassas, Virginia", "203.0.113.12", "Manassas VA Campus", 38.7509, -77.4753, 10332, 32, 57838, 135706},
        {"Dallas, Texas", "203.0.113.13", "Dallas TX Data Corridor", 32.7767, -96.7970, 1337, 39, 65757, 148559},
        {"Houston, Texas", "203.0.113.14", "Houston TX Carrier Hotel", 29.7604, -95.3698, 10332, 46, 73676, 161412},
        {"Austin, Texas", "203.0.113.15", "Austin TX Cloud Region", 30.2672, -97.7431, 1337, 53, 81595, 174265},
        {"Chicago, Illinois", "203.0.113.16", "Chicago IL Midwest Hub", 41.8781, -87.6298, 1337, 60, 89514, 187118},
        {"Elk Grove Village, Illinois", "203.0.113.17", "Elk Grove Village IL Hub", 42.0039, -87.9703, 10332, 67, 97433, 199971},
        {"Santa Clara, California", "203.0.113.18", "Santa Clara CA Silicon Valley", 37.3541, -121.9552, 1337, 74, 105352, 212824},
        {"San Jose, California", "203.0.113.19", "San Jose CA Carrier Hub", 37.3382, -121.8863, 10332, 81, 113271, 225677},
        {"Los Angeles, California", "203.0.113.20", "Los Angeles CA Media Hub", 34.0522, -118.2437, 1337, 88, 121190, 238530},
        {"Seattle, Washington", "203.0.113.21", "Seattle WA Cloud Hub", 47.6062, -122.3321, 1337, 95, 129109, 251383},
        {"Portland, Oregon", "203.0.113.22", "Portland OR Edge Hub", 45.5152, -122.6784, 10332, 102, 137028, 264236},
        {"Phoenix, Arizona", "203.0.113.23", "Phoenix AZ Desert Campus", 33.4484, -112.0740, 1337, 109, 144947, 277089},
        {"Denver, Colorado", "203.0.113.24", "Denver CO Mountain Hub", 39.7392, -104.9903, 10332, 116, 152866, 289942},
        {"Salt Lake City, Utah", "203.0.113.25", "Salt Lake City UT Mountain Hub", 40.7608, -111.8910, 1337, 123, 160785, 302795},
        {"New York, New York", "203.0.113.26", "New York NY Metro Hub", 40.7128, -74.0060, 1337, 130, 168704, 315648},
        {"Secaucus, New Jersey", "203.0.113.27", "Secaucus NJ Financial Hub", 40.7895, -74.0565, 10332, 137, 176623, 328501},
        {"Atlanta, Georgia", "203.0.113.28", "Atlanta GA Southeast Hub", 33.7490, -84.3880, 1337, 144, 184542, 341354},
        {"Miami, Florida", "203.0.113.29", "Miami FL LatAm Gateway", 25.7617, -80.1918, 10332, 151, 192461, 354207},
        {"Boston, Massachusetts", "203.0.113.30", "Boston MA Research Hub", 42.3601, -71.0589, 1337, 158, 200380, 367060},
        {"Toronto, Ontario", "203.0.113.31", "Toronto ON Cloud Region", 43.6532, -79.3832, 1337, 165, 208299, 379913},
        {"Montreal, Quebec", "203.0.113.32", "Montreal QC Low-Carbon Hub", 45.5017, -73.5673, 10332, 172, 216218, 392766},
        {"Vancouver, British Columbia", "203.0.113.33", "Vancouver BC Pacific Hub", 49.2827, -123.1207, 1337, 179, 224137, 405619},
        {"Calgary, Alberta", "203.0.113.34", "Calgary AB Prairie Hub", 51.0447, -114.0719, 10332, 186, 232056, 418472},
        {"Mexico City, Mexico", "203.0.113.35", "Mexico City MX Cloud Region", 19.4326, -99.1332, 1337, 193, 239975, 431325},
        {"Queretaro, Mexico", "203.0.113.36", "Queretaro MX Hyperscale Campus", 20.5888, -100.3899, 1337, 200, 247894, 444178},
        {"Monterrey, Mexico", "203.0.113.37", "Monterrey MX Industrial Hub", 25.6866, -100.3161, 10332, 207, 45813, 457031},
        {"Sao Paulo, Brazil", "203.0.113.38", "Sao Paulo BR Cloud Region", -23.5505, -46.6333, 1337, 214, 53732, 469884},
        {"Rio de Janeiro, Brazil", "203.0.113.39", "Rio de Janeiro BR Edge Hub", -22.9068, -43.1729, 10332, 221, 61651, 482737},
        {"Fortaleza, Brazil", "203.0.113.40", "Fortaleza BR Cable Landing", -3.7319, -38.5267, 1337, 228, 69570, 495590},
        {"Santiago, Chile", "203.0.113.41", "Santiago CL Cloud Region", -33.4489, -70.6693, 1337, 235, 77489, 508443},
        {"Buenos Aires, Argentina", "203.0.113.42", "Buenos Aires AR Metro Hub", -34.6037, -58.3816, 10332, 242, 85408, 521296},
        {"Bogota, Colombia", "203.0.113.43", "Bogota CO Cloud Region", 4.7110, -74.0721, 1337, 19, 93327, 534149},
        {"Lima, Peru", "203.0.113.44", "Lima PE Metro Hub", -12.0464, -77.0428, 10332, 26, 101246, 547002},
        {"London, United Kingdom", "203.0.113.45", "London UK Docklands Hub", 51.5072, -0.1276, 1337, 33, 109165, 559855},
        {"Slough, United Kingdom", "203.0.113.46", "Slough UK Hyperscale Belt", 51.5105, -0.5950, 1337, 40, 117084, 572708},
        {"Manchester, United Kingdom", "203.0.113.47", "Manchester UK Northern Hub", 53.4808, -2.2426, 10332, 47, 125003, 585561},
        {"Dublin, Ireland", "203.0.113.48", "Dublin IE Cloud Region", 53.3498, -6.2603, 1337, 54, 132922, 598414},
        {"Paris, France", "203.0.113.49", "Paris FR Cloud Region", 48.8566, 2.3522, 10332, 61, 140841, 611267},
        {"Marseille, France", "203.0.113.50", "Marseille FR Cable Landing", 43.2965, 5.3698, 1337, 68, 148760, 624120},
        {"Frankfurt, Germany", "203.0.113.51", "Frankfurt DE Internet Exchange", 50.1109, 8.6821, 1337, 75, 156679, 116973},
        {"Berlin, Germany", "203.0.113.52", "Berlin DE Edge Hub", 52.5200, 13.4050, 10332, 82, 164598, 129826},
        {"Munich, Germany", "203.0.113.53", "Munich DE Enterprise Hub", 48.1351, 11.5820, 1337, 89, 172517, 142679},
        {"Amsterdam, Netherlands", "203.0.113.54", "Amsterdam NL Internet Exchange", 52.3676, 4.9041, 10332, 96, 180436, 155532},
        {"Rotterdam, Netherlands", "203.0.113.55", "Rotterdam NL Port Hub", 51.9244, 4.4777, 1337, 103, 188355, 168385},
        {"Brussels, Belgium", "203.0.113.56", "Brussels BE EU Hub", 50.8503, 4.3517, 1337, 110, 196274, 181238},
        {"Zurich, Switzerland", "203.0.113.57", "Zurich CH Financial Hub", 47.3769, 8.5417, 10332, 117, 204193, 194091},
        {"Geneva, Switzerland", "203.0.113.58", "Geneva CH International Hub", 46.2044, 6.1432, 1337, 124, 212112, 206944},
        {"Milan, Italy", "203.0.113.59", "Milan IT Cloud Region", 45.4642, 9.1900, 10332, 131, 220031, 219797},
        {"Rome, Italy", "203.0.113.60", "Rome IT Government Hub", 41.9028, 12.4964, 1337, 138, 227950, 232650},
        {"Madrid, Spain", "203.0.113.61", "Madrid ES Cloud Region", 40.4168, -3.7038, 1337, 145, 235869, 245503},
        {"Barcelona, Spain", "203.0.113.62", "Barcelona ES Mediterranean Hub", 41.3874, 2.1686, 10332, 152, 243788, 258356},
        {"Lisbon, Portugal", "203.0.113.63", "Lisbon PT Atlantic Hub", 38.7223, -9.1393, 1337, 159, 251707, 271209},
        {"Stockholm, Sweden", "203.0.113.64", "Stockholm SE Nordic Hub", 59.3293, 18.0686, 10332, 166, 49626, 284062},
        {"Oslo, Norway", "203.0.113.65", "Oslo NO Nordic Hub", 59.9139, 10.7522, 1337, 173, 57545, 296915},
        {"Copenhagen, Denmark", "203.0.113.66", "Copenhagen DK Nordic Hub", 55.6761, 12.5683, 1337, 180, 65464, 309768},
        {"Helsinki, Finland", "203.0.113.67", "Helsinki FI Nordic Hub", 60.1699, 24.9384, 10332, 187, 73383, 322621},
        {"Warsaw, Poland", "203.0.113.68", "Warsaw PL Central Europe", 52.2297, 21.0122, 1337, 194, 81302, 335474},
        {"Prague, Czechia", "203.0.113.69", "Prague CZ Central Europe", 50.0755, 14.4378, 10332, 201, 89221, 348327},
        {"Vienna, Austria", "203.0.113.70", "Vienna AT Central Europe", 48.2082, 16.3738, 1337, 208, 97140, 361180},
        {"Budapest, Hungary", "203.0.113.71", "Budapest HU Danube Hub", 47.4979, 19.0402, 1337, 215, 105059, 374033},
        {"Bucharest, Romania", "203.0.113.72", "Bucharest RO Balkan Hub", 44.4268, 26.1025, 10332, 222, 112978, 386886},
        {"Sofia, Bulgaria", "203.0.113.73", "Sofia BG Balkan Hub", 42.6977, 23.3219, 1337, 229, 120897, 399739},
        {"Athens, Greece", "203.0.113.74", "Athens GR Mediterranean Hub", 37.9838, 23.7275, 10332, 236, 128816, 412592},
        {"Istanbul, Turkiye", "203.0.113.75", "Istanbul TR Eurasia Hub", 41.0082, 28.9784, 1337, 243, 136735, 425445},
        {"Reykjavik, Iceland", "203.0.113.76", "Reykjavik IS Low-Carbon Hub", 64.1466, -21.9426, 1337, 20, 144654, 438298},
        {"Luxembourg City, Luxembourg", "203.0.113.77", "Luxembourg LU Financial Hub", 49.6116, 6.1319, 10332, 27, 152573, 451151},
        {"Singapore", "203.0.113.78", "Singapore SG Internet Exchange", 1.3521, 103.8198, 1337, 34, 160492, 464004},
        {"Johor Bahru, Malaysia", "203.0.113.79", "Johor Bahru MY Hyperscale Belt", 1.4927, 103.7414, 10332, 41, 168411, 476857},
        {"Kuala Lumpur, Malaysia", "203.0.113.80", "Kuala Lumpur MY Cloud Hub", 3.1390, 101.6869, 1337, 48, 176330, 489710},
        {"Jakarta, Indonesia", "203.0.113.81", "Jakarta ID Cloud Region", -6.2088, 106.8456, 1337, 55, 184249, 502563},
        {"Batam, Indonesia", "203.0.113.82", "Batam ID Cable Landing", 1.0456, 104.0305, 10332, 62, 192168, 515416},
        {"Bangkok, Thailand", "203.0.113.83", "Bangkok TH Regional Hub", 13.7563, 100.5018, 1337, 69, 200087, 528269},
        {"Ho Chi Minh City, Vietnam", "203.0.113.84", "Ho Chi Minh City VN Edge Hub", 10.8231, 106.6297, 10332, 76, 208006, 541122},
        {"Hanoi, Vietnam", "203.0.113.85", "Hanoi VN Government Hub", 21.0278, 105.8342, 1337, 83, 215925, 553975},
        {"Manila, Philippines", "203.0.113.86", "Manila PH Cloud Hub", 14.5995, 120.9842, 1337, 90, 223844, 566828},
        {"Hong Kong", "203.0.113.87", "Hong Kong HK Exchange Hub", 22.3193, 114.1694, 10332, 97, 231763, 579681},
        {"Shenzhen, China", "203.0.113.88", "Shenzhen CN Manufacturing Hub", 22.5431, 114.0579, 1337, 104, 239682, 592534},
        {"Shanghai, China", "203.0.113.89", "Shanghai CN Financial Hub", 31.2304, 121.4737, 10332, 111, 247601, 605387},
        {"Beijing, China", "203.0.113.90", "Beijing CN Cloud Region", 39.9042, 116.4074, 1337, 118, 45520, 618240},
        {"Taipei, Taiwan", "203.0.113.91", "Taipei TW Semiconductor Hub", 25.0330, 121.5654, 1337, 125, 53439, 111093},
        {"Tokyo, Japan", "203.0.113.92", "Tokyo JP Cloud Region", 35.6762, 139.6503, 10332, 132, 61358, 123946},
        {"Osaka, Japan", "203.0.113.93", "Osaka JP West Region", 34.6937, 135.5023, 1337, 139, 69277, 136799},
        {"Fukuoka, Japan", "203.0.113.94", "Fukuoka JP Edge Hub", 33.5902, 130.4017, 10332, 146, 77196, 149652},
        {"Seoul, South Korea", "203.0.113.95", "Seoul KR Cloud Region", 37.5665, 126.9780, 1337, 153, 85115, 162505},
        {"Busan, South Korea", "203.0.113.96", "Busan KR Cable Landing", 35.1796, 129.0756, 1337, 160, 93034, 175358},
        {"Mumbai, India", "203.0.113.97", "Mumbai IN Cloud Region", 19.0760, 72.8777, 10332, 167, 100953, 188211},
        {"Navi Mumbai, India", "203.0.113.98", "Navi Mumbai IN Hyperscale Campus", 19.0330, 73.0297, 1337, 174, 108872, 201064},
        {"Chennai, India", "203.0.113.99", "Chennai IN Cable Landing", 13.0827, 80.2707, 10332, 181, 116791, 213917},
        {"Bengaluru, India", "203.0.113.100", "Bengaluru IN Tech Hub", 12.9716, 77.5946, 1337, 188, 124710, 226770},
        {"Hyderabad, India", "203.0.113.101", "Hyderabad IN Cloud Region", 17.3850, 78.4867, 1337, 195, 132629, 239623},
        {"Delhi NCR, India", "203.0.113.102", "Delhi NCR IN Metro Hub", 28.6139, 77.2090, 10332, 202, 140548, 252476},
        {"Pune, India", "203.0.113.103", "Pune IN Enterprise Hub", 18.5204, 73.8567, 1337, 209, 148467, 265329},
        {"Dhaka, Bangladesh", "203.0.113.104", "Dhaka BD Metro Hub", 23.8103, 90.4125, 10332, 216, 156386, 278182},
        {"Colombo, Sri Lanka", "203.0.113.105", "Colombo LK Cable Landing", 6.9271, 79.8612, 1337, 223, 164305, 291035},
        {"Karachi, Pakistan", "203.0.113.106", "Karachi PK Cable Landing", 24.8607, 67.0011, 1337, 230, 172224, 303888},
        {"Dubai, United Arab Emirates", "203.0.113.107", "Dubai AE Cloud Region", 25.2048, 55.2708, 10332, 237, 180143, 316741},
        {"Abu Dhabi, United Arab Emirates", "203.0.113.108", "Abu Dhabi AE Government Hub", 24.4539, 54.3773, 1337, 244, 188062, 329594},
        {"Doha, Qatar", "203.0.113.109", "Doha QA Regional Hub", 25.2854, 51.5310, 10332, 21, 195981, 342447},
        {"Riyadh, Saudi Arabia", "198.51.100.10", "Riyadh SA Cloud Region", 24.7136, 46.6753, 1337, 28, 203900, 355300},
        {"Jeddah, Saudi Arabia", "198.51.100.11", "Jeddah SA Red Sea Hub", 21.4858, 39.1925, 1337, 35, 211819, 368153},
        {"Tel Aviv, Israel", "198.51.100.12", "Tel Aviv IL Cloud Region", 32.0853, 34.7818, 10332, 42, 219738, 381006},
        {"Amman, Jordan", "198.51.100.13", "Amman JO Regional Hub", 31.9539, 35.9106, 1337, 49, 227657, 393859},
        {"Manama, Bahrain", "198.51.100.14", "Manama BH Gulf Hub", 26.2235, 50.5876, 10332, 56, 235576, 406712},
        {"Muscat, Oman", "198.51.100.15", "Muscat OM Cable Landing", 23.5880, 58.3829, 1337, 63, 243495, 419565},
        {"Johannesburg, South Africa", "198.51.100.16", "Johannesburg ZA Cloud Region", -26.2041, 28.0473, 1337, 70, 251414, 432418},
        {"Cape Town, South Africa", "198.51.100.17", "Cape Town ZA Cable Landing", -33.9249, 18.4241, 10332, 77, 49333, 445271},
        {"Durban, South Africa", "198.51.100.18", "Durban ZA Edge Hub", -29.8587, 31.0218, 1337, 84, 57252, 458124},
        {"Nairobi, Kenya", "198.51.100.19", "Nairobi KE East Africa Hub", -1.2921, 36.8219, 10332, 91, 65171, 470977},
        {"Mombasa, Kenya", "198.51.100.20", "Mombasa KE Cable Landing", -4.0435, 39.6682, 1337, 98, 73090, 483830},
        {"Lagos, Nigeria", "198.51.100.21", "Lagos NG West Africa Hub", 6.5244, 3.3792, 1337, 105, 81009, 496683},
        {"Abuja, Nigeria", "198.51.100.22", "Abuja NG Government Hub", 9.0765, 7.3986, 10332, 112, 88928, 509536},
        {"Accra, Ghana", "198.51.100.23", "Accra GH Regional Hub", 5.6037, -0.1870, 1337, 119, 96847, 522389},
        {"Cairo, Egypt", "198.51.100.24", "Cairo EG North Africa Hub", 30.0444, 31.2357, 10332, 126, 104766, 535242},
        {"Alexandria, Egypt", "198.51.100.25", "Alexandria EG Cable Landing", 31.2001, 29.9187, 1337, 133, 112685, 548095},
        {"Casablanca, Morocco", "198.51.100.26", "Casablanca MA Regional Hub", 33.5731, -7.5898, 1337, 140, 120604, 560948},
        {"Tunis, Tunisia", "198.51.100.27", "Tunis TN Mediterranean Hub", 36.8065, 10.1815, 10332, 147, 128523, 573801},
        {"Addis Ababa, Ethiopia", "198.51.100.28", "Addis Ababa ET Regional Hub", 8.9806, 38.7578, 1337, 154, 136442, 586654},
        {"Kigali, Rwanda", "198.51.100.29", "Kigali RW Regional Hub", -1.9441, 30.0619, 10332, 161, 144361, 599507},
        {"Port Louis, Mauritius", "198.51.100.30", "Port Louis MU Island Hub", -20.1609, 57.5012, 1337, 168, 152280, 612360},
        {"Sydney, Australia", "198.51.100.31", "Sydney AU Cloud Region", -33.8688, 151.2093, 1337, 175, 160199, 625213},
        {"Melbourne, Australia", "198.51.100.32", "Melbourne AU Cloud Region", -37.8136, 144.9631, 10332, 182, 168118, 118066},
        {"Brisbane, Australia", "198.51.100.33", "Brisbane AU Edge Hub", -27.4698, 153.0251, 1337, 189, 176037, 130919},
        {"Perth, Australia", "198.51.100.34", "Perth AU Western Hub", -31.9523, 115.8613, 10332, 196, 183956, 143772},
        {"Auckland, New Zealand", "198.51.100.35", "Auckland NZ Cloud Hub", -36.8509, 174.7645, 1337, 203, 191875, 156625},
        {"Christchurch, New Zealand", "198.51.100.36", "Christchurch NZ Edge Hub", -43.5321, 172.6362, 1337, 210, 199794, 169478},
        {"Guam", "198.51.100.37", "Guam Pacific Cable Hub", 13.4443, 144.7937, 10332, 217, 207713, 182331},
        {"Honolulu, Hawaii", "198.51.100.38", "Honolulu HI Pacific Hub", 21.3069, -157.8583, 1337, 224, 215632, 195184},
        {"Anchorage, Alaska", "198.51.100.39", "Anchorage AK Arctic Hub", 61.2181, -149.9003, 10332, 231, 223551, 208037},
        {"Moscow, Russia", "198.51.100.40", "Moscow RU Metro Hub", 55.7558, 37.6173, 1337, 238, 231470, 220890},
        {"Saint Petersburg, Russia", "198.51.100.41", "Saint Petersburg RU Baltic Hub", 59.9311, 30.3609, 1337, 245, 239389, 233743},
        {"Novosibirsk, Russia", "198.51.100.42", "Novosibirsk RU Siberian Hub", 55.0084, 82.9357, 10332, 22, 247308, 246596},
        {"Kyiv, Ukraine", "198.51.100.43", "Kyiv UA Regional Hub", 50.4501, 30.5234, 1337, 29, 45227, 259449},
        {"Tallinn, Estonia", "198.51.100.44", "Tallinn EE Digital Hub", 59.4370, 24.7536, 10332, 36, 53146, 272302},
        {"Riga, Latvia", "198.51.100.45", "Riga LV Baltic Hub", 56.9496, 24.1052, 1337, 43, 61065, 285155},
        {"Vilnius, Lithuania", "198.51.100.46", "Vilnius LT Baltic Hub", 54.6872, 25.2797, 1337, 50, 68984, 298008},
        {"Belgrade, Serbia", "198.51.100.47", "Belgrade RS Balkan Hub", 44.7866, 20.4489, 10332, 57, 76903, 310861},
        {"Zagreb, Croatia", "198.51.100.48", "Zagreb HR Adriatic Hub", 45.8150, 15.9819, 1337, 64, 84822, 323714},
        {"Ljubljana, Slovenia", "198.51.100.49", "Ljubljana SI Alpine Hub", 46.0569, 14.5058, 10332, 71, 92741, 336567},
        {"Bratislava, Slovakia", "198.51.100.50", "Bratislava SK Central Hub", 48.1486, 17.1077, 1337, 78, 100660, 349420},
        {"Nicosia, Cyprus", "198.51.100.51", "Nicosia CY Island Hub", 35.1856, 33.3823, 1337, 85, 108579, 362273},
        {"Panama City, Panama", "198.51.100.52", "Panama City PA Cable Hub", 8.9824, -79.5199, 10332, 92, 116498, 375126},
        {"San Jose, Costa Rica", "198.51.100.53", "San Jose CR Regional Hub", 9.9281, -84.0907, 1337, 99, 124417, 387979},
        {"Quito, Ecuador", "198.51.100.54", "Quito EC Regional Hub", -0.1807, -78.4678, 10332, 106, 132336, 400832},
        {"Montevideo, Uruguay", "198.51.100.55", "Montevideo UY South Cone Hub", -34.9011, -56.1645, 1337, 113, 140255, 413685},
        {"Asuncion, Paraguay", "198.51.100.56", "Asuncion PY Regional Hub", -25.2637, -57.5759, 1337, 120, 148174, 426538},
        {"La Paz, Bolivia", "198.51.100.57", "La Paz BO Regional Hub", -16.4897, -68.1193, 10332, 127, 156093, 439391},
        {"San Juan, Puerto Rico", "198.51.100.58", "San Juan PR Caribbean Hub", 18.4655, -66.1057, 1337, 134, 164012, 452244},
        {"Reykjanesbaer, Iceland", "198.51.100.59", "Reykjanesbaer IS Geothermal Campus", 63.9998, -22.5583, 10332, 141, 171931, 465097},
        {"Lulea, Sweden", "192.0.2.10", "Lulea SE Low-Carbon Data Center Campus", 65.5848, 22.1547, 1337, 149, 179850, 477950},
        {"Gavle, Sweden", "192.0.2.11", "Gavle SE Baltic Fiber Hub", 60.6749, 17.1413, 10332, 156, 187769, 490803},
        {"Hamina, Finland", "192.0.2.12", "Hamina FI Nordic Data Center Campus", 60.5697, 27.1979, 1337, 163, 195688, 503656},
        {"Oulu, Finland", "192.0.2.13", "Oulu FI Edge Data Center Market", 65.0121, 25.4651, 10332, 170, 203607, 516509},
        {"Eemshaven, Netherlands", "192.0.2.14", "Eemshaven NL Hyperscale Data Center Campus", 53.4422, 6.8327, 1337, 177, 211526, 529362},
        {"Groningen, Netherlands", "192.0.2.15", "Groningen NL Regional Fiber Hub", 53.2194, 6.5665, 1337, 184, 219445, 542215},
        {"Mons, Belgium", "192.0.2.16", "Mons BE Data Center Campus", 50.4542, 3.9523, 10332, 191, 227364, 555068},
        {"Odense, Denmark", "192.0.2.17", "Odense DK Renewable Data Center Campus", 55.4038, 10.4024, 1337, 198, 235283, 567921},
        {"Stavanger, Norway", "192.0.2.18", "Stavanger NO Cable Landing Hub", 58.9700, 5.7331, 10332, 205, 243202, 580774},
        {"Bergen, Norway", "192.0.2.19", "Bergen NO Regional Data Center Market", 60.3913, 5.3221, 1337, 212, 251121, 593627},
        {"Newcastle, United Kingdom", "192.0.2.20", "Newcastle UK Northern Data Center Market", 54.9783, -1.6178, 1337, 219, 259040, 606480},
        {"Cardiff, United Kingdom", "192.0.2.21", "Cardiff UK Regional Data Center Market", 51.4816, -3.1791, 10332, 226, 266959, 619333},
        {"Leeds, United Kingdom", "192.0.2.22", "Leeds UK Internet Exchange Market", 53.8008, -1.5491, 1337, 233, 274878, 632186},
        {"Lyon, France", "192.0.2.23", "Lyon FR Regional Data Center Market", 45.7640, 4.8357, 10332, 240, 282797, 645039},
        {"Toulouse, France", "192.0.2.24", "Toulouse FR Edge Data Center Market", 43.6047, 1.4442, 1337, 247, 290716, 657892},
        {"Hamburg, Germany", "192.0.2.25", "Hamburg DE Carrier Hotel Market", 53.5511, 9.9937, 1337, 24, 298635, 670745},
        {"Dusseldorf, Germany", "192.0.2.26", "Dusseldorf DE Rhine-Ruhr Data Center Market", 51.2277, 6.7735, 10332, 31, 306554, 683598},
        {"Nuremberg, Germany", "192.0.2.27", "Nuremberg DE Regional Data Center Market", 49.4521, 11.0767, 1337, 38, 314473, 696451},
        {"Valencia, Spain", "192.0.2.28", "Valencia ES Mediterranean Data Center Hub", 39.4699, -0.3763, 10332, 45, 322392, 709304},
        {"Bilbao, Spain", "192.0.2.29", "Bilbao ES Atlantic Cable Hub", 43.2630, -2.9350, 1337, 52, 330311, 722157},
        {"Turin, Italy", "192.0.2.30", "Turin IT Northern Data Center Market", 45.0703, 7.6869, 1337, 59, 338230, 735010},
        {"Naples, Italy", "192.0.2.31", "Naples IT Mediterranean Data Center Market", 40.8518, 14.2681, 10332, 66, 346149, 747863},
        {"Krakow, Poland", "192.0.2.32", "Krakow PL Regional Data Center Market", 50.0647, 19.9450, 1337, 73, 354068, 760716},
        {"Wroclaw, Poland", "192.0.2.33", "Wroclaw PL Carrier-Neutral Data Center Market", 51.1079, 17.0385, 10332, 80, 361987, 773569},
        {"Brno, Czechia", "192.0.2.34", "Brno CZ Regional Data Center Market", 49.1951, 16.6068, 1337, 87, 369906, 786422},
        {"Cluj-Napoca, Romania", "192.0.2.35", "Cluj-Napoca RO Regional Fiber Hub", 46.7712, 23.6236, 1337, 94, 377825, 799275},
        {"Izmir, Turkiye", "192.0.2.36", "Izmir TR Aegean Cable Hub", 38.4237, 27.1428, 10332, 101, 385744, 812128},
        {"Ankara, Turkiye", "192.0.2.37", "Ankara TR Government Data Center Market", 39.9334, 32.8597, 1337, 108, 393663, 824981},
        {"Haifa, Israel", "192.0.2.38", "Haifa IL Submarine Cable Landing Hub", 32.7940, 34.9896, 10332, 115, 401582, 837834},
        {"Kuwait City, Kuwait", "192.0.2.39", "Kuwait City KW Gulf Data Center Market", 29.3759, 47.9774, 1337, 122, 409501, 850687},
        {"Kolkata, India", "192.0.2.40", "Kolkata IN Eastern Data Center Market", 22.5726, 88.3639, 1337, 129, 417420, 863540},
        {"Ahmedabad, India", "192.0.2.41", "Ahmedabad IN Regional Data Center Market", 23.0225, 72.5714, 10332, 136, 425339, 876393},
        {"Surabaya, Indonesia", "192.0.2.42", "Surabaya ID East Java Data Center Market", -7.2575, 112.7521, 1337, 143, 433258, 889246},
        {"Bandung, Indonesia", "192.0.2.43", "Bandung ID Regional Data Center Market", -6.9175, 107.6191, 10332, 150, 441177, 902099},
        {"Chiang Mai, Thailand", "192.0.2.44", "Chiang Mai TH Northern Edge Hub", 18.7883, 98.9853, 1337, 157, 449096, 914952},
        {"Da Nang, Vietnam", "192.0.2.45", "Da Nang VN Cable Landing Hub", 16.0544, 108.2022, 1337, 164, 457015, 927805},
        {"Cebu, Philippines", "192.0.2.46", "Cebu PH Regional Data Center Market", 10.3157, 123.8854, 10332, 171, 464934, 940658},
        {"Canberra, Australia", "192.0.2.47", "Canberra AU Government Data Center Market", -35.2809, 149.1300, 1337, 178, 472853, 953511},
        {"Adelaide, Australia", "192.0.2.48", "Adelaide AU Regional Data Center Market", -34.9285, 138.6007, 10332, 185, 480772, 966364},
        {"Wellington, New Zealand", "192.0.2.49", "Wellington NZ Government Data Center Market", -41.2865, 174.7762, 1337, 192, 488691, 979217},
        {"Quebec City, Quebec", "192.0.2.50", "Quebec City CA Low-Temperature Data Center Market", 46.8139, -71.2080, 1337, 199, 496610, 992070},
        {"Ottawa, Ontario", "192.0.2.51", "Ottawa CA Government Data Center Market", 45.4215, -75.6972, 10332, 206, 504529, 1004923},
        {"Las Vegas, Nevada", "192.0.2.52", "Las Vegas NV Data Center Campus", 36.1716, -115.1391, 1337, 213, 512448, 1017776},
        {"Minneapolis, Minnesota", "192.0.2.53", "Minneapolis MN Upper Midwest Data Center Market", 44.9778, -93.2650, 10332, 220, 520367, 1030629},
        {"Kansas City, Missouri", "192.0.2.54", "Kansas City MO Central Data Center Market", 39.0997, -94.5786, 1337, 227, 528286, 1043482},
        {"Columbus, Ohio", "192.0.2.55", "Columbus OH Midwest Data Center Market", 39.9612, -82.9988, 1337, 234, 536205, 1056335},
        {"Raleigh, North Carolina", "192.0.2.56", "Raleigh NC Research Triangle Data Center Market", 35.7796, -78.6382, 10332, 241, 544124, 1069188},
        {"Nashville, Tennessee", "192.0.2.57", "Nashville TN Regional Data Center Market", 36.1627, -86.7816, 1337, 248, 552043, 1082041},
        {"Tampa, Florida", "192.0.2.58", "Tampa FL Gulf Coast Data Center Market", 27.9506, -82.4572, 10332, 25, 559962, 1094894},
        {"San Diego, California", "192.0.2.59", "San Diego CA Edge Data Center Market", 32.7157, -117.1611, 1337, 32, 567881, 1107747},
    };
    return demo_peers;
}

inline QString cityState(const Peer& peer)
{
    return QString::fromUtf8(peer.city);
}

inline QString ip(const Peer& peer)
{
    return QString::fromLatin1(peer.ip);
}

inline QString dataCenter(const Peer& peer)
{
    return QString::fromUtf8(peer.data_center);
}

inline QString fqdn(const Peer& peer)
{
    return dataCenter(peer);
}

} // namespace DefcoinDemoPeers

#endif // BITCOIN_QT_DEFCOINDUMMYPEERS_H
