#include "settings/SettingsService.hpp"

#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QLocale>
#include <QPalette>
#include <QSettings>

#include <algorithm>

namespace {

bool isSupportedLanguage(const QString& language)
{
    return language == QStringLiteral("zh-CN") || language == QStringLiteral("en-US");
}

bool isSupportedThemeMode(const QString& themeMode)
{
    return themeMode == QStringLiteral("light")
        || themeMode == QStringLiteral("dark")
        || themeMode == QStringLiteral("system");
}

bool isSupportedSortOrder(const QString& sortOrder)
{
    return sortOrder == QStringLiteral("title")
        || sortOrder == QStringLiteral("artist")
        || sortOrder == QStringLiteral("album")
        || sortOrder == QStringLiteral("recent")
        || sortOrder == QStringLiteral("duration");
}

QString defaultLanguageForSystem()
{
    return QLocale::system().language() == QLocale::Chinese
        ? QStringLiteral("zh-CN")
        : QStringLiteral("en-US");
}

QString detectSystemThemeMode()
{
    const QColor windowColor = QGuiApplication::palette().color(QPalette::Window);
    return windowColor.lightness() < 128 ? QStringLiteral("dark") : QStringLiteral("light");
}

} // namespace

namespace opusora {

SettingsService::SettingsService(QObject* parent)
    : QObject(parent)
{
    QSettings settings;
    m_language = settings.value(QStringLiteral("language"), defaultLanguageForSystem()).toString();
    if (!isSupportedLanguage(m_language)) {
        m_language = defaultLanguageForSystem();
    }

    m_themeMode = settings.value(QStringLiteral("theme_mode"), QStringLiteral("light")).toString();
    if (!isSupportedThemeMode(m_themeMode)) {
        m_themeMode = QStringLiteral("light");
    }
    m_systemThemeMode = detectSystemThemeMode();
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->installEventFilter(this);
    }

    m_volume = settings.value(QStringLiteral("volume"), 0.8).toDouble();
    m_volume = std::max(0.0, std::min(m_volume, 1.0));

    m_restoreQueueOnStartup = settings.value(QStringLiteral("restore_queue_on_startup"), true).toBool();
    m_desktopNotificationsEnabled = settings.value(QStringLiteral("desktop_notifications_enabled"), true).toBool();
    m_mediaKeysEnabled = settings.value(QStringLiteral("media_keys_enabled"), true).toBool();
    m_replayGainEnabled = settings.value(QStringLiteral("playback/replaygain_enabled"), false).toBool();
    m_gaplessPlaybackEnabled = settings.value(QStringLiteral("playback/gapless_enabled"), true).toBool();
    m_defaultSortOrder = settings.value(QStringLiteral("default_sort_order"), QStringLiteral("title")).toString();
    if (!isSupportedSortOrder(m_defaultSortOrder)) {
        m_defaultSortOrder = QStringLiteral("title");
    }
    m_welcomeCompleted = settings.value(QStringLiteral("welcome_completed"), false).toBool();
    m_windowWidth = std::max(920, settings.value(QStringLiteral("window/width"), 1180).toInt());
    m_windowHeight = std::max(620, settings.value(QStringLiteral("window/height"), 760).toInt());
}

QString SettingsService::language() const
{
    return m_language;
}

QString SettingsService::themeMode() const
{
    return m_themeMode;
}

QString SettingsService::systemThemeMode() const
{
    return m_systemThemeMode;
}

double SettingsService::volume() const
{
    return m_volume;
}

bool SettingsService::restoreQueueOnStartup() const
{
    return m_restoreQueueOnStartup;
}

bool SettingsService::desktopNotificationsEnabled() const
{
    return m_desktopNotificationsEnabled;
}

bool SettingsService::mediaKeysEnabled() const
{
    return m_mediaKeysEnabled;
}

bool SettingsService::replayGainEnabled() const
{
    return m_replayGainEnabled;
}

bool SettingsService::gaplessPlaybackEnabled() const
{
    return m_gaplessPlaybackEnabled;
}

QString SettingsService::defaultSortOrder() const
{
    return m_defaultSortOrder;
}

bool SettingsService::welcomeCompleted() const
{
    return m_welcomeCompleted;
}

int SettingsService::windowWidth() const
{
    return m_windowWidth;
}

int SettingsService::windowHeight() const
{
    return m_windowHeight;
}

void SettingsService::setLanguage(const QString& language)
{
    const QString normalized = isSupportedLanguage(language) ? language : QStringLiteral("en-US");
    if (m_language == normalized) {
        return;
    }

    m_language = normalized;
    QSettings().setValue(QStringLiteral("language"), m_language);
    emit languageChanged();
}

void SettingsService::setThemeMode(const QString& themeMode)
{
    const QString normalized = isSupportedThemeMode(themeMode) ? themeMode : QStringLiteral("light");
    if (m_themeMode == normalized) {
        return;
    }

    m_themeMode = normalized;
    QSettings().setValue(QStringLiteral("theme_mode"), m_themeMode);
    emit themeModeChanged();
}

void SettingsService::setVolume(double volume)
{
    const double bounded = std::max(0.0, std::min(volume, 1.0));
    if (qFuzzyCompare(m_volume, bounded)) {
        return;
    }

    m_volume = bounded;
    QSettings().setValue(QStringLiteral("volume"), m_volume);
    emit volumeChanged();
}

void SettingsService::setRestoreQueueOnStartup(bool restore)
{
    if (m_restoreQueueOnStartup == restore) {
        return;
    }

    m_restoreQueueOnStartup = restore;
    QSettings().setValue(QStringLiteral("restore_queue_on_startup"), m_restoreQueueOnStartup);
    emit restoreQueueOnStartupChanged();
}

void SettingsService::setDesktopNotificationsEnabled(bool enabled)
{
    if (m_desktopNotificationsEnabled == enabled) {
        return;
    }

    m_desktopNotificationsEnabled = enabled;
    QSettings().setValue(QStringLiteral("desktop_notifications_enabled"), m_desktopNotificationsEnabled);
    emit desktopNotificationsEnabledChanged();
}

void SettingsService::setMediaKeysEnabled(bool enabled)
{
    if (m_mediaKeysEnabled == enabled) {
        return;
    }

    m_mediaKeysEnabled = enabled;
    QSettings().setValue(QStringLiteral("media_keys_enabled"), m_mediaKeysEnabled);
    emit mediaKeysEnabledChanged();
}

void SettingsService::setReplayGainEnabled(bool enabled)
{
    if (m_replayGainEnabled == enabled) {
        return;
    }

    m_replayGainEnabled = enabled;
    QSettings().setValue(QStringLiteral("playback/replaygain_enabled"), m_replayGainEnabled);
    emit replayGainEnabledChanged();
}

void SettingsService::setGaplessPlaybackEnabled(bool enabled)
{
    if (m_gaplessPlaybackEnabled == enabled) {
        return;
    }

    m_gaplessPlaybackEnabled = enabled;
    QSettings().setValue(QStringLiteral("playback/gapless_enabled"), m_gaplessPlaybackEnabled);
    emit gaplessPlaybackEnabledChanged();
}

void SettingsService::setDefaultSortOrder(const QString& sortOrder)
{
    const QString normalized = isSupportedSortOrder(sortOrder) ? sortOrder : QStringLiteral("title");
    if (m_defaultSortOrder == normalized) {
        return;
    }

    m_defaultSortOrder = normalized;
    QSettings().setValue(QStringLiteral("default_sort_order"), m_defaultSortOrder);
    emit defaultSortOrderChanged();
}

void SettingsService::setWelcomeCompleted(bool completed)
{
    if (m_welcomeCompleted == completed) {
        return;
    }

    m_welcomeCompleted = completed;
    QSettings().setValue(QStringLiteral("welcome_completed"), m_welcomeCompleted);
    emit welcomeCompletedChanged();
}

void SettingsService::setWindowSize(int width, int height)
{
    const int boundedWidth = std::max(920, width);
    const int boundedHeight = std::max(620, height);
    if (m_windowWidth == boundedWidth && m_windowHeight == boundedHeight) {
        return;
    }

    m_windowWidth = boundedWidth;
    m_windowHeight = boundedHeight;
    QSettings settings;
    settings.setValue(QStringLiteral("window/width"), m_windowWidth);
    settings.setValue(QStringLiteral("window/height"), m_windowHeight);
    emit windowSizeChanged();
}

bool SettingsService::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched);
    if (event && (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::PaletteChange)) {
        const QString nextMode = detectSystemThemeMode();
        if (m_systemThemeMode != nextMode) {
            m_systemThemeMode = nextMode;
            emit systemThemeChanged();
        }
    }
    return QObject::eventFilter(watched, event);
}

} // namespace opusora
