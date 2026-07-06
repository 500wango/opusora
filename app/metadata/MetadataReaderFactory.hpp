#pragma once

#include "metadata/MetadataReader.hpp"

#include <memory>

namespace opusora {

std::unique_ptr<MetadataReader> createMetadataReader();

} // namespace opusora
