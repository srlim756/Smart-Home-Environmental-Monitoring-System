#ifndef DB_H
#define DB_H

#include <sqlite3.h>
#include "common.h"

/* SQLite 数据库操作接口，WAL 模式支持读写并发 */

int  db_init(sqlite3 **db);                    /* 初始化数据库，创建表，启用 WAL 模式 */
int  db_insert_sensor(sqlite3 *db, const sensor_data_t *data); /* 插入传感器日志 */
int  db_insert_alarm(sqlite3 *db, sensor_type_t type, float value, const char *msg); /* 插入告警日志 */
int  db_query_recent(sqlite3 *db, sensor_type_t type, int limit, sensor_data_t *out); /* 查询最近 N 条数据 */
int  db_query_alert_count(sqlite3 *db);        /* 查询告警总数 */
void db_close(sqlite3 *db);                    /* 关闭数据库连接 */

#endif
