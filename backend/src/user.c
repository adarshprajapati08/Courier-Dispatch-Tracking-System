#include "user.h"
#include "shipment.h"
#include "utils.h"
#include "validation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t error_size, const char *message) {
    if (error != NULL && error_size > 0) snprintf(error, error_size, "%s", message == NULL ? "User operation failed" : message);
}

static int bounded_query_int(const char *query, const char *key, int default_value, int min, int max) {
    char value[32], *end = NULL;
    long parsed;
    query_value(query, key, value, sizeof(value));
    if (!value[0]) return default_value;
    parsed = strtol(value, &end, 10);
    return (end == value || *end != '\0' || parsed < min || parsed > max) ? default_value : (int)parsed;
}

static void user_json(cJSON *object, sqlite3_stmt *statement, bool include_stats) {
    cJSON_AddNumberToObject(object, "id", sqlite3_column_int(statement, 0));
    json_add_string_or_null(object, "name", (const char *)sqlite3_column_text(statement, 1));
    json_add_string_or_null(object, "email", (const char *)sqlite3_column_text(statement, 2));
    json_add_string_or_null(object, "phone", (const char *)sqlite3_column_text(statement, 3));
    json_add_string_or_null(object, "role", (const char *)sqlite3_column_text(statement, 4));
    json_add_string_or_null(object, "account_status", (const char *)sqlite3_column_text(statement, 5));
    json_add_string_or_null(object, "created_at", (const char *)sqlite3_column_text(statement, 6));
    if (include_stats) {
        cJSON_AddNumberToObject(object, "shipment_count", sqlite3_column_int(statement, 7));
        cJSON_AddNumberToObject(object, "delivered_count", sqlite3_column_int(statement, 8));
    }
}

bool user_get_profile(Database *database, int user_id, cJSON **result, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    int sqlite_result;
    cJSON *profile;
    if (database == NULL || database->conn == NULL || user_id <= 0 || result == NULL) { set_error(error, error_size, "Invalid profile request"); return false; }
    db_lock(database);
    sqlite_result = sqlite3_prepare_v2(database->conn,
        "SELECT u.id,u.name,u.email,u.phone,u.role,u.account_status,u.created_at,"
        "(SELECT COUNT(*) FROM shipments s WHERE s.customer_id=u.id),"
        "(SELECT COUNT(*) FROM shipments s WHERE s.customer_id=u.id AND s.current_status='DELIVERED') FROM users u WHERE u.id=?", -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) { sqlite3_bind_int(statement, 1, user_id); sqlite_result = sqlite3_step(statement); }
    if (sqlite_result != SQLITE_ROW) { sqlite3_finalize(statement); db_unlock(database); set_error(error, error_size, "User not found"); return false; }
    profile = cJSON_CreateObject();
    if (profile == NULL) { sqlite3_finalize(statement); db_unlock(database); set_error(error, error_size, "Unable to load profile"); return false; }
    user_json(profile, statement, true);
    sqlite3_finalize(statement);
    db_unlock(database);
    *result = profile;
    return true;
}

bool user_update_profile(Database *database, int user_id, const cJSON *payload, cJSON **result, char *error, size_t error_size) {
    sqlite3_stmt *lookup = NULL;
    sqlite3_stmt *statement = NULL;
    const char *name, *phone, *email, *new_password, *current_password;
    char existing_email[160], existing_hash[65], existing_salt[65], salt[65], hash[65];
    int sqlite_result;
    bool password_change = false;
    if (payload == NULL || !cJSON_IsObject(payload) || user_id <= 0) { set_error(error, error_size, "Invalid profile update request"); return false; }
    name = json_string(payload, "name");
    phone = json_string(payload, "phone");
    email = json_string(payload, "email");
    new_password = json_string(payload, "new_password");
    current_password = json_string(payload, "current_password");
    if (name == NULL || strlen(name) < 2 || strlen(name) > 120 || !validate_phone_number(phone)) { set_error(error, error_size, "Enter a valid name and phone number"); return false; }
    if (email != NULL && !validate_email_address(email)) { set_error(error, error_size, "Enter a valid email address"); return false; }
    if (new_password != NULL) {
        if (!validate_password_strength(new_password, error, error_size) || current_password == NULL) { if (current_password == NULL) set_error(error, error_size, "Current password is required to change password"); return false; }
        password_change = true;
    }
    db_lock(database);
    sqlite_result = sqlite3_prepare_v2(database->conn, "SELECT email,password_hash,password_salt FROM users WHERE id=?", -1, &lookup, NULL);
    if (sqlite_result == SQLITE_OK) { sqlite3_bind_int(lookup, 1, user_id); sqlite_result = sqlite3_step(lookup); }
    if (sqlite_result != SQLITE_ROW) { sqlite3_finalize(lookup); db_unlock(database); set_error(error, error_size, "User not found"); return false; }
    copy_string(existing_email, sizeof(existing_email), (const char *)sqlite3_column_text(lookup, 0));
    copy_string(existing_hash, sizeof(existing_hash), (const char *)sqlite3_column_text(lookup, 1));
    copy_string(existing_salt, sizeof(existing_salt), (const char *)sqlite3_column_text(lookup, 2));
    sqlite3_finalize(lookup);
    if (password_change && !auth_verify_password(current_password, existing_salt, existing_hash)) { db_unlock(database); set_error(error, error_size, "Current password is incorrect"); return false; }
    if (password_change && !auth_hash_password(new_password, salt, sizeof(salt), hash, sizeof(hash))) { db_unlock(database); set_error(error, error_size, "Unable to securely process password"); return false; }
    sqlite_result = sqlite3_prepare_v2(database->conn,
        "UPDATE users SET name=?,email=?,phone=?,password_hash=CASE WHEN ? THEN ? ELSE password_hash END,password_salt=CASE WHEN ? THEN ? ELSE password_salt END,updated_at=strftime('%Y-%m-%dT%H:%M:%SZ','now') WHERE id=?", -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) {
        sqlite3_bind_text(statement, 1, name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, email == NULL ? existing_email : email, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 3, phone, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statement, 4, password_change ? 1 : 0);
        sqlite3_bind_text(statement, 5, password_change ? hash : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statement, 6, password_change ? 1 : 0);
        sqlite3_bind_text(statement, 7, password_change ? salt : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statement, 8, user_id);
        sqlite_result = sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
    db_unlock(database);
    if (sqlite_result != SQLITE_DONE) { if (sqlite_result == SQLITE_CONSTRAINT) set_error(error, error_size, "An account with that email already exists"); else set_error(error, error_size, "Unable to update profile"); return false; }
    return user_get_profile(database, user_id, result, error, error_size);
}

static bool list_role_users(Database *database, const char *role, const char *query, cJSON **result, char *error, size_t error_size) {
    char search[161], escaped[321], pattern[330];
    int page = bounded_query_int(query, "page", 1, 1, 100000);
    int limit = bounded_query_int(query, "limit", 10, 1, 100);
    char where[800];
    char count_sql[1200], list_sql[2200];
    sqlite3_stmt *statement = NULL;
    int sqlite_result, total = 0, index;
    cJSON *data = NULL, *items = NULL, *pagination = NULL;
    query_value(query, "search", search, sizeof(search));
    trim_whitespace(search);
    if (strlen(search) > 150) { set_error(error, error_size, "Search query is too long"); return false; }
    escape_like_pattern(search, escaped, sizeof(escaped));
    snprintf(pattern, sizeof(pattern), "%%%s%%", escaped);
    snprintf(where, sizeof(where), " WHERE u.role=?");
    if (search[0]) strncat(where, " AND (u.name LIKE ? ESCAPE '\\' OR u.email LIKE ? ESCAPE '\\' OR u.phone LIKE ? ESCAPE '\\')", sizeof(where) - strlen(where) - 1);
    snprintf(count_sql, sizeof(count_sql), "SELECT COUNT(*) FROM users u %s", where);
    db_lock(database);
    sqlite_result = sqlite3_prepare_v2(database->conn, count_sql, -1, &statement, NULL);
    if (sqlite_result != SQLITE_OK) { db_unlock(database); set_error(error, error_size, "Unable to load user count"); return false; }
    index = 1;
    sqlite3_bind_text(statement, index++, role, -1, SQLITE_STATIC);
    if (search[0]) { sqlite3_bind_text(statement, index++, pattern, -1, SQLITE_TRANSIENT); sqlite3_bind_text(statement, index++, pattern, -1, SQLITE_TRANSIENT); sqlite3_bind_text(statement, index++, pattern, -1, SQLITE_TRANSIENT); }
    if (sqlite3_step(statement) == SQLITE_ROW) total = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    snprintf(list_sql, sizeof(list_sql),
        "SELECT u.id,u.name,u.email,u.phone,u.role,u.account_status,u.created_at,COUNT(s.id),SUM(CASE WHEN s.current_status='DELIVERED' THEN 1 ELSE 0 END) "
        "FROM users u LEFT JOIN shipments s ON %s %s GROUP BY u.id ORDER BY u.created_at DESC LIMIT ? OFFSET ?",
        strcmp(role, "STAFF") == 0 ? "s.assigned_staff_id=u.id" : "s.customer_id=u.id", where);
    sqlite_result = sqlite3_prepare_v2(database->conn, list_sql, -1, &statement, NULL);
    if (sqlite_result != SQLITE_OK) { db_unlock(database); set_error(error, error_size, "Unable to load users"); return false; }
    index = 1;
    sqlite3_bind_text(statement, index++, role, -1, SQLITE_STATIC);
    if (search[0]) { sqlite3_bind_text(statement, index++, pattern, -1, SQLITE_TRANSIENT); sqlite3_bind_text(statement, index++, pattern, -1, SQLITE_TRANSIENT); sqlite3_bind_text(statement, index++, pattern, -1, SQLITE_TRANSIENT); }
    sqlite3_bind_int(statement, index++, limit);
    sqlite3_bind_int(statement, index, (page - 1) * limit);
    data = cJSON_CreateObject(); items = cJSON_CreateArray(); pagination = cJSON_CreateObject();
    if (data == NULL || items == NULL || pagination == NULL) { sqlite3_finalize(statement); db_unlock(database); cJSON_Delete(data); cJSON_Delete(items); cJSON_Delete(pagination); set_error(error, error_size, "Unable to allocate user list"); return false; }
    while ((sqlite_result = sqlite3_step(statement)) == SQLITE_ROW) {
        cJSON *user = cJSON_CreateObject();
        if (user == NULL) { sqlite3_finalize(statement); db_unlock(database); cJSON_Delete(data); cJSON_Delete(items); cJSON_Delete(pagination); set_error(error, error_size, "Unable to allocate user record"); return false; }
        user_json(user, statement, true);
        cJSON_AddItemToArray(items, user);
    }
    sqlite3_finalize(statement);
    db_unlock(database);
    if (sqlite_result != SQLITE_DONE) { cJSON_Delete(data); cJSON_Delete(items); cJSON_Delete(pagination); set_error(error, error_size, "Unable to load users"); return false; }
    cJSON_AddNumberToObject(pagination, "page", page);
    cJSON_AddNumberToObject(pagination, "limit", limit);
    cJSON_AddNumberToObject(pagination, "total", total);
    cJSON_AddNumberToObject(pagination, "total_pages", total ? (total + limit - 1) / limit : 0);
    cJSON_AddItemToObject(data, "items", items);
    cJSON_AddItemToObject(data, "pagination", pagination);
    *result = data;
    return true;
}

bool user_list_customers(Database *database, const char *query, cJSON **result, char *error, size_t error_size) {
    return list_role_users(database, "CUSTOMER", query, result, error, error_size);
}

bool user_get_customer_details(Database *database, int customer_id, cJSON **result, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    int sqlite_result;
    bool is_customer = false;
    cJSON *profile = NULL;
    cJSON *shipments = NULL;
    cJSON *data = NULL;
    if (database == NULL || customer_id <= 0 || result == NULL) { set_error(error, error_size, "Invalid customer request"); return false; }
    db_lock(database);
    sqlite_result = sqlite3_prepare_v2(database->conn, "SELECT 1 FROM users WHERE id=? AND role='CUSTOMER'", -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) { sqlite3_bind_int(statement, 1, customer_id); is_customer = sqlite3_step(statement) == SQLITE_ROW; }
    sqlite3_finalize(statement);
    db_unlock(database);
    if (!is_customer) { set_error(error, error_size, "Customer not found"); return false; }
    if (!user_get_profile(database, customer_id, &profile, error, error_size)) return false;
    if (!shipment_list_customer(database, customer_id, "page=1&limit=100", &shipments, error, error_size)) { cJSON_Delete(profile); return false; }
    data = cJSON_CreateObject();
    if (data == NULL) { cJSON_Delete(profile); cJSON_Delete(shipments); set_error(error, error_size, "Unable to allocate customer data"); return false; }
    cJSON_AddItemToObject(data, "profile", profile);
    cJSON_AddItemToObject(data, "shipments", shipments);
    *result = data;
    return true;
}

bool user_list_staff(Database *database, const char *query, cJSON **result, char *error, size_t error_size) {
    return list_role_users(database, "STAFF", query, result, error, error_size);
}

bool user_create_staff(Database *database, const cJSON *payload, cJSON **result, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    char salt[65], hash[65];
    int sqlite_result;
    int user_id;
    if (!user_payload_valid(payload, true, error, error_size)) return false;
    if (!auth_hash_password(json_string(payload, "password"), salt, sizeof(salt), hash, sizeof(hash))) { set_error(error, error_size, "Unable to securely process password"); return false; }
    db_lock(database);
    sqlite_result = sqlite3_prepare_v2(database->conn, "INSERT INTO users(name,email,phone,password_hash,password_salt,role,account_status) VALUES(?,?,?,?,?,'STAFF','ACTIVE')", -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) {
        sqlite3_bind_text(statement, 1, json_string(payload, "name"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, json_string(payload, "email"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 3, json_string(payload, "phone"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 4, hash, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 5, salt, -1, SQLITE_TRANSIENT);
        sqlite_result = sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
    user_id = (int)sqlite3_last_insert_rowid(database->conn);
    db_unlock(database);
    if (sqlite_result != SQLITE_DONE) { if (sqlite_result == SQLITE_CONSTRAINT) set_error(error, error_size, "An account with this email already exists"); else set_error(error, error_size, "Unable to create staff account"); return false; }
    return user_get_profile(database, user_id, result, error, error_size);
}

static bool update_role_user(Database *database, int user_id, const char *expected_role, const cJSON *payload, cJSON **result, char *error, size_t error_size) {
    sqlite3_stmt *lookup = NULL, *statement = NULL;
    char name[128], email[160], phone[32], current_role[16], current_status[16], hash[65], salt[65];
    const char *new_name, *new_email, *new_phone, *account_status, *new_password;
    int sqlite_result;
    bool password_change = false;
    if (user_id <= 0 || payload == NULL || !cJSON_IsObject(payload)) { set_error(error, error_size, "Invalid user update request"); return false; }
    new_name = json_string(payload, "name"); new_email = json_string(payload, "email"); new_phone = json_string(payload, "phone");
    account_status = json_string(payload, "account_status"); new_password = json_string(payload, "password");
    if (new_name != NULL && (strlen(new_name) < 2 || strlen(new_name) > 120)) { set_error(error, error_size, "Name must be 2 to 120 characters"); return false; }
    if (new_email != NULL && !validate_email_address(new_email)) { set_error(error, error_size, "A valid email address is required"); return false; }
    if (new_phone != NULL && !validate_phone_number(new_phone)) { set_error(error, error_size, "A valid phone number is required"); return false; }
    if (account_status != NULL && strcmp(account_status, "ACTIVE") != 0 && strcmp(account_status, "INACTIVE") != 0) { set_error(error, error_size, "Account status must be ACTIVE or INACTIVE"); return false; }
    if (new_password != NULL) { if (!validate_password_strength(new_password, error, error_size) || !auth_hash_password(new_password, salt, sizeof(salt), hash, sizeof(hash))) return false; password_change = true; }
    db_lock(database);
    sqlite_result = sqlite3_prepare_v2(database->conn, "SELECT name,email,phone,role,account_status FROM users WHERE id=?", -1, &lookup, NULL);
    if (sqlite_result == SQLITE_OK) { sqlite3_bind_int(lookup, 1, user_id); sqlite_result = sqlite3_step(lookup); }
    if (sqlite_result != SQLITE_ROW) { sqlite3_finalize(lookup); db_unlock(database); set_error(error, error_size, "User not found"); return false; }
    copy_string(name, sizeof(name), (const char *)sqlite3_column_text(lookup, 0));
    copy_string(email, sizeof(email), (const char *)sqlite3_column_text(lookup, 1));
    copy_string(phone, sizeof(phone), (const char *)sqlite3_column_text(lookup, 2));
    copy_string(current_role, sizeof(current_role), (const char *)sqlite3_column_text(lookup, 3));
    copy_string(current_status, sizeof(current_status), (const char *)sqlite3_column_text(lookup, 4));
    sqlite3_finalize(lookup);
    if (strcmp(current_role, expected_role) != 0) { db_unlock(database); set_error(error, error_size, "Requested account has the wrong role"); return false; }
    sqlite_result = sqlite3_prepare_v2(database->conn,
        "UPDATE users SET name=?,email=?,phone=?,account_status=?,password_hash=CASE WHEN ? THEN ? ELSE password_hash END,password_salt=CASE WHEN ? THEN ? ELSE password_salt END,updated_at=strftime('%Y-%m-%dT%H:%M:%SZ','now') WHERE id=?", -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) {
        sqlite3_bind_text(statement, 1, new_name == NULL ? name : new_name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, new_email == NULL ? email : new_email, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 3, new_phone == NULL ? phone : new_phone, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 4, account_status == NULL ? current_status : account_status, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statement, 5, password_change ? 1 : 0);
        sqlite3_bind_text(statement, 6, password_change ? hash : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statement, 7, password_change ? 1 : 0);
        sqlite3_bind_text(statement, 8, password_change ? salt : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statement, 9, user_id);
        sqlite_result = sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
    db_unlock(database);
    if (sqlite_result != SQLITE_DONE) { if (sqlite_result == SQLITE_CONSTRAINT) set_error(error, error_size, "An account with that email already exists"); else set_error(error, error_size, "Unable to update account"); return false; }
    return user_get_profile(database, user_id, result, error, error_size);
}

bool user_update_staff(Database *database, int staff_id, const cJSON *payload, cJSON **result, char *error, size_t error_size) {
    return update_role_user(database, staff_id, "STAFF", payload, result, error, error_size);
}

bool user_update_customer(Database *database, int customer_id, const cJSON *payload, cJSON **result, char *error, size_t error_size) {
    return update_role_user(database, customer_id, "CUSTOMER", payload, result, error, error_size);
}
