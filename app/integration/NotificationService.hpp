#pragma once

#include <QObject>

namespace opusora {

class PlaybackController;

class NotificationService : public QObject {
    Q_OBJECT

public:
    explicit NotificationService(PlaybackController* playback, QObject* parent = nullptr);

private slots:
    void showCurrentTrack();

private:
    PlaybackController* m_playback = nullptr;
};

} // namespace opusora
