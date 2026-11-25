#ifndef COURIER_ADMIN_H
#define COURIER_ADMIN_H

#include <stdbool.h>
#include <stddef.h>
#include <cjson/cJSON.h>
#include "database.h"

bool admin_dashboard(Database *database, cJSON **result, char *error, size_t error_size);
bool admin_reports(Database *database, cJSON **result, char *error, size_t error_size);

#endif
