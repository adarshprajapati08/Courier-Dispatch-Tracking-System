#ifndef COURIER_SHIPMENT_H
#define COURIER_SHIPMENT_H

#include <stdbool.h>
#include <stddef.h>
#include <cjson/cJSON.h>
#include "auth.h"
#include "database.h"

bool shipment_create(Database *database, const cJSON *payload, int customer_id,
                     const char *actor_name, cJSON **result, char *error, size_t error_size);
bool shipment_get(Database *database, int shipment_id, cJSON **result, char *error, size_t error_size);
bool shipment_get_for_customer(Database *database, int shipment_id, int customer_id,
                               cJSON **result, char *error, size_t error_size);
bool shipment_get_for_staff(Database *database, int shipment_id, int staff_id,
                            cJSON **result, char *error, size_t error_size);
bool shipment_list_customer(Database *database, int customer_id, const char *query,
                            cJSON **result, char *error, size_t error_size);
bool shipment_list_admin(Database *database, const char *query, cJSON **result,
                         char *error, size_t error_size);
bool shipment_list_staff(Database *database, int staff_id, const char *query, cJSON **result,
                         char *error, size_t error_size);
bool shipment_update(Database *database, int shipment_id, const cJSON *payload,
                     cJSON **result, char *error, size_t error_size);
bool shipment_update_status(Database *database, int shipment_id, const cJSON *payload,
                            const AuthUser *actor, bool staff_restricted,
                            cJSON **result, char *error, size_t error_size);
bool shipment_assign_staff(Database *database, int shipment_id, const cJSON *payload,
                          cJSON **result, char *error, size_t error_size);
bool shipment_cancel(Database *database, int shipment_id, const AuthUser *actor,
                     cJSON **result, char *error, size_t error_size);
bool shipment_delete(Database *database, int shipment_id, char *error, size_t error_size);

#endif
