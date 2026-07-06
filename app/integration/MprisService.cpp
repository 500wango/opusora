#include "integration/MprisService.hpp"

#include "player/PlaybackController.hpp"

#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDebug>
#include <QFileInfo>
#include <QUrl>
#include <QVariantMap>

namespace {

constexpr const char* MPRIS_SERVICE = "org.mpris.MediaPlayer2.opusora";
constexpr const char* MPRIS_PATH = "/org/mpris/MediaPlayer2";

QStringList supportedMimeTypes()
{
    return {
        QStringLiteral("audio/aac"),
        QStringLiteral("audio/flac"),
        QStringLiteral("audio/mpeg"),
        QStringLiteral("audio/mp4"),
        QStringLiteral("audio/ogg"),
        QStringLiteral("audio/opus"),
        QStringLiteral("audio/wav"),
        QStringLiteral("audio/x-flac"),
        QStringLiteral("audio/x-m4a"),
        QStringLiteral("audio/x-wav"),
    };
}

void emitPropertiesChanged(const QString& interfaceName, const QVariantMap& changed)
{
    QDBusMessage message = QDBusMessage::createSignal(
        QStringLiteral("/org/mpris/MediaPlayer2"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    message << interfaceName << changed << QStringList();
    QDBusConnection::sessionBus().send(message);
}

} // namespace

namespace opusora {

class MprisMediaPlayer2Adaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit)
    Q_PROPERTY(bool CanRaise READ canRaise)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes)

public:
    explicit MprisMediaPlayer2Adaptor(QObject* parent)
        : QDBusAbstractAdaptor(parent)
    {
    }

    bool canQuit() const { return true; }
    bool canRaise() const { return false; }
    bool hasTrackList() const { return false; }
    QString identity() const { return QStringLiteral("Opusora"); }
    QString desktopEntry() const { return QStringLiteral("opusora"); }
    QStringList supportedUriSchemes() const { return { QStringLiteral("file"), QStringLiteral("http"), QStringLiteral("https") }; }
    QStringList supportedMimeTypes() const { return ::supportedMimeTypes(); }

public slots:
    void Raise() {}
    void Quit() { QCoreApplication::quit(); }
};

class MprisPlayerAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QString LoopStatus READ loopStatus WRITE setLoopStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(bool Shuffle READ shuffle WRITE setShuffle)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ minimumRate)
    Q_PROPERTY(double MaximumRate READ maximumRate)
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPause)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl)

public:
    MprisPlayerAdaptor(PlaybackController* playback, QObject* parent)
        : QDBusAbstractAdaptor(parent)
        , m_playback(playback)
    {
        if (!m_playback) {
            return;
        }

        connect(m_playback, &PlaybackController::currentTrackChanged, this, &MprisPlayerAdaptor::publishState);
        connect(m_playback, &PlaybackController::playbackStateChanged, this, &MprisPlayerAdaptor::publishState);
        connect(m_playback, &PlaybackController::queueChanged, this, &MprisPlayerAdaptor::publishState);
        connect(m_playback, &PlaybackController::volumeChanged, this, &MprisPlayerAdaptor::publishVolume);
        connect(m_playback, &PlaybackController::playbackModeChanged, this, &MprisPlayerAdaptor::publishMode);
    }

    QString playbackStatus() const
    {
        if (!m_playback) {
            return QStringLiteral("Stopped");
        }
        if (m_playback->playbackState() == QStringLiteral("playing")) {
            return QStringLiteral("Playing");
        }
        if (m_playback->playbackState() == QStringLiteral("paused")) {
            return QStringLiteral("Paused");
        }
        return QStringLiteral("Stopped");
    }

    QString loopStatus() const
    {
        if (!m_playback) {
            return QStringLiteral("None");
        }
        if (m_playback->playbackMode() == QStringLiteral("repeatOne")) {
            return QStringLiteral("Track");
        }
        if (m_playback->playbackMode() == QStringLiteral("repeatAll")) {
            return QStringLiteral("Playlist");
        }
        return QStringLiteral("None");
    }

    void setLoopStatus(const QString& value)
    {
        if (!m_playback) {
            return;
        }
        if (value == QStringLiteral("Track")) {
            m_playback->setPlaybackMode(QStringLiteral("repeatOne"));
        } else if (value == QStringLiteral("Playlist")) {
            m_playback->setPlaybackMode(QStringLiteral("repeatAll"));
        } else {
            m_playback->setPlaybackMode(QStringLiteral("sequential"));
        }
    }

    double rate() const { return 1.0; }
    void setRate(double rate) { Q_UNUSED(rate); }

    bool shuffle() const
    {
        return m_playback && m_playback->playbackMode() == QStringLiteral("shuffle");
    }

    void setShuffle(bool enabled)
    {
        if (!m_playback) {
            return;
        }
        m_playback->setPlaybackMode(enabled ? QStringLiteral("shuffle") : QStringLiteral("sequential"));
    }

    QVariantMap metadata() const
    {
        QVariantMap data;
        if (!m_playback || m_playback->currentFilePath().isEmpty() || m_playback->queueIndex() < 0) {
            data.insert(QStringLiteral("mpris:trackid"), QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/mpris/MediaPlayer2/Track/NoTrack"))));
            return data;
        }

        data.insert(
            QStringLiteral("mpris:trackid"),
            QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/mpris/MediaPlayer2/Track/%1").arg(m_playback->queueIndex()))));
        data.insert(QStringLiteral("mpris:length"), QVariant::fromValue<qlonglong>(m_playback->durationMs() * 1000));
        data.insert(QStringLiteral("xesam:title"), m_playback->title());
        if (!m_playback->artist().isEmpty()) {
            data.insert(QStringLiteral("xesam:artist"), QStringList { m_playback->artist() });
        }
        const QUrl sourceUrl(m_playback->currentFilePath());
        data.insert(
            QStringLiteral("xesam:url"),
            sourceUrl.isValid() && !sourceUrl.scheme().isEmpty()
                ? sourceUrl.toString()
                : QUrl::fromLocalFile(m_playback->currentFilePath()).toString());
        if (!m_playback->coverUrl().isEmpty()) {
            data.insert(QStringLiteral("mpris:artUrl"), m_playback->coverUrl().toString());
        }
        return data;
    }

    double volume() const { return m_playback ? m_playback->volume() : 0.0; }
    void setVolume(double volume)
    {
        if (m_playback) {
            m_playback->setVolume(volume);
        }
    }

    qlonglong position() const { return m_playback ? m_playback->positionMs() * 1000 : 0; }
    double minimumRate() const { return 1.0; }
    double maximumRate() const { return 1.0; }
    bool canGoNext() const { return m_playback && m_playback->queueCount() > 0; }
    bool canGoPrevious() const { return m_playback && m_playback->queueCount() > 0; }
    bool canPlay() const { return m_playback != nullptr; }
    bool canPause() const { return m_playback != nullptr; }
    bool canSeek() const { return m_playback && m_playback->durationMs() > 0; }
    bool canControl() const { return m_playback != nullptr; }

public slots:
    void Next()
    {
        if (m_playback) {
            m_playback->next();
        }
    }

    void Previous()
    {
        if (m_playback) {
            m_playback->previous();
        }
    }

    void Pause()
    {
        if (m_playback && m_playback->isPlaying()) {
            m_playback->togglePlayPause();
        }
    }

    void PlayPause()
    {
        if (m_playback) {
            m_playback->togglePlayPause();
        }
    }

    void Stop()
    {
        if (m_playback) {
            m_playback->stop();
        }
    }

    void Play()
    {
        if (m_playback && !m_playback->isPlaying()) {
            m_playback->togglePlayPause();
        }
    }

    void Seek(qlonglong offset)
    {
        if (!m_playback) {
            return;
        }
        const qlonglong nextPositionMs = m_playback->positionMs() + offset / 1000;
        m_playback->seek(nextPositionMs);
        emit Seeked(m_playback->positionMs() * 1000);
    }

    void SetPosition(const QDBusObjectPath& trackId, qlonglong position)
    {
        Q_UNUSED(trackId);
        if (!m_playback) {
            return;
        }
        m_playback->seek(position / 1000);
        emit Seeked(m_playback->positionMs() * 1000);
    }

    void OpenUri(const QString& uri)
    {
        if (!m_playback) {
            return;
        }
        const QUrl url(uri);
        if (url.isLocalFile()) {
            m_playback->openFile(url.toLocalFile());
        }
    }

signals:
    void Seeked(qlonglong position);

private slots:
    void publishState()
    {
        QVariantMap changed;
        changed.insert(QStringLiteral("PlaybackStatus"), playbackStatus());
        changed.insert(QStringLiteral("Metadata"), metadata());
        changed.insert(QStringLiteral("CanGoNext"), canGoNext());
        changed.insert(QStringLiteral("CanGoPrevious"), canGoPrevious());
        changed.insert(QStringLiteral("CanSeek"), canSeek());
        emitPropertiesChanged(QStringLiteral("org.mpris.MediaPlayer2.Player"), changed);
    }

    void publishVolume()
    {
        emitPropertiesChanged(
            QStringLiteral("org.mpris.MediaPlayer2.Player"),
            QVariantMap { { QStringLiteral("Volume"), volume() } });
    }

    void publishMode()
    {
        emitPropertiesChanged(
            QStringLiteral("org.mpris.MediaPlayer2.Player"),
            QVariantMap {
                { QStringLiteral("LoopStatus"), loopStatus() },
                { QStringLiteral("Shuffle"), shuffle() },
            });
    }

private:
    PlaybackController* m_playback = nullptr;
};

MprisService::MprisService(PlaybackController* playback, QObject* parent)
    : QObject(parent)
{
    new MprisMediaPlayer2Adaptor(this);
    new MprisPlayerAdaptor(playback, this);

    QDBusConnection bus = QDBusConnection::sessionBus();
    m_registered = bus.registerService(QStringLiteral("org.mpris.MediaPlayer2.opusora"))
        && bus.registerObject(QStringLiteral("/org/mpris/MediaPlayer2"), this, QDBusConnection::ExportAdaptors);
    if (!m_registered) {
        qWarning() << "Failed to register MPRIS service" << bus.lastError().message();
    } else {
        qInfo() << "Registered MPRIS service" << MPRIS_SERVICE;
    }
}

MprisService::~MprisService()
{
    if (!m_registered) {
        return;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterObject(QStringLiteral("/org/mpris/MediaPlayer2"));
    bus.unregisterService(QStringLiteral("org.mpris.MediaPlayer2.opusora"));
}

bool MprisService::isRegistered() const
{
    return m_registered;
}

} // namespace opusora

#include "MprisService.moc"
