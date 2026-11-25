#ifndef COURIER_SERVER_H
#define COURIER_SERVER_H

#include <stdbool.h>
#include "database.h"

struct mg_context;

typedef struct {
    Database database;
    struct mg_context *http_context;
} AppContext;

bool server_start(AppContext *application, const char *frontend_path, const char *port);
void server_stop(AppContext *application);

#endif
