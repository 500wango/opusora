#pragma once

#include "metadata/MetadataReader.hpp"

namespace opusora {

class BasicMetadataReader final : public MetadataReader {
public:
    TrackMetadata read(const std::string& filePath) override;
};

} // namespace opusora
