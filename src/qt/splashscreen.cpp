// Copyright (c) 2011-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/splashscreen.h>

#include <clientversion.h>
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <logging.h>
#include <qt/guiutil.h>
#include <qt/networkstyle.h>
#include <qt/walletmodel.h>
#include <util/system.h>
#include <util/translation.h>

#include <QApplication>
#include <QCloseEvent>
#include <QEventLoop>
#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPolygonF>
#include <QRadialGradient>
#include <QScreen>
#include <QStringList>

#include <algorithm>

SplashScreen::SplashScreen(Qt::WindowFlags f, const NetworkStyle *networkStyle) :
    QWidget(nullptr, f), curAlignment(0)
{
    m_elapsed_timer.start();
    m_alive_timer = new QTimer(this);
    connect(m_alive_timer, &QTimer::timeout, this, QOverload<>::of(&SplashScreen::update));
    m_alive_timer->start(1000);

    const int splashWidth       = 640;
    const int splashHeight      = 360;
    const int textLeft          = 282;
    const int textRightPadding  = 34;
    const int titleTop          = ENABLE_DEFCOIN_FUN_UI ? 90 : 112;
    const int rightTextWidth    = splashWidth - textLeft - textRightPadding;

    float devicePixelRatio      = 1.0;
    devicePixelRatio = static_cast<QGuiApplication*>(QCoreApplication::instance())->devicePixelRatio();

    // define text to place
    const QString titleText = QString(PACKAGE_NAME);
    const QString subtitleText = ENABLE_DEFCOIN_FUN_UI ? tr("DC34 Edition") : QString();
    const QString windowTitleText = ENABLE_DEFCOIN_FUN_UI ? tr("%1 DC34 Edition").arg(PACKAGE_NAME) : titleText;
    QString versionText     = QString("Version %1").arg(QString::fromStdString(FormatFullVersion()));
    QString codenameText    = QString("Codename: %1").arg(QString::fromStdString(FormatReleaseCodename()));
    QString copyrightText   = QString::fromUtf8(strprintf("\xc2\xa9 2014-%u The Defcoin Core developers\n\xc2\xa9 2011-%u The Litecoin Core developers\n\xc2\xa9 2009-%u The Bitcoin Core developers", COPYRIGHT_YEAR, COPYRIGHT_YEAR, COPYRIGHT_YEAR).c_str());
    QString titleAddText    = networkStyle->getTitleAddText();

    QString font            = QApplication::font().toString();

    // create a bitmap according to device pixelratio
    QSize splashSize(splashWidth*devicePixelRatio,splashHeight*devicePixelRatio);
    pixmap = QPixmap(splashSize);

    // change to HiDPI if it makes sense
    pixmap.setDevicePixelRatio(devicePixelRatio);

    QPainter pixPaint(&pixmap);
    pixPaint.setRenderHint(QPainter::Antialiasing, true);

    const QPalette palette = QApplication::palette();
    const QString configured_theme = GUIUtil::getConfiguredAppearanceTheme();
    const bool defcon34_theme = GUIUtil::appearanceThemeBaseStyle(configured_theme) == QStringLiteral("34");
    const bool yam_theme = GUIUtil::appearanceThemeBaseStyle(configured_theme) == QStringLiteral("yam");
    const bool dark_theme = defcon34_theme || yam_theme || palette.color(QPalette::Window).lightness() < 128;
    const QColor dc34Teal(139, 203, 193);
    const QColor dc34Blue(45, 129, 180);
    const QColor dc34Yellow(216, 201, 52);
    const QColor dc34Red(214, 36, 86);
    const QColor titleColor = yam_theme ? QColor(200, 120, 255) : (defcon34_theme ? dc34Teal : (dark_theme ? QColor(242, 245, 248) : QColor(92, 96, 101)));
    const QColor bodyColor = yam_theme ? QColor(200, 120, 255) : (defcon34_theme ? QColor(222, 230, 235) : (dark_theme ? QColor(185, 197, 210) : QColor(102, 106, 111)));
    const QColor bgStart = yam_theme ? QColor(0, 0, 0) : (defcon34_theme ? QColor(5, 12, 36) : (dark_theme ? QColor(28, 34, 42) : QColor(255, 255, 255)));
    const QColor bgEnd = yam_theme ? QColor(51, 51, 51) : (defcon34_theme ? QColor(12, 24, 48) : (dark_theme ? QColor(8, 10, 13) : QColor(247, 247, 247)));

    // draw a slightly radial gradient
    QRadialGradient gradient(QPoint(0,0), splashSize.width()/devicePixelRatio);
    gradient.setColorAt(0, bgStart);
    gradient.setColorAt(1, bgEnd);
    QRect rGradient(QPoint(0,0), splashSize);
    pixPaint.fillRect(rGradient, gradient);
    pixPaint.setPen(defcon34_theme ? QColor(43, 90, 96) : (dark_theme ? QColor(58, 71, 88) : QColor(228, 232, 236)));
    pixPaint.drawRoundedRect(QRectF(0.5, 0.5, splashWidth - 1, splashHeight - 1), 10, 10);
    if (defcon34_theme) {
        pixPaint.save();
        pixPaint.setRenderHint(QPainter::Antialiasing, true);
        const QColor lineColors[] = {QColor(dc34Blue.red(), dc34Blue.green(), dc34Blue.blue(), 130),
                                     QColor(dc34Teal.red(), dc34Teal.green(), dc34Teal.blue(), 120),
                                     QColor(dc34Yellow.red(), dc34Yellow.green(), dc34Yellow.blue(), 95),
                                     QColor(dc34Red.red(), dc34Red.green(), dc34Red.blue(), 85)};
        for (int i = 0; i < 4; ++i) {
            pixPaint.setPen(QPen(lineColors[i], i < 2 ? 2.0 : 1.4, Qt::SolidLine, Qt::RoundCap));
            QPainterPath path;
            const qreal y = 42 + i * 42;
            path.moveTo(280, y);
            path.cubicTo(350, y - 26, 420, y + 28, 492, y);
            path.cubicTo(540, y - 18, 588, y + 18, 632, y + 2);
            pixPaint.drawPath(path);
        }
        pixPaint.restore();
    }

    // draw the bitcoin icon, expected size of PNG: 1024x1024
    QRect rectIcon(QPoint(-58,32), QSize(318,318));

    const QSize requiredSize(1024,1024);
    QPixmap icon = GUIUtil::themeAssetPixmap(QStringLiteral("splash_screen"), QStringLiteral(":/icons/defcoin_splash"), configured_theme);
    if (icon.isNull()) {
        icon = networkStyle->getAppIcon().pixmap(requiredSize);
    }

    pixPaint.drawPixmap(rectIcon, icon);

    // check font size and drawing width
    int titleFontSize = ENABLE_DEFCOIN_FUN_UI ? 36 : 30;
    pixPaint.setFont(QFont(font, titleFontSize));
    QFontMetrics fm = pixPaint.fontMetrics();
    int titleTextWidth = GUIUtil::TextWidth(fm, titleText);
    while (titleTextWidth > rightTextWidth && titleFontSize > 22) {
        --titleFontSize;
        pixPaint.setFont(QFont(font, titleFontSize));
        fm = pixPaint.fontMetrics();
        titleTextWidth = GUIUtil::TextWidth(fm, titleText);
    }

    QPixmap legacy_wordmark;
#if ENABLE_DEFCOIN_FUN_UI
    if (yam_theme) {
        legacy_wordmark = GUIUtil::themeAssetPixmap(QStringLiteral("legacy_wordmark"), QString(), configured_theme);
    }
#endif
    if (yam_theme && !legacy_wordmark.isNull()) {
        const QRect logoRect(QPoint(textLeft - 2, 38), QSize(240, 40));
        pixPaint.drawPixmap(logoRect, legacy_wordmark);
    } else {
        pixPaint.setPen(titleColor);
        pixPaint.drawText(textLeft, titleTop, titleText);
    }

    int versionTop = titleTop + 23;
    if (!subtitleText.isEmpty()) {
        int subtitleFontSize = 23;
        pixPaint.setFont(QFont(font, subtitleFontSize));
        fm = pixPaint.fontMetrics();
        int subtitleTextWidth = GUIUtil::TextWidth(fm, subtitleText);
        while (subtitleTextWidth > rightTextWidth && subtitleFontSize > 16) {
            --subtitleFontSize;
            pixPaint.setFont(QFont(font, subtitleFontSize));
            fm = pixPaint.fontMetrics();
            subtitleTextWidth = GUIUtil::TextWidth(fm, subtitleText);
        }
        pixPaint.setPen(defcon34_theme ? dc34Yellow : titleColor);
        pixPaint.drawText(textLeft + 1, titleTop + 40, subtitleText);
        versionTop = titleTop + 72;
    }

    int versionFontSize = 12;
    pixPaint.setFont(QFont(font, versionFontSize));

    // if the version string is too long, reduce size
    fm = pixPaint.fontMetrics();
    int versionTextWidth  = GUIUtil::TextWidth(fm, versionText);
    while(versionTextWidth > rightTextWidth && versionFontSize > 9) {
        --versionFontSize;
        pixPaint.setFont(QFont(font, versionFontSize));
        fm = pixPaint.fontMetrics();
        versionTextWidth = GUIUtil::TextWidth(fm, versionText);
    }
    pixPaint.setPen(bodyColor);
    pixPaint.drawText(textLeft + 1, versionTop, versionText);

    const int codenameTop = versionTop + pixPaint.fontMetrics().height() + 3;
    pixPaint.setFont(QFont(font, std::max(9, versionFontSize - 1)));
    pixPaint.drawText(textLeft + 1, codenameTop, codenameText);

    // draw copyright stuff
    {
        int copyrightFontSize = 11;
        const QStringList lines = copyrightText.split('\n');
        bool fits = false;
        while (!fits && copyrightFontSize > 8) {
            fits = true;
            pixPaint.setFont(QFont(font, copyrightFontSize));
            fm = pixPaint.fontMetrics();
            for (const QString& line : lines) {
                if (GUIUtil::TextWidth(fm, line) > rightTextWidth) {
                    fits = false;
                    --copyrightFontSize;
                    break;
                }
            }
        }

        pixPaint.setFont(QFont(font, copyrightFontSize));
        pixPaint.setPen(bodyColor);
        int y = codenameTop + 31;
        const int lineStep = pixPaint.fontMetrics().height() + 1;
        for (const QString& line : lines) {
            pixPaint.drawText(textLeft + 1, y, line);
            y += lineStep;
        }
    }

    // draw additional text if special network
    if(!titleAddText.isEmpty()) {
        QFont boldFont = QFont(font, 12);
        boldFont.setWeight(QFont::Bold);
        pixPaint.setFont(boldFont);
        fm = pixPaint.fontMetrics();
        int titleAddTextWidth  = GUIUtil::TextWidth(fm, titleAddText);
        pixPaint.drawText(splashWidth-titleAddTextWidth-14,18,titleAddText);
    }

    pixPaint.end();

    // Set window title
    setWindowTitle(windowTitleText + " " + titleAddText);

    // Resize window and move to center of desktop, disallow resizing
    QRect r(QPoint(), QSize(pixmap.size().width()/devicePixelRatio,pixmap.size().height()/devicePixelRatio));
    resize(r.size());
    setFixedSize(r.size());
    move(QGuiApplication::primaryScreen()->geometry().center() - r.center());

    installEventFilter(this);

    GUIUtil::handleCloseWindowShortcut(this);
}

SplashScreen::~SplashScreen()
{
    if (m_node) unsubscribeFromCoreSignals();
}

void SplashScreen::setNode(interfaces::Node& node)
{
    assert(!m_node);
    m_node = &node;
    subscribeToCoreSignals();
    if (m_shutdown) m_node->startShutdown();
}

void SplashScreen::shutdown()
{
    m_shutdown = true;
    if (m_node) m_node->startShutdown();
}

bool SplashScreen::eventFilter(QObject * obj, QEvent * ev) {
    if (ev->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(ev);
        if (keyEvent->key() == Qt::Key_Q) {
            shutdown();
        }
    }
    return QObject::eventFilter(obj, ev);
}

void SplashScreen::finish()
{
    /* If the window is minimized, hide() will be ignored. */
    /* Make sure we de-minimize the splashscreen window before hiding */
    if (isMinimized())
        showNormal();
    hide();
    deleteLater(); // No more need for this
}

static void InitMessage(SplashScreen *splash, const std::string &message)
{
    bool invoked = QMetaObject::invokeMethod(splash, "showMessage",
        Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(message)),
        Q_ARG(int, Qt::AlignBottom|Qt::AlignHCenter),
        Q_ARG(QColor, QColor(55,55,55)));
    assert(invoked);
}

static void ShowProgress(SplashScreen *splash, const std::string &title, int nProgress, bool resume_possible)
{
    InitMessage(splash, title + std::string("\n") +
            (resume_possible ? _("(press q to shutdown and continue later)").translated
                                : _("press q to shutdown").translated) +
            strprintf("\n%d", nProgress) + "%");
}

void SplashScreen::subscribeToCoreSignals()
{
    // Connect signals to client
    m_handler_init_message = m_node->handleInitMessage(std::bind(InitMessage, this, std::placeholders::_1));
    m_handler_show_progress = m_node->handleShowProgress(std::bind(ShowProgress, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void SplashScreen::handleLoadWallet()
{
#ifdef ENABLE_WALLET
    if (!WalletModel::isWalletEnabled()) return;
    m_handler_load_wallet = m_node->walletClient().handleLoadWallet([this](std::unique_ptr<interfaces::Wallet> wallet) {
        m_connected_wallet_handlers.emplace_back(wallet->handleShowProgress(std::bind(ShowProgress, this, std::placeholders::_1, std::placeholders::_2, false)));
        m_connected_wallets.emplace_back(std::move(wallet));
    });
#endif
}

void SplashScreen::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    m_handler_init_message->disconnect();
    m_handler_show_progress->disconnect();
    for (const auto& handler : m_connected_wallet_handlers) {
        handler->disconnect();
    }
    m_connected_wallet_handlers.clear();
    m_connected_wallets.clear();
}

void SplashScreen::showMessage(const QString &message, int alignment, const QColor &color)
{
    const QStringList display_lines = message.split(QLatin1Char('\n'));
    curMessage = display_lines.isEmpty() ? message : display_lines.first().trimmed();
    curAlignment = alignment;
    curColor = color;
    if (!QCoreApplication::instance()->property("defcoinDebugLogSessionStartSize").isValid()) {
        QFile debug_log(GUIUtil::boostPathToQString(GetDataDir() / "debug.log"));
        QCoreApplication::instance()->setProperty("defcoinDebugLogSessionStartSize", debug_log.exists() ? debug_log.size() : 0);
    }
    LogPrintf("GUI startup: %s\n", message.toStdString());
    m_progress_percent = -1;
    const QStringList lines = message.split(QLatin1Char('\n'));
    const bool has_shutdown_hint = message.contains(QStringLiteral("press q"), Qt::CaseInsensitive);
    if (!has_shutdown_hint && !lines.isEmpty()) {
        QString last_line = lines.last().trimmed();
        if (last_line.endsWith(QLatin1Char('%'))) {
            last_line.chop(1);
            bool ok = false;
            const int progress = last_line.toInt(&ok);
            if (ok) m_progress_percent = std::max(0, std::min(100, progress));
        }
    }
    update();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 15);
}

void SplashScreen::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, pixmap);
    QRect r = rect().adjusted(70, 5, -70, -18);
    painter.setPen(curColor);
    painter.drawText(r, Qt::AlignHCenter | Qt::AlignBottom, curMessage);
    const qint64 elapsed_secs = m_elapsed_timer.isValid() ? m_elapsed_timer.elapsed() / 1000 : 0;
    const QString elapsed = QStringLiteral("%1:%2")
        .arg(elapsed_secs / 60)
        .arg(elapsed_secs % 60, 2, 10, QLatin1Char('0'));
    painter.setPen(curColor);
    painter.drawText(rect().adjusted(0, 5, -12, -18), Qt::AlignRight | Qt::AlignBottom, elapsed);

    if (m_progress_percent > 0 && m_progress_percent < 95) {
        const QRectF bar_rect(rect().center().x() - 170, rect().bottom() - 42, 340, 5);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(90, 100, 112, 130));
        painter.drawRoundedRect(bar_rect, 2.5, 2.5);
        QRectF fill_rect = bar_rect;
        fill_rect.setWidth(bar_rect.width() * m_progress_percent / 100.0);
        painter.setBrush(QColor(102, 179, 255, 210));
        painter.drawRoundedRect(fill_rect, 2.5, 2.5);
    }
}

void SplashScreen::closeEvent(QCloseEvent *event)
{
    shutdown(); // allows an "emergency" shutdown during startup
    event->ignore();
}
