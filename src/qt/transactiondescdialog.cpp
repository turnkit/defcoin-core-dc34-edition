// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/transactiondescdialog.h>
#include <qt/forms/ui_transactiondescdialog.h>

#include <qt/guiutil.h>
#include <qt/transactiontablemodel.h>

#include <QModelIndex>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QSettings>
#include <QUrl>

TransactionDescDialog::TransactionDescDialog(const QModelIndex &idx, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TransactionDescDialog)
{
    ui->setupUi(this);
    setMinimumSize(760, 420);
    resize(800, 440);
    setWindowTitle(tr("Details for %1").arg(idx.data(TransactionTableModel::TxHashRole).toString()));
    QString desc = idx.data(TransactionTableModel::LongDescriptionRole).toString();
    ui->detailText->setHtml(desc);
    const QString tx_hash = idx.data(TransactionTableModel::TxHashRole).toString();
    const QString wallet_address = idx.data(TransactionTableModel::AddressRole).toString();
    if (!QSettings().value(QStringLiteral("ThirdPartyTxUrlsEnabled"), false).toBool()) {
        GUIUtil::handleCloseWindowShortcut(this);
        return;
    }

    const QStringList tx_urls = QSettings().value(QStringLiteral("strThirdPartyTxUrls"), QString()).toString().split("|", QString::SkipEmptyParts);
    for (const QString& raw_url : tx_urls) {
        const QString url = raw_url.trimmed();
        const QString host = QUrl(url, QUrl::StrictMode).host();
        if (host.isEmpty() || !url.contains(QStringLiteral("%s"))) continue;
        QString address_url = url;
        address_url.replace(QStringLiteral("/tx/%s"), QStringLiteral("/address/%s"), Qt::CaseInsensitive);
        address_url.replace(QStringLiteral("/transaction/%s"), QStringLiteral("/address/%s"), Qt::CaseInsensitive);
        QString html = tr("Use %1 block explorer to open:").arg(host.toHtmlEscaped());
        html += QStringLiteral("<br>");
        html += tr("Transaction ID: <a href=\"%1\">%2</a>").arg(QString(url).replace(QStringLiteral("%s"), tx_hash).toHtmlEscaped(), tx_hash.toHtmlEscaped());
        if (!wallet_address.isEmpty()) {
            html += QStringLiteral("<br>");
            html += tr("Wallet Address: <a href=\"%1\">%2</a>").arg(QString(address_url).replace(QStringLiteral("%s"), wallet_address).toHtmlEscaped(), wallet_address.toHtmlEscaped());
        }
        QLabel* link = new QLabel(html, this);
        link->setTextFormat(Qt::RichText);
        link->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextBrowserInteraction);
        link->setOpenExternalLinks(false);
        connect(link, &QLabel::linkActivated, this, [](const QString& link_url) {
            QDesktopServices::openUrl(QUrl::fromUserInput(link_url));
        });
        QWidget* link_row = new QWidget(this);
        QHBoxLayout* link_layout = new QHBoxLayout(link_row);
        link_layout->setContentsMargins(0, 2, 0, 4);
        link_layout->setSpacing(8);
        QLabel* icon = new QLabel(link_row);
        icon->setPixmap(QIcon(QStringLiteral(":/icons/block_explorer_link")).pixmap(18, 18));
        icon->setFixedWidth(22);
        icon->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        link_layout->addWidget(icon);
        link_layout->addWidget(link, 1);
        ui->verticalLayout->insertWidget(1, link_row);
        break;
    }

    GUIUtil::handleCloseWindowShortcut(this);
}

TransactionDescDialog::~TransactionDescDialog()
{
    delete ui;
}
