#include "integration/NotificationService.hpp"

#include "player/PlaybackController.hpp"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QSettings>
#include <QVariantMap>

namespace opusora {

NotificationService::NotificationService(PlaybackController* playback, QObject* parent)
    : QObject(parent)
    , m_playback(playback)
{
    if (m_playback) {
        connect(m_playback, &PlaybackController::currentTrackChanged, this, &NotificationService::showCurrentTrack);
    }
}

void NotificationService::showCurrentTrack()
{
    if (!m_playback || m_playback->currentFilePath().isEmpty()) {
        return;
    }
    if (!QSettings().value(QStringLiteral("desktop_notifications_enabled"), true).toBool()) {
        return;
    }

    const QString title = m_playback->title().trimmed();
    if (title.isEmpty()) {
        return;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("Notify"));
    message << QStringLiteral("Opusora")
            << static_cast<uint>(0)
            << QStringLiteral("opusora")
            << title
            << m_playback->artist()
            << QStringList()
            << QVariantMap()
            << 4000;
    QDBusConnection::sessionBus().asyncCall(message);
}

} // namespace opusora
