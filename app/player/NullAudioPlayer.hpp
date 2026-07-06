#pragma once

#include "player/IAudioPlayer.hpp"

namespace opusora {

class NullAudioPlayer final : public IAudioPlayer {
public:
    void load(const std::string& uri) override;
    void play() override;
    void pause() override;
    void stop() override;
    void seekMs(std::int64_t positionMs) override;

    std::int64_t positionMs() const override;
    std::int64_t durationMs() const override;

    void setVolume(double volume) override;
    double volume() const override;
    void setEqualizer(double lowGainDb, double midGainDb, double highGainDb) override;
    void setGaplessNextUri(const std::string& uri) override;
    void setGaplessTransitionCallback(std::function<void()> callback) override;

    PlaybackState state() const override;
    std::string lastError() const override;

private:
    std::string m_uri;
    std::int64_t m_positionMs = 0;
    std::int64_t m_durationMs = 0;
    double m_volume = 0.8;
    double m_lowGainDb = 0.0;
    double m_midGainDb = 0.0;
    double m_highGainDb = 0.0;
    PlaybackState m_state = PlaybackState::Idle;
    std::string m_lastError;
};

} // namespace opusora
