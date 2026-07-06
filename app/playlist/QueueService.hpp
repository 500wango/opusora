#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <utility>

namespace opusora {

struct QueueItem {
    qint64 queueToken = 0;
    int trackId = 0;
    QString filePath;
    QString title;
    QString artist;
    QString album;
    QString coverUrl;
    double replayGainTrackGainDb = 0.0;
    double replayGainAlbumGainDb = 0.0;
    bool hasReplayGainTrackGain = false;
    bool hasReplayGainAlbumGain = false;
};

class QueueService {
public:
    void setItems(QList<QueueItem> items, int currentIndex)
    {
        m_items = std::move(items);
        if (m_items.isEmpty()) {
            m_currentIndex = -1;
            return;
        }

        const int lastIndex = static_cast<int>(m_items.size()) - 1;
        m_currentIndex = std::max(0, std::min(currentIndex, lastIndex));
    }

    void clear()
    {
        m_items.clear();
        m_currentIndex = -1;
    }

    bool empty() const { return m_items.isEmpty(); }
    int count() const { return static_cast<int>(m_items.size()); }
    int currentIndex() const { return m_currentIndex; }
    bool hasPrevious() const { return m_currentIndex > 0; }
    bool hasNext() const { return m_currentIndex >= 0 && m_currentIndex < count() - 1; }
    const QList<QueueItem>& items() const { return m_items; }

    void append(QueueItem item)
    {
        m_items.append(std::move(item));
        if (m_currentIndex < 0) {
            m_currentIndex = 0;
        }
    }

    bool setCurrentIndex(int index)
    {
        if (index < 0 || index >= count()) {
            return false;
        }

        m_currentIndex = index;
        return true;
    }

    const QueueItem* current() const
    {
        if (m_currentIndex < 0 || m_currentIndex >= count()) {
            return nullptr;
        }

        return &m_items.at(m_currentIndex);
    }

    bool updateCurrentDisplay(const QString& title, const QString& artist, const QString& album)
    {
        if (m_currentIndex < 0 || m_currentIndex >= count()) {
            return false;
        }

        m_items[m_currentIndex].title = title;
        m_items[m_currentIndex].artist = artist;
        m_items[m_currentIndex].album = album;
        return true;
    }

    const QueueItem* movePrevious()
    {
        if (!hasPrevious()) {
            return nullptr;
        }

        --m_currentIndex;
        return current();
    }

    const QueueItem* moveNext()
    {
        if (!hasNext()) {
            return nullptr;
        }

        ++m_currentIndex;
        return current();
    }

    bool removeAt(int index)
    {
        if (index < 0 || index >= count()) {
            return false;
        }

        m_items.removeAt(index);
        if (m_items.isEmpty()) {
            m_currentIndex = -1;
            return true;
        }

        if (index < m_currentIndex) {
            --m_currentIndex;
        } else if (index == m_currentIndex && m_currentIndex >= count()) {
            m_currentIndex = count() - 1;
        }

        return true;
    }

    bool moveAt(int from, int to)
    {
        if (from < 0 || from >= count() || to < 0 || to >= count()) {
            return false;
        }
        if (from == to) {
            return true;
        }

        QueueItem item = m_items.takeAt(from);
        m_items.insert(to, std::move(item));

        if (m_currentIndex == from) {
            m_currentIndex = to;
        } else if (from < m_currentIndex && to >= m_currentIndex) {
            --m_currentIndex;
        } else if (from > m_currentIndex && to <= m_currentIndex) {
            ++m_currentIndex;
        }

        return true;
    }

private:
    QList<QueueItem> m_items;
    int m_currentIndex = -1;
};

} // namespace opusora
