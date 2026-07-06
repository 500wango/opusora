#pragma once

#include <QObject>

namespace opusora {

class SettingsService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString systemThemeMode READ systemThemeMode NOTIFY systemThemeChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool restoreQueueOnStartup READ restoreQueueOnStartup WRITE setRestoreQueueOnStartup NOTIFY restoreQueueOnStartupChanged)
    Q_PROPERTY(bool desktopNotificationsEnabled READ desktopNotificationsEnabled WRITE setDesktopNotificationsEnabled NOTIFY desktopNotificationsEnabledChanged)
    Q_PROPERTY(bool mediaKeysEnabled READ mediaKeysEnabled WRITE setMediaKeysEnabled NOTIFY mediaKeysEnabledChanged)
    Q_PROPERTY(bool replayGainEnabled READ replayGainEnabled WRITE setReplayGainEnabled NOTIFY replayGainEnabledChanged)
    Q_PROPERTY(bool gaplessPlaybackEnabled READ gaplessPlaybackEnabled WRITE setGaplessPlaybackEnabled NOTIFY gaplessPlaybackEnabledChanged)
    Q_PROPERTY(QString defaultSortOrder READ defaultSortOrder WRITE setDefaultSortOrder NOTIFY defaultSortOrderChanged)
    Q_PROPERTY(bool welcomeCompleted READ welcomeCompleted WRITE setWelcomeCompleted NOTIFY welcomeCompletedChanged)
    Q_PROPERTY(int windowWidth READ windowWidth NOTIFY windowSizeChanged)
    Q_PROPERTY(int windowHeight READ windowHeight NOTIFY windowSizeChanged)

public:
    explicit SettingsService(QObject* parent = nullptr);

    QString language() const;
    QString themeMode() const;
    QString systemThemeMode() const;
    double volume() const;
    bool restoreQueueOnStartup() const;
    bool desktopNotificationsEnabled() const;
    bool mediaKeysEnabled() const;
    bool replayGainEnabled() const;
    bool gaplessPlaybackEnabled() const;
    QString defaultSortOrder() const;
    bool welcomeCompleted() const;
    int windowWidth() const;
    int windowHeight() const;

    Q_INVOKABLE void setLanguage(const QString& language);
    Q_INVOKABLE void setThemeMode(const QString& themeMode);
    Q_INVOKABLE void setVolume(double volume);
    Q_INVOKABLE void setRestoreQueueOnStartup(bool restore);
    Q_INVOKABLE void setDesktopNotificationsEnabled(bool enabled);
    Q_INVOKABLE void setMediaKeysEnabled(bool enabled);
    Q_INVOKABLE void setReplayGainEnabled(bool enabled);
    Q_INVOKABLE void setGaplessPlaybackEnabled(bool enabled);
    Q_INVOKABLE void setDefaultSortOrder(const QString& sortOrder);
    Q_INVOKABLE void setWelcomeCompleted(bool completed);
    Q_INVOKABLE void setWindowSize(int width, int height);

signals:
    void languageChanged();
    void themeModeChanged();
    void systemThemeChanged();
    void volumeChanged();
    void restoreQueueOnStartupChanged();
    void desktopNotificationsEnabledChanged();
    void mediaKeysEnabledChanged();
    void replayGainEnabledChanged();
    void gaplessPlaybackEnabledChanged();
    void defaultSortOrderChanged();
    void welcomeCompletedChanged();
    void windowSizeChanged();

private:
    bool eventFilter(QObject* watched, QEvent* event) override;

    QString m_language;
    QString m_themeMode;
    QString m_systemThemeMode;
    double m_volume = 0.8;
    bool m_restoreQueueOnStartup = true;
    bool m_desktopNotificationsEnabled = true;
    bool m_mediaKeysEnabled = true;
    bool m_replayGainEnabled = false;
    bool m_gaplessPlaybackEnabled = true;
    QString m_defaultSortOrder = QStringLiteral("title");
    bool m_welcomeCompleted = false;
    int m_windowWidth = 1180;
    int m_windowHeight = 760;
};

} // namespace opusora
