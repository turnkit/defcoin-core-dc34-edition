// Copyright (c) 2011-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/guiutil.h>

#include <qt/bitcoinaddressvalidator.h>
#include <qt/bitcoinunits.h>
#include <qt/qvalidatedlineedit.h>
#include <qt/sendcoinsrecipient.h>

#include <chainparams.h>
#include <clientversion.h>
#include <interfaces/node.h>
#include <key_io.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <protocol.h>
#include <script/script.h>
#include <script/standard.h>
#include <util/system.h>

#ifdef WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#endif

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDoubleValidator>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QImage>
#include <QLineEdit>
#include <QList>
#include <QMap>
#include <QMenu>
#include <QMouseEvent>
#include <QPalette>
#include <QPainter>
#include <QPainterPath>
#include <QProgressDialog>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QSize>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QTextDocument> // for Qt::mightBeRichText
#include <QThread>
#include <QUrl>
#include <QUrlQuery>
#include <QtGlobal>

#include <algorithm>

#if defined(Q_OS_MAC)

#include <QProcess>

void ForceActivation();
#endif

namespace GUIUtil {

QString dateTimeStr(const QDateTime &date)
{
    return date.date().toString(Qt::SystemLocaleShortDate) + QString(" ") + date.toString("hh:mm");
}

QString dateTimeStr(qint64 nTime)
{
    return dateTimeStr(QDateTime::fromTime_t((qint32)nTime));
}

QFont fixedPitchFont()
{
    return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

namespace {
struct ThemeEntry
{
    GUIUtil::ThemeDescriptor descriptor;
    QMap<QString, QString> assets;
    QMap<QString, QString> colors;
    QMap<QString, int> assetPriority;
    QMap<QString, int> assetRank;
    bool hidden{false};
};

QMap<QString, ThemeEntry> g_theme_catalog;
bool g_theme_catalog_loaded{false};
bool g_theme_catalog_loading{false};

QStringList fixedThemeIds()
{
    return QStringList{
        QStringLiteral("auto"),
        QStringLiteral("light"),
        QStringLiteral("dark"),
#if ENABLE_DEFCOIN_FUN_UI
        QStringLiteral("modern"),
        QStringLiteral("34")
#endif
    };
}

QStringList themeAssetExtensions()
{
#ifdef Q_OS_WIN
    return QStringList{
        QStringLiteral("png"),
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("webp"),
        QStringLiteral("ico"),
        QStringLiteral("svg")
    };
#else
    return QStringList{
        QStringLiteral("svg"),
        QStringLiteral("png"),
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("webp"),
        QStringLiteral("ico")
    };
#endif
}

int extensionPriority(const QString& suffix)
{
    const QString ext = suffix.toLower();
    const QStringList exts = themeAssetExtensions();
    const int index = exts.indexOf(ext);
    return index < 0 ? 100 : index;
}

QString stripNumberedPrefix(const QString& base)
{
    const QStringList parts = base.split(QStringLiteral("_"));
    if (parts.size() >= 3) {
        bool ok = false;
        parts.at(1).toInt(&ok);
        if (ok && (parts.at(0) == QStringLiteral("button") || parts.at(0) == QStringLiteral("menu"))) {
            return parts.mid(2).join(QStringLiteral("_")).toLower();
        }
    }
    return base.toLower();
}

QString normalizedThemeAssetKey(const QFileInfo& file_info)
{
    return stripNumberedPrefix(file_info.completeBaseName());
}

void insertThemeAsset(ThemeEntry& entry, const QFileInfo& file_info, int rank)
{
    const QString suffix = file_info.suffix().toLower();
    if (!themeAssetExtensions().contains(suffix)) return;

    const QString key = normalizedThemeAssetKey(file_info);
    if (key.isEmpty()) return;

    const int priority = extensionPriority(suffix);
    const bool has_existing = entry.assets.contains(key);
    if (!has_existing ||
        rank > entry.assetRank.value(key, -1) ||
        (rank == entry.assetRank.value(key, -1) && priority < entry.assetPriority.value(key, 100))) {
        entry.assets.insert(key, file_info.absoluteFilePath());
        entry.assetPriority.insert(key, priority);
        entry.assetRank.insert(key, rank);
    }
}

QJsonObject readThemeJson(const QString& theme_dir)
{
    QFile file(QDir(theme_dir).filePath(QStringLiteral("theme.json")));
    if (!file.open(QIODevice::ReadOnly)) return QJsonObject();

    QJsonParseError parse_error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !doc.isObject()) return QJsonObject();
    return doc.object();
}

QString jsonString(const QJsonObject& object, const QString& key, const QString& fallback = QString())
{
    const QJsonValue value = object.value(key);
    return value.isString() ? value.toString() : fallback;
}

GUIUtil::ThemeDescriptor fixedThemeDescriptor(const QString& id)
{
    GUIUtil::ThemeDescriptor descriptor;
    descriptor.id = id;
    descriptor.baseTheme = id;
    descriptor.creator = QStringLiteral("Defcoin Core developers");
    descriptor.created = QStringLiteral("2026-04-30");

    if (id == QStringLiteral("light")) {
        descriptor.name = QStringLiteral("Light");
        descriptor.shortName = QStringLiteral("Light");
        descriptor.description = QStringLiteral("Light Defcoin Core interface theme.");
        descriptor.tooltip = QStringLiteral("Theme: Light. Click to switch to Dark.");
    } else if (id == QStringLiteral("dark")) {
        descriptor.name = QStringLiteral("Dark");
        descriptor.shortName = QStringLiteral("Dark");
        descriptor.description = QStringLiteral("Dark Defcoin Core interface theme.");
        descriptor.tooltip = QStringLiteral("Theme: Dark. Click to switch to Modern.");
    } else if (id == QStringLiteral("modern")) {
        descriptor.name = QStringLiteral("Modern");
        descriptor.shortName = QStringLiteral("Modern");
        descriptor.description = QStringLiteral("Modern dark Defcoin Core interface theme.");
        descriptor.tooltip = QStringLiteral("Theme: Modern. Click to switch to 34.");
    } else if (id == QStringLiteral("34")) {
        descriptor.name = QStringLiteral("DC34");
        descriptor.shortName = QStringLiteral("34");
        descriptor.description = QStringLiteral("DC34 Edition Defcoin Core theme assets.");
        descriptor.tooltip = QStringLiteral("Theme: DC34. Click to switch to Auto.");
    } else {
        descriptor.id = QStringLiteral("auto");
        descriptor.baseTheme = QStringLiteral("auto");
        descriptor.name = QStringLiteral("Auto (system)");
        descriptor.shortName = QStringLiteral("Auto");
        descriptor.description = QStringLiteral("Follows the system appearance where Qt supports it.");
        descriptor.tooltip = QStringLiteral("Theme: Auto. Click to switch to Light.");
    }
    return descriptor;
}

void mergeThemeMetadata(ThemeEntry& entry, const QString& id, const QString& folder_name, const QJsonObject& metadata, bool custom)
{
    entry.descriptor.id = id;
    entry.descriptor.name = jsonString(metadata, QStringLiteral("name"), entry.descriptor.name.isEmpty() ? folder_name : entry.descriptor.name);
    entry.descriptor.shortName = jsonString(metadata, QStringLiteral("shortName"), entry.descriptor.shortName.isEmpty() ? entry.descriptor.name : entry.descriptor.shortName);
    entry.descriptor.description = jsonString(metadata, QStringLiteral("description"), entry.descriptor.description);
    entry.descriptor.creator = jsonString(metadata, QStringLiteral("creator"), entry.descriptor.creator);
    entry.descriptor.created = jsonString(metadata, QStringLiteral("created"), entry.descriptor.created);
    entry.descriptor.baseTheme = jsonString(metadata, QStringLiteral("baseTheme"), entry.descriptor.baseTheme.isEmpty() ? QStringLiteral("auto") : entry.descriptor.baseTheme).toLower();
    entry.descriptor.tooltip = jsonString(metadata, QStringLiteral("tooltip"), entry.descriptor.tooltip);
    entry.descriptor.custom = entry.descriptor.custom || custom;
    entry.hidden = metadata.value(QStringLiteral("hidden")).toBool(entry.hidden);

    const QJsonObject colors = metadata.value(QStringLiteral("colors")).toObject();
    for (auto it = colors.constBegin(); it != colors.constEnd(); ++it) {
        if (it.value().isString()) {
            entry.colors.insert(it.key(), it.value().toString());
        }
    }
}

QStringList builtinThemeRootCandidates()
{
    QStringList roots;
    const QString app_dir = QCoreApplication::applicationDirPath();
    roots << QDir(app_dir).absoluteFilePath(QStringLiteral("../Resources/themes"));
    roots << QDir(app_dir).absoluteFilePath(QStringLiteral("themes"));
    roots << QDir(app_dir).absoluteFilePath(QStringLiteral("../themes"));
    roots << QDir(app_dir).absoluteFilePath(QStringLiteral("res/themes"));
    roots << QDir(app_dir).absoluteFilePath(QStringLiteral("../../../src/qt/res/themes"));
    roots << QDir::current().absoluteFilePath(QStringLiteral("src/qt/res/themes"));
    roots << QDir::current().absoluteFilePath(QStringLiteral("qt/res/themes"));
    roots.removeDuplicates();
    return roots;
}

void writeTextFileIfMissing(const QString& path, const QString& text)
{
    if (QFileInfo::exists(path)) return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    file.write(text.toUtf8());
}

void ensureThemeTemplateFolderTree(const QString& example_dir)
{
    const QStringList menu_dirs{
        QStringLiteral("menu_00_app"),
        QStringLiteral("menu_01_overview"),
        QStringLiteral("menu_02_send"),
        QStringLiteral("menu_03_receive"),
        QStringLiteral("menu_04_transactions"),
        QStringLiteral("menu_05_node"),
        QStringLiteral("menu_90_status_bar"),
    };

    QDir().mkpath(example_dir);
    for (const QString& menu_dir : menu_dirs) {
        QDir().mkpath(QDir(example_dir).filePath(menu_dir));
    }
}

QString customThemesRoot()
{
    return QDir(boostPathToQString(GetDataDir(false))).filePath(QStringLiteral("custom_themes"));
}

void ensureCustomThemeTemplate()
{
    const QString custom_root = customThemesRoot();
    QDir().mkpath(custom_root);

    const QString example_dir = QDir(custom_root).filePath(QStringLiteral("example_theme"));
    ensureThemeTemplateFolderTree(example_dir);
    writeTextFileIfMissing(QDir(example_dir).filePath(QStringLiteral("DISABLED")), QString());
    writeTextFileIfMissing(QDir(example_dir).filePath(QStringLiteral("theme.json")),
        QStringLiteral("{\n"
                       "  \"id\": \"example_theme\",\n"
                       "  \"name\": \"Example Theme\",\n"
                       "  \"shortName\": \"Example\",\n"
                       "  \"description\": \"Disabled starter theme for local Defcoin Core theme customization.\",\n"
                       "  \"creator\": \"Local user\",\n"
                       "  \"created\": \"\",\n"
                       "  \"baseTheme\": \"default\",\n"
                       "  \"tooltip\": \"Theme: Example Theme. Click to switch theme.\",\n"
                       "  \"colors\": {\n"
                       "    \"status_icon_sky_blue\": \"#8fcaff\",\n"
                       "    \"checkbox_checked_blue\": \"#1f6fd0\",\n"
                       "    \"checkbox_checked_border\": \"#d7ebff\",\n"
                       "    \"check_mark_green\": \"#46e889\",\n"
                       "    \"disabled_slash_red\": \"#ff4d66\"\n"
                       "  }\n"
                       "}\n"));
    writeTextFileIfMissing(QDir(custom_root).filePath(QStringLiteral("README.txt")),
        QStringLiteral("Defcoin Core custom themes\n"
                       "==========================\n\n"
                       "Custom themes are loaded from this folder. Each direct child folder is treated as one theme.\n"
                       "A theme folder is enabled unless it contains a file or folder named DISABLED.\n\n"
                       "The example_theme folder is a generated modder template. Copy or rename it, edit the images and theme.json metadata,\n"
                       "then remove DISABLED to make it appear in the theme menu.\n"));
    writeTextFileIfMissing(QDir(example_dir).filePath(QStringLiteral("README.txt")),
        QStringLiteral("Defcoin Core example theme\n"
                       "==========================\n\n"
                       "This folder is a custom theme starter. It mirrors the built-in default theme folder layout without copying the default graphics into this folder.\n\n"
                       "Add replacement files using names such as:\n"
                       "- menu_00_app/button_01_settings.svg\n"
                       "- menu_00_app/button_02_theme.svg\n"
                       "- menu_00_app/splash_screen.png\n"
                       "- menu_00_app/about_logo.png\n"
                       "- menu_05_node/button_01_node.svg\n\n"
                       "Any missing graphic falls back to the built-in default theme.\n\n"
                       "To enable this theme, delete or rename the DISABLED file, e.g. change to ENABLED.\n"
                       "To disable it again, create an empty file, or an empty folder, named DISABLED in this folder.\n"));
}

void registerFixedThemes()
{
    for (const QString& id : fixedThemeIds()) {
        ThemeEntry& entry = g_theme_catalog[id];
        if (entry.descriptor.id.isEmpty()) {
            entry.descriptor = fixedThemeDescriptor(id);
        }
    }
}

void scanThemeRoot(const QString& root, bool custom, int rank)
{
    QDir root_dir(root);
    if (!root_dir.exists()) return;

    const QFileInfoList theme_dirs = root_dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& theme_dir_info : theme_dirs) {
        QDir theme_dir(theme_dir_info.absoluteFilePath());
        if (custom && QFileInfo::exists(theme_dir.filePath(QStringLiteral("DISABLED")))) {
            continue;
        }

        const QJsonObject metadata = readThemeJson(theme_dir_info.absoluteFilePath());
        const QString folder_name = theme_dir_info.fileName();
        const QString id = jsonString(metadata, QStringLiteral("id"), folder_name).toLower();
        if (id.isEmpty()) continue;

        ThemeEntry& entry = g_theme_catalog[id];
        if (entry.descriptor.id.isEmpty() && fixedThemeIds().contains(id)) {
            entry.descriptor = fixedThemeDescriptor(id);
        }
        mergeThemeMetadata(entry, id, folder_name, metadata, custom);

        QDirIterator it(theme_dir_info.absoluteFilePath(), QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QFileInfo file_info = it.fileInfo();
            if (file_info.isSymLink()) {
                continue;
            }
            if (file_info.fileName() == QStringLiteral("theme.json") ||
                file_info.fileName().compare(QStringLiteral("README.txt"), Qt::CaseInsensitive) == 0 ||
                file_info.fileName() == QStringLiteral("DISABLED")) {
                continue;
            }
            insertThemeAsset(entry, file_info, rank);
        }
    }
}

const QMap<QString, ThemeEntry>& themeCatalog()
{
    if (!g_theme_catalog_loaded && !g_theme_catalog_loading) {
        GUIUtil::refreshAppearanceThemeCatalog();
    }
    return g_theme_catalog;
}
} // namespace

void refreshAppearanceThemeCatalog()
{
    if (g_theme_catalog_loading) return;
    g_theme_catalog_loading = true;

    g_theme_catalog.clear();
    registerFixedThemes();

    for (const QString& root : builtinThemeRootCandidates()) {
        scanThemeRoot(root, false, 1);
    }

    ensureCustomThemeTemplate();
    scanThemeRoot(customThemesRoot(), true, 2);

    g_theme_catalog_loaded = true;
    g_theme_catalog_loading = false;
}

QVector<ThemeDescriptor> availableAppearanceThemes()
{
#if !ENABLE_DEFCOIN_FUN_UI
    return {
        appearanceThemeDescriptor(QStringLiteral("light")),
        appearanceThemeDescriptor(QStringLiteral("dark")),
        appearanceThemeDescriptor(QStringLiteral("auto"))
    };
#else
    const QMap<QString, ThemeEntry>& catalog = themeCatalog();
    QVector<ThemeDescriptor> themes;
    for (const QString& id : fixedThemeIds()) {
        const ThemeEntry entry = catalog.value(id);
        if (!entry.descriptor.id.isEmpty() && !entry.hidden) themes.push_back(entry.descriptor);
    }
    for (auto it = catalog.constBegin(); it != catalog.constEnd(); ++it) {
        if (fixedThemeIds().contains(it.key()) || it.key() == QStringLiteral("default") || it.value().hidden) continue;
        themes.push_back(it.value().descriptor);
    }
    return themes;
#endif
}

ThemeDescriptor appearanceThemeDescriptor(const QString& theme)
{
    const QString id = theme.toLower();
    const QMap<QString, ThemeEntry>& catalog = themeCatalog();
    if (catalog.contains(id) && !catalog.value(id).descriptor.id.isEmpty()) {
        return catalog.value(id).descriptor;
    }
    return fixedThemeDescriptor(QStringLiteral("auto"));
}

QString nextAppearanceTheme(const QString& theme)
{
    const QVector<ThemeDescriptor> themes = availableAppearanceThemes();
    if (themes.isEmpty()) return QStringLiteral("auto");

    const QString id = normalizeAppearanceTheme(theme);
    for (int i = 0; i < themes.size(); ++i) {
        if (themes.at(i).id == id) {
            return themes.at((i + 1) % themes.size()).id;
        }
    }
    return themes.first().id;
}

QString normalizeAppearanceTheme(const QString& theme)
{
#if !ENABLE_DEFCOIN_FUN_UI
    const QString normalized = theme.toLower();
    if (normalized == QStringLiteral("light") || normalized == QStringLiteral("dark") || normalized == QStringLiteral("auto")) {
        return normalized;
    }
    return QStringLiteral("auto");
#else
    const QString normalized = theme.toLower();
    const QMap<QString, ThemeEntry>& catalog = themeCatalog();
    if (catalog.contains(normalized) && !catalog.value(normalized).hidden && normalized != QStringLiteral("default")) {
        return normalized;
    }
    return QStringLiteral("auto");
#endif
}

QString getConfiguredAppearanceTheme()
{
    QSettings settings;
#if ENABLE_DEFCOIN_FUN_UI
    return normalizeAppearanceTheme(settings.value(QStringLiteral("appearanceTheme"), QStringLiteral("34")).toString());
#else
    return normalizeAppearanceTheme(settings.value(QStringLiteral("appearanceTheme"), QStringLiteral("auto")).toString());
#endif
}

QString appearanceThemeBaseStyle(const QString& theme)
{
    const ThemeDescriptor descriptor = appearanceThemeDescriptor(normalizeAppearanceTheme(theme));
    const QString base = descriptor.baseTheme.toLower();
    if (base == QStringLiteral("light") ||
        base == QStringLiteral("dark") ||
        base == QStringLiteral("modern") ||
        base == QStringLiteral("34") ||
        base == QStringLiteral("yam")) {
        return base;
    }
    return QStringLiteral("auto");
}

QString appearanceThemeToolTip(const QString& theme)
{
    const QString normalized = normalizeAppearanceTheme(theme);
    const ThemeDescriptor descriptor = appearanceThemeDescriptor(normalized);
    const QString next = appearanceThemeDescriptor(nextAppearanceTheme(normalized)).name;
    if (!descriptor.tooltip.isEmpty()) {
        return descriptor.tooltip;
    }
    return QObject::tr("Theme: %1. Click to switch to %2.").arg(descriptor.name, next);
}

QString resolveThemeAsset(const QString& asset_name, const QString& theme)
{
#if !ENABLE_DEFCOIN_FUN_UI
    Q_UNUSED(asset_name);
    Q_UNUSED(theme);
    return QString();
#else
    const QString key = stripNumberedPrefix(asset_name).toLower();
    const QString normalized_theme = normalizeAppearanceTheme(theme.isEmpty() ? getConfiguredAppearanceTheme() : theme);
    const QMap<QString, ThemeEntry>& catalog = themeCatalog();

    const ThemeEntry selected = catalog.value(normalized_theme);
    if (selected.assets.contains(key)) return selected.assets.value(key);

    const ThemeEntry fallback = catalog.value(QStringLiteral("default"));
    if (fallback.assets.contains(key)) return fallback.assets.value(key);

    return QString();
#endif
}

QIcon themeAssetIcon(const QString& asset_name, const QString& fallback_resource, const QString& theme)
{
    const QString path = resolveThemeAsset(asset_name, theme);
    if (!path.isEmpty()) return QIcon(path);
    return fallback_resource.isEmpty() ? QIcon() : QIcon(fallback_resource);
}

QPixmap themeAssetPixmap(const QString& asset_name, const QString& fallback_resource, const QString& theme)
{
    const QString path = resolveThemeAsset(asset_name, theme);
    QPixmap pixmap;
    if (!path.isEmpty()) pixmap.load(path);
    if (pixmap.isNull() && !fallback_resource.isEmpty()) pixmap.load(fallback_resource);
    return pixmap;
}

static void setPaletteColors(QPalette& palette,
                             const QColor& window,
                             const QColor& window_text,
                             const QColor& base,
                             const QColor& alternate_base,
                             const QColor& text,
                             const QColor& button,
                             const QColor& button_text,
                             const QColor& highlight,
                             const QColor& highlighted_text)
{
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, window_text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, alternate_base);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, button_text);
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::HighlightedText, highlighted_text);
    palette.setColor(QPalette::ToolTipBase, base);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Link, highlight);
    palette.setColor(QPalette::BrightText, QColor(255, 92, 92));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, window_text.darker(130));
    palette.setColor(QPalette::Disabled, QPalette::Text, text.darker(135));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, button_text.darker(135));
}

static QString themeColorToken(const QString& theme, const QString& key, const QString& fallback)
{
    const QMap<QString, ThemeEntry>& catalog = themeCatalog();
    const QString normalized = GUIUtil::normalizeAppearanceTheme(theme);
    const ThemeEntry selected = catalog.value(normalized);
    if (selected.colors.contains(key)) return selected.colors.value(key);

    const ThemeEntry base = catalog.value(GUIUtil::appearanceThemeBaseStyle(normalized));
    if (base.colors.contains(key)) return base.colors.value(key);

    const ThemeEntry default_theme = catalog.value(QStringLiteral("default"));
    if (default_theme.colors.contains(key)) return default_theme.colors.value(key);

    return fallback;
}

static QString checkboxCheckedIndicatorImageUrl(const QString& theme,
                                                const QString& check_color,
                                                const QString& checked_background,
                                                const QString& checked_border)
{
    const QColor color(check_color);
    const QString safe_color = color.isValid() ? color.name(QColor::HexRgb) : QStringLiteral("#46e889");
    const QColor fill_color(checked_background);
    const QColor border_color(checked_border);
    const QString safe_fill = fill_color.isValid() ? fill_color.name(QColor::HexRgb) : QStringLiteral("#2f7ed8");
    const QString safe_border = border_color.isValid() ? border_color.name(QColor::HexRgb) : QStringLiteral("#1f5f99");
    const QString safe_theme = theme.isEmpty() ? QStringLiteral("auto") : theme;

    QString cache_root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cache_root.isEmpty()) {
        cache_root = QDir::temp().filePath(QStringLiteral("defcoin-qt-cache"));
    }
    const QString dir_path = QDir(cache_root).filePath(QStringLiteral("theme-checkmarks"));
    QDir().mkpath(dir_path);

    QString file_key = safe_theme;
    for (int i = 0; i < file_key.size(); ++i) {
        const QChar ch = file_key.at(i);
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('_') && ch != QLatin1Char('-')) {
            file_key[i] = QLatin1Char('_');
        }
    }
    const QString file_path = QDir(dir_path).filePath(QStringLiteral("checkbox-checked-%1-%2-%3-%4.png")
        .arg(file_key, safe_fill.mid(1), safe_border.mid(1), safe_color.mid(1)));

    QImage checkmark(15, 15, QImage::Format_ARGB32_Premultiplied);
    checkmark.fill(Qt::transparent);
    {
        QPainter painter(&checkmark);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QPen pen(QColor(safe_color), 2.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        QPainterPath path;
        path.moveTo(2.5, 7.9);
        path.lineTo(6.0, 11.4);
        path.lineTo(12.8, 3.8);
        painter.drawPath(path);
    }
    checkmark.save(file_path, "PNG");

    return QUrl::fromLocalFile(file_path).toString();
}

static QString checkboxIndicatorStyle(const QString& border,
                                      const QString& disabled_border,
                                      const QString& checked_background,
                                      const QString& checked_border,
                                      const QString& checkmark_url)
{
    return QStringLiteral(
        "QCheckBox { spacing: 8px; margin-right: 8px; padding-right: 2px; padding-top: 1px; padding-bottom: 1px; }"
        "QCheckBox::indicator { width: 17px; height: 17px; border: 1px solid %1; background-color: transparent; border-radius: 3px; margin: 0px; padding: 0px; }"
        "QCheckBox::indicator:unchecked { image: none; border-image: none; background-color: transparent; }"
        "QCheckBox::indicator:unchecked:disabled { image: none; border-image: none; background-color: transparent; border-color: %2; }"
        "QCheckBox::indicator:checked { border: 1px solid %4; background-color: %3; image: url(\"%5\"); }"
        "QCheckBox::indicator:checked:disabled { border: 1px solid %4; background-color: %3; image: url(\"%5\"); }")
        .arg(border, disabled_border, checked_background, checked_border, checkmark_url);
}

static QString spinBoxControlStyle(const QString& border,
                                   const QString& button_background,
                                   const QString& button_hover)
{
    return QStringLiteral(
        "QSpinBox, QDoubleSpinBox { padding-right: 8px; }"
        "QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
        " subcontrol-origin: border; width: 18px; background-color: %2; border-left: 1px solid %1; border-right: 1px solid %1;"
        " margin: 0px; padding: 0px; }"
        "QSpinBox::up-button, QDoubleSpinBox::up-button { subcontrol-position: top right; border-top: 1px solid %1; border-bottom: 1px solid %1; }"
        "QSpinBox::down-button, QDoubleSpinBox::down-button { subcontrol-position: bottom right; border-bottom: 1px solid %1; }"
        "QSpinBox::up-button:hover, QSpinBox::down-button:hover, QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover { background-color: %3; }")
        .arg(border, button_background, button_hover);
}

static QString defaultButtonStyle(const QString& background,
                                  const QString& foreground,
                                  const QString& border)
{
    return QStringLiteral(
        "QPushButton:default { background-color: %1; color: %2; border: 1px solid %3; border-radius: 5px; padding: 5px 12px; min-height: 24px; }"
        "QPushButton:focus { outline: none; }")
        .arg(background, foreground, border);
}

void applyAppearanceTheme(const QString& theme)
{
    QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!app) return;

    const QString normalized = normalizeAppearanceTheme(theme);
    QString style_theme = appearanceThemeBaseStyle(normalized);
    const QString theme_check_mark = themeColorToken(normalized, QStringLiteral("check_mark_green"), QStringLiteral("#46e889"));
    const auto themedCheckboxStyle = [&](const QString& border,
                                         const QString& disabled_border,
                                         const QString& checked_background_fallback,
                                         const QString& checked_border_fallback) {
        const QString checked_background = themeColorToken(normalized, QStringLiteral("checkbox_checked_blue"), checked_background_fallback);
        const QString checked_border = themeColorToken(normalized, QStringLiteral("checkbox_checked_border"), checked_border_fallback);
        return checkboxIndicatorStyle(border, disabled_border, checked_background, checked_border, checkboxCheckedIndicatorImageUrl(normalized, theme_check_mark, checked_background, checked_border));
    };

    app->setStyleSheet(QString());
    app->setPalette(app->style()->standardPalette());

#if !ENABLE_DEFCOIN_FUN_UI
    if (style_theme == QStringLiteral("auto")) {
        const QColor system_window = app->palette().color(QPalette::Window);
        style_theme = system_window.lightness() < 128 ? QStringLiteral("dark") : QStringLiteral("light");
    }
#else
    if (style_theme == QStringLiteral("auto")) {
        app->setStyleSheet(QStringLiteral(
            "QToolBar { spacing: 4px; padding-right: 10px; }"
            "QToolBar QToolButton { color: #d7ecff; border: 1px solid transparent; border-radius: 5px; padding: 4px 7px; margin: 2px; min-height: 30px; }"
            "QToolBar QToolButton:hover { border-color: #8fbce8; background-color: rgba(143,188,232,42); }"
            "QToolBar QToolButton:checked { background-color: #1f6fd0; border-color: #b8dcff; color: #ffffff; }"
            "QToolBar QToolButton#toolbarIconButton { min-width: 48px; max-width: 48px; min-height: 48px; max-height: 48px; padding: 0; margin: 2px 4px; }"
            "QToolBar QToolButton#toolbarIconButton::menu-indicator { image: none; width: 0px; height: 0px; }"
            "QComboBox::down-arrow { image: url(:/icons/dropdown_arrow); width: 10px; height: 7px; }"
            "QStatusBar QLabel#statusPlainIcon, QStatusBar QLabel#statusPlainReadout { background-color: transparent; color: #eaf6ff; border: none; padding: 0px 4px; }"
            "QStatusBar QLabel#networkStatusReadout { background-color: #0f1a27; color: #eaf6ff; border: 1px solid #9dccff; border-radius: 4px; padding: 4px 10px; min-height: 24px; }"
            "QStatusBar QLabel#statusUnitControl, QStatusBar QLabel#statusIconControl { background-color: #0f1a27; color: #eaf6ff; border: 1px solid #9dccff; border-radius: 4px; padding: 4px 7px; min-width: 44px; min-height: 24px; }"
            "QStatusBar QLabel#statusUnitControl:hover, QStatusBar QLabel#statusIconControl:hover { background-color: #20344d; border-color: #ffffff; color: #ffffff; }")
            + spinBoxControlStyle(QStringLiteral("#8fcaff"), QStringLiteral("#0f1a27"), QStringLiteral("#20344d"))
            + defaultButtonStyle(QStringLiteral("#1f6fd0"), QStringLiteral("#ffffff"), QStringLiteral("#b8dcff"))
            + themedCheckboxStyle(QStringLiteral("#8fcaff"), QStringLiteral("#6f8498"), QStringLiteral("#1f6fd0"), QStringLiteral("#d7ebff")));
        return;
    }
#endif

    if (style_theme == QStringLiteral("light")) {
        QPalette palette;
        setPaletteColors(palette,
            QColor(246, 248, 250), QColor(26, 32, 42),
            QColor(255, 255, 255), QColor(239, 243, 247),
            QColor(26, 32, 42), QColor(246, 248, 250),
            QColor(26, 32, 42), QColor(31, 95, 153),
            QColor(255, 255, 255));
        app->setPalette(palette);
        app->setStyleSheet(QStringLiteral(
            "QToolBar { background-color: #eef3f8; border-bottom: 1px solid #b8c5d3; spacing: 4px; padding-right: 10px; }"
            "QToolBar QToolButton { background-color: transparent; color: #142235; border: 1px solid transparent; border-radius: 5px; padding: 4px 7px; margin: 2px; min-height: 30px; }"
            "QToolBar QToolButton:hover { border-color: #315f91; background-color: #dce9f7; }"
            "QToolBar QToolButton:checked { background-color: #2f7ed8; border-color: #1f5f99; color: #ffffff; }"
            "QToolBar QToolButton#toolbarIconButton { min-width: 48px; max-width: 48px; min-height: 48px; max-height: 48px; padding: 0; margin: 2px 4px; }"
            "QToolBar QToolButton#toolbarIconButton::menu-indicator { image: none; width: 0px; height: 0px; }"
            "QTableView, QTreeView, QTextEdit, QPlainTextEdit, QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QAbstractSpinBox { background-color: #ffffff; color: #1a202a; border: 1px solid #9eb0c2; border-radius: 3px; min-height: 22px; }QTableView { gridline-color: #aebccc; }"
            "QComboBox { padding: 2px 8px; }QComboBox::drop-down { border-left: 1px solid #8da2b8; width: 22px; }QComboBox::down-arrow { image: url(:/icons/dropdown_arrow); width: 10px; height: 7px; }"
            "QComboBox QAbstractItemView { background-color: #ffffff; color: #1a202a; selection-background-color: #2f7ed8; selection-color: #ffffff; }"
            "QHeaderView::section { background-color: #e9eef4; color: #1a202a; border: 1px solid #aebccc; padding: 4px; }"
            "QPushButton, QToolButton { background-color: #f4f7fb; color: #1a202a; border: 1px solid #8da2b8; border-radius: 5px; padding: 5px 10px; }"
            "QPushButton:hover, QToolButton:hover { border-color: #315f91; background-color: #e5f0fb; }"
            "QPushButton:pressed, QToolButton:pressed { background-color: #d7e6f7; }"
            "QPushButton:disabled, QToolButton:disabled { background-color: #edf1f5; color: #7a8694; border-color: #c1ccd7; }"
            "QRadioButton::indicator { width: 14px; height: 14px; border: 1px solid #697d91; background-color: #ffffff; border-radius: 7px; }QRadioButton::indicator:checked { background-color: #2f7ed8; border: 3px solid #ffffff; outline: 1px solid #1f5f99; }"
            "QStatusBar { background-color: #eef3f8; border-top: 1px solid #b8c5d3; }"
            "QStatusBar QLabel#statusPlainIcon, QStatusBar QLabel#statusPlainReadout { background-color: transparent; color: #172335; border: none; padding: 0px 4px; }"
            "QStatusBar QLabel#networkStatusReadout { background-color: #f8fbff; color: #172335; border: 1px solid #315f91; border-radius: 4px; padding: 4px 10px; min-height: 24px; }"
            "QStatusBar QLabel#statusUnitControl, QStatusBar QLabel#statusIconControl { background-color: #f8fbff; color: #172335; border: 1px solid #315f91; border-radius: 4px; padding: 4px 7px; min-width: 44px; min-height: 24px; font-weight: 700; }"
            "QStatusBar QLabel#statusUnitControl:hover, QStatusBar QLabel#statusIconControl:hover { background-color: #e5f0fb; border-color: #173f68; color: #000000; }")
            + spinBoxControlStyle(QStringLiteral("#9eb0c2"), QStringLiteral("#f4f7fb"), QStringLiteral("#e5f0fb"))
            + defaultButtonStyle(QStringLiteral("#2f7ed8"), QStringLiteral("#ffffff"), QStringLiteral("#1f5f99"))
            + themedCheckboxStyle(QStringLiteral("#697d91"), QStringLiteral("#9aa8b6"), QStringLiteral("#2f7ed8"), QStringLiteral("#1f5f99")));
        return;
    }

#if ENABLE_DEFCOIN_FUN_UI
    if (style_theme == QStringLiteral("34")) {
        QPalette palette;
        setPaletteColors(palette,
            QColor(0, 0, 0), QColor(238, 238, 238),
            QColor(5, 6, 6), QColor(17, 17, 17),
            QColor(238, 238, 238), QColor(17, 17, 17),
            QColor(238, 238, 238), QColor(102, 204, 238),
            QColor(0, 0, 0));
        app->setPalette(palette);
        app->setStyleSheet(QStringLiteral(
            "QWidget { selection-background-color: #66ccee; selection-color: #000000; }"
            "QMainWindow, QDialog { background-color: #000000; color: #eeeeee; }"
            "QWidget#RPCConsole, QWidget#tab_info, QWidget#tab_console, QWidget#tab_nettraffic, QWidget#tab_peers, QWidget#tab_peermap { background-color: #000000; color: #eeeeee; }"
            "QWidget#RPCConsole QTabWidget::pane, QDialog#OptionsDialog QTabWidget::pane { background-color: #000000; border: 1px solid #2b5a60; }"
            "QWidget#RPCConsole QTabWidget QWidget, QDialog#OptionsDialog QTabWidget QWidget { background-color: #000000; color: #eeeeee; }"
            "QWidget#RPCConsole QLabel, QWidget#RPCConsole QCheckBox, QWidget#RPCConsole QGroupBox, QDialog#OptionsDialog QLabel, QDialog#OptionsDialog QCheckBox, QDialog#OptionsDialog QGroupBox { color: #eeeeee; }"
            "QDialog#OptionsDialog QWidget#tabMain, QDialog#OptionsDialog QWidget#tabWallet, QDialog#OptionsDialog QWidget#tabNetwork, QDialog#OptionsDialog QWidget#tabWindow, QDialog#OptionsDialog QWidget#tabDisplay, QDialog#OptionsDialog QFrame#frame { background-color: #000000; color: #eeeeee; }"
            "QDialog#OptionsDialog QGroupBox#disabledLitecoinFeatures { background-color: #050a0d; color: #eeeeee; border-color: #66ccee; }"
            "QDialog#OptionsDialog QGroupBox#disabledLitecoinFeatures::title { color: #eeeeee; }"
            "QDialog#OptionsDialog QLabel#disabledFeatureNote, QDialog#OptionsDialog QLabel#disabledFeatureItem { background-color: transparent; color: #eeeeee; padding-left: 0px; }"
            "QMenuBar { background-color: #000000; color: #eeeeee; border-bottom: 1px solid #2b5a60; }"
            "QMenuBar::item { color: #eeeeee; background: transparent; padding: 4px 8px; }"
            "QMenuBar::item:selected { background-color: #0b1b22; color: #ffffff; }"
            "QMenu, QToolTip { background-color: #090b0c; color: #eeeeee; border: 1px solid #377ba4; }"
            "QMenu::item { color: #eeeeee; background-color: transparent; padding: 5px 24px 5px 24px; }"
            "QMenu::item:selected { background-color: #132b35; color: #ffffff; }"
            "QMenu::item:disabled { color: #7f8b92; background-color: transparent; }"
            "QMenu::separator { height: 1px; background: #2b5a60; margin: 4px 8px; }"
            "QToolBar { background-color: #000000; border-bottom: 1px solid #2b5a60; spacing: 4px; padding-right: 10px; }"
            "QToolBar QToolButton { background-color: transparent; color: #eeeeee; border: 1px solid transparent; border-radius: 5px; padding: 4px 7px; margin: 2px; min-height: 30px; }"
            "QToolBar QToolButton:hover { border-color: #66ccee; background-color: #0b1b22; }"
            "QToolBar QToolButton:checked { background-color: #377ba4; border-color: #66ccee; color: #ffffff; }"
            "QToolBar QToolButton#toolbarIconButton { min-width: 48px; max-width: 48px; min-height: 48px; max-height: 48px; padding: 0; margin: 2px 4px; }"
            "QTabWidget::pane, QGroupBox { border: 1px solid #2b5a60; border-radius: 8px; }"
            "QDialog#OptionsDialog QGroupBox { margin-top: 22px; padding-top: 16px; border-color: #66ccee; }"
            "QDialog#OptionsDialog QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; top: 2px; padding: 0 6px; background-color: #000000; color: #eeeeee; }"
            "QTableView, QTreeView, QTextEdit, QPlainTextEdit, QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QAbstractSpinBox { background-color: #050606; color: #eeeeee; border: 1px solid #377ba4; border-radius: 4px; min-height: 26px; }QTableView { gridline-color: #2b5a60; }"
            "QComboBox { padding: 4px 8px; }QComboBox::drop-down { border-left: 1px solid #377ba4; width: 24px; }QComboBox::down-arrow { image: url(:/icons/dropdown_arrow); width: 10px; height: 7px; }"
            "QComboBox QAbstractItemView { background-color: #050606; color: #eeeeee; selection-background-color: #377ba4; selection-color: #ffffff; }"
            "QHeaderView::section { background-color: #111111; color: #eeeeee; border: 1px solid #2b5a60; padding: 4px; }"
            "QTabBar::tab { background-color: #101214; color: #eeeeee; border: 1px solid #2b5a60; padding: 5px 16px; min-height: 24px; min-width: 96px; }"
            "QTabBar::tab:!selected { background-color: #101214; color: #eeeeee; }"
            "QTabBar::tab:selected { background-color: #377ba4; color: #ffffff; border-color: #66ccee; }"
            "QTabBar::tab:hover { background-color: #0b1b22; color: #ffffff; }"
            "QPushButton, QToolButton { background-color: #111111; color: #eeeeee; border: 1px solid #377ba4; border-radius: 5px; padding: 6px 11px; }"
            "QPushButton:hover, QToolButton:hover { border-color: #66ccee; background-color: #0b1b22; }"
            "QPushButton:pressed, QToolButton:pressed { background-color: #132b35; }"
            "QPushButton:disabled, QToolButton:disabled { background-color: #181b1d; color: #77838c; border-color: #2f4348; }"
            "QRadioButton::indicator { width: 14px; height: 14px; border: 1px solid #66ccee; background-color: #050606; border-radius: 7px; }QRadioButton::indicator:checked { background-color: #66ccee; border: 3px solid #050606; outline: 1px solid #66ccee; }"
            "QStatusBar { background-color: #000000; border-top: 1px solid #2b5a60; }"
            "QStatusBar QLabel#statusPlainIcon, QStatusBar QLabel#statusPlainReadout { background-color: transparent; color: #eeeeee; border: none; padding: 0px 4px; }"
            "QStatusBar QLabel#networkStatusReadout { background-color: #071317; color: #eeeeee; border: 1px solid #66ccee; border-radius: 4px; padding: 4px 10px; min-height: 24px; }"
            "QStatusBar QLabel#statusUnitControl, QStatusBar QLabel#statusIconControl { background-color: #071317; color: #ffffff; border: 1px solid #66ccee; border-radius: 4px; padding: 4px 7px; min-width: 44px; min-height: 24px; }"
            "QStatusBar QLabel#statusUnitControl:hover, QStatusBar QLabel#statusIconControl:hover { background-color: #0f2a34; border-color: #ccbb44; }"
            "QScrollBar:vertical, QScrollBar:horizontal { background: #050606; }"
            "QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background: #2b5a60; border-radius: 3px; }")
            + spinBoxControlStyle(QStringLiteral("#377ba4"), QStringLiteral("#071317"), QStringLiteral("#0f2a34"))
            + defaultButtonStyle(QStringLiteral("#377ba4"), QStringLiteral("#ffffff"), QStringLiteral("#66ccee"))
            + themedCheckboxStyle(QStringLiteral("#66ccee"), QStringLiteral("#3c6f80"), QStringLiteral("#377ba4"), QStringLiteral("#66ccee")));
        return;
    }

    if (style_theme == QStringLiteral("yam")) {
        QPalette palette;
        setPaletteColors(palette,
            QColor(68, 68, 68), QColor(204, 204, 204),
            QColor(51, 51, 51), QColor(68, 68, 68),
            QColor(204, 204, 204), QColor(51, 51, 51),
            QColor(204, 204, 204), QColor(200, 120, 255),
            QColor(51, 51, 51));
        app->setPalette(palette);
        app->setStyleSheet(QStringLiteral(
            "QWidget { selection-background-color: #5a3a72; selection-color: #ffffff; }"
            "QMainWindow, QDialog { background-color: #444444; color: #cccccc; }"
            "QWidget#RPCConsole, QWidget#tab_info, QWidget#tab_console, QWidget#tab_nettraffic, QWidget#tab_peers, QWidget#tab_peermap { background-color: #444444; color: #cccccc; }"
            "QWidget#RPCConsole QTabWidget::pane, QDialog#OptionsDialog QTabWidget::pane { background-color: #444444; border: 1px solid #cccccc; }"
            "QWidget#RPCConsole QTabWidget QWidget, QDialog#OptionsDialog QTabWidget QWidget { background-color: #444444; color: #cccccc; }"
            "QWidget#RPCConsole QLabel, QWidget#RPCConsole QCheckBox, QWidget#RPCConsole QGroupBox, QDialog#OptionsDialog QLabel, QDialog#OptionsDialog QCheckBox, QDialog#OptionsDialog QGroupBox { color: #cccccc; }"
            "QDialog#OptionsDialog QWidget#tabMain, QDialog#OptionsDialog QWidget#tabWallet, QDialog#OptionsDialog QWidget#tabNetwork, QDialog#OptionsDialog QWidget#tabWindow, QDialog#OptionsDialog QWidget#tabDisplay, QDialog#OptionsDialog QFrame#frame { background-color: #444444; color: #cccccc; }"
            "QDialog#OptionsDialog QGroupBox#disabledLitecoinFeatures { background-color: #333333; color: #cccccc; border-color: #999999; }"
            "QDialog#OptionsDialog QGroupBox#disabledLitecoinFeatures::title { color: #cccccc; }"
            "QDialog#OptionsDialog QLabel#disabledFeatureNote, QDialog#OptionsDialog QLabel#disabledFeatureItem { background-color: transparent; color: #cccccc; padding-left: 0px; }"
            "QMenuBar { background-color: #333333; color: #cccccc; border-bottom: 1px solid #cccccc; }"
            "QMenuBar::item { color: #cccccc; background: transparent; padding: 4px 8px; }"
            "QMenuBar::item:selected { background-color: #222222; color: #ffffff; }"
            "QMenu, QToolTip { background-color: #333333; color: #cccccc; border: 1px solid #111111; }"
            "QMenu::item { color: #cccccc; background-color: transparent; padding: 5px 24px 5px 24px; }"
            "QMenu::item:selected { background-color: #444444; color: #ffffff; }"
            "QMenu::item:disabled { color: #666666; background-color: transparent; }"
            "QMenu::separator { height: 1px; background: #777777; margin: 4px 8px; }"
            "QToolBar { background-color: #333333; border-bottom: 1px solid #cccccc; spacing: 4px; padding-right: 10px; }"
            "QToolBar QToolButton { background-color: #333333; color: #cccccc; border: 1px solid transparent; border-radius: 0px; padding: 4px 7px; margin: 2px; min-height: 30px; }"
            "QToolBar QToolButton:hover { background-color: #222222; border-color: #c878ff; }"
            "QToolBar QToolButton:checked { background-color: #444444; border-color: #c878ff; color: #ffffff; }"
            "QToolBar QToolButton#toolbarIconButton { min-width: 48px; max-width: 48px; min-height: 48px; max-height: 48px; padding: 0; margin: 2px 4px; }"
            "QToolBar QToolButton#toolbarIconButton::menu-indicator { image: none; width: 0px; height: 0px; }"
            "QTabWidget::pane, QGroupBox { border: 1px solid #cccccc; border-radius: 0px; }"
            "QDialog#OptionsDialog QGroupBox { margin-top: 22px; padding-top: 16px; border-color: #c878ff; }"
            "QDialog#OptionsDialog QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; top: 2px; padding: 0 6px; background-color: #444444; color: #cccccc; }"
            "QTableView, QTreeView, QTextEdit, QPlainTextEdit, QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QAbstractSpinBox { background-color: #333333; color: #dddddd; border: 1px solid #cccccc; border-radius: 0px; min-height: 26px; }QTableView { gridline-color: #222222; alternate-background-color: #404040; }"
            "QComboBox { padding: 4px 8px; }QComboBox::drop-down { border-left: 1px solid #111111; width: 24px; }QComboBox::down-arrow { image: url(:/icons/dropdown_arrow); width: 10px; height: 7px; }"
            "QComboBox QAbstractItemView { background-color: #333333; color: #cccccc; selection-background-color: #5a3a72; selection-color: #ffffff; }"
            "QHeaderView::section { background-color: #333333; color: #cccccc; border: 1px solid #222222; padding: 5px; }"
            "QTabBar::tab { background-color: #333333; color: #cccccc; border: 1px solid #111111; padding: 5px 16px; min-height: 24px; min-width: 96px; }"
            "QTabBar::tab:!selected { background-color: #333333; color: #cccccc; }"
            "QTabBar::tab:selected { background-color: #444444; color: #ffffff; border-color: #c878ff; }"
            "QTabBar::tab:hover { background-color: #222222; color: #ffffff; }"
            "QPushButton, QToolButton { background-color: #333333; color: #cccccc; border: 1px solid #111111; border-radius: 0px; padding: 5px 10px; }"
            "QPushButton:hover, QToolButton:hover { border-color: #c878ff; background-color: #222222; }"
            "QPushButton:pressed, QToolButton:pressed { background-color: #444444; }"
            "QPushButton:disabled, QToolButton:disabled { background-color: #353535; color: #666666; border-color: #333333; }"
            "QRadioButton::indicator { width: 14px; height: 14px; border: 1px solid #c878ff; background-color: #333333; border-radius: 7px; }QRadioButton::indicator:checked { background-color: #c878ff; border: 3px solid #333333; outline: 1px solid #c878ff; }"
            "QStatusBar { background-color: #333333; border-top: 1px solid #cccccc; color: #cccccc; }"
            "QStatusBar QLabel#statusPlainIcon, QStatusBar QLabel#statusPlainReadout { background-color: transparent; color: #cccccc; border: none; padding: 0px 4px; }"
            "QStatusBar QLabel#networkStatusReadout { background-color: #333333; color: #cccccc; border: 1px solid #c878ff; border-radius: 0px; padding: 4px 10px; min-height: 24px; }"
            "QStatusBar QLabel#statusUnitControl, QStatusBar QLabel#statusIconControl { background-color: #333333; color: #cccccc; border: 1px solid #c878ff; border-radius: 0px; padding: 4px 7px; min-width: 44px; min-height: 24px; }"
            "QStatusBar QLabel#statusUnitControl:hover, QStatusBar QLabel#statusIconControl:hover { background-color: #444444; border-color: #ffffff; }"
            "QScrollBar:vertical, QScrollBar:horizontal { background: #333333; }"
            "QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background: #666666; border-radius: 0px; }")
            + spinBoxControlStyle(QStringLiteral("#cccccc"), QStringLiteral("#333333"), QStringLiteral("#444444"))
            + defaultButtonStyle(QStringLiteral("#5a3a72"), QStringLiteral("#ffffff"), QStringLiteral("#c878ff"))
            + themedCheckboxStyle(QStringLiteral("#c878ff"), QStringLiteral("#666666"), QStringLiteral("#553366"), QStringLiteral("#c878ff")));
        return;
    }

    const bool modern = style_theme == QStringLiteral("modern");
#else
    const bool modern = false;
#endif
    QPalette palette;
    setPaletteColors(palette,
        modern ? QColor(8, 10, 13) : QColor(11, 16, 23),
        modern ? QColor(242, 245, 248) : QColor(237, 243, 248),
        modern ? QColor(12, 16, 22) : QColor(17, 25, 35),
        modern ? QColor(18, 24, 32) : QColor(21, 31, 43),
        modern ? QColor(242, 245, 248) : QColor(237, 243, 248),
        modern ? QColor(18, 24, 32) : QColor(21, 31, 43),
        modern ? QColor(242, 245, 248) : QColor(237, 243, 248),
        modern ? QColor(131, 197, 255) : QColor(102, 179, 255),
        QColor(5, 8, 12));
    app->setPalette(palette);

    const QString sheet = modern ? QStringLiteral(
        "QWidget { selection-background-color: #83c5ff; selection-color: #05080c; }"
        "QMainWindow, QDialog { background-color: #080a0d; }"
        "QWidget#RPCConsole, QWidget#tab_info, QWidget#tab_console, QWidget#tab_nettraffic, QWidget#tab_peers, QWidget#tab_peermap { background-color: #080a0d; color: #f2f5f8; }"
        "QWidget#RPCConsole QTabWidget::pane, QDialog#OptionsDialog QTabWidget::pane { background-color: #080a0d; border: 1px solid rgba(145,165,188,0.24); }"
        "QWidget#RPCConsole QTabWidget QWidget, QDialog#OptionsDialog QTabWidget QWidget { background-color: #080a0d; color: #f2f5f8; }"
        "QWidget#RPCConsole QLabel, QWidget#RPCConsole QCheckBox, QWidget#RPCConsole QGroupBox, QDialog#OptionsDialog QLabel, QDialog#OptionsDialog QCheckBox, QDialog#OptionsDialog QGroupBox { color: #f2f5f8; }"
        "QDialog#OptionsDialog QWidget#tabMain, QDialog#OptionsDialog QWidget#tabWallet, QDialog#OptionsDialog QWidget#tabNetwork, QDialog#OptionsDialog QWidget#tabWindow, QDialog#OptionsDialog QWidget#tabDisplay, QDialog#OptionsDialog QFrame#frame { background-color: #080a0d; color: #f2f5f8; }"
        "QDialog#OptionsDialog QGroupBox#disabledLitecoinFeatures { background-color: #0f1720; color: #f2f5f8; border-color: rgba(176,198,222,0.62); }"
        "QDialog#OptionsDialog QGroupBox#disabledLitecoinFeatures::title { color: #f2f5f8; }"
        "QDialog#OptionsDialog QLabel#disabledFeatureNote, QDialog#OptionsDialog QLabel#disabledFeatureItem { background-color: transparent; color: #f2f5f8; padding-left: 0px; }"
        "QMenuBar { background-color: #080a0d; color: #f2f5f8; border-bottom: 1px solid rgba(176,198,222,0.30); }"
        "QMenuBar::item { color: #f2f5f8; background: transparent; padding: 4px 8px; }"
        "QMenuBar::item:selected { background-color: #172230; color: #ffffff; }"
        "QMenu, QToolTip { background-color: #121820; color: #f2f5f8; border: 1px solid rgba(145,165,188,0.35); }"
        "QMenu::item { color: #f2f5f8; background-color: transparent; padding: 5px 24px 5px 24px; }"
        "QMenu::item:selected { background-color: #203246; color: #ffffff; }"
        "QMenu::item:disabled { color: #7d8a98; background-color: transparent; }"
        "QMenu::separator { height: 1px; background: rgba(176,198,222,0.30); margin: 4px 8px; }"
        "QToolBar { background-color: #080a0d; border-bottom: 1px solid rgba(176,198,222,0.30); spacing: 4px; }"
        "QToolBar QToolButton { background-color: transparent; color: #f2f5f8; border: 1px solid transparent; border-radius: 5px; padding: 4px 7px; margin: 2px; min-height: 30px; }"
        "QToolBar QToolButton:hover { border-color: #83c5ff; background-color: #172230; }"
        "QToolBar QToolButton:checked { background-color: #1f6fd0; border-color: #b8dcff; color: #ffffff; }"
        "QToolBar QToolButton#toolbarIconButton { min-width: 48px; max-width: 48px; min-height: 48px; max-height: 48px; padding: 0; margin: 2px 4px; }"
        "QTabWidget::pane, QGroupBox { border: 1px solid rgba(145,165,188,0.24); border-radius: 8px; }"
        "QTableView, QTreeView, QTextEdit, QPlainTextEdit, QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QAbstractSpinBox { background-color: #0c1016; color: #f2f5f8; border: 1px solid rgba(176,198,222,0.46); border-radius: 4px; min-height: 22px; }QTableView { gridline-color: rgba(176,198,222,0.32); }"
        "QComboBox { padding: 2px 8px; }QComboBox::drop-down { border-left: 1px solid #8da2b8; width: 22px; }QComboBox::down-arrow { image: url(:/icons/dropdown_arrow); width: 10px; height: 7px; }"
        "QComboBox QAbstractItemView { background-color: #0c1016; color: #f2f5f8; selection-background-color: #1f6fd0; selection-color: #ffffff; }"
        "QHeaderView::section { background-color: #121820; color: #f2f5f8; border: 1px solid rgba(176,198,222,0.46); padding: 4px; }"
        "QTabBar::tab { background-color: #121820; color: #f2f5f8; border: 1px solid rgba(176,198,222,0.35); padding: 5px 16px; min-height: 24px; min-width: 76px; }"
        "QTabBar::tab:!selected { background-color: #121820; color: #f2f5f8; }"
        "QTabBar::tab:selected { background-color: #1f6fd0; color: #ffffff; border-color: #b8dcff; }"
        "QTabBar::tab:hover { background-color: #172230; color: #ffffff; }"
        "QPushButton, QToolButton { background-color: #121820; color: #f2f5f8; border: 1px solid rgba(176,198,222,0.52); border-radius: 5px; padding: 5px 10px; }"
        "QPushButton:hover { border-color: #83c5ff; background-color: #172230; }"
        "QToolButton:hover { border-color: #83c5ff; background-color: #172230; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #203246; }"
        "QPushButton:disabled, QToolButton:disabled { background-color: #171d26; color: #7d8a98; border-color: rgba(145,165,188,0.28); }"
        "QRadioButton::indicator { width: 14px; height: 14px; border: 1px solid #83c5ff; background-color: #0c1016; border-radius: 7px; }QRadioButton::indicator:checked { background-color: #83c5ff; border: 3px solid #0c1016; outline: 1px solid #b8dcff; }"
        "QStatusBar { background-color: #080a0d; border-top: 1px solid rgba(176,198,222,0.28); }"
        "QStatusBar QLabel#statusPlainIcon, QStatusBar QLabel#statusPlainReadout { background-color: transparent; color: #f2f5f8; border: none; padding: 0px 4px; }"
        "QStatusBar QLabel#networkStatusReadout { background-color: #182435; color: #f2f5f8; border: 1px solid rgba(210,231,255,0.86); border-radius: 4px; padding: 4px 10px; min-height: 24px; }"
        "QStatusBar QLabel#statusUnitControl, QStatusBar QLabel#statusIconControl { background-color: #182435; color: #ffffff; border: 1px solid rgba(210,231,255,0.86); border-radius: 4px; padding: 4px 7px; min-width: 44px; min-height: 24px; }"
        "QStatusBar QLabel#statusUnitControl:hover, QStatusBar QLabel#statusIconControl:hover { background-color: #20344d; border-color: #ffffff; }"
        "QScrollBar:vertical, QScrollBar:horizontal { background: #0c1016; }"
        "QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background: #34475f; border-radius: 3px; }")
        : QStringLiteral(
        "QWidget { selection-background-color: #66b3ff; selection-color: #05080c; }"
        "QMainWindow, QDialog { background-color: #0b1017; }"
        "QWidget#RPCConsole, QWidget#tab_info, QWidget#tab_console, QWidget#tab_nettraffic, QWidget#tab_peers, QWidget#tab_peermap { background-color: #0b1017; color: #edf3f8; }"
        "QWidget#RPCConsole QTabWidget::pane, QDialog#OptionsDialog QTabWidget::pane { background-color: #0b1017; border: 1px solid #52677f; }"
        "QWidget#RPCConsole QTabWidget QWidget, QDialog#OptionsDialog QTabWidget QWidget { background-color: #0b1017; color: #edf3f8; }"
        "QWidget#RPCConsole QLabel, QWidget#RPCConsole QCheckBox, QWidget#RPCConsole QGroupBox, QDialog#OptionsDialog QLabel, QDialog#OptionsDialog QCheckBox, QDialog#OptionsDialog QGroupBox { color: #edf3f8; }"
        "QDialog#OptionsDialog QWidget#tabMain, QDialog#OptionsDialog QWidget#tabWallet, QDialog#OptionsDialog QWidget#tabNetwork, QDialog#OptionsDialog QWidget#tabWindow, QDialog#OptionsDialog QWidget#tabDisplay, QDialog#OptionsDialog QFrame#frame { background-color: #0b1017; color: #edf3f8; }"
        "QDialog#OptionsDialog QGroupBox#disabledLitecoinFeatures { background-color: #14202c; color: #edf3f8; border-color: #8fcaff; }"
        "QDialog#OptionsDialog QGroupBox#disabledLitecoinFeatures::title { color: #edf3f8; }"
        "QDialog#OptionsDialog QLabel#disabledFeatureNote, QDialog#OptionsDialog QLabel#disabledFeatureItem { background-color: transparent; color: #edf3f8; padding-left: 0px; }"
        "QMenuBar { background-color: #0b1017; color: #edf3f8; border-bottom: 1px solid #52677f; }"
        "QMenuBar::item { color: #edf3f8; background: transparent; padding: 4px 8px; }"
        "QMenuBar::item:selected { background-color: #1d2b3b; color: #ffffff; }"
        "QMenu, QToolTip { background-color: #151f2b; color: #edf3f8; border: 1px solid #283546; }"
        "QMenu::item { color: #edf3f8; background-color: transparent; padding: 5px 24px 5px 24px; }"
        "QMenu::item:selected { background-color: #26384c; color: #ffffff; }"
        "QMenu::item:disabled { color: #7f8d99; background-color: transparent; }"
        "QMenu::separator { height: 1px; background: #52677f; margin: 4px 8px; }"
        "QToolBar { background-color: #0b1017; border-bottom: 1px solid #52677f; spacing: 4px; }"
        "QToolBar QToolButton { background-color: transparent; color: #edf3f8; border: 1px solid transparent; border-radius: 5px; padding: 4px 7px; margin: 2px; min-height: 30px; }"
        "QToolBar QToolButton:hover { border-color: #9dccff; background-color: #1d2b3b; }"
        "QToolBar QToolButton:checked { background-color: #34465d; border-color: #8fcaff; color: #ffffff; }"
        "QToolBar QToolButton#toolbarIconButton { min-width: 48px; max-width: 48px; min-height: 48px; max-height: 48px; padding: 0; margin: 2px 4px; }"
        "QTableView, QTreeView, QTextEdit, QPlainTextEdit, QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QAbstractSpinBox { background-color: #111923; color: #edf3f8; border: 1px solid #52677f; min-height: 22px; }QTableView { gridline-color: #52677f; }"
        "QComboBox { padding: 2px 8px; }QComboBox::drop-down { border-left: 1px solid #8da2b8; width: 22px; }QComboBox::down-arrow { image: url(:/icons/dropdown_arrow); width: 10px; height: 7px; }"
        "QComboBox QAbstractItemView { background-color: #111923; color: #edf3f8; selection-background-color: #1f6fd0; selection-color: #ffffff; }"
        "QHeaderView::section { background-color: #151f2b; color: #edf3f8; border: 1px solid #52677f; padding: 4px; }"
        "QTabBar::tab { background-color: #151f2b; color: #edf3f8; border: 1px solid #52677f; padding: 5px 16px; min-height: 24px; min-width: 76px; }"
        "QTabBar::tab:!selected { background-color: #151f2b; color: #edf3f8; }"
        "QTabBar::tab:selected { background-color: #1f6fd0; color: #ffffff; border-color: #d7ebff; }"
        "QTabBar::tab:hover { background-color: #1d2b3b; color: #ffffff; }"
        "QPushButton, QToolButton { background-color: #151f2b; color: #edf3f8; border: 1px solid #8fcaff; border-radius: 4px; padding: 5px 10px; }"
        "QPushButton:hover, QToolButton:hover { border-color: #8fcaff; background-color: #1d2b3b; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #26384c; }"
        "QPushButton:disabled, QToolButton:disabled { background-color: #1a2532; color: #81909f; border-color: #38495a; }"
        "QRadioButton::indicator { width: 14px; height: 14px; border: 1px solid #8fcaff; background-color: #111923; border-radius: 7px; }QRadioButton::indicator:checked { background-color: #8fcaff; border: 3px solid #111923; outline: 1px solid #d7ebff; }"
        "QStatusBar { background-color: #0b1017; border-top: 1px solid #52677f; }"
        "QStatusBar QLabel#statusPlainIcon, QStatusBar QLabel#statusPlainReadout { background-color: transparent; color: #edf3f8; border: none; padding: 0px 4px; }"
        "QStatusBar QLabel#networkStatusReadout { background-color: #1f3044; color: #edf3f8; border: 1px solid #8fcaff; border-radius: 4px; padding: 4px 10px; min-height: 24px; }"
        "QStatusBar QLabel#statusUnitControl, QStatusBar QLabel#statusIconControl { background-color: #1f3044; color: #ffffff; border: 1px solid #8fcaff; border-radius: 4px; padding: 4px 7px; min-width: 44px; min-height: 24px; font-weight: 700; }"
            "QStatusBar QLabel#statusUnitControl:hover, QStatusBar QLabel#statusIconControl:hover { background-color: #2a4059; border-color: #ffffff; }");
    app->setStyleSheet(sheet + (modern
        ? spinBoxControlStyle(QStringLiteral("rgba(176,198,222,0.46)"), QStringLiteral("#182435"), QStringLiteral("#20344d")) +
            defaultButtonStyle(QStringLiteral("#1f6fd0"), QStringLiteral("#ffffff"), QStringLiteral("#b8dcff")) +
            themedCheckboxStyle(QStringLiteral("#83c5ff"), QStringLiteral("#5f7186"), QStringLiteral("#1f6fd0"), QStringLiteral("#b8dcff"))
        : spinBoxControlStyle(QStringLiteral("#52677f"), QStringLiteral("#1f3044"), QStringLiteral("#2a4059")) +
            defaultButtonStyle(QStringLiteral("#1f6fd0"), QStringLiteral("#ffffff"), QStringLiteral("#d7ebff")) +
            themedCheckboxStyle(QStringLiteral("#8fcaff"), QStringLiteral("#657b91"), QStringLiteral("#1f6fd0"), QStringLiteral("#d7ebff"))));
}

void setupAddressWidget(QValidatedLineEdit *widget, QWidget *parent)
{
    parent->setFocusProxy(widget);

    widget->setFont(fixedPitchFont());
    // We don't want translators to use own addresses in translations
    // and this is the only place, where this address is supplied.
    widget->setPlaceholderText(QObject::tr("Enter a Defcoin address (e.g. %1)").arg(
        QStringLiteral("dfc1q07z32ql4m0pd3j4g0zc808dx786gsw6y5lru70")));
    widget->setValidator(new BitcoinAddressEntryValidator(parent));
    widget->setCheckValidator(new BitcoinAddressCheckValidator(parent));
}

bool parseBitcoinURI(const QUrl &uri, SendCoinsRecipient *out)
{
    // return if URI is not valid or is no bitcoin: URI
    if(!uri.isValid() || uri.scheme() != QString("defcoin"))
        return false;

    SendCoinsRecipient rv;
    rv.address = uri.path();
    // Trim any following forward slash which may have been added by the OS
    if (rv.address.endsWith("/")) {
        rv.address.truncate(rv.address.length() - 1);
    }
    rv.amount = 0;

    QUrlQuery uriQuery(uri);
    QList<QPair<QString, QString> > items = uriQuery.queryItems();
    for (QList<QPair<QString, QString> >::iterator i = items.begin(); i != items.end(); i++)
    {
        bool fShouldReturnFalse = false;
        if (i->first.startsWith("req-"))
        {
            i->first.remove(0, 4);
            fShouldReturnFalse = true;
        }

        if (i->first == "label")
        {
            rv.label = i->second;
            fShouldReturnFalse = false;
        }
        if (i->first == "message")
        {
            rv.message = i->second;
            fShouldReturnFalse = false;
        }
        else if (i->first == "amount")
        {
            if(!i->second.isEmpty())
            {
                if(!BitcoinUnits::parse(BitcoinUnits::BTC, i->second, &rv.amount))
                {
                    return false;
                }
            }
            fShouldReturnFalse = false;
        }

        if (fShouldReturnFalse)
            return false;
    }
    if(out)
    {
        *out = rv;
    }
    return true;
}

bool parseBitcoinURI(QString uri, SendCoinsRecipient *out)
{
    QUrl uriInstance(uri);
    return parseBitcoinURI(uriInstance, out);
}

QString formatBitcoinURI(const SendCoinsRecipient &info)
{
    bool bech_32 = info.address.startsWith(QString::fromStdString(Params().Bech32HRP() + "1"));

    QString ret = QString("defcoin:%1").arg(bech_32 ? info.address.toUpper() : info.address);
    int paramCount = 0;

    if (info.amount)
    {
        ret += QString("?amount=%1").arg(BitcoinUnits::format(BitcoinUnits::BTC, info.amount, false, BitcoinUnits::SeparatorStyle::NEVER));
        paramCount++;
    }

    if (!info.label.isEmpty())
    {
        QString lbl(QUrl::toPercentEncoding(info.label));
        ret += QString("%1label=%2").arg(paramCount == 0 ? "?" : "&").arg(lbl);
        paramCount++;
    }

    if (!info.message.isEmpty())
    {
        QString msg(QUrl::toPercentEncoding(info.message));
        ret += QString("%1message=%2").arg(paramCount == 0 ? "?" : "&").arg(msg);
        paramCount++;
    }

    return ret;
}

bool isDust(interfaces::Node& node, const QString& address, const CAmount& amount)
{
    CTxDestination dest = DecodeDestination(address.toStdString());
    CScript script = GetScriptForDestination(dest);
    CTxOut txOut(amount, script);
    return IsDust(txOut, node.getDustRelayFee());
}

bool isMWEBAddressBeforeActivated(interfaces::Node& node, const QString& address)
{
    CTxDestination dest = DecodeDestination(address.toStdString());
    if (dest.type() == typeid(StealthAddress)) {
        return !node.isMWEBActive();
    }

    return false;
}

QString HtmlEscape(const QString& str, bool fMultiLine)
{
    QString escaped = str.toHtmlEscaped();
    if(fMultiLine)
    {
        escaped = escaped.replace("\n", "<br>\n");
    }
    return escaped;
}

QString HtmlEscape(const std::string& str, bool fMultiLine)
{
    return HtmlEscape(QString::fromStdString(str), fMultiLine);
}

void copyEntryData(const QAbstractItemView *view, int column, int role)
{
    if(!view || !view->selectionModel())
        return;
    QModelIndexList selection = view->selectionModel()->selectedRows(column);

    if(!selection.isEmpty())
    {
        // Copy first item
        setClipboard(selection.at(0).data(role).toString());
    }
}

QList<QModelIndex> getEntryData(const QAbstractItemView *view, int column)
{
    if(!view || !view->selectionModel())
        return QList<QModelIndex>();
    return view->selectionModel()->selectedRows(column);
}

bool hasEntryData(const QAbstractItemView *view, int column, int role)
{
    QModelIndexList selection = getEntryData(view, column);
    if (selection.isEmpty()) return false;
    return !selection.at(0).data(role).toString().isEmpty();
}

QString getDefaultDataDirectory()
{
    return boostPathToQString(GetDefaultDataDir());
}

QString getSaveFileName(QWidget *parent, const QString &caption, const QString &dir,
    const QString &filter,
    QString *selectedSuffixOut)
{
    QString selectedFilter;
    QString myDir;
    if(dir.isEmpty()) // Default to user documents location
    {
        myDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    else
    {
        myDir = dir;
    }
    /* Directly convert path to native OS path separators */
    QString result = QDir::toNativeSeparators(QFileDialog::getSaveFileName(parent, caption, myDir, filter, &selectedFilter));

    /* Extract first suffix from filter pattern "Description (*.foo)" or "Description (*.foo *.bar ...) */
    QRegExp filter_re(".* \\(\\*\\.(.*)[ \\)]");
    QString selectedSuffix;
    if(filter_re.exactMatch(selectedFilter))
    {
        selectedSuffix = filter_re.cap(1);
    }

    /* Add suffix if needed */
    QFileInfo info(result);
    if(!result.isEmpty())
    {
        if(info.suffix().isEmpty() && !selectedSuffix.isEmpty())
        {
            /* No suffix specified, add selected suffix */
            if(!result.endsWith("."))
                result.append(".");
            result.append(selectedSuffix);
        }
    }

    /* Return selected suffix if asked to */
    if(selectedSuffixOut)
    {
        *selectedSuffixOut = selectedSuffix;
    }
    return result;
}

QString getOpenFileName(QWidget *parent, const QString &caption, const QString &dir,
    const QString &filter,
    QString *selectedSuffixOut)
{
    QString selectedFilter;
    QString myDir;
    if(dir.isEmpty()) // Default to user documents location
    {
        myDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    else
    {
        myDir = dir;
    }
    /* Directly convert path to native OS path separators */
    QString result = QDir::toNativeSeparators(QFileDialog::getOpenFileName(parent, caption, myDir, filter, &selectedFilter));

    if(selectedSuffixOut)
    {
        /* Extract first suffix from filter pattern "Description (*.foo)" or "Description (*.foo *.bar ...) */
        QRegExp filter_re(".* \\(\\*\\.(.*)[ \\)]");
        QString selectedSuffix;
        if(filter_re.exactMatch(selectedFilter))
        {
            selectedSuffix = filter_re.cap(1);
        }
        *selectedSuffixOut = selectedSuffix;
    }
    return result;
}

Qt::ConnectionType blockingGUIThreadConnection()
{
    if(QThread::currentThread() != qApp->thread())
    {
        return Qt::BlockingQueuedConnection;
    }
    else
    {
        return Qt::DirectConnection;
    }
}

bool checkPoint(const QPoint &p, const QWidget *w)
{
    QWidget *atW = QApplication::widgetAt(w->mapToGlobal(p));
    if (!atW) return false;
    return atW->window() == w;
}

bool isObscured(QWidget *w)
{
    return !(checkPoint(QPoint(0, 0), w)
        && checkPoint(QPoint(w->width() - 1, 0), w)
        && checkPoint(QPoint(0, w->height() - 1), w)
        && checkPoint(QPoint(w->width() - 1, w->height() - 1), w)
        && checkPoint(QPoint(w->width() / 2, w->height() / 2), w));
}

void bringToFront(QWidget* w)
{
#ifdef Q_OS_MAC
    ForceActivation();
#endif

    if (w) {
        // activateWindow() (sometimes) helps with keyboard focus on Windows
        if (w->isMinimized()) {
            w->showNormal();
        } else {
            w->show();
        }
        w->activateWindow();
        w->raise();
    }
}

void handleCloseWindowShortcut(QWidget* w)
{
    QObject::connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_W), w), &QShortcut::activated, w, &QWidget::close);
}

void openDebugLogfile()
{
    fs::path pathDebug = GetDataDir() / "debug.log";

    /* Open debug.log with the associated application */
    if (fs::exists(pathDebug)) {
        const QString path = boostPathToQString(pathDebug);
#ifdef Q_OS_MAC
        if (QProcess::startDetached("/usr/bin/open", QStringList{"-a", "Console", path})) {
            return;
        }
#endif
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

bool openBitcoinConf()
{
    fs::path pathConfig = GetConfigFile(gArgs.GetArg("-conf", BITCOIN_CONF_FILENAME));

    /* Create the file */
    fsbridge::ofstream configFile(pathConfig, std::ios_base::app);

    if (!configFile.good())
        return false;

    configFile.close();

    /* Open bitcoin.conf with the associated application */
    bool res = QDesktopServices::openUrl(QUrl::fromLocalFile(boostPathToQString(pathConfig)));
#ifdef Q_OS_MAC
    // Workaround for macOS-specific behavior; see #15409.
    if (!res) {
        res = QProcess::startDetached("/usr/bin/open", QStringList{"-t", boostPathToQString(pathConfig)});
    }
#endif

    return res;
}

ToolTipToRichTextFilter::ToolTipToRichTextFilter(int _size_threshold, QObject *parent) :
    QObject(parent),
    size_threshold(_size_threshold)
{

}

bool ToolTipToRichTextFilter::eventFilter(QObject *obj, QEvent *evt)
{
    if(evt->type() == QEvent::ToolTipChange)
    {
        QWidget *widget = static_cast<QWidget*>(obj);
        QString tooltip = widget->toolTip();
        if(tooltip.size() > size_threshold && !tooltip.startsWith("<qt") && !Qt::mightBeRichText(tooltip))
        {
            // Envelop with <qt></qt> to make sure Qt detects this as rich text
            // Escape the current message as HTML and replace \n by <br>
            tooltip = "<qt>" + HtmlEscape(tooltip, true) + "</qt>";
            widget->setToolTip(tooltip);
            return true;
        }
    }
    return QObject::eventFilter(obj, evt);
}

LabelOutOfFocusEventFilter::LabelOutOfFocusEventFilter(QObject* parent)
    : QObject(parent)
{
}

bool LabelOutOfFocusEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::FocusOut) {
        auto focus_out = static_cast<QFocusEvent*>(event);
        if (focus_out->reason() != Qt::PopupFocusReason) {
            auto label = qobject_cast<QLabel*>(watched);
            if (label) {
                auto flags = label->textInteractionFlags();
                label->setTextInteractionFlags(Qt::NoTextInteraction);
                label->setTextInteractionFlags(flags);
            }
        }
    }

    return QObject::eventFilter(watched, event);
}

void TableViewLastColumnResizingFixer::connectViewHeadersSignals()
{
    connect(tableView->horizontalHeader(), &QHeaderView::sectionResized, this, &TableViewLastColumnResizingFixer::on_sectionResized);
    connect(tableView->horizontalHeader(), &QHeaderView::geometriesChanged, this, &TableViewLastColumnResizingFixer::on_geometriesChanged);
}

// We need to disconnect these while handling the resize events, otherwise we can enter infinite loops.
void TableViewLastColumnResizingFixer::disconnectViewHeadersSignals()
{
    disconnect(tableView->horizontalHeader(), &QHeaderView::sectionResized, this, &TableViewLastColumnResizingFixer::on_sectionResized);
    disconnect(tableView->horizontalHeader(), &QHeaderView::geometriesChanged, this, &TableViewLastColumnResizingFixer::on_geometriesChanged);
}

// Setup the resize mode, handles compatibility for Qt5 and below as the method signatures changed.
// Refactored here for readability.
void TableViewLastColumnResizingFixer::setViewHeaderResizeMode(int logicalIndex, QHeaderView::ResizeMode resizeMode)
{
    tableView->horizontalHeader()->setSectionResizeMode(logicalIndex, resizeMode);
}

void TableViewLastColumnResizingFixer::resizeColumn(int nColumnIndex, int width)
{
    tableView->setColumnWidth(nColumnIndex, width);
    tableView->horizontalHeader()->resizeSection(nColumnIndex, width);
}

int TableViewLastColumnResizingFixer::getColumnsWidth()
{
    int nColumnsWidthSum = 0;
    for (int i = 0; i < columnCount; i++)
    {
        nColumnsWidthSum += tableView->horizontalHeader()->sectionSize(i);
    }
    return nColumnsWidthSum;
}

int TableViewLastColumnResizingFixer::getAvailableWidthForColumn(int column)
{
    int nResult = lastColumnMinimumWidth;
    int nTableWidth = tableView->horizontalHeader()->width();

    if (nTableWidth > 0)
    {
        int nOtherColsWidth = getColumnsWidth() - tableView->horizontalHeader()->sectionSize(column);
        nResult = std::max(nResult, nTableWidth - nOtherColsWidth);
    }

    return nResult;
}

// Make sure we don't make the columns wider than the table's viewport width.
void TableViewLastColumnResizingFixer::adjustTableColumnsWidth()
{
    disconnectViewHeadersSignals();
    resizeColumn(lastColumnIndex, getAvailableWidthForColumn(lastColumnIndex));
    connectViewHeadersSignals();

    int nTableWidth = tableView->horizontalHeader()->width();
    int nColsWidth = getColumnsWidth();
    if (nColsWidth > nTableWidth)
    {
        resizeColumn(secondToLastColumnIndex,getAvailableWidthForColumn(secondToLastColumnIndex));
    }
}

// Make column use all the space available, useful during window resizing.
void TableViewLastColumnResizingFixer::stretchColumnWidth(int column)
{
    disconnectViewHeadersSignals();
    resizeColumn(column, getAvailableWidthForColumn(column));
    connectViewHeadersSignals();
}

// When a section is resized this is a slot-proxy for ajustAmountColumnWidth().
void TableViewLastColumnResizingFixer::on_sectionResized(int logicalIndex, int oldSize, int newSize)
{
    adjustTableColumnsWidth();
    int remainingWidth = getAvailableWidthForColumn(logicalIndex);
    if (newSize > remainingWidth)
    {
       resizeColumn(logicalIndex, remainingWidth);
    }
}

// When the table's geometry is ready, we manually perform the stretch of the "Message" column,
// as the "Stretch" resize mode does not allow for interactive resizing.
void TableViewLastColumnResizingFixer::on_geometriesChanged()
{
    if ((getColumnsWidth() - this->tableView->horizontalHeader()->width()) != 0)
    {
        disconnectViewHeadersSignals();
        resizeColumn(secondToLastColumnIndex, getAvailableWidthForColumn(secondToLastColumnIndex));
        connectViewHeadersSignals();
    }
}

/**
 * Initializes all internal variables and prepares the
 * the resize modes of the last 2 columns of the table and
 */
TableViewLastColumnResizingFixer::TableViewLastColumnResizingFixer(QTableView* table, int lastColMinimumWidth, int allColsMinimumWidth, QObject *parent) :
    QObject(parent),
    tableView(table),
    lastColumnMinimumWidth(lastColMinimumWidth),
    allColumnsMinimumWidth(allColsMinimumWidth)
{
    columnCount = tableView->horizontalHeader()->count();
    lastColumnIndex = columnCount - 1;
    secondToLastColumnIndex = columnCount - 2;
    tableView->horizontalHeader()->setMinimumSectionSize(allColumnsMinimumWidth);
    setViewHeaderResizeMode(secondToLastColumnIndex, QHeaderView::Interactive);
    setViewHeaderResizeMode(lastColumnIndex, QHeaderView::Interactive);
}

#ifdef WIN32
fs::path static StartupShortcutPath()
{
    std::string chain = gArgs.GetChainName();
    if (chain == CBaseChainParams::MAIN)
        return GetSpecialFolderPath(CSIDL_STARTUP) / "Defcoin.lnk";
    if (chain == CBaseChainParams::TESTNET) // Remove this special case when CBaseChainParams::TESTNET = "testnet4"
        return GetSpecialFolderPath(CSIDL_STARTUP) / "Defcoin (testnet).lnk";
    return GetSpecialFolderPath(CSIDL_STARTUP) / strprintf("Defcoin (%s).lnk", chain);
}

bool GetStartOnSystemStartup()
{
    // check for Bitcoin*.lnk
    return fs::exists(StartupShortcutPath());
}

bool SetStartOnSystemStartup(bool fAutoStart)
{
    // If the shortcut exists already, remove it for updating
    fs::remove(StartupShortcutPath());

    if (fAutoStart)
    {
        CoInitialize(nullptr);

        // Get a pointer to the IShellLink interface.
        IShellLinkW* psl = nullptr;
        HRESULT hres = CoCreateInstance(CLSID_ShellLink, nullptr,
            CLSCTX_INPROC_SERVER, IID_IShellLinkW,
            reinterpret_cast<void**>(&psl));

        if (SUCCEEDED(hres))
        {
            // Get the current executable path
            WCHAR pszExePath[MAX_PATH];
            GetModuleFileNameW(nullptr, pszExePath, ARRAYSIZE(pszExePath));

            // Start client minimized
            QString strArgs = "-min";
            // Set -testnet /-regtest options
            strArgs += QString::fromStdString(strprintf(" -chain=%s", gArgs.GetChainName()));

            // Set the path to the shortcut target
            psl->SetPath(pszExePath);
            PathRemoveFileSpecW(pszExePath);
            psl->SetWorkingDirectory(pszExePath);
            psl->SetShowCmd(SW_SHOWMINNOACTIVE);
            psl->SetArguments(strArgs.toStdWString().c_str());

            // Query IShellLink for the IPersistFile interface for
            // saving the shortcut in persistent storage.
            IPersistFile* ppf = nullptr;
            hres = psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&ppf));
            if (SUCCEEDED(hres))
            {
                // Save the link by calling IPersistFile::Save.
                hres = ppf->Save(StartupShortcutPath().wstring().c_str(), TRUE);
                ppf->Release();
                psl->Release();
                CoUninitialize();
                return true;
            }
            psl->Release();
        }
        CoUninitialize();
        return false;
    }
    return true;
}
#elif defined(Q_OS_LINUX)

// Follow the Desktop Application Autostart Spec:
// http://standards.freedesktop.org/autostart-spec/autostart-spec-latest.html

fs::path static GetAutostartDir()
{
    char* pszConfigHome = getenv("XDG_CONFIG_HOME");
    if (pszConfigHome) return fs::path(pszConfigHome) / "autostart";
    char* pszHome = getenv("HOME");
    if (pszHome) return fs::path(pszHome) / ".config" / "autostart";
    return fs::path();
}

fs::path static GetAutostartFilePath()
{
    std::string chain = gArgs.GetChainName();
    if (chain == CBaseChainParams::MAIN)
        return GetAutostartDir() / "defcoin.desktop";
    return GetAutostartDir() / strprintf("defcoin-%s.desktop", chain);
}

bool GetStartOnSystemStartup()
{
    fsbridge::ifstream optionFile(GetAutostartFilePath());
    if (!optionFile.good())
        return false;
    // Scan through file for "Hidden=true":
    std::string line;
    while (!optionFile.eof())
    {
        getline(optionFile, line);
        if (line.find("Hidden") != std::string::npos &&
            line.find("true") != std::string::npos)
            return false;
    }
    optionFile.close();

    return true;
}

bool SetStartOnSystemStartup(bool fAutoStart)
{
    if (!fAutoStart)
        fs::remove(GetAutostartFilePath());
    else
    {
        char pszExePath[MAX_PATH+1];
        ssize_t r = readlink("/proc/self/exe", pszExePath, sizeof(pszExePath) - 1);
        if (r == -1)
            return false;
        pszExePath[r] = '\0';

        fs::create_directories(GetAutostartDir());

        fsbridge::ofstream optionFile(GetAutostartFilePath(), std::ios_base::out | std::ios_base::trunc);
        if (!optionFile.good())
            return false;
        std::string chain = gArgs.GetChainName();
        // Write a bitcoin.desktop file to the autostart directory:
        optionFile << "[Desktop Entry]\n";
        optionFile << "Type=Application\n";
        if (chain == CBaseChainParams::MAIN)
            optionFile << "Name=Defcoin\n";
        else
            optionFile << strprintf("Name=Defcoin (%s)\n", chain);
        optionFile << "Exec=" << pszExePath << strprintf(" -min -chain=%s\n", chain);
        optionFile << "Terminal=false\n";
        optionFile << "Hidden=false\n";
        optionFile.close();
    }
    return true;
}

#else

bool GetStartOnSystemStartup() { return false; }
bool SetStartOnSystemStartup(bool fAutoStart) { return false; }

#endif

void setClipboard(const QString& str)
{
    QApplication::clipboard()->setText(str, QClipboard::Clipboard);
    QApplication::clipboard()->setText(str, QClipboard::Selection);
}

fs::path qstringToBoostPath(const QString &path)
{
    return fs::path(path.toStdString());
}

QString boostPathToQString(const fs::path &path)
{
    return QString::fromStdString(path.string());
}

QString formatDurationStr(int secs)
{
    QStringList strList;
    int days = secs / 86400;
    int hours = (secs % 86400) / 3600;
    int mins = (secs % 3600) / 60;
    int seconds = secs % 60;

    if (days)
        strList.append(QString(QObject::tr("%1 d")).arg(days));
    if (hours)
        strList.append(QString(QObject::tr("%1 h")).arg(hours));
    if (mins)
        strList.append(QString(QObject::tr("%1 m")).arg(mins));
    if (seconds || (!days && !hours && !mins))
        strList.append(QString(QObject::tr("%1 s")).arg(seconds));

    return strList.join(" ");
}

QString formatServicesStr(quint64 mask)
{
    QStringList strList;

    for (const auto& flag : serviceFlagsToStr(mask)) {
        strList.append(QString::fromStdString(flag));
    }

    if (strList.size())
        return strList.join(" & ");
    else
        return QObject::tr("None");
}

QString formatPingTime(int64_t ping_usec)
{
    return (ping_usec == std::numeric_limits<int64_t>::max() || ping_usec == 0) ? QObject::tr("N/A") : QString(QObject::tr("%1 ms")).arg(QString::number((int)(ping_usec / 1000), 10));
}

QString formatTimeOffset(int64_t nTimeOffset)
{
  return QString(QObject::tr("%1 s")).arg(QString::number((int)nTimeOffset, 10));
}

QString formatNiceTimeOffset(qint64 secs)
{
    // Represent time from last generated block in human readable text
    QString timeBehindText;
    const int HOUR_IN_SECONDS = 60*60;
    const int DAY_IN_SECONDS = 24*60*60;
    const int WEEK_IN_SECONDS = 7*24*60*60;
    const int YEAR_IN_SECONDS = 31556952; // Average length of year in Gregorian calendar
    if(secs < 60)
    {
        timeBehindText = QObject::tr("%n second(s)","",secs);
    }
    else if(secs < 2*HOUR_IN_SECONDS)
    {
        timeBehindText = QObject::tr("%n minute(s)","",secs/60);
    }
    else if(secs < 2*DAY_IN_SECONDS)
    {
        timeBehindText = QObject::tr("%n hour(s)","",secs/HOUR_IN_SECONDS);
    }
    else if(secs < 2*WEEK_IN_SECONDS)
    {
        timeBehindText = QObject::tr("%n day(s)","",secs/DAY_IN_SECONDS);
    }
    else if(secs < YEAR_IN_SECONDS)
    {
        timeBehindText = QObject::tr("%n week(s)","",secs/WEEK_IN_SECONDS);
    }
    else
    {
        qint64 years = secs / YEAR_IN_SECONDS;
        qint64 remainder = secs % YEAR_IN_SECONDS;
        timeBehindText = QObject::tr("%1 and %2").arg(QObject::tr("%n year(s)", "", years)).arg(QObject::tr("%n week(s)","", remainder/WEEK_IN_SECONDS));
    }
    return timeBehindText;
}

QString formatBytes(uint64_t bytes)
{
    if(bytes < 1024)
        return QString(QObject::tr("%1 B")).arg(bytes);
    if(bytes < 1024 * 1024)
        return QString(QObject::tr("%1 KB")).arg(bytes / 1024);
    if(bytes < 1024 * 1024 * 1024)
        return QString(QObject::tr("%1 MB")).arg(bytes / 1024 / 1024);

    return QString(QObject::tr("%1 GB")).arg(bytes / 1024 / 1024 / 1024);
}

qreal calculateIdealFontSize(int width, const QString& text, QFont font, qreal minPointSize, qreal font_size) {
    while(font_size >= minPointSize) {
        font.setPointSizeF(font_size);
        QFontMetrics fm(font);
        if (TextWidth(fm, text) < width) {
            break;
        }
        font_size -= 0.5;
    }
    return font_size;
}

void ClickableLabel::mouseReleaseEvent(QMouseEvent *event)
{
    Q_EMIT clicked(event->pos());
}

void ClickableProgressBar::mouseReleaseEvent(QMouseEvent *event)
{
    Q_EMIT clicked(event->pos());
}

bool ItemDelegate::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            Q_EMIT keyEscapePressed();
        }
    }
    return QItemDelegate::eventFilter(object, event);
}

void PolishProgressDialog(QProgressDialog* dialog)
{
#ifdef Q_OS_MAC
    // Workaround for macOS-only Qt bug; see: QTBUG-65750, QTBUG-70357.
    const int margin = TextWidth(dialog->fontMetrics(), ("X"));
    dialog->resize(dialog->width() + 2 * margin, dialog->height());
    dialog->show();
#else
    Q_UNUSED(dialog);
#endif
}

void PolishComboBox(QComboBox* combo, int minimum_width)
{
    if (!combo) return;

    QFontMetrics metrics(combo->font());
    int text_width = 0;
    for (int i = 0; i < combo->count(); ++i) {
        text_width = std::max(text_width, TextWidth(metrics, combo->itemText(i)));
    }

    const int frame_padding = combo->style()->pixelMetric(QStyle::PM_ComboBoxFrameWidth, nullptr, combo) * 2;
    const int arrow_width = 34;
    const int horizontal_padding = 22;
    const int desired_width = std::max(minimum_width, text_width + frame_padding + arrow_width + horizontal_padding);

    combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    combo->setMinimumWidth(desired_width);
    if (combo->view()) {
        combo->view()->setMinimumWidth(std::max(desired_width, text_width + 34));
    }
}

int TextWidth(const QFontMetrics& fm, const QString& text)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
    return fm.horizontalAdvance(text);
#else
    return fm.width(text);
#endif
}

void LogQtInfo()
{
#ifdef QT_STATIC
    const std::string qt_link{"static"};
#else
    const std::string qt_link{"dynamic"};
#endif
#ifdef QT_STATICPLUGIN
    const std::string plugin_link{"static"};
#else
    const std::string plugin_link{"dynamic"};
#endif
    LogPrintf("Qt %s (%s), plugin=%s (%s)\n", qVersion(), qt_link, QGuiApplication::platformName().toStdString(), plugin_link);
    LogPrintf("System: %s, %s\n", QSysInfo::prettyProductName().toStdString(), QSysInfo::buildAbi().toStdString());
    for (const QScreen* s : QGuiApplication::screens()) {
        LogPrintf("Screen: %s %dx%d, pixel ratio=%.1f\n", s->name().toStdString(), s->size().width(), s->size().height(), s->devicePixelRatio());
    }
}

void PopupMenu(QMenu* menu, const QPoint& point, QAction* at_action)
{
    // The qminimal plugin does not provide window system integration.
    if (QApplication::platformName() == "minimal") return;
    menu->popup(point, at_action);
}

} // namespace GUIUtil
