#include "core/AppPaths.hpp"
#include "core/Logger.hpp"
#include "core/TextEncoding.hpp"
#include "db/Database.hpp"
#include "i18n/TranslationService.hpp"
#include "integration/MprisService.hpp"
#include "integration/NotificationService.hpp"
#include "integration/TrayService.hpp"
#include "library/AlbumListModel.hpp"
#include "library/ArtistListModel.hpp"
#include "library/GenreListModel.hpp"
#include "library/LibraryController.hpp"
#include "library/LibraryRepository.hpp"
#include "library/TrackListModel.hpp"
#include "metadata/OnlineMetadataService.hpp"
#if OPUSORA_WITH_GSTREAMER
#include "player/GStreamerPlayer.hpp"
#endif
#include "player/NullAudioPlayer.hpp"
#include "player/PlaybackController.hpp"
#include "playlist/PlaylistListModel.hpp"
#include "radio/RadioStationModel.hpp"
#include "search/SearchController.hpp"
#include "settings/SettingsService.hpp"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QProcessEnvironment>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariantMap>

#include <memory>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Opusora"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("opusora.local"));
    QCoreApplication::setApplicationName(QStringLiteral("Opusora"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    if (!opusora::AppPaths::ensureCreated()) {
        qWarning("Could not create Opusora application directories");
    }
    opusora::Logger::install();

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    opusora::Database database(opusora::AppPaths::databasePath());
    if (!database.migrate()) {
        qWarning() << "Database migration failed" << database.lastError();
    }
    opusora::LibraryRepository(&database).repairStoredMetadataText();

    opusora::SettingsService settings;
    opusora::TranslationService i18n;
    i18n.setLanguage(settings.language());

#if OPUSORA_WITH_GSTREAMER
    auto audioPlayer = std::make_unique<opusora::GStreamerPlayer>();
    qInfo() << "Using GStreamer audio backend";
#else
    auto audioPlayer = std::make_unique<opusora::NullAudioPlayer>();
    qWarning() << "Using Null audio backend; install GStreamer development packages and rebuild for playback";
#endif
    opusora::PlaybackController playback(std::move(audioPlayer));
    playback.setVolume(settings.volume());
    playback.setReplayGainEnabled(settings.replayGainEnabled());
    playback.setGaplessPlaybackEnabled(settings.gaplessPlaybackEnabled());
    std::unique_ptr<opusora::MprisService> mpris;
    opusora::NotificationService notifications(&playback);
    opusora::TrayService tray(&playback);
    opusora::LibraryController library(&database);
    opusora::TrackListModel trackModel(&database);
    opusora::TrackListModel albumTrackModel(&database);
    opusora::TrackListModel artistTrackModel(&database);
    opusora::TrackListModel genreTrackModel(&database);
    opusora::TrackListModel playlistTrackModel(&database);
    opusora::TrackListModel recentAddedTrackModel(&database);
    opusora::TrackListModel recentPlayedTrackModel(&database);
    opusora::TrackListModel duplicateTrackModel(&database);
    opusora::AlbumListModel albumModel(&database);
    opusora::ArtistListModel artistModel(&database);
    opusora::GenreListModel genreModel(&database);
    opusora::PlaylistListModel playlistModel(&database);
    opusora::RadioStationModel radioModel;
    opusora::SearchController search(&database);
    opusora::OnlineMetadataService onlineMetadata(opusora::AppPaths::databasePath());
    trackModel.setSortOrder(settings.defaultSortOrder());
    recentAddedTrackModel.loadRecentAdded(8);
    recentPlayedTrackModel.loadRecentPlayed(8);
    duplicateTrackModel.loadDuplicates(100);

    auto syncMediaKeyService = [&]() {
        if (QProcessEnvironment::systemEnvironment().value(QStringLiteral("OPUSORA_TEST_DISABLE_MEDIA_KEYS")) == QStringLiteral("1")) {
            if (mpris) {
                mpris.reset();
            }
            return;
        }
        if (settings.mediaKeysEnabled()) {
            if (!mpris) {
                mpris = std::make_unique<opusora::MprisService>(&playback);
            }
        } else if (mpris) {
            mpris.reset();
            qInfo() << "MPRIS media key service disabled";
        }
    };
    syncMediaKeyService();

    QObject::connect(&settings, &opusora::SettingsService::languageChanged, [&]() {
        i18n.setLanguage(settings.language());
    });
    QObject::connect(&settings, &opusora::SettingsService::mediaKeysEnabledChanged, syncMediaKeyService);
    QObject::connect(&settings, &opusora::SettingsService::defaultSortOrderChanged, [&]() {
        trackModel.setSortOrder(settings.defaultSortOrder());
    });
    QObject::connect(&settings, &opusora::SettingsService::volumeChanged, [&]() {
        playback.setVolume(settings.volume());
    });
    QObject::connect(&settings, &opusora::SettingsService::replayGainEnabledChanged, [&]() {
        playback.setReplayGainEnabled(settings.replayGainEnabled());
    });
    QObject::connect(&settings, &opusora::SettingsService::gaplessPlaybackEnabledChanged, [&]() {
        playback.setGaplessPlaybackEnabled(settings.gaplessPlaybackEnabled());
    });
    QObject::connect(&playback, &opusora::PlaybackController::volumeChanged, [&]() {
        settings.setVolume(playback.volume());
    });
    QObject::connect(&playback, &opusora::PlaybackController::replayGainEnabledChanged, [&]() {
        settings.setReplayGainEnabled(playback.replayGainEnabled());
    });
    QObject::connect(&playback, &opusora::PlaybackController::gaplessPlaybackEnabledChanged, [&]() {
        settings.setGaplessPlaybackEnabled(playback.gaplessPlaybackEnabled());
    });
    QObject::connect(&library, &opusora::LibraryController::libraryChanged, &trackModel, &opusora::TrackListModel::reload);
    QObject::connect(&library, &opusora::LibraryController::libraryChanged, &albumTrackModel, &opusora::TrackListModel::reload);
    QObject::connect(&library, &opusora::LibraryController::libraryChanged, &artistTrackModel, &opusora::TrackListModel::reload);
    QObject::connect(&library, &opusora::LibraryController::libraryChanged, &genreTrackModel, &opusora::TrackListModel::reload);
    QObject::connect(&library, &opusora::LibraryController::libraryChanged, &playlistTrackModel, &opusora::TrackListModel::reload);
    QObject::connect(&library, &opusora::LibraryController::libraryChanged, &recentAddedTrackModel, [&]() {
        recentAddedTrackModel.loadRecentAdded(8);
    });
    QObject::connect(&library, &opusora::LibraryController::libraryChanged, &recentPlayedTrackModel, [&]() {
        recentPlayedTrackModel.loadRecentPlayed(8);
    });
    QObject::connect(&library, &opusora::LibraryController::libraryChanged, &duplicateTrackModel, [&]() {
        duplicateTrackModel.loadDuplicates(100);
    });
    QObject::connect(&library, &opusora::LibraryController::libraryChanged, &albumModel, &opusora::AlbumListModel::reload);
    QObject::connect(&library, &opusora::LibraryController::libraryChanged, &artistModel, &opusora::ArtistListModel::reload);
    QObject::connect(&library, &opusora::LibraryController::libraryChanged, &genreModel, &opusora::GenreListModel::reload);
    QObject::connect(&library, &opusora::LibraryController::libraryChanged, &search, &opusora::SearchController::reload);
    QObject::connect(&playlistModel, &opusora::PlaylistListModel::playlistChanged, &playlistTrackModel, &opusora::TrackListModel::reload);
    QObject::connect(&playback, &opusora::PlaybackController::trackPlaybackStarted, [&](int trackId) {
        if (opusora::LibraryRepository(&database).markTrackPlayed(trackId)) {
            recentPlayedTrackModel.loadRecentPlayed(8);
        }
    });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"), &settings);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &i18n);
    engine.rootContext()->setContextProperty(QStringLiteral("PlaybackController"), &playback);
    engine.rootContext()->setContextProperty(QStringLiteral("LibraryController"), &library);
    engine.rootContext()->setContextProperty(QStringLiteral("TrackModel"), &trackModel);
    engine.rootContext()->setContextProperty(QStringLiteral("AlbumTrackModel"), &albumTrackModel);
    engine.rootContext()->setContextProperty(QStringLiteral("ArtistTrackModel"), &artistTrackModel);
    engine.rootContext()->setContextProperty(QStringLiteral("GenreTrackModel"), &genreTrackModel);
    engine.rootContext()->setContextProperty(QStringLiteral("PlaylistTrackModel"), &playlistTrackModel);
    engine.rootContext()->setContextProperty(QStringLiteral("RecentAddedTrackModel"), &recentAddedTrackModel);
    engine.rootContext()->setContextProperty(QStringLiteral("RecentPlayedTrackModel"), &recentPlayedTrackModel);
    engine.rootContext()->setContextProperty(QStringLiteral("DuplicateTrackModel"), &duplicateTrackModel);
    engine.rootContext()->setContextProperty(QStringLiteral("AlbumModel"), &albumModel);
    engine.rootContext()->setContextProperty(QStringLiteral("ArtistModel"), &artistModel);
    engine.rootContext()->setContextProperty(QStringLiteral("GenreModel"), &genreModel);
    engine.rootContext()->setContextProperty(QStringLiteral("PlaylistModel"), &playlistModel);
    engine.rootContext()->setContextProperty(QStringLiteral("RadioModel"), &radioModel);
    engine.rootContext()->setContextProperty(QStringLiteral("SearchController"), &search);
    engine.rootContext()->setContextProperty(QStringLiteral("OnlineMetadataService"), &onlineMetadata);

    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Opusora/app/ui/qml/AppWindow.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.size() > 1 && arguments.at(1) == QStringLiteral("--app-smoke")) {
        QTimer::singleShot(0, []() {
            qInfo() << "App smoke loaded";
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 1 && arguments.at(1) == QStringLiteral("--encoding-smoke")) {
        QTimer::singleShot(0, []() {
            const QString korean = opusora::repairMojibake(QString::fromLatin1("\xC7\xD1\xB1\xB9"));
            const QString koreanArtist = opusora::repairMojibake(QString::fromLatin1("\xC0\xCC\xB9\xCC\xC0\xDA"));
            const QString chinese = opusora::repairMojibake(QString::fromLatin1("\xC7\xF1\xB9\xFA\xD0\xC2\xD4\xAD\xB4\xB4"));
            const QString chineseArtist = opusora::repairMojibake(QString::fromLatin1("\xCD\xF2\xB7\xBC"));
            const QString chineseRain = opusora::repairMojibake(QString::fromLatin1("\xD4\xDA\xD3\xEA\xD6\xD0"));
            const QString chineseVancouver = opusora::repairMojibake(QString::fromLatin1("\xCE\xC2\xB8\xE7\xBB\xAA\xB1\xAF\xC9\xCB\xD2\xBB\xBA\xC5"));
            const QString chineseJacky = opusora::repairMojibake(QString::fromLatin1("\xD5\xC5\xD1\xA7\xD3\xD1"));
            const QString chineseFarewell = opusora::repairMojibake(QString::fromLatin1("\xB0\xD4\xCD\xF5\xB1\xF0\xBC\xA7"));
            const QString mixedChinese = opusora::repairMojibake(
                QString::fromLatin1("\xB1\xB1\xBE\xA9\xB1\xB1\xBE\xA9")
                + QStringLiteral(", 北京北京, 北京北京"));
            const QString commaChinese = opusora::repairMojibake(
                QString::fromLatin1("\xD0\xC2\xB2\xBB\xC1\xCB\xC7\xE9\x2C\xB5\xE7\xD3\xB0\xC5\xE4\xC0\xD6"));
            const QString questionDuplicate = opusora::repairMojibake(QStringLiteral("???, 蓝莲花, 蓝莲花"));
            const QString questionPrefixDuplicate = opusora::repairMojibake(QStringLiteral(
                "St. Philips Boy's Choir???????, St. Philips Boy's Choir英国圣菲利普童声合唱团, St. Philips Boy's Choir英国圣菲利普童声合唱团"));
            const QString normalQuestion = opusora::repairMojibake(QStringLiteral("Who Are You?"));
            const QString rawChineseArtist = opusora::metadataStringFromLegacyBytes(QByteArray::fromHex("CDF2B7BC"));
            const QString rawJapaneseThanks = opusora::metadataStringFromLegacyBytes(QByteArray::fromHex("82A082E882AA82C682A4"));
            const QString normalKorean = opusora::repairMojibake(QStringLiteral("아이유"));
            const QString accented = opusora::repairMojibake(QStringLiteral("Beyoncé"));
            const bool ok = korean == QStringLiteral("한국")
                && koreanArtist == QStringLiteral("이미자")
                && chinese == QStringLiteral("邱国新原创")
                && chineseArtist == QStringLiteral("万芳")
                && chineseRain == QStringLiteral("在雨中")
                && chineseVancouver == QStringLiteral("温哥华悲伤一号")
                && chineseJacky == QStringLiteral("张学友")
                && chineseFarewell == QStringLiteral("霸王别姬")
                && mixedChinese == QStringLiteral("北京北京")
                && commaChinese == QStringLiteral("新不了情,电影配乐")
                && questionDuplicate == QStringLiteral("蓝莲花")
                && questionPrefixDuplicate == QStringLiteral("St. Philips Boy's Choir英国圣菲利普童声合唱团")
                && normalQuestion == QStringLiteral("Who Are You?")
                && rawChineseArtist == QStringLiteral("万芳")
                && rawJapaneseThanks == QStringLiteral("ありがとう")
                && normalKorean == QStringLiteral("아이유")
                && accented == QStringLiteral("Beyoncé");
            qInfo() << "Encoding smoke"
                    << korean
                    << koreanArtist
                    << chinese
                    << chineseArtist
                    << chineseRain
                    << chineseVancouver
                    << chineseJacky
                    << chineseFarewell
                    << mixedChinese
                    << commaChinese
                    << questionDuplicate
                    << questionPrefixDuplicate
                    << normalQuestion
                    << rawChineseArtist
                    << rawJapaneseThanks
                    << normalKorean
                    << accented
                    << (ok ? "ok" : "failed");
            QCoreApplication::exit(ok ? 0 : 1);
        });
    } else if (arguments.size() > 1 && arguments.at(1) == QStringLiteral("--online-metadata-smoke")) {
        QTimer::singleShot(0, []() {
            const QByteArray lyricsJson = R"([
                {
                    "trackName": "Example",
                    "artistName": "Tester",
                    "albumName": "Smoke",
                    "duration": 123,
                    "plainLyrics": "first line\nsecond line",
                    "syncedLyrics": "[00:01.00]first line\n[00:02.00]second line"
                }
            ])";
            const QByteArray neteaseLyricsJson = R"({
                "lrc": {
                    "version": 1,
                    "lyric": "[00:01.00]残酷な天使のテーゼ\n[00:02.00]少年よ 神話になれ"
                },
                "code": 200
            })";
            const QByteArray kugouLyricsJson = R"({
                "status": 200,
                "content": "WzAwOjAxLjAwXea1i+ivleatjOivjQ=="
            })";
            const QByteArray metadataJson = R"({
                "recordings": [
                    {
                        "score": 100,
                        "title": "Example",
                        "length": 123000,
                        "artist-credit": [
                            { "name": "Tester", "joinphrase": "" }
                        ],
                        "releases": [
                            { "title": "Smoke", "status": "Official" }
                        ]
                    }
                ]
            })";
            const QString lyrics = opusora::OnlineMetadataService::lyricsFromLrclibJsonForTest(lyricsJson);
            const QString neteaseLyrics = opusora::OnlineMetadataService::lyricsFromNeteaseJsonForTest(neteaseLyricsJson);
            const QString kugouLyrics = opusora::OnlineMetadataService::lyricsFromKugouJsonForTest(kugouLyricsJson);
            const QVariantMap metadata = opusora::OnlineMetadataService::metadataFromMusicBrainzJsonForTest(metadataJson);
            const bool ok = lyrics.contains(QStringLiteral("[00:01.00]first line"))
                && neteaseLyrics.contains(QStringLiteral("残酷な天使のテーゼ"))
                && kugouLyrics.contains(QStringLiteral("测试歌词"))
                && metadata.value(QStringLiteral("title")).toString() == QStringLiteral("Example")
                && metadata.value(QStringLiteral("artist")).toString() == QStringLiteral("Tester")
                && metadata.value(QStringLiteral("album")).toString() == QStringLiteral("Smoke");
            qInfo() << "Online metadata smoke" << (ok ? "ok" : "failed");
            QCoreApplication::exit(ok ? 0 : 1);
        });
    } else if (arguments.size() > 2 && arguments.at(1) == QStringLiteral("--ui-screenshot")) {
        const QString screenshotPath = QDir::cleanPath(arguments.at(2));
        const QString popupObjectName = arguments.size() > 4 ? arguments.at(4) : QString();
        if (arguments.size() > 3 && !engine.rootObjects().isEmpty()) {
            engine.rootObjects().first()->setProperty("currentPage", arguments.at(3));
        }
        QTimer::singleShot(500, &engine, [&engine, screenshotPath, popupObjectName]() {
            auto* window = engine.rootObjects().isEmpty()
                ? nullptr
                : qobject_cast<QQuickWindow*>(engine.rootObjects().first());
            if (!window) {
                qWarning() << "UI screenshot failed: root object is not a QQuickWindow";
                QCoreApplication::exit(1);
                return;
            }

            if (!popupObjectName.isEmpty()) {
                QObject* popupTarget = window->findChild<QObject*>(popupObjectName);
                if (!popupTarget || !QMetaObject::invokeMethod(popupTarget, "openPopup")) {
                    qWarning() << "UI screenshot failed: could not open popup" << popupObjectName;
                    QCoreApplication::exit(1);
                    return;
                }
                QCoreApplication::processEvents();
            }

            const auto result = window->contentItem()->grabToImage();
            if (result.isNull()) {
                qWarning() << "UI screenshot failed: could not start item grab";
                QCoreApplication::exit(1);
                return;
            }
            QObject::connect(result.data(), &QQuickItemGrabResult::ready, &engine, [result, screenshotPath]() {
                const bool saved = result->saveToFile(screenshotPath);
                qInfo() << "UI screenshot" << screenshotPath << (saved ? "saved" : "failed");
                QCoreApplication::exit(saved ? 0 : 1);
            });
        });
    } else if (arguments.size() > 1 && arguments.at(1) == QStringLiteral("--restore-smoke")) {
        QTimer::singleShot(0, &playback, [&playback]() {
            qInfo() << "Restore smoke queue count" << playback.queueCount()
                    << "index" << playback.queueIndex()
                    << "position" << playback.positionMs()
                    << "file" << playback.currentFilePath();
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 1 && arguments.at(1) == QStringLiteral("--recent-smoke")) {
        QTimer::singleShot(0, &recentAddedTrackModel, [&database, &recentAddedTrackModel, &recentPlayedTrackModel]() {
            recentAddedTrackModel.loadRecentAdded(8);
            qInfo() << "Recent added returned" << recentAddedTrackModel.count() << "rows";
            if (recentAddedTrackModel.count() > 0) {
                const QModelIndex modelIndex = recentAddedTrackModel.index(0, 0);
                const int trackId = recentAddedTrackModel.data(modelIndex, opusora::TrackListModel::IdRole).toInt();
                opusora::LibraryRepository(&database).markTrackPlayed(trackId);
            }
            recentPlayedTrackModel.loadRecentPlayed(8);
            qInfo() << "Recent played returned" << recentPlayedTrackModel.count() << "rows";
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 1 && arguments.at(1) == QStringLiteral("--duplicates-smoke")) {
        QTimer::singleShot(0, &duplicateTrackModel, [&duplicateTrackModel]() {
            duplicateTrackModel.loadDuplicates(100);
            qInfo() << "Duplicate tracks returned" << duplicateTrackModel.count() << "rows";
            for (int row = 0; row < duplicateTrackModel.count(); ++row) {
                const QModelIndex modelIndex = duplicateTrackModel.index(row, 0);
                qInfo() << "Duplicate track" << row
                        << duplicateTrackModel.data(modelIndex, opusora::TrackListModel::TitleRole).toString()
                        << duplicateTrackModel.data(modelIndex, opusora::TrackListModel::FilePathRole).toString();
            }
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 2 && arguments.at(1) == QStringLiteral("--scan")) {
        const QString scanPath = QDir::cleanPath(arguments.at(2));
        QTimer::singleShot(0, &library, [&library, &trackModel, &albumModel, &artistModel, &genreModel, scanPath]() {
            QObject::connect(&library, &opusora::LibraryController::scanStatusChanged, &library, [&library, &trackModel, &albumModel, &artistModel, &genreModel]() {
                const QString status = library.scanStatus();
                if (status != QStringLiteral("completed")
                    && status != QStringLiteral("cancelled")
                    && status != QStringLiteral("error")) {
                    return;
                }
                trackModel.reload();
                albumModel.reload();
                artistModel.reload();
                genreModel.reload();
                QCoreApplication::quit();
            });
            if (library.roots().contains(scanPath)) {
                library.scanRoot(scanPath);
            } else {
                library.addRoot(scanPath);
            }
        });
    } else if (arguments.size() > 2 && arguments.at(1) == QStringLiteral("--remove-root")) {
        const QString rootPath = QDir::cleanPath(arguments.at(2));
        QTimer::singleShot(0, &library, [&library, rootPath]() {
            library.removeRoot(rootPath);
            qInfo() << "Removed library root" << rootPath;
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 1 && arguments.at(1) == QStringLiteral("--genres")) {
        QTimer::singleShot(0, &genreModel, [&genreModel]() {
            genreModel.reload();
            qInfo() << "Genres returned" << genreModel.count() << "rows";
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 2 && arguments.at(1) == QStringLiteral("--genre-tracks")) {
        const QString genreName = arguments.mid(2).join(QLatin1Char(' '));
        QTimer::singleShot(0, &genreTrackModel, [&genreTrackModel, genreName]() {
            genreTrackModel.loadGenre(genreName);
            qInfo() << "Genre tracks returned" << genreTrackModel.count() << "rows for genre" << genreName;
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 1 && arguments.at(1) == QStringLiteral("--playlists")) {
        QTimer::singleShot(0, &playlistModel, [&playlistModel]() {
            playlistModel.reload();
            qInfo() << "Playlists returned" << playlistModel.count() << "rows";
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 1 && arguments.at(1) == QStringLiteral("--radio-list")) {
        QTimer::singleShot(0, &radioModel, [&radioModel]() {
            qInfo() << "Radio stations returned" << radioModel.count() << "rows";
            for (int row = 0; row < radioModel.count(); ++row) {
                qInfo() << "Radio station" << row
                        << radioModel.stationName(row)
                        << radioModel.stationUrl(row);
            }
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 3 && arguments.at(1) == QStringLiteral("--radio-add")) {
        const QString stationUrl = arguments.last();
        const QString stationName = arguments.mid(2, arguments.size() - 3).join(QLatin1Char(' '));
        QTimer::singleShot(0, &radioModel, [&radioModel, stationName, stationUrl]() {
            const bool added = radioModel.addStation(stationName, stationUrl);
            qInfo() << "Add radio station" << stationName << stationUrl
                    << (added ? "succeeded" : "failed")
                    << "total rows" << radioModel.count();
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 2 && arguments.at(1) == QStringLiteral("--radio-remove")) {
        const int row = arguments.at(2).toInt();
        QTimer::singleShot(0, &radioModel, [&radioModel, row]() {
            const bool removed = radioModel.removeStation(row);
            qInfo() << "Remove radio station row" << row
                    << (removed ? "succeeded" : "failed")
                    << "total rows" << radioModel.count();
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 2 && arguments.at(1) == QStringLiteral("--create-playlist")) {
        const QString playlistName = arguments.mid(2).join(QLatin1Char(' '));
        QTimer::singleShot(0, &playlistModel, [&playlistModel, playlistName]() {
            const bool created = playlistModel.createPlaylist(playlistName);
            qInfo() << "Create playlist" << playlistName << (created ? "succeeded" : "failed")
                    << "total rows" << playlistModel.count();
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 3 && arguments.at(1) == QStringLiteral("--rename-playlist")) {
        const int playlistId = arguments.at(2).toInt();
        const QString playlistName = arguments.mid(3).join(QLatin1Char(' '));
        QTimer::singleShot(0, &playlistModel, [&playlistModel, playlistId, playlistName]() {
            const bool renamed = playlistModel.renamePlaylist(playlistId, playlistName);
            qInfo() << "Rename playlist" << playlistId << "to" << playlistName
                    << (renamed ? "succeeded" : "failed");
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 2 && arguments.at(1) == QStringLiteral("--delete-playlist")) {
        const int playlistId = arguments.at(2).toInt();
        QTimer::singleShot(0, &playlistModel, [&playlistModel, playlistId]() {
            const bool deleted = playlistModel.deletePlaylist(playlistId);
            qInfo() << "Delete playlist" << playlistId << (deleted ? "succeeded" : "failed")
                    << "total rows" << playlistModel.count();
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 3 && arguments.at(1) == QStringLiteral("--export-playlist-m3u")) {
        const int playlistId = arguments.at(2).toInt();
        const QString filePath = QDir::cleanPath(arguments.at(3));
        QTimer::singleShot(0, &playlistModel, [&playlistModel, playlistId, filePath]() {
            const bool exported = playlistModel.exportM3u(playlistId, filePath);
            qInfo() << "Export playlist" << playlistId << "to" << filePath
                    << (exported ? "succeeded" : "failed");
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 2 && arguments.at(1) == QStringLiteral("--import-playlist-m3u")) {
        const QString filePath = QDir::cleanPath(arguments.at(2));
        const QString playlistName = arguments.size() > 3 ? arguments.mid(3).join(QLatin1Char(' ')) : QString();
        QTimer::singleShot(0, &playlistModel, [&playlistModel, filePath, playlistName]() {
            const bool imported = playlistModel.importM3u(filePath, playlistName);
            qInfo() << "Import playlist from" << filePath << (imported ? "succeeded" : "failed")
                    << "total rows" << playlistModel.count();
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 3 && arguments.at(1) == QStringLiteral("--add-track-to-playlist")) {
        const int playlistId = arguments.at(2).toInt();
        const int trackId = arguments.at(3).toInt();
        QTimer::singleShot(0, &playlistModel, [&playlistModel, playlistId, trackId]() {
            const bool added = playlistModel.addTrack(playlistId, trackId);
            qInfo() << "Add track" << trackId << "to playlist" << playlistId
                    << (added ? "succeeded" : "failed");
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 1 && arguments.at(1) == QStringLiteral("--playlist-bulk-smoke")) {
        QTimer::singleShot(0, &playlistModel, [&database, &playlistModel, &playlistTrackModel]() {
            QTemporaryDir tempDir;
            if (!tempDir.isValid()) {
                qWarning() << "Playlist bulk smoke failed to create temporary directory";
                QCoreApplication::exit(1);
                return;
            }

            QList<int> trackIds;
            for (int index = 0; index < 3; ++index) {
                const QString filePath = tempDir.filePath(QStringLiteral("bulk-%1.mp3").arg(index));
                QFile file(filePath);
                if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    qWarning() << "Playlist bulk smoke failed to create track file" << filePath << file.errorString();
                    QCoreApplication::exit(1);
                    return;
                }
                file.write(QStringLiteral("opusora playlist bulk smoke %1").arg(index).toUtf8());
                file.close();

                opusora::TrackMetadata metadata;
                metadata.title = QStringLiteral("Bulk Smoke %1").arg(index).toStdString();
                metadata.artist = "Opusora";
                metadata.album = "Bulk Smoke";
                metadata.durationMs = 1000 + index;
                metadata.readStatus = opusora::MetadataReadStatus::Success;
                metadata.encodingStatus = opusora::MetadataEncodingStatus::Normal;
                if (!opusora::LibraryRepository(&database).upsertTrack(filePath, metadata)) {
                    qWarning() << "Playlist bulk smoke failed to insert track" << filePath;
                    QCoreApplication::exit(1);
                    return;
                }

                QSqlQuery idQuery(database.connection());
                idQuery.prepare(QStringLiteral("SELECT id FROM track WHERE file_path = :file_path"));
                idQuery.bindValue(QStringLiteral(":file_path"), QFileInfo(filePath).absoluteFilePath());
                if (!idQuery.exec() || !idQuery.next()) {
                    qWarning() << "Playlist bulk smoke failed to load track id" << filePath
                               << idQuery.lastError().text();
                    QCoreApplication::exit(1);
                    return;
                }
                trackIds.append(idQuery.value(0).toInt());
            }

            const QString playlistName = QStringLiteral("Bulk Smoke Playlist");
            if (!playlistModel.createPlaylist(playlistName)) {
                qWarning() << "Playlist bulk smoke failed to create playlist";
                QCoreApplication::exit(1);
                return;
            }

            QSqlQuery playlistIdQuery(database.connection());
            playlistIdQuery.prepare(QStringLiteral(
                "SELECT id FROM playlist WHERE name = :name ORDER BY id DESC LIMIT 1"));
            playlistIdQuery.bindValue(QStringLiteral(":name"), playlistName);
            if (!playlistIdQuery.exec() || !playlistIdQuery.next()) {
                qWarning() << "Playlist bulk smoke failed to load playlist id"
                           << playlistIdQuery.lastError().text();
                QCoreApplication::exit(1);
                return;
            }

            const int playlistId = playlistIdQuery.value(0).toInt();
            QVariantList bulkIds;
            for (const int trackId : trackIds) {
                bulkIds.append(trackId);
            }

            const int added = playlistModel.addTracks(playlistId, bulkIds);
            playlistTrackModel.loadPlaylist(playlistId);

            bool orderOk = playlistTrackModel.count() == trackIds.size();
            for (int row = 0; orderOk && row < playlistTrackModel.count(); ++row) {
                const QModelIndex modelIndex = playlistTrackModel.index(row, 0);
                orderOk = playlistTrackModel.data(modelIndex, opusora::TrackListModel::IdRole).toInt() == trackIds.at(row)
                    && playlistTrackModel.data(modelIndex, opusora::TrackListModel::PlaylistPositionRole).toInt() == row;
            }

            const bool ok = added == trackIds.size() && orderOk;
            qInfo() << "Playlist bulk smoke added" << added
                    << "tracks rows" << playlistTrackModel.count()
                    << (ok ? "ok" : "failed");
            QCoreApplication::exit(ok ? 0 : 1);
        });
    } else if (arguments.size() > 3 && arguments.at(1) == QStringLiteral("--remove-track-from-playlist")) {
        const int playlistId = arguments.at(2).toInt();
        const int trackId = arguments.at(3).toInt();
        QTimer::singleShot(0, &playlistModel, [&playlistModel, playlistId, trackId]() {
            const bool removed = playlistModel.removeTrack(playlistId, trackId);
            qInfo() << "Remove track" << trackId << "from playlist" << playlistId
                    << (removed ? "succeeded" : "failed");
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 3 && arguments.at(1) == QStringLiteral("--remove-playlist-entry")) {
        const int playlistId = arguments.at(2).toInt();
        const qint64 entryId = arguments.at(3).toLongLong();
        QTimer::singleShot(0, &playlistModel, [&playlistModel, playlistId, entryId]() {
            const bool removed = playlistModel.removeEntry(playlistId, entryId);
            qInfo() << "Remove playlist entry" << entryId << "from playlist" << playlistId
                    << (removed ? "succeeded" : "failed");
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 4 && arguments.at(1) == QStringLiteral("--move-playlist-entry")) {
        const int playlistId = arguments.at(2).toInt();
        const qint64 entryId = arguments.at(3).toLongLong();
        const int direction = arguments.at(4).toInt();
        QTimer::singleShot(0, &playlistModel, [&playlistModel, playlistId, entryId, direction]() {
            const bool moved = playlistModel.moveEntry(playlistId, entryId, direction);
            qInfo() << "Move playlist entry" << entryId << "in playlist" << playlistId
                    << "direction" << direction << (moved ? "succeeded" : "failed");
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 2 && arguments.at(1) == QStringLiteral("--playlist-tracks")) {
        const int playlistId = arguments.at(2).toInt();
        QTimer::singleShot(0, &playlistTrackModel, [&playlistTrackModel, playlistId]() {
            playlistTrackModel.loadPlaylist(playlistId);
            qInfo() << "Playlist tracks returned" << playlistTrackModel.count() << "rows for playlist" << playlistId;
            for (int row = 0; row < playlistTrackModel.count(); ++row) {
                const QModelIndex modelIndex = playlistTrackModel.index(row, 0);
                qInfo() << "Playlist track" << row
                        << "entry" << playlistTrackModel.data(modelIndex, opusora::TrackListModel::PlaylistEntryIdRole).toLongLong()
                        << "position" << playlistTrackModel.data(modelIndex, opusora::TrackListModel::PlaylistPositionRole).toInt()
                        << "track" << playlistTrackModel.data(modelIndex, opusora::TrackListModel::IdRole).toInt()
                        << "title" << playlistTrackModel.data(modelIndex, opusora::TrackListModel::TitleRole).toString();
            }
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 2 && arguments.at(1) == QStringLiteral("--queue-smoke")) {
        const QString filePath = QDir::cleanPath(arguments.at(2));
        QTimer::singleShot(0, &playback, [&engine, &playback, filePath]() {
            if (!engine.rootObjects().isEmpty()) {
                engine.rootObjects().first()->setProperty("currentPage", QStringLiteral("queue"));
            }

            QVariantMap firstItem;
            firstItem.insert(QStringLiteral("filePath"), filePath);
            firstItem.insert(QStringLiteral("title"), QStringLiteral("Queue Smoke 1"));
            firstItem.insert(QStringLiteral("artist"), QStringLiteral("Opusora"));

            QVariantMap secondItem;
            secondItem.insert(QStringLiteral("filePath"), filePath);
            secondItem.insert(QStringLiteral("title"), QStringLiteral("Queue Smoke 2"));
            secondItem.insert(QStringLiteral("artist"), QStringLiteral("Opusora"));

            playback.appendQueueItem(firstItem);
            QCoreApplication::processEvents();
            qInfo() << "Queue smoke appended first count" << playback.queueCount() << "index" << playback.queueIndex();
            playback.appendQueueItem(secondItem);
            qInfo() << "Queue smoke appended second count" << playback.queueCount() << "index" << playback.queueIndex();
            for (const QVariant& itemVariant : playback.queueItems()) {
                const QVariantMap item = itemVariant.toMap();
                qInfo() << "Queue smoke item" << item.value(QStringLiteral("index")).toInt()
                        << "current" << item.value(QStringLiteral("current")).toBool()
                        << "title" << item.value(QStringLiteral("title")).toString();
            }

            playback.playQueueIndex(0);
            qInfo() << "Queue smoke jumped count" << playback.queueCount() << "index" << playback.queueIndex();

            playback.removeQueueItem(0);
            qInfo() << "Queue smoke removed count" << playback.queueCount() << "index" << playback.queueIndex();

            playback.clearQueue();
            qInfo() << "Queue smoke cleared count" << playback.queueCount() << "index" << playback.queueIndex();

            const int appended = playback.appendQueueItems(QVariantList { firstItem, secondItem });
            qInfo() << "Queue smoke batch appended" << appended
                    << "count" << playback.queueCount() << "index" << playback.queueIndex();
            playback.clearQueue();
            qInfo() << "Queue smoke batch cleared count" << playback.queueCount() << "index" << playback.queueIndex();
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 2 && arguments.at(1) == QStringLiteral("--playqueue-index-smoke")) {
        const QString filePath = QDir::cleanPath(arguments.at(2));
        QTimer::singleShot(0, &playback, [&playback, filePath]() {
            QVariantMap invalidItem;
            invalidItem.insert(QStringLiteral("filePath"), QString());
            invalidItem.insert(QStringLiteral("title"), QStringLiteral("Invalid Smoke"));

            QVariantMap selectedItem;
            selectedItem.insert(QStringLiteral("filePath"), filePath);
            selectedItem.insert(QStringLiteral("title"), QStringLiteral("Selected Smoke"));
            selectedItem.insert(QStringLiteral("artist"), QStringLiteral("Opusora"));

            QVariantMap nextItem;
            nextItem.insert(QStringLiteral("filePath"), filePath);
            nextItem.insert(QStringLiteral("title"), QStringLiteral("Next Smoke"));
            nextItem.insert(QStringLiteral("artist"), QStringLiteral("Opusora"));

            playback.clearQueue();
            playback.playQueue(QVariantList { invalidItem, selectedItem, nextItem }, 1);
            const bool selectedItemPlayed = playback.queueCount() == 2
                && playback.queueIndex() == 0
                && playback.title() == QStringLiteral("Selected Smoke");

            playback.clearQueue();
            playback.playQueue(QVariantList { invalidItem, nextItem }, 0);
            const bool invalidSelectionDidNotAdvance = playback.queueCount() == 0
                && playback.currentFilePath().isEmpty();

            qInfo() << "Play queue index smoke selected item"
                    << (selectedItemPlayed ? "ok" : "failed")
                    << "invalid selected item"
                    << (invalidSelectionDidNotAdvance ? "ok" : "failed");
            QCoreApplication::exit(selectedItemPlayed && invalidSelectionDidNotAdvance ? 0 : 1);
        });
    } else if (arguments.size() > 2 && arguments.at(1) == QStringLiteral("--playqueue-identity-smoke")) {
        const QString filePath = QDir::cleanPath(arguments.at(2));
        QTimer::singleShot(0, &playback, [&playback, filePath]() {
            QVariantMap firstItem;
            firstItem.insert(QStringLiteral("trackId"), 101);
            firstItem.insert(QStringLiteral("filePath"), filePath);
            firstItem.insert(QStringLiteral("title"), QStringLiteral("Identity Smoke 1"));
            firstItem.insert(QStringLiteral("artist"), QStringLiteral("Opusora"));

            QVariantMap selectedItem;
            selectedItem.insert(QStringLiteral("trackId"), 102);
            selectedItem.insert(QStringLiteral("filePath"), filePath);
            selectedItem.insert(QStringLiteral("title"), QStringLiteral("Identity Smoke 2"));
            selectedItem.insert(QStringLiteral("artist"), QStringLiteral("Opusora"));

            QVariantMap thirdItem;
            thirdItem.insert(QStringLiteral("trackId"), 103);
            thirdItem.insert(QStringLiteral("filePath"), filePath);
            thirdItem.insert(QStringLiteral("title"), QStringLiteral("Identity Smoke 3"));
            thirdItem.insert(QStringLiteral("artist"), QStringLiteral("Opusora"));

            playback.clearQueue();
            const QVariantList items { firstItem, selectedItem, thirdItem };
            playback.playQueue(items, 0);
            playback.playQueueItemByIdentity(items, 102, filePath);
            QTimer::singleShot(500, &playback, [&playback]() {
                const bool ok = playback.queueCount() == 3
                    && playback.queueIndex() == 1
                    && playback.title() == QStringLiteral("Identity Smoke 2");
                qInfo() << "Play queue identity smoke"
                        << "count" << playback.queueCount()
                        << "index" << playback.queueIndex()
                        << "title" << playback.title()
                        << (ok ? "ok" : "failed");
                playback.clearQueue();
                QCoreApplication::exit(ok ? 0 : 1);
            });
        });
    } else if (arguments.size() > 2 && arguments.at(1) == QStringLiteral("--queue-token-smoke")) {
        const QString filePath = QDir::cleanPath(arguments.at(2));
        QTimer::singleShot(0, &playback, [&playback, filePath]() {
            QVariantMap firstItem;
            firstItem.insert(QStringLiteral("filePath"), filePath);
            firstItem.insert(QStringLiteral("title"), QStringLiteral("Token Smoke 1"));
            firstItem.insert(QStringLiteral("artist"), QStringLiteral("Opusora"));

            QVariantMap selectedItem;
            selectedItem.insert(QStringLiteral("filePath"), filePath);
            selectedItem.insert(QStringLiteral("title"), QStringLiteral("Token Smoke 2"));
            selectedItem.insert(QStringLiteral("artist"), QStringLiteral("Opusora"));

            playback.clearQueue();
            playback.appendQueueItems(QVariantList { firstItem, selectedItem });
            const QVariantList queueItems = playback.queueItems();
            const qint64 selectedToken = queueItems.size() > 1
                ? queueItems.at(1).toMap().value(QStringLiteral("queueToken")).toLongLong()
                : 0;
            playback.playQueueToken(selectedToken);
            QTimer::singleShot(500, &playback, [&playback, selectedToken]() {
                const bool ok = playback.queueCount() == 2
                    && playback.queueIndex() == 1
                    && playback.title() == QStringLiteral("Token Smoke 2");
                qInfo() << "Queue token smoke"
                        << "token" << selectedToken
                        << "index" << playback.queueIndex()
                        << "title" << playback.title()
                        << (ok ? "ok" : "failed");
                playback.clearQueue();
                QCoreApplication::exit(ok ? 0 : 1);
            });
        });
    } else if (arguments.size() > 2 && arguments.at(1) == QStringLiteral("--playqueue-large-smoke")) {
        const QString selectedFilePath = QDir::cleanPath(arguments.at(2));
        QTimer::singleShot(0, &playback, [&playback, selectedFilePath]() {
            constexpr int itemCount = 1000;
            constexpr int selectedIndex = 700;
            QVariantList items;
            items.reserve(itemCount);
            for (int i = 0; i < itemCount; ++i) {
                QVariantMap item;
                item.insert(
                    QStringLiteral("filePath"),
                    i == selectedIndex
                        ? selectedFilePath
                        : QStringLiteral("/run/user/1000/gvfs/smb-share:server=openmediavault.local,share=data/music/large-smoke-%1.mp3").arg(i));
                item.insert(QStringLiteral("title"), QStringLiteral("Large Queue %1").arg(i));
                item.insert(QStringLiteral("artist"), QStringLiteral("Opusora"));
                items.append(item);
            }

            QElapsedTimer elapsed;
            elapsed.start();
            playback.playQueue(items, selectedIndex);
            const qint64 elapsedMs = elapsed.elapsed();
            const bool ok = playback.queueCount() == itemCount
                && playback.queueIndex() == selectedIndex
                && playback.title() == QStringLiteral("Large Queue 700")
                && elapsedMs < 1000;
            qInfo() << "Play queue large smoke count" << playback.queueCount()
                    << "index" << playback.queueIndex()
                    << "elapsed ms" << elapsedMs
                    << (ok ? "ok" : "failed");
            playback.clearQueue();
            QCoreApplication::exit(ok ? 0 : 1);
        });
    } else if (arguments.size() > 1 && arguments.at(1) == QStringLiteral("--empty-playback-smoke")) {
        QTimer::singleShot(0, &playback, [&playback]() {
            const QString emptyPath = QDir::temp().absoluteFilePath(QStringLiteral("opusora-empty-playback-smoke.wav"));
            QFile file(emptyPath);
            file.open(QIODevice::WriteOnly | QIODevice::Truncate);
            file.close();

            playback.openFile(emptyPath);
            QCoreApplication::processEvents();
            const bool ok = playback.playbackState() == QStringLiteral("error")
                && playback.errorMessage() == QStringLiteral("Audio file is empty or unreadable");
            qInfo() << "Empty playback smoke"
                    << playback.playbackState()
                    << playback.errorMessage()
                    << (ok ? "ok" : "failed");
            QFile::remove(emptyPath);
            QCoreApplication::exit(ok ? 0 : 1);
        });
    } else if (arguments.size() > 1 && arguments.at(1) == QStringLiteral("--unrecognizable-playback-smoke")) {
        QTimer::singleShot(0, &playback, [&playback]() {
            const QString corruptPath = QDir::temp().absoluteFilePath(QStringLiteral("opusora-unrecognizable-playback-smoke.mp3"));
            QFile file(corruptPath);
            file.open(QIODevice::WriteOnly | QIODevice::Truncate);
            file.write(QByteArray(8192, '\0'));
            file.close();

            playback.openFile(corruptPath);
            QCoreApplication::processEvents();
            const bool ok = playback.playbackState() == QStringLiteral("error")
                && playback.errorMessage() == QStringLiteral("Audio file is not recognizable or appears to be corrupted");
            qInfo() << "Unrecognizable playback smoke"
                    << playback.playbackState()
                    << playback.errorMessage()
                    << (ok ? "ok" : "failed");
            QFile::remove(corruptPath);
            QCoreApplication::exit(ok ? 0 : 1);
        });
    } else if (arguments.size() > 2 && arguments.at(1) == QStringLiteral("--replaygain-smoke")) {
        const QString filePath = QDir::cleanPath(arguments.at(2));
        QTimer::singleShot(0, &playback, [&playback, filePath]() {
            const double previousVolume = playback.volume();
            const bool previousReplayGainEnabled = playback.replayGainEnabled();

            QVariantMap item;
            item.insert(QStringLiteral("filePath"), filePath);
            item.insert(QStringLiteral("title"), QStringLiteral("ReplayGain Smoke"));
            item.insert(QStringLiteral("artist"), QStringLiteral("Opusora"));
            item.insert(QStringLiteral("hasReplayGainTrackGain"), true);
            item.insert(QStringLiteral("replayGainTrackGainDb"), -6.0);

            playback.setVolume(1.0);
            playback.setReplayGainEnabled(true);
            playback.appendQueueItem(item);
            QCoreApplication::processEvents();
            const QVariantMap queueItem = playback.queueItems().isEmpty()
                ? QVariantMap()
                : playback.queueItems().first().toMap();
            qInfo() << "ReplayGain smoke enabled" << playback.replayGainEnabled()
                    << "queue count" << playback.queueCount()
                    << "has gain" << queueItem.value(QStringLiteral("hasReplayGainTrackGain")).toBool()
                    << "gain db" << queueItem.value(QStringLiteral("replayGainTrackGainDb")).toDouble();
            playback.clearQueue();
            playback.setReplayGainEnabled(previousReplayGainEnabled);
            playback.setVolume(previousVolume);
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 2 && arguments.at(1) == QStringLiteral("--gapless-smoke")) {
        const QString filePath = QDir::cleanPath(arguments.at(2));
        QTimer::singleShot(0, &playback, [&playback, filePath]() {
            const bool previousGaplessEnabled = playback.gaplessPlaybackEnabled();

            QVariantMap firstItem;
            firstItem.insert(QStringLiteral("filePath"), filePath);
            firstItem.insert(QStringLiteral("title"), QStringLiteral("Gapless Smoke 1"));
            firstItem.insert(QStringLiteral("artist"), QStringLiteral("Opusora"));

            QVariantMap secondItem;
            secondItem.insert(QStringLiteral("filePath"), filePath);
            secondItem.insert(QStringLiteral("title"), QStringLiteral("Gapless Smoke 2"));
            secondItem.insert(QStringLiteral("artist"), QStringLiteral("Opusora"));

            playback.setGaplessPlaybackEnabled(true);
            playback.playQueue(QVariantList { firstItem, secondItem }, 0);
            qInfo() << "Gapless smoke immediate enabled" << playback.gaplessPlaybackEnabled()
                    << "queue count" << playback.queueCount()
                    << "index" << playback.queueIndex()
                    << "prepared" << playback.gaplessNextPrepared();
            QTimer::singleShot(300, &playback, [&playback, previousGaplessEnabled]() {
                qInfo() << "Gapless smoke delayed index" << playback.queueIndex()
                        << "prepared" << playback.gaplessNextPrepared();
                playback.clearQueue();
                playback.setGaplessPlaybackEnabled(previousGaplessEnabled);
                QCoreApplication::quit();
            });
        });
    } else if (arguments.size() > 2 && arguments.at(1) == QStringLiteral("--search")) {
        const QString searchQuery = arguments.mid(2).join(QLatin1Char(' '));
        QTimer::singleShot(0, &search, [&search, searchQuery]() {
            search.search(searchQuery);
            qInfo() << "Search returned" << search.count() << "rows for query" << search.query();
            QCoreApplication::quit();
        });
    } else if (arguments.size() > 1) {
        const QString filePath = arguments.at(1);
        QTimer::singleShot(0, &playback, [&playback, filePath]() {
            playback.openFile(filePath);
        });
    }

    return app.exec();
}
