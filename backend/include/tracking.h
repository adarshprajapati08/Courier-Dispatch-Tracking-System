#ifndef COURIER_TRACKING_H
#define COURIER_TRACKING_H

#include <stdbool.h>
#include <stddef.h>
#include <cjson/cJSON.h>
#include "database.h"

bool tracking_add_event(Database *database, int shipment_id, const char *tracking_number,
                        const char *status, const char *location, const char *remarks,
                        const char *updated_by, char *error, size_t error_size);
bool tracking_get_public(Database *database, const char *tracking_number, cJSON **result,
                         char *error, size_t error_size);
bool tracking_history_for_shipment(Database *database, int shipment_id, cJSON **history,
                                   char *error, size_t error_size);

#endif
