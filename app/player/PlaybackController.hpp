#pragma once

#include "player/IAudioPlayer.hpp"
#include "playlist/QueueService.hpp"

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <memory>

namespace opusora {

class PlaybackController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY currentTrackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY currentTrackChanged)
    Q_PROPERTY(QString album READ album NOTIFY currentTrackChanged)
    Q_PROPERTY(QUrl coverUrl READ coverUrl NOTIFY currentTrackChanged)
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY durationChanged)
    Q_PROPERTY(QString playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playbackStateChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(double equalizerLowGain READ equalizerLowGain WRITE setEqualizerLowGain NOTIFY equalizerChanged)
    Q_PROPERTY(double equalizerMidGain READ equalizerMidGain WRITE setEqualizerMidGain NOTIFY equalizerChanged)
    Q_PROPERTY(double equalizerHighGain READ equalizerHighGain WRITE setEqualizerHighGain NOTIFY equalizerChanged)
    Q_PROPERTY(bool replayGainEnabled READ replayGainEnabled WRITE setReplayGainEnabled NOTIFY replayGainEnabledChanged)
    Q_PROPERTY(bool gaplessPlaybackEnabled READ gaplessPlaybackEnabled WRITE setGaplessPlaybackEnabled NOTIFY gaplessPlaybackEnabledChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(QString playbackMode READ playbackMode WRITE setPlaybackMode NOTIFY playbackModeChanged)
    Q_PROPERTY(int queueCount READ queueCount NOTIFY queueChanged)
    Q_PROPERTY(int queueIndex READ queueIndex NOTIFY queueChanged)
    Q_PROPERTY(QVariantList queueItems READ queueItems NOTIFY queueChanged)
    Q_PROPERTY(int currentTrackId READ currentTrackId NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentFilePath READ currentFilePath NOTIFY currentTrackChanged)

public:
    explicit PlaybackController(std::unique_ptr<IAudioPlayer> audioPlayer, QObject* parent = nullptr);

    QString title() const;
    QString artist() const;
    QString album() const;
    QUrl coverUrl() const;
    qint64 positionMs() const;
    qint64 durationMs() const;
    QString playbackState() const;
    QString errorMessage() const;
    bool isPlaying() const;
    double volume() const;
    double equalizerLowGain() const;
    double equalizerMidGain() const;
    double equalizerHighGain() const;
    bool replayGainEnabled() const;
    bool gaplessPlaybackEnabled() const;
    bool gaplessNextPrepared() const;
    bool muted() const;
    QString playbackMode() const;
    int queueCount() const;
    int queueIndex() const;
    QVariantList queueItems() const;
    int currentTrackId() const;
    QString currentFilePath() const;

    Q_INVOKABLE void togglePlayPause();
    Q_INVOKABLE void requestOpenFile();
    Q_INVOKABLE void openFile(const QString& filePath);
    Q_INVOKABLE void openStream(const QString& streamUrl, const QString& title);
    Q_INVOKABLE void playQueue(const QVariantList& items, int startIndex);
    Q_INVOKABLE void playQueueItemByIdentity(const QVariantList& items, int trackId, const QString& filePath);
    Q_INVOKABLE bool appendQueueItem(const QVariantMap& item);
    Q_INVOKABLE int appendQueueItems(const QVariantList& items);
    Q_INVOKABLE void playQueueIndex(int index);
    Q_INVOKABLE void playQueueToken(qint64 queueToken);
    Q_INVOKABLE void removeQueueItem(int index);
    Q_INVOKABLE void moveQueueItem(int index, int direction);
    Q_INVOKABLE void clearQueue();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void next();
    Q_INVOKABLE void seek(qint64 positionMs);
    Q_INVOKABLE void setVolume(double volume);
    Q_INVOKABLE void setEqualizerLowGain(double gainDb);
    Q_INVOKABLE void setEqualizerMidGain(double gainDb);
    Q_INVOKABLE void setEqualizerHighGain(double gainDb);
    Q_INVOKABLE void resetEqualizer();
    Q_INVOKABLE void setReplayGainEnabled(bool enabled);
    Q_INVOKABLE void setGaplessPlaybackEnabled(bool enabled);
    Q_INVOKABLE void setMuted(bool muted);
    Q_INVOKABLE void toggleMuted();
    Q_INVOKABLE void setPlaybackMode(const QString& playbackMode);
    Q_INVOKABLE void cyclePlaybackMode();
    Q_INVOKABLE void updateCurrentTrackDisplay(const QString& title, const QString& artist, const QString& album);

signals:
    void currentTrackChanged();
    void positionChanged();
    void durationChanged();
    void playbackStateChanged();
    void errorMessageChanged();
    void volumeChanged();
    void equalizerChanged();
    void replayGainEnabledChanged();
    void gaplessPlaybackEnabledChanged();
    void mutedChanged();
    void playbackModeChanged();
    void queueChanged();
    void trackPlaybackStarted(int trackId);

private:
    QueueItem queueItemFromMap(const QVariantMap& item, bool* ok) const;
    QueueItem assignQueueToken(QueueItem item);
    int indexOfQueueToken(qint64 queueToken) const;
    bool loadQueueItem(const QueueItem& item, bool autoplay, qint64 restorePositionMs = 0);
    void playQueueItem(const QueueItem& item);
    void clearCurrentTrack();
    void syncFromPlayer();
    bool applyPendingRestorePosition(qint64 playerPosition);
    void handleTrackEnded();
    bool moveToRandomQueueItem();
    void applyEffectiveVolume();
    void applyEqualizer();
    bool nextGaplessQueueItem(QueueItem* item, int* index) const;
    void prepareGaplessNextItem();
    void clearGaplessNextItem();
    void finalizeGaplessTransition();
    void persistQueue() const;
    void persistPlaybackPosition(bool force = false);
    void restoreQueue();
    void setPlaybackState(const QString& playbackState);
    void setErrorMessage(const QString& errorMessage);

    std::unique_ptr<IAudioPlayer> m_audioPlayer;
    QueueService m_queue;
    QTimer m_syncTimer;
    QElapsedTimer m_positionPersistTimer;
    QString m_title;
    QString m_artist;
    QString m_album;
    QUrl m_coverUrl;
    QString m_currentFilePath;
    int m_currentTrackId = 0;
    qint64 m_positionMs = 0;
    qint64 m_lastPersistedPositionMs = -1;
    qint64 m_pendingRestorePositionMs = 0;
    QString m_pendingRestoreFilePath;
    qint64 m_durationMs = 0;
    QString m_playbackState = QStringLiteral("idle");
    QString m_errorMessage;
    QString m_localPlaybackError;
    bool m_isPlaying = false;
    bool m_currentItemReachedPlaying = false;
    bool m_currentTrackPlaybackReported = false;
    double m_volume = 0.8;
    double m_equalizerLowGain = 0.0;
    double m_equalizerMidGain = 0.0;
    double m_equalizerHighGain = 0.0;
    bool m_replayGainEnabled = false;
    bool m_gaplessPlaybackEnabled = true;
    bool m_gaplessPreparedValid = false;
    int m_gaplessPreparedIndex = -1;
    QueueItem m_gaplessPreparedItem;
    std::atomic_bool m_gaplessTransitionPending = false;
    bool m_muted = false;
    QString m_playbackMode = QStringLiteral("sequential");
    qint64 m_nextQueueToken = 1;
};

} // namespace opusora
