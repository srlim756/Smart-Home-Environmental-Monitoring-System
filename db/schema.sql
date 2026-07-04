CREATE TABLE IF NOT EXISTS sensor_log (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    type        INTEGER NOT NULL,
    value       REAL    NOT NULL,
    timestamp   INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS alarm_log (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    type        INTEGER NOT NULL,
    value       REAL    NOT NULL,
    message     TEXT,
    timestamp   INTEGER NOT NULL
);

PRAGMA journal_mode=WAL;
