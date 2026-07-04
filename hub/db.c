#include "db.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int db_init(sqlite3 **db) {
    int rc = sqlite3_open("smarthome.db", db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB] Failed to open: %s\n", sqlite3_errmsg(*db));
        return -1;
    }

    const char *sql =
        "CREATE TABLE IF NOT EXISTS sensor_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  type INTEGER NOT NULL,"
        "  value REAL NOT NULL,"
        "  timestamp INTEGER NOT NULL);"

        "CREATE TABLE IF NOT EXISTS alarm_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  type INTEGER NOT NULL,"
        "  value REAL NOT NULL,"
        "  message TEXT,"
        "  timestamp INTEGER NOT NULL);"

        "PRAGMA journal_mode=WAL;";

    char *err = NULL;
    if (sqlite3_exec(*db, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "[DB] Init error: %s\n", err);
        sqlite3_free(err);
        return -1;
    }

    printf("[DB] Initialized (WAL mode)\n");
    return 0;
}

int db_insert_sensor(sqlite3 *db, const sensor_data_t *data) {
    const char *sql = "INSERT INTO sensor_log (type, value, timestamp) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return -1;

    sqlite3_bind_int(stmt, 1, (int)data->type);
    sqlite3_bind_double(stmt, 2, data->value);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)data->timestamp);

    int rc = sqlite3_step(stmt) == SQLITE_DONE ? 0 : -1;
    sqlite3_finalize(stmt);
    return rc;
}

int db_insert_alarm(sqlite3 *db, sensor_type_t type, float value, const char *msg) {
    const char *sql = "INSERT INTO alarm_log (type, value, message, timestamp) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return -1;

    sqlite3_bind_int(stmt, 1, (int)type);
    sqlite3_bind_double(stmt, 2, value);
    sqlite3_bind_text(stmt, 3, msg, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, (sqlite3_int64)time(NULL));

    int rc = sqlite3_step(stmt) == SQLITE_DONE ? 0 : -1;
    sqlite3_finalize(stmt);
    return rc;
}

int db_query_recent(sqlite3 *db, sensor_type_t type, int limit, sensor_data_t *out) {
    const char *sql = "SELECT value, timestamp FROM sensor_log "
                      "WHERE type=? ORDER BY id DESC LIMIT ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return -1;

    sqlite3_bind_int(stmt, 1, (int)type);
    sqlite3_bind_int(stmt, 2, limit);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < limit) {
        out[count].type = type;
        out[count].value = (float)sqlite3_column_double(stmt, 0);
        out[count].timestamp = (uint32_t)sqlite3_column_int64(stmt, 1);
        count++;
    }
    sqlite3_finalize(stmt);
    return count;
}

int db_query_alert_count(sqlite3 *db) {
    const char *sql = "SELECT COUNT(*) FROM alarm_log;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return -1;

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

void db_close(sqlite3 *db) {
    if (db) sqlite3_close(db);
}
