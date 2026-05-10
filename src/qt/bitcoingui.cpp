// Copyright (c) 2011-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/bitcoingui.h>

#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/createwalletdialog.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/modaloverlay.h>
#include <qt/networkstyle.h>
#include <qt/notificator.h>
#include <qt/openuridialog.h>
#include <qt/optionsdialog.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/rpcconsole.h>
#include <qt/utilitydialog.h>

#ifdef ENABLE_WALLET
#include <qt/walletcontroller.h>
#include <qt/walletframe.h>
#include <qt/walletmodel.h>
#include <qt/walletview.h>
#endif // ENABLE_WALLET

#ifdef Q_OS_MAC
#include <qt/macdockiconhandler.h>
#endif

#include <chain.h>
#include <chainparams.h>
#include <clientversion.h>
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <node/ui_interface.h>
#include <util/system.h>
#include <util/translation.h>
#include <validation.h>

#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLayout>
#include <QList>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTableView>
#include <QTreeView>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWindow>

namespace {
void ApplyFontRecursively(QWidget* root, const QFont& font)
{
    if (!root) return;
    root->setFont(font);
    const QList<QWidget*> widgets = root->findChildren<QWidget*>();
    for (QWidget* widget : widgets) {
        if (!widget) continue;
        widget->setFont(font);
        if (QAbstractItemView* view = qobject_cast<QAbstractItemView*>(widget)) {
            QHeaderView* horizontal_header = nullptr;
            QHeaderView* vertical_header = nullptr;
            QTableView* table_view = nullptr;
            if (QTableView* table = qobject_cast<QTableView*>(view)) {
                table_view = table;
                horizontal_header = table->horizontalHeader();
                vertical_header = table->verticalHeader();
            } else if (QTreeView* tree = qobject_cast<QTreeView*>(view)) {
                horizontal_header = tree->header();
            }
            if (horizontal_header) {
                horizontal_header->setFont(font);
                horizontal_header->setMinimumHeight(QFontMetrics(font).height() + 12);
                horizontal_header->updateGeometry();
            }
            if (vertical_header) {
                vertical_header->setFont(font);
                vertical_header->setDefaultSectionSize(QFontMetrics(font).height() + 10);
                vertical_header->updateGeometry();
            }
            if (table_view && table_view->viewport()) {
                table_view->viewport()->update();
            }
            view->updateGeometry();
        }
        if (QLayout* layout = widget->layout()) {
            layout->invalidate();
        }
        widget->updateGeometry();
    }
    if (QLayout* layout = root->layout()) {
        layout->invalidate();
        layout->activate();
    }
    root->updateGeometry();
}
} // namespace


const std::string BitcoinGUI::DEFAULT_UIPLATFORM =
#if defined(Q_OS_MAC)
        "macosx"
#elif defined(Q_OS_WIN)
        "windows"
#else
        "other"
#endif
        ;

BitcoinGUI::BitcoinGUI(interfaces::Node& node, const PlatformStyle *_platformStyle, const NetworkStyle *networkStyle, QWidget *parent) :
    QMainWindow(parent),
    m_node(node),
    trayIconMenu{new QMenu()},
    platformStyle(_platformStyle),
    m_network_style(networkStyle)
{
    QSettings settings;
    const bool restored_geometry = restoreGeometry(settings.value("MainWindowGeometry").toByteArray());
    if (!restored_geometry) {
        // Restore failed (perhaps missing setting), center the window
        move(QGuiApplication::primaryScreen()->availableGeometry().center() - frameGeometry().center());
    }

#ifdef ENABLE_WALLET
    enableWallet = WalletModel::isWalletEnabled();
#endif // ENABLE_WALLET
    QApplication::setWindowIcon(m_network_style->getTrayAndWindowIcon());
    setWindowIcon(m_network_style->getTrayAndWindowIcon());
    updateWindowTitle();

    rpcConsole = new RPCConsole(node, _platformStyle, nullptr);
    helpMessageDialog = new HelpMessageDialog(this, false);
#ifdef ENABLE_WALLET
    if(enableWallet)
    {
        /** Create wallet frame and make it the central widget */
        walletFrame = new WalletFrame(_platformStyle, this);
        setCentralWidget(walletFrame);
    } else
#endif // ENABLE_WALLET
    {
        /* When compiled without wallet or -disablewallet is provided,
         * the central widget is the rpc console.
         */
        setCentralWidget(rpcConsole);
        Q_EMIT consoleShown(rpcConsole);
    }

    modalOverlay = new ModalOverlay(enableWallet, this->centralWidget());

    // Accept D&D of URIs
    setAcceptDrops(true);

    // Create actions for the toolbar, menu bar and tray/dock icon
    // Needs walletFrame to be initialized
    createActions();

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();
    m_main_window_font_size = settings.value(QStringLiteral("mainWindowFontSize"), font().pointSize()).toInt();
    applyMainWindowFontSize();

    // Create system tray icon and notification
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        createTrayIcon();
    }
    notificator = new Notificator(QApplication::applicationName(), trayIcon, this);

    // Create status bar
    statusBar();

    // Disable size grip because it looks ugly and nobody needs it
    statusBar()->setSizeGripEnabled(false);

    // Status bar notification icons
    QFrame *frameBlocks = new QFrame();
    frameBlocks->setObjectName(QStringLiteral("statusIconFrame"));
    frameBlocks->setContentsMargins(0,0,0,0);
    frameBlocks->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    QHBoxLayout *frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(8,2,12,2);
    frameBlocksLayout->setSpacing(4);
    const QSize status_button_size(64, 32);
    labelWalletEncryptionIcon = new QLabel();
    labelWalletEncryptionIcon->setObjectName(QStringLiteral("statusIconControl"));
    labelWalletHDStatusIcon = new QLabel();
    labelWalletHDStatusIcon->setObjectName(QStringLiteral("statusPlainIcon"));
    labelWalletHDStatusIcon->setStyleSheet(QStringLiteral("QLabel#statusPlainIcon { background: transparent; border: none; padding: 0px; }"));
    labelProxyIcon = new GUIUtil::ClickableLabel();
    labelProxyIcon->setObjectName(QStringLiteral("statusIconControl"));
    m_network_sync_status_label = new QLabel();
    m_network_sync_status_label->setObjectName(QStringLiteral("statusPlainReadout"));
    m_network_sync_status_label->setStyleSheet(QStringLiteral("QLabel#statusPlainReadout { background: transparent; border: none; padding: 0px 4px; }"));
    m_network_sync_status_label->setTextFormat(Qt::RichText);
    m_network_sync_status_label->setAlignment(Qt::AlignVCenter | Qt::AlignCenter);
    m_network_sync_status_label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_network_sync_status_label->setMinimumHeight(status_button_size.height());
    m_network_sync_status_label->setTextInteractionFlags(Qt::NoTextInteraction);
    labelBlocksIcon = new GUIUtil::ClickableLabel();
    labelBlocksIcon->setParent(this);
    labelBlocksIcon->hide();
    QLabel* status_icons[] = {labelWalletEncryptionIcon, labelWalletHDStatusIcon, labelProxyIcon};
    for (QLabel* status_icon : status_icons) {
        status_icon->setFixedSize(status_button_size);
        status_icon->setAlignment(Qt::AlignCenter);
    }
    if(enableWallet)
    {
        frameBlocksLayout->addWidget(labelWalletEncryptionIcon);
        frameBlocksLayout->addWidget(labelWalletHDStatusIcon);
    }
    frameBlocksLayout->addWidget(labelProxyIcon);
    frameBlocksLayout->addSpacing(6);
    frameBlocksLayout->addWidget(m_network_sync_status_label);

    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(false);
    progressBar = new GUIUtil::ProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setVisible(false);

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://doc.qt.io/qt-5/gallery.html
    QString curStyle = QApplication::style()->metaObject()->className();
    if(curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle")
    {
        progressBar->setStyleSheet("QProgressBar { background-color: #e8e8e8; border: 1px solid grey; border-radius: 7px; padding: 1px; text-align: center; } QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #FF8000, stop: 1 orange); border-radius: 7px; margin: 0px; }");
    }

    statusBar()->addWidget(progressBarLabel);
    statusBar()->addWidget(progressBar);
    statusBar()->addPermanentWidget(frameBlocks);

    // Install event filter to be able to catch status tip events (QEvent::StatusTip)
    this->installEventFilter(this);

    // Initially wallet actions should be disabled
    setWalletActionsEnabled(false);

    if (!restored_geometry) {
        resize(std::max(width(), ENABLE_DEFCOIN_FUN_UI ? 1480 : 1260), std::max(height(), 720));
    }

    // Subscribe to notifications from core
    subscribeToCoreSignals();

    connect(labelProxyIcon, &GUIUtil::ClickableLabel::clicked, [this] {
        openOptionsDialogWithTab(OptionsDialog::TAB_NETWORK);
    });

    connect(progressBar, &GUIUtil::ClickableProgressBar::clicked, this, &BitcoinGUI::showModalOverlay);
#ifdef ENABLE_WALLET
    if(enableWallet) {
        connect(walletFrame, &WalletFrame::requestedSyncWarningInfo, this, &BitcoinGUI::showModalOverlay);
    }
#endif

#ifdef Q_OS_MAC
    m_app_nap_inhibitor = new CAppNapInhibitor;
#endif

    GUIUtil::handleCloseWindowShortcut(this);
}

BitcoinGUI::~BitcoinGUI()
{
    // Unsubscribe from notifications from core
    unsubscribeFromCoreSignals();

    QSettings settings;
    settings.setValue("MainWindowGeometry", saveGeometry());
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete m_app_nap_inhibitor;
    delete appMenuBar;
    MacDockIconHandler::cleanup();
#endif

    delete rpcConsole;
}

void BitcoinGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);
    tabGroup->setExclusive(true);
    connect(modalOverlay, &ModalOverlay::triggered, tabGroup, &QActionGroup::setEnabled);

    overviewAction = new QAction(platformStyle->SingleColorIcon(":/icons/overview"), tr("&Overview"), this);
    overviewAction->setStatusTip(tr("Show general overview of wallet"));
    overviewAction->setToolTip(overviewAction->statusTip());
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction(platformStyle->SingleColorIcon(":/icons/send"), tr("&Send"), this);
    sendCoinsAction->setStatusTip(tr("Send coins to a Defcoin address"));
    sendCoinsAction->setToolTip(sendCoinsAction->statusTip());
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendCoinsAction);

    sendCoinsMenuAction = new QAction(sendCoinsAction->text(), this);
    sendCoinsMenuAction->setStatusTip(sendCoinsAction->statusTip());
    sendCoinsMenuAction->setToolTip(sendCoinsMenuAction->statusTip());

    receiveCoinsAction = new QAction(platformStyle->SingleColorIcon(":/icons/receiving_addresses"), tr("&Receive"), this);
    receiveCoinsAction->setStatusTip(tr("Request payments (generates QR codes and defcoin: URIs)"));
    receiveCoinsAction->setToolTip(receiveCoinsAction->statusTip());
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(receiveCoinsAction);

    receiveCoinsMenuAction = new QAction(receiveCoinsAction->text(), this);
    receiveCoinsMenuAction->setStatusTip(receiveCoinsAction->statusTip());
    receiveCoinsMenuAction->setToolTip(receiveCoinsMenuAction->statusTip());

    historyAction = new QAction(platformStyle->SingleColorIcon(":/icons/history"), tr("&Transactions"), this);
    historyAction->setStatusTip(tr("Browse transaction history"));
    historyAction->setToolTip(historyAction->statusTip());
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);

#ifdef ENABLE_WALLET
    // These showNormalIfMinimized are needed because Send Coins and Receive Coins
    // can be triggered from the tray menu, and need to show the GUI to be useful.
    connect(overviewAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
    connect(overviewAction, &QAction::triggered, this, &BitcoinGUI::gotoOverviewPage);
    connect(sendCoinsAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
    connect(sendCoinsAction, &QAction::triggered, [this]{ gotoSendCoinsPage(); });
    connect(sendCoinsMenuAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
    connect(sendCoinsMenuAction, &QAction::triggered, [this]{ gotoSendCoinsPage(); });
    connect(receiveCoinsAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
    connect(receiveCoinsAction, &QAction::triggered, this, &BitcoinGUI::gotoReceiveCoinsPage);
    connect(receiveCoinsMenuAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
    connect(receiveCoinsMenuAction, &QAction::triggered, this, &BitcoinGUI::gotoReceiveCoinsPage);
    connect(historyAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
    connect(historyAction, &QAction::triggered, this, &BitcoinGUI::gotoHistoryPage);
#endif // ENABLE_WALLET

    quitAction = new QAction(tr("E&xit"), this);
    quitAction->setStatusTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    const QString about_product_name = ENABLE_DEFCOIN_FUN_UI ? tr("%1 DC34 Edition").arg(PACKAGE_NAME) : QString(PACKAGE_NAME);
    aboutAction = new QAction(tr("&About %1").arg(about_product_name), this);
    aboutAction->setStatusTip(tr("Show information about %1").arg(about_product_name));
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutAction->setEnabled(false);
#if ENABLE_DEFCOIN_FUN_UI
    walletHelpAction = new QAction(tr("&Defcoin Core Help Manual"), this);
    walletHelpAction->setStatusTip(tr("Open the Defcoin Core help manual"));
    walletHelpAction->setMenuRole(QAction::NoRole);
#endif
    aboutQtAction = new QAction(tr("About &Qt"), this);
    aboutQtAction->setStatusTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(platformStyle->SingleColorIcon(":/icons/settings"), tr("&Preferences..."), this);
    optionsAction->setStatusTip(tr("Modify preferences for %1").arg(PACKAGE_NAME));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    optionsAction->setEnabled(false);
    toggleHideAction = new QAction(tr("&Show / Hide"), this);
    toggleHideAction->setStatusTip(tr("Show or hide the main Window"));

    encryptWalletAction = new QAction(tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setStatusTip(tr("Encrypt the private keys that belong to your wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(tr("&Backup Wallet..."), this);
    backupWalletAction->setStatusTip(tr("Backup wallet to another location"));
    changePassphraseAction = new QAction(tr("&Change Passphrase..."), this);
    changePassphraseAction->setStatusTip(tr("Change the passphrase used for wallet encryption"));
    signMessageAction = new QAction(tr("Sign &message..."), this);
    signMessageAction->setStatusTip(tr("Sign messages with your Defcoin addresses to prove you own them"));
    verifyMessageAction = new QAction(tr("&Verify message..."), this);
    verifyMessageAction->setStatusTip(tr("Verify messages to ensure they were signed with specified Defcoin addresses"));
    m_load_psbt_action = new QAction(tr("&Load PSBT from file..."), this);
    m_load_psbt_action->setStatusTip(tr("Load Partially Signed Defcoin Transaction"));
    m_load_psbt_clipboard_action = new QAction(tr("Load PSBT from clipboard..."), this);
    m_load_psbt_clipboard_action->setStatusTip(tr("Load Partially Signed Defcoin Transaction from clipboard"));

    openRPCConsoleAction = new QAction(QIcon(":/icons/node"), tr("Node"), this);
    openRPCConsoleAction->setStatusTip(tr("Open node debugging and diagnostic console"));
    // initially disable the debug window menu item
    openRPCConsoleAction->setEnabled(false);
    openRPCConsoleAction->setObjectName("openRPCConsoleAction");

    m_network_toggle_action = new QAction(QIcon(":/icons/network_connected"), tr("Network"), this);
    m_network_toggle_action->setStatusTip(tr("Enable or disable Defcoin network activity"));
    m_network_toggle_action->setToolTip(m_network_toggle_action->statusTip());
    m_network_toggle_action->setCheckable(true);
    m_network_toggle_action->setEnabled(false);
    m_network_toggle_action->setObjectName("networkToggleAction");
    connect(m_network_toggle_action, &QAction::triggered, [this] {
        m_node.setNetworkActive(!m_node.getNetworkActive());
    });

    m_theme_cycle_action = new QAction(this);
    connect(m_theme_cycle_action, &QAction::triggered, this, &BitcoinGUI::cycleAppearanceTheme);
    updateAppearanceThemeButton();

    usedSendingAddressesAction = new QAction(tr("&Sending addresses"), this);
    usedSendingAddressesAction->setStatusTip(tr("Show the list of used sending addresses and labels"));
    usedReceivingAddressesAction = new QAction(tr("&Receiving addresses"), this);
    usedReceivingAddressesAction->setStatusTip(tr("Show the list of used receiving addresses and labels"));

    openAction = new QAction(tr("Open &URI..."), this);
    openAction->setStatusTip(tr("Open a defcoin: URI"));

    m_open_wallet_action = new QAction(tr("Open Wallet"), this);
    m_open_wallet_action->setEnabled(false);
    m_open_wallet_action->setStatusTip(tr("Open a wallet"));
    m_open_wallet_menu = new QMenu(this);

    m_close_wallet_action = new QAction(tr("Close Wallet..."), this);
    m_close_wallet_action->setStatusTip(tr("Close wallet"));
    
    m_create_wallet_action = new QAction(tr("Create Wallet..."), this);
    m_create_wallet_action->setEnabled(false);
    m_create_wallet_action->setStatusTip(tr("Create a new wallet"));

    m_close_all_wallets_action = new QAction(tr("Close All Wallets..."), this);
    m_close_all_wallets_action->setStatusTip(tr("Close all wallets"));

    showHelpMessageAction = new QAction(tr("&Command-line options"), this);
    showHelpMessageAction->setMenuRole(QAction::NoRole);
    showHelpMessageAction->setStatusTip(tr("Show the %1 help message to get a list with possible Defcoin command-line options").arg(PACKAGE_NAME));

    m_mask_values_action = new QAction(tr("&Mask values"), this);
    m_mask_values_action->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_M));
    m_mask_values_action->setStatusTip(tr("Hide wallet balances and transaction amounts on the Overview page"));
    m_mask_values_action->setToolTip(tr("Mask values on the Overview page"));
    m_mask_values_action->setCheckable(true);
    m_mask_values_action->setChecked(QSettings().value(QStringLiteral("MaskValues"), false).toBool());
    updateMaskValuesIcon();

    connect(quitAction, &QAction::triggered, qApp, QApplication::quit);
    connect(aboutAction, &QAction::triggered, this, &BitcoinGUI::aboutClicked);
    connect(aboutQtAction, &QAction::triggered, qApp, QApplication::aboutQt);
    connect(optionsAction, &QAction::triggered, this, &BitcoinGUI::optionsClicked);
    connect(toggleHideAction, &QAction::triggered, this, &BitcoinGUI::toggleHidden);
    connect(showHelpMessageAction, &QAction::triggered, this, &BitcoinGUI::showHelpMessageClicked);
#if ENABLE_DEFCOIN_FUN_UI
    connect(walletHelpAction, &QAction::triggered, this, &BitcoinGUI::showWalletHelpClicked);
#endif
    connect(openRPCConsoleAction, &QAction::triggered, this, &BitcoinGUI::showDebugWindow);
    // prevents an open debug window from becoming stuck/unusable on client shutdown
    connect(quitAction, &QAction::triggered, rpcConsole, &QWidget::hide);

#ifdef ENABLE_WALLET
    if(walletFrame)
    {
        connect(encryptWalletAction, &QAction::triggered, walletFrame, &WalletFrame::encryptWallet);
        connect(backupWalletAction, &QAction::triggered, walletFrame, &WalletFrame::backupWallet);
        connect(changePassphraseAction, &QAction::triggered, walletFrame, &WalletFrame::changePassphrase);
        connect(signMessageAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
        connect(signMessageAction, &QAction::triggered, [this]{ gotoSignMessageTab(); });
        connect(m_load_psbt_action, &QAction::triggered, [this]{ gotoLoadPSBT(); });
        connect(m_load_psbt_clipboard_action, &QAction::triggered, [this]{ gotoLoadPSBT(true); });
        connect(verifyMessageAction, &QAction::triggered, [this]{ showNormalIfMinimized(); });
        connect(verifyMessageAction, &QAction::triggered, [this]{ gotoVerifyMessageTab(); });
        connect(usedSendingAddressesAction, &QAction::triggered, walletFrame, &WalletFrame::usedSendingAddresses);
        connect(usedReceivingAddressesAction, &QAction::triggered, walletFrame, &WalletFrame::usedReceivingAddresses);
        connect(openAction, &QAction::triggered, this, &BitcoinGUI::openClicked);
        connect(m_open_wallet_menu, &QMenu::aboutToShow, [this] {
            m_open_wallet_menu->clear();
            for (const std::pair<const std::string, bool>& i : m_wallet_controller->listWalletDir()) {
                const std::string& path = i.first;
                QString name = path.empty() ? QString("["+tr("default wallet")+"]") : QString::fromStdString(path);
                // Menu items remove single &. Single & are shown when && is in
                // the string, but only the first occurrence. So replace only
                // the first & with &&.
                name.replace(name.indexOf(QChar('&')), 1, QString("&&"));
                QAction* action = m_open_wallet_menu->addAction(name);

                if (i.second) {
                    // This wallet is already loaded
                    action->setEnabled(false);
                    continue;
                }

                connect(action, &QAction::triggered, [this, path] {
                    auto activity = new OpenWalletActivity(m_wallet_controller, this);
                    connect(activity, &OpenWalletActivity::opened, this, &BitcoinGUI::setCurrentWallet);
                    connect(activity, &OpenWalletActivity::finished, activity, &QObject::deleteLater);
                    activity->open(path);
                });
            }
            if (m_open_wallet_menu->isEmpty()) {
                QAction* action = m_open_wallet_menu->addAction(tr("No wallets available"));
                action->setEnabled(false);
            }
        });
        connect(m_close_wallet_action, &QAction::triggered, [this] {
            m_wallet_controller->closeWallet(walletFrame->currentWalletModel(), this);
        });
        connect(m_create_wallet_action, &QAction::triggered, [this] {
            auto activity = new CreateWalletActivity(m_wallet_controller, this);
            connect(activity, &CreateWalletActivity::created, this, &BitcoinGUI::setCurrentWallet);
            connect(activity, &CreateWalletActivity::finished, activity, &QObject::deleteLater);
            activity->create();
        });
        connect(m_close_all_wallets_action, &QAction::triggered, [this] {
            m_wallet_controller->closeAllWallets(this);
        });
        connect(m_mask_values_action, &QAction::toggled, this, [this](bool privacy) {
            QSettings().setValue(QStringLiteral("MaskValues"), privacy);
            updateMaskValuesIcon();
            Q_EMIT setPrivacy(privacy);
        });
    }
#endif // ENABLE_WALLET

    connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C), this), &QShortcut::activated, this, &BitcoinGUI::showDebugWindowActivateConsole);
    connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_D), this), &QShortcut::activated, this, &BitcoinGUI::showDebugWindow);
}

void BitcoinGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
    if(walletFrame)
    {
        file->addAction(m_create_wallet_action);
        file->addAction(m_open_wallet_action);
        file->addAction(m_close_wallet_action);
        file->addAction(m_close_all_wallets_action);
        file->addSeparator();
        file->addAction(openAction);
        file->addAction(backupWalletAction);
        file->addAction(signMessageAction);
        file->addAction(verifyMessageAction);
        file->addAction(m_load_psbt_action);
        file->addAction(m_load_psbt_clipboard_action);
        file->addSeparator();
    }
    file->addAction(quitAction);

    QMenu *settings = appMenuBar->addMenu(tr("&Preferences"));
    if(walletFrame)
    {
        settings->addAction(encryptWalletAction);
        settings->addAction(changePassphraseAction);
        settings->addSeparator();
        settings->addAction(m_mask_values_action);
        settings->addSeparator();
    }
    settings->addAction(optionsAction);
    settings->addSeparator();
    QMenu* text_size_menu = settings->addMenu(tr("Text Size"));
    QAction* decrease_text_action = text_size_menu->addAction(tr("Decrease Text Size"));
    decrease_text_action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Minus));
    QAction* increase_text_action = text_size_menu->addAction(tr("Increase Text Size"));
    increase_text_action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Plus));
    QAction* reset_text_action = text_size_menu->addAction(tr("Reset Text Size"));
    connect(decrease_text_action, &QAction::triggered, this, [this] { adjustMainWindowFontSize(-1); });
    connect(increase_text_action, &QAction::triggered, this, [this] { adjustMainWindowFontSize(1); });
    connect(reset_text_action, &QAction::triggered, this, [this] {
        setMainWindowFontSize(QApplication::font().pointSize());
    });

    QMenu* window_menu = appMenuBar->addMenu(tr("&Window"));

    QAction* minimize_action = window_menu->addAction(tr("Minimize"));
    minimize_action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_M));
    connect(minimize_action, &QAction::triggered, [] {
        QApplication::activeWindow()->showMinimized();
    });
    QPointer<QAction> guarded_minimize_action(minimize_action);
    connect(qApp, &QApplication::focusWindowChanged, appMenuBar, [guarded_minimize_action] (QWindow* window) {
        if (!guarded_minimize_action) return;
        guarded_minimize_action->setEnabled(window != nullptr && (window->flags() & Qt::Dialog) != Qt::Dialog && window->windowState() != Qt::WindowMinimized);
    });

#ifdef Q_OS_MAC
    QAction* zoom_action = window_menu->addAction(tr("Zoom"));
    connect(zoom_action, &QAction::triggered, [] {
        QWindow* window = qApp->focusWindow();
        if (window->windowState() != Qt::WindowMaximized) {
            window->showMaximized();
        } else {
            window->showNormal();
        }
    });

    QPointer<QAction> guarded_zoom_action(zoom_action);
    connect(qApp, &QApplication::focusWindowChanged, appMenuBar, [guarded_zoom_action] (QWindow* window) {
        if (!guarded_zoom_action) return;
        guarded_zoom_action->setEnabled(window != nullptr);
    });
#endif

    if (walletFrame) {
#ifdef Q_OS_MAC
        window_menu->addSeparator();
        QAction* main_window_action = window_menu->addAction(tr("Main Window"));
        connect(main_window_action, &QAction::triggered, [this] {
            GUIUtil::bringToFront(this);
        });
#endif
        window_menu->addSeparator();
        window_menu->addAction(usedSendingAddressesAction);
        window_menu->addAction(usedReceivingAddressesAction);
    }

    window_menu->addSeparator();
    QMenu* node_menu = window_menu->addMenu(tr("Node..."));
    node_menu->addAction(openRPCConsoleAction);
    node_menu->addSeparator();
    for (RPCConsole::TabTypes tab_type : rpcConsole->tabs()) {
        QAction* tab_action = node_menu->addAction(rpcConsole->tabTitle(tab_type));
        tab_action->setShortcut(rpcConsole->tabShortcut(tab_type));
        connect(tab_action, &QAction::triggered, [this, tab_type] {
            rpcConsole->setTabFocus(tab_type);
            showDebugWindow();
        });
    }

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
#if ENABLE_DEFCOIN_FUN_UI
    help->addAction(walletHelpAction);
#endif
    help->addAction(showHelpMessageAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);
}

QWidget* BitcoinGUI::createMainTextControls()
{
    QWidget* controls = new QWidget(this);
    controls->setObjectName(QStringLiteral("mainTextControls"));
    QHBoxLayout* layout = new QHBoxLayout(controls);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);

    QPushButton* smaller = new QPushButton(tr("A-"), controls);
    QPushButton* larger = new QPushButton(tr("A+"), controls);
    smaller->setToolTip(tr("Make main wallet text smaller"));
    larger->setToolTip(tr("Make main wallet text larger"));
    const QString button_style = QStringLiteral(
        "QPushButton { color: #8fcaff; background: rgba(15, 24, 34, 185);"
        " border: 1px solid rgba(143, 202, 255, 210); border-radius: 4px; padding: 0 4px; }"
        "QPushButton:hover { color: #ffffff; background: rgba(34, 56, 78, 220); border-color: #d7ebff; }"
        "QPushButton:pressed { background: rgba(7, 15, 24, 235); }");
    for (QPushButton* button : {smaller, larger}) {
        QFont f = button->font();
        f.setPointSize(std::max(11, f.pointSize() + 1));
        f.setBold(true);
        button->setFont(f);
        button->setFixedSize(48, 40);
        button->setStyleSheet(button_style);
        layout->addWidget(button);
    }
    connect(smaller, &QPushButton::clicked, this, [this] { adjustMainWindowFontSize(-1); });
    connect(larger, &QPushButton::clicked, this, [this] { adjustMainWindowFontSize(1); });
    controls->setFixedHeight(48);
    controls->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return controls;
}

void BitcoinGUI::adjustMainWindowFontSize(int delta)
{
    const int current = m_main_window_font_size > 0 ? m_main_window_font_size : font().pointSize();
    setMainWindowFontSize(current + delta);
}

void BitcoinGUI::setMainWindowFontSize(int size)
{
    const int clamped = std::max(8, std::min(28, size));
    if (m_main_window_font_size == clamped) return;
    m_main_window_font_size = clamped;
    QSettings settings;
    settings.setValue(QStringLiteral("mainWindowFontSize"), clamped);
    if (settings.value(QStringLiteral("LinkWalletTextSizes"), true).toBool()) {
        settings.setValue(QStringLiteral("nodeWindowFontSize"), clamped);
        if (rpcConsole) {
            rpcConsole->setNodeWindowFontSize(clamped);
        }
    }
    applyMainWindowFontSize();
}

void BitcoinGUI::applyMainWindowFontSize()
{
    if (m_main_window_font_size <= 0) {
        m_main_window_font_size = QSettings().value(QStringLiteral("mainWindowFontSize"), font().pointSize()).toInt();
    }
    QFont f = font();
    f.setPointSize(std::max(8, std::min(28, m_main_window_font_size)));
    ApplyFontRecursively(this, f);
    if (appMenuBar) ApplyFontRecursively(appMenuBar, f);
    for (QWidget* top_level : QApplication::topLevelWidgets()) {
        if (!top_level || top_level == this || top_level == rpcConsole) continue;
        if (top_level->parentWidget() == this || top_level->windowModality() != Qt::NonModal) {
            ApplyFontRecursively(top_level, f);
        }
    }
    updateMainWindowFontDependentLayout(f);
    if (m_main_text_controls) m_main_text_controls->setVisible(!QSettings().value(QStringLiteral("HideTextResizeControls"), false).toBool());
    if (layout()) layout()->activate();
    if (centralWidget() && centralWidget()->layout()) centralWidget()->layout()->activate();
    QTimer::singleShot(0, this, [this] {
        updateMainWindowFontDependentLayout(font());
        if (layout()) layout()->activate();
        if (centralWidget() && centralWidget()->layout()) centralWidget()->layout()->activate();
        updateGeometry();
    });
    updateGeometry();
}

void BitcoinGUI::refreshTextSizeSettings()
{
    QSettings settings;
    m_main_window_font_size = settings.value(QStringLiteral("mainWindowFontSize"), QApplication::font().pointSize()).toInt();
    applyMainWindowFontSize();
}

void BitcoinGUI::updateMainWindowFontDependentLayout(const QFont& font)
{
    const QFontMetrics metrics(font);
    const int nav_height = std::max(48, metrics.height() + 24);
    if (m_main_text_controls) {
        const int button_height = std::max(34, nav_height - 8);
        const int button_width = std::max(44, GUIUtil::TextWidth(metrics, QStringLiteral("A+")) + 18);
        for (QPushButton* button : m_main_text_controls->findChildren<QPushButton*>()) {
            button->setMinimumSize(button_width, button_height);
            button->setMaximumSize(button_width, button_height);
        }
        m_main_text_controls->setFixedHeight(nav_height);
    }
    if (!appToolBar) return;
    appToolBar->setFont(font);
    const int icon_width = appToolBar->iconSize().width();
    auto update_action_widget = [&](QAction* action, int minimum_width, bool icon_and_text) {
        QWidget* button = appToolBar->widgetForAction(action);
        if (!button) return;
        QString label = action ? action->text() : QString();
        label.remove('&');
        const int text_width = label.isEmpty() ? 0 : GUIUtil::TextWidth(metrics, label);
        const int width = std::max(minimum_width, text_width + (icon_and_text ? icon_width + 38 : 24));
        button->setMaximumWidth(QWIDGETSIZE_MAX);
        button->setMinimumSize(width, nav_height);
        button->setMaximumHeight(nav_height);
        button->updateGeometry();
    };
    update_action_widget(overviewAction, 104, true);
    update_action_widget(sendCoinsAction, 104, true);
    update_action_widget(receiveCoinsAction, 104, true);
    update_action_widget(historyAction, 104, true);
    update_action_widget(openRPCConsoleAction, 104, true);
    update_action_widget(m_network_toggle_action, 136, true);
    const int icon_button_size = std::max(48, nav_height);
    for (QToolButton* button : {m_mask_values_button, m_settings_button, m_theme_button}) {
        if (!button) continue;
        button->setFixedSize(icon_button_size, icon_button_size);
        button->updateGeometry();
    }
    appToolBar->setMinimumHeight(nav_height + 4);
    appToolBar->updateGeometry();
}

void BitcoinGUI::createToolBars()
{
    if(walletFrame)
    {
        QToolBar *toolbar = addToolBar(tr("Tabs toolbar"));
        appToolBar = toolbar;
        toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
        toolbar->setContentsMargins(0, 0, 12, 0);
        toolbar->setMovable(false);
#if ENABLE_DEFCOIN_FUN_UI
        toolbar->setIconSize(QSize(34, 34));
#else
        toolbar->setIconSize(QSize(34, 34));
#endif
        toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toolbar->addAction(overviewAction);
        toolbar->addAction(sendCoinsAction);
        toolbar->addAction(receiveCoinsAction);
        toolbar->addAction(historyAction);
        toolbar->addSeparator();
        toolbar->addAction(m_network_toggle_action);
        toolbar->addAction(openRPCConsoleAction);
#if ENABLE_DEFCOIN_FUN_UI
        const QList<QAction*> fun_nav_actions{overviewAction, sendCoinsAction, receiveCoinsAction, historyAction, openRPCConsoleAction};
        QFontMetrics nav_metrics(toolbar->font());
        int fun_nav_width = 0;
        for (QAction* action : fun_nav_actions) {
            QString label = action->text();
            label.remove('&');
            fun_nav_width = std::max(fun_nav_width, GUIUtil::TextWidth(nav_metrics, label));
        }
        fun_nav_width = std::max(104, std::min(132, fun_nav_width + toolbar->iconSize().width() + 34));
        const QSize fun_nav_button_size(fun_nav_width, 48);
        for (QAction* action : fun_nav_actions) {
            if (QWidget* button = toolbar->widgetForAction(action)) {
                button->setMinimumSize(fun_nav_button_size);
                button->setMaximumSize(fun_nav_button_size);
            }
        }
        if (QWidget* button = toolbar->widgetForAction(m_network_toggle_action)) {
            button->setMinimumSize(QSize(142, 48));
            button->setMaximumSize(QSize(142, 48));
        }
#else
        const QList<QAction*> nav_actions{overviewAction, sendCoinsAction, receiveCoinsAction, historyAction, openRPCConsoleAction};
        for (QAction* action : nav_actions) {
            if (QWidget* button = toolbar->widgetForAction(action)) {
                button->setMinimumHeight(48);
            }
        }
        if (QWidget* button = toolbar->widgetForAction(m_network_toggle_action)) {
            button->setMinimumHeight(48);
        }
#endif
        overviewAction->setChecked(true);

#ifdef ENABLE_WALLET
        QWidget *spacer = new QWidget();
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        toolbar->addWidget(spacer);

        m_wallet_selector = new QComboBox();
        m_wallet_selector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        connect(m_wallet_selector, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &BitcoinGUI::setCurrentWalletBySelectorIndex);

        m_wallet_selector_label = new QLabel();
        m_wallet_selector_label->setText(tr("Wallet:") + " ");
        m_wallet_selector_label->setBuddy(m_wallet_selector);

        m_wallet_selector_label_action = appToolBar->addWidget(m_wallet_selector_label);
        m_wallet_selector_action = appToolBar->addWidget(m_wallet_selector);

        m_wallet_selector_label_action->setVisible(false);
        m_wallet_selector_action->setVisible(false);

        m_main_text_controls = createMainTextControls();
        m_main_text_controls->setVisible(!QSettings().value(QStringLiteral("HideTextResizeControls"), false).toBool());
        toolbar->addWidget(m_main_text_controls);

        toolbar->addSeparator();

        m_mask_values_button = new QToolButton(toolbar);
        m_mask_values_button->setObjectName(QStringLiteral("toolbarIconButton"));
        m_mask_values_button->setAutoRaise(true);
        m_mask_values_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_mask_values_button->setIconSize(toolbar->iconSize());
        m_mask_values_button->setFixedSize(48, 48);
        m_mask_values_button->setDefaultAction(m_mask_values_action);
        m_mask_values_button->setToolTip(m_mask_values_action->toolTip());
        toolbar->addWidget(m_mask_values_button);

        m_settings_button = new QToolButton(toolbar);
        m_settings_button->setObjectName(QStringLiteral("toolbarIconButton"));
        m_settings_button->setAutoRaise(true);
        m_settings_button->setIcon(platformStyle->SingleColorIcon(":/icons/settings"));
        m_settings_button->setIconSize(toolbar->iconSize());
        m_settings_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_settings_button->setFixedSize(48, 48);
        m_settings_button->setToolTip(tr("Preferences"));
        m_settings_button->setPopupMode(QToolButton::DelayedPopup);
        m_settings_button->setDefaultAction(optionsAction);
        m_settings_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        QMenu* settings_menu = new QMenu(m_settings_button);
        QAction* main_preferences_action = settings_menu->addAction(tr("Main"));
        QAction* wallet_preferences_action = settings_menu->addAction(tr("Wallet"));
        QAction* network_preferences_action = settings_menu->addAction(tr("Network"));
        QAction* display_preferences_action = settings_menu->addAction(tr("Display"));
        connect(main_preferences_action, &QAction::triggered, [this] { openOptionsDialogWithTab(OptionsDialog::TAB_MAIN); });
        connect(wallet_preferences_action, &QAction::triggered, [this] { openOptionsDialogWithTab(OptionsDialog::TAB_WALLET); });
        connect(network_preferences_action, &QAction::triggered, [this] { openOptionsDialogWithTab(OptionsDialog::TAB_NETWORK); });
        connect(display_preferences_action, &QAction::triggered, [this] { openOptionsDialogWithTab(OptionsDialog::TAB_DISPLAY); });
        wallet_preferences_action->setEnabled(enableWallet);
        m_settings_button->setMenu(settings_menu);
        toolbar->addWidget(m_settings_button);

#if ENABLE_DEFCOIN_FUN_UI
        m_theme_button = new QToolButton(toolbar);
        m_theme_button->setObjectName(QStringLiteral("toolbarIconButton"));
        m_theme_button->setAutoRaise(true);
        m_theme_button->setIconSize(toolbar->iconSize());
        m_theme_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_theme_button->setFixedSize(48, 48);
        m_theme_button->setPopupMode(QToolButton::DelayedPopup);
        m_theme_button->setDefaultAction(m_theme_cycle_action);
        m_theme_menu = new QMenu(m_theme_button);
        connect(m_theme_menu, &QMenu::aboutToShow, this, &BitcoinGUI::rebuildThemeMenus);
        m_theme_button->setMenu(m_theme_menu);
        toolbar->addWidget(m_theme_button);
        rebuildThemeMenus();
        updateAppearanceThemeButton();
#else
        rebuildThemeMenus();
        updateAppearanceThemeButton();
#endif
#endif
    }
}

void BitcoinGUI::setClientModel(ClientModel *_clientModel, interfaces::BlockAndHeaderTipInfo* tip_info)
{
    this->clientModel = _clientModel;
    if(_clientModel)
    {
        // Create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        createTrayIconMenu();

        // Keep up to date with client
        updateNetworkState();
        connect(_clientModel, &ClientModel::numConnectionsChanged, this, &BitcoinGUI::setNumConnections);
        connect(_clientModel, &ClientModel::networkActiveChanged, this, &BitcoinGUI::setNetworkActive);

        modalOverlay->setKnownBestHeight(tip_info->header_height, QDateTime::fromTime_t(tip_info->header_time));
        setNumBlocks(tip_info->block_height, QDateTime::fromTime_t(tip_info->block_time), tip_info->verification_progress, false, SynchronizationState::INIT_DOWNLOAD);
        connect(_clientModel, &ClientModel::numBlocksChanged, this, &BitcoinGUI::setNumBlocks);

        // Receive and report messages from client model
        connect(_clientModel, &ClientModel::message, [this](const QString &title, const QString &message, unsigned int style){
            this->message(title, message, style);
        });

        // Show progress dialog
        connect(_clientModel, &ClientModel::showProgress, this, &BitcoinGUI::showProgress);

        rpcConsole->setClientModel(_clientModel, tip_info->block_height, tip_info->block_time, tip_info->verification_progress);
        maybeScheduleUiCapture();

        updateProxyIcon();
        if (m_network_toggle_action) m_network_toggle_action->setEnabled(true);

#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->setClientModel(_clientModel);
        }
#endif // ENABLE_WALLET
        if (unitDisplayControl) unitDisplayControl->setOptionsModel(_clientModel->getOptionsModel());

        OptionsModel* optionsModel = _clientModel->getOptionsModel();
        if (optionsModel) {
            if (trayIcon) {
                // be aware of the tray icon disable state change reported by the OptionsModel object.
                connect(optionsModel, &OptionsModel::hideTrayIconChanged, this, &BitcoinGUI::setTrayIconVisible);

                // initialize the disable state of the tray icon with the current value in the model.
                setTrayIconVisible(optionsModel->getHideTrayIcon());
            }
            connect(optionsModel, &OptionsModel::appearanceThemeChanged, this, [this] {
                updateThemedIcons();
            });
            updateThemedIcons();
        }
    } else {
        // Disable possibility to show main window via action
        toggleHideAction->setEnabled(false);
        if(trayIconMenu)
        {
            // Disable context menu on tray icon
            trayIconMenu->clear();
        }
        // Propagate cleared model to child objects
        rpcConsole->setClientModel(nullptr);
#ifdef ENABLE_WALLET
        if (walletFrame)
        {
            walletFrame->setClientModel(nullptr);
        }
#endif // ENABLE_WALLET
        if (unitDisplayControl) unitDisplayControl->setOptionsModel(nullptr);
        if (m_network_toggle_action) m_network_toggle_action->setEnabled(false);
    }
}

void BitcoinGUI::maybeScheduleUiCapture()
{
    if (m_ui_capture_scheduled || !clientModel) return;

    const QString capture_dir = QString::fromLocal8Bit(qgetenv("DEFCOIN_UI_CAPTURE_DIR"));
    if (capture_dir.isEmpty()) return;

    m_ui_capture_scheduled = true;
    QTimer::singleShot(1500, this, [this, capture_dir] {
        QDir dir(capture_dir);
        dir.mkpath(QStringLiteral("."));
        grab().save(dir.filePath(QStringLiteral("main-default.png")));
        if (rpcConsole) {
            showDebugWindow();
            rpcConsole->captureUiScreenshots(capture_dir);
        }
    });
}

#ifdef ENABLE_WALLET
void BitcoinGUI::setWalletController(WalletController* wallet_controller)
{
    assert(!m_wallet_controller);
    assert(wallet_controller);

    m_wallet_controller = wallet_controller;

    m_create_wallet_action->setEnabled(true);
    m_open_wallet_action->setEnabled(true);
    m_open_wallet_action->setMenu(m_open_wallet_menu);

    connect(wallet_controller, &WalletController::walletAdded, this, &BitcoinGUI::addWallet);
    connect(wallet_controller, &WalletController::walletRemoved, this, &BitcoinGUI::removeWallet);

    for (WalletModel* wallet_model : m_wallet_controller->getOpenWallets()) {
        addWallet(wallet_model);
    }
}

WalletController* BitcoinGUI::getWalletController()
{
    return m_wallet_controller;
}

void BitcoinGUI::addWallet(WalletModel* walletModel)
{
    if (!walletFrame) return;
    if (!walletFrame->addWallet(walletModel)) return;
    rpcConsole->addWallet(walletModel);
    if (m_wallet_selector->count() == 0) {
        setWalletActionsEnabled(true);
    } else if (m_wallet_selector->count() == 1) {
        m_wallet_selector_label_action->setVisible(true);
        m_wallet_selector_action->setVisible(true);
    }
    const QString display_name = walletModel->getDisplayName();
    m_wallet_selector->addItem(display_name, QVariant::fromValue(walletModel));
    applyMainWindowFontSize();
}

void BitcoinGUI::removeWallet(WalletModel* walletModel)
{
    if (!walletFrame) return;

    labelWalletHDStatusIcon->hide();
    labelWalletEncryptionIcon->hide();

    int index = m_wallet_selector->findData(QVariant::fromValue(walletModel));
    m_wallet_selector->removeItem(index);
    if (m_wallet_selector->count() == 0) {
        setWalletActionsEnabled(false);
        overviewAction->setChecked(true);
    } else if (m_wallet_selector->count() == 1) {
        m_wallet_selector_label_action->setVisible(false);
        m_wallet_selector_action->setVisible(false);
    }
    rpcConsole->removeWallet(walletModel);
    walletFrame->removeWallet(walletModel);
    updateWindowTitle();
}

void BitcoinGUI::setCurrentWallet(WalletModel* wallet_model)
{
    if (!walletFrame) return;
    walletFrame->setCurrentWallet(wallet_model);
    for (int index = 0; index < m_wallet_selector->count(); ++index) {
        if (m_wallet_selector->itemData(index).value<WalletModel*>() == wallet_model) {
            m_wallet_selector->setCurrentIndex(index);
            break;
        }
    }
    updateWindowTitle();
}

void BitcoinGUI::setCurrentWalletBySelectorIndex(int index)
{
    WalletModel* wallet_model = m_wallet_selector->itemData(index).value<WalletModel*>();
    if (wallet_model) setCurrentWallet(wallet_model);
}

void BitcoinGUI::removeAllWallets()
{
    if(!walletFrame)
        return;
    setWalletActionsEnabled(false);
    walletFrame->removeAllWallets();
}
#endif // ENABLE_WALLET

void BitcoinGUI::setWalletActionsEnabled(bool enabled)
{
    overviewAction->setEnabled(enabled);
    sendCoinsAction->setEnabled(enabled);
    sendCoinsMenuAction->setEnabled(enabled);
    receiveCoinsAction->setEnabled(enabled);
    receiveCoinsMenuAction->setEnabled(enabled);
    historyAction->setEnabled(enabled);
    encryptWalletAction->setEnabled(enabled);
    backupWalletAction->setEnabled(enabled);
    changePassphraseAction->setEnabled(enabled);
    signMessageAction->setEnabled(enabled);
    verifyMessageAction->setEnabled(enabled);
    usedSendingAddressesAction->setEnabled(enabled);
    usedReceivingAddressesAction->setEnabled(enabled);
    openAction->setEnabled(enabled);
    m_close_wallet_action->setEnabled(enabled);
    m_close_all_wallets_action->setEnabled(enabled);
}

void BitcoinGUI::createTrayIcon()
{
    assert(QSystemTrayIcon::isSystemTrayAvailable());

#ifndef Q_OS_MAC
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        trayIcon = new QSystemTrayIcon(m_network_style->getTrayAndWindowIcon(), this);
        QString toolTip = tr("%1 client").arg(PACKAGE_NAME) + " " + m_network_style->getTitleAddText();
        trayIcon->setToolTip(toolTip);
    }
#endif
}

void BitcoinGUI::createTrayIconMenu()
{
#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-macOSes)
    if (!trayIcon)
        return;

    trayIcon->setContextMenu(trayIconMenu.get());
    connect(trayIcon, &QSystemTrayIcon::activated, this, &BitcoinGUI::trayIconActivated);
#else
    // Note: On macOS, the Dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    connect(dockIconHandler, &MacDockIconHandler::dockIconClicked, this, &BitcoinGUI::macosDockIconActivated);
    trayIconMenu->setAsDockMenu();
#endif

    // Configuration of the tray icon (or Dock icon) menu
#ifndef Q_OS_MAC
    // Note: On macOS, the Dock icon's menu already has Show / Hide action.
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
#endif
    if (enableWallet) {
        trayIconMenu->addAction(sendCoinsMenuAction);
        trayIconMenu->addAction(receiveCoinsMenuAction);
        trayIconMenu->addSeparator();
        trayIconMenu->addAction(signMessageAction);
        trayIconMenu->addAction(verifyMessageAction);
        trayIconMenu->addSeparator();
    }
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);
#ifndef Q_OS_MAC // This is built-in on macOS
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

void BitcoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
#ifndef Q_OS_MAC
    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers show/hide of the main window
        toggleHidden();
    }
#else
    Q_UNUSED(reason);
#endif
}

void BitcoinGUI::macosDockIconActivated()
{
#ifdef Q_OS_MAC
    show();
    activateWindow();
#endif
}

void BitcoinGUI::optionsClicked()
{
    openOptionsDialogWithTab(OptionsDialog::TAB_MAIN);
}

void BitcoinGUI::aboutClicked()
{
    if(!clientModel)
        return;

    HelpMessageDialog dlg(this, true);
    dlg.exec();
}

void BitcoinGUI::showDebugWindow()
{
    GUIUtil::bringToFront(rpcConsole);
    Q_EMIT consoleShown(rpcConsole);
}

void BitcoinGUI::showDebugWindowActivateConsole()
{
    rpcConsole->setTabFocus(RPCConsole::TabTypes::CONSOLE);
    showDebugWindow();
}

void BitcoinGUI::showHelpMessageClicked()
{
    GUIUtil::bringToFront(helpMessageDialog);
}

void BitcoinGUI::showWalletHelpClicked()
{
    HelpMessageDialog dlg(this, HelpMessageDialog::Mode::WalletHelp);
    dlg.exec();
}

#ifdef ENABLE_WALLET
void BitcoinGUI::openClicked()
{
    OpenURIDialog dlg(this);
    if(dlg.exec())
    {
        Q_EMIT receivedURI(dlg.getURI());
    }
}

void BitcoinGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    if (walletFrame) walletFrame->gotoOverviewPage();
}

void BitcoinGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    if (walletFrame) walletFrame->gotoHistoryPage();
}

void BitcoinGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoReceiveCoinsPage();
}

void BitcoinGUI::gotoSendCoinsPage(QString addr)
{
    sendCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoSendCoinsPage(addr);
}

void BitcoinGUI::gotoSignMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoSignMessageTab(addr);
}

void BitcoinGUI::gotoVerifyMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoVerifyMessageTab(addr);
}
void BitcoinGUI::gotoLoadPSBT(bool from_clipboard)
{
    if (walletFrame) walletFrame->gotoLoadPSBT(from_clipboard);
}
#endif // ENABLE_WALLET

void BitcoinGUI::updateNetworkState()
{
    if (!clientModel) return;

    int count = clientModel->getNumConnections();
    QString icon;
    icon = ":/icons/network_connected";

    QString tooltip;
    const bool network_active = m_node.getNetworkActive();

    if (network_active) {
        tooltip = tr("%n active connection(s) to Defcoin network", "", count) + QString(".<br>") + tr("Click to disable network activity.");
    } else {
        tooltip = tr("Network activity disabled.") + QString("<br>") + tr("Click to enable network activity again.");
        icon = ":/icons/network_isolated";
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");
    if (m_network_toggle_action) {
        m_network_toggle_action->setToolTip(tooltip);
        m_network_toggle_action->setStatusTip(network_active ? tr("Network activity enabled") : tr("Network activity disabled"));
        m_network_toggle_action->setText(network_active ? tr("CONNECTED") : tr("ISOLATED"));
        m_network_toggle_action->setChecked(network_active);
        if (appToolBar) {
            if (QWidget* button = appToolBar->widgetForAction(m_network_toggle_action)) {
                const QString theme = GUIUtil::appearanceThemeBaseStyle(GUIUtil::getConfiguredAppearanceTheme());
                const QString connected_color = QStringLiteral("#46e889");
                const QString isolated_color = (theme == QStringLiteral("light") || theme == QStringLiteral("auto"))
                    ? QStringLiteral("#a01818")
                    : QStringLiteral("#ff5f6d");
                button->setStyleSheet(QStringLiteral("font-weight: 700; color: %1;").arg(network_active ? connected_color : isolated_color));
            }
        }
    }

    const QString asset_name = icon.mid(QStringLiteral(":/icons/").size());
#if ENABLE_DEFCOIN_FUN_UI
    if (m_network_toggle_action) m_network_toggle_action->setIcon(GUIUtil::themeAssetIcon(asset_name, icon));
#else
    if (m_network_toggle_action) m_network_toggle_action->setIcon(QIcon(icon));
#endif

    if (m_network_sync_status_label) {
        QString dot_color;
        QString status_text;
        const QString peer_text = count == 1 ? tr("1 peer") : tr("%1 peers").arg(count);
        if (!network_active) {
            dot_color = QStringLiteral("#ff4d4d");
            status_text = tr("Disconnected: network activity disabled.");
        } else if (count <= 0) {
            dot_color = QStringLiteral("#ff4d4d");
            status_text = tr("Disconnected: no peers.");
        } else if (!m_block_sync_up_to_date) {
            dot_color = QStringLiteral("#ffd24a");
            status_text = tr("Connected: %1 and blockchain syncing.").arg(peer_text);
        } else {
            dot_color = QStringLiteral("#46e889");
            status_text = tr("Connected: %1 and blockchain up to date.").arg(peer_text);
        }
        m_network_sync_status_label->setText(QStringLiteral("<span style=\"font-size:15px; color:%1;\">&#9679;</span>&nbsp;%2")
            .arg(dot_color, status_text.toHtmlEscaped()));
        m_network_sync_status_label->setToolTip(tr("Network and blockchain synchronization status."));
    }
}

void BitcoinGUI::setNumConnections(int count)
{
    updateNetworkState();
}

void BitcoinGUI::setNetworkActive(bool networkActive)
{
    updateNetworkState();
}

void BitcoinGUI::updateHeadersSyncProgressLabel()
{
    int64_t headersTipTime = clientModel->getHeaderTipTime();
    int headersTipHeight = clientModel->getHeaderTipHeight();
    int estHeadersLeft = (GetTime() - headersTipTime) / Params().GetConsensus().nPowTargetSpacing;
    if (estHeadersLeft > HEADER_HEIGHT_DELTA_SYNC)
        progressBarLabel->setText(tr("Syncing Headers (%1%)...").arg(QString::number(100.0 / (headersTipHeight+estHeadersLeft)*headersTipHeight, 'f', 1)));
}

void BitcoinGUI::openOptionsDialogWithTab(OptionsDialog::Tab tab)
{
    if (!clientModel || !clientModel->getOptionsModel())
        return;

    OptionsDialog dlg(this, enableWallet);
    dlg.setCurrentTab(tab);
    dlg.setModel(clientModel->getOptionsModel());
    QFont dialog_font = font();
    dialog_font.setPointSize(std::max(8, std::min(28, m_main_window_font_size > 0 ? m_main_window_font_size : font().pointSize())));
    ApplyFontRecursively(&dlg, dialog_font);
    dlg.adjustSize();
    dlg.exec();
    refreshTextSizeSettings();
    updateThemedIcons();
}

void BitcoinGUI::applyPrivacySetting(bool privacy)
{
    if (!m_mask_values_action) return;
    if (m_mask_values_action->isChecked() != privacy) {
        m_mask_values_action->setChecked(privacy);
    } else {
        Q_EMIT setPrivacy(privacy);
    }
    updateMaskValuesIcon();
}

void BitcoinGUI::setAppearanceTheme(const QString& theme)
{
    const QString normalized = GUIUtil::normalizeAppearanceTheme(theme);
    if (clientModel && clientModel->getOptionsModel()) {
        OptionsModel* options_model = clientModel->getOptionsModel();
        options_model->setData(options_model->index(OptionsModel::AppearanceTheme, 0), normalized, Qt::EditRole);
    } else {
        QSettings settings;
        settings.setValue(QStringLiteral("appearanceTheme"), normalized);
        GUIUtil::applyAppearanceTheme(normalized);
    }
    rebuildThemeMenus();
    updateAppearanceThemeButton();
}

void BitcoinGUI::cycleAppearanceTheme()
{
    setAppearanceTheme(GUIUtil::nextAppearanceTheme(GUIUtil::getConfiguredAppearanceTheme()));
}

void BitcoinGUI::updateAppearanceThemeButton()
{
    if (!m_theme_cycle_action) return;

#if !ENABLE_DEFCOIN_FUN_UI
    const QSize toolbar_icon_size(34, 34);
    const QSize toolbar_button_size(48, 48);
    if (appToolBar) appToolBar->setIconSize(toolbar_icon_size);
    if (m_mask_values_button && appToolBar) {
        m_mask_values_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_mask_values_button->setIconSize(toolbar_icon_size);
        m_mask_values_button->setMinimumSize(toolbar_button_size);
        m_mask_values_button->setMaximumSize(toolbar_button_size);
        m_mask_values_button->setStyleSheet(QStringLiteral(
            "QToolButton#toolbarIconButton::menu-indicator { image: none; width: 0px; height: 0px; }"));
        m_mask_values_button->resize(toolbar_button_size);
    }
    if (m_settings_button && appToolBar) {
        m_settings_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_settings_button->setIcon(platformStyle->SingleColorIcon(":/icons/settings"));
        m_settings_button->setIconSize(toolbar_icon_size);
        m_settings_button->setMinimumSize(toolbar_button_size);
        m_settings_button->setMaximumSize(toolbar_button_size);
        m_settings_button->setStyleSheet(QStringLiteral(
            "QToolButton#toolbarIconButton::menu-indicator { image: none; width: 0px; height: 0px; }"));
        m_settings_button->resize(toolbar_button_size);
    }
    return;
#else
    const QString toolbar_icon_button_style = QStringLiteral(
        "QToolButton#toolbarIconButton::menu-indicator { image: none; width: 0px; height: 0px; }");
    const QString theme = GUIUtil::getConfiguredAppearanceTheme();
    const GUIUtil::ThemeDescriptor theme_descriptor = GUIUtil::appearanceThemeDescriptor(theme);
    m_theme_cycle_action->setIcon(GUIUtil::themeAssetIcon(QStringLiteral("theme"), QStringLiteral(":/icons/theme_auto"), theme));
    m_theme_cycle_action->setText(theme_descriptor.shortName.isEmpty() ? theme_descriptor.name : theme_descriptor.shortName);
    m_theme_cycle_action->setToolTip(GUIUtil::appearanceThemeToolTip(theme));

    const QSize toolbar_icon_size(34, 34);
    const QSize toolbar_button_size(48, 48);
    if (appToolBar) appToolBar->setIconSize(toolbar_icon_size);
    if (m_mask_values_button) {
        m_mask_values_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_mask_values_button->setIconSize(toolbar_icon_size);
        m_mask_values_button->setMinimumSize(toolbar_button_size);
        m_mask_values_button->setMaximumSize(toolbar_button_size);
        m_mask_values_button->setStyleSheet(toolbar_icon_button_style);
        m_mask_values_button->resize(toolbar_button_size);
    }
    if (m_settings_button) {
        m_settings_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_settings_button->setIconSize(toolbar_icon_size);
        m_settings_button->setMinimumSize(toolbar_button_size);
        m_settings_button->setMaximumSize(toolbar_button_size);
        m_settings_button->setStyleSheet(toolbar_icon_button_style);
        m_settings_button->resize(toolbar_button_size);
    }
    if (m_theme_button) {
        m_theme_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_theme_button->setIcon(m_theme_cycle_action->icon());
        m_theme_button->setIconSize(toolbar_icon_size);
        m_theme_button->setMinimumSize(toolbar_button_size);
        m_theme_button->setMaximumSize(toolbar_button_size);
        m_theme_button->setToolTip(m_theme_cycle_action->toolTip());
        m_theme_button->setStyleSheet(toolbar_icon_button_style);
        m_theme_button->resize(toolbar_button_size);
    }
#endif
}

void BitcoinGUI::updateMaskValuesIcon()
{
    if (!m_mask_values_action) return;
    const bool masked = m_mask_values_action->isChecked();
    const QString icon_path = masked ? QStringLiteral(":/icons/mask_values_hidden") : QStringLiteral(":/icons/mask_values_visible");
#if ENABLE_DEFCOIN_FUN_UI
    m_mask_values_action->setIcon(GUIUtil::themeAssetIcon(masked ? QStringLiteral("mask_values_hidden") : QStringLiteral("mask_values_visible"), icon_path));
#else
    m_mask_values_action->setIcon(QIcon(icon_path));
#endif
    m_mask_values_action->setToolTip(masked ? tr("Unmask Overview values") : tr("Mask Overview values"));
    if (m_mask_values_button) {
        m_mask_values_button->setIcon(m_mask_values_action->icon());
        m_mask_values_button->setToolTip(m_mask_values_action->toolTip());
    }
}

void BitcoinGUI::rebuildThemeMenus()
{
    const QVector<GUIUtil::ThemeDescriptor> themes = GUIUtil::availableAppearanceThemes();
    const QString current = GUIUtil::getConfiguredAppearanceTheme();

    const auto populate_menu = [this, &themes, &current](QMenu* menu) {
        if (!menu) return;
        menu->clear();
        for (const GUIUtil::ThemeDescriptor& theme : themes) {
            QAction* action = menu->addAction(theme.name);
            action->setData(theme.id);
            action->setCheckable(true);
            action->setChecked(theme.id == current);
            if (!theme.description.isEmpty()) action->setToolTip(theme.description);
            connect(action, &QAction::triggered, [this, theme] {
                setAppearanceTheme(theme.id);
            });
        }
    };

    populate_menu(m_theme_menu);
    populate_menu(m_settings_theme_menu);
}

void BitcoinGUI::updateThemedIcons()
{
#if ENABLE_DEFCOIN_FUN_UI
    if (overviewAction) overviewAction->setIcon(GUIUtil::themeAssetIcon(QStringLiteral("overview"), QStringLiteral(":/icons/overview")));
    if (sendCoinsAction) sendCoinsAction->setIcon(GUIUtil::themeAssetIcon(QStringLiteral("send"), QStringLiteral(":/icons/send")));
    if (receiveCoinsAction) receiveCoinsAction->setIcon(GUIUtil::themeAssetIcon(QStringLiteral("receive"), QStringLiteral(":/icons/receiving_addresses")));
    if (historyAction) historyAction->setIcon(GUIUtil::themeAssetIcon(QStringLiteral("transactions"), QStringLiteral(":/icons/history")));
    if (optionsAction) optionsAction->setIcon(GUIUtil::themeAssetIcon(QStringLiteral("settings"), QStringLiteral(":/icons/settings")));
    if (openRPCConsoleAction) openRPCConsoleAction->setIcon(GUIUtil::themeAssetIcon(QStringLiteral("node"), QStringLiteral(":/icons/node")));
    if (m_settings_button) m_settings_button->setIcon(GUIUtil::themeAssetIcon(QStringLiteral("settings"), QStringLiteral(":/icons/settings")));
    if (m_network_toggle_action) m_network_toggle_action->setIcon(GUIUtil::themeAssetIcon(QStringLiteral("network_connected"), QStringLiteral(":/icons/network_connected")));
#else
    if (overviewAction) overviewAction->setIcon(platformStyle->SingleColorIcon(QStringLiteral(":/icons/overview")));
    if (sendCoinsAction) sendCoinsAction->setIcon(platformStyle->SingleColorIcon(QStringLiteral(":/icons/send")));
    if (receiveCoinsAction) receiveCoinsAction->setIcon(platformStyle->SingleColorIcon(QStringLiteral(":/icons/receiving_addresses")));
    if (historyAction) historyAction->setIcon(platformStyle->SingleColorIcon(QStringLiteral(":/icons/history")));
    if (optionsAction) optionsAction->setIcon(platformStyle->SingleColorIcon(QStringLiteral(":/icons/settings")));
    if (openRPCConsoleAction) openRPCConsoleAction->setIcon(QIcon(QStringLiteral(":/icons/node")));
    if (m_settings_button) m_settings_button->setIcon(platformStyle->SingleColorIcon(QStringLiteral(":/icons/settings")));
    if (m_network_toggle_action) m_network_toggle_action->setIcon(QIcon(QStringLiteral(":/icons/network_connected")));
#endif

    updateMaskValuesIcon();
    updateAppearanceThemeButton();
    updateNetworkState();
    if (clientModel && labelProxyIcon) updateProxyIcon();
#ifdef ENABLE_WALLET
    if (walletFrame && labelWalletEncryptionIcon && labelWalletHDStatusIcon) updateWalletStatus();
#endif
}

void BitcoinGUI::setNumBlocks(int count, const QDateTime& blockDate, double nVerificationProgress, bool header, SynchronizationState sync_state)
{
// Disabling macOS App Nap on initial sync, disk and reindex operations.
#ifdef Q_OS_MAC
    if (sync_state == SynchronizationState::POST_INIT) {
        m_app_nap_inhibitor->enableAppNap();
    } else {
        m_app_nap_inhibitor->disableAppNap();
    }
#endif

    if (modalOverlay)
    {
        if (header)
            modalOverlay->setKnownBestHeight(count, blockDate);
        else
            modalOverlay->tipUpdate(count, blockDate, nVerificationProgress);
    }
    if (!clientModel)
        return;

    // Prevent orphan statusbar messages (e.g. hover Quit in main menu, wait until chain-sync starts -> garbled text)
    statusBar()->clearMessage();

    // Acquire current block source
    enum BlockSource blockSource = clientModel->getBlockSource();
    switch (blockSource) {
        case BlockSource::NETWORK:
            if (header) {
                updateHeadersSyncProgressLabel();
                return;
            }
            progressBarLabel->setText(tr("Synchronizing with network..."));
            updateHeadersSyncProgressLabel();
            break;
        case BlockSource::DISK:
            if (header) {
                progressBarLabel->setText(tr("Indexing blocks on disk..."));
            } else {
                progressBarLabel->setText(tr("Processing blocks on disk..."));
            }
            break;
        case BlockSource::REINDEX:
            progressBarLabel->setText(tr("Reindexing blocks on disk..."));
            break;
        case BlockSource::NONE:
            if (header) {
                return;
            }
            progressBarLabel->setText(tr("Connecting to peers..."));
            break;
    }

    QString tooltip;

    QDateTime currentDate = QDateTime::currentDateTime();
    qint64 secs = blockDate.secsTo(currentDate);

    tooltip = tr("Processed %n block(s) of transaction history.", "", count);

    // Set icon state: spinning if catching up, tick otherwise
    if (secs < MAX_BLOCK_TIME_GAP) {
        m_block_sync_up_to_date = true;
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
#if ENABLE_DEFCOIN_FUN_UI
        if (labelBlocksIcon) labelBlocksIcon->setPixmap(GUIUtil::themeAssetIcon(QStringLiteral("sync_status"), QStringLiteral(":/icons/synced")).pixmap(STATUSBAR_WIDE_ICON_WIDTH, STATUSBAR_WIDE_ICON_HEIGHT));
#else
        if (labelBlocksIcon) labelBlocksIcon->setPixmap(platformStyle->SingleColorIcon(QStringLiteral(":/icons/synced")).pixmap(STATUSBAR_WIDE_ICON_WIDTH, STATUSBAR_WIDE_ICON_HEIGHT));
#endif

#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->showOutOfSyncWarning(false);
            modalOverlay->showHide(true, true);
        }
#endif // ENABLE_WALLET

        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);
    }
    else
    {
        m_block_sync_up_to_date = false;
        QString timeBehindText = GUIUtil::formatNiceTimeOffset(secs);

        progressBarLabel->setVisible(true);
        progressBar->setFormat(tr("%1 behind").arg(timeBehindText));
        progressBar->setMaximum(1000000000);
        progressBar->setValue(nVerificationProgress * 1000000000.0 + 0.5);
        progressBar->setVisible(true);

        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
        if (labelBlocksIcon) {
            labelBlocksIcon->setPixmap(GUIUtil::themeAssetIcon(QStringLiteral("syncing"), QStringLiteral(":/icons/syncing"))
                .pixmap(STATUSBAR_WIDE_ICON_WIDTH, STATUSBAR_WIDE_ICON_HEIGHT));
        }
        if(count != prevBlocks)
        {
            spinnerFrame = (spinnerFrame + 1) % SPINNER_FRAMES;
        }
        prevBlocks = count;

#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->showOutOfSyncWarning(true);
            modalOverlay->showHide();
        }
#endif // ENABLE_WALLET

        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1 ago.").arg(timeBehindText);
        tooltip += QString("<br>");
        tooltip += tr("Transactions after this will not yet be visible.");
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    if (labelBlocksIcon) labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
    updateNetworkState();
}

void BitcoinGUI::message(const QString& title, QString message, unsigned int style, bool* ret, const QString& detailed_message)
{
    // Default title. On macOS, the window title is ignored (as required by the macOS Guidelines).
    QString strTitle{PACKAGE_NAME};
    // Default to information icon
    int nMBoxIcon = QMessageBox::Information;
    int nNotifyIcon = Notificator::Information;

    QString msgType;
    if (!title.isEmpty()) {
        msgType = title;
    } else {
        switch (style) {
        case CClientUIInterface::MSG_ERROR:
            msgType = tr("Error");
            message = tr("Error: %1").arg(message);
            break;
        case CClientUIInterface::MSG_WARNING:
            msgType = tr("Warning");
            message = tr("Warning: %1").arg(message);
            break;
        case CClientUIInterface::MSG_INFORMATION:
            msgType = tr("Information");
            // No need to prepend the prefix here.
            break;
        default:
            break;
        }
    }

    if (!msgType.isEmpty()) {
        strTitle += " - " + msgType;
    }

    if (style & CClientUIInterface::ICON_ERROR) {
        nMBoxIcon = QMessageBox::Critical;
        nNotifyIcon = Notificator::Critical;
    } else if (style & CClientUIInterface::ICON_WARNING) {
        nMBoxIcon = QMessageBox::Warning;
        nNotifyIcon = Notificator::Warning;
    }

    if (style & CClientUIInterface::MODAL) {
        // Check for buttons, use OK as default, if none was supplied
        QMessageBox::StandardButton buttons;
        if (!(buttons = (QMessageBox::StandardButton)(style & CClientUIInterface::BTN_MASK)))
            buttons = QMessageBox::Ok;

        showNormalIfMinimized();
        QMessageBox mBox(static_cast<QMessageBox::Icon>(nMBoxIcon), strTitle, message, buttons, this);
        mBox.setTextFormat(Qt::PlainText);
        mBox.setDetailedText(detailed_message);
        int r = mBox.exec();
        if (ret != nullptr)
            *ret = r == QMessageBox::Ok;
    } else {
        notificator->notify(static_cast<Notificator::Class>(nNotifyIcon), strTitle, message);
    }
}

void BitcoinGUI::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel() && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, &BitcoinGUI::hide);
                e->ignore();
            }
            else if((wsevt->oldState() & Qt::WindowMinimized) && !isMinimized())
            {
                QTimer::singleShot(0, this, &BitcoinGUI::show);
                e->ignore();
            }
        }
    }
#endif
}

void BitcoinGUI::closeEvent(QCloseEvent *event)
{
#ifndef Q_OS_MAC // Ignored on Mac
    if(clientModel && clientModel->getOptionsModel())
    {
        if(!clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            // close rpcConsole in case it was open to make some space for the shutdown window
            rpcConsole->close();

            QApplication::quit();
        }
        else
        {
            QMainWindow::showMinimized();
            event->ignore();
        }
    }
#else
    QMainWindow::closeEvent(event);
#endif
}

void BitcoinGUI::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // enable the debug window when the main window shows up
    openRPCConsoleAction->setEnabled(true);
    aboutAction->setEnabled(true);
    optionsAction->setEnabled(true);
    maybeScheduleUiCapture();
}

#ifdef ENABLE_WALLET
void BitcoinGUI::incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address, const QString& label, const QString& walletName)
{
    // On new transaction, make an info balloon
    QString msg = tr("Date: %1\n").arg(date) +
                  tr("Amount: %1\n").arg(BitcoinUnits::formatWithUnit(unit, amount, true));
    if (m_node.walletClient().getWallets().size() > 1 && !walletName.isEmpty()) {
        msg += tr("Wallet: %1\n").arg(walletName);
    }
    msg += tr("Type: %1\n").arg(type);
    if (!label.isEmpty())
        msg += tr("Label: %1\n").arg(label);
    else if (!address.isEmpty())
        msg += tr("Address: %1\n").arg(address);
    message((amount)<0 ? tr("Sent transaction") : tr("Incoming transaction"),
             msg, CClientUIInterface::MSG_INFORMATION);
}
#endif // ENABLE_WALLET

void BitcoinGUI::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void BitcoinGUI::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        for (const QUrl &uri : event->mimeData()->urls())
        {
            Q_EMIT receivedURI(uri.toString());
        }
    }
    event->acceptProposedAction();
}

bool BitcoinGUI::eventFilter(QObject *object, QEvent *event)
{
    // Catch status tip events
    if (event->type() == QEvent::StatusTip)
    {
        // Prevent adding text from setStatusTip(), if we currently use the status bar for displaying other stuff
        if (progressBarLabel->isVisible() || progressBar->isVisible())
            return true;
    }
    return QMainWindow::eventFilter(object, event);
}

#ifdef ENABLE_WALLET
bool BitcoinGUI::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    // URI has to be valid
    if (walletFrame && walletFrame->handlePaymentRequest(recipient))
    {
        showNormalIfMinimized();
        gotoSendCoinsPage();
        return true;
    }
    return false;
}

void BitcoinGUI::setHDStatus(bool privkeyDisabled, int hdEnabled)
{
    const QString asset_name = privkeyDisabled ? QStringLiteral("eye") : hdEnabled ? QStringLiteral("hd_enabled") : QStringLiteral("hd_disabled");
    const QString fallback = privkeyDisabled ? QStringLiteral(":/icons/eye") : hdEnabled ? QStringLiteral(":/icons/hd_enabled") : QStringLiteral(":/icons/hd_disabled");
#if ENABLE_DEFCOIN_FUN_UI
    labelWalletHDStatusIcon->setPixmap(GUIUtil::themeAssetIcon(asset_name, fallback).pixmap(STATUSBAR_WIDE_ICON_WIDTH, STATUSBAR_WIDE_ICON_HEIGHT));
#else
    Q_UNUSED(asset_name);
    labelWalletHDStatusIcon->setPixmap(privkeyDisabled ?
        platformStyle->SingleColorIcon(fallback).pixmap(STATUSBAR_WIDE_ICON_WIDTH, STATUSBAR_WIDE_ICON_HEIGHT) :
        QIcon(fallback).pixmap(STATUSBAR_WIDE_ICON_WIDTH, STATUSBAR_WIDE_ICON_HEIGHT));
#endif
    const QString hd_explanation = tr("Hierarchical Deterministic (HD) key generation creates private and public keys from one secure master seed based on BIP-32. This allows one wallet backup to restore the wallet's generated key chain and addresses.");
    labelWalletHDStatusIcon->setToolTip(privkeyDisabled ? tr("Private key <b>disabled</b>") : hdEnabled ? tr("HD key generation enabled.<br>%1").arg(hd_explanation) : tr("HD key generation disabled.<br>%1").arg(hd_explanation));
    labelWalletHDStatusIcon->show();
    // eventually disable the QLabel to set its opacity to 50%
    labelWalletHDStatusIcon->setEnabled(hdEnabled);
}

void BitcoinGUI::setEncryptionStatus(int status)
{
    switch(status)
    {
    case WalletModel::Unencrypted:
        labelWalletEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        labelWalletEncryptionIcon->show();
#if ENABLE_DEFCOIN_FUN_UI
        labelWalletEncryptionIcon->setPixmap(GUIUtil::themeAssetIcon(QStringLiteral("lock_open"), QStringLiteral(":/icons/lock_open")).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
#else
        labelWalletEncryptionIcon->setPixmap(platformStyle->SingleColorIcon(QStringLiteral(":/icons/lock_open")).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
#endif
        labelWalletEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false);
        break;
    case WalletModel::Locked:
        labelWalletEncryptionIcon->show();
#if ENABLE_DEFCOIN_FUN_UI
        labelWalletEncryptionIcon->setPixmap(GUIUtil::themeAssetIcon(QStringLiteral("lock_closed"), QStringLiteral(":/icons/lock_closed")).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
#else
        labelWalletEncryptionIcon->setPixmap(platformStyle->SingleColorIcon(QStringLiteral(":/icons/lock_closed")).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
#endif
        labelWalletEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false);
        break;
    }
}

void BitcoinGUI::updateWalletStatus()
{
    if (!walletFrame) {
        return;
    }
    WalletView * const walletView = walletFrame->currentWalletView();
    if (!walletView) {
        return;
    }
    WalletModel * const walletModel = walletView->getWalletModel();
    setEncryptionStatus(walletModel->getEncryptionStatus());
    setHDStatus(walletModel->wallet().privateKeysDisabled(), walletModel->wallet().hdEnabled());
}
#endif // ENABLE_WALLET

void BitcoinGUI::updateProxyIcon()
{
    if (!clientModel || !labelProxyIcon) return;

    std::string ip_port;
    bool proxy_enabled = clientModel->getProxyInfo(ip_port);

    if (proxy_enabled) {
        if (labelProxyIcon->pixmap() == nullptr) {
            QString ip_port_q = QString::fromStdString(ip_port);
#if ENABLE_DEFCOIN_FUN_UI
            labelProxyIcon->setPixmap(GUIUtil::themeAssetIcon(QStringLiteral("proxy"), QStringLiteral(":/icons/proxy")).pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
#else
            labelProxyIcon->setPixmap(platformStyle->SingleColorIcon(QStringLiteral(":/icons/proxy")).pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
#endif
            labelProxyIcon->setToolTip(tr("Proxy is <b>enabled</b>: %1").arg(ip_port_q));
        } else {
            labelProxyIcon->show();
        }
    } else {
        labelProxyIcon->hide();
    }
}

void BitcoinGUI::updateWindowTitle()
{
    QString window_title = ENABLE_DEFCOIN_FUN_UI ? tr("%1 DC34 Edition").arg(PACKAGE_NAME) : QString(PACKAGE_NAME);
#ifdef ENABLE_WALLET
    if (walletFrame) {
        WalletModel* const wallet_model = walletFrame->currentWalletModel();
        if (wallet_model && !wallet_model->getWalletName().isEmpty()) {
            window_title += " - " + wallet_model->getDisplayName();
        }
    }
#endif
    if (!m_network_style->getTitleAddText().isEmpty()) {
        window_title += " - " + m_network_style->getTitleAddText();
    }
    setWindowTitle(window_title);
}

void BitcoinGUI::showNormalIfMinimized(bool fToggleHidden)
{
    if(!clientModel)
        return;

    if (!isHidden() && !isMinimized() && !GUIUtil::isObscured(this) && fToggleHidden) {
        hide();
    } else {
        GUIUtil::bringToFront(this);
    }
}

void BitcoinGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void BitcoinGUI::detectShutdown()
{
    if (m_node.shutdownRequested())
    {
        if(rpcConsole)
            rpcConsole->hide();
        qApp->quit();
    }
}

void BitcoinGUI::showProgress(const QString &title, int nProgress)
{
    if (nProgress == 0) {
        progressDialog = new QProgressDialog(title, QString(), 0, 100);
        GUIUtil::PolishProgressDialog(progressDialog);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    } else if (nProgress == 100) {
        if (progressDialog) {
            progressDialog->close();
            progressDialog->deleteLater();
            progressDialog = nullptr;
        }
    } else if (progressDialog) {
        progressDialog->setValue(nProgress);
    }
}

void BitcoinGUI::setTrayIconVisible(bool fHideTrayIcon)
{
    if (trayIcon)
    {
        trayIcon->setVisible(!fHideTrayIcon);
    }
}

void BitcoinGUI::showModalOverlay()
{
    if (modalOverlay && (progressBar->isVisible() || modalOverlay->isLayerVisible()))
        modalOverlay->toggleVisibility();
}

static bool ThreadSafeMessageBox(BitcoinGUI* gui, const bilingual_str& message, const std::string& caption, unsigned int style)
{
    bool modal = (style & CClientUIInterface::MODAL);
    // The SECURE flag has no effect in the Qt GUI.
    // bool secure = (style & CClientUIInterface::SECURE);
    style &= ~CClientUIInterface::SECURE;
    bool ret = false;

    QString detailed_message; // This is original message, in English, for googling and referencing.
    if (message.original != message.translated) {
        detailed_message = BitcoinGUI::tr("Original message:") + "\n" + QString::fromStdString(message.original);
    }

    // In case of modal message, use blocking connection to wait for user to click a button
    bool invoked = QMetaObject::invokeMethod(gui, "message",
                               modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
                               Q_ARG(QString, QString::fromStdString(caption)),
                               Q_ARG(QString, QString::fromStdString(message.translated)),
                               Q_ARG(unsigned int, style),
                               Q_ARG(bool*, &ret),
                               Q_ARG(QString, detailed_message));
    assert(invoked);
    return ret;
}

void BitcoinGUI::subscribeToCoreSignals()
{
    // Connect signals to client
    m_handler_message_box = m_node.handleMessageBox(std::bind(ThreadSafeMessageBox, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    m_handler_question = m_node.handleQuestion(std::bind(ThreadSafeMessageBox, this, std::placeholders::_1, std::placeholders::_3, std::placeholders::_4));
}

void BitcoinGUI::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    m_handler_message_box->disconnect();
    m_handler_question->disconnect();
}

bool BitcoinGUI::isPrivacyModeActivated() const
{
    assert(m_mask_values_action);
    return m_mask_values_action->isChecked();
}

UnitDisplayStatusBarControl::UnitDisplayStatusBarControl(const PlatformStyle *platformStyle) :
    optionsModel(nullptr),
    menu(nullptr)
{
    Q_UNUSED(platformStyle);
    createContextMenu();
    setToolTip(tr("Unit to show amounts in. Click to select another unit."));
    QList<BitcoinUnits::Unit> units = BitcoinUnits::availableUnits();
    int max_width = 0;
    const QFontMetrics fm(font());
    for (const BitcoinUnits::Unit unit : units)
    {
        max_width = qMax(max_width, GUIUtil::TextWidth(fm, BitcoinUnits::longName(unit)));
    }
    setMinimumSize(max_width, 0);
    setAlignment(Qt::AlignCenter);
}

/** So that it responds to button clicks */
void UnitDisplayStatusBarControl::mousePressEvent(QMouseEvent *event)
{
    onDisplayUnitsClicked(event->pos());
}

/** Creates context menu, its actions, and wires up all the relevant signals for mouse events. */
void UnitDisplayStatusBarControl::createContextMenu()
{
    menu = new QMenu(this);
    for (const BitcoinUnits::Unit u : BitcoinUnits::availableUnits())
    {
        QString label;
        switch (u) {
        case BitcoinUnits::BTC:
            label = tr("Defcoin (1.0)");
            break;
        case BitcoinUnits::mBTC:
            label = tr("Packet (0.001)");
            break;
        case BitcoinUnits::uBTC:
            label = tr("Tock (0.000001)");
            break;
        case BitcoinUnits::SAT:
            label = tr("Mote (0.00000001)");
            break;
        }
        QAction *menuAction = new QAction(label, this);
        menuAction->setData(QVariant(u));
        menu->addAction(menuAction);
    }
    connect(menu, &QMenu::triggered, this, &UnitDisplayStatusBarControl::onMenuSelection);
}

/** Lets the control know about the Options Model (and its signals) */
void UnitDisplayStatusBarControl::setOptionsModel(OptionsModel *_optionsModel)
{
    if (_optionsModel)
    {
        this->optionsModel = _optionsModel;

        // be aware of a display unit change reported by the OptionsModel object.
        connect(_optionsModel, &OptionsModel::displayUnitChanged, this, &UnitDisplayStatusBarControl::updateDisplayUnit);

        // initialize the display units label with the current value in the model.
        updateDisplayUnit(_optionsModel->getDisplayUnit());
    }
}

/** When Display Units are changed on OptionsModel it will refresh the display text of the control on the status bar */
void UnitDisplayStatusBarControl::updateDisplayUnit(int newUnits)
{
    setText(BitcoinUnits::longName(newUnits));
}

/** Shows context menu with Display Unit options by the mouse coordinates */
void UnitDisplayStatusBarControl::onDisplayUnitsClicked(const QPoint& point)
{
    QPoint globalPos = mapToGlobal(point);
    menu->exec(globalPos);
}

/** Tells underlying optionsModel to update its current display unit. */
void UnitDisplayStatusBarControl::onMenuSelection(QAction* action)
{
    if (action)
    {
        optionsModel->setDisplayUnit(action->data());
    }
}
