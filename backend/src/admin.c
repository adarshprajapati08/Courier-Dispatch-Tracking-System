#include "admin.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>

static void set_error(char *error, size_t error_size, const char *message) {
    if (error != NULL && error_size > 0) snprintf(error, error_size, "%s", message == NULL ? "Unable to load administrator data" : message);
}

static int scalar_unlocked(Database *database, const char *sql) {
    sqlite3_stmt *statement = NULL;
    int value = 0;
    if (sqlite3_prepare_v2(database->conn, sql, -1, &statement, NULL) == SQLITE_OK) {
        if (sqlite3_step(statement) == SQLITE_ROW) value = sqlite3_column_int(statement, 0);
    }
    sqlite3_finalize(statement);
    return value;
}

static bool add_recent_shipments(Database *database, cJSON *data, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    cJSON *items = cJSON_CreateArray();
    int result;
    if (items == NULL) return false;
    result = sqlite3_prepare_v2(database->conn,
        "SELECT s.id,s.tracking_number,COALESCE(c.name,'Walk-in / Admin'),s.receiver_name,s.receiver_city,s.current_status,s.delivery_type,s.booking_date "
        "FROM shipments s LEFT JOIN users c ON c.id=s.customer_id ORDER BY s.booking_date DESC,s.id DESC LIMIT 8", -1, &statement, NULL);
    if (result != SQLITE_OK) { cJSON_Delete(items); set_error(error, error_size, "Unable to load recent shipments"); return false; }
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        cJSON *row = cJSON_CreateObject();
        if (row == NULL) { sqlite3_finalize(statement); cJSON_Delete(items); return false; }
        cJSON_AddNumberToObject(row, "id", sqlite3_column_int(statement, 0));
        json_add_string_or_null(row, "tracking_number", (const char *)sqlite3_column_text(statement, 1));
        json_add_string_or_null(row, "customer_name", (const char *)sqlite3_column_text(statement, 2));
        json_add_string_or_null(row, "receiver_name", (const char *)sqlite3_column_text(statement, 3));
        json_add_string_or_null(row, "destination", (const char *)sqlite3_column_text(statement, 4));
        json_add_string_or_null(row, "current_status", (const char *)sqlite3_column_text(statement, 5));
        json_add_string_or_null(row, "delivery_type", (const char *)sqlite3_column_text(statement, 6));
        json_add_string_or_null(row, "booking_date", (const char *)sqlite3_column_text(statement, 7));
        cJSON_AddItemToArray(items, row);
    }
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE) { cJSON_Delete(items); set_error(error, error_size, "Unable to load recent shipments"); return false; }
    cJSON_AddItemToObject(data, "recent_shipments", items);
    return true;
}

static bool add_recent_updates(Database *database, cJSON *data, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    cJSON *items = cJSON_CreateArray();
    int result;
    if (items == NULL) return false;
    result = sqlite3_prepare_v2(database->conn,
        "SELECT h.id,h.tracking_number,h.status,h.location,h.remarks,h.updated_by,h.created_at FROM tracking_history h ORDER BY h.created_at DESC,h.id DESC LIMIT 8", -1, &statement, NULL);
    if (result != SQLITE_OK) { cJSON_Delete(items); set_error(error, error_size, "Unable to load tracking updates"); return false; }
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        cJSON *row = cJSON_CreateObject();
        if (row == NULL) { sqlite3_finalize(statement); cJSON_Delete(items); return false; }
        cJSON_AddNumberToObject(row, "id", sqlite3_column_int(statement, 0));
        json_add_string_or_null(row, "tracking_number", (const char *)sqlite3_column_text(statement, 1));
        json_add_string_or_null(row, "status", (const char *)sqlite3_column_text(statement, 2));
        json_add_string_or_null(row, "location", (const char *)sqlite3_column_text(statement, 3));
        json_add_string_or_null(row, "remarks", (const char *)sqlite3_column_text(statement, 4));
        json_add_string_or_null(row, "updated_by", (const char *)sqlite3_column_text(statement, 5));
        json_add_string_or_null(row, "created_at", (const char *)sqlite3_column_text(statement, 6));
        cJSON_AddItemToArray(items, row);
    }
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE) { cJSON_Delete(items); set_error(error, error_size, "Unable to load tracking updates"); return false; }
    cJSON_AddItemToObject(data, "recent_tracking_updates", items);
    return true;
}

bool admin_dashboard(Database *database, cJSON **result, char *error, size_t error_size) {
    cJSON *data = NULL, *statistics = NULL, *overview = NULL;
    sqlite3_stmt *statement = NULL;
    int sqlite_result;
    int total, delivered;
    if (database == NULL || database->conn == NULL || result == NULL) { set_error(error, error_size, "Database is unavailable"); return false; }
    data = cJSON_CreateObject(); statistics = cJSON_CreateObject(); overview = cJSON_CreateArray();
    if (data == NULL || statistics == NULL || overview == NULL) { cJSON_Delete(data); cJSON_Delete(statistics); cJSON_Delete(overview); set_error(error, error_size, "Unable to allocate dashboard data"); return false; }
    db_lock(database);
    total = scalar_unlocked(database, "SELECT COUNT(*) FROM shipments");
    delivered = scalar_unlocked(database, "SELECT COUNT(*) FROM shipments WHERE current_status='DELIVERED'");
    cJSON_AddNumberToObject(statistics, "total_shipments", total);
    cJSON_AddNumberToObject(statistics, "pending_shipments", scalar_unlocked(database, "SELECT COUNT(*) FROM shipments WHERE current_status IN ('BOOKED','PICKED_UP','OUT_FOR_DELIVERY','DELIVERY_FAILED')"));
    cJSON_AddNumberToObject(statistics, "in_transit_shipments", scalar_unlocked(database, "SELECT COUNT(*) FROM shipments WHERE current_status='IN_TRANSIT'"));
    cJSON_AddNumberToObject(statistics, "delivered_shipments", delivered);
    cJSON_AddNumberToObject(statistics, "cancelled_shipments", scalar_unlocked(database, "SELECT COUNT(*) FROM shipments WHERE current_status='CANCELLED'"));
    cJSON_AddNumberToObject(statistics, "total_customers", scalar_unlocked(database, "SELECT COUNT(*) FROM users WHERE role='CUSTOMER'"));
    cJSON_AddNumberToObject(statistics, "total_staff", scalar_unlocked(database, "SELECT COUNT(*) FROM users WHERE role='STAFF' AND account_status='ACTIVE'"));
    cJSON_AddNumberToObject(statistics, "delivery_success_rate", total == 0 ? 0.0 : (100.0 * delivered / total));
    sqlite_result = sqlite3_prepare_v2(database->conn, "SELECT current_status,COUNT(*) FROM shipments GROUP BY current_status ORDER BY current_status", -1, &statement, NULL);
    if (sqlite_result != SQLITE_OK) { db_unlock(database); cJSON_Delete(data); cJSON_Delete(statistics); cJSON_Delete(overview); set_error(error, error_size, "Unable to load dashboard status overview"); return false; }
    while ((sqlite_result = sqlite3_step(statement)) == SQLITE_ROW) {
        cJSON *item = cJSON_CreateObject();
        if (item == NULL) { sqlite3_finalize(statement); db_unlock(database); cJSON_Delete(data); cJSON_Delete(statistics); cJSON_Delete(overview); return false; }
        json_add_string_or_null(item, "status", (const char *)sqlite3_column_text(statement, 0));
        cJSON_AddNumberToObject(item, "count", sqlite3_column_int(statement, 1));
        cJSON_AddItemToArray(overview, item);
    }
    sqlite3_finalize(statement);
    if (sqlite_result != SQLITE_DONE || !add_recent_shipments(database, data, error, error_size) || !add_recent_updates(database, data, error, error_size)) {
        db_unlock(database); cJSON_Delete(data); cJSON_Delete(statistics); cJSON_Delete(overview); if (error != NULL && error[0] == '\0') set_error(error, error_size, "Unable to load dashboard data"); return false;
    }
    db_unlock(database);
    cJSON_AddItemToObject(data, "statistics", statistics);
    cJSON_AddItemToObject(data, "status_overview", overview);
    *result = data;
    return true;
}

bool admin_reports(Database *database, cJSON **result, char *error, size_t error_size) {
    cJSON *data = NULL, *summary = NULL, *delivery_types = NULL, *monthly = NULL;
    sqlite3_stmt *statement = NULL;
    int result_code;
    if (database == NULL || database->conn == NULL || result == NULL) { set_error(error, error_size, "Database is unavailable"); return false; }
    data = cJSON_CreateObject(); summary = cJSON_CreateObject(); delivery_types = cJSON_CreateArray(); monthly = cJSON_CreateArray();
    if (data == NULL || summary == NULL || delivery_types == NULL || monthly == NULL) { cJSON_Delete(data); cJSON_Delete(summary); cJSON_Delete(delivery_types); cJSON_Delete(monthly); set_error(error, error_size, "Unable to allocate report data"); return false; }
    db_lock(database);
    cJSON_AddNumberToObject(summary, "total_bookings", scalar_unlocked(database, "SELECT COUNT(*) FROM shipments"));
    cJSON_AddNumberToObject(summary, "successful_deliveries", scalar_unlocked(database, "SELECT COUNT(*) FROM shipments WHERE current_status='DELIVERED'"));
    cJSON_AddNumberToObject(summary, "pending_deliveries", scalar_unlocked(database, "SELECT COUNT(*) FROM shipments WHERE current_status IN ('BOOKED','PICKED_UP','IN_TRANSIT','OUT_FOR_DELIVERY')"));
    cJSON_AddNumberToObject(summary, "cancelled_deliveries", scalar_unlocked(database, "SELECT COUNT(*) FROM shipments WHERE current_status='CANCELLED'"));
    cJSON_AddNumberToObject(summary, "failed_deliveries", scalar_unlocked(database, "SELECT COUNT(*) FROM shipments WHERE current_status='DELIVERY_FAILED'"));
    result_code = sqlite3_prepare_v2(database->conn, "SELECT delivery_type,COUNT(*) FROM shipments GROUP BY delivery_type ORDER BY delivery_type", -1, &statement, NULL);
    if (result_code != SQLITE_OK) goto failure;
    while ((result_code = sqlite3_step(statement)) == SQLITE_ROW) {
        cJSON *item = cJSON_CreateObject();
        if (item == NULL) { sqlite3_finalize(statement); goto failure; }
        json_add_string_or_null(item, "delivery_type", (const char *)sqlite3_column_text(statement, 0));
        cJSON_AddNumberToObject(item, "count", sqlite3_column_int(statement, 1));
        cJSON_AddItemToArray(delivery_types, item);
    }
    sqlite3_finalize(statement); statement = NULL;
    if (result_code != SQLITE_DONE) goto failure;
    result_code = sqlite3_prepare_v2(database->conn,
        "SELECT substr(booking_date,1,7) AS month,COUNT(*) AS total,"
        "SUM(CASE WHEN current_status='DELIVERED' THEN 1 ELSE 0 END) AS delivered,"
        "SUM(CASE WHEN delivery_type='Express' THEN 1 ELSE 0 END) AS express "
        "FROM shipments GROUP BY substr(booking_date,1,7) ORDER BY month ASC", -1, &statement, NULL);
    if (result_code != SQLITE_OK) goto failure;
    while ((result_code = sqlite3_step(statement)) == SQLITE_ROW) {
        cJSON *item = cJSON_CreateObject();
        if (item == NULL) { sqlite3_finalize(statement); goto failure; }
        json_add_string_or_null(item, "month", (const char *)sqlite3_column_text(statement, 0));
        cJSON_AddNumberToObject(item, "total", sqlite3_column_int(statement, 1));
        cJSON_AddNumberToObject(item, "delivered", sqlite3_column_int(statement, 2));
        cJSON_AddNumberToObject(item, "express", sqlite3_column_int(statement, 3));
        cJSON_AddItemToArray(monthly, item);
    }
    sqlite3_finalize(statement); statement = NULL;
    if (result_code != SQLITE_DONE) goto failure;
    db_unlock(database);
    cJSON_AddItemToObject(data, "summary", summary);
    cJSON_AddItemToObject(data, "delivery_types", delivery_types);
    cJSON_AddItemToObject(data, "monthly_statistics", monthly);
    *result = data;
    return true;
failure:
    sqlite3_finalize(statement);
    db_unlock(database);
    cJSON_Delete(data); cJSON_Delete(summary); cJSON_Delete(delivery_types); cJSON_Delete(monthly);
    set_error(error, error_size, "Unable to load reports");
    return false;
}
