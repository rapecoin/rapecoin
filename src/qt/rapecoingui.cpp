/*
 * Qt4 rapecoin GUI.
 *
 * W.J. van der Laan 2011-2012
 * The Bitcoin developers 2011-2012
 */

#include <QApplication>

#include "rapecoingui.h"

#include "transactiontablemodel.h"
#include "optionsdialog.h"
#include "aboutdialog.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "walletframe.h"
#include "optionsmodel.h"
#include "transactiondescdialog.h"
#include "rapecoinunits.h"
#include "guiconstants.h"
#include "notificator.h"
#include "guiutil.h"
#include "rpcconsole.h"
#include "ui_interface.h"
#include "wallet.h"
#include "init.h"

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include <QMenuBar>
#include <QMenu>
#include <QIcon>
#include <QVBoxLayout>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QStackedWidget>
#include <QDateTime>
#include <QMovie>
#include <QDragEnterEvent>
#if QT_VERSION < 0x050000
#include <QUrl>
#endif
#include <QMimeData>
#include <QStyle>
#include <QSettings>
#include <QDesktopWidget>
#include <QListWidget>

#include <iostream>

const QString RapecoinGUI::DEFAULT_WALLET = "~Default";

RapecoinGUI::RapecoinGUI(QWidget *parent) :
    QMainWindow(parent),
    clientModel(0),
    encryptWalletAction(0),
    changePassphraseAction(0),
    aboutQtAction(0),
    trayIcon(0),
    notificator(0),
    rpcConsole(0),
    prevBlocks(0)
{
    restoreWindowGeometry();
    setWindowTitle(tr("Rapecoin") + " - " + tr("Wallet"));
#ifndef Q_OS_MAC
    QApplication::setWindowIcon(QIcon(":icons/rapecoin"));
    setWindowIcon(QIcon(":icons/rapecoin"));
#else
    setUnifiedTitleAndToolBarOnMac(true);
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

    // Create wallet frame and make it the central widget
    walletFrame = new WalletFrame(this);
    setCentralWidget(walletFrame);

    // Accept D&D of URIs
    setAcceptDrops(true);

    // Create actions for the toolbar, menu bar and tray/dock icon
    // Needs walletFrame to be initialized
    createActions();

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create system tray icon and notification
    createTrayIcon();

    // Create status bar
    statusBar();

    // Status bar notification icons
    QFrame *frameBlocks = new QFrame();
    frameBlocks->setContentsMargins(0,0,0,0);
    frameBlocks->setMinimumWidth(56);
    frameBlocks->setMaximumWidth(56);
    QHBoxLayout *frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(3,0,3,0);
    frameBlocksLayout->setSpacing(3);
    labelEncryptionIcon = new QLabel();
    labelConnectionsIcon = new QLabel();
    labelBlocksIcon = new QLabel();
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelEncryptionIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelConnectionsIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addStretch();

    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(false);
    progressBar = new QProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setVisible(false);

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://qt-project.org/doc/qt-4.8/gallery.html
    QString curStyle = QApplication::style()->metaObject()->className();
    if(curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle")
    {
        progressBar->setStyleSheet("QProgressBar { background-color: #e9e9e9; color:#222222; border: 1px solid #535353; border-radius: 0px; padding: 1px; text-align: center; } QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #0a015e, stop: 1 137bca); border-radius: 0px; margin: 0px; }");
    }

    statusBar()->addWidget(progressBarLabel);
    statusBar()->addWidget(progressBar);
    statusBar()->addPermanentWidget(frameBlocks);

    syncIconMovie = new QMovie(":/movies/update_spinner", "mng", this);

    rpcConsole = new RPCConsole(this);
    connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(show()));

    // Install event filter to be able to catch status tip events (QEvent::StatusTip)
    this->installEventFilter(this);

    // Initially wallet actions should be disabled
    setWalletActionsEnabled(false);

   qApp->setStyleSheet("* {background-color: #282828; color:#ffffff;} QMenu{background-color:#535353} QMenuBar {background-color: #535353; color:#ffffff; border-bottom: 1px solid #111111;} QMenuBar::item {padding: 4px 8px; background-color: #535353; color:#ffffff; border-right: 1px solid #111111;} QHeaderView {background-color: #535353; color:#ffffff;} QHeaderView::section {background-color: #535353; color:#ffffff;} QToolbar::handle{background:transparent; color: #282828; border: 1px solid #282828; height:1px; width:1px} QToolbar {background-color: #535353; color:#ffffff; border:1px solid #535353;} QToolButton {background-color: #282828; margin-bottom:5px; padding-top:4px; color:#ffffff; width: 70px; height: 70px; border:1px solid #535353;} QPushButton {background-color: #0f6daa; color:#ffffff;} QToolTip {color: #ffffff; background-color: #565656; border: none} QLineEdit{height:25px; border:1px solid #888888; background-color: #565656; color: #ffffff;} QComboBox{height:25px; border:1px solid #888888; color: #ffffff; background-color: #565656;} QTabWidget::pane {border: 1px solid #565656;}QTabWidget::tab-bar{background:#282828; border:none; height:32px;} QTabWidget::tab{background:#282828; color:#ffffff; border-radius:0px; border:none; 32px;}QTabWidget::tab:selected{background:#565656; color:#ffffff; border-radius:0px; border:none; 32px;} QRadioButton::indicator:checked, QRadioButton::indicator:unchecked{ color: #b1b1b1; background-color: #323232; border: 1px solid #b1b1b1; border-radius: 6px; } QRadioButton::indicator:checked { background-color: qradialgradient( cx: 0.5, cy: 0.5, fx: 0.5, fy: 0.5, radius: 1.0, stop: 0.25 #ffaa00, stop: 0.3 #323232 ); } QCheckBox::indicator{ color: #b1b1b1; background-color: #323232; border: 1px solid #b1b1b1; width: 9px; height: 9px; } QRadioButton::indicator { border-radius: 6px; } QRadioButton::indicator:hover, QCheckBox::indicator:hover { border: 1px solid #ffaa00; } QCheckBox::indicator:checked { image:url(:/icons/checkbox.png); } QCheckBox::indicator:disabled, QRadioButton::indicator:disabled { border: 1px solid #444; } QLabel { border: none; } QScrollBar { border: 1px solid #5A5A5A; background: #191919; } QScrollBar:horizontal { height: 15px; margin: 0px 0px 0px 32px; } QScrollBar:vertical { width: 15px; margin: 32px 0px 0px 0px; } QScrollBar::handle { background: #353535; border: 1px solid #5A5A5A; } QScrollBar::handle:horizontal { border-width: 0px 1px 0px 1px; } QScrollBar::handle:vertical { border-width: 1px 0px 1px 0px; } QScrollBar::handle:horizontal { min-width: 20px; } QScrollBar::handle:vertical { min-height: 20px; } QScrollBar::add-line, QScrollBar::sub-line { background:#353535; border: 1px solid #5A5A5A; subcontrol-origin: margin; } QScrollBar::add-line { position: absolute; } QScrollBar::add-line:horizontal { width: 15px; subcontrol-position: left; left: 15px; } QScrollBar::add-line:vertical { height: 15px; subcontrol-position: top; top: 15px; } QScrollBar::sub-line:horizontal { width: 15px; subcontrol-position: top left; } QScrollBar::sub-line:vertical { height: 15px; subcontrol-position: top; } QScrollBar:left-arrow, QScrollBar::right-arrow, QScrollBar::up-arrow, QScrollBar::down-arrow { border: 1px solid #5A5A5A; width: 3px; height: 3px; } QScrollBar::add-page, QScrollBar::sub-page { background: none; } QTabBar { margin-left: 2px; } QTabBar::tab { background: #282828; border-radius: 0px; padding: 4px; margin: 4px; color:#ffffff} QTabBar::tab:selected, QTabBar::tab:hover { background: #535353; color:#ffffff } ");
    //qApp->setStyleSheet("* { background: #191919; color: #DDDDDD; border: 1px solid #5A5A5A; } QWidget::item:selected { background: #3D7848; } QCheckBox, QRadioButton { border: none; } QRadioButton::indicator, QCheckBox::indicator { width: 13px; height: 13px; } QRadioButton::indicator::unchecked, QCheckBox::indicator::unchecked { border: 1px solid #5A5A5A; background: none; } QRadioButton::indicator:unchecked:hover, QCheckBox::indicator:unchecked:hover { border: 1px solid #DDDDDD; } QRadioButton::indicator::checked, QCheckBox::indicator::checked { border: 1px solid #5A5A5A; background: #5A5A5A; } QRadioButton::indicator:checked:hover, QCheckBox::indicator:checked:hover { border: 1px solid #DDDDDD; background: #DDDDDD; } QGroupBox { margin-top: 6px; } QGroupBox::title { top: -7px; left: 7px; } QScrollBar { border: 1px solid #5A5A5A; background: #191919; } QScrollBar:horizontal { height: 15px; margin: 0px 0px 0px 32px; } QScrollBar:vertical { width: 15px; margin: 32px 0px 0px 0px; } QScrollBar::handle { background: #353535; border: 1px solid #5A5A5A; } QScrollBar::handle:horizontal { border-width: 0px 1px 0px 1px; } QScrollBar::handle:vertical { border-width: 1px 0px 1px 0px; } QScrollBar::handle:horizontal { min-width: 20px; } QScrollBar::handle:vertical { min-height: 20px; } QScrollBar::add-line, QScrollBar::sub-line { background:#353535; border: 1px solid #5A5A5A; subcontrol-origin: margin; } QScrollBar::add-line { position: absolute; } QScrollBar::add-line:horizontal { width: 15px; subcontrol-position: left; left: 15px; } QScrollBar::add-line:vertical { height: 15px; subcontrol-position: top; top: 15px; } QScrollBar::sub-line:horizontal { width: 15px; subcontrol-position: top left; } QScrollBar::sub-line:vertical { height: 15px; subcontrol-position: top; } QScrollBar:left-arrow, QScrollBar::right-arrow, QScrollBar::up-arrow, QScrollBar::down-arrow { border: 1px solid #5A5A5A; width: 3px; height: 3px; } QScrollBar::add-page, QScrollBar::sub-page { background: none; } QAbstractButton:hover { background: #353535; } QAbstractButton:pressed { background: #5A5A5A; } QAbstractItemView { show-decoration-selected: 1; selection-background-color: #3D7848; selection-color: #DDDDDD; alternate-background-color: #353535; } QHeaderView { border: 1px solid #5A5A5A; } QHeaderView::section { background: #191919; border: 1px solid #5A5A5A; padding: 4px; } QHeaderView::section:selected, QHeaderView::section::checked { background: #353535; } QTableView { gridline-color: #5A5A5A; } QTabBar { margin-left: 2px; } QTabBar::tab { border-radius: 0px; padding: 4px; margin: 4px; } QTabBar::tab:selected { background: #353535; } QComboBox::down-arrow { border: 1px solid #5A5A5A; background: #353535; } QComboBox::drop-down { border: 1px solid #5A5A5A; background: #353535; } QComboBox::down-arrow { width: 3px; height: 3px; border: 1px solid #5A5A5A; } QAbstractSpinBox { padding-right: 15px; } QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { border: 1px solid #5A5A5A; background: #353535; subcontrol-origin: border; } QAbstractSpinBox::up-arrow, QAbstractSpinBox::down-arrow { width: 3px; height: 3px; border: 1px solid #5A5A5A; } QSlider { border: none; } QSlider::groove:horizontal { height: 5px; margin: 4px 0px 4px 0px; } QSlider::groove:vertical { width: 5px; margin: 0px 4px 0px 4px; } QSlider::handle { border: 1px solid #5A5A5A; background: #353535; } QSlider::handle:horizontal { width: 15px; margin: -4px 0px -4px 0px; } QSlider::handle:vertical { height: 15px; margin: 0px -4px 0px -4px; } QSlider::add-page:vertical, QSlider::sub-page:horizontal { background: #3D7848; } QSlider::sub-page:vertical, QSlider::add-page:horizontal { background: #353535; } QLabel { border: none; } QProgressBar { text-align: center; } QProgressBar::chunk { width: 1px; background-color: #3D7848; } QMenu::separator { background: #353535; }");
}

RapecoinGUI::~RapecoinGUI()
{
    // Unsubscribe from notifications from core
    saveWindowGeometry();
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete appMenuBar;
    MacDockIconHandler::instance()->setMainWindow(NULL);
#endif
}

void RapecoinGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QAction(QIcon(":/icons/overview"), tr("&Overview"), this);
    overviewAction->setStatusTip(tr("Show general overview of wallet"));
    overviewAction->setToolTip(overviewAction->statusTip());
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction(QIcon(":/icons/send"), tr("&Send"), this);
    sendCoinsAction->setStatusTip(tr("Send coins to a Rapecoin address"));
    sendCoinsAction->setToolTip(sendCoinsAction->statusTip());
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendCoinsAction);

    receiveCoinsAction = new QAction(QIcon(":/icons/receiving_addresses"), tr("&Receive"), this);
    receiveCoinsAction->setStatusTip(tr("Show the list of addresses for receiving payments"));
    receiveCoinsAction->setToolTip(receiveCoinsAction->statusTip());
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(receiveCoinsAction);

    historyAction = new QAction(QIcon(":/icons/history"), tr("&Transactions"), this);
    historyAction->setStatusTip(tr("Browse transaction history"));
    historyAction->setToolTip(historyAction->statusTip());
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);

    addressBookAction = new QAction(QIcon(":/icons/address-book"), tr("&Addresses"), this);
    addressBookAction->setStatusTip(tr("Edit the list of stored addresses and labels"));
    addressBookAction->setToolTip(addressBookAction->statusTip());
    addressBookAction->setCheckable(true);
    addressBookAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    tabGroup->addAction(addressBookAction);

    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(gotoAddressBookPage()));

    quitAction = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setStatusTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction = new QAction(QIcon(":/icons/rapecoin"), tr("&About Rapecoin"), this);
    aboutAction->setStatusTip(tr("Show information about Rapecoin"));
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutQtAction = new QAction(QIcon(":/trolltech/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
    aboutQtAction->setStatusTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(QIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setStatusTip(tr("Modify configuration options for Rapecoin"));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    toggleHideAction = new QAction(QIcon(":/icons/rapecoin"), tr("&Show / Hide"), this);
    toggleHideAction->setStatusTip(tr("Show or hide the main Window"));

    encryptWalletAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setStatusTip(tr("Encrypt the private keys that belong to your wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(QIcon(":/icons/filesave"), tr("&Backup Wallet..."), this);
    backupWalletAction->setStatusTip(tr("Backup wallet to another location"));
    changePassphraseAction = new QAction(QIcon(":/icons/key"), tr("&Change Passphrase..."), this);
    changePassphraseAction->setStatusTip(tr("Change the passphrase used for wallet encryption"));
    signMessageAction = new QAction(QIcon(":/icons/edit"), tr("Sign &message..."), this);
    signMessageAction->setStatusTip(tr("Sign messages with your Rapecoin addresses to prove you own them"));
    verifyMessageAction = new QAction(QIcon(":/icons/transaction_0"), tr("&Verify message..."), this);
    verifyMessageAction->setStatusTip(tr("Verify messages to ensure they were signed with specified Rapecoin addresses"));

    openRPCConsoleAction = new QAction(QIcon(":/icons/debugwindow"), tr("&Debug window"), this);
    openRPCConsoleAction->setStatusTip(tr("Open debugging and diagnostic console"));

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(encryptWalletAction, SIGNAL(triggered(bool)), walletFrame, SLOT(encryptWallet(bool)));
    connect(backupWalletAction, SIGNAL(triggered()), walletFrame, SLOT(backupWallet()));
    connect(changePassphraseAction, SIGNAL(triggered()), walletFrame, SLOT(changePassphrase()));
    connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
    connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
}

void RapecoinGUI::createMenuBar()
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
    file->addAction(backupWalletAction);
    file->addAction(signMessageAction);
    file->addAction(verifyMessageAction);
    file->addSeparator();
    file->addAction(quitAction);

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    settings->addAction(encryptWalletAction);
    settings->addAction(changePassphraseAction);
    settings->addSeparator();
    settings->addAction(optionsAction);

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    help->addAction(openRPCConsoleAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);

}

void RapecoinGUI::createToolBars()
{
    QToolBar *toolbar = new QToolBar(tr("Tabs toolbar"), this);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolbar->addAction(overviewAction);
    toolbar->addAction(sendCoinsAction);
    toolbar->addAction(receiveCoinsAction);
    toolbar->addAction(historyAction);
    toolbar->addAction(addressBookAction);
    toolbar->setMovable(false);
    toolbar->setAllowedAreas(Qt::LeftToolBarArea);
    QMainWindow::addToolBar(Qt::LeftToolBarArea, toolbar);
 }

void RapecoinGUI::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;
    if(clientModel)
    {
        // Replace some strings and icons, when using the testnet
        if(clientModel->isTestNet())
        {
            setWindowTitle(windowTitle() + QString(" ") + tr("[testnet]"));
#ifndef Q_OS_MAC
            QApplication::setWindowIcon(QIcon(":icons/rapecoin_testnet"));
            setWindowIcon(QIcon(":icons/rapecoin_testnet"));
#else
            MacDockIconHandler::instance()->setIcon(QIcon(":icons/rapecoin_testnet"));
#endif
            if(trayIcon)
            {
                // Just attach " [testnet]" to the existing tooltip
                trayIcon->setToolTip(trayIcon->toolTip() + QString(" ") + tr("[testnet]"));
                trayIcon->setIcon(QIcon(":/icons/toolbar_testnet"));
            }

            toggleHideAction->setIcon(QIcon(":/icons/toolbar_testnet"));
            aboutAction->setIcon(QIcon(":/icons/toolbar_testnet"));
        }

        // Create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        createTrayIconMenu();

        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

        setNumBlocks(clientModel->getNumBlocks(), clientModel->getNumBlocksOfPeers());
        connect(clientModel, SIGNAL(numBlocksChanged(int,int)), this, SLOT(setNumBlocks(int,int)));

        // Receive and report messages from network/worker thread
        connect(clientModel, SIGNAL(message(QString,QString,unsigned int)), this, SLOT(message(QString,QString,unsigned int)));

        rpcConsole->setClientModel(clientModel);
        walletFrame->setClientModel(clientModel);
    }
}

bool RapecoinGUI::addWallet(const QString& name, WalletModel *walletModel)
{
    setWalletActionsEnabled(true);
    return walletFrame->addWallet(name, walletModel);
}

bool RapecoinGUI::setCurrentWallet(const QString& name)
{
    return walletFrame->setCurrentWallet(name);
}

void RapecoinGUI::removeAllWallets()
{
    setWalletActionsEnabled(false);
    walletFrame->removeAllWallets();
}

void RapecoinGUI::setWalletActionsEnabled(bool enabled)
{
    overviewAction->setEnabled(enabled);
    sendCoinsAction->setEnabled(enabled);
    receiveCoinsAction->setEnabled(enabled);
    historyAction->setEnabled(enabled);
    encryptWalletAction->setEnabled(enabled);
    backupWalletAction->setEnabled(enabled);
    changePassphraseAction->setEnabled(enabled);
    signMessageAction->setEnabled(enabled);
    verifyMessageAction->setEnabled(enabled);
    addressBookAction->setEnabled(enabled);
}

void RapecoinGUI::createTrayIcon()
{
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);

    trayIcon->setToolTip(tr("Rapecoin client"));
    trayIcon->setIcon(QIcon(":/icons/toolbar"));
    trayIcon->show();
#endif

    notificator = new Notificator(QApplication::applicationName(), trayIcon);
}

void RapecoinGUI::createTrayIconMenu()
{
    QMenu *trayIconMenu;
#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-Mac OSes)
    if (!trayIcon)
        return;

    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow((QMainWindow *)this);
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(sendCoinsAction);
    trayIconMenu->addAction(receiveCoinsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(signMessageAction);
    trayIconMenu->addAction(verifyMessageAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

#ifndef Q_OS_MAC
void RapecoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers show/hide of the main window
        toggleHideAction->trigger();
    }
}
#endif

void RapecoinGUI::saveWindowGeometry()
{
    QSettings settings;
    settings.setValue("nWindowPos", pos());
    settings.setValue("nWindowSize", size());
}

void RapecoinGUI::restoreWindowGeometry()
{
    QSettings settings;
    QPoint pos = settings.value("nWindowPos").toPoint();
    QSize size = settings.value("nWindowSize", QSize(850, 550)).toSize();
    if (!pos.x() && !pos.y())
    {
        QRect screen = QApplication::desktop()->screenGeometry();
        pos.setX((screen.width()-size.width())/2);
        pos.setY((screen.height()-size.height())/2);
    }
    resize(size);
    move(pos);
}

void RapecoinGUI::optionsClicked()
{
    if(!clientModel || !clientModel->getOptionsModel())
        return;
    OptionsDialog dlg;
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

void RapecoinGUI::aboutClicked()
{
    AboutDialog dlg;
    dlg.setModel(clientModel);
    dlg.exec();
}

void RapecoinGUI::gotoOverviewPage()
{
    if (walletFrame) walletFrame->gotoOverviewPage();
}

void RapecoinGUI::gotoHistoryPage()
{
    if (walletFrame) walletFrame->gotoHistoryPage();
}

void RapecoinGUI::gotoAddressBookPage()
{
    if (walletFrame) walletFrame->gotoAddressBookPage();
}

void RapecoinGUI::gotoReceiveCoinsPage()
{
    if (walletFrame) walletFrame->gotoReceiveCoinsPage();
}

void RapecoinGUI::gotoSendCoinsPage(QString addr)
{
    if (walletFrame) walletFrame->gotoSendCoinsPage(addr);
}

void RapecoinGUI::gotoSignMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoSignMessageTab(addr);
}

void RapecoinGUI::gotoVerifyMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoVerifyMessageTab(addr);
}

void RapecoinGUI::setNumConnections(int count)
{
    QString icon;
    switch(count)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }
    labelConnectionsIcon->setPixmap(QIcon(icon).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelConnectionsIcon->setToolTip(tr("%n active connection(s) to Rapecoin network", "", count));
}

void RapecoinGUI::setNumBlocks(int count, int nTotalBlocks)
{
    // Prevent orphan statusbar messages (e.g. hover Quit in main menu, wait until chain-sync starts -> garbelled text)
    statusBar()->clearMessage();

    // Acquire current block source
    enum BlockSource blockSource = clientModel->getBlockSource();
    switch (blockSource) {
        case BLOCK_SOURCE_NETWORK:
            progressBarLabel->setText(tr("Synchronizing with network..."));
            break;
        case BLOCK_SOURCE_DISK:
            progressBarLabel->setText(tr("Importing blocks from disk..."));
            break;
        case BLOCK_SOURCE_REINDEX:
            progressBarLabel->setText(tr("Reindexing blocks on disk..."));
            break;
        case BLOCK_SOURCE_NONE:
            // Case: not Importing, not Reindexing and no network connection
            progressBarLabel->setText(tr("No block source available..."));
            break;
    }

    QString tooltip;

    QDateTime lastBlockDate = clientModel->getLastBlockDate();
    QDateTime currentDate = QDateTime::currentDateTime();
    int secs = lastBlockDate.secsTo(currentDate);

    if(count < nTotalBlocks)
    {
        tooltip = tr("Processed %1 of %2 (estimated) blocks of transaction history.").arg(count).arg(nTotalBlocks);
    }
    else
    {
        tooltip = tr("Processed %1 blocks of transaction history.").arg(count);
    }

    // Set icon state: spinning if catching up, tick otherwise
    if(secs < 90*60 && count >= nTotalBlocks)
    {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        labelBlocksIcon->setPixmap(QIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

        walletFrame->showOutOfSyncWarning(false);

        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);
    }
    else
    {
        // Represent time from last generated block in human readable text
        QString timeBehindText;
        if(secs < 48*60*60)
        {
            timeBehindText = tr("%n hour(s)","",secs/(60*60));
        }
        else if(secs < 14*24*60*60)
        {
            timeBehindText = tr("%n day(s)","",secs/(24*60*60));
        }
        else
        {
            timeBehindText = tr("%n week(s)","",secs/(7*24*60*60));
        }

        progressBarLabel->setVisible(true);
        progressBar->setFormat(tr("%1 behind").arg(timeBehindText));
        progressBar->setMaximum(1000000000);
        progressBar->setValue(clientModel->getVerificationProgress() * 1000000000.0 + 0.5);
        progressBar->setVisible(true);

        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
        labelBlocksIcon->setMovie(syncIconMovie);
        if(count != prevBlocks)
            syncIconMovie->jumpToNextFrame();
        prevBlocks = count;

        walletFrame->showOutOfSyncWarning(true);

        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1 ago.").arg(timeBehindText);
        tooltip += QString("<br>");
        tooltip += tr("Transactions after this will not yet be visible.");
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

void RapecoinGUI::message(const QString &title, const QString &message, unsigned int style, bool *ret)
{
    QString strTitle = tr("Rapecoin"); // default title
    // Default to information icon
    int nMBoxIcon = QMessageBox::Information;
    int nNotifyIcon = Notificator::Information;

    // Override title based on style
    QString msgType;
    switch (style) {
    case CClientUIInterface::MSG_ERROR:
        msgType = tr("Error");
        break;
    case CClientUIInterface::MSG_WARNING:
        msgType = tr("Warning");
        break;
    case CClientUIInterface::MSG_INFORMATION:
        msgType = tr("Information");
        break;
    default:
        msgType = title; // Use supplied title
    }
    if (!msgType.isEmpty())
        strTitle += " - " + msgType;

    // Check for error/warning icon
    if (style & CClientUIInterface::ICON_ERROR) {
        nMBoxIcon = QMessageBox::Critical;
        nNotifyIcon = Notificator::Critical;
    }
    else if (style & CClientUIInterface::ICON_WARNING) {
        nMBoxIcon = QMessageBox::Warning;
        nNotifyIcon = Notificator::Warning;
    }

    // Display message
    if (style & CClientUIInterface::MODAL) {
        // Check for buttons, use OK as default, if none was supplied
        QMessageBox::StandardButton buttons;
        if (!(buttons = (QMessageBox::StandardButton)(style & CClientUIInterface::BTN_MASK)))
            buttons = QMessageBox::Ok;

        QMessageBox mBox((QMessageBox::Icon)nMBoxIcon, strTitle, message, buttons);
        int r = mBox.exec();
        if (ret != NULL)
            *ret = r == QMessageBox::Ok;
    }
    else
        notificator->notify((Notificator::Class)nNotifyIcon, strTitle, message);
}

void RapecoinGUI::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void RapecoinGUI::closeEvent(QCloseEvent *event)
{
    if(clientModel)
    {
#ifndef Q_OS_MAC // Ignored on Mac
        if(!clientModel->getOptionsModel()->getMinimizeToTray() &&
           !clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            QApplication::quit();
        }
#endif
    }
    QMainWindow::closeEvent(event);

}

void RapecoinGUI::askFee(qint64 nFeeRequired, bool *payFee)
{
    QString strMessage = tr("This transaction is over the size limit. You can still send it for a fee of %1, "
        "which goes to the nodes that process your transaction and helps to support the network. "
        "Do you want to pay the fee?").arg(RapecoinUnits::formatWithUnit(RapecoinUnits::Rap, nFeeRequired));
    QMessageBox::StandardButton retval = QMessageBox::question(
          this, tr("Confirm transaction fee"), strMessage,
          QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Yes);
    *payFee = (retval == QMessageBox::Yes);
}

void RapecoinGUI::incomingTransaction(const QString& date, int unit, qint64 amount, const QString& type, const QString& address)
{
    // On new transaction, make an info balloon
    message((amount)<0 ? tr("Sent transaction") : tr("Incoming transaction"),
             tr("Date: %1\n"
                "Amount: %2\n"
                "Type: %3\n"
                "Address: %4\n")
                  .arg(date)
                  .arg(RapecoinUnits::formatWithUnit(unit, amount, true))
                  .arg(type)
                  .arg(address), CClientUIInterface::MSG_INFORMATION);
}

void RapecoinGUI::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void RapecoinGUI::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        int nValidUrisFound = 0;
        QList<QUrl> uris = event->mimeData()->urls();
        foreach(const QUrl &uri, uris)
        {
            if (walletFrame->handleURI(uri.toString()))
                nValidUrisFound++;
        }

        // if valid URIs were found
        if (nValidUrisFound)
            walletFrame->gotoSendCoinsPage();
        else
            message(tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid Rapecoin address or malformed URI parameters."),
                      CClientUIInterface::ICON_WARNING);
    }

    event->acceptProposedAction();
}

bool RapecoinGUI::eventFilter(QObject *object, QEvent *event)
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

void RapecoinGUI::handleURI(QString strURI)
{
    // URI has to be valid
    if (!walletFrame->handleURI(strURI))
        message(tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid Rapecoin address or malformed URI parameters."),
                  CClientUIInterface::ICON_WARNING);
}

void RapecoinGUI::setEncryptionStatus(int status)
{
    switch(status)
    {
    case WalletModel::Unencrypted:
        labelEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_open").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    case WalletModel::Locked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_closed").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    }
}

void RapecoinGUI::showNormalIfMinimized(bool fToggleHidden)
{
    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden())
    {
        show();
        activateWindow();
    }
    else if (isMinimized())
    {
        showNormal();
        activateWindow();
    }
    else if (GUIUtil::isObscured(this))
    {
        raise();
        activateWindow();
    }
    else if(fToggleHidden)
        hide();
}

void RapecoinGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void RapecoinGUI::detectShutdown()
{
    /*if (ShutdownRequested()){
        QMetaObject::invokeMethod(QCoreApplication::instance(), "quit", Qt::QueuedConnection);
    }*/
    if (ShutdownRequested())
    {
        if(rpcConsole)
            rpcConsole->hide();
        qApp->quit();
    }
}
