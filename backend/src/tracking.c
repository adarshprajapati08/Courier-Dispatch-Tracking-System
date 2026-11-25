#include "tracking.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>

static void set_error(char *error, size_t error_size, const char *message) {
    if (error != NULL && error_size > 0) snprintf(error, error_size, "%s", message == NULL ? "Tracking operation failed" : message);
}

static bool history_unlocked(Database *database, int shipment_id, cJSON **history, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    cJSON *array = cJSON_CreateArray();
    int result;
    if (array == NULL) { set_error(error, error_size, "Unable to allocate tracking history"); return false; }
    result = sqlite3_prepare_v2(database->conn,
        "SELECT id,tracking_number,status,location,remarks,updated_by,created_at FROM tracking_history WHERE shipment_id=? ORDER BY created_at ASC,id ASC",
        -1, &statement, NULL);
    if (result != SQLITE_OK) { cJSON_Delete(array); set_error(error, error_size, "Unable to load tracking history"); return false; }
    sqlite3_bind_int(statement, 1, shipment_id);
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        cJSON *event = cJSON_CreateObject();
        if (event == NULL) { sqlite3_finalize(statement); cJSON_Delete(array); set_error(error, error_size, "Unable to allocate tracking event"); return false; }
        cJSON_AddNumberToObject(event, "id", sqlite3_column_int(statement, 0));
        json_add_string_or_null(event, "tracking_number", (const char *)sqlite3_column_text(statement, 1));
        json_add_string_or_null(event, "status", (const char *)sqlite3_column_text(statement, 2));
        json_add_string_or_null(event, "location", (const char *)sqlite3_column_text(statement, 3));
        json_add_string_or_null(event, "remarks", (const char *)sqlite3_column_text(statement, 4));
        json_add_string_or_null(event, "updated_by", (const char *)sqlite3_column_text(statement, 5));
        json_add_string_or_null(event, "created_at", (const char *)sqlite3_column_text(statement, 6));
        cJSON_AddItemToArray(array, event);
    }
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE) { cJSON_Delete(array); set_error(error, error_size, "Unable to load tracking history"); return false; }
    *history = array;
    return true;
}

bool tracking_add_event(Database *database, int shipment_id, const char *tracking_number,
                        const char *status, const char *location, const char *remarks,
                        const char *updated_by, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    int result;
    if (database == NULL || database->conn == NULL || shipment_id <= 0 || tracking_number == NULL || status == NULL || location == NULL || remarks == NULL || updated_by == NULL) {
        set_error(error, error_size, "Invalid tracking event"); return false;
    }
    result = sqlite3_prepare_v2(database->conn,
        "INSERT INTO tracking_history(shipment_id,tracking_number,status,location,remarks,updated_by) VALUES(?,?,?,?,?,?)",
        -1, &statement, NULL);
    if (result != SQLITE_OK) { set_error(error, error_size, "Unable to save tracking event"); return false; }
    sqlite3_bind_int(statement, 1, shipment_id);
    sqlite3_bind_text(statement, 2, tracking_number, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, status, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, location, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, remarks, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 6, updated_by, -1, SQLITE_TRANSIENT);
    result = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE) { set_error(error, error_size, "Unable to save tracking event"); return false; }
    return true;
}

bool tracking_history_for_shipment(Database *database, int shipment_id, cJSON **history, char *error, size_t error_size) {
    bool success;
    if (database == NULL || database->conn == NULL || shipment_id <= 0 || history == NULL) { set_error(error, error_size, "Invalid tracking history request"); return false; }
    db_lock(database);
    success = history_unlocked(database, shipment_id, history, error, error_size);
    db_unlock(database);
    return success;
}

bool tracking_get_public(Database *database, const char *tracking_number, cJSON **result, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    cJSON *shipment = NULL;
    cJSON *history = NULL;
    int sqlite_result;
    int shipment_id;
    if (database == NULL || database->conn == NULL || result == NULL || tracking_number == NULL || strlen(tracking_number) < 6 || strlen(tracking_number) > 40) {
        set_error(error, error_size, "Invalid tracking number"); return false;
    }
    db_lock(database);
    sqlite_result = sqlite3_prepare_v2(database->conn,
        "SELECT id,tracking_number,sender_name,receiver_name,sender_city,sender_state,receiver_city,receiver_state,booking_date,estimated_delivery_date,current_status,package_weight,delivery_type,package_type "
        "FROM shipments WHERE tracking_number=? COLLATE NOCASE LIMIT 1", -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) {
        sqlite3_bind_text(statement, 1, tracking_number, -1, SQLITE_TRANSIENT);
        sqlite_result = sqlite3_step(statement);
    }
    if (sqlite_result != SQLITE_ROW) {
        sqlite3_finalize(statement); db_unlock(database);
        set_error(error, error_size, "No shipment was found for that tracking number"); return false;
    }
    shipment_id = sqlite3_column_int(statement, 0);
    shipment = cJSON_CreateObject();
    if (shipment == NULL) { sqlite3_finalize(statement); db_unlock(database); set_error(error, error_size, "Unable to load shipment"); return false; }
    cJSON_AddNumberToObject(shipment, "id", shipment_id);
    json_add_string_or_null(shipment, "tracking_number", (const char *)sqlite3_column_text(statement, 1));
    json_add_string_or_null(shipment, "sender_name", (const char *)sqlite3_column_text(statement, 2));
    json_add_string_or_null(shipment, "receiver_name", (const char *)sqlite3_column_text(statement, 3));
    json_add_string_or_null(shipment, "sender_city", (const char *)sqlite3_column_text(statement, 4));
    json_add_string_or_null(shipment, "sender_state", (const char *)sqlite3_column_text(statement, 5));
    json_add_string_or_null(shipment, "receiver_city", (const char *)sqlite3_column_text(statement, 6));
    json_add_string_or_null(shipment, "receiver_state", (const char *)sqlite3_column_text(statement, 7));
    json_add_string_or_null(shipment, "booking_date", (const char *)sqlite3_column_text(statement, 8));
    json_add_string_or_null(shipment, "estimated_delivery_date", (const char *)sqlite3_column_text(statement, 9));
    json_add_string_or_null(shipment, "current_status", (const char *)sqlite3_column_text(statement, 10));
    cJSON_AddNumberToObject(shipment, "package_weight", sqlite3_column_double(statement, 11));
    json_add_string_or_null(shipment, "delivery_type", (const char *)sqlite3_column_text(statement, 12));
    json_add_string_or_null(shipment, "package_type", (const char *)sqlite3_column_text(statement, 13));
    sqlite3_finalize(statement);
    if (!history_unlocked(database, shipment_id, &history, error, error_size)) { cJSON_Delete(shipment); db_unlock(database); return false; }
    cJSON_AddItemToObject(shipment, "tracking_history", history);
    db_unlock(database);
    *result = shipment;
    return true;
}
