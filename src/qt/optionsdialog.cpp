// Copyright (c) 2011-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/optionsdialog.h>
#include <qt/forms/ui_optionsdialog.h>

#include <qt/bitcoinunits.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <clientversion.h>
#include <interfaces/node.h>
#include <net_processing.h>
#include <validation.h> // for DEFAULT_SCRIPTCHECK_THREADS and MAX_SCRIPTCHECK_THREADS
#include <netbase.h>
#include <txdb.h> // for -dbcache defaults
#include <util/system.h>

#include <QApplication>
#include <QAbstractSpinBox>
#include <QDataWidgetMapper>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStringList>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFontMetrics>
#include <QSystemTrayIcon>
#include <QTabBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {
QString NormalizeThirdPartyExplorerUrl(QString url)
{
    url = url.trimmed();
    if (url.isEmpty()) return QString();
    if (url.contains(QStringLiteral("%s"))) return url;

    QUrl parsed(url);
    if (!parsed.isValid() || parsed.scheme().isEmpty() || parsed.host().isEmpty()) return QString();
    if (parsed.host().contains(QStringLiteral("blockchair.com"), Qt::CaseInsensitive)) {
        return QStringLiteral("https://blockchair.com/litecoin/transaction/%s");
    }

    QString normalized = url;
    if (normalized.endsWith(QLatin1Char('/'))) {
        normalized.chop(1);
    }
    return normalized + QStringLiteral("/tx/%s");
}

void SetTextResizeControlsVisible(bool visible)
{
    const QStringList control_names{
        QStringLiteral("mainTextControls"),
        QStringLiteral("nodeTextControls"),
        QStringLiteral("nodeTextControlPair")
    };
    for (QWidget* top_level : QApplication::topLevelWidgets()) {
        if (!top_level) continue;
        for (const QString& name : control_names) {
            const QList<QWidget*> controls = top_level->findChildren<QWidget*>(name);
            for (QWidget* control : controls) {
                if (control) control->setVisible(visible);
            }
        }
    }
}
} // namespace

OptionsDialog::OptionsDialog(QWidget *parent, bool enableWallet) :
    QDialog(parent),
    ui(new Ui::OptionsDialog),
    model(nullptr),
    mapper(nullptr)
{
    ui->setupUi(this);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setMinimumSize(700, 660);
    resize(740, 700);
    if (QTabBar* tab_bar = ui->tabWidget->tabBar()) {
        tab_bar->setElideMode(Qt::ElideNone);
        tab_bar->setUsesScrollButtons(false);
        tab_bar->setStyleSheet(QStringLiteral("QTabBar::tab { min-width: 112px; }"));
    }
    ui->frame->setMinimumHeight(76);
    ui->frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    ui->overriddenByCommandLineInfoLabel->setWordWrap(true);

    /* Main elements init */
    ui->databaseCache->setMinimum(nMinDbCache);
    ui->databaseCache->setMaximum(nMaxDbCache);
    ui->threadsScriptVerif->setMinimum(-GetNumCores());
    ui->threadsScriptVerif->setMaximum(MAX_SCRIPTCHECK_THREADS);
    for (QSpinBox* spin_box : {ui->pruneSize, ui->databaseCache, ui->threadsScriptVerif}) {
        spin_box->setMinimumWidth(132);
        spin_box->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin_box->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    ui->pruneWarning->setVisible(false);
    ui->pruneWarning->setStyleSheet("QLabel { color: red; }");

    ui->pruneSize->setEnabled(false);
    connect(ui->prune, &QPushButton::toggled, ui->pruneSize, &QWidget::setEnabled);

    QPushButton* apply_button = new QPushButton(tr("&Apply"), this);
    apply_button->setObjectName(QStringLiteral("applyButton"));
    apply_button->setAutoDefault(false);
    apply_button->setToolTip(tr("Apply Preferences changes without closing this window."));
    ui->horizontalLayout->addWidget(apply_button);
    const int dialog_button_height = std::max(32, QFontMetrics(font()).height() + 14);
    for (QPushButton* button : {ui->okButton, ui->cancelButton, apply_button}) {
        if (!button) continue;
        button->setMinimumHeight(dialog_button_height);
        button->setMaximumHeight(dialog_button_height);
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
    connect(apply_button, &QPushButton::clicked, this, [this] {
        if (!validateThirdPartyTxUrlSettings(true)) return;
        if (mapper) mapper->submit();
        if (thirdPartyTxUrlsEnabled) QSettings().setValue(QStringLiteral("ThirdPartyTxUrlsEnabled"), thirdPartyTxUrlsEnabled->isChecked());
        updateDefaultProxyNets();
        clearStatusLabel();
    });

    QGroupBox* disabled_litecoin_features = new QGroupBox(tr("Unavailable Litecoin Core Features"), this);
    disabled_litecoin_features->setObjectName(QStringLiteral("disabledLitecoinFeatures"));
    disabled_litecoin_features->setToolTip(tr("These inherited Litecoin Core features remain unavailable for Defcoin Core in this build. They are shown here as a reminder and cannot be changed in Preferences."));
    QFont disabled_features_font = disabled_litecoin_features->font();
    const QFont disabled_features_body_font = font();
    disabled_features_font.setPointSize(disabled_features_font.pointSize() + 1);
    disabled_features_font.setBold(true);
    disabled_litecoin_features->setFont(disabled_features_font);
    QVBoxLayout* disabled_litecoin_features_layout = new QVBoxLayout(disabled_litecoin_features);
    disabled_litecoin_features_layout->setContentsMargins(14, 18, 14, 12);
    disabled_litecoin_features_layout->setSpacing(7);
    QLabel* disabled_features_note = new QLabel(tr("All features listed below are currently unavailable in this version of Defcoin Core."), disabled_litecoin_features);
    disabled_features_note->setObjectName(QStringLiteral("disabledFeatureNote"));
    disabled_features_note->setFont(disabled_features_body_font);
    disabled_features_note->setWordWrap(true);
    disabled_features_note->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    disabled_litecoin_features_layout->addWidget(disabled_features_note);
    const QString disabled_feature_tooltip = tr("This inherited Litecoin Core feature is currently unavailable for Defcoin Core in this build.");
    const int feature_row_height = QFontMetrics(disabled_features_body_font).height() + 7;
    const QStringList disabled_feature_names{
        tr("BIP65/CLTV"),
        tr("BIP66 strict DER signatures"),
        tr("CSV"),
        tr("SegWit activation"),
        tr("Taproot"),
        tr("MWEB"),
        tr("Signet")
    };
    for (const QString& feature_name : disabled_feature_names) {
        QLabel* feature_label = new QLabel(QStringLiteral("- %1").arg(feature_name), disabled_litecoin_features);
        feature_label->setObjectName(QStringLiteral("disabledFeatureItem"));
        feature_label->setFont(disabled_features_body_font);
        feature_label->setMinimumHeight(feature_row_height);
        feature_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        feature_label->setToolTip(disabled_feature_tooltip);
        disabled_litecoin_features_layout->addWidget(feature_label);
    }
    ui->verticalLayout_Main->insertSpacing(ui->verticalLayout_Main->count() - 1, 14);
    ui->verticalLayout_Main->insertWidget(ui->verticalLayout_Main->count() - 1, disabled_litecoin_features);

    /* Network elements init */
#ifndef USE_UPNP
    ui->mapPortUpnp->setEnabled(false);
    ui->mapPortUpnp->setToolTip(tr("UPnP support was not compiled into this build. Rebuild with miniupnpc support to enable automatic router port mapping."));
#endif

    ui->proxyIp->setEnabled(false);
    ui->proxyPort->setEnabled(false);
    ui->proxyPort->setValidator(new QIntValidator(1, 65535, this));

    ui->proxyIpTor->setEnabled(false);
    ui->proxyPortTor->setEnabled(false);
    ui->proxyPortTor->setValidator(new QIntValidator(1, 65535, this));

    connect(ui->connectSocks, &QPushButton::toggled, ui->proxyIp, &QWidget::setEnabled);
    connect(ui->connectSocks, &QPushButton::toggled, ui->proxyPort, &QWidget::setEnabled);
    connect(ui->connectSocks, &QPushButton::toggled, this, &OptionsDialog::updateProxyValidationState);

    connect(ui->connectSocksTor, &QPushButton::toggled, ui->proxyIpTor, &QWidget::setEnabled);
    connect(ui->connectSocksTor, &QPushButton::toggled, ui->proxyPortTor, &QWidget::setEnabled);
    connect(ui->connectSocksTor, &QPushButton::toggled, this, &OptionsDialog::updateProxyValidationState);
    QCheckBox* show_local_node = new QCheckBox(tr("Show local node in Peers list and peer stats"), this);
    show_local_node->setToolTip(tr("When enabled, the Peers page includes this running wallet/node as a local row and counts it in peer summary statistics."));
    show_local_node->setChecked(QSettings().value(QStringLiteral("ShowLocalNodeInPeers"), true).toBool());
    ui->verticalLayout_Network->insertWidget(0, show_local_node);
    connect(show_local_node, &QCheckBox::toggled, this, [](bool checked) {
        QSettings().setValue(QStringLiteral("ShowLocalNodeInPeers"), checked);
    });

    QCheckBox* discover_local_nodes = new QCheckBox(tr("Discover local network nodes"), this);
    discover_local_nodes->setToolTip(tr("Probe the local network for Defcoin nodes. Leave this off to avoid macOS Local Network permission prompts unless you intentionally want LAN peer discovery."));
    discover_local_nodes->setChecked(QSettings().value(QStringLiteral("DiscoverLocalNetworkNodes"), true).toBool());
    ui->verticalLayout_Network->insertWidget(1, discover_local_nodes);
    connect(discover_local_nodes, &QCheckBox::toggled, this, [](bool checked) {
        QSettings().setValue(QStringLiteral("DiscoverLocalNetworkNodes"), checked);
    });

    QCheckBox* only_defcoin_user_agents = new QCheckBox(tr("Only accept Defcoin prefixed User Agents"), this);
    only_defcoin_user_agents->setToolTip(tr("Disconnect peers whose advertised user agent does not start with Defcoin, ignoring case. This can hide Litecoin or unrelated legacy peers on mixed networks."));
    const bool only_defcoin_user_agents_overridden = gArgs.IsArgSet("-onlydefcoinua");
    const bool only_defcoin_user_agents_enabled = only_defcoin_user_agents_overridden ?
        gArgs.GetBoolArg("-onlydefcoinua", DEFAULT_DEFCOIN_USER_AGENT_FILTER) :
        QSettings().value(QStringLiteral("OnlyDefcoinUserAgents"), DEFAULT_DEFCOIN_USER_AGENT_FILTER).toBool();
    only_defcoin_user_agents->setChecked(only_defcoin_user_agents_enabled);
    only_defcoin_user_agents->setEnabled(!only_defcoin_user_agents_overridden);
    if (only_defcoin_user_agents_overridden) {
        only_defcoin_user_agents->setToolTip(tr("This setting is currently controlled by the -onlydefcoinua startup option."));
    }
    SetOnlyDefcoinUserAgents(only_defcoin_user_agents_enabled);
    ui->verticalLayout_Network->insertWidget(2, only_defcoin_user_agents);
    connect(only_defcoin_user_agents, &QCheckBox::toggled, this, [](bool checked) {
        QSettings().setValue(QStringLiteral("OnlyDefcoinUserAgents"), checked);
        SetOnlyDefcoinUserAgents(checked);
    });

#if ENABLE_DEFCOIN_FUN_UI
    QHBoxLayout* peer_map_zoom_in_layout = new QHBoxLayout();
    QLabel* peer_map_zoom_in_label = new QLabel(tr("Peers Map maximum close zoom:"), this);
    QDoubleSpinBox* peer_map_zoom_in = new QDoubleSpinBox(this);
    peer_map_zoom_in->setRange(1.15, 5.00);
    peer_map_zoom_in->setSingleStep(0.05);
    peer_map_zoom_in->setDecimals(2);
    peer_map_zoom_in->setValue(QSettings().value(QStringLiteral("PeerMapZoomedInMax"), 5.00).toDouble());
    peer_map_zoom_in->setToolTip(tr("Default close-up zoom factor used by the Peers Map travel animation. Temporary keypad zoom changes reset when the Node window closes."));
    peer_map_zoom_in_label->setBuddy(peer_map_zoom_in);
    peer_map_zoom_in_layout->addWidget(peer_map_zoom_in_label);
    peer_map_zoom_in_layout->addWidget(peer_map_zoom_in);
    ui->verticalLayout_Display->insertLayout(ui->verticalLayout_Display->count() - 1, peer_map_zoom_in_layout);
    connect(peer_map_zoom_in, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [](double value) {
        QSettings().setValue(QStringLiteral("PeerMapZoomedInMax"), value);
    });

    QHBoxLayout* peer_map_zoom_out_layout = new QHBoxLayout();
    QLabel* peer_map_zoom_out_label = new QLabel(tr("Peers Map maximum wide zoom:"), this);
    QDoubleSpinBox* peer_map_zoom_out = new QDoubleSpinBox(this);
    peer_map_zoom_out->setRange(0.72, 2.20);
    peer_map_zoom_out->setSingleStep(0.05);
    peer_map_zoom_out->setDecimals(2);
    peer_map_zoom_out->setValue(QSettings().value(QStringLiteral("PeerMapZoomedOutMax"), 1.55).toDouble());
    peer_map_zoom_out->setToolTip(tr("Default wide-view zoom factor used between cities by the Peers Map travel animation. Temporary keypad zoom changes reset when the Node window closes."));
    peer_map_zoom_out_label->setBuddy(peer_map_zoom_out);
    peer_map_zoom_out_layout->addWidget(peer_map_zoom_out_label);
    peer_map_zoom_out_layout->addWidget(peer_map_zoom_out);
    ui->verticalLayout_Display->insertLayout(ui->verticalLayout_Display->count() - 1, peer_map_zoom_out_layout);
    connect(peer_map_zoom_out, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [](double value) {
        QSettings().setValue(QStringLiteral("PeerMapZoomedOutMax"), value);
    });
#endif

    /* Window elements init */
#ifdef Q_OS_MAC
    /* remove Window tab on Mac */
    ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabWindow));
    /* hide launch at startup option on macOS */
    ui->bitcoinAtStartup->setVisible(false);
    ui->verticalLayout_Main->removeWidget(ui->bitcoinAtStartup);
    ui->verticalLayout_Main->removeItem(ui->horizontalSpacer_0_Main);
#endif

    /* remove Wallet tab and 3rd party-URL textbox in case of -disablewallet */
    if (!enableWallet) {
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabWallet));
        ui->thirdPartyTxUrlsLabel->setVisible(false);
        ui->thirdPartyTxUrls->setVisible(false);
    }

    /* Display elements init */
    QDir translations(":translations");

    ui->bitcoinAtStartup->setToolTip(ui->bitcoinAtStartup->toolTip().arg(PACKAGE_NAME));
    ui->bitcoinAtStartup->setText(ui->bitcoinAtStartup->text().arg(PACKAGE_NAME));

    ui->openBitcoinConfButton->setToolTip(ui->openBitcoinConfButton->toolTip().arg(PACKAGE_NAME));

    ui->lang->setToolTip(ui->lang->toolTip().arg(PACKAGE_NAME));
    ui->lang->addItem(QString("(") + tr("default") + QString(")"), QVariant(""));
    const QStringList available_translations = translations.entryList(QDir::Files, QDir::Name);
    const QStringList complete_translations{
        QStringLiteral("en"),
        QStringLiteral("en_GB")
    };
    for (const QString &langStr : available_translations)
    {
        if (langStr.trimmed().isEmpty()) continue;
        if (!complete_translations.contains(langStr)) continue;
        QLocale locale(langStr);
        const QString native_language = locale.nativeLanguageName();
        if (native_language.trimmed().isEmpty()) continue;

        /** check if the locale name consists of 2 parts (language_country) */
        if(langStr.contains("_"))
        {
            /** display language strings as "native language - native country (locale name)", e.g. "Deutsch - Deutschland (de)" */
            ui->lang->addItem(native_language + QString(" - ") + locale.nativeCountryName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
        }
        else
        {
            /** display language strings as "native language (locale name)", e.g. "Deutsch (de)" */
            ui->lang->addItem(native_language + QString(" (") + langStr + QString(")"), QVariant(langStr));
        }
    }
    ui->unit->setModel(new BitcoinUnits(this));
    thirdPartyTxUrlsEnabled = new QCheckBox(tr("Enable third party transaction URLs"), this);
    thirdPartyTxUrlsEnabled->setToolTip(tr("Show block explorer links for transaction IDs and wallet addresses when a valid explorer URL is configured."));
    thirdPartyTxUrlsEnabled->setVisible(enableWallet);
    ui->verticalLayout_Display->insertWidget(3, thirdPartyTxUrlsEnabled);
    thirdPartyTxUrlPresets = new QComboBox(this);
    thirdPartyTxUrlPresets->setObjectName(QStringLiteral("thirdPartyTxUrlPresets"));
    thirdPartyTxUrlPresets->setToolTip(tr("Choose a known Defcoin block explorer transaction URL format. Selecting one fills the transaction URL field."));
    thirdPartyTxUrlPresets->addItem(tr("Explorer preset..."), QVariant(QString()));
    thirdPartyTxUrlPresets->addItem(tr("DC903 Explorer"), QVariant(QStringLiteral("https://defcoin.dc903.org/explorer/tx/%s")));
    thirdPartyTxUrlPresets->addItem(tr("DC903 Legacy Explorer"), QVariant(QStringLiteral("https://defcoin.dc903.org/legacyexplorer/tx/%s")));
    const QString configured_blockexplorer = NormalizeThirdPartyExplorerUrl(QString::fromStdString(gArgs.GetArg("-blockexplorer", "")));
    if (!configured_blockexplorer.isEmpty()) {
        thirdPartyTxUrlPresets->addItem(tr("Configured blockexplorer"), QVariant(configured_blockexplorer));
    }
    thirdPartyTxUrlPresets->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    thirdPartyTxUrlPresets->setMinimumWidth(GUIUtil::TextWidth(QFontMetrics(thirdPartyTxUrlPresets->font()), tr("DC903 Legacy Explorer")) + 46);
    ui->horizontalLayout_3_Display->addWidget(thirdPartyTxUrlPresets);
    thirdPartyTxUrlPresets->setVisible(enableWallet);
    thirdPartyTxUrlError = new QLabel(this);
    thirdPartyTxUrlError->setObjectName(QStringLiteral("thirdPartyTxUrlError"));
    thirdPartyTxUrlError->setStyleSheet(QStringLiteral("QLabel { color: #C62828; }"));
    thirdPartyTxUrlError->setWordWrap(true);
    thirdPartyTxUrlError->hide();
    ui->verticalLayout_Display->insertWidget(5, thirdPartyTxUrlError);
    connect(thirdPartyTxUrlPresets, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        const QString url = thirdPartyTxUrlPresets->itemData(index).toString();
        if (url.isEmpty()) {
            if (thirdPartyTxUrlsEnabled) thirdPartyTxUrlsEnabled->setChecked(false);
            ui->thirdPartyTxUrls->clear();
            if (thirdPartyTxUrlError) thirdPartyTxUrlError->hide();
        } else {
            if (thirdPartyTxUrlsEnabled) thirdPartyTxUrlsEnabled->setChecked(true);
            ui->thirdPartyTxUrls->setText(url);
        }
        validateThirdPartyTxUrlSettings(false);
    });
    connect(ui->thirdPartyTxUrls, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (thirdPartyTxUrlsEnabled) {
            thirdPartyTxUrlsEnabled->setChecked(!text.trimmed().isEmpty());
        }
        validateThirdPartyTxUrlSettings(false);
    });
    connect(thirdPartyTxUrlsEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
        if (!enabled) {
            QSignalBlocker url_blocker(ui->thirdPartyTxUrls);
            ui->thirdPartyTxUrls->clear();
            if (thirdPartyTxUrlPresets) {
                QSignalBlocker preset_blocker(thirdPartyTxUrlPresets);
                thirdPartyTxUrlPresets->setCurrentIndex(0);
            }
            if (thirdPartyTxUrlError) thirdPartyTxUrlError->hide();
        }
        updateThirdPartyTxUrlState();
    });
    if (enableWallet) {
        ui->unitLabel->setText(tr("Default currency unit:"));
        ui->unitLabel->setToolTip(tr("Choose the default unit used when showing Defcoin amounts."));
        ui->unit->setToolTip(tr("Choose the default unit used when showing Defcoin amounts."));
        ui->horizontalLayout_2_Display->removeWidget(ui->unitLabel);
        ui->horizontalLayout_2_Display->removeWidget(ui->unit);
        ui->verticalLayout_Display->removeItem(ui->horizontalLayout_2_Display);
        QHBoxLayout* wallet_unit_layout = new QHBoxLayout();
        wallet_unit_layout->setObjectName(QStringLiteral("walletUnitLayout"));
        wallet_unit_layout->setContentsMargins(0, 0, 0, 0);
        wallet_unit_layout->addWidget(ui->unitLabel);
        wallet_unit_layout->addWidget(ui->unit);
        ui->verticalLayout_2->insertLayout(0, wallet_unit_layout);
    }
    for (const GUIUtil::ThemeDescriptor& theme : GUIUtil::availableAppearanceThemes()) {
        ui->theme->addItem(theme.name, QVariant(theme.id));
    }
    QComboBox* display_combos[] = {ui->lang, ui->theme, ui->unit, thirdPartyTxUrlPresets};
    for (QComboBox* combo : display_combos) {
        combo->setMinimumHeight(31);
        combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        combo->setEnabled(true);
        GUIUtil::PolishComboBox(combo);
    }
    QHBoxLayout* text_size_layout = new QHBoxLayout();
    text_size_layout->setContentsMargins(0, 0, 0, 0);
    text_size_layout->setSpacing(12);
    QLabel* text_size_label = new QLabel(tr("Text sizing:"), this);
    text_size_label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    QCheckBox* link_text_sizes = new QCheckBox(tr("Keep main and node window text sizes linked"), this);
    link_text_sizes->setToolTip(tr("When enabled, changing the text size in one major wallet window applies the same step change to the other major wallet windows."));
    link_text_sizes->setMinimumWidth(GUIUtil::TextWidth(QFontMetrics(link_text_sizes->font()), link_text_sizes->text()) + 54);
    link_text_sizes->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    QCheckBox* hide_text_resize_controls = new QCheckBox(tr("Hide text resizing buttons"), this);
    hide_text_resize_controls->setToolTip(tr("Hide the on-screen A-/A+ text sizing controls. Text size can still be reset here in Preferences."));
    QPushButton* reset_text_sizes = new QPushButton(tr("Reset Text Sizes"), this);
    reset_text_sizes->setToolTip(tr("Reset all Main, Node, peer table, Console, Ping, and Trace Route text sizes to their defaults."));
    QSettings text_settings;
    link_text_sizes->setChecked(text_settings.value(QStringLiteral("LinkWalletTextSizes"), true).toBool());
    hide_text_resize_controls->setChecked(text_settings.value(QStringLiteral("HideTextResizeControls"), false).toBool());
    text_size_layout->addWidget(text_size_label);
    text_size_layout->addWidget(link_text_sizes);
    text_size_layout->addWidget(hide_text_resize_controls);
    text_size_layout->addWidget(reset_text_sizes);
    text_size_layout->addStretch();
    ui->verticalLayout_Display->insertLayout(ui->verticalLayout_Display->count() - 1, text_size_layout);
    QHBoxLayout* display_privacy_layout = new QHBoxLayout();
    display_privacy_layout->setContentsMargins(0, 0, 0, 0);
    display_privacy_layout->setSpacing(12);
    QCheckBox* mask_values = new QCheckBox(tr("Mask values on Overview"), this);
    mask_values->setToolTip(tr("Hide wallet balances and amount values on the Overview page when privacy mode is enabled. This is useful when sharing your screen or working in public."));
    mask_values->setChecked(text_settings.value(QStringLiteral("MaskValues"), false).toBool());
    display_privacy_layout->addWidget(mask_values);
    display_privacy_layout->addStretch();
    ui->verticalLayout_Display->insertLayout(ui->verticalLayout_Display->count() - 1, display_privacy_layout);
    connect(link_text_sizes, &QCheckBox::toggled, this, [](bool checked) {
        QSettings().setValue(QStringLiteral("LinkWalletTextSizes"), checked);
    });
    connect(hide_text_resize_controls, &QCheckBox::toggled, this, [](bool checked) {
        QSettings().setValue(QStringLiteral("HideTextResizeControls"), checked);
        SetTextResizeControlsVisible(!checked);
    });
    connect(mask_values, &QCheckBox::toggled, this, [](bool checked) {
        QSettings().setValue(QStringLiteral("MaskValues"), checked);
        for (QWidget* top_level : QApplication::topLevelWidgets()) {
            if (!top_level) continue;
            if (top_level->metaObject()->indexOfMethod("applyPrivacySetting(bool)") >= 0) {
                QMetaObject::invokeMethod(top_level, "applyPrivacySetting", Qt::QueuedConnection, Q_ARG(bool, checked));
            }
        }
    });
    connect(reset_text_sizes, &QPushButton::clicked, this, [] {
        QSettings settings;
        settings.remove(QStringLiteral("consoleFontSize"));
        settings.remove(QStringLiteral("nodeWindowFontSize"));
        settings.remove(QStringLiteral("connectedPeersPanelFontSize"));
        settings.remove(QStringLiteral("mainWindowFontSize"));
        settings.remove(QStringLiteral("pingToolFontSize"));
        settings.remove(QStringLiteral("traceRouteFontSize"));
        settings.sync();
        for (QWidget* top_level : QApplication::topLevelWidgets()) {
            if (!top_level) continue;
            if (top_level->metaObject()->indexOfMethod("refreshTextSizeSettings()") >= 0) {
                QMetaObject::invokeMethod(top_level, "refreshTextSizeSettings", Qt::QueuedConnection);
            }
        }
    });

    /* Widget-to-option mapper */
    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    mapper->setOrientation(Qt::Vertical);

    GUIUtil::ItemDelegate* delegate = new GUIUtil::ItemDelegate(mapper);
    connect(delegate, &GUIUtil::ItemDelegate::keyEscapePressed, this, &OptionsDialog::reject);
    mapper->setItemDelegate(delegate);

    /* setup/change UI elements when proxy IPs are invalid/valid */
    ui->proxyIp->setCheckValidator(new ProxyAddressValidator(parent));
    ui->proxyIpTor->setCheckValidator(new ProxyAddressValidator(parent));
    connect(ui->proxyIp, &QValidatedLineEdit::validationDidChange, this, &OptionsDialog::updateProxyValidationState);
    connect(ui->proxyIpTor, &QValidatedLineEdit::validationDidChange, this, &OptionsDialog::updateProxyValidationState);
    connect(ui->proxyPort, &QLineEdit::textChanged, this, &OptionsDialog::updateProxyValidationState);
    connect(ui->proxyPortTor, &QLineEdit::textChanged, this, &OptionsDialog::updateProxyValidationState);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        ui->hideTrayIcon->setChecked(true);
        ui->hideTrayIcon->setEnabled(false);
        ui->minimizeToTray->setChecked(false);
        ui->minimizeToTray->setEnabled(false);
    }

    if (!gArgs.IsArgSet("-debug")) {
        ui->mwebFeatures->setVisible(false);
    }

    GUIUtil::handleCloseWindowShortcut(this);
}

OptionsDialog::~OptionsDialog()
{
    delete ui;
}

void OptionsDialog::updateThirdPartyTxUrlState()
{
    const bool enabled = thirdPartyTxUrlsEnabled && thirdPartyTxUrlsEnabled->isChecked();
    ui->thirdPartyTxUrlsLabel->setEnabled(enabled);
    ui->thirdPartyTxUrls->setEnabled(enabled);
    // isVisible() is false while the dialog is still being constructed, which
    // left this preset menu disabled until the checkbox was toggled manually.
    if (thirdPartyTxUrlPresets) thirdPartyTxUrlPresets->setEnabled(!thirdPartyTxUrlPresets->isHidden());
}

bool OptionsDialog::validateThirdPartyTxUrlSettings(bool final_validation)
{
    if (!thirdPartyTxUrlsEnabled) return true;

    const bool enabled = thirdPartyTxUrlsEnabled->isChecked();
    QString error;
    const QString text = ui->thirdPartyTxUrls->text().trimmed();

    if (!enabled) {
        QSignalBlocker url_blocker(ui->thirdPartyTxUrls);
        ui->thirdPartyTxUrls->clear();
        if (thirdPartyTxUrlPresets) {
            QSignalBlocker preset_blocker(thirdPartyTxUrlPresets);
            thirdPartyTxUrlPresets->setCurrentIndex(0);
        }
        if (thirdPartyTxUrlError) thirdPartyTxUrlError->hide();
        QSettings().setValue(QStringLiteral("ThirdPartyTxUrlsEnabled"), false);
        updateThirdPartyTxUrlState();
        return true;
    }

    if (text.isEmpty()) {
        if (final_validation || !enabled) {
            thirdPartyTxUrlsEnabled->setChecked(false);
            QSettings().setValue(QStringLiteral("ThirdPartyTxUrlsEnabled"), false);
            if (thirdPartyTxUrlError) thirdPartyTxUrlError->hide();
            updateThirdPartyTxUrlState();
            return true;
        }
        error.clear();
    } else {
        const QStringList urls = text.split(QLatin1Char('|'), QString::SkipEmptyParts);
        for (const QString& raw_url : urls) {
            const QString url_text = raw_url.trimmed();
            QString validation_url_text = url_text;
            validation_url_text.replace(QStringLiteral("%s"), QStringLiteral("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
            const QUrl url(validation_url_text, QUrl::StrictMode);
            if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty() || !url_text.contains(QStringLiteral("%s"))) {
                error = tr("Enter a valid URL containing %s, for example https://example.com/tx/%s");
                break;
            }
        }
    }

    if (thirdPartyTxUrlError) {
        thirdPartyTxUrlError->setText(error);
        thirdPartyTxUrlError->setVisible(!error.isEmpty());
    }
    if (final_validation && !error.isEmpty()) {
        ui->tabWidget->setCurrentWidget(ui->tabDisplay);
        ui->thirdPartyTxUrls->setFocus();
        return false;
    }
    QSettings().setValue(QStringLiteral("ThirdPartyTxUrlsEnabled"), thirdPartyTxUrlsEnabled->isChecked() && error.isEmpty() && !text.isEmpty());
    updateThirdPartyTxUrlState();
    return true;
}

void OptionsDialog::setModel(OptionsModel *_model)
{
    this->model = _model;

    if(_model)
    {
        /* check if client restart is needed and show persistent message */
        if (_model->isRestartRequired())
            showRestartWarning(true);

        // Prune values are in GB to be consistent with intro.cpp
        static constexpr uint64_t nMinDiskSpace = (MIN_DISK_SPACE_FOR_BLOCK_FILES / GB_BYTES) + (MIN_DISK_SPACE_FOR_BLOCK_FILES % GB_BYTES) ? 1 : 0;
        ui->pruneSize->setRange(nMinDiskSpace, std::numeric_limits<int>::max());

        QString strLabel = _model->getOverriddenByCommandLine();
        if (strLabel.isEmpty())
            strLabel = tr("none");
        ui->overriddenByCommandLineLabel->setText(strLabel);

        mapper->setModel(_model);
        setMapper();
        mapper->toFirst();
        if (thirdPartyTxUrlsEnabled) {
            const QString url = ui->thirdPartyTxUrls->text().trimmed();
            thirdPartyTxUrlsEnabled->setChecked(QSettings().value(QStringLiteral("ThirdPartyTxUrlsEnabled"), !url.isEmpty()).toBool() && !url.isEmpty());
            if (thirdPartyTxUrlPresets) {
                int matching_index = 0;
                for (int i = 1; i < thirdPartyTxUrlPresets->count(); ++i) {
                    if (thirdPartyTxUrlPresets->itemData(i).toString() == url) {
                        matching_index = i;
                        break;
                    }
                }
                QSignalBlocker blocker(thirdPartyTxUrlPresets);
                thirdPartyTxUrlPresets->setCurrentIndex(matching_index);
            }
            updateThirdPartyTxUrlState();
            validateThirdPartyTxUrlSettings(false);
        }
        togglePruneWarning(ui->prune->isChecked());

        updateDefaultProxyNets();
    }

    /* warn when one of the following settings changes by user action (placed here so init via mapper doesn't trigger them) */

    /* Main */
    connect(ui->prune, &QCheckBox::clicked, this, &OptionsDialog::showRestartWarning);
    connect(ui->prune, &QCheckBox::toggled, this, &OptionsDialog::togglePruneWarning);
    connect(ui->pruneSize, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &OptionsDialog::showRestartWarning);
    connect(ui->databaseCache, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &OptionsDialog::showRestartWarning);
    connect(ui->threadsScriptVerif, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &OptionsDialog::showRestartWarning);
    /* Wallet */
    connect(ui->spendZeroConfChange, &QCheckBox::clicked, this, &OptionsDialog::showRestartWarning);
    /* Network */
    connect(ui->allowIncoming, &QCheckBox::clicked, this, &OptionsDialog::showRestartWarning);
    connect(ui->connectSocks, &QCheckBox::clicked, this, &OptionsDialog::showRestartWarning);
    connect(ui->connectSocksTor, &QCheckBox::clicked, this, &OptionsDialog::showRestartWarning);
    /* Display */
    connect(ui->lang, static_cast<void (QValueComboBox::*)()>(&QValueComboBox::valueChanged), [this]{ showRestartWarning(); });
}

void OptionsDialog::setCurrentTab(OptionsDialog::Tab tab)
{
    QWidget *tab_widget = nullptr;
    if (tab == OptionsDialog::Tab::TAB_MAIN) tab_widget = ui->tabMain;
    if (tab == OptionsDialog::Tab::TAB_WALLET) tab_widget = ui->tabWallet;
    if (tab == OptionsDialog::Tab::TAB_NETWORK) tab_widget = ui->tabNetwork;
    if (tab == OptionsDialog::Tab::TAB_DISPLAY) tab_widget = ui->tabDisplay;
    if (tab_widget && ui->tabWidget->currentWidget() != tab_widget) {
        ui->tabWidget->setCurrentWidget(tab_widget);
    }
}

void OptionsDialog::setMapper()
{
    /* Main */
    mapper->addMapping(ui->bitcoinAtStartup, OptionsModel::StartAtStartup);
    mapper->addMapping(ui->threadsScriptVerif, OptionsModel::ThreadsScriptVerif);
    mapper->addMapping(ui->databaseCache, OptionsModel::DatabaseCache);
    mapper->addMapping(ui->prune, OptionsModel::Prune);
    mapper->addMapping(ui->pruneSize, OptionsModel::PruneSize);

    /* Wallet */
    mapper->addMapping(ui->spendZeroConfChange, OptionsModel::SpendZeroConfChange);
    mapper->addMapping(ui->coinControlFeatures, OptionsModel::CoinControlFeatures);
    mapper->addMapping(ui->mwebFeatures, OptionsModel::MWEBFeatures);

    /* Network */
    mapper->addMapping(ui->mapPortUpnp, OptionsModel::MapPortUPnP);
    mapper->addMapping(ui->allowIncoming, OptionsModel::Listen);

    mapper->addMapping(ui->connectSocks, OptionsModel::ProxyUse);
    mapper->addMapping(ui->proxyIp, OptionsModel::ProxyIP);
    mapper->addMapping(ui->proxyPort, OptionsModel::ProxyPort);

    mapper->addMapping(ui->connectSocksTor, OptionsModel::ProxyUseTor);
    mapper->addMapping(ui->proxyIpTor, OptionsModel::ProxyIPTor);
    mapper->addMapping(ui->proxyPortTor, OptionsModel::ProxyPortTor);

    /* Window */
#ifndef Q_OS_MAC
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        mapper->addMapping(ui->hideTrayIcon, OptionsModel::HideTrayIcon);
        mapper->addMapping(ui->minimizeToTray, OptionsModel::MinimizeToTray);
    }
    mapper->addMapping(ui->minimizeOnClose, OptionsModel::MinimizeOnClose);
#endif

    /* Display */
    mapper->addMapping(ui->lang, OptionsModel::Language);
    mapper->addMapping(ui->theme, OptionsModel::AppearanceTheme);
    mapper->addMapping(ui->unit, OptionsModel::DisplayUnit);
    mapper->addMapping(ui->thirdPartyTxUrls, OptionsModel::ThirdPartyTxUrls);
}

void OptionsDialog::setOkButtonState(bool fState)
{
    ui->okButton->setEnabled(fState);
}

void OptionsDialog::on_resetButton_clicked()
{
    if(model)
    {
        // confirmation dialog
        QMessageBox::StandardButton btnRetVal = QMessageBox::question(this, tr("Confirm options reset"),
            tr("Client restart required to activate changes.") + "<br><br>" + tr("Client will be shut down. Do you want to proceed?"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

        if(btnRetVal == QMessageBox::Cancel)
            return;

        /* reset all options and close GUI */
        model->Reset();
        QApplication::quit();
    }
}

void OptionsDialog::on_openBitcoinConfButton_clicked()
{
    /* explain the purpose of the config file */
    QMessageBox::information(this, tr("Configuration options"),
        tr("The configuration file is used to specify advanced user options which override GUI settings. "
           "Additionally, any command-line options will override this configuration file."));

    /* show an error if there was some problem opening the file */
    if (!GUIUtil::openBitcoinConf())
        QMessageBox::critical(this, tr("Error"), tr("The configuration file could not be opened."));
}

void OptionsDialog::on_okButton_clicked()
{
    if (!validateThirdPartyTxUrlSettings(true)) return;
    mapper->submit();
    if (thirdPartyTxUrlsEnabled) QSettings().setValue(QStringLiteral("ThirdPartyTxUrlsEnabled"), thirdPartyTxUrlsEnabled->isChecked());
    accept();
    updateDefaultProxyNets();
}

void OptionsDialog::on_cancelButton_clicked()
{
    reject();
}

void OptionsDialog::on_hideTrayIcon_stateChanged(int fState)
{
    if(fState)
    {
        ui->minimizeToTray->setChecked(false);
        ui->minimizeToTray->setEnabled(false);
    }
    else
    {
        ui->minimizeToTray->setEnabled(true);
    }
}

void OptionsDialog::togglePruneWarning(bool enabled)
{
    ui->pruneWarning->setVisible(enabled);
}

void OptionsDialog::showRestartWarning(bool fPersistent)
{
    ui->statusLabel->setStyleSheet("QLabel { color: red; }");

    if(fPersistent)
    {
        ui->statusLabel->setText(tr("Client restart required to activate changes."));
    }
    else
    {
        ui->statusLabel->setText(tr("This change would require a client restart."));
        // clear non-persistent status label after 10 seconds
        // Todo: should perhaps be a class attribute, if we extend the use of statusLabel
        QTimer::singleShot(10000, this, &OptionsDialog::clearStatusLabel);
    }
}

void OptionsDialog::clearStatusLabel()
{
    ui->statusLabel->clear();
    if (model && model->isRestartRequired()) {
        showRestartWarning(true);
    }
}

void OptionsDialog::updateProxyValidationState()
{
    QValidatedLineEdit *pUiProxyIp = ui->proxyIp;
    QValidatedLineEdit *otherProxyWidget = (pUiProxyIp == ui->proxyIpTor) ? ui->proxyIp : ui->proxyIpTor;
    if (pUiProxyIp->isValid() && (!ui->proxyPort->isEnabled() || ui->proxyPort->text().toInt() > 0) && (!ui->proxyPortTor->isEnabled() || ui->proxyPortTor->text().toInt() > 0))
    {
        setOkButtonState(otherProxyWidget->isValid()); //only enable ok button if both proxys are valid
        clearStatusLabel();
    }
    else
    {
        setOkButtonState(false);
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(tr("The supplied proxy address is invalid."));
    }
}

void OptionsDialog::updateDefaultProxyNets()
{
    proxyType proxy;
    std::string strProxy;
    QString strDefaultProxyGUI;

    model->node().getProxy(NET_IPV4, proxy);
    strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
    strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
    (strProxy == strDefaultProxyGUI.toStdString()) ? ui->proxyReachIPv4->setChecked(true) : ui->proxyReachIPv4->setChecked(false);

    model->node().getProxy(NET_IPV6, proxy);
    strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
    strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
    (strProxy == strDefaultProxyGUI.toStdString()) ? ui->proxyReachIPv6->setChecked(true) : ui->proxyReachIPv6->setChecked(false);

    model->node().getProxy(NET_ONION, proxy);
    strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
    strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
    (strProxy == strDefaultProxyGUI.toStdString()) ? ui->proxyReachTor->setChecked(true) : ui->proxyReachTor->setChecked(false);
}

ProxyAddressValidator::ProxyAddressValidator(QObject *parent) :
QValidator(parent)
{
}

QValidator::State ProxyAddressValidator::validate(QString &input, int &pos) const
{
    Q_UNUSED(pos);
    // Validate the proxy
    CService serv(LookupNumeric(input.toStdString(), DEFAULT_GUI_PROXY_PORT));
    proxyType addrProxy = proxyType(serv, true);
    if (addrProxy.IsValid())
        return QValidator::Acceptable;

    return QValidator::Invalid;
}
