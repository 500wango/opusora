#include "player/GStreamerPlayer.hpp"

#if OPUSORA_WITH_GSTREAMER

#include <QDebug>

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <utility>

namespace {

void ensureGStreamerInitialized()
{
    static std::once_flag once;
    std::call_once(once, []() {
        gst_init(nullptr, nullptr);
    });
}

GstState toGstState(opusora::PlaybackState state)
{
    switch (state) {
    case opusora::PlaybackState::Playing:
        return GST_STATE_PLAYING;
    case opusora::PlaybackState::Paused:
        return GST_STATE_PAUSED;
    case opusora::PlaybackState::Stopped:
    case opusora::PlaybackState::Idle:
    case opusora::PlaybackState::Loading:
    case opusora::PlaybackState::Buffering:
    case opusora::PlaybackState::Error:
        return GST_STATE_READY;
    }

    return GST_STATE_READY;
}

bool isGenericInternalStreamError(const std::string& message)
{
    return message.find("Internal data stream error") != std::string::npos;
}

} // namespace

namespace opusora {

GStreamerPlayer::GStreamerPlayer()
{
    ensureGStreamerInitialized();
    m_pipeline = gst_element_factory_make("playbin", "opusora-playbin");
    if (!m_pipeline) {
        setState(PlaybackState::Error);
        setLastError("Failed to create GStreamer playbin element");
        qWarning() << "Failed to create GStreamer playbin element";
        return;
    }
    g_signal_connect(
        m_pipeline,
        "about-to-finish",
        G_CALLBACK(+[](GstElement*, gpointer userData) {
            static_cast<GStreamerPlayer*>(userData)->handleAboutToFinish();
        }),
        this);

    if (std::getenv("OPUSORA_GST_FAKE_AUDIO_SINK")) {
        GstElement* sink = gst_element_factory_make("fakesink", "opusora-fake-audio-sink");
        if (sink) {
            g_object_set(G_OBJECT(sink), "sync", FALSE, nullptr);
            gst_object_ref_sink(sink);
            g_object_set(G_OBJECT(m_pipeline), "audio-sink", sink, nullptr);
            gst_object_unref(sink);
            qInfo() << "Using GStreamer fakesink audio output";
        }
    }

    m_equalizer = gst_element_factory_make("equalizer-3bands", "opusora-equalizer");
    if (m_equalizer) {
        gst_object_ref_sink(m_equalizer);
        g_object_set(G_OBJECT(m_pipeline), "audio-filter", m_equalizer, nullptr);
    } else {
        qWarning() << "GStreamer equalizer-3bands element is not available";
    }

    setVolume(m_volume);
    setEqualizer(m_lowGainDb, m_midGainDb, m_highGainDb);
}

GStreamerPlayer::~GStreamerPlayer()
{
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
    if (m_equalizer) {
        gst_object_unref(m_equalizer);
        m_equalizer = nullptr;
    }
}

void GStreamerPlayer::load(const std::string& uri)
{
    if (!m_pipeline) {
        setState(PlaybackState::Error);
        return;
    }

    setState(PlaybackState::Loading);
    setGaplessNextUri({});
    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    gst_element_get_state(m_pipeline, nullptr, nullptr, 2 * GST_SECOND);
    drainBus();
    g_object_set(G_OBJECT(m_pipeline), "uri", uri.c_str(), nullptr);
    setLastError({});
    setState(PlaybackState::Stopped);
}

void GStreamerPlayer::play()
{
    if (!m_pipeline) {
        setState(PlaybackState::Error);
        return;
    }

    const GstStateChangeReturn result = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (result == GST_STATE_CHANGE_FAILURE) {
        drainBus();
        setState(PlaybackState::Error);
        std::string currentError;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            currentError = m_lastError;
        }
        if (currentError.empty()) {
            setLastError("GStreamer failed to enter PLAYING state");
        }
        return;
    }

    setState(PlaybackState::Playing);
}

void GStreamerPlayer::pause()
{
    if (!m_pipeline) {
        return;
    }

    const GstStateChangeReturn result = gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    if (result == GST_STATE_CHANGE_FAILURE) {
        setState(PlaybackState::Error);
        setLastError("GStreamer failed to enter PAUSED state");
        return;
    }

    setState(PlaybackState::Paused);
}

void GStreamerPlayer::stop()
{
    if (!m_pipeline) {
        return;
    }

    gst_element_set_state(m_pipeline, GST_STATE_READY);
    setState(PlaybackState::Stopped);
}

void GStreamerPlayer::seekMs(std::int64_t positionMs)
{
    if (!m_pipeline) {
        return;
    }

    const auto bounded = std::max<std::int64_t>(0, positionMs);
    if (!gst_element_seek_simple(
            m_pipeline,
            GST_FORMAT_TIME,
            static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
            bounded * GST_MSECOND)) {
        setLastError("GStreamer seek failed");
    }
}

std::int64_t GStreamerPlayer::positionMs() const
{
    drainBus();
    if (!m_pipeline) {
        return 0;
    }

    gint64 position = 0;
    if (gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &position)) {
        return static_cast<std::int64_t>(position / GST_MSECOND);
    }

    return 0;
}

std::int64_t GStreamerPlayer::durationMs() const
{
    drainBus();
    if (!m_pipeline) {
        return 0;
    }

    gint64 duration = 0;
    if (gst_element_query_duration(m_pipeline, GST_FORMAT_TIME, &duration)) {
        return static_cast<std::int64_t>(duration / GST_MSECOND);
    }

    return 0;
}

void GStreamerPlayer::setVolume(double volume)
{
    m_volume = std::max(0.0, std::min(volume, 1.0));
    if (m_pipeline) {
        g_object_set(G_OBJECT(m_pipeline), "volume", m_volume, nullptr);
    }
}

double GStreamerPlayer::volume() const
{
    return m_volume;
}

void GStreamerPlayer::setEqualizer(double lowGainDb, double midGainDb, double highGainDb)
{
    m_lowGainDb = std::max(-12.0, std::min(lowGainDb, 12.0));
    m_midGainDb = std::max(-12.0, std::min(midGainDb, 12.0));
    m_highGainDb = std::max(-12.0, std::min(highGainDb, 12.0));
    if (m_equalizer) {
        g_object_set(
            G_OBJECT(m_equalizer),
            "band0", m_lowGainDb,
            "band1", m_midGainDb,
            "band2", m_highGainDb,
            nullptr);
    }
}

void GStreamerPlayer::setGaplessNextUri(const std::string& uri)
{
    std::lock_guard<std::mutex> lock(m_gaplessMutex);
    m_gaplessNextUri = uri;
}

void GStreamerPlayer::setGaplessTransitionCallback(std::function<void()> callback)
{
    std::lock_guard<std::mutex> lock(m_gaplessMutex);
    m_gaplessTransitionCallback = std::move(callback);
}

PlaybackState GStreamerPlayer::state() const
{
    drainBus();
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

std::string GStreamerPlayer::lastError() const
{
    drainBus();
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

void GStreamerPlayer::drainBus() const
{
    if (!m_pipeline) {
        return;
    }

    GstBus* bus = gst_element_get_bus(m_pipeline);
    if (!bus) {
        return;
    }

    while (GstMessage* message = gst_bus_pop(bus)) {
        switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError* error = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_error(message, &error, &debug);
            const std::string errorMessage = error ? error->message : "Unknown GStreamer error";
            bool keepExistingSpecificError = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                keepExistingSpecificError = isGenericInternalStreamError(errorMessage) && !m_lastError.empty();
            }
            setState(PlaybackState::Error);
            if (!keepExistingSpecificError) {
                setLastError(errorMessage);
            }
            qWarning() << "GStreamer error:" << (error ? error->message : "unknown")
                       << (debug ? debug : "");
            if (error) {
                g_error_free(error);
            }
            if (debug) {
                g_free(debug);
            }
            break;
        }
        case GST_MESSAGE_EOS:
            gst_element_set_state(m_pipeline, GST_STATE_READY);
            setState(PlaybackState::Stopped);
            break;
        case GST_MESSAGE_BUFFERING: {
            gint percent = 0;
            gst_message_parse_buffering(message, &percent);
            if (percent < 100) {
                setState(PlaybackState::Buffering);
            } else {
                GstState current = GST_STATE_NULL;
                gst_element_get_state(m_pipeline, &current, nullptr, 0);
                setState(current == GST_STATE_PLAYING ? PlaybackState::Playing : PlaybackState::Paused);
            }
            break;
        }
        case GST_MESSAGE_STATE_CHANGED:
            if (GST_MESSAGE_SRC(message) == GST_OBJECT(m_pipeline)) {
                GstState oldState = GST_STATE_NULL;
                GstState newState = GST_STATE_NULL;
                GstState pending = GST_STATE_NULL;
                gst_message_parse_state_changed(message, &oldState, &newState, &pending);
                switch (newState) {
                case GST_STATE_PLAYING:
                    setState(PlaybackState::Playing);
                    break;
                case GST_STATE_PAUSED:
                    setState(PlaybackState::Paused);
                    break;
                case GST_STATE_READY:
                    setState(PlaybackState::Stopped);
                    break;
                case GST_STATE_NULL:
                    setState(PlaybackState::Idle);
                    break;
                default:
                    break;
                }
            }
            break;
        default:
            break;
        }
        gst_message_unref(message);
    }

    gst_object_unref(bus);
}

void GStreamerPlayer::handleAboutToFinish()
{
    if (!m_pipeline) {
        return;
    }

    std::string nextUri;
    std::function<void()> transitionCallback;
    {
        std::lock_guard<std::mutex> lock(m_gaplessMutex);
        nextUri = std::move(m_gaplessNextUri);
        m_gaplessNextUri.clear();
        transitionCallback = m_gaplessTransitionCallback;
    }

    if (nextUri.empty()) {
        return;
    }

    g_object_set(G_OBJECT(m_pipeline), "uri", nextUri.c_str(), nullptr);
    qInfo() << "Prepared gapless playback handoff" << QString::fromStdString(nextUri);
    if (transitionCallback) {
        transitionCallback();
    }
}

void GStreamerPlayer::setState(PlaybackState state) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = state;
}

void GStreamerPlayer::setLastError(std::string error) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastError = std::move(error);
}

} // namespace opusora

#endif
