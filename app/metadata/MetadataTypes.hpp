#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace opusora {

enum class MetadataReadStatus {
    Pending,
    Reading,
    Success,
    Partial,
    Failed
};

enum class MetadataEncodingStatus {
    Normal,
    SuspectedEncodingIssue,
    Unknown
};

struct EmbeddedCover {
    std::string mimeType;
    std::vector<std::byte> data;
};

struct TrackMetadata {
    std::string title;
    std::string artist;
    std::string album;
    std::string albumArtist;
    std::string genre;
    std::string releaseDate;

    int trackNumber = 0;
    int discNumber = 0;
    int durationMs = 0;
    int bitrate = 0;
    int sampleRate = 0;
    int channels = 0;

    std::string format;
    std::string composer;
    std::string comment;
    std::string lyrics;
    int bpm = 0;
    double replayGainTrackGainDb = 0.0;
    double replayGainAlbumGainDb = 0.0;
    bool hasReplayGainTrackGain = false;
    bool hasReplayGainAlbumGain = false;

    std::optional<EmbeddedCover> cover;
    MetadataReadStatus readStatus = MetadataReadStatus::Pending;
    MetadataEncodingStatus encodingStatus = MetadataEncodingStatus::Unknown;
};

} // namespace opusora
