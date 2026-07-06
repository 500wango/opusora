#pragma once

#include <QByteArray>
#include <QString>

#include <string>

namespace opusora {

QString repairMojibake(const QString& text);
QString metadataStringFromUtf8(const std::string& value);
QString metadataStringFromLegacyBytes(const QByteArray& value);
bool hasLikelyEncodingDamage(const QString& text);

} // namespace opusora
