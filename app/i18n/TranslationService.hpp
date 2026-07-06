#pragma once

#include <QHash>
#include <QObject>

namespace opusora {

class TranslationService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(int revision READ revision NOTIFY languageChanged)

public:
    explicit TranslationService(QObject* parent = nullptr);

    QString language() const;
    int revision() const;

    Q_INVOKABLE QString t(const QString& key) const;

public slots:
    void setLanguage(const QString& language);

signals:
    void languageChanged();

private:
    QHash<QString, QString> activeDictionary() const;

    QString m_language = QStringLiteral("en-US");
    int m_revision = 0;
    QHash<QString, QString> m_en;
    QHash<QString, QString> m_zh;
};

} // namespace opusora
