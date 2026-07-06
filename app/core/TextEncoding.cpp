#include "core/TextEncoding.hpp"

#include <QByteArray>
#include <QChar>
#include <QStringList>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <iconv.h>
#include <optional>

namespace {

QString repairMojibakeFlat(const QString& text);

bool isHangul(uint codePoint)
{
    return (codePoint >= 0xAC00 && codePoint <= 0xD7AF)
        || (codePoint >= 0x1100 && codePoint <= 0x11FF)
        || (codePoint >= 0x3130 && codePoint <= 0x318F)
        || (codePoint >= 0xA960 && codePoint <= 0xA97F)
        || (codePoint >= 0xD7B0 && codePoint <= 0xD7FF);
}

bool isCjk(uint codePoint)
{
    return (codePoint >= 0x3400 && codePoint <= 0x4DBF)
        || (codePoint >= 0x4E00 && codePoint <= 0x9FFF)
        || (codePoint >= 0xF900 && codePoint <= 0xFAFF);
}

bool isKana(uint codePoint)
{
    return (codePoint >= 0x3040 && codePoint <= 0x30FF)
        || (codePoint >= 0x31F0 && codePoint <= 0x31FF);
}

bool isHalfwidthKana(uint codePoint)
{
    return codePoint >= 0xFF66 && codePoint <= 0xFF9D;
}

bool isSuspiciousLatin1(uint codePoint)
{
    return codePoint >= 0x00A0 && codePoint <= 0x00FF;
}

bool isDamageMarker(QChar character)
{
    return character == QLatin1Char('?') || character.unicode() == 0xFFFD;
}

int damageMarkerCount(const QString& text)
{
    int count = 0;
    for (const QChar character : text) {
        if (isDamageMarker(character)) {
            ++count;
        }
    }
    return count;
}

bool hasRepeatedQuestionMarkers(const QString& text)
{
    int runLength = 0;
    for (const QChar character : text) {
        if (character == QLatin1Char('?')) {
            ++runLength;
            if (runLength >= 2) {
                return true;
            }
        } else {
            runLength = 0;
        }
    }
    return false;
}

QString withoutDamageMarkers(const QString& text)
{
    QString cleaned;
    cleaned.reserve(text.size());
    for (const QChar character : text) {
        if (!isDamageMarker(character)) {
            cleaned.append(character);
        }
    }
    return cleaned.trimmed();
}

QString latinDigitSignature(const QString& text)
{
    QString signature;
    for (const QChar character : text.toCaseFolded()) {
        const uint codePoint = character.unicode();
        if ((codePoint >= 'a' && codePoint <= 'z') || (codePoint >= '0' && codePoint <= '9')) {
            signature.append(character);
        }
    }
    return signature;
}

int latin1LikeCount(const QString& text)
{
    int count = 0;
    for (const QChar character : text) {
        const uint codePoint = character.unicode();
        if (isSuspiciousLatin1(codePoint) || codePoint == 0x20AC) {
            ++count;
        }
    }
    return count;
}

bool looksLikeLatin1Mojibake(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.size() < 2) {
        return false;
    }

    const int suspicious = latin1LikeCount(trimmed);
    return suspicious >= 2 && suspicious * 5 >= trimmed.size();
}

std::optional<char> windows1252Byte(QChar character)
{
    switch (character.unicode()) {
    case 0x20AC:
        return static_cast<char>(0x80);
    case 0x201A:
        return static_cast<char>(0x82);
    case 0x0192:
        return static_cast<char>(0x83);
    case 0x201E:
        return static_cast<char>(0x84);
    case 0x2026:
        return static_cast<char>(0x85);
    case 0x2020:
        return static_cast<char>(0x86);
    case 0x2021:
        return static_cast<char>(0x87);
    case 0x02C6:
        return static_cast<char>(0x88);
    case 0x2030:
        return static_cast<char>(0x89);
    case 0x0160:
        return static_cast<char>(0x8A);
    case 0x2039:
        return static_cast<char>(0x8B);
    case 0x0152:
        return static_cast<char>(0x8C);
    case 0x017D:
        return static_cast<char>(0x8E);
    case 0x2018:
        return static_cast<char>(0x91);
    case 0x2019:
        return static_cast<char>(0x92);
    case 0x201C:
        return static_cast<char>(0x93);
    case 0x201D:
        return static_cast<char>(0x94);
    case 0x2022:
        return static_cast<char>(0x95);
    case 0x2013:
        return static_cast<char>(0x96);
    case 0x2014:
        return static_cast<char>(0x97);
    case 0x02DC:
        return static_cast<char>(0x98);
    case 0x2122:
        return static_cast<char>(0x99);
    case 0x0161:
        return static_cast<char>(0x9A);
    case 0x203A:
        return static_cast<char>(0x9B);
    case 0x0153:
        return static_cast<char>(0x9C);
    case 0x017E:
        return static_cast<char>(0x9E);
    case 0x0178:
        return static_cast<char>(0x9F);
    default:
        break;
    }
    return std::nullopt;
}

QByteArray recoverSingleByteText(const QString& text)
{
    QByteArray bytes;
    bytes.reserve(text.size());
    for (const QChar character : text) {
        const uint codePoint = character.unicode();
        if (codePoint <= 0x00FF) {
            bytes.append(static_cast<char>(codePoint));
            continue;
        }
        if (const std::optional<char> mapped = windows1252Byte(character)) {
            bytes.append(*mapped);
            continue;
        }
        return {};
    }
    return bytes;
}

std::optional<QString> decodeBytes(const QByteArray& bytes, const char* encoding)
{
    iconv_t converter = iconv_open("UTF-8", encoding);
    if (converter == reinterpret_cast<iconv_t>(-1)) {
        return std::nullopt;
    }

    QByteArray output(std::max<qsizetype>(32, bytes.size() * 8 + 16), Qt::Uninitialized);
    const char* inputData = bytes.constData();
    size_t inputLeft = static_cast<size_t>(bytes.size());
    char* outputData = output.data();
    size_t outputLeft = static_cast<size_t>(output.size());

    while (inputLeft > 0) {
        char* mutableInput = const_cast<char*>(inputData);
        const size_t result = iconv(converter, &mutableInput, &inputLeft, &outputData, &outputLeft);
        inputData = mutableInput;
        if (result != static_cast<size_t>(-1)) {
            continue;
        }

        if (errno == E2BIG) {
            const qsizetype used = outputData - output.data();
            output.resize(output.size() * 2 + 16);
            outputData = output.data() + used;
            outputLeft = static_cast<size_t>(output.size() - used);
            continue;
        }

        iconv_close(converter);
        return std::nullopt;
    }

    output.truncate(outputData - output.data());
    iconv_close(converter);
    return QString::fromUtf8(output);
}

QByteArray trimmedLegacyBytes(const QByteArray& value)
{
    qsizetype begin = 0;
    qsizetype end = value.size();
    while (begin < end && (value.at(begin) == '\0' || value.at(begin) == ' ')) {
        ++begin;
    }
    while (end > begin && (value.at(end - 1) == '\0' || value.at(end - 1) == ' ')) {
        --end;
    }
    return value.mid(begin, end - begin);
}

int scriptCount(const QString& text)
{
    int count = 0;
    for (const QChar character : text) {
        const uint codePoint = character.unicode();
        if (isHangul(codePoint) || isCjk(codePoint) || isKana(codePoint)) {
            ++count;
        }
    }
    return count;
}

int textQualityScore(const QString& text)
{
    int hangul = 0;
    int cjk = 0;
    int kana = 0;
    int ascii = 0;
    int latin1 = 0;
    int controls = 0;
    int halfwidthKana = 0;
    int other = 0;

    for (const QChar character : text) {
        const uint codePoint = character.unicode();
        if (codePoint == 0xFFFD) {
            return -100000;
        }
        if ((codePoint < 0x20 && !character.isSpace()) || (codePoint >= 0x7F && codePoint < 0xA0)) {
            ++controls;
        } else if (isHangul(codePoint)) {
            ++hangul;
        } else if (isCjk(codePoint)) {
            ++cjk;
        } else if (isKana(codePoint)) {
            ++kana;
        } else if (isHalfwidthKana(codePoint)) {
            ++halfwidthKana;
        } else if (codePoint < 0x80) {
            ++ascii;
        } else if (isSuspiciousLatin1(codePoint)) {
            ++latin1;
        } else {
            ++other;
        }
    }

    const int scriptTotal = hangul + cjk + kana;
    const int dominantScript = std::max({ hangul, cjk, kana });
    return hangul * 6
        + cjk * 5
        + kana * 7
        + ascii
        + other
        + dominantScript * 2
        - (scriptTotal - dominantScript) * 4
        - latin1 * 4
        - halfwidthKana * 2
        - controls * 30;
}

QStringList legacyDecodeCandidates(const QString& text)
{
    const QString trimmed = text.trimmed();
    QStringList candidates { trimmed };
    if (!looksLikeLatin1Mojibake(trimmed)) {
        return candidates;
    }

    const QByteArray bytes = recoverSingleByteText(trimmed);
    if (bytes.isEmpty()) {
        return candidates;
    }

    const char* encodings[] = {
        "GB18030",
        "CP949",
        "BIG5",
        "SHIFT_JIS",
    };

    for (const char* encoding : encodings) {
        const std::optional<QString> decoded = decodeBytes(bytes, encoding);
        if (!decoded || decoded->isEmpty() || *decoded == trimmed) {
            continue;
        }

        const QString candidate = decoded->trimmed();
        if (!candidates.contains(candidate)) {
            candidates.append(candidate);
        }
    }

    return candidates;
}

QStringList rawByteDecodeCandidates(const QByteArray& bytes)
{
    QStringList candidates;
    const QByteArray trimmed = trimmedLegacyBytes(bytes);
    if (trimmed.isEmpty()) {
        return candidates;
    }

    const char* encodings[] = {
        "UTF-8",
        "GB18030",
        "CP949",
        "EUC-KR",
        "BIG5",
        "BIG5-HKSCS",
        "SHIFT_JIS",
        "EUC-JP",
        "WINDOWS-1252",
        "ISO-8859-1",
    };

    for (const char* encoding : encodings) {
        const std::optional<QString> decoded = decodeBytes(trimmed, encoding);
        if (!decoded) {
            continue;
        }

        const QString repaired = repairMojibakeFlat(decoded->trimmed());
        if (!repaired.isEmpty() && !candidates.contains(repaired)) {
            candidates.append(repaired);
        }
    }

    return candidates;
}

QString repairMojibakeFlat(const QString& text)
{
    const QString trimmed = text.trimmed();
    const QStringList candidates = legacyDecodeCandidates(trimmed);
    QString best = trimmed;
    int bestScore = textQualityScore(trimmed);
    for (const QString& candidate : candidates) {
        if (candidate == trimmed) {
            continue;
        }

        const int decodedScore = textQualityScore(candidate);
        if (decodedScore > bestScore) {
            best = candidate;
            bestScore = decodedScore;
        }
    }

    const int minimumScriptCount = trimmed.size() <= 2 ? 1 : 2;
    if (best != trimmed
        && scriptCount(best) >= minimumScriptCount
        && latin1LikeCount(best) < latin1LikeCount(trimmed)
        && bestScore >= textQualityScore(trimmed) + 8) {
        return best.trimmed();
    }

    return trimmed;
}

QString dedupeKey(const QString& text)
{
    return text.simplified().toCaseFolded();
}

bool shouldDropDamagedCommaPart(
    const QString& original,
    const QString& repaired,
    const QStringList& normalAnchors)
{
    if (normalAnchors.isEmpty()) {
        return false;
    }
    if (damageMarkerCount(original) < 2 && damageMarkerCount(repaired) < 2) {
        return false;
    }

    const QString cleaned = dedupeKey(withoutDamageMarkers(repaired.isEmpty() ? original : repaired));
    if (cleaned.isEmpty()) {
        return true;
    }

    for (const QString& anchor : normalAnchors) {
        if (anchor.contains(cleaned) || cleaned.contains(anchor)) {
            return true;
        }

        const QString cleanedSignature = latinDigitSignature(cleaned);
        const QString anchorSignature = latinDigitSignature(anchor);
        if (cleanedSignature.size() >= 4
            && (anchorSignature.contains(cleanedSignature) || cleanedSignature.contains(anchorSignature))) {
            return true;
        }
    }

    return false;
}

QString repairCommaSeparatedMojibake(const QString& text, const QString& flatRepair)
{
    const QStringList parts = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return flatRepair;
    }

    QStringList normalAnchors;
    for (const QString& part : parts) {
        const QString original = part.trimmed();
        if (!looksLikeLatin1Mojibake(original) && scriptCount(original) > 0) {
            const QString key = dedupeKey(original);
            if (!normalAnchors.contains(key)) {
                normalAnchors.append(key);
            }
        }
    }

    QStringList repairedParts;
    QStringList seenKeys;
    bool removedDuplicate = false;
    bool improvedSegment = false;
    bool droppedDamaged = false;
    for (const QString& part : parts) {
        const QString original = part.trimmed();
        if (original.isEmpty()) {
            continue;
        }

        QString repaired = repairMojibakeFlat(original);
        if (!normalAnchors.isEmpty()) {
            for (const QString& candidate : legacyDecodeCandidates(original)) {
                if (normalAnchors.contains(dedupeKey(candidate))) {
                    repaired = candidate;
                    break;
                }
            }
        }
        improvedSegment = improvedSegment || repaired != original;

        if (shouldDropDamagedCommaPart(original, repaired, normalAnchors)) {
            droppedDamaged = true;
            continue;
        }

        const QString key = dedupeKey(repaired);
        if (seenKeys.contains(key)) {
            removedDuplicate = true;
            continue;
        }

        seenKeys.append(key);
        repairedParts.append(repaired);
    }

    if (repairedParts.isEmpty()) {
        return flatRepair;
    }

    const QString joined = repairedParts.join(QStringLiteral(", "));
    if (removedDuplicate || droppedDamaged) {
        return joined.trimmed();
    }

    if (improvedSegment
        && latin1LikeCount(joined) < latin1LikeCount(flatRepair)
        && textQualityScore(joined) >= textQualityScore(flatRepair) + 8) {
        return joined.trimmed();
    }

    return flatRepair;
}

} // namespace

namespace opusora {

QString repairMojibake(const QString& text)
{
    const QString trimmed = text.trimmed();
    const QString flatRepair = repairMojibakeFlat(trimmed);
    return trimmed.contains(QLatin1Char(','))
        ? repairCommaSeparatedMojibake(trimmed, flatRepair)
        : flatRepair;
}

QString metadataStringFromUtf8(const std::string& value)
{
    return repairMojibake(QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())));
}

QString metadataStringFromLegacyBytes(const QByteArray& value)
{
    const QStringList candidates = rawByteDecodeCandidates(value);
    if (candidates.isEmpty()) {
        return {};
    }

    QString best = candidates.first();
    int bestScore = textQualityScore(best);
    for (const QString& candidate : candidates) {
        const int score = textQualityScore(candidate);
        if (score > bestScore) {
            best = candidate;
            bestScore = score;
        }
    }
    return best.trimmed();
}

bool hasLikelyEncodingDamage(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    if (trimmed.contains(QChar(0xFFFD))) {
        return true;
    }

    const int markers = damageMarkerCount(trimmed);
    if (markers <= 0 || !hasRepeatedQuestionMarkers(trimmed)) {
        return false;
    }

    const QString cleaned = withoutDamageMarkers(trimmed);
    if (cleaned.isEmpty()) {
        return true;
    }

    return markers >= 3 && scriptCount(trimmed) == 0 && markers * 2 >= trimmed.size();
}

} // namespace opusora
