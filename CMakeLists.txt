cmake_minimum_required(VERSION 3.22)

project(Opusora
    VERSION 0.1.0
    DESCRIPTION "A refined local music player for Linux"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(PkgConfig QUIET)

find_package(Qt6 6.2 REQUIRED COMPONENTS
    Core
    Gui
    DBus
    Network
    Qml
    Quick
    QuickControls2
    Sql
    Widgets
)

if(PkgConfig_FOUND)
    pkg_check_modules(GSTREAMER QUIET IMPORTED_TARGET gstreamer-1.0)
    pkg_check_modules(GSTREAMER_PBUTILS QUIET IMPORTED_TARGET gstreamer-pbutils-1.0)
endif()

set(OPUSORA_SOURCES
    app/main.cpp
    app/core/AppPaths.cpp
    app/core/Logger.cpp
    app/core/TextEncoding.cpp
    app/db/Database.cpp
    app/i18n/TranslationService.cpp
    app/integration/MprisService.cpp
    app/integration/NotificationService.cpp
    app/integration/TrayService.cpp
    app/library/AlbumListModel.cpp
    app/library/ArtistListModel.cpp
    app/library/GenreListModel.cpp
    app/library/LibraryController.cpp
    app/library/LibraryRepository.cpp
    app/library/LibraryScanner.cpp
    app/library/TrackListModel.cpp
    app/metadata/BasicMetadataReader.cpp
    app/metadata/MetadataReaderFactory.cpp
    app/metadata/OnlineMetadataService.cpp
    app/player/NullAudioPlayer.cpp
    app/player/PlaybackController.cpp
    app/playlist/PlaylistListModel.cpp
    app/playlist/PlaylistRepository.cpp
    app/radio/RadioStationModel.cpp
    app/search/SearchController.cpp
    app/settings/SettingsService.cpp
)

set(OPUSORA_HEADERS
    app/core/AppPaths.hpp
    app/core/Logger.hpp
    app/core/TextEncoding.hpp
    app/db/Database.hpp
    app/i18n/TranslationService.hpp
    app/integration/MprisService.hpp
    app/integration/NotificationService.hpp
    app/integration/TrayService.hpp
    app/library/AlbumListModel.hpp
    app/library/ArtistListModel.hpp
    app/library/GenreListModel.hpp
    app/library/LibraryController.hpp
    app/library/LibraryRepository.hpp
    app/library/LibraryScanner.hpp
    app/library/TrackListModel.hpp
    app/metadata/BasicMetadataReader.hpp
    app/metadata/GStreamerMetadataReader.hpp
    app/metadata/MetadataReader.hpp
    app/metadata/MetadataReaderFactory.hpp
    app/metadata/MetadataTypes.hpp
    app/metadata/OnlineMetadataService.hpp
    app/player/GStreamerPlayer.hpp
    app/player/IAudioPlayer.hpp
    app/player/NullAudioPlayer.hpp
    app/player/PlaybackController.hpp
    app/player/PlaybackState.hpp
    app/playlist/PlaylistListModel.hpp
    app/playlist/PlaylistRepository.hpp
    app/playlist/QueueService.hpp
    app/radio/RadioStationModel.hpp
    app/search/SearchController.hpp
    app/settings/SettingsService.hpp
)

qt_add_executable(opusora
    ${OPUSORA_SOURCES}
    ${OPUSORA_HEADERS}
)

qt_add_resources(opusora "opusora_migrations"
    PREFIX "/"
    FILES
        app/db/migrations/001_initial.sql
)

set_source_files_properties(app/ui/qml/theme/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(opusora
    URI Opusora
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        app/ui/qml/AppWindow.qml
        app/ui/qml/components/AddToPlaylistButton.qml
        app/ui/qml/components/AddToQueueButton.qml
        app/ui/qml/components/CoverImage.qml
        app/ui/qml/components/EmptyState.qml
        app/ui/qml/components/GlassButton.qml
        app/ui/qml/components/GlassComboBox.qml
        app/ui/qml/components/GlassPanel.qml
        app/ui/qml/components/GlassSwitch.qml
        app/ui/qml/components/GlassTextArea.qml
        app/ui/qml/components/GlassTextField.qml
        app/ui/qml/components/IconButton.qml
        app/ui/qml/components/MacTitleBar.qml
        app/ui/qml/components/PlayerBar.qml
        app/ui/qml/components/ProgressSlider.qml
        app/ui/qml/components/SearchBox.qml
        app/ui/qml/components/Sidebar.qml
        app/ui/qml/components/SidebarItem.qml
        app/ui/qml/components/WindowTrafficButton.qml
        app/ui/qml/pages/AlbumsPage.qml
        app/ui/qml/pages/ArtistsPage.qml
        app/ui/qml/pages/GenresPage.qml
        app/ui/qml/pages/LibraryPage.qml
        app/ui/qml/pages/NowPlayingPage.qml
        app/ui/qml/pages/PlaylistsPage.qml
        app/ui/qml/pages/QueuePage.qml
        app/ui/qml/pages/RadioPage.qml
        app/ui/qml/pages/SearchResultsPage.qml
        app/ui/qml/pages/SettingsPage.qml
        app/ui/qml/pages/SongsPage.qml
        app/ui/qml/pages/WelcomePage.qml
        app/ui/qml/theme/Theme.qml
)

target_include_directories(opusora PRIVATE app)

target_link_libraries(opusora PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::DBus
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
    Qt6::Widgets
)

if(GSTREAMER_FOUND)
    target_sources(opusora PRIVATE app/player/GStreamerPlayer.cpp)
    target_link_libraries(opusora PRIVATE PkgConfig::GSTREAMER)
    target_compile_definitions(opusora PRIVATE OPUSORA_WITH_GSTREAMER=1)
    message(STATUS "GStreamer backend enabled")
else()
    message(STATUS "GStreamer development package not found; using NullAudioPlayer")
endif()

if(GSTREAMER_PBUTILS_FOUND)
    target_sources(opusora PRIVATE app/metadata/GStreamerMetadataReader.cpp)
    target_link_libraries(opusora PRIVATE PkgConfig::GSTREAMER_PBUTILS)
    target_compile_definitions(opusora PRIVATE OPUSORA_WITH_GSTREAMER_METADATA=1)
    message(STATUS "GStreamer metadata reader enabled")
else()
    message(STATUS "GStreamer pbutils development package not found; using basic metadata fallback")
endif()

include(GNUInstallDirs)

enable_testing()

add_test(
    NAME opusora_app_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen OPUSORA_TEST_DISABLE_MEDIA_KEYS=1 XDG_CONFIG_HOME=/tmp/opusora-app-ctest-config XDG_DATA_HOME=/tmp/opusora-app-ctest-data $<TARGET_FILE:opusora> --app-smoke
)

add_test(
    NAME opusora_encoding_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen OPUSORA_TEST_DISABLE_MEDIA_KEYS=1 XDG_CONFIG_HOME=/tmp/opusora-encoding-ctest-config XDG_DATA_HOME=/tmp/opusora-encoding-ctest-data $<TARGET_FILE:opusora> --encoding-smoke
)

add_test(
    NAME opusora_online_metadata_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen OPUSORA_TEST_DISABLE_MEDIA_KEYS=1 XDG_CONFIG_HOME=/tmp/opusora-online-metadata-ctest-config XDG_DATA_HOME=/tmp/opusora-online-metadata-ctest-data $<TARGET_FILE:opusora> --online-metadata-smoke
)

add_test(
    NAME opusora_search_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen OPUSORA_TEST_DISABLE_MEDIA_KEYS=1 XDG_CONFIG_HOME=/tmp/opusora-search-ctest-config XDG_DATA_HOME=/tmp/opusora-search-ctest-data $<TARGET_FILE:opusora> --search test
)

add_test(
    NAME opusora_restore_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen OPUSORA_TEST_DISABLE_MEDIA_KEYS=1 XDG_CONFIG_HOME=/tmp/opusora-restore-ctest-config XDG_DATA_HOME=/tmp/opusora-restore-ctest-data $<TARGET_FILE:opusora> --restore-smoke
)

add_test(
    NAME opusora_recent_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen OPUSORA_TEST_DISABLE_MEDIA_KEYS=1 XDG_CONFIG_HOME=/tmp/opusora-recent-ctest-config XDG_DATA_HOME=/tmp/opusora-recent-ctest-data $<TARGET_FILE:opusora> --recent-smoke
)

add_test(
    NAME opusora_duplicates_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen OPUSORA_TEST_DISABLE_MEDIA_KEYS=1 XDG_CONFIG_HOME=/tmp/opusora-duplicates-ctest-config XDG_DATA_HOME=/tmp/opusora-duplicates-ctest-data $<TARGET_FILE:opusora> --duplicates-smoke
)

add_test(
    NAME opusora_playqueue_index_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen OPUSORA_TEST_DISABLE_MEDIA_KEYS=1 OPUSORA_GST_FAKE_AUDIO_SINK=1 XDG_CONFIG_HOME=/tmp/opusora-playqueue-index-ctest-config XDG_DATA_HOME=/tmp/opusora-playqueue-index-ctest-data $<TARGET_FILE:opusora> --playqueue-index-smoke $<TARGET_FILE:opusora>
)

add_test(
    NAME opusora_playqueue_identity_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen OPUSORA_TEST_DISABLE_MEDIA_KEYS=1 OPUSORA_GST_FAKE_AUDIO_SINK=1 XDG_CONFIG_HOME=/tmp/opusora-playqueue-identity-ctest-config XDG_DATA_HOME=/tmp/opusora-playqueue-identity-ctest-data $<TARGET_FILE:opusora> --playqueue-identity-smoke $<TARGET_FILE:opusora>
)

add_test(
    NAME opusora_queue_token_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen OPUSORA_TEST_DISABLE_MEDIA_KEYS=1 OPUSORA_GST_FAKE_AUDIO_SINK=1 XDG_CONFIG_HOME=/tmp/opusora-queue-token-ctest-config XDG_DATA_HOME=/tmp/opusora-queue-token-ctest-data $<TARGET_FILE:opusora> --queue-token-smoke $<TARGET_FILE:opusora>
)

add_test(
    NAME opusora_playlist_bulk_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen OPUSORA_TEST_DISABLE_MEDIA_KEYS=1 XDG_CONFIG_HOME=/tmp/opusora-playlist-bulk-ctest-config XDG_DATA_HOME=/tmp/opusora-playlist-bulk-ctest-data $<TARGET_FILE:opusora> --playlist-bulk-smoke
)

add_test(
    NAME opusora_playqueue_large_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen OPUSORA_TEST_DISABLE_MEDIA_KEYS=1 OPUSORA_GST_FAKE_AUDIO_SINK=1 XDG_CONFIG_HOME=/tmp/opusora-playqueue-large-ctest-config XDG_DATA_HOME=/tmp/opusora-playqueue-large-ctest-data $<TARGET_FILE:opusora> --playqueue-large-smoke $<TARGET_FILE:opusora>
)

add_test(
    NAME opusora_empty_playback_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen OPUSORA_TEST_DISABLE_MEDIA_KEYS=1 OPUSORA_GST_FAKE_AUDIO_SINK=1 XDG_CONFIG_HOME=/tmp/opusora-empty-playback-ctest-config XDG_DATA_HOME=/tmp/opusora-empty-playback-ctest-data $<TARGET_FILE:opusora> --empty-playback-smoke
)

add_test(
    NAME opusora_unrecognizable_playback_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen OPUSORA_TEST_DISABLE_MEDIA_KEYS=1 OPUSORA_GST_FAKE_AUDIO_SINK=1 XDG_CONFIG_HOME=/tmp/opusora-unrecognizable-playback-ctest-config XDG_DATA_HOME=/tmp/opusora-unrecognizable-playback-ctest-data $<TARGET_FILE:opusora> --unrecognizable-playback-smoke
)

add_test(
    NAME opusora_replaygain_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen OPUSORA_TEST_DISABLE_MEDIA_KEYS=1 OPUSORA_GST_FAKE_AUDIO_SINK=1 XDG_CONFIG_HOME=/tmp/opusora-replaygain-ctest-config XDG_DATA_HOME=/tmp/opusora-replaygain-ctest-data $<TARGET_FILE:opusora> --replaygain-smoke $<TARGET_FILE:opusora>
)

add_test(
    NAME opusora_gapless_smoke
    COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen OPUSORA_TEST_DISABLE_MEDIA_KEYS=1 OPUSORA_GST_FAKE_AUDIO_SINK=1 XDG_CONFIG_HOME=/tmp/opusora-gapless-ctest-config XDG_DATA_HOME=/tmp/opusora-gapless-ctest-data $<TARGET_FILE:opusora> --gapless-smoke $<TARGET_FILE:opusora>
)

set(OPUSORA_SMOKE_TESTS
    opusora_app_smoke
    opusora_encoding_smoke
    opusora_online_metadata_smoke
    opusora_search_smoke
    opusora_restore_smoke
    opusora_recent_smoke
    opusora_duplicates_smoke
    opusora_playqueue_index_smoke
    opusora_playqueue_identity_smoke
    opusora_queue_token_smoke
    opusora_playlist_bulk_smoke
    opusora_playqueue_large_smoke
    opusora_empty_playback_smoke
    opusora_unrecognizable_playback_smoke
    opusora_replaygain_smoke
    opusora_gapless_smoke
)

set_tests_properties(${OPUSORA_SMOKE_TESTS}
    PROPERTIES
        ENVIRONMENT "QT_QUICK_BACKEND=software;QSG_RENDER_LOOP=basic;LIBGL_ALWAYS_SOFTWARE=1;OPUSORA_GST_FAKE_AUDIO_SINK=1"
)

install(TARGETS opusora
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES packaging/linux/opusora.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)

install(FILES packaging/linux/opusora.xml
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/mime/packages
)

install(FILES packaging/linux/opusora.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo
)

install(FILES packaging/linux/icons/hicolor/scalable/apps/opusora.svg
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
)

if(CMAKE_LIBRARY_ARCHITECTURE AND NOT CMAKE_INSTALL_LIBDIR MATCHES "${CMAKE_LIBRARY_ARCHITECTURE}$")
    set(OPUSORA_QML_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/${CMAKE_LIBRARY_ARCHITECTURE}/qt6/qml/Opusora")
else()
    set(OPUSORA_QML_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/qt6/qml/Opusora")
endif()

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Opusora/
    DESTINATION ${OPUSORA_QML_INSTALL_DIR}
)

set(CPACK_PACKAGE_NAME "Opusora")
set(CPACK_PACKAGE_VENDOR "Opusora")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A refined local music player for Linux")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Opusora")
set(CPACK_GENERATOR "TGZ;DEB;RPM")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Opusora")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://opusora.local")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libqt6sql6-sqlite, qml6-module-qtqml-workerscript, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-templates, qml6-module-qtquick-window, qt6-qpa-plugins, gstreamer1.0-plugins-base, gstreamer1.0-plugins-good, gstreamer1.0-plugins-bad, gstreamer1.0-plugins-ugly, gstreamer1.0-libav"
)
set(CPACK_RPM_PACKAGE_LICENSE "Proprietary")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_RELEASE 1)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
include(CPack)
