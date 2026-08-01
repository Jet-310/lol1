#pragma once

#include <QMainWindow>
#include <QVector>
#include <memory>

#include "FFmpegWrapper.h"
#include "Logger.h"
class QTableWidget;
class QPlainTextEdit;
class QLabel;
class QProgressBar;
class QPushButton;
class QComboBox;
class QToolBar;
class QDockWidget;
class QListWidget;
class QListWidgetItem;
class QAction;
class QTimer;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;

class SettingsManager;
class Logger;

// MainWindow is the application's single top level window. The entire UI
// is constructed in code (no .ui file) for maximum transparency and to
// keep the project buildable with a plain CMake + Qt toolchain with no
// UIC-specific IDE configuration required.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    // Toolbar / menu actions
    void onBrowseFfmpeg();
    void onBrowseFfprobe();
    void onBrowseVideo();
    void onBrowseOutputFolder();
    void onRecentFileActivated(QListWidgetItem *item);
    void onClearRecentFiles();

    // Operations
    void onExtractTrack1();
    void onExtractTrack2();
    void onExtractAllTracks();
    void onSwapTracks();
    void onDeleteTrack1();
    void onDeleteTrack2();
    void onDeleteAllAudioTracks();
    void onMuteVideo();
    void onReplaceAudio();
    void onMergeExternalAudio();
    void onCancelOperation();

    // FFmpegWrapper callbacks
    void onProbeFinished(bool success, const QVector<AudioTrackInfo> &tracks,
                          double durationSeconds, const QString &errorMessage);
    void onOperationStarted(const QString &commandLine);
    void onOutputLine(const QString &line, bool isStderr);
    void onProgressChanged(int percent, qint64 elapsedMs, qint64 remainingMsEstimate);
    void onOperationFinished(bool success, const QString &message, const QString &outputFilePath);

    // Misc UI
    void onSaveLog();
    void onToggleTheme();
    void updateElapsedTimeDisplay();

private:
    // --- UI construction helpers ---
    void buildUi();
    void buildToolbar();
    void buildCentralWidget();
    void buildConsoleDock();
    void buildRecentFilesDock();
    void buildStatusBar();
    void applyDarkTheme();
    void applyLightTheme();
    void connectSignals();
    void restoreSettings();
    void saveSettingsNow();

    // --- Logic helpers ---
    void loadVideo(const QString &path);
    void refreshTrackTable(const QVector<AudioTrackInfo> &tracks);
    void appendConsoleLine(LogLevel level, const QString &text);
    void setBusy(bool busy);
    bool validateReadyForOperation(int minAudioTracksRequired = 0);
    void runOperation(OperationType op, const QString &extraInputPath = QString());
    void updateRecentFilesList();
    QString promptForExternalAudioFile();

    // --- Members ---
    std::unique_ptr<SettingsManager> m_settings;
    std::unique_ptr<FFmpegWrapper> m_ffmpeg;

    // Paths
    QString m_ffmpegPath;
    QString m_ffprobePath;
    QString m_currentVideoPath;
    QString m_outputFolder;
    double m_currentDurationSeconds = 0.0;
    QVector<AudioTrackInfo> m_currentTracks;
    bool m_operationRunning = false;

    // Toolbar / path widgets
    QLabel *m_ffmpegPathLabel = nullptr;
    QLabel *m_ffprobePathLabel = nullptr;
    QLabel *m_videoPathLabel = nullptr;
    QLabel *m_outputFolderLabel = nullptr;

    // Central widget
    QTableWidget *m_trackTable = nullptr;
    QPushButton *m_extractTrack1Btn = nullptr;
    QPushButton *m_extractTrack2Btn = nullptr;
    QPushButton *m_extractAllBtn = nullptr;
    QPushButton *m_swapBtn = nullptr;
    QPushButton *m_deleteTrack1Btn = nullptr;
    QPushButton *m_deleteTrack2Btn = nullptr;
    QPushButton *m_deleteAllAudioBtn = nullptr;
    QPushButton *m_muteBtn = nullptr;
    QPushButton *m_replaceAudioBtn = nullptr;
    QPushButton *m_mergeAudioBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;

    // Progress area
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_elapsedLabel = nullptr;
    QLabel *m_remainingLabel = nullptr;
    QLabel *m_commandLabel = nullptr;
    QTimer *m_elapsedTimer = nullptr;
    qint64 m_operationElapsedMs = 0;
    qint64 m_operationRemainingMs = 0;

    // Console dock
    QDockWidget *m_consoleDock = nullptr;
    QPlainTextEdit *m_consoleView = nullptr;

    // Recent files dock
    QDockWidget *m_recentDock = nullptr;
    QListWidget *m_recentFilesList = nullptr;

    // Theme
    bool m_darkTheme = true;
};
