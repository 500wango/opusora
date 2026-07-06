#include "integration/TrayService.hpp"

#include "player/PlaybackController.hpp"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

namespace opusora {

TrayService::TrayService(PlaybackController* playback, QObject* parent)
    : QObject(parent)
    , m_playback(playback)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable() || !m_playback) {
        return;
    }

    m_menu = new QMenu();
    m_playPauseAction = m_menu->addAction(tr("Play"));
    connect(m_playPauseAction, &QAction::triggered, m_playback, &PlaybackController::togglePlayPause);

    QAction* previousAction = m_menu->addAction(tr("Previous"));
    connect(previousAction, &QAction::triggered, m_playback, &PlaybackController::previous);

    QAction* nextAction = m_menu->addAction(tr("Next"));
    connect(nextAction, &QAction::triggered, m_playback, &PlaybackController::next);

    QAction* stopAction = m_menu->addAction(tr("Stop"));
    connect(stopAction, &QAction::triggered, m_playback, &PlaybackController::stop);

    m_menu->addSeparator();
    QAction* quitAction = m_menu->addAction(tr("Quit"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon::fromTheme(QStringLiteral("multimedia-player")));
    m_trayIcon->setContextMenu(m_menu);
    m_trayIcon->show();

    connect(m_playback, &PlaybackController::currentTrackChanged, this, &TrayService::refresh);
    connect(m_playback, &PlaybackController::playbackStateChanged, this, &TrayService::refresh);
    refresh();
}

TrayService::~TrayService()
{
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
    delete m_menu;
}

void TrayService::refresh()
{
    if (!m_trayIcon || !m_playback) {
        return;
    }

    const QString title = m_playback->title().trimmed().isEmpty()
        ? QStringLiteral("Opusora")
        : m_playback->title().trimmed();
    const QString artist = m_playback->artist().trimmed();
    m_trayIcon->setToolTip(artist.isEmpty() ? title : title + QStringLiteral(" - ") + artist);

    if (m_playPauseAction) {
        m_playPauseAction->setText(m_playback->isPlaying() ? tr("Pause") : tr("Play"));
    }
}

} // namespace opusora
