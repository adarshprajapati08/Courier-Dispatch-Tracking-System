#include "shipment.h"
#include "tracking.h"
#include "utils.h"
#include "validation.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STANDARD_BASE_RATE 70.0
#define STANDARD_PER_KG_RATE 35.0
#define EXPRESS_BASE_RATE 140.0
#define EXPRESS_PER_KG_RATE 60.0
#define DECLARED_VALUE_COVER_RATE 0.01

static const char *shipment_columns =
    "s.id,s.tracking_number,s.customer_id,COALESCE(c.name,''),"
    "s.sender_name,s.sender_phone,s.sender_email,s.sender_address,s.sender_city,s.sender_state,s.sender_pincode,"
    "s.receiver_name,s.receiver_phone,s.receiver_email,s.receiver_address,s.receiver_city,s.receiver_state,s.receiver_pincode,"
    "s.package_description,s.package_weight,s.package_type,s.delivery_type,s.declared_value,s.shipping_charge,s.current_status,"
    "s.assigned_staff_id,COALESCE(a.name,''),s.booking_date,s.estimated_delivery_date,s.delivered_at,s.created_at,s.updated_at";

static const char *shipment_from =
    " FROM shipments s LEFT JOIN users c ON c.id=s.customer_id LEFT JOIN users a ON a.id=s.assigned_staff_id ";

typedef struct {
    char search[161];
    char status[32];
    char date_from[16];
    char date_to[16];
    int page;
    int limit;
} ShipmentFilters;

static void set_error(char *error, size_t error_size, const char *message) {
    if (error != NULL && error_size > 0) snprintf(error, error_size, "%s", message == NULL ? "Shipment operation failed" : message);
}

static bool valid_date(const char *text) {
    size_t i;
    if (text == NULL || strlen(text) != 10 || text[4] != '-' || text[7] != '-') return false;
    for (i = 0; i < 10; i++) if (i != 4 && i != 7 && !isdigit((unsigned char)text[i])) return false;
    return true;
}

static int bounded_query_int(const char *query, const char *key, int default_value, int min, int max) {
    char value[32];
    char *end = NULL;
    long parsed;
    query_value(query, key, value, sizeof(value));
    if (value[0] == '\0') return default_value;
    parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed < min || parsed > max) return default_value;
    return (int)parsed;
}

static bool parse_filters(const char *query, ShipmentFilters *filters, char *error, size_t error_size) {
    char raw_search[161];
    if (filters == NULL) return false;
    memset(filters, 0, sizeof(*filters));
    filters->page = bounded_query_int(query, "page", 1, 1, 100000);
    filters->limit = bounded_query_int(query, "limit", 10, 1, 100);
    query_value(query, "search", raw_search, sizeof(raw_search));
    trim_whitespace(raw_search);
    if (strlen(raw_search) > 150) { set_error(error, error_size, "Search query is too long"); return false; }
    if (raw_search[0]) escape_like_pattern(raw_search, filters->search, sizeof(filters->search));
    query_value(query, "status", filters->status, sizeof(filters->status));
    if (filters->status[0] && !validate_status(filters->status)) { set_error(error, error_size, "Invalid shipment status filter"); return false; }
    query_value(query, "date_from", filters->date_from, sizeof(filters->date_from));
    query_value(query, "date_to", filters->date_to, sizeof(filters->date_to));
    if (filters->date_from[0] && !valid_date(filters->date_from)) { set_error(error, error_size, "date_from must use YYYY-MM-DD format"); return false; }
    if (filters->date_to[0] && !valid_date(filters->date_to)) { set_error(error, error_size, "date_to must use YYYY-MM-DD format"); return false; }
    if (filters->date_from[0] && filters->date_to[0] && strcmp(filters->date_from, filters->date_to) > 0) { set_error(error, error_size, "date_from cannot be after date_to"); return false; }
    return true;
}

static void add_shipment_json(cJSON *object, sqlite3_stmt *statement) {
    cJSON_AddNumberToObject(object, "id", sqlite3_column_int(statement, 0));
    json_add_string_or_null(object, "tracking_number", (const char *)sqlite3_column_text(statement, 1));
    if (sqlite3_column_type(statement, 2) == SQLITE_NULL) cJSON_AddNullToObject(object, "customer_id");
    else cJSON_AddNumberToObject(object, "customer_id", sqlite3_column_int(statement, 2));
    json_add_string_or_null(object, "customer_name", (const char *)sqlite3_column_text(statement, 3));
    json_add_string_or_null(object, "sender_name", (const char *)sqlite3_column_text(statement, 4));
    json_add_string_or_null(object, "sender_phone", (const char *)sqlite3_column_text(statement, 5));
    json_add_string_or_null(object, "sender_email", (const char *)sqlite3_column_text(statement, 6));
    json_add_string_or_null(object, "sender_address", (const char *)sqlite3_column_text(statement, 7));
    json_add_string_or_null(object, "sender_city", (const char *)sqlite3_column_text(statement, 8));
    json_add_string_or_null(object, "sender_state", (const char *)sqlite3_column_text(statement, 9));
    json_add_string_or_null(object, "sender_pincode", (const char *)sqlite3_column_text(statement, 10));
    json_add_string_or_null(object, "receiver_name", (const char *)sqlite3_column_text(statement, 11));
    json_add_string_or_null(object, "receiver_phone", (const char *)sqlite3_column_text(statement, 12));
    json_add_string_or_null(object, "receiver_email", (const char *)sqlite3_column_text(statement, 13));
    json_add_string_or_null(object, "receiver_address", (const char *)sqlite3_column_text(statement, 14));
    json_add_string_or_null(object, "receiver_city", (const char *)sqlite3_column_text(statement, 15));
    json_add_string_or_null(object, "receiver_state", (const char *)sqlite3_column_text(statement, 16));
    json_add_string_or_null(object, "receiver_pincode", (const char *)sqlite3_column_text(statement, 17));
    json_add_string_or_null(object, "package_description", (const char *)sqlite3_column_text(statement, 18));
    cJSON_AddNumberToObject(object, "package_weight", sqlite3_column_double(statement, 19));
    json_add_string_or_null(object, "package_type", (const char *)sqlite3_column_text(statement, 20));
    json_add_string_or_null(object, "delivery_type", (const char *)sqlite3_column_text(statement, 21));
    cJSON_AddNumberToObject(object, "declared_value", sqlite3_column_double(statement, 22));
    cJSON_AddNumberToObject(object, "shipping_charge", sqlite3_column_double(statement, 23));
    json_add_string_or_null(object, "current_status", (const char *)sqlite3_column_text(statement, 24));
    if (sqlite3_column_type(statement, 25) == SQLITE_NULL) cJSON_AddNullToObject(object, "assigned_staff_id");
    else cJSON_AddNumberToObject(object, "assigned_staff_id", sqlite3_column_int(statement, 25));
    json_add_string_or_null(object, "assigned_staff_name", (const char *)sqlite3_column_text(statement, 26));
    json_add_string_or_null(object, "booking_date", (const char *)sqlite3_column_text(statement, 27));
    json_add_string_or_null(object, "estimated_delivery_date", (const char *)sqlite3_column_text(statement, 28));
    json_add_string_or_null(object, "delivered_at", (const char *)sqlite3_column_text(statement, 29));
    json_add_string_or_null(object, "created_at", (const char *)sqlite3_column_text(statement, 30));
    json_add_string_or_null(object, "updated_at", (const char *)sqlite3_column_text(statement, 31));
}

static double calculate_charge(double weight, const char *delivery_type, double declared_value) {
    double base = strcmp(delivery_type, "Express") == 0 ? EXPRESS_BASE_RATE : STANDARD_BASE_RATE;
    double per_kg = strcmp(delivery_type, "Express") == 0 ? EXPRESS_PER_KG_RATE : STANDARD_PER_KG_RATE;
    return base + weight * per_kg + declared_value * DECLARED_VALUE_COVER_RATE;
}

static bool customer_is_active(Database *database, int customer_id) {
    sqlite3_stmt *statement = NULL;
    int result;
    if (customer_id <= 0) return false;
    result = sqlite3_prepare_v2(database->conn, "SELECT 1 FROM users WHERE id=? AND role='CUSTOMER' AND account_status='ACTIVE'", -1, &statement, NULL);
    if (result == SQLITE_OK) {
        sqlite3_bind_int(statement, 1, customer_id);
        result = sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
    return result == SQLITE_ROW;
}

static bool generate_tracking(char *tracking, size_t size) {
    char raw[9];
    char date[16];
    size_t i;
    date_days_from_now(0, date, sizeof(date));
    if (!random_hex(raw, sizeof(raw), 4)) return false;
    for (i = 0; raw[i]; i++) raw[i] = (char)toupper((unsigned char)raw[i]);
    snprintf(tracking, size, "CPD%.4s%.2s%.2s%s", date, date + 5, date + 8, raw);
    return true;
}

bool shipment_create(Database *database, const cJSON *payload, int customer_id, const char *actor_name, cJSON **result, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    char tracking[32], booking_date[64], estimated_date[32];
    double weight, declared_value, charge;
    int sqlite_result = SQLITE_ERROR;
    int shipment_id = 0;
    int attempts;
    const char *delivery_type;
    bool transaction_open = false;
    if (!shipment_payload_valid(payload, error, error_size)) return false;
    if (customer_id < 0) { set_error(error, error_size, "Invalid customer account"); return false; }
    weight = json_number(payload, "package_weight", 0.0);
    declared_value = json_number(payload, "declared_value", 0.0);
    delivery_type = json_string(payload, "delivery_type");
    charge = calculate_charge(weight, delivery_type, declared_value);
    timestamp_now(booking_date, sizeof(booking_date));
    date_days_from_now(strcmp(delivery_type, "Express") == 0 ? 2 : 5, estimated_date, sizeof(estimated_date));
    db_lock(database);
    if (customer_id > 0 && !customer_is_active(database, customer_id)) { db_unlock(database); set_error(error, error_size, "Selected customer account is not active"); return false; }
    if (!db_exec(database, "BEGIN IMMEDIATE", error, error_size)) { db_unlock(database); return false; }
    transaction_open = true;
    for (attempts = 0; attempts < 5; attempts++) {
        if (!generate_tracking(tracking, sizeof(tracking))) { set_error(error, error_size, "Unable to generate tracking number"); break; }
        sqlite_result = sqlite3_prepare_v2(database->conn,
            "INSERT INTO shipments(tracking_number,customer_id,sender_name,sender_phone,sender_email,sender_address,sender_city,sender_state,sender_pincode,"
            "receiver_name,receiver_phone,receiver_email,receiver_address,receiver_city,receiver_state,receiver_pincode,package_description,package_weight,package_type,delivery_type,declared_value,shipping_charge,current_status,booking_date,estimated_delivery_date)"
            " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,'BOOKED',?,?)", -1, &statement, NULL);
        if (sqlite_result != SQLITE_OK) break;
        sqlite3_bind_text(statement, 1, tracking, -1, SQLITE_TRANSIENT);
        if (customer_id > 0) sqlite3_bind_int(statement, 2, customer_id); else sqlite3_bind_null(statement, 2);
        sqlite3_bind_text(statement, 3, json_string(payload, "sender_name"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 4, json_string(payload, "sender_phone"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 5, json_string(payload, "sender_email"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 6, json_string(payload, "sender_address"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 7, json_string(payload, "sender_city"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 8, json_string(payload, "sender_state"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 9, json_string(payload, "sender_pincode"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 10, json_string(payload, "receiver_name"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 11, json_string(payload, "receiver_phone"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 12, json_string(payload, "receiver_email"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 13, json_string(payload, "receiver_address"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 14, json_string(payload, "receiver_city"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 15, json_string(payload, "receiver_state"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 16, json_string(payload, "receiver_pincode"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 17, json_string(payload, "package_description"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(statement, 18, weight);
        sqlite3_bind_text(statement, 19, json_string(payload, "package_type"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 20, delivery_type, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(statement, 21, declared_value);
        sqlite3_bind_double(statement, 22, charge);
        sqlite3_bind_text(statement, 23, booking_date, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 24, estimated_date, -1, SQLITE_TRANSIENT);
        sqlite_result = sqlite3_step(statement);
        sqlite3_finalize(statement);
        statement = NULL;
        if (sqlite_result == SQLITE_DONE) { shipment_id = (int)sqlite3_last_insert_rowid(database->conn); break; }
        if (sqlite_result != SQLITE_CONSTRAINT) break;
    }
    if (sqlite_result != SQLITE_DONE || shipment_id <= 0 ||
        !tracking_add_event(database, shipment_id, tracking, "BOOKED", json_string(payload, "sender_city"), "Shipment booked successfully", actor_name == NULL ? "System" : actor_name, error, error_size) ||
        !db_exec(database, "COMMIT", error, error_size)) {
        if (transaction_open) db_exec(database, "ROLLBACK", NULL, 0);
        db_unlock(database);
        if (error != NULL && error[0] == '\0') set_error(error, error_size, "Unable to create shipment");
        return false;
    }
    db_unlock(database);
    *result = cJSON_CreateObject();
    if (*result == NULL) { set_error(error, error_size, "Shipment was created but confirmation could not be generated"); return false; }
    cJSON_AddNumberToObject(*result, "id", shipment_id);
    cJSON_AddStringToObject(*result, "tracking_number", tracking);
    cJSON_AddStringToObject(*result, "current_status", "BOOKED");
    cJSON_AddNumberToObject(*result, "shipping_charge", charge);
    cJSON_AddStringToObject(*result, "booking_date", booking_date);
    cJSON_AddStringToObject(*result, "estimated_delivery_date", estimated_date);
    return true;
}

static bool shipment_get_internal(Database *database, int shipment_id, cJSON **result, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    cJSON *shipment = NULL;
    char sql[2048];
    int sqlite_result;
    snprintf(sql, sizeof(sql), "SELECT %s %s WHERE s.id=?", shipment_columns, shipment_from);
    sqlite_result = sqlite3_prepare_v2(database->conn, sql, -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) {
        sqlite3_bind_int(statement, 1, shipment_id);
        sqlite_result = sqlite3_step(statement);
    }
    if (sqlite_result != SQLITE_ROW) { sqlite3_finalize(statement); set_error(error, error_size, "Shipment not found"); return false; }
    shipment = cJSON_CreateObject();
    if (shipment == NULL) { sqlite3_finalize(statement); set_error(error, error_size, "Unable to load shipment"); return false; }
    add_shipment_json(shipment, statement);
    sqlite3_finalize(statement);
    *result = shipment;
    return true;
}

bool shipment_get(Database *database, int shipment_id, cJSON **result, char *error, size_t error_size) {
    cJSON *shipment = NULL;
    cJSON *history = NULL;
    if (database == NULL || database->conn == NULL || shipment_id <= 0 || result == NULL) { set_error(error, error_size, "Invalid shipment request"); return false; }
    db_lock(database);
    if (!shipment_get_internal(database, shipment_id, &shipment, error, error_size)) { db_unlock(database); return false; }
    db_unlock(database);
    if (!tracking_history_for_shipment(database, shipment_id, &history, error, error_size)) { cJSON_Delete(shipment); return false; }
    cJSON_AddItemToObject(shipment, "tracking_history", history);
    *result = shipment;
    return true;
}

bool shipment_get_for_customer(Database *database, int shipment_id, int customer_id, cJSON **result, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    int sqlite_result;
    bool owned = false;
    if (customer_id <= 0) { set_error(error, error_size, "Invalid customer account"); return false; }
    db_lock(database);
    sqlite_result = sqlite3_prepare_v2(database->conn, "SELECT 1 FROM shipments WHERE id=? AND customer_id=?", -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) {
        sqlite3_bind_int(statement, 1, shipment_id);
        sqlite3_bind_int(statement, 2, customer_id);
        owned = sqlite3_step(statement) == SQLITE_ROW;
    }
    sqlite3_finalize(statement);
    db_unlock(database);
    if (!owned) { set_error(error, error_size, "Shipment not found"); return false; }
    return shipment_get(database, shipment_id, result, error, error_size);
}

bool shipment_get_for_staff(Database *database, int shipment_id, int staff_id, cJSON **result, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    int sqlite_result;
    bool assigned = false;
    if (staff_id <= 0) { set_error(error, error_size, "Invalid staff account"); return false; }
    db_lock(database);
    sqlite_result = sqlite3_prepare_v2(database->conn, "SELECT 1 FROM shipments WHERE id=? AND assigned_staff_id=?", -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) {
        sqlite3_bind_int(statement, 1, shipment_id);
        sqlite3_bind_int(statement, 2, staff_id);
        assigned = sqlite3_step(statement) == SQLITE_ROW;
    }
    sqlite3_finalize(statement);
    db_unlock(database);
    if (!assigned) { set_error(error, error_size, "Shipment not found"); return false; }
    return shipment_get(database, shipment_id, result, error, error_size);
}

static bool list_shipments(Database *database, int scope, int scope_id, const char *query, cJSON **result, char *error, size_t error_size) {
    ShipmentFilters filters;
    char where[1600];
    char count_sql[2200];
    char select_sql[5000];
    sqlite3_stmt *statement = NULL;
    int sqlite_result;
    int total = 0;
    cJSON *data = NULL;
    cJSON *items = NULL;
    cJSON *pagination = NULL;
    int bind_index;
    if (!parse_filters(query, &filters, error, error_size)) return false;
    snprintf(where, sizeof(where), " WHERE 1=1");
    if (scope == 1) strncat(where, " AND s.customer_id=?", sizeof(where) - strlen(where) - 1);
    else if (scope == 2) strncat(where, " AND s.assigned_staff_id=?", sizeof(where) - strlen(where) - 1);
    if (filters.search[0]) strncat(where, " AND (s.tracking_number LIKE ? ESCAPE '\\' OR s.sender_name LIKE ? ESCAPE '\\' OR s.receiver_name LIKE ? ESCAPE '\\' OR c.name LIKE ? ESCAPE '\\')", sizeof(where) - strlen(where) - 1);
    if (filters.status[0]) strncat(where, " AND s.current_status=?", sizeof(where) - strlen(where) - 1);
    if (filters.date_from[0]) strncat(where, " AND substr(s.booking_date,1,10)>=?", sizeof(where) - strlen(where) - 1);
    if (filters.date_to[0]) strncat(where, " AND substr(s.booking_date,1,10)<=?", sizeof(where) - strlen(where) - 1);
    snprintf(count_sql, sizeof(count_sql), "SELECT COUNT(*) FROM shipments s LEFT JOIN users c ON c.id=s.customer_id %s", where);
    db_lock(database);
    sqlite_result = sqlite3_prepare_v2(database->conn, count_sql, -1, &statement, NULL);
    if (sqlite_result != SQLITE_OK) { db_unlock(database); set_error(error, error_size, "Unable to load shipment count"); return false; }
    /* The search placeholder occurs four times. Bind each one explicitly. */
    bind_index = 1;
    if (scope != 0) sqlite3_bind_int(statement, bind_index++, scope_id);
    if (filters.search[0]) {
        char pattern[330];
        int i;
        snprintf(pattern, sizeof(pattern), "%%%s%%", filters.search);
        for (i = 0; i < 4; i++) sqlite3_bind_text(statement, bind_index++, pattern, -1, SQLITE_TRANSIENT);
    }
    if (filters.status[0]) sqlite3_bind_text(statement, bind_index++, filters.status, -1, SQLITE_TRANSIENT);
    if (filters.date_from[0]) sqlite3_bind_text(statement, bind_index++, filters.date_from, -1, SQLITE_TRANSIENT);
    if (filters.date_to[0]) sqlite3_bind_text(statement, bind_index++, filters.date_to, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement) == SQLITE_ROW) total = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    statement = NULL;
    snprintf(select_sql, sizeof(select_sql), "SELECT %s %s %s ORDER BY s.booking_date DESC, s.id DESC LIMIT ? OFFSET ?", shipment_columns, shipment_from, where);
    sqlite_result = sqlite3_prepare_v2(database->conn, select_sql, -1, &statement, NULL);
    if (sqlite_result != SQLITE_OK) { db_unlock(database); set_error(error, error_size, "Unable to load shipments"); return false; }
    bind_index = 1;
    if (scope != 0) sqlite3_bind_int(statement, bind_index++, scope_id);
    if (filters.search[0]) {
        char pattern[330];
        int i;
        snprintf(pattern, sizeof(pattern), "%%%s%%", filters.search);
        for (i = 0; i < 4; i++) sqlite3_bind_text(statement, bind_index++, pattern, -1, SQLITE_TRANSIENT);
    }
    if (filters.status[0]) sqlite3_bind_text(statement, bind_index++, filters.status, -1, SQLITE_TRANSIENT);
    if (filters.date_from[0]) sqlite3_bind_text(statement, bind_index++, filters.date_from, -1, SQLITE_TRANSIENT);
    if (filters.date_to[0]) sqlite3_bind_text(statement, bind_index++, filters.date_to, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, bind_index++, filters.limit);
    sqlite3_bind_int(statement, bind_index, (filters.page - 1) * filters.limit);
    data = cJSON_CreateObject();
    items = cJSON_CreateArray();
    pagination = cJSON_CreateObject();
    if (data == NULL || items == NULL || pagination == NULL) {
        sqlite3_finalize(statement); db_unlock(database); cJSON_Delete(data); cJSON_Delete(items); cJSON_Delete(pagination); set_error(error, error_size, "Unable to allocate shipment list"); return false;
    }
    while ((sqlite_result = sqlite3_step(statement)) == SQLITE_ROW) {
        cJSON *shipment = cJSON_CreateObject();
        if (shipment == NULL) { sqlite3_finalize(statement); db_unlock(database); cJSON_Delete(data); cJSON_Delete(items); cJSON_Delete(pagination); set_error(error, error_size, "Unable to allocate shipment data"); return false; }
        add_shipment_json(shipment, statement);
        cJSON_AddItemToArray(items, shipment);
    }
    sqlite3_finalize(statement);
    db_unlock(database);
    if (sqlite_result != SQLITE_DONE) { cJSON_Delete(data); cJSON_Delete(items); cJSON_Delete(pagination); set_error(error, error_size, "Unable to load shipments"); return false; }
    cJSON_AddNumberToObject(pagination, "page", filters.page);
    cJSON_AddNumberToObject(pagination, "limit", filters.limit);
    cJSON_AddNumberToObject(pagination, "total", total);
    cJSON_AddNumberToObject(pagination, "total_pages", total == 0 ? 0 : (total + filters.limit - 1) / filters.limit);
    cJSON_AddItemToObject(data, "items", items);
    cJSON_AddItemToObject(data, "pagination", pagination);
    *result = data;
    return true;
}

bool shipment_list_customer(Database *database, int customer_id, const char *query, cJSON **result, char *error, size_t error_size) {
    if (customer_id <= 0) { set_error(error, error_size, "Invalid customer account"); return false; }
    return list_shipments(database, 1, customer_id, query, result, error, error_size);
}

bool shipment_list_admin(Database *database, const char *query, cJSON **result, char *error, size_t error_size) {
    return list_shipments(database, 0, 0, query, result, error, error_size);
}

bool shipment_list_staff(Database *database, int staff_id, const char *query, cJSON **result, char *error, size_t error_size) {
    if (staff_id <= 0) { set_error(error, error_size, "Invalid staff account"); return false; }
    return list_shipments(database, 2, staff_id, query, result, error, error_size);
}

bool shipment_update(Database *database, int shipment_id, const cJSON *payload, cJSON **result, char *error, size_t error_size) {
    sqlite3_stmt *check = NULL;
    sqlite3_stmt *statement = NULL;
    int sqlite_result;
    const unsigned char *status;
    if (shipment_id <= 0 || !shipment_payload_valid(payload, error, error_size)) return false;
    db_lock(database);
    sqlite_result = sqlite3_prepare_v2(database->conn, "SELECT current_status FROM shipments WHERE id=?", -1, &check, NULL);
    if (sqlite_result == SQLITE_OK) { sqlite3_bind_int(check, 1, shipment_id); sqlite_result = sqlite3_step(check); }
    if (sqlite_result != SQLITE_ROW) { sqlite3_finalize(check); db_unlock(database); set_error(error, error_size, "Shipment not found"); return false; }
    status = sqlite3_column_text(check, 0);
    if (status != NULL && (strcmp((const char *)status, "DELIVERED") == 0 || strcmp((const char *)status, "CANCELLED") == 0)) { sqlite3_finalize(check); db_unlock(database); set_error(error, error_size, "Delivered or cancelled shipments cannot be edited"); return false; }
    sqlite3_finalize(check);
    sqlite_result = sqlite3_prepare_v2(database->conn,
        "UPDATE shipments SET sender_name=?,sender_phone=?,sender_email=?,sender_address=?,sender_city=?,sender_state=?,sender_pincode=?,"
        "receiver_name=?,receiver_phone=?,receiver_email=?,receiver_address=?,receiver_city=?,receiver_state=?,receiver_pincode=?,"
        "package_description=?,package_weight=?,package_type=?,delivery_type=?,declared_value=?,shipping_charge=?,estimated_delivery_date=?,updated_at=strftime('%Y-%m-%dT%H:%M:%SZ','now') WHERE id=?", -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) {
        double weight = json_number(payload, "package_weight", 0.0);
        double value = json_number(payload, "declared_value", 0.0);
        const char *delivery = json_string(payload, "delivery_type");
        char estimated[32];
        date_days_from_now(strcmp(delivery, "Express") == 0 ? 2 : 5, estimated, sizeof(estimated));
        sqlite3_bind_text(statement, 1, json_string(payload, "sender_name"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, json_string(payload, "sender_phone"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 3, json_string(payload, "sender_email"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 4, json_string(payload, "sender_address"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 5, json_string(payload, "sender_city"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 6, json_string(payload, "sender_state"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 7, json_string(payload, "sender_pincode"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 8, json_string(payload, "receiver_name"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 9, json_string(payload, "receiver_phone"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 10, json_string(payload, "receiver_email"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 11, json_string(payload, "receiver_address"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 12, json_string(payload, "receiver_city"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 13, json_string(payload, "receiver_state"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 14, json_string(payload, "receiver_pincode"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 15, json_string(payload, "package_description"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(statement, 16, weight);
        sqlite3_bind_text(statement, 17, json_string(payload, "package_type"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 18, delivery, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(statement, 19, value);
        sqlite3_bind_double(statement, 20, calculate_charge(weight, delivery, value));
        sqlite3_bind_text(statement, 21, estimated, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statement, 22, shipment_id);
        sqlite_result = sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
    db_unlock(database);
    if (sqlite_result != SQLITE_DONE) { set_error(error, error_size, "Unable to update shipment"); return false; }
    return shipment_get(database, shipment_id, result, error, error_size);
}

bool shipment_assign_staff(Database *database, int shipment_id, const cJSON *payload, cJSON **result, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    int staff_id = json_integer(payload, "staff_id", 0);
    int sqlite_result;
    int affected = 0;
    if (shipment_id <= 0 || staff_id <= 0) { set_error(error, error_size, "A valid staff member is required"); return false; }
    db_lock(database);
    sqlite_result = sqlite3_prepare_v2(database->conn, "SELECT 1 FROM users WHERE id=? AND role='STAFF' AND account_status='ACTIVE'", -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) { sqlite3_bind_int(statement, 1, staff_id); sqlite_result = sqlite3_step(statement); }
    sqlite3_finalize(statement);
    if (sqlite_result != SQLITE_ROW) { db_unlock(database); set_error(error, error_size, "Selected staff member is not active"); return false; }
    sqlite_result = sqlite3_prepare_v2(database->conn, "UPDATE shipments SET assigned_staff_id=?,updated_at=strftime('%Y-%m-%dT%H:%M:%SZ','now') WHERE id=?", -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) {
        sqlite3_bind_int(statement, 1, staff_id);
        sqlite3_bind_int(statement, 2, shipment_id);
        sqlite_result = sqlite3_step(statement);
        if (sqlite_result == SQLITE_DONE) affected = sqlite3_changes(database->conn);
    }
    sqlite3_finalize(statement);
    db_unlock(database);
    if (sqlite_result != SQLITE_DONE || affected == 0) { set_error(error, error_size, "Shipment not found or assignment failed"); return false; }
    return shipment_get(database, shipment_id, result, error, error_size);
}

bool shipment_update_status(Database *database, int shipment_id, const cJSON *payload, const AuthUser *actor, bool staff_restricted, cJSON **result, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    const char *new_status = json_string(payload, "status");
    const char *location = json_string(payload, "location");
    const char *remarks = json_string(payload, "remarks");
    char tracking[64], old_status[32], destination[128];
    int assigned_staff = 0;
    int sqlite_result;
    bool transaction_open = false;
    if (shipment_id <= 0 || actor == NULL || !validate_status(new_status)) { set_error(error, error_size, "A valid shipment status is required"); return false; }
    if (location != NULL && (string_is_blank(location) || strlen(location) > 120)) { set_error(error, error_size, "Location must be 1 to 120 characters"); return false; }
    if (remarks != NULL && strlen(remarks) > 500) { set_error(error, error_size, "Remarks cannot exceed 500 characters"); return false; }
    db_lock(database);
    if (!db_exec(database, "BEGIN IMMEDIATE", error, error_size)) { db_unlock(database); return false; }
    transaction_open = true;
    sqlite_result = sqlite3_prepare_v2(database->conn, "SELECT tracking_number,current_status,receiver_city,assigned_staff_id FROM shipments WHERE id=?", -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) { sqlite3_bind_int(statement, 1, shipment_id); sqlite_result = sqlite3_step(statement); }
    if (sqlite_result != SQLITE_ROW) { sqlite3_finalize(statement); db_exec(database, "ROLLBACK", NULL, 0); db_unlock(database); set_error(error, error_size, "Shipment not found"); return false; }
    copy_string(tracking, sizeof(tracking), (const char *)sqlite3_column_text(statement, 0));
    copy_string(old_status, sizeof(old_status), (const char *)sqlite3_column_text(statement, 1));
    copy_string(destination, sizeof(destination), (const char *)sqlite3_column_text(statement, 2));
    if (sqlite3_column_type(statement, 3) != SQLITE_NULL) assigned_staff = sqlite3_column_int(statement, 3);
    sqlite3_finalize(statement);
    if (staff_restricted && assigned_staff != actor->id) { db_exec(database, "ROLLBACK", NULL, 0); db_unlock(database); set_error(error, error_size, "You can update only shipments assigned to you"); return false; }
    if (!valid_status_transition(old_status, new_status)) { db_exec(database, "ROLLBACK", NULL, 0); db_unlock(database); set_error(error, error_size, "This shipment status transition is not allowed"); return false; }
    sqlite_result = sqlite3_prepare_v2(database->conn,
        "UPDATE shipments SET current_status=?,delivered_at=CASE WHEN ?='DELIVERED' THEN strftime('%Y-%m-%dT%H:%M:%SZ','now') ELSE delivered_at END,updated_at=strftime('%Y-%m-%dT%H:%M:%SZ','now') WHERE id=?", -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) { sqlite3_bind_text(statement, 1, new_status, -1, SQLITE_TRANSIENT); sqlite3_bind_text(statement, 2, new_status, -1, SQLITE_TRANSIENT); sqlite3_bind_int(statement, 3, shipment_id); sqlite_result = sqlite3_step(statement); }
    sqlite3_finalize(statement);
    if (sqlite_result != SQLITE_DONE || !tracking_add_event(database, shipment_id, tracking, new_status, location == NULL ? destination : location,
        remarks == NULL ? "Shipment status updated" : remarks, actor->name, error, error_size) || !db_exec(database, "COMMIT", error, error_size)) {
        if (transaction_open) db_exec(database, "ROLLBACK", NULL, 0);
        db_unlock(database);
        if (error != NULL && error[0] == '\0') set_error(error, error_size, "Unable to update shipment status");
        return false;
    }
    db_unlock(database);
    return shipment_get(database, shipment_id, result, error, error_size);
}

bool shipment_cancel(Database *database, int shipment_id, const AuthUser *actor, cJSON **result, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    int sqlite_result;
    char tracking[64], current[32], location[128];
    int customer_id = 0;
    bool transaction_open = false;
    if (shipment_id <= 0 || actor == NULL || (actor->role != ROLE_CUSTOMER && actor->role != ROLE_ADMIN)) { set_error(error, error_size, "You are not allowed to cancel this shipment"); return false; }
    db_lock(database);
    if (!db_exec(database, "BEGIN IMMEDIATE", error, error_size)) { db_unlock(database); return false; }
    transaction_open = true;
    sqlite_result = sqlite3_prepare_v2(database->conn, "SELECT tracking_number,current_status,sender_city,customer_id FROM shipments WHERE id=?", -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) { sqlite3_bind_int(statement, 1, shipment_id); sqlite_result = sqlite3_step(statement); }
    if (sqlite_result != SQLITE_ROW) { sqlite3_finalize(statement); db_exec(database, "ROLLBACK", NULL, 0); db_unlock(database); set_error(error, error_size, "Shipment not found"); return false; }
    copy_string(tracking, sizeof(tracking), (const char *)sqlite3_column_text(statement, 0));
    copy_string(current, sizeof(current), (const char *)sqlite3_column_text(statement, 1));
    copy_string(location, sizeof(location), (const char *)sqlite3_column_text(statement, 2));
    if (sqlite3_column_type(statement, 3) != SQLITE_NULL) customer_id = sqlite3_column_int(statement, 3);
    sqlite3_finalize(statement);
    if (actor->role == ROLE_CUSTOMER && customer_id != actor->id) { db_exec(database, "ROLLBACK", NULL, 0); db_unlock(database); set_error(error, error_size, "Shipment not found"); return false; }
    if (!valid_status_transition(current, "CANCELLED")) { db_exec(database, "ROLLBACK", NULL, 0); db_unlock(database); set_error(error, error_size, "This shipment can no longer be cancelled"); return false; }
    sqlite_result = sqlite3_prepare_v2(database->conn, "UPDATE shipments SET current_status='CANCELLED',updated_at=strftime('%Y-%m-%dT%H:%M:%SZ','now') WHERE id=?", -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) { sqlite3_bind_int(statement, 1, shipment_id); sqlite_result = sqlite3_step(statement); }
    sqlite3_finalize(statement);
    if (sqlite_result != SQLITE_DONE || !tracking_add_event(database, shipment_id, tracking, "CANCELLED", location, "Shipment cancelled", actor->name, error, error_size) || !db_exec(database, "COMMIT", error, error_size)) {
        if (transaction_open) db_exec(database, "ROLLBACK", NULL, 0);
        db_unlock(database);
        if (error != NULL && error[0] == '\0') set_error(error, error_size, "Unable to cancel shipment");
        return false;
    }
    db_unlock(database);
    return shipment_get(database, shipment_id, result, error, error_size);
}

bool shipment_delete(Database *database, int shipment_id, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    int sqlite_result;
    if (shipment_id <= 0) { set_error(error, error_size, "Invalid shipment id"); return false; }
    db_lock(database);
    sqlite_result = sqlite3_prepare_v2(database->conn, "DELETE FROM shipments WHERE id=? AND current_status='CANCELLED'", -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) { sqlite3_bind_int(statement, 1, shipment_id); sqlite_result = sqlite3_step(statement); }
    sqlite3_finalize(statement);
    if (sqlite_result != SQLITE_DONE || sqlite3_changes(database->conn) == 0) { db_unlock(database); set_error(error, error_size, "Only cancelled shipments can be deleted"); return false; }
    db_unlock(database);
    return true;
}
