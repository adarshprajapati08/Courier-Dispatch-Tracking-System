#include "staff.h"
#include "utils.h"

#include <stdio.h>

static void set_error(char *error, size_t error_size, const char *message) {
    if (error != NULL && error_size > 0) snprintf(error, error_size, "%s", message == NULL ? "Unable to load staff dashboard" : message);
}

static int staff_scalar(Database *database, int staff_id, const char *condition) {
    char sql[512];
    sqlite3_stmt *statement = NULL;
    int value = 0;
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM shipments WHERE assigned_staff_id=? %s", condition == NULL ? "" : condition);
    if (sqlite3_prepare_v2(database->conn, sql, -1, &statement, NULL) == SQLITE_OK) {
        sqlite3_bind_int(statement, 1, staff_id);
        if (sqlite3_step(statement) == SQLITE_ROW) value = sqlite3_column_int(statement, 0);
    }
    sqlite3_finalize(statement);
    return value;
}

bool staff_dashboard(Database *database, int staff_id, cJSON **result, char *error, size_t error_size) {
    cJSON *data = NULL, *statistics = NULL, *shipments = NULL;
    sqlite3_stmt *statement = NULL;
    int sqlite_result;
    if (database == NULL || database->conn == NULL || result == NULL || staff_id <= 0) { set_error(error, error_size, "Invalid staff dashboard request"); return false; }
    data = cJSON_CreateObject(); statistics = cJSON_CreateObject(); shipments = cJSON_CreateArray();
    if (data == NULL || statistics == NULL || shipments == NULL) { cJSON_Delete(data); cJSON_Delete(statistics); cJSON_Delete(shipments); set_error(error, error_size, "Unable to allocate dashboard data"); return false; }
    db_lock(database);
    cJSON_AddNumberToObject(statistics, "assigned_shipments", staff_scalar(database, staff_id, ""));
    cJSON_AddNumberToObject(statistics, "action_required", staff_scalar(database, staff_id, "AND current_status IN ('PICKED_UP','IN_TRANSIT','OUT_FOR_DELIVERY','DELIVERY_FAILED')"));
    cJSON_AddNumberToObject(statistics, "in_transit_shipments", staff_scalar(database, staff_id, "AND current_status='IN_TRANSIT'"));
    cJSON_AddNumberToObject(statistics, "delivered_shipments", staff_scalar(database, staff_id, "AND current_status='DELIVERED'"));
    sqlite_result = sqlite3_prepare_v2(database->conn,
        "SELECT id,tracking_number,receiver_name,receiver_city,current_status,delivery_type,estimated_delivery_date,booking_date "
        "FROM shipments WHERE assigned_staff_id=? ORDER BY CASE current_status WHEN 'OUT_FOR_DELIVERY' THEN 0 WHEN 'DELIVERY_FAILED' THEN 1 WHEN 'IN_TRANSIT' THEN 2 WHEN 'PICKED_UP' THEN 3 ELSE 4 END, booking_date DESC LIMIT 10", -1, &statement, NULL);
    if (sqlite_result != SQLITE_OK) { db_unlock(database); cJSON_Delete(data); cJSON_Delete(statistics); cJSON_Delete(shipments); set_error(error, error_size, "Unable to load assigned shipments"); return false; }
    sqlite3_bind_int(statement, 1, staff_id);
    while ((sqlite_result = sqlite3_step(statement)) == SQLITE_ROW) {
        cJSON *row = cJSON_CreateObject();
        if (row == NULL) { sqlite3_finalize(statement); db_unlock(database); cJSON_Delete(data); cJSON_Delete(statistics); cJSON_Delete(shipments); set_error(error, error_size, "Unable to allocate shipment data"); return false; }
        cJSON_AddNumberToObject(row, "id", sqlite3_column_int(statement, 0));
        json_add_string_or_null(row, "tracking_number", (const char *)sqlite3_column_text(statement, 1));
        json_add_string_or_null(row, "receiver_name", (const char *)sqlite3_column_text(statement, 2));
        json_add_string_or_null(row, "destination", (const char *)sqlite3_column_text(statement, 3));
        json_add_string_or_null(row, "current_status", (const char *)sqlite3_column_text(statement, 4));
        json_add_string_or_null(row, "delivery_type", (const char *)sqlite3_column_text(statement, 5));
        json_add_string_or_null(row, "estimated_delivery_date", (const char *)sqlite3_column_text(statement, 6));
        json_add_string_or_null(row, "booking_date", (const char *)sqlite3_column_text(statement, 7));
        cJSON_AddItemToArray(shipments, row);
    }
    sqlite3_finalize(statement);
    db_unlock(database);
    if (sqlite_result != SQLITE_DONE) { cJSON_Delete(data); cJSON_Delete(statistics); cJSON_Delete(shipments); set_error(error, error_size, "Unable to load assigned shipments"); return false; }
    cJSON_AddItemToObject(data, "statistics", statistics);
    cJSON_AddItemToObject(data, "assigned_shipments", shipments);
    *result = data;
    return true;
}
