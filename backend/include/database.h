#ifndef COURIER_DATABASE_H
#define COURIER_DATABASE_H

#include <stdbool.h>
#include <sqlite3.h>
#include <stddef.h>

#if defined(_WIN32)
#include <windows.h>
typedef CRITICAL_SECTION DatabaseMutex;
#else
#include <pthread.h>
typedef pthread_mutex_t DatabaseMutex;
#endif

typedef struct {
    sqlite3 *conn;
    DatabaseMutex mutex;
    char path[1024];
} Database;

bool db_open(Database *database, const char *path, char *error, size_t error_size);
void db_close(Database *database);
bool db_initialize_schema(Database *database, char *error, size_t error_size);
bool db_seed_demo(Database *database, const char *demo_password, char *error, size_t error_size);

void db_lock(Database *database);
void db_unlock(Database *database);
bool db_exec(Database *database, const char *sql, char *error, size_t error_size);
int db_scalar_int(Database *database, const char *sql, int fallback);

#endif
