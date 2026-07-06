#include "metadata/MetadataReaderFactory.hpp"

#include "metadata/BasicMetadataReader.hpp"
#if OPUSORA_WITH_GSTREAMER_METADATA
#include "metadata/GStreamerMetadataReader.hpp"
#endif

namespace opusora {

std::unique_ptr<MetadataReader> createMetadataReader()
{
#if OPUSORA_WITH_GSTREAMER_METADATA
    return std::make_unique<GStreamerMetadataReader>();
#else
    return std::make_unique<BasicMetadataReader>();
#endif
}

} // namespace opusora
