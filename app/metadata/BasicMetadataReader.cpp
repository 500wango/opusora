#include "metadata/BasicMetadataReader.hpp"

#include <QFileInfo>

namespace opusora {

TrackMetadata BasicMetadataReader::read(const std::string& filePath)
{
    const QFileInfo info(QString::fromStdString(filePath));

    TrackMetadata metadata;
    metadata.title = info.completeBaseName().toStdString();
    metadata.format = info.suffix().toUpper().toStdString();
    metadata.readStatus = MetadataReadStatus::Partial;
    metadata.encodingStatus = MetadataEncodingStatus::Normal;
    return metadata;
}

} // namespace opusora
