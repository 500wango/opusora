#include "metadata/GStreamerMetadataReader.hpp"

#if OPUSORA_WITH_GSTREAMER_METADATA

#include "core/TextEncoding.hpp"

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

#include <mutex>
#include <optional>
#include <string>

namespace {

void ensureGStreamerInitialized()
{
    static std::once_flag once;
    std::call_once(once, []() {
        gst_init(nullptr, nullptr);
    });
}

std::string tagString(const GstTagList* tags, const gchar* tag)
{
    if (!tags) {
        return {};
    }

    gchar* value = nullptr;
    if (!gst_tag_list_get_string(tags, tag, &value) || !value) {
        return {};
    }

    std::string result(value);
    g_free(value);
    return result;
}

int tagUInt(const GstTagList* tags, const gchar* tag)
{
    if (!tags) {
        return 0;
    }

    guint value = 0;
    return gst_tag_list_get_uint(tags, tag, &value) ? static_cast<int>(value) : 0;
}

std::optional<double> tagDouble(const GstTagList* tags, const gchar* tag)
{
    if (!tags) {
        return std::nullopt;
    }

    gdouble value = 0.0;
    if (!gst_tag_list_get_double(tags, tag, &value)) {
        return std::nullopt;
    }

    return static_cast<double>(value);
}

std::string tagDate(const GstTagList* tags)
{
    if (!tags) {
        return {};
    }

    GstDateTime* dateTime = nullptr;
    if (gst_tag_list_get_date_time(tags, GST_TAG_DATE_TIME, &dateTime) && dateTime) {
        gchar* iso = gst_date_time_to_iso8601_string(dateTime);
        std::string result = iso ? iso : "";
        if (iso) {
            g_free(iso);
        }
        gst_date_time_unref(dateTime);
        return result;
    }

    GDate* date = nullptr;
    if (gst_tag_list_get_date(tags, GST_TAG_DATE, &date) && date) {
        const std::string result = std::to_string(g_date_get_year(date));
        g_date_free(date);
        return result;
    }

    return {};
}

std::optional<opusora::EmbeddedCover> tagCover(const GstTagList* tags)
{
    if (!tags) {
        return std::nullopt;
    }

    GstSample* sample = nullptr;
    if (!gst_tag_list_get_sample(tags, GST_TAG_IMAGE, &sample) || !sample) {
        if (!gst_tag_list_get_sample(tags, GST_TAG_PREVIEW_IMAGE, &sample) || !sample) {
            return std::nullopt;
        }
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (!buffer) {
        gst_sample_unref(sample);
        return std::nullopt;
    }

    GstMapInfo mapInfo;
    if (!gst_buffer_map(buffer, &mapInfo, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return std::nullopt;
    }

    opusora::EmbeddedCover cover;
    cover.data.resize(mapInfo.size);
    std::memcpy(cover.data.data(), mapInfo.data, mapInfo.size);

    GstCaps* caps = gst_sample_get_caps(sample);
    if (caps && gst_caps_get_size(caps) > 0) {
        const GstStructure* structure = gst_caps_get_structure(caps, 0);
        if (structure) {
            cover.mimeType = gst_structure_get_name(structure);
        }
    }

    gst_buffer_unmap(buffer, &mapInfo);
    gst_sample_unref(sample);
    return cover;
}

std::string fallbackTitle(const QString& filePath)
{
    return QFileInfo(filePath).completeBaseName().toStdString();
}

std::string fallbackFormat(const QString& filePath)
{
    return QFileInfo(filePath).suffix().toUpper().toStdString();
}

std::string utf8String(const QString& text)
{
    const QByteArray bytes = text.toUtf8();
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

QString metadataStringFromTagValue(const std::string& value)
{
    return opusora::metadataStringFromUtf8(value).trimmed();
}

struct Id3v1Metadata {
    QString title;
    QString artist;
    QString album;
    QString releaseDate;
    QString comment;
    int trackNumber = 0;
};

QString id3v1Field(const QByteArray& tag, qsizetype offset, qsizetype length)
{
    if (tag.size() < offset + length) {
        return {};
    }
    return opusora::metadataStringFromLegacyBytes(tag.mid(offset, length)).trimmed();
}

std::optional<Id3v1Metadata> readId3v1Metadata(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly) || file.size() < 128) {
        return std::nullopt;
    }
    if (!file.seek(file.size() - 128)) {
        return std::nullopt;
    }

    const QByteArray tag = file.read(128);
    if (tag.size() != 128 || tag.left(3) != QByteArrayLiteral("TAG")) {
        return std::nullopt;
    }

    Id3v1Metadata metadata;
    metadata.title = id3v1Field(tag, 3, 30);
    metadata.artist = id3v1Field(tag, 33, 30);
    metadata.album = id3v1Field(tag, 63, 30);
    metadata.releaseDate = id3v1Field(tag, 93, 4);
    if (static_cast<unsigned char>(tag.at(125)) == 0 && static_cast<unsigned char>(tag.at(126)) != 0) {
        metadata.comment = id3v1Field(tag, 97, 28);
        metadata.trackNumber = static_cast<unsigned char>(tag.at(126));
    } else {
        metadata.comment = id3v1Field(tag, 97, 30);
    }
    return metadata;
}

void preferLegacyText(std::string& target, const QString& legacyText)
{
    const QString trimmed = legacyText.trimmed();
    if (trimmed.isEmpty() || opusora::hasLikelyEncodingDamage(trimmed)) {
        return;
    }

    const QString current = metadataStringFromTagValue(target);
    if (current.isEmpty() || opusora::hasLikelyEncodingDamage(current)) {
        target = utf8String(trimmed);
    }
}

} // namespace

namespace opusora {

GStreamerMetadataReader::GStreamerMetadataReader()
{
    ensureGStreamerInitialized();
}

TrackMetadata GStreamerMetadataReader::read(const std::string& filePath)
{
    const QString qFilePath = QString::fromStdString(filePath);
    TrackMetadata metadata;
    metadata.title = fallbackTitle(qFilePath);
    metadata.format = fallbackFormat(qFilePath);
    metadata.readStatus = MetadataReadStatus::Failed;
    metadata.encodingStatus = MetadataEncodingStatus::Unknown;

    GError* error = nullptr;
    GstDiscoverer* discoverer = gst_discoverer_new(5 * GST_SECOND, &error);
    if (!discoverer) {
        if (error) {
            metadata.comment = error->message;
            g_error_free(error);
        }
        return metadata;
    }

    const QByteArray uri = QUrl::fromLocalFile(qFilePath).toString().toUtf8();
    GstDiscovererInfo* info = gst_discoverer_discover_uri(discoverer, uri.constData(), &error);
    if (!info) {
        if (error) {
            metadata.comment = error->message;
            g_error_free(error);
        }
        g_object_unref(discoverer);
        return metadata;
    }

    const GstDiscovererResult result = gst_discoverer_info_get_result(info);
    if (result != GST_DISCOVERER_OK) {
        metadata.readStatus = MetadataReadStatus::Partial;
    } else {
        metadata.readStatus = MetadataReadStatus::Success;
    }

    const GstTagList* tags = gst_discoverer_info_get_tags(info);
    const std::string title = tagString(tags, GST_TAG_TITLE);
    if (!title.empty()) {
        metadata.title = title;
    }
    metadata.artist = tagString(tags, GST_TAG_ARTIST);
    metadata.album = tagString(tags, GST_TAG_ALBUM);
    metadata.albumArtist = tagString(tags, GST_TAG_ALBUM_ARTIST);
    metadata.genre = tagString(tags, GST_TAG_GENRE);
    metadata.composer = tagString(tags, GST_TAG_COMPOSER);
    metadata.comment = tagString(tags, GST_TAG_COMMENT);
    metadata.lyrics = tagString(tags, GST_TAG_LYRICS);
    metadata.releaseDate = tagDate(tags);
    metadata.trackNumber = tagUInt(tags, GST_TAG_TRACK_NUMBER);
    metadata.discNumber = tagUInt(tags, GST_TAG_ALBUM_VOLUME_NUMBER);
    metadata.bitrate = tagUInt(tags, GST_TAG_BITRATE);
    metadata.cover = tagCover(tags);
    if (const std::optional<double> trackGain = tagDouble(tags, GST_TAG_TRACK_GAIN)) {
        metadata.replayGainTrackGainDb = *trackGain;
        metadata.hasReplayGainTrackGain = true;
    }
    if (const std::optional<double> albumGain = tagDouble(tags, GST_TAG_ALBUM_GAIN)) {
        metadata.replayGainAlbumGainDb = *albumGain;
        metadata.hasReplayGainAlbumGain = true;
    }

    const GstClockTime duration = gst_discoverer_info_get_duration(info);
    if (GST_CLOCK_TIME_IS_VALID(duration)) {
        metadata.durationMs = static_cast<int>(duration / GST_MSECOND);
    }

    if (const std::optional<Id3v1Metadata> id3v1 = readId3v1Metadata(qFilePath)) {
        preferLegacyText(metadata.title, id3v1->title);
        preferLegacyText(metadata.artist, id3v1->artist);
        preferLegacyText(metadata.album, id3v1->album);
        preferLegacyText(metadata.releaseDate, id3v1->releaseDate);
        preferLegacyText(metadata.comment, id3v1->comment);
        if (metadata.trackNumber <= 0 && id3v1->trackNumber > 0) {
            metadata.trackNumber = id3v1->trackNumber;
        }
    }

    GList* audioStreams = gst_discoverer_info_get_audio_streams(info);
    if (audioStreams) {
        auto* audioInfo = GST_DISCOVERER_AUDIO_INFO(audioStreams->data);
        if (audioInfo) {
            metadata.channels = static_cast<int>(gst_discoverer_audio_info_get_channels(audioInfo));
            metadata.sampleRate = static_cast<int>(gst_discoverer_audio_info_get_sample_rate(audioInfo));
            const int streamBitrate = static_cast<int>(gst_discoverer_audio_info_get_bitrate(audioInfo));
            if (metadata.bitrate == 0) {
                metadata.bitrate = streamBitrate;
            }
        }
        gst_discoverer_stream_info_list_free(audioStreams);
    }

    metadata.encodingStatus = MetadataEncodingStatus::Normal;

    gst_discoverer_info_unref(info);
    g_object_unref(discoverer);
    return metadata;
}

} // namespace opusora

#endif
