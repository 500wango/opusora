#pragma once

#include <QObject>

class QAction;
class QMenu;
class QSystemTrayIcon;

namespace opusora {

class PlaybackController;

class TrayService : public QObject {
    Q_OBJECT

public:
    explicit TrayService(PlaybackController* playback, QObject* parent = nullptr);
    ~TrayService() override;

private slots:
    void refresh();

private:
    PlaybackController* m_playback = nullptr;
    QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu* m_menu = nullptr;
    QAction* m_playPauseAction = nullptr;
};

} // namespace opusora
