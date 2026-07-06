#pragma once

#include "metadata/MetadataTypes.hpp"

#include <string>

namespace opusora {

class MetadataReader {
public:
    virtual ~MetadataReader() = default;

    virtual TrackMetadata read(const std::string& filePath) = 0;
};

} // namespace opusora
