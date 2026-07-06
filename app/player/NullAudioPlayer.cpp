#include "player/NullAudioPlayer.hpp"

#include <algorithm>

namespace opusora {

void NullAudioPlayer::load(const std::string& uri)
{
    m_uri = uri;
    m_positionMs = 0;
    m_durationMs = 0;
    m_state = PlaybackState::Stopped;
    m_lastError = "GStreamer development package was not available at build time";
}

void NullAudioPlayer::play()
{
    m_state = m_uri.empty() ? PlaybackState::Idle : PlaybackState::Playing;
}

void NullAudioPlayer::pause()
{
    if (m_state == PlaybackState::Playing) {
        m_state = PlaybackState::Paused;
    }
}

void NullAudioPlayer::stop()
{
    m_positionMs = 0;
    m_state = PlaybackState::Stopped;
}

void NullAudioPlayer::seekMs(std::int64_t positionMs)
{
    m_positionMs = std::max<std::int64_t>(0, std::min(positionMs, m_durationMs));
}

std::int64_t NullAudioPlayer::positionMs() const
{
    return m_positionMs;
}

std::int64_t NullAudioPlayer::durationMs() const
{
    return m_durationMs;
}

void NullAudioPlayer::setVolume(double volume)
{
    m_volume = std::max(0.0, std::min(volume, 1.0));
}

double NullAudioPlayer::volume() const
{
    return m_volume;
}

void NullAudioPlayer::setEqualizer(double lowGainDb, double midGainDb, double highGainDb)
{
    m_lowGainDb = std::max(-12.0, std::min(lowGainDb, 12.0));
    m_midGainDb = std::max(-12.0, std::min(midGainDb, 12.0));
    m_highGainDb = std::max(-12.0, std::min(highGainDb, 12.0));
}

void NullAudioPlayer::setGaplessNextUri(const std::string& uri)
{
    (void)uri;
}

void NullAudioPlayer::setGaplessTransitionCallback(std::function<void()> callback)
{
    (void)callback;
}

PlaybackState NullAudioPlayer::state() const
{
    return m_state;
}

std::string NullAudioPlayer::lastError() const
{
    return m_lastError;
}

} // namespace opusora
