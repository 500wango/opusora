#pragma once

#include <QObject>

namespace opusora {

class PlaybackStateNamespace {
    Q_GADGET
public:
    enum Value {
        Idle,
        Loading,
        Playing,
        Paused,
        Stopped,
        Buffering,
        Error
    };
    Q_ENUM(Value)
};

using PlaybackState = PlaybackStateNamespace::Value;

} // namespace opusora
