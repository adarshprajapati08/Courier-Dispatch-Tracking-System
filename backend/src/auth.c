#include "auth.h"
#include "utils.h"
#include "validation.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <civetweb.h>

#define PBKDF2_ITERATIONS 210000
#define HASH_BYTES 32
#define SALT_BYTES 16
#define SESSION_TOKEN_BYTES 32

static void set_error(char *error, size_t error_size, const char *message) {
    if (error != NULL && error_size > 0) snprintf(error, error_size, "%s", message == NULL ? "Authentication failed" : message);
}

static bool hex_to_bytes(const char *hex, unsigned char *bytes, size_t bytes_length) {
    size_t i;
    if (hex == NULL || bytes == NULL || strlen(hex) != bytes_length * 2) return false;
    for (i = 0; i < bytes_length; i++) {
        int high, low;
        if (!isxdigit((unsigned char)hex[i * 2]) || !isxdigit((unsigned char)hex[i * 2 + 1])) return false;
        high = isdigit((unsigned char)hex[i * 2]) ? hex[i * 2] - '0' : tolower((unsigned char)hex[i * 2]) - 'a' + 10;
        low = isdigit((unsigned char)hex[i * 2 + 1]) ? hex[i * 2 + 1] - '0' : tolower((unsigned char)hex[i * 2 + 1]) - 'a' + 10;
        bytes[i] = (unsigned char)((high << 4) | low);
    }
    return true;
}

static void bytes_to_hex(const unsigned char *bytes, size_t length, char *output, size_t output_size) {
    size_t i;
    if (output == NULL || output_size < length * 2 + 1) return;
    for (i = 0; i < length; i++) snprintf(output + i * 2, output_size - i * 2, "%02x", bytes[i]);
}

static void session_expiry(char *output, size_t output_size) {
    time_t future = time(NULL) + 7 * 24 * 60 * 60;
    struct tm value;
#if defined(_WIN32)
    gmtime_s(&value, &future);
#else
    gmtime_r(&future, &value);
#endif
    strftime(output, output_size, "%Y-%m-%dT%H:%M:%SZ", &value);
}

const char *role_to_string(UserRole role) {
    switch (role) {
        case ROLE_CUSTOMER: return "CUSTOMER";
        case ROLE_ADMIN: return "ADMIN";
        case ROLE_STAFF: return "STAFF";
        default: return "";
    }
}

UserRole role_from_string(const char *role) {
    if (role == NULL) return ROLE_NONE;
    if (strcmp(role, "CUSTOMER") == 0) return ROLE_CUSTOMER;
    if (strcmp(role, "ADMIN") == 0) return ROLE_ADMIN;
    if (strcmp(role, "STAFF") == 0) return ROLE_STAFF;
    return ROLE_NONE;
}

bool auth_hash_password(const char *password, char *salt_hex, size_t salt_size, char *hash_hex, size_t hash_size) {
    unsigned char salt[SALT_BYTES];
    unsigned char hash[HASH_BYTES];
    if (password == NULL || salt_hex == NULL || hash_hex == NULL || salt_size < SALT_BYTES * 2 + 1 || hash_size < HASH_BYTES * 2 + 1) return false;
    if (!random_bytes(salt, sizeof(salt))) return false;
    if (PKCS5_PBKDF2_HMAC(password, (int)strlen(password), salt, sizeof(salt), PBKDF2_ITERATIONS, EVP_sha256(), sizeof(hash), hash) != 1) return false;
    bytes_to_hex(salt, sizeof(salt), salt_hex, salt_size);
    bytes_to_hex(hash, sizeof(hash), hash_hex, hash_size);
    return true;
}

bool auth_verify_password(const char *password, const char *salt_hex, const char *hash_hex) {
    unsigned char salt[SALT_BYTES], stored[HASH_BYTES], calculated[HASH_BYTES];
    if (password == NULL || !hex_to_bytes(salt_hex, salt, sizeof(salt)) || !hex_to_bytes(hash_hex, stored, sizeof(stored))) return false;
    if (PKCS5_PBKDF2_HMAC(password, (int)strlen(password), salt, sizeof(salt), PBKDF2_ITERATIONS, EVP_sha256(), sizeof(calculated), calculated) != 1) return false;
    return constant_time_equals(stored, calculated, sizeof(stored));
}

bool auth_get_me_json(const AuthUser *user, cJSON **result) {
    cJSON *data;
    if (user == NULL || result == NULL) return false;
    data = cJSON_CreateObject();
    if (data == NULL) return false;
    cJSON_AddNumberToObject(data, "id", user->id);
    cJSON_AddStringToObject(data, "name", user->name);
    cJSON_AddStringToObject(data, "email", user->email);
    cJSON_AddStringToObject(data, "phone", user->phone);
    cJSON_AddStringToObject(data, "role", user->role_name);
    *result = data;
    return true;
}

bool auth_register_customer(Database *database, const cJSON *payload, cJSON **result, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    char salt[65], hash[65];
    const char *password;
    const char *confirmation;
    int sqlite_result;
    int user_id;
    AuthUser user;
    if (!user_payload_valid(payload, true, error, error_size)) return false;
    password = json_string(payload, "password");
    confirmation = json_string(payload, "confirm_password");
    if (confirmation != NULL && strcmp(password, confirmation) != 0) { set_error(error, error_size, "Password confirmation does not match"); return false; }
    if (!auth_hash_password(password, salt, sizeof(salt), hash, sizeof(hash))) { set_error(error, error_size, "Could not securely process the password"); return false; }
    db_lock(database);
    sqlite_result = sqlite3_prepare_v2(database->conn,
        "INSERT INTO users(name,email,phone,password_hash,password_salt,role,account_status) VALUES(?,?,?,?,?,'CUSTOMER','ACTIVE')",
        -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) {
        sqlite3_bind_text(statement, 1, json_string(payload, "name"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, json_string(payload, "email"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 3, json_string(payload, "phone"), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 4, hash, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 5, salt, -1, SQLITE_TRANSIENT);
        sqlite_result = sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
    if (sqlite_result != SQLITE_DONE) {
        if (sqlite_result == SQLITE_CONSTRAINT) set_error(error, error_size, "An account with this email already exists");
        else set_error(error, error_size, "Unable to create the account");
        db_unlock(database);
        return false;
    }
    user_id = (int)sqlite3_last_insert_rowid(database->conn);
    db_unlock(database);
    memset(&user, 0, sizeof(user));
    user.id = user_id;
    user.role = ROLE_CUSTOMER;
    copy_string(user.role_name, sizeof(user.role_name), "CUSTOMER");
    copy_string(user.name, sizeof(user.name), json_string(payload, "name"));
    copy_string(user.email, sizeof(user.email), json_string(payload, "email"));
    copy_string(user.phone, sizeof(user.phone), json_string(payload, "phone"));
    return auth_get_me_json(&user, result);
}

bool auth_login(Database *database, const cJSON *payload, AuthUser *user, char *token, size_t token_size, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    const char *email;
    const char *password;
    const char *requested_role;
    const unsigned char *stored_hash;
    const unsigned char *stored_salt;
    const unsigned char *role;
    const unsigned char *status;
    char token_hash[65], expiry[64];
    int sqlite_result;
    if (payload == NULL || !cJSON_IsObject(payload) || user == NULL || token == NULL) { set_error(error, error_size, "Invalid login request"); return false; }
    email = json_string(payload, "email");
    password = json_string(payload, "password");
    requested_role = json_string(payload, "role");
    if (!validate_email_address(email) || password == NULL || strlen(password) > 128) { set_error(error, error_size, "Enter a valid email address and password"); return false; }
    if (requested_role != NULL && role_from_string(requested_role) == ROLE_NONE) { set_error(error, error_size, "Invalid account role"); return false; }
    db_lock(database);
    sqlite_result = sqlite3_prepare_v2(database->conn,
        "SELECT id,name,email,phone,password_hash,password_salt,role,account_status FROM users WHERE email = ? COLLATE NOCASE LIMIT 1",
        -1, &statement, NULL);
    if (sqlite_result != SQLITE_OK) { db_unlock(database); set_error(error, error_size, "Unable to process login"); return false; }
    sqlite3_bind_text(statement, 1, email, -1, SQLITE_TRANSIENT);
    sqlite_result = sqlite3_step(statement);
    if (sqlite_result != SQLITE_ROW) { sqlite3_finalize(statement); db_unlock(database); set_error(error, error_size, "Invalid email or password"); return false; }
    stored_hash = sqlite3_column_text(statement, 4);
    stored_salt = sqlite3_column_text(statement, 5);
    role = sqlite3_column_text(statement, 6);
    status = sqlite3_column_text(statement, 7);
    if (stored_hash == NULL || stored_salt == NULL || role == NULL || status == NULL || strcmp((const char *)status, "ACTIVE") != 0 ||
        !auth_verify_password(password, (const char *)stored_salt, (const char *)stored_hash) ||
        (requested_role != NULL && strcmp(requested_role, (const char *)role) != 0)) {
        sqlite3_finalize(statement); db_unlock(database); set_error(error, error_size, "Invalid email, password, or selected role"); return false;
    }
    memset(user, 0, sizeof(*user));
    user->id = sqlite3_column_int(statement, 0);
    user->role = role_from_string((const char *)role);
    copy_string(user->role_name, sizeof(user->role_name), (const char *)role);
    copy_string(user->name, sizeof(user->name), (const char *)sqlite3_column_text(statement, 1));
    copy_string(user->email, sizeof(user->email), (const char *)sqlite3_column_text(statement, 2));
    copy_string(user->phone, sizeof(user->phone), (const char *)sqlite3_column_text(statement, 3));
    sqlite3_finalize(statement);
    if (!random_hex(token, token_size, SESSION_TOKEN_BYTES) || !sha256_hex(token, token_hash, sizeof(token_hash))) { db_unlock(database); set_error(error, error_size, "Unable to create a secure session"); return false; }
    session_expiry(expiry, sizeof(expiry));
    db_exec(database, "DELETE FROM sessions WHERE expires_at <= strftime('%Y-%m-%dT%H:%M:%SZ','now')", NULL, 0);
    sqlite_result = sqlite3_prepare_v2(database->conn, "INSERT INTO sessions(user_id,token_hash,expires_at) VALUES(?,?,?)", -1, &statement, NULL);
    if (sqlite_result == SQLITE_OK) {
        sqlite3_bind_int(statement, 1, user->id);
        sqlite3_bind_text(statement, 2, token_hash, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 3, expiry, -1, SQLITE_TRANSIENT);
        sqlite_result = sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
    db_unlock(database);
    if (sqlite_result != SQLITE_DONE) { set_error(error, error_size, "Unable to create session"); return false; }
    return true;
}

bool auth_logout(Database *database, const char *token, char *error, size_t error_size) {
    sqlite3_stmt *statement = NULL;
    char hash[65];
    int result;
    if (token == NULL || strlen(token) != SESSION_TOKEN_BYTES * 2 || !sha256_hex(token, hash, sizeof(hash))) { set_error(error, error_size, "No active session found"); return false; }
    db_lock(database);
    result = sqlite3_prepare_v2(database->conn, "DELETE FROM sessions WHERE token_hash = ?", -1, &statement, NULL);
    if (result == SQLITE_OK) {
        sqlite3_bind_text(statement, 1, hash, -1, SQLITE_TRANSIENT);
        result = sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
    db_unlock(database);
    if (result != SQLITE_DONE) { set_error(error, error_size, "Unable to end session"); return false; }
    return true;
}

static bool cookie_value(const char *cookie_header, const char *name, char *output, size_t output_size) {
    const char *cursor = cookie_header;
    size_t name_length = strlen(name);
    if (output == NULL || output_size == 0) return false;
    output[0] = '\0';
    while (cursor != NULL && *cursor) {
        const char *end;
        while (*cursor == ' ' || *cursor == ';') cursor++;
        if (strncmp(cursor, name, name_length) == 0 && cursor[name_length] == '=') {
            const char *value = cursor + name_length + 1;
            end = strchr(value, ';');
            if (end == NULL) end = value + strlen(value);
            if ((size_t)(end - value) >= output_size) return false;
            memcpy(output, value, (size_t)(end - value));
            output[end - value] = '\0';
            return true;
        }
        cursor = strchr(cursor, ';');
        if (cursor != NULL) cursor++;
    }
    return false;
}

bool auth_get_user_from_cookie(Database *database, const struct mg_connection *connection, AuthUser *user, char *error, size_t error_size) {
    const char *cookie_header;
    char token[SESSION_TOKEN_BYTES * 2 + 1];
    char hash[65];
    sqlite3_stmt *statement = NULL;
    int result;
    if (database == NULL || connection == NULL || user == NULL) { set_error(error, error_size, "Authentication is unavailable"); return false; }
    cookie_header = mg_get_header(connection, "Cookie");
    if (!cookie_value(cookie_header, "courier_session", token, sizeof(token)) || !sha256_hex(token, hash, sizeof(hash))) { set_error(error, error_size, "Please sign in to continue"); return false; }
    db_lock(database);
    result = sqlite3_prepare_v2(database->conn,
        "SELECT u.id,u.name,u.email,u.phone,u.role FROM sessions s JOIN users u ON u.id=s.user_id "
        "WHERE s.token_hash=? AND s.expires_at > strftime('%Y-%m-%dT%H:%M:%SZ','now') AND u.account_status='ACTIVE' LIMIT 1",
        -1, &statement, NULL);
    if (result == SQLITE_OK) {
        sqlite3_bind_text(statement, 1, hash, -1, SQLITE_TRANSIENT);
        result = sqlite3_step(statement);
    }
    if (result == SQLITE_ROW) {
        memset(user, 0, sizeof(*user));
        user->id = sqlite3_column_int(statement, 0);
        copy_string(user->name, sizeof(user->name), (const char *)sqlite3_column_text(statement, 1));
        copy_string(user->email, sizeof(user->email), (const char *)sqlite3_column_text(statement, 2));
        copy_string(user->phone, sizeof(user->phone), (const char *)sqlite3_column_text(statement, 3));
        copy_string(user->role_name, sizeof(user->role_name), (const char *)sqlite3_column_text(statement, 4));
        user->role = role_from_string(user->role_name);
    }
    sqlite3_finalize(statement);
    db_unlock(database);
    if (result != SQLITE_ROW || user->role == ROLE_NONE) { set_error(error, error_size, "Your session has expired. Please sign in again"); return false; }
    return true;
}
