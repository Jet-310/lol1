#include "MainWindow.h"
#include "SettingsManager.h"
#include "FileUtils.h"
#include "Logger.h"

#include <QToolBar>
#include <QAction>
#include <QDockWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QTimer>
#include <QDateTime>
#include <QFileInfo>
#include <QScrollBar>
#include <QStyleFactory>
#include <QApplication>

namespace {
// Column indices for the audio track table.
enum TrackColumn {
    ColTrackNumber = 0,
    ColCodec,
    ColBitrate,
    ColChannels,
    ColSampleRate,
    ColLanguage,
    ColDuration,
    ColDefault,
    ColCount
};
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_settings(std::make_unique<SettingsManager>())
    , m_ffmpeg(std::make_unique<FFmpegWrapper>(this))
{
    setAcceptDrops(true);
    setWindowTitle(tr("FFmpeg Audio Track Editor"));
    resize(1100, 750);

    buildUi();
    connectSignals();
    restoreSettings();

    Logger::instance().info(tr("Application started."));
}

MainWindow::~MainWindow()
{
    saveSettingsNow();
}

// ---------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------

void MainWindow::buildUi()
{
    buildToolbar();
    buildCentralWidget();
    buildConsoleDock();
    buildRecentFilesDock();
    buildStatusBar();
    applyDarkTheme();

    // Menu bar with a few convenience actions / keyboard shortcuts.
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *openAction = fileMenu->addAction(tr("&Open Video..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onBrowseVideo);

    QAction *quitAction = fileMenu->addAction(tr("E&xit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_consoleDock->toggleViewAction());
    viewMenu->addAction(m_recentDock->toggleViewAction());
    QAction *toggleThemeAction = viewMenu->addAction(tr("Toggle &Dark / Light Theme"));
    toggleThemeAction->setShortcut(QStringLiteral("Ctrl+T"));
    connect(toggleThemeAction, &QAction::triggered, this, &MainWindow::onToggleTheme);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *aboutAction = helpMenu->addAction(tr("&About"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, tr("About"),
                            tr("FFmpeg Audio Track Editor\n\n"
                               "A graphical front end for FFmpeg / FFprobe that lets you "
                               "inspect and edit the audio tracks of a video file.\n\n"
                               "Built with Qt 6 and C++20."));
    });
}

void MainWindow::buildToolbar()
{
    QToolBar *toolbar = addToolBar(tr("Main Toolbar"));
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(20, 20));

    QAction *ffmpegAction = toolbar->addAction(tr("Set ffmpeg.exe"));
    connect(ffmpegAction, &QAction::triggered, this, &MainWindow::onBrowseFfmpeg);

    QAction *ffprobeAction = toolbar->addAction(tr("Set ffprobe.exe"));
    connect(ffprobeAction, &QAction::triggered, this, &MainWindow::onBrowseFfprobe);

    toolbar->addSeparator();

    QAction *videoAction = toolbar->addAction(tr("Open Video"));
    videoAction->setShortcut(QKeySequence::Open);
    connect(videoAction, &QAction::triggered, this, &MainWindow::onBrowseVideo);

    QAction *outputAction = toolbar->addAction(tr("Output Folder"));
    connect(outputAction, &QAction::triggered, this, &MainWindow::onBrowseOutputFolder);

    toolbar->addSeparator();

    // Path indicator labels shown directly in the toolbar for quick glance.
    auto *pathsWidget = new QWidget(toolbar);
    auto *pathsLayout = new QVBoxLayout(pathsWidget);
    pathsLayout->setContentsMargins(8, 0, 8, 0);
    pathsLayout->setSpacing(1);

    m_ffmpegPathLabel = new QLabel(tr("ffmpeg: (not set)"), pathsWidget);
    m_ffprobePathLabel = new QLabel(tr("ffprobe: (not set)"), pathsWidget);
    m_ffmpegPathLabel->setObjectName(QStringLiteral("pathLabel"));
    m_ffprobePathLabel->setObjectName(QStringLiteral("pathLabel"));
    pathsLayout->addWidget(m_ffmpegPathLabel);
    pathsLayout->addWidget(m_ffprobePathLabel);
    toolbar->addWidget(pathsWidget);
}

void MainWindow::buildCentralWidget()
{
    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);

    // --- Video / output selection row ---
    auto *selectionGroup = new QGroupBox(tr("Video && Output"), central);
    auto *selectionLayout = new QGridLayout(selectionGroup);

    auto *videoBrowseBtn = new QPushButton(tr("Browse Video..."), selectionGroup);
    connect(videoBrowseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseVideo);
    m_videoPathLabel = new QLabel(tr("No video selected. You can also drag && drop a file here."), selectionGroup);
    m_videoPathLabel->setWordWrap(true);

    auto *outputBrowseBtn = new QPushButton(tr("Choose Output Folder..."), selectionGroup);
    connect(outputBrowseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseOutputFolder);
    m_outputFolderLabel = new QLabel(tr("No output folder selected."), selectionGroup);
    m_outputFolderLabel->setWordWrap(true);

    selectionLayout->addWidget(videoBrowseBtn, 0, 0);
    selectionLayout->addWidget(m_videoPathLabel, 0, 1);
    selectionLayout->addWidget(outputBrowseBtn, 1, 0);
    selectionLayout->addWidget(m_outputFolderLabel, 1, 1);
    selectionLayout->setColumnStretch(1, 1);

    mainLayout->addWidget(selectionGroup);

    // --- Track table ---
    auto *tableGroup = new QGroupBox(tr("Audio Tracks"), central);
    auto *tableLayout = new QVBoxLayout(tableGroup);

    m_trackTable = new QTableWidget(0, ColCount, tableGroup);
    m_trackTable->setHorizontalHeaderLabels({
        tr("Track"), tr("Codec"), tr("Bitrate"), tr("Channels"),
        tr("Sample Rate"), tr("Language"), tr("Duration"), tr("Default")
    });
    m_trackTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_trackTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_trackTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_trackTable->setAlternatingRowColors(true);
    m_trackTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_trackTable, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        if (m_currentTracks.isEmpty()) {
            return;
        }
        QMenu menu(this);
        menu.addAction(tr("Extract Track 1"), this, &MainWindow::onExtractTrack1);
        menu.addAction(tr("Extract Track 2"), this, &MainWindow::onExtractTrack2);
        menu.addAction(tr("Extract All Tracks"), this, &MainWindow::onExtractAllTracks);
        menu.addSeparator();
        menu.addAction(tr("Swap Track 1 && 2"), this, &MainWindow::onSwapTracks);
        menu.exec(m_trackTable->viewport()->mapToGlobal(pos));
    });

    tableLayout->addWidget(m_trackTable);
    mainLayout->addWidget(tableGroup, 1);

    // --- Operations panel ---
    auto *opsGroup = new QGroupBox(tr("Operations"), central);
    auto *opsLayout = new QGridLayout(opsGroup);

    m_extractTrack1Btn = new QPushButton(tr("Extract Track 1"), opsGroup);
    m_extractTrack2Btn = new QPushButton(tr("Extract Track 2"), opsGroup);
    m_extractAllBtn    = new QPushButton(tr("Extract All Tracks"), opsGroup);
    m_swapBtn           = new QPushButton(tr("Swap Track 1 && 2"), opsGroup);
    m_deleteTrack1Btn   = new QPushButton(tr("Delete Track 1"), opsGroup);
    m_deleteTrack2Btn   = new QPushButton(tr("Delete Track 2"), opsGroup);
    m_deleteAllAudioBtn = new QPushButton(tr("Delete All Audio Tracks"), opsGroup);
    m_muteBtn           = new QPushButton(tr("Mute Video"), opsGroup);
    m_replaceAudioBtn   = new QPushButton(tr("Replace Audio..."), opsGroup);
    m_mergeAudioBtn     = new QPushButton(tr("Merge External Audio..."), opsGroup);
    m_cancelBtn         = new QPushButton(tr("Cancel"), opsGroup);
    m_cancelBtn->setEnabled(false);

    const QList<QPushButton *> allOpButtons = {
        m_extractTrack1Btn, m_extractTrack2Btn, m_extractAllBtn, m_swapBtn,
        m_deleteTrack1Btn, m_deleteTrack2Btn, m_deleteAllAudioBtn, m_muteBtn,
        m_replaceAudioBtn, m_mergeAudioBtn
    };
    for (QPushButton *btn : allOpButtons) {
        btn->setObjectName(QStringLiteral("opButton"));
        btn->setMinimumHeight(34);
    }
    m_cancelBtn->setObjectName(QStringLiteral("cancelButton"));
    m_cancelBtn->setMinimumHeight(34);

    int row = 0, col = 0;
    const int columns = 4;
    for (QPushButton *btn : allOpButtons) {
        opsLayout->addWidget(btn, row, col);
        if (++col >= columns) { col = 0; ++row; }
    }
    opsLayout->addWidget(m_cancelBtn, row + 1, 0, 1, columns);

    mainLayout->addWidget(opsGroup);

    // --- Progress area ---
    auto *progressGroup = new QGroupBox(tr("Progress"), central);
    auto *progressLayout = new QVBoxLayout(progressGroup);

    m_progressBar = new QProgressBar(progressGroup);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    progressLayout->addWidget(m_progressBar);

    auto *timeRow = new QHBoxLayout();
    m_elapsedLabel = new QLabel(tr("Elapsed: 00:00:00"), progressGroup);
    m_remainingLabel = new QLabel(tr("Remaining: --:--:--"), progressGroup);
    timeRow->addWidget(m_elapsedLabel);
    timeRow->addStretch(1);
    timeRow->addWidget(m_remainingLabel);
    progressLayout->addLayout(timeRow);

    m_commandLabel = new QLabel(tr("No command running."), progressGroup);
    m_commandLabel->setWordWrap(true);
    m_commandLabel->setObjectName(QStringLiteral("commandLabel"));
    progressLayout->addWidget(m_commandLabel);

    mainLayout->addWidget(progressGroup);

    setCentralWidget(central);

    // Timer used purely to keep the elapsed-time label ticking between
    // progress updates from ffmpeg (which only arrive when a new "time="
    // line is printed).
    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(500);
    connect(m_elapsedTimer, &QTimer::timeout, this, &MainWindow::updateElapsedTimeDisplay);
}

void MainWindow::buildConsoleDock()
{
    m_consoleDock = new QDockWidget(tr("FFmpeg Console"), this);
    m_consoleDock->setObjectName(QStringLiteral("consoleDock"));
    m_consoleDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

    auto *container = new QWidget(m_consoleDock);
    auto *layout = new QVBoxLayout(container);

    m_consoleView = new QPlainTextEdit(container);
    m_consoleView->setReadOnly(true);
    m_consoleView->setMaximumBlockCount(5000);
    m_consoleView->setObjectName(QStringLiteral("consoleView"));
    layout->addWidget(m_consoleView);

    auto *buttonRow = new QHBoxLayout();
    auto *saveLogBtn = new QPushButton(tr("Save Log..."), container);
    connect(saveLogBtn, &QPushButton::clicked, this, &MainWindow::onSaveLog);
    auto *clearLogBtn = new QPushButton(tr("Clear"), container);
    connect(clearLogBtn, &QPushButton::clicked, m_consoleView, &QPlainTextEdit::clear);
    buttonRow->addWidget(saveLogBtn);
    buttonRow->addWidget(clearLogBtn);
    buttonRow->addStretch(1);
    layout->addLayout(buttonRow);

    m_consoleDock->setWidget(container);
    addDockWidget(Qt::BottomDockWidgetArea, m_consoleDock);
}

void MainWindow::buildRecentFilesDock()
{
    m_recentDock = new QDockWidget(tr("Recent Files"), this);
    m_recentDock->setObjectName(QStringLiteral("recentDock"));
    m_recentDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto *container = new QWidget(m_recentDock);
    auto *layout = new QVBoxLayout(container);

    m_recentFilesList = new QListWidget(container);
    m_recentFilesList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_recentFilesList, &QListWidget::itemDoubleClicked, this, &MainWindow::onRecentFileActivated);
    connect(m_recentFilesList, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu(this);
        menu.addAction(tr("Clear Recent Files"), this, &MainWindow::onClearRecentFiles);
        menu.exec(m_recentFilesList->viewport()->mapToGlobal(pos));
    });
    layout->addWidget(m_recentFilesList);

    m_recentDock->setWidget(container);
    addDockWidget(Qt::LeftDockWidgetArea, m_recentDock);
}

void MainWindow::buildStatusBar()
{
    statusBar()->showMessage(tr("Ready."));
}

void MainWindow::connectSignals()
{
    connect(m_extractTrack1Btn, &QPushButton::clicked, this, &MainWindow::onExtractTrack1);
    connect(m_extractTrack2Btn, &QPushButton::clicked, this, &MainWindow::onExtractTrack2);
    connect(m_extractAllBtn, &QPushButton::clicked, this, &MainWindow::onExtractAllTracks);
    connect(m_swapBtn, &QPushButton::clicked, this, &MainWindow::onSwapTracks);
    connect(m_deleteTrack1Btn, &QPushButton::clicked, this, &MainWindow::onDeleteTrack1);
    connect(m_deleteTrack2Btn, &QPushButton::clicked, this, &MainWindow::onDeleteTrack2);
    connect(m_deleteAllAudioBtn, &QPushButton::clicked, this, &MainWindow::onDeleteAllAudioTracks);
    connect(m_muteBtn, &QPushButton::clicked, this, &MainWindow::onMuteVideo);
    connect(m_replaceAudioBtn, &QPushButton::clicked, this, &MainWindow::onReplaceAudio);
    connect(m_mergeAudioBtn, &QPushButton::clicked, this, &MainWindow::onMergeExternalAudio);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancelOperation);

    connect(m_ffmpeg.get(), &FFmpegWrapper::probeFinished, this, &MainWindow::onProbeFinished);
    connect(m_ffmpeg.get(), &FFmpegWrapper::operationStarted, this, &MainWindow::onOperationStarted);
    connect(m_ffmpeg.get(), &FFmpegWrapper::outputLine, this, &MainWindow::onOutputLine);
    connect(m_ffmpeg.get(), &FFmpegWrapper::progressChanged, this, &MainWindow::onProgressChanged);
    connect(m_ffmpeg.get(), &FFmpegWrapper::operationFinished, this, &MainWindow::onOperationFinished);

    connect(&Logger::instance(), &Logger::messageLogged, this, [this](LogLevel level, const QString &line) {
        appendConsoleLine(level, line);
    });
}

// ---------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------

void MainWindow::applyDarkTheme()
{
    m_darkTheme = true;
    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    const QString styleSheet = QStringLiteral(R"(
        QMainWindow, QWidget { background-color: #1e1f22; color: #e6e6e6; }
        QGroupBox { border: 1px solid #3a3d41; border-radius: 6px; margin-top: 10px; font-weight: bold; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #9cdcfe; }
        QPushButton#opButton {
            background-color: #2d2f33; border: 1px solid #454951; border-radius: 8px;
            padding: 6px 10px; color: #e6e6e6;
        }
        QPushButton#opButton:hover { background-color: #3a3d41; border-color: #5a90ff; }
        QPushButton#opButton:pressed { background-color: #232529; }
        QPushButton#opButton:disabled { color: #6b6f76; background-color: #232529; }
        QPushButton#cancelButton {
            background-color: #5a1e1e; border: 1px solid #8a2f2f; border-radius: 8px; color: #ffdddd;
        }
        QPushButton#cancelButton:disabled { color: #6b6f76; background-color: #232529; border-color: #454951; }
        QPushButton { border-radius: 6px; padding: 5px 10px; }
        QTableWidget { background-color: #26282b; gridline-color: #3a3d41; }
        QHeaderView::section { background-color: #2d2f33; color: #cfd2d6; padding: 4px; border: none; }
        QPlainTextEdit#consoleView { background-color: #101114; color: #d8d8d8; font-family: Consolas, monospace; }
        QLabel#pathLabel { color: #9cdcfe; font-size: 10px; }
        QLabel#commandLabel { color: #9cdcfe; font-family: Consolas, monospace; font-size: 11px; }
        QProgressBar { border: 1px solid #454951; border-radius: 6px; text-align: center; }
        QProgressBar::chunk { background-color: #3a7bd5; border-radius: 6px; }
        QDockWidget::title { background-color: #2d2f33; padding: 4px; }
        QToolBar { background-color: #26282b; border: none; spacing: 6px; }
        QListWidget { background-color: #26282b; }
        QMenuBar { background-color: #26282b; }
        QMenu { background-color: #2d2f33; color: #e6e6e6; }
    )");
    qApp->setStyleSheet(styleSheet);
}

void MainWindow::applyLightTheme()
{
    m_darkTheme = false;
    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    qApp->setStyleSheet(QStringLiteral(R"(
        QPushButton#opButton { border-radius: 8px; padding: 6px 10px; }
        QPushButton#cancelButton { border-radius: 8px; padding: 6px 10px; color: #a00000; }
        QPlainTextEdit#consoleView { font-family: Consolas, monospace; }
        QLabel#commandLabel { font-family: Consolas, monospace; font-size: 11px; }
    )"));
}

void MainWindow::onToggleTheme()
{
    if (m_darkTheme) {
        applyLightTheme();
    } else {
        applyDarkTheme();
    }
    m_settings->setTheme(m_darkTheme ? QStringLiteral("dark") : QStringLiteral("light"));
}

// ---------------------------------------------------------------------
// Settings persistence
// ---------------------------------------------------------------------

void MainWindow::restoreSettings()
{
    m_ffmpegPath = m_settings->ffmpegPath();
    m_ffprobePath = m_settings->ffprobePath();
    m_outputFolder = m_settings->outputFolder();

    if (!m_ffmpegPath.isEmpty()) {
        m_ffmpegPathLabel->setText(tr("ffmpeg: %1").arg(m_ffmpegPath));
        m_ffmpeg->setFfmpegPath(m_ffmpegPath);
    }
    if (!m_ffprobePath.isEmpty()) {
        m_ffprobePathLabel->setText(tr("ffprobe: %1").arg(m_ffprobePath));
        m_ffmpeg->setFfprobePath(m_ffprobePath);
    }
    if (!m_outputFolder.isEmpty()) {
        m_outputFolderLabel->setText(m_outputFolder);
    }

    const QByteArray geometry = m_settings->windowGeometry();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    const QByteArray state = m_settings->windowState();
    if (!state.isEmpty()) {
        restoreState(state);
    }

    if (m_settings->theme() == QLatin1String("light")) {
        applyLightTheme();
    }

    updateRecentFilesList();

    if (m_ffmpegPath.isEmpty() || m_ffprobePath.isEmpty()) {
        QTimer::singleShot(300, this, [this]() {
            QMessageBox::information(this, tr("Setup Required"),
                                      tr("Please configure the paths to ffmpeg.exe and ffprobe.exe "
                                         "using the toolbar buttons before working with videos."));
        });
    }
}

void MainWindow::saveSettingsNow()
{
    m_settings->setFfmpegPath(m_ffmpegPath);
    m_settings->setFfprobePath(m_ffprobePath);
    m_settings->setOutputFolder(m_outputFolder);
    m_settings->setWindowGeometry(saveGeometry());
    m_settings->setWindowState(saveState());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_operationRunning) {
        const auto reply = QMessageBox::question(this, tr("Operation in Progress"),
                                                   tr("An ffmpeg operation is still running. "
                                                      "Cancel it and exit anyway?"),
                                                   QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        m_ffmpeg->cancelCurrentOperation();
    }
    saveSettingsNow();
    event->accept();
}

// ---------------------------------------------------------------------
// Drag and drop
// ---------------------------------------------------------------------

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) {
        return;
    }

    const QString path = urls.first().toLocalFile();
    const QFileInfo info(path);

    if (info.isDir()) {
        // A folder was dropped: treat it as the output folder.
        m_outputFolder = path;
        m_outputFolderLabel->setText(m_outputFolder);
        m_settings->addRecentFolder(m_outputFolder);
        Logger::instance().info(tr("Output folder set via drag && drop: %1").arg(path));
        return;
    }

    const QString ext = info.suffix().toLower();
    if (ext == QLatin1String("exe")) {
        // Executable dropped: guess whether it's ffmpeg or ffprobe by name.
        const QString baseName = info.completeBaseName().toLower();
        if (baseName.contains(QStringLiteral("ffprobe"))) {
            m_ffprobePath = path;
            m_ffprobePathLabel->setText(tr("ffprobe: %1").arg(path));
            m_ffmpeg->setFfprobePath(path);
            Logger::instance().info(tr("ffprobe path set via drag && drop: %1").arg(path));
        } else {
            m_ffmpegPath = path;
            m_ffmpegPathLabel->setText(tr("ffmpeg: %1").arg(path));
            m_ffmpeg->setFfmpegPath(path);
            Logger::instance().info(tr("ffmpeg path set via drag && drop: %1").arg(path));
        }
        return;
    }

    if (FileUtils::supportedVideoExtensions().contains(ext)) {
        loadVideo(path);
    } else {
        QMessageBox::warning(this, tr("Unsupported File"),
                              tr("The dropped file type is not a supported video format."));
    }
}

// ---------------------------------------------------------------------
// Toolbar / browse actions
// ---------------------------------------------------------------------

void MainWindow::onBrowseFfmpeg()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Locate ffmpeg.exe"),
                                                        QString(), tr("Executable (ffmpeg.exe);;All Files (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    m_ffmpegPath = path;
    m_ffmpeg->setFfmpegPath(path);
    m_ffmpegPathLabel->setText(tr("ffmpeg: %1").arg(path));
    m_settings->setFfmpegPath(path);
    Logger::instance().success(tr("ffmpeg path set: %1").arg(path));
}

void MainWindow::onBrowseFfprobe()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Locate ffprobe.exe"),
                                                        QString(), tr("Executable (ffprobe.exe);;All Files (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    m_ffprobePath = path;
    m_ffmpeg->setFfprobePath(path);
    m_ffprobePathLabel->setText(tr("ffprobe: %1").arg(path));
    m_settings->setFfprobePath(path);
    Logger::instance().success(tr("ffprobe path set: %1").arg(path));
}

void MainWindow::onBrowseVideo()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Select Video File"),
                                                        QString(), FileUtils::videoFileDialogFilter());
    if (path.isEmpty()) {
        return;
    }
    loadVideo(path);
}

void MainWindow::onBrowseOutputFolder()
{
    const QString path = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"));
    if (path.isEmpty()) {
        return;
    }
    m_outputFolder = path;
    m_outputFolderLabel->setText(path);
    m_settings->setOutputFolder(path);
    m_settings->addRecentFolder(path);
    Logger::instance().info(tr("Output folder set: %1").arg(path));
}

void MainWindow::onRecentFileActivated(QListWidgetItem *item)
{
    if (!item) {
        return;
    }
    const QString path = item->data(Qt::UserRole).toString();
    if (FileUtils::isValidFile(path)) {
        loadVideo(path);
    } else {
        QMessageBox::warning(this, tr("File Not Found"), tr("This file no longer exists:\n%1").arg(path));
    }
}

void MainWindow::onClearRecentFiles()
{
    m_settings->clearRecentFiles();
    updateRecentFilesList();
}

void MainWindow::updateRecentFilesList()
{
    m_recentFilesList->clear();
    for (const QString &path : m_settings->recentFiles()) {
        auto *item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        m_recentFilesList->addItem(item);
    }
}

// ---------------------------------------------------------------------
// Loading a video / probing tracks
// ---------------------------------------------------------------------

void MainWindow::loadVideo(const QString &path)
{
    if (m_ffmpegPath.isEmpty() || m_ffprobePath.isEmpty()) {
        QMessageBox::warning(this, tr("Missing FFmpeg / FFprobe"),
                              tr("Please configure both ffmpeg.exe and ffprobe.exe paths first."));
        return;
    }
    if (!FileUtils::isValidFile(path)) {
        QMessageBox::warning(this, tr("Missing Input File"), tr("The selected file does not exist or cannot be read."));
        return;
    }

    m_currentVideoPath = path;
    m_videoPathLabel->setText(path);
    m_settings->addRecentFile(path);
    updateRecentFilesList();

    statusBar()->showMessage(tr("Probing audio tracks..."));
    Logger::instance().info(tr("Loading video: %1").arg(path));
    m_ffmpeg->probeAudioTracks(path);
}

void MainWindow::onProbeFinished(bool success, const QVector<AudioTrackInfo> &tracks,
                                  double durationSeconds, const QString &errorMessage)
{
    if (!success) {
        statusBar()->showMessage(tr("Probe failed."), 5000);
        Logger::instance().error(tr("Probe failed: %1").arg(errorMessage));
        QMessageBox::critical(this, tr("Probe Failed"), errorMessage);
        return;
    }

    m_currentTracks = tracks;
    m_currentDurationSeconds = durationSeconds;
    refreshTrackTable(tracks);

    if (tracks.isEmpty()) {
        statusBar()->showMessage(tr("This video has no audio tracks."), 5000);
        Logger::instance().warning(tr("No audio tracks found in this video."));
    } else if (tracks.size() == 1) {
        statusBar()->showMessage(tr("1 audio track found."), 5000);
        Logger::instance().info(tr("This video only has one audio track; some operations are disabled."));
    } else {
        statusBar()->showMessage(tr("%1 audio tracks found.").arg(tracks.size()), 5000);
        Logger::instance().success(tr("%1 audio tracks detected.").arg(tracks.size()));
    }

    // Enable/disable operation buttons based on how many audio tracks exist.
    const bool hasAudio = !tracks.isEmpty();
    const bool hasTwoPlus = tracks.size() >= 2;

    m_extractTrack1Btn->setEnabled(hasAudio);
    m_extractTrack2Btn->setEnabled(hasTwoPlus);
    m_extractAllBtn->setEnabled(hasAudio);
    m_swapBtn->setEnabled(hasTwoPlus);
    m_deleteTrack1Btn->setEnabled(hasAudio);
    m_deleteTrack2Btn->setEnabled(hasTwoPlus);
    m_deleteAllAudioBtn->setEnabled(hasAudio);
    m_muteBtn->setEnabled(true);
    m_replaceAudioBtn->setEnabled(true);
    m_mergeAudioBtn->setEnabled(true);
}

void MainWindow::refreshTrackTable(const QVector<AudioTrackInfo> &tracks)
{
    m_trackTable->setRowCount(tracks.size());
    for (int row = 0; row < tracks.size(); ++row) {
        const AudioTrackInfo &t = tracks.at(row);
        m_trackTable->setItem(row, ColTrackNumber, new QTableWidgetItem(tr("Track %1").arg(t.audioTrackNumber)));
        m_trackTable->setItem(row, ColCodec, new QTableWidgetItem(t.codec.isEmpty() ? tr("Unknown") : t.codec.toUpper()));
        m_trackTable->setItem(row, ColBitrate, new QTableWidgetItem(FileUtils::formatBitrate(t.bitRate)));
        m_trackTable->setItem(row, ColChannels, new QTableWidgetItem(
            t.channelLayout.isEmpty() ? QString::number(t.channels) : QStringLiteral("%1 (%2)").arg(t.channels).arg(t.channelLayout)));
        m_trackTable->setItem(row, ColSampleRate, new QTableWidgetItem(t.sampleRate > 0 ? tr("%1 Hz").arg(t.sampleRate) : tr("N/A")));
        m_trackTable->setItem(row, ColLanguage, new QTableWidgetItem(t.language.isEmpty() ? tr("N/A") : t.language));
        m_trackTable->setItem(row, ColDuration, new QTableWidgetItem(FileUtils::formatDuration(t.durationSeconds)));
        m_trackTable->setItem(row, ColDefault, new QTableWidgetItem(t.isDefault ? tr("Yes") : tr("No")));
    }
}

// ---------------------------------------------------------------------
// Operations
// ---------------------------------------------------------------------

bool MainWindow::validateReadyForOperation(int minAudioTracksRequired)
{
    if (m_ffmpegPath.isEmpty() || m_ffprobePath.isEmpty()) {
        QMessageBox::warning(this, tr("Missing FFmpeg / FFprobe"), tr("Please configure ffmpeg.exe and ffprobe.exe first."));
        return false;
    }
    if (m_currentVideoPath.isEmpty() || !FileUtils::isValidFile(m_currentVideoPath)) {
        QMessageBox::warning(this, tr("Missing Input File"), tr("Please select a valid video file first."));
        return false;
    }
    if (m_outputFolder.isEmpty() || !FileUtils::isValidDirectory(m_outputFolder)) {
        QMessageBox::warning(this, tr("Invalid Output Folder"), tr("Please choose a valid output folder first."));
        return false;
    }
    if (minAudioTracksRequired > 0 && m_currentTracks.size() < minAudioTracksRequired) {
        QMessageBox::warning(this, tr("Not Enough Audio Tracks"),
                              tr("This operation requires at least %1 audio track(s), "
                                 "but this video only has %2.")
                                  .arg(minAudioTracksRequired).arg(m_currentTracks.size()));
        return false;
    }
    if (m_operationRunning) {
        QMessageBox::information(this, tr("Busy"), tr("Another operation is already running. Please wait or cancel it."));
        return false;
    }
    return true;
}

void MainWindow::runOperation(OperationType op, const QString &extraInputPath)
{
    setBusy(true);
    m_ffmpeg->runOperation(op, m_currentVideoPath, m_outputFolder, m_currentTracks,
                            m_currentDurationSeconds, extraInputPath);
}

void MainWindow::onExtractTrack1()
{
    if (!validateReadyForOperation(1)) return;
    runOperation(OperationType::ExtractTrack1);
}

void MainWindow::onExtractTrack2()
{
    if (!validateReadyForOperation(2)) return;
    runOperation(OperationType::ExtractTrack2);
}

void MainWindow::onExtractAllTracks()
{
    if (!validateReadyForOperation(1)) return;
    runOperation(OperationType::ExtractAllTracks);
}

void MainWindow::onSwapTracks()
{
    if (!validateReadyForOperation(2)) return;
    runOperation(OperationType::SwapTrack1And2);
}

void MainWindow::onDeleteTrack1()
{
    if (!validateReadyForOperation(1)) return;
    runOperation(OperationType::DeleteTrack1);
}

void MainWindow::onDeleteTrack2()
{
    if (!validateReadyForOperation(2)) return;
    runOperation(OperationType::DeleteTrack2);
}

void MainWindow::onDeleteAllAudioTracks()
{
    if (!validateReadyForOperation(1)) return;
    const auto reply = QMessageBox::question(this, tr("Confirm"),
                                              tr("This will remove all audio tracks from the video. Continue?"),
                                              QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;
    runOperation(OperationType::DeleteAllAudioTracks);
}

void MainWindow::onMuteVideo()
{
    if (!validateReadyForOperation(0)) return;
    runOperation(OperationType::MuteVideo);
}

QString MainWindow::promptForExternalAudioFile()
{
    return QFileDialog::getOpenFileName(this, tr("Select Audio File"), QString(),
                                         tr("Audio Files (*.wav *.mp3 *.aac *.flac *.m4a *.ogg);;All Files (*.*)"));
}

void MainWindow::onReplaceAudio()
{
    if (!validateReadyForOperation(0)) return;
    const QString audioPath = promptForExternalAudioFile();
    if (audioPath.isEmpty()) return;
    runOperation(OperationType::ReplaceAudio, audioPath);
}

void MainWindow::onMergeExternalAudio()
{
    if (!validateReadyForOperation(0)) return;
    const QString audioPath = promptForExternalAudioFile();
    if (audioPath.isEmpty()) return;
    runOperation(OperationType::MergeExternalAudio, audioPath);
}

void MainWindow::onCancelOperation()
{
    m_ffmpeg->cancelCurrentOperation();
    Logger::instance().warning(tr("Operation cancelled by user."));
}

// ---------------------------------------------------------------------
// FFmpegWrapper callbacks
// ---------------------------------------------------------------------

void MainWindow::onOperationStarted(const QString &commandLine)
{
    m_commandLabel->setText(commandLine);
    m_progressBar->setValue(0);
    m_operationElapsedMs = 0;
    m_operationRemainingMs = 0;
    m_elapsedTimer->start();
    statusBar()->showMessage(tr("Running ffmpeg..."));
}

void MainWindow::onOutputLine(const QString &line, bool isStderr)
{
    Q_UNUSED(isStderr);
    LogLevel level = LogLevel::Info;
    const QString lower = line.toLower();
    if (lower.contains(QStringLiteral("error"))) {
        level = LogLevel::Error;
    } else if (lower.contains(QStringLiteral("warning"))) {
        level = LogLevel::Warning;
    }
    Logger::instance().log(level, line);
}

void MainWindow::onProgressChanged(int percent, qint64 elapsedMs, qint64 remainingMsEstimate)
{
    m_progressBar->setValue(percent);
    m_operationElapsedMs = elapsedMs;
    m_operationRemainingMs = remainingMsEstimate;
    updateElapsedTimeDisplay();
}

void MainWindow::updateElapsedTimeDisplay()
{
    if (!m_operationRunning) {
        return;
    }
    const qint64 elapsed = m_elapsedTimer->isActive() ? m_operationElapsedMs : 0;
    const QTime elapsedTime = QTime(0, 0).addMSecs(static_cast<int>(elapsed));
    m_elapsedLabel->setText(tr("Elapsed: %1").arg(elapsedTime.toString(QStringLiteral("hh:mm:ss"))));

    if (m_operationRemainingMs > 0) {
        const QTime remainingTime = QTime(0, 0).addMSecs(static_cast<int>(m_operationRemainingMs));
        m_remainingLabel->setText(tr("Remaining: %1").arg(remainingTime.toString(QStringLiteral("hh:mm:ss"))));
    } else {
        m_remainingLabel->setText(tr("Remaining: --:--:--"));
    }
}

void MainWindow::onOperationFinished(bool success, const QString &message, const QString &outputFilePath)
{
    setBusy(false);
    m_elapsedTimer->stop();
    m_progressBar->setValue(success ? 100 : m_progressBar->value());

    if (success) {
        statusBar()->showMessage(tr("Operation completed: %1").arg(outputFilePath), 8000);
        QMessageBox::information(this, tr("Success"),
                                  tr("Operation completed successfully.\n\nOutput:\n%1").arg(outputFilePath));
        // Re-probe the source video is unnecessary; the track table still
        // reflects the original input, which remains valid.
    } else {
        statusBar()->showMessage(tr("Operation failed."), 8000);
        QMessageBox::critical(this, tr("Operation Failed"), message);
    }
}

// ---------------------------------------------------------------------
// Console / logging
// ---------------------------------------------------------------------

void MainWindow::appendConsoleLine(LogLevel level, const QString &text)
{
    QString color;
    switch (level) {
    case LogLevel::Error:   color = QStringLiteral("#ff6b6b"); break;
    case LogLevel::Warning: color = QStringLiteral("#f2c94c"); break;
    case LogLevel::Success: color = QStringLiteral("#6bd88a"); break;
    case LogLevel::Command: color = QStringLiteral("#9cdcfe"); break;
    case LogLevel::Info:    color = QStringLiteral("#d8d8d8"); break;
    }
    m_consoleView->appendHtml(QStringLiteral("<span style='color:%1;'>%2</span>")
                                   .arg(color, text.toHtmlEscaped()));
    QScrollBar *bar = m_consoleView->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void MainWindow::onSaveLog()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Log"), QStringLiteral("ffmpeg_log.txt"),
                                                        tr("Text Files (*.txt);;All Files (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    if (Logger::instance().saveSessionLog(path)) {
        statusBar()->showMessage(tr("Log saved to %1").arg(path), 5000);
    } else {
        QMessageBox::warning(this, tr("Save Failed"), tr("Could not write log file to:\n%1").arg(path));
    }
}

// ---------------------------------------------------------------------
// Busy state
// ---------------------------------------------------------------------

void MainWindow::setBusy(bool busy)
{
    m_operationRunning = busy;
    m_cancelBtn->setEnabled(busy);

    const QList<QPushButton *> allOpButtons = {
        m_extractTrack1Btn, m_extractTrack2Btn, m_extractAllBtn, m_swapBtn,
        m_deleteTrack1Btn, m_deleteTrack2Btn, m_deleteAllAudioBtn, m_muteBtn,
        m_replaceAudioBtn, m_mergeAudioBtn
    };
    for (QPushButton *btn : allOpButtons) {
        btn->setEnabled(!busy);
    }

    if (busy) {
        m_elapsedTimer->start();
    }
}
