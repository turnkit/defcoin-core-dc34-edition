// Copyright (c) 2011-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/utilitydialog.h>

#include <qt/forms/ui_helpmessagedialog.h>

#include <qt/guiutil.h>

#include <clientversion.h>
#include <init.h>
#include <util/system.h>
#include <util/strencodings.h>

#include <algorithm>

#include <stdio.h>

#include <QCloseEvent>
#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QRegExp>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>
#include <QTimer>
#include <QVBoxLayout>

/** "Help message" or "About" dialog box */
HelpMessageDialog::HelpMessageDialog(QWidget *parent, bool about) :
    HelpMessageDialog(parent, about ? Mode::About : Mode::CommandLine)
{
}

HelpMessageDialog::HelpMessageDialog(QWidget *parent, Mode mode) :
    QDialog(parent),
    ui(new Ui::HelpMessageDialog)
{
    ui->setupUi(this);

    const QString product_name = QString::fromStdString(FormatProductName());
    QString version = product_name + " " + tr("version") + " " + QString::fromStdString(FormatFullVersion());
    const QString codename = tr("Codename: %1").arg(QString::fromStdString(FormatReleaseCodename()));

    if (mode == Mode::About)
    {
        m_about_sound_on_accept = false;
        setWindowTitle(tr("About %1").arg(product_name));
        QPixmap about_logo = GUIUtil::themeAssetPixmap(QStringLiteral("about_logo"), QStringLiteral(":/icons/defcoin"));
        ui->aboutLogo->setPixmap(about_logo);

        std::string licenseInfo = LicenseInfo();
        /// HTML-format the license message from the core
        QString licenseInfoHTML = QString::fromStdString(LicenseInfo());
        // Make URLs clickable
        QRegExp uri("<(.*)>", Qt::CaseSensitive, QRegExp::RegExp2);
        uri.setMinimal(true); // use non-greedy matching
        licenseInfoHTML.replace(uri, "<a href=\"\\1\">\\1</a>");
        // Replace newlines with HTML breaks
        licenseInfoHTML.replace("\n", "<br>");
        const QString seedInfoHTML =
            "<br><b>" + tr("Mainnet seed addresses") + "</b><br>"
            "seed.defcoin.io<br>"
            "seed.defcoin.mikej.tech<br>"
            "seed.defcoin.dc903.org<br>"
            "seed.defcoincore.org";

        ui->aboutMessage->setTextFormat(Qt::RichText);
        ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        text = version + "\n" + codename + "\n" + QString::fromStdString(FormatParagraph(licenseInfo)) +
               "\nMainnet seed addresses:\n"
               "seed.defcoin.io\n"
               "seed.defcoin.mikej.tech\n"
               "seed.defcoin.dc903.org\n"
               "seed.defcoincore.org";
        ui->aboutMessage->setText(version + "<br>" + codename + "<br><br>" + licenseInfoHTML + seedInfoHTML);
        ui->aboutMessage->setWordWrap(true);
        ui->helpMessage->setVisible(false);
    } else if (mode == Mode::WalletHelp) {
        setWindowTitle(tr("Defcoin Core Help Manual"));
        resize(820, 560);

        const QString help_html = QStringLiteral(R"HTML(
<style>
body { line-height: 1.35; }
h2 { margin-top: 0; }
h3 { margin-top: 18px; }
dt { font-weight: 600; margin-top: 8px; }
dd { margin-bottom: 6px; }
.note { color: #6c737d; }
</style>
<h2>Defcoin Core Help Manual</h2>
<p><b>Defcoin Core</b> is a native full-node wallet for the Defcoin network. It validates blocks, relays peer-to-peer traffic, stores wallet data locally, and gives you direct control over receiving, sending, backup, encryption, and node operation.</p>

<h3>Intro Guide</h3>
<p>Use Defcoin Core when you want the wallet and the validating node on your own computer. The wallet view manages keys and transactions. The node view shows synchronization, peer connectivity, traffic, latency, and local diagnostics.</p>
<p>Create or open wallets carefully. Back up <b>wallet.dat</b> before upgrades, imports, rescans, recovery work, or moving to another computer. If the wallet is encrypted, keep the passphrase offline and test backups before relying on them.</p>
<p>To receive DFC, create a receiving address and share it with the sender. To send DFC, enter the destination, amount, and fee settings, then review the confirmation dialog before broadcasting. Confirmations show how deeply a transaction is buried in the blockchain.</p>
<p>Historically, Defcoin has been used by its community online and in DC34-related in-person contexts, including badge-oriented experiments and paper-wallet style exchanges. This is community history, not a current official DC34 endorsement.</p>

<h3>Main Wallet Pages</h3>
<dl>
<dt>Overview</dt><dd>Shows balances, recent transactions, synchronization status, and quick wallet state.</dd>
<dt>Send</dt><dd>Creates outgoing Defcoin payments. Confirm the destination address, amount, fee, and change behavior before sending.</dd>
<dt>Receive</dt><dd>Creates receive requests and addresses. A receive request is a convenience record; the actual spendable object is the address and the transaction that later pays it.</dd>
<dt>Transactions</dt><dd>Lists wallet activity. Double-click a transaction for detail, confirmations, transaction ID, and status.</dd>
<dt>Node</dt><dd>Opens node diagnostics: Information, Console, Network Traffic, Peers, banned peers, and peer inspection tools.</dd>
</dl>

<h3>Lower-Right Status Icons</h3>
<dl>
<dt>Unit Selector</dt><dd>Changes display units only. It does not alter balances or transactions. Defcoin = 1.0 DFC, Packet = 0.001 DFC, Tock = 0.000001 DFC, and Mote = 0.00000001 DFC.</dd>
<dt>Wallet Lock</dt><dd>Shows encryption and lock state. Unlock only when spending or signing.</dd>
<dt>HD</dt><dd>Shows whether hierarchical deterministic key generation is enabled. HD wallets derive future addresses from wallet seed material, making backups more durable than one-backup-per-key workflows.</dd>
<dt>Proxy</dt><dd>Appears when proxy routing is active. Configure it in Preferences &gt; Network. Advanced settings include <b>proxy</b>, <b>onion</b>, and <b>proxyrandomize</b> in defcoin.conf or on the command line.</dd>
<dt>Network Activity</dt><dd>Shows whether the node is connected to peers. Click it to disable or re-enable network activity temporarily.</dd>
<dt>Synchronization Checkmark</dt><dd>Shows whether the node is up to date. Click it to inspect synchronization progress while catching up.</dd>
</dl>

<h3>Preferences</h3>
<dl>
<dt>Main</dt><dd>Controls startup behavior, database cache, pruning, and script verification threads.</dd>
<dt>Wallet</dt><dd>Controls wallet behavior such as coin control and spend-related display options.</dd>
<dt>Network</dt><dd>Controls incoming connections, UPnP, proxy routing, and reachability by IPv4, IPv6, Tor, or other supported transports.</dd>
<dt>Display</dt><dd>Controls language, theme, amount unit, third-party transaction URLs, and text-size behavior.</dd>
</dl>

<h3>Pruning and Storage</h3>
<p>Pruning saves disk space by deleting old raw block and undo files after those files have been validated and used to build the block index and UTXO database. A pruned node still validates the chain, relays transactions and recent blocks, and maintains wallet state.</p>
<p><b>Warning:</b> pruning limits old wallet rescans and is incompatible with transaction-index behavior. Returning to an unpruned node requires redownloading or reindexing the blockchain. If you expect to import old keys, rescan old wallets, or run explorer-style queries, keep an unpruned node.</p>

<h3>Node Window</h3>
<dl>
<dt>Information</dt><dd>Shows client version, user agent, data directory, blocks directory, connections, mempool size, chain height, and verification progress.</dd>
<dt>Console</dt><dd>Runs RPC commands. Use it only when you understand the command because RPC can change wallet or node state. The console is powerful and intentionally warns against pasting untrusted commands.</dd>
<dt>Network Traffic</dt><dd>Charts received bytes, sent bytes, recent average latency, peer-average latency, optional moving averages, and visible time ranges. Lower latency means a peer set is responding faster.</dd>
<dt>Peers</dt><dd>Lists connected, inactive, local, and optionally banned peers. Standard columns include Node ID, Direction, IP Address:Port, Recent Ping, Sent, Rec'd, and User Agent. DC34 Edition can add Geo, City/St, Mapped AS, AS Name, AS Hosting Company, Traffic Health, and other diagnostic columns.</dd>
<dt>Traffic Health</dt><dd>A compact heartbeat-style graph for each peer. It summarizes recent ping timing and traffic activity. Green generally means faster relative response, amber moderate response, and red slower response.</dd>
<dt>Lookup Geo</dt><dd>Best-effort city and state/region lookup. Public peers use their IP address; local and LAN rows use this node's public WAN address. Defcoin Core uses <a href="https://ip-api.com/docs">ip-api.com</a> for this lookup, caches results in memory for the Node window session, skips private/LAN/Tor/I2P addresses, and throttles requests below the public free-service limit.</dd>
<dt>Lookup Mapped AS</dt><dd>DC34 Edition only. Looks up public peer Autonomous System information using Team Cymru's DNS-based <a href="https://www.team-cymru.com/ip-asn-mapping">IP to ASN Mapping</a>. Defcoin Core uses cached DNS TXT answers, skips private/LAN/Tor/I2P addresses, and keeps lookup concurrency low. The AS Name and AS Hosting Company fields are derived from Team Cymru's AS-name TXT response when available.</dd>
<dt>Peer lookup sources</dt><dd>Geo and AS lookups send the peer's public IP address, or this node's public WAN address for local/LAN rows, to the lookup provider. Review <a href="https://ip-api.com/docs/legal">ip-api.com legal/privacy information</a> and <a href="https://www.team-cymru.com/ip-asn-mapping">Team Cymru's IP-ASN mapping documentation</a> before enabling these diagnostics on sensitive networks.</dd>
<dt>Trace Route</dt><dd>Opens a separate traceroute window for the selected public peer. This uses system networking tools and is diagnostic only.</dd>
<dt>Peer Map</dt><dd>Visualizes peers with usable location data on an animated globe when this build includes the map view. Real peer locations are approximate diagnostics, and remote-to-remote traffic lines are illustrative because this node cannot directly measure traffic exchanged between two other peers.</dd>
</dl>

<h3>Wallet Safety</h3>
<ul>
<li>Encrypt wallets that hold meaningful value.</li>
<li>Keep multiple offline backups of wallet.dat after creating or importing keys.</li>
<li>Do not paste commands from strangers into the Console.</li>
<li>Do not expose private keys to miners, pools, websites, or support chats.</li>
<li>Test restores with a copy before depending on a backup.</li>
</ul>

<h3>Partially Signed Bitcoin/Defcoin Transaction (PSBT)</h3>
<p>PSBT is a Bitcoin-origin transaction handoff format used by wallets and signers. In Defcoin Core it should be treated as an advanced signing workflow. Because Defcoin mainnet is intentionally legacy-compatible, do not assume every modern Bitcoin or Litecoin PSBT workflow is appropriate without testing.</p>

<h3>Unsupported or Compatibility-Limited Features</h3>
<p>Defcoin mainnet does not currently treat SegWit, CSV, CLTV, BIP66, Taproot, MWEB, or Signet as active Defcoin mainnet features. Some inherited Litecoin Core code paths still exist because the app is based on Litecoin Core, but the GUI should avoid presenting unsupported Litecoin-only behavior as a Defcoin feature.</p>

<h3>Glossary</h3>
<dl>
<dt>DFC</dt><dd>The primary Defcoin unit.</dd>
<dt>Packet</dt><dd>0.001 DFC.</dd>
<dt>Tock</dt><dd>0.000001 DFC.</dd>
<dt>Mote</dt><dd>0.00000001 DFC, the smallest displayed unit.</dd>
<dt>Berkeley DB wallet</dt><dd>The legacy wallet.dat database format used by historical Defcoin wallets.</dd>
<dt>FQDN</dt><dd>Fully qualified domain name, a complete DNS name for a host.</dd>
<dt>HD wallet</dt><dd>A hierarchical deterministic wallet that derives future keys from wallet seed material.</dd>
<dt>Latency</dt><dd>Network delay measured from peer ping timing. Lower is usually better.</dd>
<dt>Mempool</dt><dd>Transactions your node knows about that are waiting to be mined into a block.</dd>
<dt>Peer</dt><dd>Another node connected to your node.</dd>
<dt>Seed node</dt><dd>A DNS host or fixed address used to discover peers when your node starts.</dd>
</dl>

<h3>Further Reading</h3>
<ol>
<li><a href="https://bitcoincore.org/en/doc/">Bitcoin Core RPC documentation</a> for inherited RPC behavior.</li>
<li><a href="https://bitcoincore.org/en/releases/0.11.0/">Bitcoin Core 0.11.0 release notes</a> for pruning design and limitations.</li>
<li><a href="https://bips.dev/174/">BIP 174</a> for the PSBT standard.</li>
<li><a href="https://bips.dev/32/">BIP 32</a> for HD wallet terminology.</li>
<li><a href="https://bips.dev/155/">BIP 155</a> for modern peer address relay concepts.</li>
<li><a href="https://bitcoinops.org/en/river-descriptors-psbt/">Bitcoin Optech on descriptors and PSBT</a> for a practical explanation of signing workflows.</li>
<li>Terminology background: peer-to-peer (<a href="https://en.wikipedia.org/wiki/Peer-to-peer">Wikipedia</a> | <a href="https://grokipedia.com/page/Peer-to-peer">Grokipedia</a>), FQDN (<a href="https://en.wikipedia.org/wiki/Fully_qualified_domain_name">Wikipedia</a> | <a href="https://grokipedia.com/page/Fully_qualified_domain_name">Grokipedia</a>), latency (<a href="https://en.wikipedia.org/wiki/Latency_(engineering)">Wikipedia</a> | <a href="https://grokipedia.com/page/Latency_(engineering)">Grokipedia</a>), traceroute (<a href="https://en.wikipedia.org/wiki/Traceroute">Wikipedia</a> | <a href="https://grokipedia.com/page/Traceroute">Grokipedia</a>), and Berkeley DB (<a href="https://en.wikipedia.org/wiki/Berkeley_DB">Wikipedia</a> | <a href="https://grokipedia.com/page/Berkeley_DB">Grokipedia</a>).</li>
</ol>

<h3>Known Issues and Review Notes</h3>
<ul>
<li>Explicit addnode can connect to known IPv6 or non-default-port peers, but automatic discovery still depends on DNS seeds or peer address gossip advertising that exact address and port.</li>
<li>The "Only accept Defcoin-prefixed User Agents" setting improves network hygiene but may reduce discovery paths if legacy peers identify themselves with old Litecoin-style user agents while still relaying useful Defcoin addresses.</li>
<li>The Peer Map tab is a prototype. Map imagery, animation smoothness, and Windows performance are still under active review where the map view is included.</li>
<li>The DMG produced by local packaging is ad-hoc signed for testing. Public distribution should use Developer ID signing and notarization.</li>
<li>Geo and reverse-DNS data are best-effort diagnostics, not consensus or identity proof.</li>
</ul>
)HTML");

        QWidget* help_tools = new QWidget(this);
        QHBoxLayout* help_tools_layout = new QHBoxLayout(help_tools);
        help_tools_layout->setContentsMargins(0, 0, 0, 6);
        help_tools_layout->setSpacing(6);
        QComboBox* help_index = new QComboBox(help_tools);
        help_index->setToolTip(tr("Jump to a help topic"));
        const QStringList topics{
            tr("Intro Guide"),
            tr("Main Wallet Pages"),
            tr("Lower-Right Status Icons"),
            tr("Preferences"),
            tr("Pruning and Storage"),
            tr("Node Window"),
            tr("Wallet Safety"),
            tr("Partially Signed Bitcoin/Defcoin Transaction (PSBT)"),
            tr("Unsupported or Compatibility-Limited Features"),
            tr("Glossary"),
            tr("Further Reading"),
            tr("Known Issues and Review Notes")
        };
        for (const QString& topic : topics) {
            help_index->addItem(topic, topic);
        }
        QLineEdit* help_search = new QLineEdit(help_tools);
        help_search->setPlaceholderText(tr("Search help manual"));
        QPushButton* help_prev = new QPushButton(tr("Previous"), help_tools);
        QPushButton* help_next = new QPushButton(tr("Next"), help_tools);
        help_tools_layout->addWidget(new QLabel(tr("Index"), help_tools));
        help_tools_layout->addWidget(help_index, 0);
        help_tools_layout->addWidget(help_search, 1);
        help_tools_layout->addWidget(help_prev, 0);
        help_tools_layout->addWidget(help_next, 0);
        ui->verticalLayout->insertWidget(0, help_tools);

        text = tr("Defcoin Core Help Manual");
        ui->helpMessage->setAcceptRichText(true);
        ui->helpMessage->setHtml(help_html);
        ui->helpMessage->moveCursor(QTextCursor::Start);
        ui->scrollArea->setVisible(false);
        ui->aboutLogo->setVisible(false);
        auto find_help_text = [this, help_search](QTextDocument::FindFlags flags) {
            const QString needle = help_search->text().trimmed();
            if (needle.isEmpty()) return;
            if (ui->helpMessage->find(needle, flags)) return;
            QTextCursor cursor = ui->helpMessage->textCursor();
            cursor.movePosition(flags & QTextDocument::FindBackward ? QTextCursor::End : QTextCursor::Start);
            ui->helpMessage->setTextCursor(cursor);
            ui->helpMessage->find(needle, flags);
        };
        connect(help_next, &QPushButton::clicked, this, [find_help_text] {
            find_help_text(QTextDocument::FindFlags());
        });
        connect(help_prev, &QPushButton::clicked, this, [find_help_text] {
            find_help_text(QTextDocument::FindBackward);
        });
        connect(help_search, &QLineEdit::returnPressed, this, [find_help_text] {
            find_help_text(QTextDocument::FindFlags());
        });
        connect(help_index, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [this, help_index](int index) {
            const QString topic = help_index->itemData(index).toString();
            QTextCursor cursor = ui->helpMessage->textCursor();
            cursor.movePosition(QTextCursor::Start);
            ui->helpMessage->setTextCursor(cursor);
            ui->helpMessage->find(topic);
        });
    } else {
        setWindowTitle(tr("Command-line options"));
        QString header = "Usage:  defcoin-qt [command-line options]                     \n";
        QTextCursor cursor(ui->helpMessage->document());
        cursor.insertText(version);
        cursor.insertBlock();
        cursor.insertText(header);
        cursor.insertBlock();

        std::string strUsage = gArgs.GetHelpMessage();
        QString coreOptions = QString::fromStdString(strUsage);
        text = version + "\n\n" + header + "\n" + coreOptions;

        QTextTableFormat tf;
        tf.setBorderStyle(QTextFrameFormat::BorderStyle_None);
        tf.setCellPadding(2);
        QVector<QTextLength> widths;
        widths << QTextLength(QTextLength::PercentageLength, 35);
        widths << QTextLength(QTextLength::PercentageLength, 65);
        tf.setColumnWidthConstraints(widths);

        QTextCharFormat bold;
        bold.setFontWeight(QFont::Bold);

        for (const QString &line : coreOptions.split("\n")) {
            if (line.startsWith("  -"))
            {
                cursor.currentTable()->appendRows(1);
                cursor.movePosition(QTextCursor::PreviousCell);
                cursor.movePosition(QTextCursor::NextRow);
                cursor.insertText(line.trimmed());
                cursor.movePosition(QTextCursor::NextCell);
            } else if (line.startsWith("   ")) {
                cursor.insertText(line.trimmed()+' ');
            } else if (line.size() > 0) {
                //Title of a group
                if (cursor.currentTable())
                    cursor.currentTable()->appendRows(1);
                cursor.movePosition(QTextCursor::Down);
                cursor.insertText(line.trimmed(), bold);
                cursor.insertTable(1, 2, tf);
            }
        }

        ui->helpMessage->moveCursor(QTextCursor::Start);
        ui->scrollArea->setVisible(false);
        ui->aboutLogo->setVisible(false);
    }

    GUIUtil::handleCloseWindowShortcut(this);
}

HelpMessageDialog::~HelpMessageDialog()
{
    delete ui;
}

void HelpMessageDialog::printToConsole()
{
    // On other operating systems, the expected action is to print the message to the console.
    tfm::format(std::cout, "%s\n", qPrintable(text));
}

void HelpMessageDialog::showOrPrint()
{
#if defined(WIN32)
    // On Windows, show a message box, as there is no stderr/stdout in windowed applications
    exec();
#else
    // On other operating systems, print help text to console
    printToConsole();
#endif
}

void HelpMessageDialog::on_okButton_accepted()
{
    if (m_about_sound_on_accept) {
        QApplication::beep();
        QTimer::singleShot(95, qApp, [] { QApplication::beep(); });
        QTimer::singleShot(180, qApp, [] { QApplication::beep(); });
    }
    close();
}


/** "Shutdown" window */
ShutdownWindow::ShutdownWindow(QWidget *parent, Qt::WindowFlags f):
    QWidget(parent, f)
{
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(new QLabel(
        tr("%1 is shutting down...").arg(PACKAGE_NAME) + "<br /><br />" +
        tr("Do not shut down the computer until this window disappears.")));
    setLayout(layout);

    GUIUtil::handleCloseWindowShortcut(this);
}

QWidget* ShutdownWindow::showShutdownWindow(QMainWindow* window)
{
    assert(window != nullptr);

    // Show a simple window indicating shutdown status
    QWidget *shutdownWindow = new ShutdownWindow();
    shutdownWindow->setWindowTitle(window->windowTitle());

    // Center shutdown window at where main window was
    const QPoint global = window->mapToGlobal(window->rect().center());
    shutdownWindow->move(global.x() - shutdownWindow->width() / 2, global.y() - shutdownWindow->height() / 2);
    shutdownWindow->show();
    return shutdownWindow;
}

void ShutdownWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
}
