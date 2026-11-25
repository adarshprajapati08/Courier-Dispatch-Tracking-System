#ifndef COURIER_STAFF_H
#define COURIER_STAFF_H

#include <stdbool.h>
#include <stddef.h>
#include <cjson/cJSON.h>
#include "database.h"

bool staff_dashboard(Database *database, int staff_id, cJSON **result, char *error, size_t error_size);

#endif
