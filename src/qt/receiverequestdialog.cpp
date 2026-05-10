// Copyright (c) 2011-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/receiverequestdialog.h>
#include <qt/forms/ui_receiverequestdialog.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/qrimagewidget.h>
#include <qt/walletmodel.h>

#include <QDialog>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QSettings>
#include <QString>
#include <QUrl>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h> /* for USE_QRCODE */
#endif

namespace {
QString ExplorerAddressUrlTemplate(QString url)
{
    QString address_url = url;
    address_url.replace(QStringLiteral("/tx/%s"), QStringLiteral("/address/%s"), Qt::CaseInsensitive);
    address_url.replace(QStringLiteral("/transaction/%s"), QStringLiteral("/address/%s"), Qt::CaseInsensitive);
    address_url.replace(QStringLiteral("/transactions/%s"), QStringLiteral("/address/%s"), Qt::CaseInsensitive);
    return address_url;
}
}

ReceiveRequestDialog::ReceiveRequestDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ReceiveRequestDialog),
    model(nullptr)
{
    ui->setupUi(this);
    GUIUtil::handleCloseWindowShortcut(this);
}

ReceiveRequestDialog::~ReceiveRequestDialog()
{
    delete ui;
}

void ReceiveRequestDialog::setModel(WalletModel *_model)
{
    this->model = _model;

    if (_model)
        connect(_model->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &ReceiveRequestDialog::updateDisplayUnit);

    // update the display unit if necessary
    update();
}

void ReceiveRequestDialog::setInfo(const SendCoinsRecipient &_info)
{
    this->info = _info;
    setWindowTitle(tr("Request payment to %1").arg(info.label.isEmpty() ? info.address : info.label));
    QString uri = GUIUtil::formatBitcoinURI(info);

#ifdef USE_QRCODE
    if (ui->qr_code->setQR(uri, info.address)) {
        connect(ui->btnSaveAs, &QPushButton::clicked, ui->qr_code, &QRImageWidget::saveImage);
    } else {
        ui->btnSaveAs->setEnabled(false);
    }
#else
    ui->btnSaveAs->hide();
    ui->qr_code->hide();
#endif

    ui->uri_content->setText("<a href=\"" + uri + "\">" + GUIUtil::HtmlEscape(uri) + "</a>");
    ui->address_content->setText(info.address);

    if (!info.amount) {
        ui->amount_tag->hide();
        ui->amount_content->hide();
    } // Amount is set in updateDisplayUnit() slot.
    updateDisplayUnit();

    if (!info.label.isEmpty()) {
        ui->label_content->setText(info.label);
    } else {
        ui->label_tag->hide();
        ui->label_content->hide();
    }

    if (!info.message.isEmpty()) {
        ui->message_content->setText(info.message);
    } else {
        ui->message_tag->hide();
        ui->message_content->hide();
    }

    if (!model->getWalletName().isEmpty()) {
        ui->wallet_content->setText(model->getWalletName());
    } else {
        ui->wallet_tag->hide();
        ui->wallet_content->hide();
    }

    addExplorerLinkRow();
}

void ReceiveRequestDialog::updateDisplayUnit()
{
    if (!model) return;
    ui->amount_content->setText(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), info.amount));
}

void ReceiveRequestDialog::on_btnCopyURI_clicked()
{
    GUIUtil::setClipboard(GUIUtil::formatBitcoinURI(info));
}

void ReceiveRequestDialog::on_btnCopyAddress_clicked()
{
    GUIUtil::setClipboard(info.address);
}

void ReceiveRequestDialog::addExplorerLinkRow()
{
    if (!QSettings().value(QStringLiteral("ThirdPartyTxUrlsEnabled"), false).toBool()) return;
    if (info.address.isEmpty()) return;

    const QStringList urls = QSettings().value(QStringLiteral("strThirdPartyTxUrls"), QString()).toString().split(QStringLiteral("|"), QString::SkipEmptyParts);
    for (const QString& raw_url : urls) {
        const QString url = raw_url.trimmed();
        const QString host = QUrl(url, QUrl::StrictMode).host();
        if (host.isEmpty() || !url.contains(QStringLiteral("%s"))) continue;

        const QString address_url = ExplorerAddressUrlTemplate(url);
        const QString resolved_url = QString(address_url).replace(QStringLiteral("%s"), info.address);
        QWidget* row = new QWidget(this);
        QHBoxLayout* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 4, 0, 6);
        layout->setSpacing(8);

        QLabel* icon = new QLabel(row);
        icon->setPixmap(QIcon(QStringLiteral(":/icons/block_explorer_link")).pixmap(18, 18));
        icon->setFixedWidth(22);
        icon->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

        QLabel* link = new QLabel(row);
        link->setTextFormat(Qt::RichText);
        link->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextBrowserInteraction);
        link->setOpenExternalLinks(false);
        link->setWordWrap(true);
        link->setText(tr("Use %1 block explorer to open:<br>Wallet Address: <a href=\"%2\">%3</a>")
            .arg(host.toHtmlEscaped(), resolved_url.toHtmlEscaped(), info.address.toHtmlEscaped()));
        connect(link, &QLabel::linkActivated, this, [](const QString& link_url) {
            QDesktopServices::openUrl(QUrl::fromUserInput(link_url));
        });

        layout->addWidget(icon);
        layout->addWidget(link, 1);
        ui->gridLayout->addWidget(row, 8, 0, 1, 2);
        return;
    }
}
