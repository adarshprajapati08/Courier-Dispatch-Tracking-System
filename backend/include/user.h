#ifndef COURIER_USER_H
#define COURIER_USER_H

#include <stdbool.h>
#include <stddef.h>
#include <cjson/cJSON.h>
#include "auth.h"
#include "database.h"

bool user_get_profile(Database *database, int user_id, cJSON **result, char *error, size_t error_size);
bool user_update_profile(Database *database, int user_id, const cJSON *payload,
                         cJSON **result, char *error, size_t error_size);
bool user_list_customers(Database *database, const char *query, cJSON **result,
                         char *error, size_t error_size);
bool user_get_customer_details(Database *database, int customer_id, cJSON **result,
                               char *error, size_t error_size);
bool user_list_staff(Database *database, const char *query, cJSON **result,
                     char *error, size_t error_size);
bool user_create_staff(Database *database, const cJSON *payload, cJSON **result,
                       char *error, size_t error_size);
bool user_update_staff(Database *database, int staff_id, const cJSON *payload,
                       cJSON **result, char *error, size_t error_size);
bool user_update_customer(Database *database, int customer_id, const cJSON *payload,
                          cJSON **result, char *error, size_t error_size);

#endif
