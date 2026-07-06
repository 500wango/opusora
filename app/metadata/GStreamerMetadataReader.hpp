#pragma once

#include "metadata/MetadataReader.hpp"

namespace opusora {

class GStreamerMetadataReader final : public MetadataReader {
public:
    GStreamerMetadataReader();

    TrackMetadata read(const std::string& filePath) override;
};

} // namespace opusora
