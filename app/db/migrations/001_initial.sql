CREATE TABLE IF NOT EXISTS track (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_path TEXT NOT NULL UNIQUE,
    file_hash TEXT,
    title TEXT,
    title_sort TEXT,
    display_artist TEXT,
    display_album TEXT,
    display_album_artist TEXT,
    album_id INTEGER,
    cover_id INTEGER,
    genre TEXT,
    year INTEGER,
    release_date TEXT,
    track_number INTEGER,
    disc_number INTEGER,
    duration_ms INTEGER,
    bitrate INTEGER,
    sample_rate INTEGER,
    channels INTEGER,
    format TEXT,
    file_size INTEGER,
    modified_at INTEGER,
    added_at INTEGER,
    last_played_at INTEGER,
    play_count INTEGER DEFAULT 0,
    composer TEXT,
    comment TEXT,
    bpm INTEGER,
    replaygain_track_gain REAL,
    replaygain_album_gain REAL,
    lyrics_text TEXT,
    metadata_read_status TEXT,
    metadata_encoding_status TEXT,
    created_at INTEGER,
    updated_at INTEGER,
    FOREIGN KEY(album_id) REFERENCES album(id),
    FOREIGN KEY(cover_id) REFERENCES cover(id)
);

CREATE TABLE IF NOT EXISTS album (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL,
    title_sort TEXT,
    album_artist_id INTEGER,
    display_album_artist TEXT,
    cover_id INTEGER,
    year INTEGER,
    release_date TEXT,
    genre TEXT,
    track_count INTEGER DEFAULT 0,
    duration_ms INTEGER DEFAULT 0,
    created_at INTEGER,
    updated_at INTEGER,
    FOREIGN KEY(album_artist_id) REFERENCES artist(id),
    FOREIGN KEY(cover_id) REFERENCES cover(id)
);

CREATE TABLE IF NOT EXISTS artist (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,
    name_sort TEXT,
    album_count INTEGER DEFAULT 0,
    track_count INTEGER DEFAULT 0,
    created_at INTEGER,
    updated_at INTEGER
);

CREATE TABLE IF NOT EXISTS track_artist (
    track_id INTEGER NOT NULL,
    artist_id INTEGER NOT NULL,
    role TEXT NOT NULL DEFAULT 'primary',
    position INTEGER DEFAULT 0,
    PRIMARY KEY (track_id, artist_id, role),
    FOREIGN KEY(track_id) REFERENCES track(id) ON DELETE CASCADE,
    FOREIGN KEY(artist_id) REFERENCES artist(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS playlist (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    description TEXT,
    sort_order INTEGER DEFAULT 0,
    created_at INTEGER,
    updated_at INTEGER
);

CREATE TABLE IF NOT EXISTS playlist_track (
    playlist_id INTEGER NOT NULL,
    track_id INTEGER NOT NULL,
    position INTEGER NOT NULL,
    added_at INTEGER,
    PRIMARY KEY (playlist_id, track_id, position),
    FOREIGN KEY(playlist_id) REFERENCES playlist(id) ON DELETE CASCADE,
    FOREIGN KEY(track_id) REFERENCES track(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS cover (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_track_id INTEGER,
    image_hash TEXT,
    image_path TEXT NOT NULL,
    mime_type TEXT,
    width INTEGER,
    height INTEGER,
    created_at INTEGER,
    FOREIGN KEY(source_track_id) REFERENCES track(id)
);

CREATE TABLE IF NOT EXISTS library_root (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    path TEXT NOT NULL UNIQUE,
    enabled INTEGER NOT NULL DEFAULT 1,
    last_scanned_at INTEGER,
    created_at INTEGER,
    updated_at INTEGER
);

CREATE TABLE IF NOT EXISTS scan_job (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    library_root_id INTEGER,
    status TEXT NOT NULL,
    total_files INTEGER DEFAULT 0,
    scanned_files INTEGER DEFAULT 0,
    failed_files INTEGER DEFAULT 0,
    started_at INTEGER,
    finished_at INTEGER,
    error_message TEXT,
    FOREIGN KEY(library_root_id) REFERENCES library_root(id)
);

CREATE TABLE IF NOT EXISTS play_queue (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    track_id INTEGER NOT NULL,
    position INTEGER NOT NULL,
    is_current INTEGER NOT NULL DEFAULT 0,
    added_at INTEGER,
    FOREIGN KEY(track_id) REFERENCES track(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS app_setting (
    key TEXT PRIMARY KEY,
    value TEXT,
    updated_at INTEGER
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_track_file_path ON track(file_path);
CREATE INDEX IF NOT EXISTS idx_track_title ON track(title);
CREATE INDEX IF NOT EXISTS idx_track_album_id ON track(album_id);
CREATE INDEX IF NOT EXISTS idx_track_added_at ON track(added_at);
CREATE INDEX IF NOT EXISTS idx_track_last_played_at ON track(last_played_at);
CREATE INDEX IF NOT EXISTS idx_album_title ON album(title);
CREATE INDEX IF NOT EXISTS idx_artist_name ON artist(name);
CREATE INDEX IF NOT EXISTS idx_playlist_track_playlist_position ON playlist_track(playlist_id, position);
CREATE INDEX IF NOT EXISTS idx_queue_position ON play_queue(position);
