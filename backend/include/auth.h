#ifndef COURIER_AUTH_H
#define COURIER_AUTH_H

#include <stdbool.h>
#include <stddef.h>
#include <cjson/cJSON.h>
#include "database.h"

struct mg_connection;

typedef enum {
    ROLE_NONE = 0,
    ROLE_CUSTOMER,
    ROLE_ADMIN,
    ROLE_STAFF
} UserRole;

typedef struct {
    int id;
    UserRole role;
    char role_name[16];
    char name[128];
    char email[160];
    char phone[32];
} AuthUser;

const char *role_to_string(UserRole role);
UserRole role_from_string(const char *role);
bool auth_hash_password(const char *password, char *salt_hex, size_t salt_size, char *hash_hex, size_t hash_size);
bool auth_verify_password(const char *password, const char *salt_hex, const char *hash_hex);
bool auth_register_customer(Database *database, const cJSON *payload, cJSON **result, char *error, size_t error_size);
bool auth_login(Database *database, const cJSON *payload, AuthUser *user, char *token, size_t token_size, char *error, size_t error_size);
bool auth_logout(Database *database, const char *token, char *error, size_t error_size);
bool auth_get_user_from_cookie(Database *database, const struct mg_connection *connection, AuthUser *user, char *error, size_t error_size);
bool auth_get_me_json(const AuthUser *user, cJSON **result);

#endif
