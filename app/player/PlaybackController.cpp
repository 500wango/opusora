#include "player/PlaybackController.hpp"

#include "core/TextEncoding.hpp"
#include "player/IAudioPlayer.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QRandomGenerator>
#include <QSettings>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace opusora {

namespace {

QString stateToString(PlaybackState state)
{
    switch (state) {
    case PlaybackState::Idle:
        return QStringLiteral("idle");
    case PlaybackState::Loading:
        return QStringLiteral("loading");
    case PlaybackState::Playing:
        return QStringLiteral("playing");
    case PlaybackState::Paused:
        return QStringLiteral("paused");
    case PlaybackState::Stopped:
        return QStringLiteral("stopped");
    case PlaybackState::Buffering:
        return QStringLiteral("buffering");
    case PlaybackState::Error:
        return QStringLiteral("error");
    }

    return QStringLiteral("idle");
}

bool isSupportedPlaybackMode(const QString& playbackMode)
{
    return playbackMode == QStringLiteral("sequential")
        || playbackMode == QStringLiteral("repeatOne")
        || playbackMode == QStringLiteral("repeatAll")
        || playbackMode == QStringLiteral("shuffle");
}

QString normalizePlaybackMode(const QString& playbackMode)
{
    return isSupportedPlaybackMode(playbackMode) ? playbackMode : QStringLiteral("sequential");
}

QString nextPlaybackMode(const QString& playbackMode)
{
    if (playbackMode == QStringLiteral("sequential")) {
        return QStringLiteral("repeatOne");
    }
    if (playbackMode == QStringLiteral("repeatOne")) {
        return QStringLiteral("repeatAll");
    }
    if (playbackMode == QStringLiteral("repeatAll")) {
        return QStringLiteral("shuffle");
    }
    return QStringLiteral("sequential");
}

QString normalizePlaybackFilePath(const QString& filePath)
{
    const QString trimmed = filePath.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QFileInfo info(trimmed);
    return info.isAbsolute() ? QDir::cleanPath(trimmed) : info.absoluteFilePath();
}

QString titleFromFilePath(const QString& filePath)
{
    const QString title = QFileInfo(filePath).completeBaseName();
    return title.isEmpty() ? filePath : title;
}

QString repairedDisplayText(const QString& text)
{
    return repairMojibake(text).trimmed();
}

QString repairedTitle(const QString& filePath, const QString& title)
{
    const QString repaired = repairedDisplayText(title);
    return repaired.isEmpty() || hasLikelyEncodingDamage(repaired)
        ? titleFromFilePath(filePath)
        : repaired;
}

QString localAudioFileError(const QString& filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists()) {
        return QStringLiteral("Audio file was not found");
    }
    if (!info.isFile()) {
        return QStringLiteral("Audio path is not a regular file");
    }
    if (info.size() <= 0) {
        return QStringLiteral("Audio file is empty or unreadable");
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QStringLiteral("Audio file is empty or unreadable");
    }

    const QByteArray header = file.read(4096);
    if (header.isEmpty()) {
        return QStringLiteral("Audio file is empty or unreadable");
    }

    const auto firstNonZero = std::find_if(header.cbegin(), header.cend(), [](char byte) {
        return byte != '\0';
    });
    if (firstNonZero == header.cend()) {
        return QStringLiteral("Audio file is not recognizable or appears to be corrupted");
    }

    return {};
}

} // namespace

PlaybackController::PlaybackController(std::unique_ptr<IAudioPlayer> audioPlayer, QObject* parent)
    : QObject(parent)
    , m_audioPlayer(std::move(audioPlayer))
{
    QSettings settings;
    m_playbackMode = normalizePlaybackMode(settings.value(QStringLiteral("playback/playback_mode"), m_playbackMode).toString());
    m_muted = settings.value(QStringLiteral("playback/muted"), false).toBool();
    m_equalizerLowGain = settings.value(QStringLiteral("equalizer/low_gain"), 0.0).toDouble();
    m_equalizerMidGain = settings.value(QStringLiteral("equalizer/mid_gain"), 0.0).toDouble();
    m_equalizerHighGain = settings.value(QStringLiteral("equalizer/high_gain"), 0.0).toDouble();
    m_replayGainEnabled = settings.value(QStringLiteral("playback/replaygain_enabled"), false).toBool();
    m_gaplessPlaybackEnabled = settings.value(QStringLiteral("playback/gapless_enabled"), true).toBool();
    if (m_audioPlayer) {
        m_audioPlayer->setGaplessTransitionCallback([this]() {
            m_gaplessTransitionPending.store(true);
            QMetaObject::invokeMethod(this, [this]() {
                finalizeGaplessTransition();
            }, Qt::QueuedConnection);
        });
    }
    applyEqualizer();
    m_positionPersistTimer.start();
    if (settings.value(QStringLiteral("restore_queue_on_startup"), true).toBool()) {
        restoreQueue();
    }

    m_syncTimer.setInterval(250);
    m_syncTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_syncTimer, &QTimer::timeout, this, &PlaybackController::syncFromPlayer);
    m_syncTimer.start();
}

QString PlaybackController::title() const
{
    return m_title;
}

QString PlaybackController::artist() const
{
    return m_artist;
}

QString PlaybackController::album() const
{
    return m_album;
}

QUrl PlaybackController::coverUrl() const
{
    return m_coverUrl;
}

qint64 PlaybackController::positionMs() const
{
    return m_positionMs;
}

qint64 PlaybackController::durationMs() const
{
    return m_durationMs;
}

QString PlaybackController::playbackState() const
{
    return m_playbackState;
}

QString PlaybackController::errorMessage() const
{
    return m_errorMessage;
}

bool PlaybackController::isPlaying() const
{
    return m_isPlaying;
}

double PlaybackController::volume() const
{
    return m_volume;
}

double PlaybackController::equalizerLowGain() const
{
    return m_equalizerLowGain;
}

double PlaybackController::equalizerMidGain() const
{
    return m_equalizerMidGain;
}

double PlaybackController::equalizerHighGain() const
{
    return m_equalizerHighGain;
}

bool PlaybackController::replayGainEnabled() const
{
    return m_replayGainEnabled;
}

bool PlaybackController::gaplessPlaybackEnabled() const
{
    return m_gaplessPlaybackEnabled;
}

bool PlaybackController::gaplessNextPrepared() const
{
    return m_gaplessPreparedValid;
}

bool PlaybackController::muted() const
{
    return m_muted;
}

QString PlaybackController::playbackMode() const
{
    return m_playbackMode;
}

int PlaybackController::queueCount() const
{
    return m_queue.count();
}

int PlaybackController::queueIndex() const
{
    return m_queue.currentIndex();
}

QVariantList PlaybackController::queueItems() const
{
    QVariantList items;
    const QList<QueueItem>& queueItems = m_queue.items();
    items.reserve(queueItems.size());

    for (int i = 0; i < queueItems.size(); ++i) {
        const QueueItem& queueItem = queueItems.at(i);
        QVariantMap item;
        item.insert(QStringLiteral("index"), i);
        item.insert(QStringLiteral("queueToken"), QVariant::fromValue(queueItem.queueToken));
        item.insert(QStringLiteral("trackId"), queueItem.trackId);
        item.insert(QStringLiteral("title"), queueItem.title);
        item.insert(QStringLiteral("artist"), queueItem.artist);
        item.insert(QStringLiteral("album"), queueItem.album);
        item.insert(QStringLiteral("filePath"), queueItem.filePath);
        item.insert(QStringLiteral("coverUrl"), queueItem.coverUrl);
        item.insert(QStringLiteral("hasReplayGainTrackGain"), queueItem.hasReplayGainTrackGain);
        item.insert(QStringLiteral("hasReplayGainAlbumGain"), queueItem.hasReplayGainAlbumGain);
        if (queueItem.hasReplayGainTrackGain) {
            item.insert(QStringLiteral("replayGainTrackGainDb"), queueItem.replayGainTrackGainDb);
        }
        if (queueItem.hasReplayGainAlbumGain) {
            item.insert(QStringLiteral("replayGainAlbumGainDb"), queueItem.replayGainAlbumGainDb);
        }
        item.insert(QStringLiteral("current"), i == m_queue.currentIndex());
        items.append(item);
    }

    return items;
}

int PlaybackController::currentTrackId() const
{
    return m_currentTrackId;
}

QString PlaybackController::currentFilePath() const
{
    return m_currentFilePath;
}

void PlaybackController::togglePlayPause()
{
    if (m_currentFilePath.isEmpty()) {
        requestOpenFile();
        return;
    }

    if (!m_audioPlayer) {
        return;
    }

    const bool wasPlaying = m_isPlaying;
    if (m_isPlaying) {
        m_audioPlayer->pause();
    } else {
        m_audioPlayer->play();
    }
    syncFromPlayer();
    if (wasPlaying) {
        persistPlaybackPosition(true);
    }
}

void PlaybackController::requestOpenFile()
{
    const QString filter = tr("Audio Files (*.mp3 *.flac *.wav *.m4a *.aac *.ogg *.opus)");
    const QString filePath = QFileDialog::getOpenFileName(
        nullptr,
        tr("Open Audio File"),
        QDir::homePath(),
        filter);

    if (!filePath.isEmpty()) {
        openFile(filePath);
    }
}

void PlaybackController::openFile(const QString& filePath)
{
    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        qWarning() << "Open file requested for invalid path" << filePath;
        return;
    }

    m_queue.setItems({
        QueueItem {
            m_nextQueueToken++,
            0,
            info.absoluteFilePath(),
            info.completeBaseName(),
            {},
            {},
            {},
        },
    },
        0);
    emit queueChanged();
    persistQueue();
    playQueueItem(*m_queue.current());
    persistPlaybackPosition(true);
}

void PlaybackController::openStream(const QString& streamUrl, const QString& title)
{
    const QUrl url(streamUrl.trimmed());
    if (!url.isValid() || (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https"))) {
        qWarning() << "Open stream requested for invalid URL" << streamUrl;
        return;
    }

    if (m_audioPlayer) {
        m_audioPlayer->stop();
    }
    m_queue.clear();
    emit queueChanged();
    persistQueue();
    clearGaplessNextItem();

    m_currentFilePath = url.toString();
    m_currentTrackId = 0;
    m_title = title.trimmed().isEmpty() ? url.host() : title.trimmed();
    m_artist = QStringLiteral("Radio");
    m_album = {};
    m_coverUrl = QUrl();
    m_positionMs = 0;
    m_durationMs = 0;
    setErrorMessage({});

    emit currentTrackChanged();
    emit positionChanged();
    emit durationChanged();

    if (m_audioPlayer) {
        m_audioPlayer->load(url.toString().toStdString());
        applyEffectiveVolume();
        m_audioPlayer->play();
        qInfo() << "Radio stream selected for playback" << url.toString();
    }
    syncFromPlayer();
}

void PlaybackController::playQueue(const QVariantList& items, int startIndex)
{
    if (items.isEmpty()) {
        return;
    }

    QList<QueueItem> queueItems;
    int queueStartIndex = -1;
    const int lastItemIndex = static_cast<int>(items.size()) - 1;
    const int boundedStartIndex = std::max(0, std::min(startIndex, lastItemIndex));

    bool selectedOk = false;
    queueItemFromMap(items.at(boundedStartIndex).toMap(), &selectedOk);
    if (!selectedOk) {
        qWarning() << "Play queue requested for invalid selected item at index" << boundedStartIndex;
        return;
    }

    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        bool ok = false;
        QueueItem queueItem = queueItemFromMap(items.at(i).toMap(), &ok);
        if (!ok) {
            continue;
        }

        if (i == boundedStartIndex) {
            queueStartIndex = queueItems.size();
        }

        queueItems.append(assignQueueToken(std::move(queueItem)));
    }

    if (queueItems.isEmpty()) {
        qWarning() << "Play queue requested with no valid local files";
        return;
    }

    if (queueStartIndex < 0) {
        queueStartIndex = 0;
    }

    m_queue.setItems(std::move(queueItems), queueStartIndex);
    emit queueChanged();
    persistQueue();

    if (const QueueItem* item = m_queue.current()) {
        playQueueItem(*item);
        persistPlaybackPosition(true);
    }
}

void PlaybackController::playQueueItemByIdentity(const QVariantList& items, int trackId, const QString& filePath)
{
    if (items.isEmpty()) {
        return;
    }

    const QString normalizedFilePath = normalizePlaybackFilePath(filePath);
    int startIndex = -1;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const QVariantMap item = items.at(i).toMap();
        const int itemTrackId = item.value(QStringLiteral("trackId")).toInt();
        const QString itemFilePath = normalizePlaybackFilePath(item.value(QStringLiteral("filePath")).toString());
        const bool trackMatches = trackId > 0 && itemTrackId == trackId;
        const bool pathMatches = !normalizedFilePath.isEmpty() && itemFilePath == normalizedFilePath;
        if ((trackMatches && (normalizedFilePath.isEmpty() || pathMatches))
            || (trackId <= 0 && pathMatches)) {
            startIndex = i;
            break;
        }
    }

    if (startIndex < 0) {
        qWarning() << "Play queue identity not found" << trackId << normalizedFilePath;
        return;
    }

    playQueue(items, startIndex);
}

bool PlaybackController::appendQueueItem(const QVariantMap& item)
{
    return appendQueueItems(QVariantList { item }) == 1;
}

int PlaybackController::appendQueueItems(const QVariantList& items)
{
    bool ok = false;
    QList<QueueItem> queueItems;
    queueItems.reserve(items.size());

    for (const QVariant& itemVariant : items) {
        QueueItem queueItem = queueItemFromMap(itemVariant.toMap(), &ok);
        if (ok) {
            queueItems.append(assignQueueToken(std::move(queueItem)));
        }
    }

    if (queueItems.isEmpty()) {
        return 0;
    }

    const bool wasEmpty = m_queue.empty();
    const int appendedCount = queueItems.size();
    for (QueueItem& queueItem : queueItems) {
        m_queue.append(std::move(queueItem));
    }
    emit queueChanged();
    persistQueue();

    if (wasEmpty) {
        if (const QueueItem* currentItem = m_queue.current()) {
            playQueueItem(*currentItem);
            persistPlaybackPosition(true);
        }
    } else {
        qInfo() << "Queued audio files" << appendedCount
                << "queue count" << m_queue.count();
        prepareGaplessNextItem();
    }

    return appendedCount;
}

void PlaybackController::playQueueIndex(int index)
{
    if (!m_queue.setCurrentIndex(index)) {
        return;
    }

    emit queueChanged();
    persistQueue();
    if (const QueueItem* item = m_queue.current()) {
        playQueueItem(*item);
        persistPlaybackPosition(true);
    }
}

void PlaybackController::playQueueToken(qint64 queueToken)
{
    const int index = indexOfQueueToken(queueToken);
    if (index < 0) {
        qWarning() << "Play queue token not found" << queueToken;
        return;
    }

    playQueueIndex(index);
}

void PlaybackController::removeQueueItem(int index)
{
    const int previousIndex = m_queue.currentIndex();
    const bool removingCurrent = index == previousIndex;
    if (!m_queue.removeAt(index)) {
        return;
    }

    emit queueChanged();
    persistQueue();

    if (m_queue.empty()) {
        if (m_audioPlayer) {
            m_audioPlayer->stop();
        }
        clearCurrentTrack();
        syncFromPlayer();
        return;
    }

    if (removingCurrent) {
        if (const QueueItem* item = m_queue.current()) {
            playQueueItem(*item);
            persistPlaybackPosition(true);
        }
    } else {
        prepareGaplessNextItem();
    }
}

void PlaybackController::moveQueueItem(int index, int direction)
{
    if (direction == 0) {
        return;
    }

    const int target = index + (direction < 0 ? -1 : 1);
    if (!m_queue.moveAt(index, target)) {
        return;
    }

    emit queueChanged();
    persistQueue();
    prepareGaplessNextItem();
}

void PlaybackController::clearQueue()
{
    m_queue.clear();
    if (m_audioPlayer) {
        m_audioPlayer->stop();
    }
    emit queueChanged();
    persistQueue();
    clearCurrentTrack();
    syncFromPlayer();
}

void PlaybackController::stop()
{
    if (!m_audioPlayer) {
        return;
    }

    m_audioPlayer->stop();
    m_positionMs = 0;
    clearGaplessNextItem();
    emit positionChanged();
    syncFromPlayer();
    persistPlaybackPosition(true);
}

void PlaybackController::previous()
{
    if (m_queue.hasPrevious()) {
        if (const QueueItem* item = m_queue.movePrevious()) {
            emit queueChanged();
            persistQueue();
            playQueueItem(*item);
            persistPlaybackPosition(true);
        }
        return;
    }

    if (m_playbackMode == QStringLiteral("repeatAll") && m_queue.count() > 1) {
        if (m_queue.setCurrentIndex(m_queue.count() - 1)) {
            emit queueChanged();
            persistQueue();
            if (const QueueItem* item = m_queue.current()) {
                playQueueItem(*item);
                persistPlaybackPosition(true);
            }
        }
        return;
    }

    if (m_audioPlayer && !m_currentFilePath.isEmpty()) {
        m_audioPlayer->seekMs(0);
        syncFromPlayer();
    }
}

void PlaybackController::next()
{
    if (m_queue.empty()) {
        return;
    }

    if (m_playbackMode == QStringLiteral("shuffle")) {
        if (moveToRandomQueueItem()) {
            emit queueChanged();
            persistQueue();
            if (const QueueItem* item = m_queue.current()) {
                playQueueItem(*item);
                persistPlaybackPosition(true);
            }
        }
        return;
    }

    if (m_queue.hasNext()) {
        if (const QueueItem* item = m_queue.moveNext()) {
            emit queueChanged();
            persistQueue();
            playQueueItem(*item);
            persistPlaybackPosition(true);
        }
        return;
    }

    if (m_playbackMode == QStringLiteral("repeatAll") && m_queue.count() > 0) {
        if (m_queue.setCurrentIndex(0)) {
            emit queueChanged();
            persistQueue();
            if (const QueueItem* item = m_queue.current()) {
                playQueueItem(*item);
                persistPlaybackPosition(true);
            }
        }
        return;
    }

    qInfo() << "Next track requested at end of queue";
}

void PlaybackController::seek(qint64 positionMs)
{
    if (!m_audioPlayer) {
        return;
    }

    m_pendingRestorePositionMs = 0;
    m_pendingRestoreFilePath.clear();
    const qint64 bounded = std::max<qint64>(0, std::min(positionMs, m_durationMs));
    m_audioPlayer->seekMs(bounded);
    syncFromPlayer();
    persistPlaybackPosition(true);
}

void PlaybackController::setVolume(double volume)
{
    const double bounded = std::max(0.0, std::min(volume, 1.0));
    if (qFuzzyCompare(m_volume, bounded)) {
        return;
    }

    m_volume = bounded;
    applyEffectiveVolume();
    emit volumeChanged();
}

void PlaybackController::setEqualizerLowGain(double gainDb)
{
    const double bounded = std::max(-12.0, std::min(gainDb, 12.0));
    if (qFuzzyCompare(m_equalizerLowGain, bounded)) {
        return;
    }

    m_equalizerLowGain = bounded;
    QSettings().setValue(QStringLiteral("equalizer/low_gain"), m_equalizerLowGain);
    applyEqualizer();
    emit equalizerChanged();
}

void PlaybackController::setEqualizerMidGain(double gainDb)
{
    const double bounded = std::max(-12.0, std::min(gainDb, 12.0));
    if (qFuzzyCompare(m_equalizerMidGain, bounded)) {
        return;
    }

    m_equalizerMidGain = bounded;
    QSettings().setValue(QStringLiteral("equalizer/mid_gain"), m_equalizerMidGain);
    applyEqualizer();
    emit equalizerChanged();
}

void PlaybackController::setEqualizerHighGain(double gainDb)
{
    const double bounded = std::max(-12.0, std::min(gainDb, 12.0));
    if (qFuzzyCompare(m_equalizerHighGain, bounded)) {
        return;
    }

    m_equalizerHighGain = bounded;
    QSettings().setValue(QStringLiteral("equalizer/high_gain"), m_equalizerHighGain);
    applyEqualizer();
    emit equalizerChanged();
}

void PlaybackController::resetEqualizer()
{
    bool changed = false;
    if (!qFuzzyCompare(m_equalizerLowGain, 0.0)) {
        m_equalizerLowGain = 0.0;
        changed = true;
    }
    if (!qFuzzyCompare(m_equalizerMidGain, 0.0)) {
        m_equalizerMidGain = 0.0;
        changed = true;
    }
    if (!qFuzzyCompare(m_equalizerHighGain, 0.0)) {
        m_equalizerHighGain = 0.0;
        changed = true;
    }
    if (!changed) {
        return;
    }

    QSettings settings;
    settings.setValue(QStringLiteral("equalizer/low_gain"), m_equalizerLowGain);
    settings.setValue(QStringLiteral("equalizer/mid_gain"), m_equalizerMidGain);
    settings.setValue(QStringLiteral("equalizer/high_gain"), m_equalizerHighGain);
    applyEqualizer();
    emit equalizerChanged();
}

void PlaybackController::setReplayGainEnabled(bool enabled)
{
    if (m_replayGainEnabled == enabled) {
        return;
    }

    m_replayGainEnabled = enabled;
    QSettings().setValue(QStringLiteral("playback/replaygain_enabled"), m_replayGainEnabled);
    applyEffectiveVolume();
    emit replayGainEnabledChanged();
}

void PlaybackController::setGaplessPlaybackEnabled(bool enabled)
{
    if (m_gaplessPlaybackEnabled == enabled) {
        return;
    }

    m_gaplessPlaybackEnabled = enabled;
    QSettings().setValue(QStringLiteral("playback/gapless_enabled"), m_gaplessPlaybackEnabled);
    if (m_gaplessPlaybackEnabled) {
        prepareGaplessNextItem();
    } else {
        clearGaplessNextItem();
    }
    emit gaplessPlaybackEnabledChanged();
}

void PlaybackController::setMuted(bool muted)
{
    if (m_muted == muted) {
        return;
    }

    m_muted = muted;
    applyEffectiveVolume();
    QSettings().setValue(QStringLiteral("playback/muted"), m_muted);
    emit mutedChanged();
}

void PlaybackController::toggleMuted()
{
    setMuted(!m_muted);
}

void PlaybackController::setPlaybackMode(const QString& playbackMode)
{
    const QString normalized = normalizePlaybackMode(playbackMode);
    if (m_playbackMode == normalized) {
        return;
    }

    m_playbackMode = normalized;
    QSettings().setValue(QStringLiteral("playback/playback_mode"), m_playbackMode);
    prepareGaplessNextItem();
    emit playbackModeChanged();
}

void PlaybackController::cyclePlaybackMode()
{
    setPlaybackMode(nextPlaybackMode(m_playbackMode));
}

void PlaybackController::updateCurrentTrackDisplay(const QString& title, const QString& artist, const QString& album)
{
    const QString trimmedTitle = repairedDisplayText(title);
    if (trimmedTitle.isEmpty() || m_currentFilePath.isEmpty()) {
        return;
    }

    const QString trimmedArtist = repairedDisplayText(artist);
    const QString trimmedAlbum = repairedDisplayText(album);
    m_title = trimmedTitle;
    m_artist = trimmedArtist;
    m_album = trimmedAlbum;
    m_queue.updateCurrentDisplay(trimmedTitle, trimmedArtist, trimmedAlbum);
    emit currentTrackChanged();
    emit queueChanged();
    persistQueue();
}

QueueItem PlaybackController::queueItemFromMap(const QVariantMap& item, bool* ok) const
{
    if (ok) {
        *ok = false;
    }

    const QString filePath = normalizePlaybackFilePath(item.value(QStringLiteral("filePath")).toString());
    if (filePath.isEmpty()) {
        return {};
    }

    const QString title = repairedDisplayText(item.value(QStringLiteral("title")).toString());
    bool trackGainOk = false;
    const QVariant trackGainValue = item.value(QStringLiteral("replayGainTrackGainDb"));
    const double trackGainDb = trackGainValue.toDouble(&trackGainOk);
    const bool hasTrackGain = trackGainOk
        && (item.value(QStringLiteral("hasReplayGainTrackGain")).toBool()
            || item.contains(QStringLiteral("replayGainTrackGainDb")));

    bool albumGainOk = false;
    const QVariant albumGainValue = item.value(QStringLiteral("replayGainAlbumGainDb"));
    const double albumGainDb = albumGainValue.toDouble(&albumGainOk);
    const bool hasAlbumGain = albumGainOk
        && (item.value(QStringLiteral("hasReplayGainAlbumGain")).toBool()
            || item.contains(QStringLiteral("replayGainAlbumGainDb")));

    if (ok) {
        *ok = true;
    }

    return QueueItem {
        item.value(QStringLiteral("queueToken")).toLongLong(),
        item.value(QStringLiteral("trackId")).toInt(),
        filePath,
        title.isEmpty() || hasLikelyEncodingDamage(title) ? titleFromFilePath(filePath) : title,
        repairedDisplayText(item.value(QStringLiteral("artist")).toString()),
        repairedDisplayText(item.value(QStringLiteral("album")).toString()),
        item.value(QStringLiteral("coverUrl")).toString().trimmed(),
        hasTrackGain ? trackGainDb : 0.0,
        hasAlbumGain ? albumGainDb : 0.0,
        hasTrackGain,
        hasAlbumGain,
    };
}

QueueItem PlaybackController::assignQueueToken(QueueItem item)
{
    if (item.queueToken <= 0) {
        item.queueToken = m_nextQueueToken++;
    } else {
        m_nextQueueToken = std::max(m_nextQueueToken, item.queueToken + 1);
    }
    return item;
}

int PlaybackController::indexOfQueueToken(qint64 queueToken) const
{
    if (queueToken <= 0) {
        return -1;
    }

    const QList<QueueItem>& items = m_queue.items();
    for (int i = 0; i < items.size(); ++i) {
        if (items.at(i).queueToken == queueToken) {
            return i;
        }
    }
    return -1;
}

bool PlaybackController::loadQueueItem(const QueueItem& item, bool autoplay, qint64 restorePositionMs)
{
    const QString filePath = normalizePlaybackFilePath(item.filePath);
    if (filePath.isEmpty()) {
        qWarning() << "Queue item points to invalid path" << item.filePath;
        return false;
    }

    const QString localError = localAudioFileError(filePath);
    m_localPlaybackError.clear();
    m_currentItemReachedPlaying = false;
    m_currentTrackPlaybackReported = false;
    m_currentFilePath = filePath;
    m_currentTrackId = item.trackId;
    m_title = repairedTitle(filePath, item.title);
    m_artist = repairedDisplayText(item.artist);
    m_album = repairedDisplayText(item.album);
    m_coverUrl = item.coverUrl.trimmed().isEmpty() ? QUrl() : QUrl(item.coverUrl.trimmed());
    m_positionMs = std::max<qint64>(0, restorePositionMs);
    m_durationMs = 0;
    if (m_positionMs > 0) {
        m_pendingRestorePositionMs = m_positionMs;
        m_pendingRestoreFilePath = m_currentFilePath;
    } else {
        m_pendingRestorePositionMs = 0;
        m_pendingRestoreFilePath.clear();
    }

    emit currentTrackChanged();
    emit positionChanged();
    emit durationChanged();
    setPlaybackState(QStringLiteral("loading"));
    setErrorMessage({});

    if (!localError.isEmpty()) {
        if (m_audioPlayer) {
            m_audioPlayer->stop();
        }
        clearGaplessNextItem();
        m_localPlaybackError = localError;
        setPlaybackState(QStringLiteral("error"));
        setErrorMessage(localError);
        qWarning() << "Audio file cannot be played" << localError << filePath;
        return false;
    }

    if (m_audioPlayer) {
        const QUrl uri = QUrl::fromLocalFile(m_currentFilePath);
        clearGaplessNextItem();
        m_audioPlayer->load(uri.toString().toStdString());
        applyEffectiveVolume();
        applyPendingRestorePosition(0);
        if (autoplay) {
            m_audioPlayer->play();
            prepareGaplessNextItem();
        }
        qInfo() << "Audio file selected for playback" << m_currentFilePath
                << "queue index" << m_queue.currentIndex() << "of" << m_queue.count();
    }
    syncFromPlayer();
    return true;
}

void PlaybackController::playQueueItem(const QueueItem& item)
{
    loadQueueItem(item, true);
}

void PlaybackController::clearCurrentTrack()
{
    clearGaplessNextItem();
    m_localPlaybackError.clear();
    m_currentTrackPlaybackReported = false;
    m_currentFilePath.clear();
    m_currentTrackId = 0;
    m_title.clear();
    m_artist.clear();
    m_album.clear();
    m_coverUrl = QUrl();
    m_positionMs = 0;
    m_durationMs = 0;
    setErrorMessage({});

    emit currentTrackChanged();
    emit positionChanged();
    emit durationChanged();

    QSettings settings;
    settings.setValue(QStringLiteral("playback/position_ms"), 0);
    settings.remove(QStringLiteral("playback/position_file_path"));
    m_lastPersistedPositionMs = 0;
}

void PlaybackController::syncFromPlayer()
{
    if (!m_audioPlayer) {
        return;
    }

    if (!m_localPlaybackError.isEmpty()) {
        setPlaybackState(QStringLiteral("error"));
        setErrorMessage(m_localPlaybackError);
        return;
    }

    const qint64 position = m_audioPlayer->positionMs();
    if (applyPendingRestorePosition(position)) {
        // Keep the restored position visible until the backend accepts the seek.
    } else if (m_positionMs != position) {
        m_positionMs = position;
        emit positionChanged();
        persistPlaybackPosition(false);
    }

    const qint64 duration = m_audioPlayer->durationMs();
    if (m_durationMs != duration) {
        m_durationMs = duration;
        emit durationChanged();
    }

    const QString previousState = m_playbackState;
    const QString nextState = stateToString(m_audioPlayer->state());
    setPlaybackState(nextState);
    if (nextState == QStringLiteral("playing")) {
        if (!m_currentItemReachedPlaying) {
            m_currentItemReachedPlaying = true;
        }
        if (!m_currentTrackPlaybackReported && m_currentTrackId > 0) {
            m_currentTrackPlaybackReported = true;
            emit trackPlaybackStarted(m_currentTrackId);
        }
    }

    const QString nextError = QString::fromStdString(m_audioPlayer->lastError());
    setErrorMessage(nextError);

    if (previousState == QStringLiteral("playing") && nextState == QStringLiteral("stopped")) {
        if (m_currentItemReachedPlaying) {
            handleTrackEnded();
        } else {
            qInfo() << "Ignored stale stopped transition while switching tracks" << m_currentFilePath;
        }
    }
}

bool PlaybackController::applyPendingRestorePosition(qint64 playerPosition)
{
    if (!m_audioPlayer || m_pendingRestorePositionMs <= 0 || m_pendingRestoreFilePath != m_currentFilePath) {
        return false;
    }

    qint64 target = m_pendingRestorePositionMs;
    if (m_durationMs > 0) {
        target = std::min(target, m_durationMs);
    }

    if (playerPosition + 1000 >= target) {
        m_pendingRestorePositionMs = 0;
        m_pendingRestoreFilePath.clear();
        return false;
    }

    m_audioPlayer->seekMs(target);
    if (m_positionMs != target) {
        m_positionMs = target;
        emit positionChanged();
    }
    return true;
}

void PlaybackController::handleTrackEnded()
{
    if (m_gaplessTransitionPending.load()) {
        return;
    }

    if (m_queue.empty()) {
        return;
    }

    if (m_playbackMode == QStringLiteral("repeatOne")) {
        if (const QueueItem* item = m_queue.current()) {
            playQueueItem(*item);
            persistPlaybackPosition(true);
        }
        return;
    }

    if (m_playbackMode == QStringLiteral("shuffle")) {
        if (moveToRandomQueueItem()) {
            emit queueChanged();
            persistQueue();
            if (const QueueItem* item = m_queue.current()) {
                playQueueItem(*item);
                persistPlaybackPosition(true);
            }
        }
        return;
    }

    if (m_queue.hasNext()) {
        if (const QueueItem* item = m_queue.moveNext()) {
            emit queueChanged();
            persistQueue();
            playQueueItem(*item);
            persistPlaybackPosition(true);
        }
        return;
    }

    if (m_playbackMode == QStringLiteral("repeatAll") && m_queue.setCurrentIndex(0)) {
        emit queueChanged();
        persistQueue();
        if (const QueueItem* item = m_queue.current()) {
            playQueueItem(*item);
            persistPlaybackPosition(true);
        }
    }
}

bool PlaybackController::moveToRandomQueueItem()
{
    const int count = m_queue.count();
    if (count <= 0) {
        return false;
    }
    if (count == 1) {
        return m_queue.setCurrentIndex(0);
    }

    const int currentIndex = m_queue.currentIndex();
    int nextIndex = QRandomGenerator::global()->bounded(count - 1);
    if (nextIndex >= currentIndex) {
        ++nextIndex;
    }

    return m_queue.setCurrentIndex(nextIndex);
}

void PlaybackController::applyEffectiveVolume()
{
    if (m_audioPlayer) {
        double effectiveVolume = m_muted ? 0.0 : m_volume;
        if (!m_muted && m_replayGainEnabled) {
            if (const QueueItem* item = m_queue.current()) {
                bool hasGain = false;
                double gainDb = 0.0;
                if (item->hasReplayGainTrackGain) {
                    gainDb = item->replayGainTrackGainDb;
                    hasGain = true;
                } else if (item->hasReplayGainAlbumGain) {
                    gainDb = item->replayGainAlbumGainDb;
                    hasGain = true;
                }
                if (hasGain) {
                    effectiveVolume *= std::pow(10.0, gainDb / 20.0);
                }
            }
        }

        m_audioPlayer->setVolume(std::max(0.0, std::min(effectiveVolume, 1.0)));
    }
}

void PlaybackController::applyEqualizer()
{
    if (m_audioPlayer) {
        m_audioPlayer->setEqualizer(m_equalizerLowGain, m_equalizerMidGain, m_equalizerHighGain);
    }
}

bool PlaybackController::nextGaplessQueueItem(QueueItem* item, int* index) const
{
    if (!m_gaplessPlaybackEnabled || m_queue.empty() || m_currentFilePath.isEmpty()) {
        return false;
    }
    if (m_currentFilePath.startsWith(QStringLiteral("http://")) || m_currentFilePath.startsWith(QStringLiteral("https://"))) {
        return false;
    }

    const int currentIndex = m_queue.currentIndex();
    if (currentIndex < 0 || currentIndex >= m_queue.count()) {
        return false;
    }

    int targetIndex = -1;
    if (m_playbackMode == QStringLiteral("repeatOne")) {
        targetIndex = currentIndex;
    } else if (currentIndex < m_queue.count() - 1) {
        targetIndex = currentIndex + 1;
    } else if (m_playbackMode == QStringLiteral("repeatAll")) {
        targetIndex = 0;
    } else {
        return false;
    }

    if (targetIndex < 0 || targetIndex >= m_queue.count()) {
        return false;
    }

    QueueItem candidate = m_queue.items().at(targetIndex);
    const QString filePath = normalizePlaybackFilePath(candidate.filePath);
    if (filePath.isEmpty()) {
        return false;
    }

    if (item) {
        *item = candidate;
        item->filePath = filePath;
    }
    if (index) {
        *index = targetIndex;
    }
    return true;
}

void PlaybackController::prepareGaplessNextItem()
{
    if (!m_audioPlayer) {
        return;
    }

    QueueItem nextItem;
    int nextIndex = -1;
    if (!nextGaplessQueueItem(&nextItem, &nextIndex)) {
        clearGaplessNextItem();
        return;
    }

    m_gaplessPreparedItem = nextItem;
    m_gaplessPreparedIndex = nextIndex;
    m_gaplessPreparedValid = true;
    m_gaplessTransitionPending.store(false);
    m_audioPlayer->setGaplessNextUri(QUrl::fromLocalFile(nextItem.filePath).toString().toStdString());
}

void PlaybackController::clearGaplessNextItem()
{
    m_gaplessPreparedValid = false;
    m_gaplessPreparedIndex = -1;
    m_gaplessPreparedItem = QueueItem {};
    m_gaplessTransitionPending.store(false);
    if (m_audioPlayer) {
        m_audioPlayer->setGaplessNextUri({});
    }
}

void PlaybackController::finalizeGaplessTransition()
{
    m_gaplessTransitionPending.store(false);
    if (!m_gaplessPreparedValid || m_gaplessPreparedIndex < 0 || m_gaplessPreparedIndex >= m_queue.count()) {
        clearGaplessNextItem();
        return;
    }

    const QueueItem item = m_gaplessPreparedItem;
    const int index = m_gaplessPreparedIndex;
    const QueueItem& queueItem = m_queue.items().at(index);
    if (normalizePlaybackFilePath(queueItem.filePath) != normalizePlaybackFilePath(item.filePath)) {
        clearGaplessNextItem();
        return;
    }

    m_gaplessPreparedValid = false;
    if (!m_queue.setCurrentIndex(index)) {
        clearGaplessNextItem();
        return;
    }

    const QFileInfo info(item.filePath);
    m_currentFilePath = info.absoluteFilePath();
    m_currentTrackId = item.trackId;
    m_title = repairedTitle(m_currentFilePath, item.title);
    m_artist = repairedDisplayText(item.artist);
    m_album = repairedDisplayText(item.album);
    m_coverUrl = item.coverUrl.trimmed().isEmpty() ? QUrl() : QUrl(item.coverUrl.trimmed());
    m_positionMs = 0;
    m_durationMs = 0;
    m_pendingRestorePositionMs = 0;
    m_pendingRestoreFilePath.clear();
    m_currentItemReachedPlaying = true;
    m_currentTrackPlaybackReported = true;
    setErrorMessage({});

    applyEffectiveVolume();
    emit currentTrackChanged();
    emit positionChanged();
    emit durationChanged();
    emit queueChanged();
    persistQueue();
    persistPlaybackPosition(true);
    if (item.trackId > 0) {
        emit trackPlaybackStarted(item.trackId);
    }
    prepareGaplessNextItem();
}

void PlaybackController::persistQueue() const
{
    QJsonArray queueArray;
    for (const QueueItem& item : m_queue.items()) {
        QJsonObject object;
        object.insert(QStringLiteral("queueToken"), item.queueToken);
        object.insert(QStringLiteral("trackId"), item.trackId);
        object.insert(QStringLiteral("filePath"), item.filePath);
        object.insert(QStringLiteral("title"), item.title);
        object.insert(QStringLiteral("artist"), item.artist);
        object.insert(QStringLiteral("album"), item.album);
        object.insert(QStringLiteral("coverUrl"), item.coverUrl);
        object.insert(QStringLiteral("hasReplayGainTrackGain"), item.hasReplayGainTrackGain);
        object.insert(QStringLiteral("hasReplayGainAlbumGain"), item.hasReplayGainAlbumGain);
        if (item.hasReplayGainTrackGain) {
            object.insert(QStringLiteral("replayGainTrackGainDb"), item.replayGainTrackGainDb);
        }
        if (item.hasReplayGainAlbumGain) {
            object.insert(QStringLiteral("replayGainAlbumGainDb"), item.replayGainAlbumGainDb);
        }
        queueArray.append(object);
    }

    QSettings settings;
    settings.setValue(
        QStringLiteral("playback/queue"),
        QString::fromUtf8(QJsonDocument(queueArray).toJson(QJsonDocument::Compact)));
    settings.setValue(QStringLiteral("playback/queue_current_index"), m_queue.currentIndex());
}

void PlaybackController::persistPlaybackPosition(bool force)
{
    if (m_currentFilePath.isEmpty()) {
        return;
    }

    if (!force && m_positionPersistTimer.isValid() && m_positionPersistTimer.elapsed() < 5000) {
        return;
    }
    if (!force && m_lastPersistedPositionMs >= 0 && std::llabs(m_positionMs - m_lastPersistedPositionMs) < 3000) {
        return;
    }

    QSettings settings;
    settings.setValue(QStringLiteral("playback/position_ms"), std::max<qint64>(0, m_positionMs));
    settings.setValue(QStringLiteral("playback/position_file_path"), m_currentFilePath);
    m_lastPersistedPositionMs = m_positionMs;
    m_positionPersistTimer.restart();
}

void PlaybackController::restoreQueue()
{
    const QSettings settings;
    const QByteArray queueJson = settings.value(QStringLiteral("playback/queue")).toString().toUtf8();
    if (queueJson.isEmpty()) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(queueJson);
    if (!document.isArray()) {
        return;
    }

    QList<QueueItem> items;
    const QJsonArray queueArray = document.array();
    for (const QJsonValue& value : queueArray) {
        const QJsonObject object = value.toObject();
        const QString filePath = normalizePlaybackFilePath(object.value(QStringLiteral("filePath")).toString());
        if (filePath.isEmpty()) {
            continue;
        }

        const QString title = repairedDisplayText(object.value(QStringLiteral("title")).toString());
        const bool hasTrackGain = object.value(QStringLiteral("hasReplayGainTrackGain")).toBool()
            && object.contains(QStringLiteral("replayGainTrackGainDb"));
        const bool hasAlbumGain = object.value(QStringLiteral("hasReplayGainAlbumGain")).toBool()
            && object.contains(QStringLiteral("replayGainAlbumGainDb"));
        QueueItem item {
            object.value(QStringLiteral("queueToken")).toVariant().toLongLong(),
            object.value(QStringLiteral("trackId")).toInt(),
            filePath,
            title.isEmpty() || hasLikelyEncodingDamage(title) ? titleFromFilePath(filePath) : title,
            repairedDisplayText(object.value(QStringLiteral("artist")).toString()),
            repairedDisplayText(object.value(QStringLiteral("album")).toString()),
            object.value(QStringLiteral("coverUrl")).toString().trimmed(),
            hasTrackGain ? object.value(QStringLiteral("replayGainTrackGainDb")).toDouble() : 0.0,
            hasAlbumGain ? object.value(QStringLiteral("replayGainAlbumGainDb")).toDouble() : 0.0,
            hasTrackGain,
            hasAlbumGain,
        };
        items.append(assignQueueToken(std::move(item)));
    }

    if (items.isEmpty()) {
        return;
    }

    const qint64 restoredPositionMs = settings.value(QStringLiteral("playback/position_ms"), 0).toLongLong();
    const QString restoredPositionFile = settings.value(QStringLiteral("playback/position_file_path")).toString();

    m_queue.setItems(std::move(items), settings.value(QStringLiteral("playback/queue_current_index"), 0).toInt());
    if (const QueueItem* item = m_queue.current()) {
        const qint64 positionForCurrent = restoredPositionFile == item->filePath ? restoredPositionMs : 0;
        loadQueueItem(*item, false, positionForCurrent);
        m_lastPersistedPositionMs = m_positionMs;
    }
    emit queueChanged();
}

void PlaybackController::setPlaybackState(const QString& playbackState)
{
    const bool nextIsPlaying = playbackState == QStringLiteral("playing");
    if (m_playbackState == playbackState && m_isPlaying == nextIsPlaying) {
        return;
    }

    m_playbackState = playbackState;
    m_isPlaying = nextIsPlaying;
    emit playbackStateChanged();
}

void PlaybackController::setErrorMessage(const QString& errorMessage)
{
    if (m_errorMessage == errorMessage) {
        return;
    }

    m_errorMessage = errorMessage;
    emit errorMessageChanged();
}

} // namespace opusora
