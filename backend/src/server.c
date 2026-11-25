#include "server.h"
#include "admin.h"
#include "auth.h"
#include "shipment.h"
#include "staff.h"
#include "tracking.h"
#include "user.h"
#include "utils.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <civetweb.h>
#include <cjson/cJSON.h>

#define MAX_JSON_BODY (1024 * 1024)

static const char *reason_phrase(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 413: return "Payload Too Large";
        case 422: return "Unprocessable Content";
        default: return "Internal Server Error";
    }
}

static void send_json(struct mg_connection *connection, int status, bool success, const char *message, cJSON *data, const char *cookie) {
    cJSON *root = cJSON_CreateObject();
    char *serialized;
    if (root == NULL) return;
    cJSON_AddBoolToObject(root, "success", success ? 1 : 0);
    cJSON_AddStringToObject(root, "message", message == NULL ? (success ? "Request completed" : "Request failed") : message);
    if (data != NULL) cJSON_AddItemToObject(root, "data", data);
    else cJSON_AddNullToObject(root, "data");
    serialized = cJSON_PrintUnformatted(root);
    if (serialized == NULL) { cJSON_Delete(root); return; }
    mg_printf(connection,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Cache-Control: no-store\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "%s"
        "Content-Length: %zu\r\n\r\n%s",
        status, reason_phrase(status), cookie == NULL ? "" : cookie, strlen(serialized), serialized);
    cJSON_free(serialized);
    cJSON_Delete(root);
}

static void send_error(struct mg_connection *connection, int status, const char *message) {
    send_json(connection, status, false, message, NULL, NULL);
}

static bool method_is(const struct mg_request_info *request, const char *method) {
    return request != NULL && request->request_method != NULL && strcmp(request->request_method, method) == 0;
}

static cJSON *read_json_body(struct mg_connection *connection, char *error, size_t error_size) {
    const struct mg_request_info *request = mg_get_request_info(connection);
    char *body;
    long long expected;
    size_t received = 0;
    const char *parse_end = NULL;
    cJSON *json;
    if (request == NULL || request->content_length <= 0) { snprintf(error, error_size, "A JSON request body is required"); return NULL; }
    expected = request->content_length;
    if (expected > MAX_JSON_BODY) { snprintf(error, error_size, "Request body is too large"); return NULL; }
    body = calloc((size_t)expected + 1, 1);
    if (body == NULL) { snprintf(error, error_size, "Unable to read request body"); return NULL; }
    while (received < (size_t)expected) {
        int bytes = mg_read(connection, body + received, (size_t)expected - received);
        if (bytes <= 0) { free(body); snprintf(error, error_size, "Unable to read request body"); return NULL; }
        received += (size_t)bytes;
    }
    json = cJSON_ParseWithLengthOpts(body, received + 1, &parse_end, 1);
    free(body);
    if (json == NULL || parse_end == NULL || !cJSON_IsObject(json)) { cJSON_Delete(json); snprintf(error, error_size, "Request body must contain a valid JSON object"); return NULL; }
    return json;
}

static bool cookie_value(const struct mg_connection *connection, const char *name, char *output, size_t output_size) {
    const char *cookie = mg_get_header(connection, "Cookie");
    const char *cursor = cookie;
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

static bool require_auth(AppContext *application, struct mg_connection *connection, AuthUser *user) {
    char error[256] = {0};
    if (!auth_get_user_from_cookie(&application->database, connection, user, error, sizeof(error))) {
        send_error(connection, 401, error);
        return false;
    }
    return true;
}

static bool require_role(struct mg_connection *connection, const AuthUser *user, UserRole role) {
    if (user->role != role) {
        send_error(connection, 403, "You do not have permission to access this resource");
        return false;
    }
    return true;
}

static bool parse_id_route(const char *uri, const char *prefix, const char *suffix, int *id) {
    const char *value;
    char *end = NULL;
    long parsed;
    size_t prefix_length;
    if (uri == NULL || prefix == NULL || suffix == NULL || id == NULL) return false;
    prefix_length = strlen(prefix);
    if (strncmp(uri, prefix, prefix_length) != 0) return false;
    value = uri + prefix_length;
    if (*value == '\0') return false;
    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || parsed <= 0 || parsed > INT_MAX || strcmp(end, suffix) != 0) return false;
    *id = (int)parsed;
    return true;
}

static void login_response(struct mg_connection *connection, AppContext *application, cJSON *payload) {
    AuthUser user;
    char token[65] = {0};
    char error[256] = {0};
    char cookie[256];
    cJSON *data = NULL;
    if (!auth_login(&application->database, payload, &user, token, sizeof(token), error, sizeof(error))) { send_error(connection, 401, error); return; }
    if (!auth_get_me_json(&user, &data)) { send_error(connection, 500, "Signed in, but could not build session response"); return; }
    snprintf(cookie, sizeof(cookie), "Set-Cookie: courier_session=%s; Path=/; HttpOnly; SameSite=Lax; Max-Age=604800\r\n", token);
    send_json(connection, 200, true, "Login successful", data, cookie);
}

static void handle_auth(AppContext *application, struct mg_connection *connection, const struct mg_request_info *request, const char *uri) {
    char error[256] = {0};
    cJSON *payload = NULL, *data = NULL;
    if (strcmp(uri, "/api/auth/register") == 0) {
        if (!method_is(request, "POST")) { send_error(connection, 405, "Method not allowed"); return; }
        payload = read_json_body(connection, error, sizeof(error));
        if (payload == NULL) { send_error(connection, 400, error); return; }
        if (!auth_register_customer(&application->database, payload, &data, error, sizeof(error))) send_error(connection, 400, error);
        else send_json(connection, 201, true, "Customer account created successfully", data, NULL);
        cJSON_Delete(payload);
        return;
    }
    if (strcmp(uri, "/api/auth/login") == 0) {
        if (!method_is(request, "POST")) { send_error(connection, 405, "Method not allowed"); return; }
        payload = read_json_body(connection, error, sizeof(error));
        if (payload == NULL) { send_error(connection, 400, error); return; }
        login_response(connection, application, payload);
        cJSON_Delete(payload);
        return;
    }
    if (strcmp(uri, "/api/auth/logout") == 0) {
        char token[128];
        char cookie[] = "Set-Cookie: courier_session=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0\r\n";
        if (!method_is(request, "POST")) { send_error(connection, 405, "Method not allowed"); return; }
        if (cookie_value(connection, "courier_session", token, sizeof(token))) auth_logout(&application->database, token, error, sizeof(error));
        send_json(connection, 200, true, "Logout successful", NULL, cookie);
        return;
    }
    if (strcmp(uri, "/api/auth/me") == 0) {
        AuthUser user;
        if (!method_is(request, "GET")) { send_error(connection, 405, "Method not allowed"); return; }
        if (!require_auth(application, connection, &user)) return;
        if (!auth_get_me_json(&user, &data)) send_error(connection, 500, "Unable to load session");
        else send_json(connection, 200, true, "Current session", data, NULL);
        return;
    }
    send_error(connection, 404, "Authentication endpoint not found");
}

static void handle_customer(AppContext *application, struct mg_connection *connection, const struct mg_request_info *request, const char *uri) {
    AuthUser user;
    cJSON *payload = NULL, *data = NULL;
    char error[256] = {0};
    int id;
    if (!require_auth(application, connection, &user) || !require_role(connection, &user, ROLE_CUSTOMER)) return;
    if (strcmp(uri, "/api/customer/profile") == 0) {
        if (method_is(request, "GET")) {
            if (!user_get_profile(&application->database, user.id, &data, error, sizeof(error))) send_error(connection, 404, error);
            else send_json(connection, 200, true, "Profile loaded", data, NULL);
        } else if (method_is(request, "PUT")) {
            payload = read_json_body(connection, error, sizeof(error));
            if (payload == NULL) { send_error(connection, 400, error); return; }
            if (!user_update_profile(&application->database, user.id, payload, &data, error, sizeof(error))) send_error(connection, 400, error);
            else send_json(connection, 200, true, "Profile updated successfully", data, NULL);
            cJSON_Delete(payload);
        } else send_error(connection, 405, "Method not allowed");
        return;
    }
    if (strcmp(uri, "/api/customer/shipments") == 0) {
        if (method_is(request, "GET")) {
            if (!shipment_list_customer(&application->database, user.id, request->query_string, &data, error, sizeof(error))) send_error(connection, 400, error);
            else send_json(connection, 200, true, "Customer shipments loaded", data, NULL);
        } else if (method_is(request, "POST")) {
            payload = read_json_body(connection, error, sizeof(error));
            if (payload == NULL) { send_error(connection, 400, error); return; }
            if (!shipment_create(&application->database, payload, user.id, user.name, &data, error, sizeof(error))) send_error(connection, 400, error);
            else send_json(connection, 201, true, "Shipment created successfully", data, NULL);
            cJSON_Delete(payload);
        } else send_error(connection, 405, "Method not allowed");
        return;
    }
    if (parse_id_route(uri, "/api/customer/shipments/", "/cancel", &id)) {
        if (!method_is(request, "POST")) { send_error(connection, 405, "Method not allowed"); return; }
        if (!shipment_cancel(&application->database, id, &user, &data, error, sizeof(error))) send_error(connection, 400, error);
        else send_json(connection, 200, true, "Shipment cancelled successfully", data, NULL);
        return;
    }
    if (parse_id_route(uri, "/api/customer/shipments/", "", &id)) {
        if (!method_is(request, "GET")) { send_error(connection, 405, "Method not allowed"); return; }
        if (!shipment_get_for_customer(&application->database, id, user.id, &data, error, sizeof(error))) send_error(connection, 404, error);
        else send_json(connection, 200, true, "Shipment loaded", data, NULL);
        return;
    }
    send_error(connection, 404, "Customer endpoint not found");
}

static void handle_admin(AppContext *application, struct mg_connection *connection, const struct mg_request_info *request, const char *uri) {
    AuthUser user;
    cJSON *payload = NULL, *data = NULL;
    char error[256] = {0};
    int id;
    if (!require_auth(application, connection, &user) || !require_role(connection, &user, ROLE_ADMIN)) return;
    if (strcmp(uri, "/api/admin/dashboard") == 0) {
        if (!method_is(request, "GET")) { send_error(connection, 405, "Method not allowed"); return; }
        if (!admin_dashboard(&application->database, &data, error, sizeof(error))) send_error(connection, 500, error);
        else send_json(connection, 200, true, "Dashboard loaded", data, NULL);
        return;
    }
    if (strcmp(uri, "/api/admin/reports") == 0) {
        if (!method_is(request, "GET")) { send_error(connection, 405, "Method not allowed"); return; }
        if (!admin_reports(&application->database, &data, error, sizeof(error))) send_error(connection, 500, error);
        else send_json(connection, 200, true, "Reports loaded", data, NULL);
        return;
    }
    if (strcmp(uri, "/api/admin/shipments") == 0) {
        if (method_is(request, "GET")) {
            if (!shipment_list_admin(&application->database, request->query_string, &data, error, sizeof(error))) send_error(connection, 400, error);
            else send_json(connection, 200, true, "Shipments loaded", data, NULL);
        } else if (method_is(request, "POST")) {
            int customer_id;
            payload = read_json_body(connection, error, sizeof(error));
            if (payload == NULL) { send_error(connection, 400, error); return; }
            customer_id = json_integer(payload, "customer_id", 0);
            if (!shipment_create(&application->database, payload, customer_id, user.name, &data, error, sizeof(error))) send_error(connection, 400, error);
            else send_json(connection, 201, true, "Shipment created successfully", data, NULL);
            cJSON_Delete(payload);
        } else send_error(connection, 405, "Method not allowed");
        return;
    }
    if (parse_id_route(uri, "/api/admin/shipments/", "/status", &id)) {
        if (!method_is(request, "PUT")) { send_error(connection, 405, "Method not allowed"); return; }
        payload = read_json_body(connection, error, sizeof(error));
        if (payload == NULL) { send_error(connection, 400, error); return; }
        if (!shipment_update_status(&application->database, id, payload, &user, false, &data, error, sizeof(error))) send_error(connection, 400, error);
        else send_json(connection, 200, true, "Shipment status updated", data, NULL);
        cJSON_Delete(payload);
        return;
    }
    if (parse_id_route(uri, "/api/admin/shipments/", "/assign", &id)) {
        if (!method_is(request, "PUT")) { send_error(connection, 405, "Method not allowed"); return; }
        payload = read_json_body(connection, error, sizeof(error));
        if (payload == NULL) { send_error(connection, 400, error); return; }
        if (!shipment_assign_staff(&application->database, id, payload, &data, error, sizeof(error))) send_error(connection, 400, error);
        else send_json(connection, 200, true, "Staff member assigned", data, NULL);
        cJSON_Delete(payload);
        return;
    }
    if (parse_id_route(uri, "/api/admin/shipments/", "", &id)) {
        if (method_is(request, "GET")) {
            if (!shipment_get(&application->database, id, &data, error, sizeof(error))) send_error(connection, 404, error);
            else send_json(connection, 200, true, "Shipment loaded", data, NULL);
        } else if (method_is(request, "PUT")) {
            payload = read_json_body(connection, error, sizeof(error));
            if (payload == NULL) { send_error(connection, 400, error); return; }
            if (!shipment_update(&application->database, id, payload, &data, error, sizeof(error))) send_error(connection, 400, error);
            else send_json(connection, 200, true, "Shipment updated successfully", data, NULL);
            cJSON_Delete(payload);
        } else if (method_is(request, "DELETE")) {
            if (!shipment_delete(&application->database, id, error, sizeof(error))) send_error(connection, 400, error);
            else send_json(connection, 200, true, "Cancelled shipment deleted", NULL, NULL);
        } else send_error(connection, 405, "Method not allowed");
        return;
    }
    if (strcmp(uri, "/api/admin/customers") == 0) {
        if (!method_is(request, "GET")) { send_error(connection, 405, "Method not allowed"); return; }
        if (!user_list_customers(&application->database, request->query_string, &data, error, sizeof(error))) send_error(connection, 400, error);
        else send_json(connection, 200, true, "Customers loaded", data, NULL);
        return;
    }
    if (parse_id_route(uri, "/api/admin/customers/", "", &id)) {
        if (method_is(request, "GET")) {
            if (!user_get_customer_details(&application->database, id, &data, error, sizeof(error))) send_error(connection, 404, error);
            else send_json(connection, 200, true, "Customer details loaded", data, NULL);
        } else if (method_is(request, "PUT")) {
            payload = read_json_body(connection, error, sizeof(error));
            if (payload == NULL) { send_error(connection, 400, error); return; }
            if (!user_update_customer(&application->database, id, payload, &data, error, sizeof(error))) send_error(connection, 400, error);
            else send_json(connection, 200, true, "Customer account updated", data, NULL);
            cJSON_Delete(payload);
        } else send_error(connection, 405, "Method not allowed");
        return;
    }
    if (strcmp(uri, "/api/admin/staff") == 0) {
        if (method_is(request, "GET")) {
            if (!user_list_staff(&application->database, request->query_string, &data, error, sizeof(error))) send_error(connection, 400, error);
            else send_json(connection, 200, true, "Staff loaded", data, NULL);
        } else if (method_is(request, "POST")) {
            payload = read_json_body(connection, error, sizeof(error));
            if (payload == NULL) { send_error(connection, 400, error); return; }
            if (!user_create_staff(&application->database, payload, &data, error, sizeof(error))) send_error(connection, 400, error);
            else send_json(connection, 201, true, "Staff account created", data, NULL);
            cJSON_Delete(payload);
        } else send_error(connection, 405, "Method not allowed");
        return;
    }
    if (parse_id_route(uri, "/api/admin/staff/", "", &id)) {
        if (!method_is(request, "PUT")) { send_error(connection, 405, "Method not allowed"); return; }
        payload = read_json_body(connection, error, sizeof(error));
        if (payload == NULL) { send_error(connection, 400, error); return; }
        if (!user_update_staff(&application->database, id, payload, &data, error, sizeof(error))) send_error(connection, 400, error);
        else send_json(connection, 200, true, "Staff account updated", data, NULL);
        cJSON_Delete(payload);
        return;
    }
    send_error(connection, 404, "Administrator endpoint not found");
}

static void handle_staff(AppContext *application, struct mg_connection *connection, const struct mg_request_info *request, const char *uri) {
    AuthUser user;
    cJSON *payload = NULL, *data = NULL;
    char error[256] = {0};
    int id;
    if (!require_auth(application, connection, &user) || !require_role(connection, &user, ROLE_STAFF)) return;
    if (strcmp(uri, "/api/staff/dashboard") == 0) {
        if (!method_is(request, "GET")) { send_error(connection, 405, "Method not allowed"); return; }
        if (!staff_dashboard(&application->database, user.id, &data, error, sizeof(error))) send_error(connection, 500, error);
        else send_json(connection, 200, true, "Staff dashboard loaded", data, NULL);
        return;
    }
    if (strcmp(uri, "/api/staff/shipments") == 0) {
        if (!method_is(request, "GET")) { send_error(connection, 405, "Method not allowed"); return; }
        if (!shipment_list_staff(&application->database, user.id, request->query_string, &data, error, sizeof(error))) send_error(connection, 400, error);
        else send_json(connection, 200, true, "Assigned shipments loaded", data, NULL);
        return;
    }
    if (parse_id_route(uri, "/api/staff/shipments/", "/status", &id)) {
        if (!method_is(request, "PUT")) { send_error(connection, 405, "Method not allowed"); return; }
        payload = read_json_body(connection, error, sizeof(error));
        if (payload == NULL) { send_error(connection, 400, error); return; }
        if (!shipment_update_status(&application->database, id, payload, &user, true, &data, error, sizeof(error))) send_error(connection, 400, error);
        else send_json(connection, 200, true, "Shipment status updated", data, NULL);
        cJSON_Delete(payload);
        return;
    }
    if (parse_id_route(uri, "/api/staff/shipments/", "", &id)) {
        if (!method_is(request, "GET")) { send_error(connection, 405, "Method not allowed"); return; }
        if (!shipment_get_for_staff(&application->database, id, user.id, &data, error, sizeof(error))) send_error(connection, 404, error);
        else send_json(connection, 200, true, "Assigned shipment loaded", data, NULL);
        return;
    }
    send_error(connection, 404, "Staff endpoint not found");
}

static int api_handler(struct mg_connection *connection, void *callback_data) {
    AppContext *application = (AppContext *)callback_data;
    const struct mg_request_info *request = mg_get_request_info(connection);
    const char *uri;
    if (application == NULL || request == NULL || request->local_uri == NULL) { send_error(connection, 500, "Server request error"); return 500; }
    uri = request->local_uri;
    if (method_is(request, "OPTIONS")) {
        mg_printf(connection, "HTTP/1.1 204 No Content\r\nAllow: GET, POST, PUT, DELETE, OPTIONS\r\nContent-Length: 0\r\n\r\n");
        return 204;
    }
    if (strncmp(uri, "/api/tracking/", 14) == 0) {
        const char *tracking_number = uri + 14;
        char error[256] = {0};
        cJSON *data = NULL;
        if (!method_is(request, "GET")) send_error(connection, 405, "Method not allowed");
        else if (tracking_number[0] == '\0' || strchr(tracking_number, '/') != NULL) send_error(connection, 400, "Invalid tracking number");
        else if (!tracking_get_public(&application->database, tracking_number, &data, error, sizeof(error))) send_error(connection, 404, error);
        else send_json(connection, 200, true, "Tracking information loaded", data, NULL);
        return 1;
    }
    if (strncmp(uri, "/api/auth/", 10) == 0) { handle_auth(application, connection, request, uri); return 1; }
    if (strncmp(uri, "/api/customer/", 14) == 0) { handle_customer(application, connection, request, uri); return 1; }
    if (strncmp(uri, "/api/admin/", 11) == 0) { handle_admin(application, connection, request, uri); return 1; }
    if (strncmp(uri, "/api/staff/", 11) == 0) { handle_staff(application, connection, request, uri); return 1; }
    send_error(connection, 404, "API endpoint not found");
    return 404;
}

bool server_start(AppContext *application, const char *frontend_path, const char *port) {
    struct mg_callbacks callbacks;
    const char *options[12];
    if (application == NULL || frontend_path == NULL || port == NULL) return false;
    memset(&callbacks, 0, sizeof(callbacks));
    options[0] = "document_root";
    options[1] = frontend_path;
    options[2] = "listening_ports";
    options[3] = port;
    options[4] = "num_threads";
    options[5] = "8";
    options[6] = "enable_keep_alive";
    options[7] = "yes";
    options[8] = "request_timeout_ms";
    options[9] = "10000";
    options[10] = NULL;
    options[11] = NULL;
    application->http_context = mg_start(&callbacks, application, options);
    if (application->http_context == NULL) return false;
    mg_set_request_handler(application->http_context, "/api", api_handler, application);
    return true;
}

void server_stop(AppContext *application) {
    if (application != NULL && application->http_context != NULL) {
        mg_stop(application->http_context);
        application->http_context = NULL;
    }
}
