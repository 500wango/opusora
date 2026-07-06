#pragma once

#include "player/PlaybackState.hpp"

#include <cstdint>
#include <functional>
#include <string>

namespace opusora {

class IAudioPlayer {
public:
    virtual ~IAudioPlayer() = default;

    virtual void load(const std::string& uri) = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void seekMs(std::int64_t positionMs) = 0;

    virtual std::int64_t positionMs() const = 0;
    virtual std::int64_t durationMs() const = 0;

    virtual void setVolume(double volume) = 0;
    virtual double volume() const = 0;
    virtual void setEqualizer(double lowGainDb, double midGainDb, double highGainDb) = 0;
    virtual void setGaplessNextUri(const std::string& uri) = 0;
    virtual void setGaplessTransitionCallback(std::function<void()> callback) = 0;

    virtual PlaybackState state() const = 0;
    virtual std::string lastError() const = 0;
};

} // namespace opusora
