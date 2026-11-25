#include "database.h"
#include "auth.h"
#include "utils.h"
#include "validation.h"

#include <stdio.h>
#include <string.h>

static void set_error(char *error, size_t error_size, const char *message) {
    if (error != NULL && error_size > 0) snprintf(error, error_size, "%s", message == NULL ? "Database operation failed" : message);
}

static bool mutex_initialize(DatabaseMutex *mutex) {
#if defined(_WIN32)
    InitializeCriticalSection(mutex);
    return true;
#else
    return pthread_mutex_init(mutex, NULL) == 0;
#endif
}

static void mutex_destroy(DatabaseMutex *mutex) {
#if defined(_WIN32)
    DeleteCriticalSection(mutex);
#else
    pthread_mutex_destroy(mutex);
#endif
}

void db_lock(Database *database) {
    if (database == NULL) return;
#if defined(_WIN32)
    EnterCriticalSection(&database->mutex);
#else
    pthread_mutex_lock(&database->mutex);
#endif
}

void db_unlock(Database *database) {
    if (database == NULL) return;
#if defined(_WIN32)
    LeaveCriticalSection(&database->mutex);
#else
    pthread_mutex_unlock(&database->mutex);
#endif
}

bool db_open(Database *database, const char *path, char *error, size_t error_size) {
    int result;
    if (database == NULL || path == NULL) { set_error(error, error_size, "Invalid database configuration"); return false; }
    memset(database, 0, sizeof(*database));
    copy_string(database->path, sizeof(database->path), path);
    if (!mutex_initialize(&database->mutex)) { set_error(error, error_size, "Unable to initialize database lock"); return false; }
    result = sqlite3_open_v2(path, &database->conn, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
    if (result != SQLITE_OK) {
        set_error(error, error_size, database->conn ? sqlite3_errmsg(database->conn) : "Unable to open database");
        if (database->conn != NULL) sqlite3_close(database->conn);
        database->conn = NULL;
        mutex_destroy(&database->mutex);
        return false;
    }
    sqlite3_busy_timeout(database->conn, 5000);
    if (!db_exec(database, "PRAGMA foreign_keys = ON; PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL;", error, error_size)) {
        db_close(database);
        return false;
    }
    return true;
}

void db_close(Database *database) {
    if (database == NULL) return;
    if (database->conn != NULL) sqlite3_close(database->conn);
    database->conn = NULL;
    mutex_destroy(&database->mutex);
}

bool db_exec(Database *database, const char *sql, char *error, size_t error_size) {
    char *sqlite_error = NULL;
    int result;
    if (database == NULL || database->conn == NULL || sql == NULL) { set_error(error, error_size, "Database is not available"); return false; }
    result = sqlite3_exec(database->conn, sql, NULL, NULL, &sqlite_error);
    if (result != SQLITE_OK) {
        set_error(error, error_size, sqlite_error != NULL ? sqlite_error : sqlite3_errmsg(database->conn));
        sqlite3_free(sqlite_error);
        return false;
    }
    return true;
}

int db_scalar_int(Database *database, const char *sql, int fallback) {
    sqlite3_stmt *statement = NULL;
    int value = fallback;
    if (database == NULL || database->conn == NULL || sql == NULL) return fallback;
    if (sqlite3_prepare_v2(database->conn, sql, -1, &statement, NULL) == SQLITE_OK) {
        if (sqlite3_step(statement) == SQLITE_ROW) value = sqlite3_column_int(statement, 0);
    }
    sqlite3_finalize(statement);
    return value;
}

bool db_initialize_schema(Database *database, char *error, size_t error_size) {
    static const char *schema =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL CHECK(length(name) BETWEEN 2 AND 120),"
        "email TEXT NOT NULL COLLATE NOCASE UNIQUE CHECK(length(email) <= 150),"
        "phone TEXT NOT NULL CHECK(length(phone) <= 20),"
        "password_hash TEXT NOT NULL,"
        "password_salt TEXT NOT NULL,"
        "role TEXT NOT NULL CHECK(role IN ('CUSTOMER','ADMIN','STAFF')),"
        "account_status TEXT NOT NULL DEFAULT 'ACTIVE' CHECK(account_status IN ('ACTIVE','INACTIVE')),"
        "created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),"
        "updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))"
        ");"
        "CREATE TABLE IF NOT EXISTS shipments ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "tracking_number TEXT NOT NULL UNIQUE COLLATE NOCASE,"
        "customer_id INTEGER REFERENCES users(id) ON DELETE SET NULL,"
        "sender_name TEXT NOT NULL, sender_phone TEXT NOT NULL, sender_email TEXT NOT NULL,"
        "sender_address TEXT NOT NULL, sender_city TEXT NOT NULL, sender_state TEXT NOT NULL, sender_pincode TEXT NOT NULL,"
        "receiver_name TEXT NOT NULL, receiver_phone TEXT NOT NULL, receiver_email TEXT NOT NULL,"
        "receiver_address TEXT NOT NULL, receiver_city TEXT NOT NULL, receiver_state TEXT NOT NULL, receiver_pincode TEXT NOT NULL,"
        "package_description TEXT NOT NULL, package_weight REAL NOT NULL CHECK(package_weight > 0),"
        "package_type TEXT NOT NULL, delivery_type TEXT NOT NULL CHECK(delivery_type IN ('Standard','Express')),"
        "declared_value REAL NOT NULL DEFAULT 0 CHECK(declared_value >= 0),"
        "shipping_charge REAL NOT NULL CHECK(shipping_charge >= 0),"
        "current_status TEXT NOT NULL DEFAULT 'BOOKED' CHECK(current_status IN ('BOOKED','PICKED_UP','IN_TRANSIT','OUT_FOR_DELIVERY','DELIVERED','DELIVERY_FAILED','CANCELLED')),"
        "assigned_staff_id INTEGER REFERENCES users(id) ON DELETE SET NULL,"
        "booking_date TEXT NOT NULL, estimated_delivery_date TEXT NOT NULL, delivered_at TEXT,"
        "created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),"
        "updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))"
        ");"
        "CREATE TABLE IF NOT EXISTS tracking_history ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "shipment_id INTEGER NOT NULL REFERENCES shipments(id) ON DELETE CASCADE,"
        "tracking_number TEXT NOT NULL, status TEXT NOT NULL CHECK(status IN ('BOOKED','PICKED_UP','IN_TRANSIT','OUT_FOR_DELIVERY','DELIVERED','DELIVERY_FAILED','CANCELLED')),"
        "location TEXT NOT NULL, remarks TEXT NOT NULL, updated_by TEXT NOT NULL,"
        "created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))"
        ");"
        "CREATE TABLE IF NOT EXISTS sessions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
        "token_hash TEXT NOT NULL UNIQUE, expires_at TEXT NOT NULL,"
        "created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_users_role_status ON users(role, account_status);"
        "CREATE INDEX IF NOT EXISTS idx_shipments_customer_date ON shipments(customer_id, booking_date DESC);"
        "CREATE INDEX IF NOT EXISTS idx_shipments_staff_status ON shipments(assigned_staff_id, current_status);"
        "CREATE INDEX IF NOT EXISTS idx_shipments_status_date ON shipments(current_status, booking_date DESC);"
        "CREATE INDEX IF NOT EXISTS idx_tracking_history_shipment_date ON tracking_history(shipment_id, created_at ASC);"
        "CREATE INDEX IF NOT EXISTS idx_sessions_token_expiry ON sessions(token_hash, expires_at);";
    bool success;
    if (database == NULL || database->conn == NULL) { set_error(error, error_size, "Database is not available"); return false; }
    db_lock(database);
    success = db_exec(database, schema, error, error_size);
    db_unlock(database);
    return success;
}

static bool seed_user(Database *database, const char *name, const char *email, const char *phone,
                      const char *role, const char *password, int *id, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    char salt[65], hash[65];
    int result;
    if (!auth_hash_password(password, salt, sizeof(salt), hash, sizeof(hash))) { set_error(error, error_size, "Could not securely create demo account password"); return false; }
    result = sqlite3_prepare_v2(database->conn,
        "INSERT INTO users(name,email,phone,password_hash,password_salt,role,account_status) VALUES(?,?,?,?,?,?,'ACTIVE')",
        -1, &statement, NULL);
    if (result != SQLITE_OK) { set_error(error, error_size, sqlite3_errmsg(database->conn)); return false; }
    sqlite3_bind_text(statement, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, email, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, phone, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, salt, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 6, role, -1, SQLITE_TRANSIENT);
    result = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE) { set_error(error, error_size, sqlite3_errmsg(database->conn)); return false; }
    *id = (int)sqlite3_last_insert_rowid(database->conn);
    return true;
}

static bool seed_event(Database *database, int shipment_id, const char *tracking, const char *status,
                       const char *location, const char *remarks, const char *timestamp, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    int result = sqlite3_prepare_v2(database->conn,
        "INSERT INTO tracking_history(shipment_id,tracking_number,status,location,remarks,updated_by,created_at) VALUES(?,?,?,?,?,'System demo seed',?)",
        -1, &statement, NULL);
    if (result != SQLITE_OK) { set_error(error, error_size, sqlite3_errmsg(database->conn)); return false; }
    sqlite3_bind_int(statement, 1, shipment_id);
    sqlite3_bind_text(statement, 2, tracking, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, status, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, location, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, remarks, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 6, timestamp, -1, SQLITE_TRANSIENT);
    result = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE) { set_error(error, error_size, sqlite3_errmsg(database->conn)); return false; }
    return true;
}

static bool seed_shipment(Database *database, int customer_id, int staff_id, const char *tracking, const char *origin,
                          const char *destination, const char *status, const char *delivery_type, double weight,
                          int days_ago, int sequence, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    char booking_day[32], booking_date[64], estimated[32], delivered_at[64];
    int result, shipment_id;
    double charge = (strcmp(delivery_type, "Express") == 0 ? 140.0 + weight * 60.0 : 70.0 + weight * 35.0);
    date_days_from_now(-days_ago, booking_day, sizeof(booking_day));
    snprintf(booking_date, sizeof(booking_date), "%sT10:%02d:00Z", booking_day, sequence % 60);
    date_days_from_now((strcmp(delivery_type, "Express") == 0 ? 2 : 5) - days_ago, estimated, sizeof(estimated));
    delivered_at[0] = '\0';
    if (strcmp(status, "DELIVERED") == 0) snprintf(delivered_at, sizeof(delivered_at), "%sT15:30:00Z", booking_day);
    result = sqlite3_prepare_v2(database->conn,
        "INSERT INTO shipments(tracking_number,customer_id,sender_name,sender_phone,sender_email,sender_address,sender_city,sender_state,sender_pincode,"
        "receiver_name,receiver_phone,receiver_email,receiver_address,receiver_city,receiver_state,receiver_pincode,package_description,package_weight,package_type,delivery_type,declared_value,shipping_charge,current_status,assigned_staff_id,booking_date,estimated_delivery_date,delivered_at)"
        " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)", -1, &statement, NULL);
    if (result != SQLITE_OK) { set_error(error, error_size, sqlite3_errmsg(database->conn)); return false; }
    sqlite3_bind_text(statement, 1, tracking, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 2, customer_id);
    sqlite3_bind_text(statement, 3, "Demo Sender", -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, "+919876543210", -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 5, "sender@example.demo", -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 6, "12 Industrial Estate", -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 7, origin, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 8, "Punjab", -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 9, "141001", -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 10, "Demo Receiver", -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 11, "+919123456789", -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 12, "receiver@example.demo", -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 13, "45 Market Road", -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 14, destination, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 15, "India", -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 16, "110001", -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 17, "Demo project shipment", -1, SQLITE_STATIC);
    sqlite3_bind_double(statement, 18, weight);
    sqlite3_bind_text(statement, 19, "Parcel", -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 20, delivery_type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 21, 1000.0);
    sqlite3_bind_double(statement, 22, charge);
    sqlite3_bind_text(statement, 23, status, -1, SQLITE_TRANSIENT);
    if (staff_id > 0) sqlite3_bind_int(statement, 24, staff_id); else sqlite3_bind_null(statement, 24);
    sqlite3_bind_text(statement, 25, booking_date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 26, estimated, -1, SQLITE_TRANSIENT);
    if (delivered_at[0]) sqlite3_bind_text(statement, 27, delivered_at, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(statement, 27);
    result = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE) { set_error(error, error_size, sqlite3_errmsg(database->conn)); return false; }
    shipment_id = (int)sqlite3_last_insert_rowid(database->conn);
    if (!seed_event(database, shipment_id, tracking, "BOOKED", origin, "Shipment booked through demo data", booking_date, error, error_size)) return false;
    if (strcmp(status, "BOOKED") != 0) {
        char event_time[64];
        snprintf(event_time, sizeof(event_time), "%sT12:%02d:00Z", booking_day, (sequence + 5) % 60);
        if (!seed_event(database, shipment_id, tracking, "PICKED_UP", origin, "Package collected from sender", event_time, error, error_size)) return false;
    }
    if (strcmp(status, "IN_TRANSIT") == 0 || strcmp(status, "OUT_FOR_DELIVERY") == 0 || strcmp(status, "DELIVERED") == 0 || strcmp(status, "DELIVERY_FAILED") == 0) {
        char event_time[64];
        snprintf(event_time, sizeof(event_time), "%sT14:%02d:00Z", booking_day, (sequence + 10) % 60);
        if (!seed_event(database, shipment_id, tracking, "IN_TRANSIT", "Regional sorting hub", "Shipment departed sorting facility", event_time, error, error_size)) return false;
    }
    if (strcmp(status, "OUT_FOR_DELIVERY") == 0 || strcmp(status, "DELIVERED") == 0 || strcmp(status, "DELIVERY_FAILED") == 0) {
        char event_time[64];
        snprintf(event_time, sizeof(event_time), "%sT16:%02d:00Z", booking_day, (sequence + 15) % 60);
        if (!seed_event(database, shipment_id, tracking, "OUT_FOR_DELIVERY", destination, "Courier is on the way", event_time, error, error_size)) return false;
    }
    if (strcmp(status, "DELIVERED") == 0 || strcmp(status, "DELIVERY_FAILED") == 0 || strcmp(status, "CANCELLED") == 0) {
        char event_time[64];
        const char *remark = strcmp(status, "DELIVERED") == 0 ? "Shipment delivered successfully" : (strcmp(status, "CANCELLED") == 0 ? "Booking cancelled by customer" : "Recipient unavailable; delivery attempt failed");
        snprintf(event_time, sizeof(event_time), "%sT18:%02d:00Z", booking_day, (sequence + 20) % 60);
        if (!seed_event(database, shipment_id, tracking, status, destination, remark, event_time, error, error_size)) return false;
    }
    return true;
}

bool db_seed_demo(Database *database, const char *demo_password, char *error, size_t error_size) {
    int admin_id, staff_one, staff_two, customers[3];
    int count;
    bool success = false;
    static const char *statuses[] = {"BOOKED", "PICKED_UP", "IN_TRANSIT", "IN_TRANSIT", "OUT_FOR_DELIVERY", "DELIVERED", "DELIVERED", "CANCELLED", "DELIVERY_FAILED", "IN_TRANSIT"};
    static const char *origins[] = {"Ludhiana", "Chandigarh", "Amritsar", "Jalandhar", "Ludhiana", "Delhi", "Jaipur", "Ludhiana", "Chandigarh", "Patiala"};
    static const char *destinations[] = {"Delhi", "Mumbai", "Bengaluru", "Pune", "Ahmedabad", "Ludhiana", "Kolkata", "Jaipur", "Hyderabad", "Chennai"};
    static const int booking_ages[] = {160, 130, 105, 80, 58, 42, 30, 21, 12, 4};
    int i;
    if (database == NULL || database->conn == NULL) { set_error(error, error_size, "Database is not available"); return false; }
    if (!validate_password_strength(demo_password, error, error_size)) return false;
    db_lock(database);
    count = db_scalar_int(database, "SELECT COUNT(*) FROM users", 0);
    if (count > 0) { set_error(error, error_size, "Demo data can only be seeded into an empty database"); goto done; }
    if (!db_exec(database, "BEGIN IMMEDIATE", error, error_size)) goto done;
    if (!seed_user(database, "System Administrator", "admin@courier.demo", "+919800000001", "ADMIN", demo_password, &admin_id, error, error_size) ||
        !seed_user(database, "Aman Dispatch", "aman.staff@courier.demo", "+919800000002", "STAFF", demo_password, &staff_one, error, error_size) ||
        !seed_user(database, "Neha Delivery", "neha.staff@courier.demo", "+919800000003", "STAFF", demo_password, &staff_two, error, error_size) ||
        !seed_user(database, "Riya Sharma", "riya.customer@courier.demo", "+919800000004", "CUSTOMER", demo_password, &customers[0], error, error_size) ||
        !seed_user(database, "Arjun Mehta", "arjun.customer@courier.demo", "+919800000005", "CUSTOMER", demo_password, &customers[1], error, error_size) ||
        !seed_user(database, "Simran Kaur", "simran.customer@courier.demo", "+919800000006", "CUSTOMER", demo_password, &customers[2], error, error_size)) {
        db_exec(database, "ROLLBACK", NULL, 0); goto done;
    }
    (void)admin_id;
    for (i = 0; i < 10; i++) {
        char tracking[32];
        snprintf(tracking, sizeof(tracking), "CDDEMO%05d", i + 1);
        if (!seed_shipment(database, customers[i % 3], (i % 2 == 0 ? staff_one : staff_two), tracking, origins[i], destinations[i], statuses[i], (i % 3 == 0 ? "Express" : "Standard"), 0.5 + i * 0.35, booking_ages[i], i, error, error_size)) {
            db_exec(database, "ROLLBACK", NULL, 0); goto done;
        }
    }
    if (!db_exec(database, "COMMIT", error, error_size)) { db_exec(database, "ROLLBACK", NULL, 0); goto done; }
    success = true;
done:
    db_unlock(database);
    return success;
}
