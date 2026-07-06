#pragma once

#include <QObject>

namespace opusora {

class PlaybackController;

class MprisService : public QObject {
    Q_OBJECT

public:
    explicit MprisService(PlaybackController* playback, QObject* parent = nullptr);
    ~MprisService() override;

    bool isRegistered() const;

private:
    bool m_registered = false;
};

} // namespace opusora
