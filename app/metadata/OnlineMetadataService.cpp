#include "metadata/OnlineMetadataService.hpp"

#include "db/Database.hpp"
#include "library/LibraryRepository.hpp"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <utility>

namespace opusora {

namespace {

constexpr int kRequestTimeoutMs = 20000;

QByteArray userAgent()
{
    return QByteArrayLiteral("Opusora/0.1.0 (https://opusora.local; user-triggered metadata lookup)");
}

QString cleanText(const QString& value)
{
    QString text = value.trimmed();
    text.replace(QChar(0x2018), QLatin1Char('\''));
    text.replace(QChar(0x2019), QLatin1Char('\''));
    text.replace(QChar(0x201C), QLatin1Char('"'));
    text.replace(QChar(0x201D), QLatin1Char('"'));
    return text.simplified();
}

QString comparableText(const QString& value)
{
    QString text = cleanText(value).toCaseFolded();
    text.remove(QChar(0x200E));
    text.remove(QChar(0x200F));
    return text;
}

QString lyricsTitleWithoutVersionText(const QString& value)
{
    QString title = cleanText(value);
    if (title.isEmpty()) {
        return title;
    }

    title.remove(QRegularExpression(QStringLiteral("\\s*[\\(（\\[][\\s\\S]*?[\\)）\\]]\\s*")));
    title = cleanText(title);

    static const QRegularExpression trailingVersion(
        QStringLiteral("\\s+[-–—]\\s*(tv\\s*size|tvサイズ|off\\s*vocal|instrumental|inst\\.?|karaoke|カラオケ|オフボーカル|version|ver\\.?|edit|remaster(?:ed)?)\\b.*$"),
        QRegularExpression::CaseInsensitiveOption);
    title.remove(trailingVersion);

    return cleanText(title);
}

QStringList lyricsTitleVariants(const QString& value)
{
    QStringList variants;
    const QString title = cleanText(value);
    if (!title.isEmpty()) {
        variants.append(title);
    }

    const QString withoutVersionText = lyricsTitleWithoutVersionText(title);
    if (!withoutVersionText.isEmpty() && !variants.contains(withoutVersionText)) {
        variants.append(withoutVersionText);
    }

    return variants;
}

QString escapedMusicBrainzTerm(const QString& value)
{
    QString escaped = cleanText(value);
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return escaped;
}

QString lyricsFromObject(const QJsonObject& object)
{
    const QString synced = object.value(QStringLiteral("syncedLyrics")).toString().trimmed();
    if (!synced.isEmpty()) {
        return synced;
    }

    return object.value(QStringLiteral("plainLyrics")).toString().trimmed();
}

struct LyricsMatch {
    QString lyrics;
    QString title;
    QString artist;
    QString album;
    int score = 0;
};

struct LyricsLookupCandidate {
    QUrl url;
    QString title;
    QString artist;
    QString album;
    int score = 0;
};

struct MetadataJobResult {
    int trackId = 0;
    QString title;
    QString artist;
    QString album;
    QString errorMessage;
};

LyricsMatch lyricsMatchFromObject(
    const QJsonObject& object,
    const QString& queryTitle,
    const QString& queryArtist,
    const QString& queryAlbum,
    int queryDurationMs)
{
    LyricsMatch match;
    match.lyrics = lyricsFromObject(object);
    if (match.lyrics.isEmpty()) {
        return match;
    }

    match.title = cleanText(object.value(QStringLiteral("trackName")).toString());
    match.artist = cleanText(object.value(QStringLiteral("artistName")).toString());
    match.album = cleanText(object.value(QStringLiteral("albumName")).toString());
    match.score = object.value(QStringLiteral("syncedLyrics")).toString().trimmed().isEmpty() ? 4 : 8;

    const QString normalizedTitle = comparableText(match.title);
    const QString normalizedArtist = comparableText(match.artist);
    const QString normalizedAlbum = comparableText(match.album);
    const QString requestedTitle = comparableText(queryTitle);
    const QString requestedArtist = comparableText(queryArtist);
    const QString requestedAlbum = comparableText(queryAlbum);

    if (!requestedTitle.isEmpty() && normalizedTitle == requestedTitle) {
        match.score += 20;
    } else if (!requestedTitle.isEmpty() && normalizedTitle.contains(requestedTitle)) {
        match.score += 8;
    }

    if (!requestedArtist.isEmpty() && normalizedArtist == requestedArtist) {
        match.score += 14;
    } else if (!requestedArtist.isEmpty() && normalizedArtist.contains(requestedArtist)) {
        match.score += 6;
    }

    if (!requestedAlbum.isEmpty() && normalizedAlbum == requestedAlbum) {
        match.score += 6;
    }

    if (queryDurationMs > 0) {
        const int durationMs = static_cast<int>(std::llround(object.value(QStringLiteral("duration")).toDouble() * 1000.0));
        if (durationMs > 0 && std::abs(durationMs - queryDurationMs) <= 2500) {
            match.score += 8;
        }
    }

    return match;
}

LyricsMatch bestLyricsMatch(
    const QJsonDocument& document,
    const QString& queryTitle,
    const QString& queryArtist,
    const QString& queryAlbum,
    int queryDurationMs)
{
    if (document.isObject()) {
        return lyricsMatchFromObject(document.object(), queryTitle, queryArtist, queryAlbum, queryDurationMs);
    }

    LyricsMatch best;
    if (!document.isArray()) {
        return best;
    }

    const QJsonArray array = document.array();
    for (const QJsonValue& value : array) {
        const LyricsMatch candidate = lyricsMatchFromObject(value.toObject(), queryTitle, queryArtist, queryAlbum, queryDurationMs);
        if (!candidate.lyrics.isEmpty() && candidate.score > best.score) {
            best = candidate;
        }
    }

    return best;
}

QString joinedSimpleArtists(const QJsonArray& artists, const QString& nameKey = QStringLiteral("name"))
{
    QStringList names;
    for (const QJsonValue& value : artists) {
        const QString name = cleanText(value.toObject().value(nameKey).toString());
        if (!name.isEmpty()) {
            names.append(name);
        }
    }
    return names.join(QStringLiteral(" / "));
}

int lyricsLookupScore(
    const QString& title,
    const QString& artist,
    const QString& album,
    int durationMs,
    const QStringList& requestedTitles,
    const QString& requestedArtist,
    const QString& requestedAlbum,
    int requestedDurationMs)
{
    int score = 0;
    const QString normalizedTitle = comparableText(title);
    for (const QString& requestedTitle : requestedTitles) {
        const QString normalizedRequestedTitle = comparableText(requestedTitle);
        if (normalizedRequestedTitle.isEmpty() || normalizedTitle.isEmpty()) {
            continue;
        }
        if (normalizedTitle == normalizedRequestedTitle) {
            score = std::max(score, 35);
        } else if (normalizedTitle.contains(normalizedRequestedTitle)
            || normalizedRequestedTitle.contains(normalizedTitle)) {
            score = std::max(score, 15);
        }
    }

    const QString normalizedArtist = comparableText(artist);
    const QString normalizedRequestedArtist = comparableText(requestedArtist);
    if (!normalizedRequestedArtist.isEmpty() && !normalizedArtist.isEmpty()) {
        if (normalizedArtist == normalizedRequestedArtist) {
            score += 18;
        } else if (normalizedArtist.contains(normalizedRequestedArtist)
            || normalizedRequestedArtist.contains(normalizedArtist)) {
            score += 8;
        }
    }

    const QString normalizedAlbum = comparableText(album);
    const QString normalizedRequestedAlbum = comparableText(requestedAlbum);
    if (!normalizedRequestedAlbum.isEmpty() && !normalizedAlbum.isEmpty()
        && normalizedAlbum == normalizedRequestedAlbum) {
        score += 6;
    }

    if (requestedDurationMs > 0 && durationMs > 0) {
        const int delta = std::abs(durationMs - requestedDurationMs);
        if (delta <= 3000) {
            score += 8;
        } else if (delta <= 8000) {
            score += 4;
        }
    }

    return score;
}

QUrl neteaseLyricsSearchUrl(const QString& keyword)
{
    QUrl url(QStringLiteral("https://music.163.com/api/search/get"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("s"), keyword);
    query.addQueryItem(QStringLiteral("type"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("5"));
    query.addQueryItem(QStringLiteral("offset"), QStringLiteral("0"));
    url.setQuery(query);
    return url;
}

QUrl neteaseLyricsUrl(qint64 songId)
{
    QUrl url(QStringLiteral("https://music.163.com/api/song/lyric"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("id"), QString::number(songId));
    query.addQueryItem(QStringLiteral("lv"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("kv"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("tv"), QStringLiteral("-1"));
    url.setQuery(query);
    return url;
}

LyricsLookupCandidate bestNeteaseLyricsCandidate(
    const QJsonDocument& document,
    const QStringList& requestedTitles,
    const QString& requestedArtist,
    const QString& requestedAlbum,
    int requestedDurationMs)
{
    LyricsLookupCandidate best;
    const QJsonArray songs = document.object().value(QStringLiteral("result")).toObject().value(QStringLiteral("songs")).toArray();
    for (int index = 0; index < songs.size(); ++index) {
        const QJsonObject song = songs.at(index).toObject();
        const qint64 songId = static_cast<qint64>(song.value(QStringLiteral("id")).toDouble());
        if (songId <= 0) {
            continue;
        }

        const QString title = cleanText(song.value(QStringLiteral("name")).toString());
        const QString artist = joinedSimpleArtists(song.value(QStringLiteral("artists")).toArray());
        const QString album = cleanText(song.value(QStringLiteral("album")).toObject().value(QStringLiteral("name")).toString());
        const int durationMs = song.value(QStringLiteral("duration")).toInt();
        int score = lyricsLookupScore(
            title,
            artist,
            album,
            durationMs,
            requestedTitles,
            requestedArtist,
            requestedAlbum,
            requestedDurationMs);
        score += std::max(0, 5 - index);

        if (score > best.score) {
            best.url = neteaseLyricsUrl(songId);
            best.title = title;
            best.artist = artist;
            best.album = album;
            best.score = score;
        }
    }

    return best.score >= 25 ? best : LyricsLookupCandidate();
}

QString lyricsFromNeteaseDocument(const QJsonDocument& document)
{
    const QString lyric = document.object()
                              .value(QStringLiteral("lrc"))
                              .toObject()
                              .value(QStringLiteral("lyric"))
                              .toString()
                              .trimmed();
    if (lyric.isEmpty() || lyric.contains(QStringLiteral("纯音乐"))) {
        return {};
    }

    return lyric;
}

QUrl kugouLyricsSearchUrl(const QString& keyword, int durationMs)
{
    QUrl url(QStringLiteral("https://lyrics.kugou.com/search"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("ver"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("man"), QStringLiteral("yes"));
    query.addQueryItem(QStringLiteral("client"), QStringLiteral("pc"));
    query.addQueryItem(QStringLiteral("keyword"), keyword);
    if (durationMs > 0) {
        query.addQueryItem(QStringLiteral("duration"), QString::number(durationMs));
    }
    query.addQueryItem(QStringLiteral("hash"), QString());
    url.setQuery(query);
    return url;
}

QUrl kugouLyricsDownloadUrl(const QString& id, const QString& accessKey)
{
    QUrl url(QStringLiteral("https://lyrics.kugou.com/download"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("ver"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("client"), QStringLiteral("pc"));
    query.addQueryItem(QStringLiteral("id"), id);
    query.addQueryItem(QStringLiteral("accesskey"), accessKey);
    query.addQueryItem(QStringLiteral("fmt"), QStringLiteral("lrc"));
    query.addQueryItem(QStringLiteral("charset"), QStringLiteral("utf8"));
    url.setQuery(query);
    return url;
}

LyricsLookupCandidate bestKugouLyricsCandidate(
    const QJsonDocument& document,
    const QStringList& requestedTitles,
    const QString& requestedArtist,
    int requestedDurationMs)
{
    LyricsLookupCandidate best;
    const QJsonArray candidates = document.object().value(QStringLiteral("candidates")).toArray();
    for (int index = 0; index < candidates.size(); ++index) {
        const QJsonObject candidate = candidates.at(index).toObject();
        const QString id = cleanText(candidate.value(QStringLiteral("id")).toVariant().toString());
        const QString accessKey = cleanText(candidate.value(QStringLiteral("accesskey")).toString());
        if (id.isEmpty() || accessKey.isEmpty()) {
            continue;
        }

        const QString title = cleanText(candidate.value(QStringLiteral("song")).toString());
        const QString artist = cleanText(candidate.value(QStringLiteral("singer")).toString());
        const int durationMs = candidate.value(QStringLiteral("duration")).toInt();
        int score = lyricsLookupScore(
            title,
            artist,
            {},
            durationMs,
            requestedTitles,
            requestedArtist,
            {},
            requestedDurationMs);
        score += std::max(0, 5 - index);

        if (score > best.score) {
            best.url = kugouLyricsDownloadUrl(id, accessKey);
            best.title = title;
            best.artist = artist;
            best.score = score;
        }
    }

    return best.score >= 25 ? best : LyricsLookupCandidate();
}

QString lyricsFromKugouDocument(const QJsonDocument& document)
{
    const QByteArray encoded = document.object().value(QStringLiteral("content")).toString().toUtf8();
    if (encoded.isEmpty()) {
        return {};
    }

    return QString::fromUtf8(QByteArray::fromBase64(encoded)).trimmed();
}

QString joinedArtistCredit(const QJsonArray& credits)
{
    QStringList artists;
    QString pendingJoinPhrase;
    for (const QJsonValue& value : credits) {
        const QJsonObject credit = value.toObject();
        QString name = cleanText(credit.value(QStringLiteral("name")).toString());
        if (name.isEmpty()) {
            name = cleanText(credit.value(QStringLiteral("artist")).toObject().value(QStringLiteral("name")).toString());
        }
        if (name.isEmpty()) {
            continue;
        }

        if (!pendingJoinPhrase.isEmpty() && !artists.isEmpty()) {
            artists.last().append(pendingJoinPhrase.trimmed().isEmpty() ? QStringLiteral(" ") : pendingJoinPhrase);
        }
        artists.append(name);
        pendingJoinPhrase = credit.value(QStringLiteral("joinphrase")).toString();
    }

    return artists.join(QString()).trimmed();
}

QString preferredReleaseTitle(const QJsonArray& releases, const QString& requestedAlbum)
{
    QString fallback;
    const QString normalizedRequestedAlbum = comparableText(requestedAlbum);

    for (const QJsonValue& value : releases) {
        const QJsonObject release = value.toObject();
        const QString title = cleanText(release.value(QStringLiteral("title")).toString());
        if (title.isEmpty()) {
            continue;
        }
        if (fallback.isEmpty()) {
            fallback = title;
        }
        if (!normalizedRequestedAlbum.isEmpty() && comparableText(title) == normalizedRequestedAlbum) {
            return title;
        }
        const QString status = release.value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("Official")) {
            fallback = title;
        }
    }

    return fallback;
}

QVariantMap metadataFromMusicBrainzDocument(
    const QJsonDocument& document,
    const QString& requestedTitle,
    const QString& requestedArtist,
    const QString& requestedAlbum,
    int requestedDurationMs)
{
    QVariantMap best;
    int bestScore = -1;

    const QJsonArray recordings = document.object().value(QStringLiteral("recordings")).toArray();
    for (const QJsonValue& value : recordings) {
        const QJsonObject recording = value.toObject();
        const QString title = cleanText(recording.value(QStringLiteral("title")).toString());
        const QString artist = cleanText(joinedArtistCredit(recording.value(QStringLiteral("artist-credit")).toArray()));
        const QString album = cleanText(preferredReleaseTitle(recording.value(QStringLiteral("releases")).toArray(), requestedAlbum));
        if (title.isEmpty() && artist.isEmpty()) {
            continue;
        }

        int score = recording.value(QStringLiteral("score")).toInt(-1);
        if (score < 0) {
            score = 0;
        }

        const QString normalizedTitle = comparableText(title);
        const QString normalizedArtist = comparableText(artist);
        const QString queryTitle = comparableText(requestedTitle);
        const QString queryArtist = comparableText(requestedArtist);
        if (!queryTitle.isEmpty() && normalizedTitle == queryTitle) {
            score += 12;
        }
        if (!queryArtist.isEmpty() && normalizedArtist == queryArtist) {
            score += 10;
        }
        if (!requestedAlbum.trimmed().isEmpty() && comparableText(album) == comparableText(requestedAlbum)) {
            score += 5;
        }
        if (requestedDurationMs > 0) {
            const int length = recording.value(QStringLiteral("length")).toInt();
            if (length > 0 && std::abs(length - requestedDurationMs) <= 3000) {
                score += 5;
            }
        }

        if (score > bestScore) {
            bestScore = score;
            best.insert(QStringLiteral("title"), title);
            best.insert(QStringLiteral("artist"), artist);
            best.insert(QStringLiteral("album"), album);
            best.insert(QStringLiteral("score"), score);
        }
    }

    if (bestScore < 60) {
        return {};
    }

    return best;
}

QJsonDocument jsonDocumentFromPayload(const QByteArray& payload)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    return error.error == QJsonParseError::NoError ? document : QJsonDocument();
}

QUrl musicBrainzLookupUrl(const QString& title, const QString& artist)
{
    QString queryText = QStringLiteral("recording:\"%1\"").arg(escapedMusicBrainzTerm(title));
    if (!artist.isEmpty()) {
        queryText += QStringLiteral(" AND artist:\"%1\"").arg(escapedMusicBrainzTerm(artist));
    }

    QUrl url(QStringLiteral("https://musicbrainz.org/ws/2/recording/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("query"), queryText);
    query.addQueryItem(QStringLiteral("fmt"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("inc"), QStringLiteral("artist-credits+releases"));
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("5"));
    url.setQuery(query);
    return url;
}

MetadataJobResult runMetadataJob(
    int trackId,
    const QString& requestedTitle,
    const QString& requestedArtist,
    const QString& requestedAlbum,
    int durationMs,
    const QString& databasePath)
{
    MetadataJobResult result;
    result.trackId = trackId;

    if (databasePath.trimmed().isEmpty()) {
        result.errorMessage = QStringLiteral("online.error.database");
        return result;
    }

    QNetworkAccessManager network;
    QNetworkRequest request(musicBrainzLookupUrl(requestedTitle, requestedArtist));
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(userAgent()));
    request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
    request.setTransferTimeout(kRequestTimeoutMs);

    QNetworkReply* reply = network.get(request);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, reply, [reply, &loop]() {
        if (reply->isRunning()) {
            reply->abort();
        }
        loop.quit();
    });
    timeout.start(kRequestTimeoutMs + 1000);
    loop.exec();

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray payload = reply->readAll();
    const bool networkOk = reply->error() == QNetworkReply::NoError;
    delete reply;

    if (!networkOk) {
        result.errorMessage = QStringLiteral("online.error.network");
        return result;
    }
    if (httpStatus < 200 || httpStatus >= 300) {
        result.errorMessage = QStringLiteral("online.error.noMatch");
        return result;
    }

    const QVariantMap metadata = metadataFromMusicBrainzDocument(
        jsonDocumentFromPayload(payload),
        requestedTitle,
        requestedArtist,
        requestedAlbum,
        durationMs);
    const QString matchedTitle = metadata.value(QStringLiteral("title")).toString().trimmed();
    const QString matchedArtist = metadata.value(QStringLiteral("artist")).toString().trimmed();
    const QString matchedAlbum = metadata.value(QStringLiteral("album")).toString().trimmed();
    if (matchedTitle.isEmpty() && matchedArtist.isEmpty() && matchedAlbum.isEmpty()) {
        result.errorMessage = QStringLiteral("online.error.noMatch");
        return result;
    }

    result.title = matchedTitle.isEmpty() ? requestedTitle : matchedTitle;
    result.artist = matchedArtist.isEmpty() ? requestedArtist : matchedArtist;
    result.album = matchedAlbum;

    Database database(databasePath);
    if (!database.open()) {
        result.errorMessage = QStringLiteral("online.error.database");
        return result;
    }

    LibraryRepository repository(&database);
    const QString existingLyrics = repository.lyricsForTrack(trackId);
    if (!repository.updateTrackDetails(trackId, result.title, result.artist, result.album, existingLyrics)) {
        result.errorMessage = QStringLiteral("online.error.database");
    }

    return result;
}

} // namespace

OnlineMetadataService::OnlineMetadataService(QObject* parent)
    : OnlineMetadataService(QString(), parent)
{
}

OnlineMetadataService::OnlineMetadataService(QString databasePath, QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_databasePath(std::move(databasePath))
{
}

bool OnlineMetadataService::busy() const
{
    return m_busy;
}

QString OnlineMetadataService::status() const
{
    return m_status;
}

QString OnlineMetadataService::errorMessage() const
{
    return m_errorMessage;
}

void OnlineMetadataService::fetchLyrics(int trackId, const QString& title, const QString& artist, const QString& album, int durationMs)
{
    if (m_busy) {
        setErrorMessage(QStringLiteral("online.error.busy"));
        return;
    }

    m_trackId = trackId;
    m_title = cleanText(title);
    m_artist = cleanText(artist);
    m_album = cleanText(album);
    m_durationMs = std::max(0, durationMs);
    m_lyricsSearchQueue.clear();
    m_neteaseLyricsSearchQueue.clear();
    m_kugouLyricsSearchQueue.clear();
    setErrorMessage({});

    if (m_trackId <= 0 || m_title.isEmpty()) {
        finishWithError(QStringLiteral("online.error.missingTitle"));
        return;
    }

    m_lyricsBusy = true;
    refreshBusy();
    setStatus(QStringLiteral("online.status.fetchingLyrics"));

    if (!m_artist.isEmpty() && !m_album.isEmpty() && m_durationMs > 0) {
        QUrl url(QStringLiteral("https://lrclib.net/api/get"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("track_name"), m_title);
        query.addQueryItem(QStringLiteral("artist_name"), m_artist);
        query.addQueryItem(QStringLiteral("album_name"), m_album);
        query.addQueryItem(QStringLiteral("duration"), QString::number(static_cast<int>(std::llround(m_durationMs / 1000.0))));
        url.setQuery(query);
        sendGet(url, RequestKind::LyricsExact);
        return;
    }

    queueLyricsSearches();
    startNextLyricsSearch();
}

void OnlineMetadataService::fetchMetadata(int trackId, const QString& title, const QString& artist, const QString& album, int durationMs)
{
    MetadataJob job;
    job.trackId = trackId;
    job.title = cleanText(title);
    job.artist = cleanText(artist);
    job.album = cleanText(album);
    job.durationMs = std::max(0, durationMs);
    setErrorMessage({});

    if (job.trackId <= 0 || job.title.isEmpty()) {
        setStatus({});
        setErrorMessage(QStringLiteral("online.error.missingTitle"));
        return;
    }

    m_metadataQueue.enqueue(job);
    setStatus(QStringLiteral("online.status.fetchingMetadata"));
    refreshBusy();
    startNextMetadataJob();
}

void OnlineMetadataService::clearMessages()
{
    setStatus({});
    setErrorMessage({});
}

QString OnlineMetadataService::lyricsFromLrclibJsonForTest(const QByteArray& payload)
{
    const QJsonDocument document = jsonDocumentFromPayload(payload);
    return bestLyricsMatch(document, {}, {}, {}, 0).lyrics;
}

QString OnlineMetadataService::lyricsFromNeteaseJsonForTest(const QByteArray& payload)
{
    return lyricsFromNeteaseDocument(jsonDocumentFromPayload(payload));
}

QString OnlineMetadataService::lyricsFromKugouJsonForTest(const QByteArray& payload)
{
    return lyricsFromKugouDocument(jsonDocumentFromPayload(payload));
}

QVariantMap OnlineMetadataService::metadataFromMusicBrainzJsonForTest(const QByteArray& payload)
{
    const QJsonDocument document = jsonDocumentFromPayload(payload);
    return metadataFromMusicBrainzDocument(document, {}, {}, {}, 0);
}

void OnlineMetadataService::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }

    m_busy = busy;
    emit busyChanged();
}

void OnlineMetadataService::setStatus(const QString& status)
{
    if (m_status == status) {
        return;
    }

    m_status = status;
    emit statusChanged();
}

void OnlineMetadataService::setErrorMessage(const QString& errorMessage)
{
    if (m_errorMessage == errorMessage) {
        return;
    }

    m_errorMessage = errorMessage;
    emit errorMessageChanged();
}

void OnlineMetadataService::queueLyricsSearches()
{
    m_lyricsSearchQueue.clear();

    QSet<QString> seenUrls;
    const auto addSearch = [&](const QString& title, bool includeArtist) {
        if (title.trimmed().isEmpty()) {
            return;
        }

        QUrl url(QStringLiteral("https://lrclib.net/api/search"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("track_name"), title);
        if (includeArtist && !m_artist.isEmpty()) {
            query.addQueryItem(QStringLiteral("artist_name"), m_artist);
        }
        url.setQuery(query);

        const QString key = url.toString(QUrl::FullyEncoded);
        if (seenUrls.contains(key)) {
            return;
        }

        seenUrls.insert(key);
        m_lyricsSearchQueue.enqueue(url);
    };

    for (const QString& title : lyricsTitleVariants(m_title)) {
        addSearch(title, true);
        addSearch(title, false);
    }
}

void OnlineMetadataService::startNextLyricsSearch()
{
    if (m_lyricsSearchQueue.isEmpty()) {
        queueNeteaseLyricsSearches();
        startNextNeteaseLyricsSearch();
        return;
    }

    sendGet(m_lyricsSearchQueue.dequeue(), RequestKind::LyricsSearch);
}

void OnlineMetadataService::queueNeteaseLyricsSearches()
{
    m_neteaseLyricsSearchQueue.clear();

    QSet<QString> seenUrls;
    const auto addSearch = [&](const QString& keyword) {
        const QString cleaned = cleanText(keyword);
        if (cleaned.isEmpty()) {
            return;
        }

        const QUrl url = neteaseLyricsSearchUrl(cleaned);
        const QString key = url.toString(QUrl::FullyEncoded);
        if (seenUrls.contains(key)) {
            return;
        }

        seenUrls.insert(key);
        m_neteaseLyricsSearchQueue.enqueue(url);
    };

    for (const QString& title : lyricsTitleVariants(m_title)) {
        if (!m_artist.isEmpty()) {
            addSearch(m_artist + QLatin1Char(' ') + title);
        }
        addSearch(title);
    }
}

void OnlineMetadataService::startNextNeteaseLyricsSearch()
{
    if (m_neteaseLyricsSearchQueue.isEmpty()) {
        queueKugouLyricsSearches();
        startNextKugouLyricsSearch();
        return;
    }

    sendGet(m_neteaseLyricsSearchQueue.dequeue(), RequestKind::NeteaseLyricsSearch);
}

void OnlineMetadataService::queueKugouLyricsSearches()
{
    m_kugouLyricsSearchQueue.clear();

    QSet<QString> seenUrls;
    const auto addSearch = [&](const QString& keyword) {
        const QString cleaned = cleanText(keyword);
        if (cleaned.isEmpty()) {
            return;
        }

        const QUrl url = kugouLyricsSearchUrl(cleaned, m_durationMs);
        const QString key = url.toString(QUrl::FullyEncoded);
        if (seenUrls.contains(key)) {
            return;
        }

        seenUrls.insert(key);
        m_kugouLyricsSearchQueue.enqueue(url);
    };

    for (const QString& title : lyricsTitleVariants(m_title)) {
        if (!m_artist.isEmpty()) {
            addSearch(m_artist + QStringLiteral(" - ") + title);
            addSearch(m_artist + QLatin1Char(' ') + title);
        }
        addSearch(title);
    }
}

void OnlineMetadataService::startNextKugouLyricsSearch()
{
    if (m_kugouLyricsSearchQueue.isEmpty()) {
        finishLyricsWithoutMatch();
        return;
    }

    sendGet(m_kugouLyricsSearchQueue.dequeue(), RequestKind::KugouLyricsSearch);
}

void OnlineMetadataService::sendGet(const QUrl& url, RequestKind kind)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(userAgent()));
    request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
    request.setTransferTimeout(kRequestTimeoutMs);

    QNetworkReply* reply = m_network->get(request);
    reply->setProperty("kind", static_cast<int>(kind));
    m_currentReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleReply(reply);
    });
}

void OnlineMetadataService::handleReply(QNetworkReply* reply)
{
    if (!reply || reply != m_currentReply) {
        if (reply) {
            reply->deleteLater();
        }
        return;
    }

    const RequestKind kind = static_cast<RequestKind>(reply->property("kind").toInt());
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray payload = reply->readAll();
    const bool networkOk = reply->error() == QNetworkReply::NoError;
    reply->deleteLater();
    m_currentReply = nullptr;

    if (!networkOk && !(kind == RequestKind::LyricsExact && httpStatus == 404)) {
        switch (kind) {
        case RequestKind::LyricsExact:
            queueLyricsSearches();
            startNextLyricsSearch();
            break;
        case RequestKind::LyricsSearch:
            startNextLyricsSearch();
            break;
        case RequestKind::NeteaseLyricsSearch:
        case RequestKind::NeteaseLyrics:
            startNextNeteaseLyricsSearch();
            break;
        case RequestKind::KugouLyricsSearch:
        case RequestKind::KugouLyrics:
            startNextKugouLyricsSearch();
            break;
        }
        return;
    }

    switch (kind) {
    case RequestKind::LyricsExact:
        handleLyricsExactReply(payload, httpStatus);
        break;
    case RequestKind::LyricsSearch:
        handleLyricsSearchReply(payload, httpStatus);
        break;
    case RequestKind::NeteaseLyricsSearch:
        handleNeteaseLyricsSearchReply(payload, httpStatus);
        break;
    case RequestKind::NeteaseLyrics:
        handleNeteaseLyricsReply(payload, httpStatus);
        break;
    case RequestKind::KugouLyricsSearch:
        handleKugouLyricsSearchReply(payload, httpStatus);
        break;
    case RequestKind::KugouLyrics:
        handleKugouLyricsReply(payload, httpStatus);
        break;
    }
}

void OnlineMetadataService::handleLyricsExactReply(const QByteArray& payload, int httpStatus)
{
    if (httpStatus >= 200 && httpStatus < 300) {
        const LyricsMatch match = bestLyricsMatch(jsonDocumentFromPayload(payload), m_title, m_artist, m_album, m_durationMs);
        if (!match.lyrics.isEmpty()) {
            setStatus(QStringLiteral("online.status.lyricsUpdated"));
            emit lyricsFetched(m_trackId, match.lyrics, match.title, match.artist, match.album);
            finishRequest();
            return;
        }
    }

    if (m_lyricsSearchQueue.isEmpty()) {
        queueLyricsSearches();
    }
    startNextLyricsSearch();
}

void OnlineMetadataService::handleLyricsSearchReply(const QByteArray& payload, int httpStatus)
{
    if (httpStatus < 200 || httpStatus >= 300) {
        startNextLyricsSearch();
        return;
    }

    const LyricsMatch match = bestLyricsMatch(jsonDocumentFromPayload(payload), m_title, m_artist, m_album, m_durationMs);
    if (match.lyrics.isEmpty()) {
        startNextLyricsSearch();
        return;
    }

    setStatus(QStringLiteral("online.status.lyricsUpdated"));
    emit lyricsFetched(m_trackId, match.lyrics, match.title, match.artist, match.album);
    finishRequest();
}

void OnlineMetadataService::handleNeteaseLyricsSearchReply(const QByteArray& payload, int httpStatus)
{
    if (httpStatus < 200 || httpStatus >= 300) {
        startNextNeteaseLyricsSearch();
        return;
    }

    const LyricsLookupCandidate candidate = bestNeteaseLyricsCandidate(
        jsonDocumentFromPayload(payload),
        lyricsTitleVariants(m_title),
        m_artist,
        m_album,
        m_durationMs);
    if (!candidate.url.isValid()) {
        startNextNeteaseLyricsSearch();
        return;
    }

    sendGet(candidate.url, RequestKind::NeteaseLyrics);
}

void OnlineMetadataService::handleNeteaseLyricsReply(const QByteArray& payload, int httpStatus)
{
    if (httpStatus < 200 || httpStatus >= 300) {
        startNextNeteaseLyricsSearch();
        return;
    }

    const QString lyrics = lyricsFromNeteaseDocument(jsonDocumentFromPayload(payload));
    if (lyrics.isEmpty()) {
        startNextNeteaseLyricsSearch();
        return;
    }

    setStatus(QStringLiteral("online.status.lyricsUpdated"));
    emit lyricsFetched(m_trackId, lyrics, m_title, m_artist, m_album);
    finishRequest();
}

void OnlineMetadataService::handleKugouLyricsSearchReply(const QByteArray& payload, int httpStatus)
{
    if (httpStatus < 200 || httpStatus >= 300) {
        startNextKugouLyricsSearch();
        return;
    }

    const LyricsLookupCandidate candidate = bestKugouLyricsCandidate(
        jsonDocumentFromPayload(payload),
        lyricsTitleVariants(m_title),
        m_artist,
        m_durationMs);
    if (!candidate.url.isValid()) {
        startNextKugouLyricsSearch();
        return;
    }

    sendGet(candidate.url, RequestKind::KugouLyrics);
}

void OnlineMetadataService::handleKugouLyricsReply(const QByteArray& payload, int httpStatus)
{
    if (httpStatus < 200 || httpStatus >= 300) {
        startNextKugouLyricsSearch();
        return;
    }

    const QString lyrics = lyricsFromKugouDocument(jsonDocumentFromPayload(payload));
    if (lyrics.isEmpty()) {
        startNextKugouLyricsSearch();
        return;
    }

    setStatus(QStringLiteral("online.status.lyricsUpdated"));
    emit lyricsFetched(m_trackId, lyrics, m_title, m_artist, m_album);
    finishRequest();
}

void OnlineMetadataService::finishLyricsWithoutMatch()
{
    finishWithError(QStringLiteral("online.error.noMatch"));
}

void OnlineMetadataService::startNextMetadataJob()
{
    if (m_metadataActive || m_metadataQueue.isEmpty()) {
        refreshBusy();
        return;
    }

    const MetadataJob job = m_metadataQueue.dequeue();
    m_metadataActive = true;
    setErrorMessage({});
    setStatus(QStringLiteral("online.status.fetchingMetadata"));
    refreshBusy();

    const QString databasePath = m_databasePath;
    const QPointer<OnlineMetadataService> self(this);
    QThread* thread = QThread::create([self, job, databasePath]() {
        const MetadataJobResult result = runMetadataJob(
            job.trackId,
            job.title,
            job.artist,
            job.album,
            job.durationMs,
            databasePath);
        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self) {
                    return;
                }

                self->finishMetadataJob(
                    result.trackId,
                    result.title,
                    result.artist,
                    result.album,
                    result.errorMessage);
            },
            Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void OnlineMetadataService::finishMetadataJob(
    int trackId,
    const QString& title,
    const QString& artist,
    const QString& album,
    const QString& errorMessage)
{
    m_metadataActive = false;

    if (errorMessage.isEmpty()) {
        setErrorMessage({});
        setStatus(QStringLiteral("online.status.metadataUpdated"));
        emit metadataFetched(trackId, title, artist, album);
    } else {
        setErrorMessage(errorMessage);
        setStatus({});
    }

    if (!m_metadataQueue.isEmpty()) {
        startNextMetadataJob();
        return;
    }

    refreshBusy();
}

void OnlineMetadataService::finishWithError(const QString& errorMessage)
{
    setErrorMessage(errorMessage);
    setStatus({});
    finishRequest();
}

void OnlineMetadataService::finishRequest()
{
    m_lyricsBusy = false;
    refreshBusy();
}

void OnlineMetadataService::refreshBusy()
{
    setBusy(m_lyricsBusy || m_metadataActive || !m_metadataQueue.isEmpty());
}

} // namespace opusora
