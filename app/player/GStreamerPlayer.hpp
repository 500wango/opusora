#pragma once

#include "player/IAudioPlayer.hpp"

#if OPUSORA_WITH_GSTREAMER
#include <gst/gst.h>
#endif

#include <functional>
#include <mutex>
#include <string>

namespace opusora {

class GStreamerPlayer final : public IAudioPlayer {
public:
    GStreamerPlayer();
    ~GStreamerPlayer() override;

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
#if OPUSORA_WITH_GSTREAMER
    void drainBus() const;
    void handleAboutToFinish();
    void setState(PlaybackState state) const;
    void setLastError(std::string error) const;

    GstElement* m_pipeline = nullptr;
    GstElement* m_equalizer = nullptr;
    mutable std::mutex m_mutex;
    std::mutex m_gaplessMutex;
    std::string m_gaplessNextUri;
    std::function<void()> m_gaplessTransitionCallback;
    mutable PlaybackState m_state = PlaybackState::Idle;
    mutable std::string m_lastError;
#endif
    double m_volume = 0.8;
    double m_lowGainDb = 0.0;
    double m_midGainDb = 0.0;
    double m_highGainDb = 0.0;
};

} // namespace opusora
