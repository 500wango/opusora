#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QString>
#include <QUrl>
#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;

namespace opusora {

class OnlineMetadataService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    explicit OnlineMetadataService(QObject* parent = nullptr);
    explicit OnlineMetadataService(QString databasePath, QObject* parent = nullptr);

    bool busy() const;
    QString status() const;
    QString errorMessage() const;

    Q_INVOKABLE void fetchLyrics(int trackId, const QString& title, const QString& artist, const QString& album, int durationMs);
    Q_INVOKABLE void fetchMetadata(int trackId, const QString& title, const QString& artist, const QString& album, int durationMs);
    Q_INVOKABLE void clearMessages();

    static QString lyricsFromLrclibJsonForTest(const QByteArray& payload);
    static QString lyricsFromNeteaseJsonForTest(const QByteArray& payload);
    static QString lyricsFromKugouJsonForTest(const QByteArray& payload);
    static QVariantMap metadataFromMusicBrainzJsonForTest(const QByteArray& payload);

signals:
    void busyChanged();
    void statusChanged();
    void errorMessageChanged();
    void lyricsFetched(int trackId, const QString& lyrics, const QString& title, const QString& artist, const QString& album);
    void metadataFetched(int trackId, const QString& title, const QString& artist, const QString& album);

private:
    enum class RequestKind {
        LyricsExact,
        LyricsSearch,
        NeteaseLyricsSearch,
        NeteaseLyrics,
        KugouLyricsSearch,
        KugouLyrics,
    };

    void setBusy(bool busy);
    void setStatus(const QString& status);
    void setErrorMessage(const QString& errorMessage);
    void queueLyricsSearches();
    void startNextLyricsSearch();
    void queueNeteaseLyricsSearches();
    void startNextNeteaseLyricsSearch();
    void queueKugouLyricsSearches();
    void startNextKugouLyricsSearch();
    void sendGet(const QUrl& url, RequestKind kind);
    void handleReply(QNetworkReply* reply);
    void handleLyricsExactReply(const QByteArray& payload, int httpStatus);
    void handleLyricsSearchReply(const QByteArray& payload, int httpStatus);
    void handleNeteaseLyricsSearchReply(const QByteArray& payload, int httpStatus);
    void handleNeteaseLyricsReply(const QByteArray& payload, int httpStatus);
    void handleKugouLyricsSearchReply(const QByteArray& payload, int httpStatus);
    void handleKugouLyricsReply(const QByteArray& payload, int httpStatus);
    void finishLyricsWithoutMatch();
    void startNextMetadataJob();
    void finishMetadataJob(
        int trackId,
        const QString& title,
        const QString& artist,
        const QString& album,
        const QString& errorMessage);
    void finishWithError(const QString& errorMessage);
    void finishRequest();
    void refreshBusy();

    struct MetadataJob {
        int trackId = 0;
        QString title;
        QString artist;
        QString album;
        int durationMs = 0;
    };

    QNetworkAccessManager* m_network = nullptr;
    QPointer<QNetworkReply> m_currentReply;
    bool m_busy = false;
    bool m_lyricsBusy = false;
    bool m_metadataActive = false;
    QString m_status;
    QString m_errorMessage;
    QString m_databasePath;
    QQueue<QUrl> m_lyricsSearchQueue;
    QQueue<QUrl> m_neteaseLyricsSearchQueue;
    QQueue<QUrl> m_kugouLyricsSearchQueue;
    QQueue<MetadataJob> m_metadataQueue;
    int m_trackId = 0;
    QString m_title;
    QString m_artist;
    QString m_album;
    int m_durationMs = 0;
};

} // namespace opusora
