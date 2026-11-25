#include "validation.h"
#include "utils.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void set_error(char *error, size_t error_size, const char *message) {
    if (error != NULL && error_size > 0) snprintf(error, error_size, "%s", message);
}

bool validate_required_string(const cJSON *object, const char *key, size_t min_len, size_t max_len, char *error, size_t error_size) {
    const char *value = json_string(object, key);
    size_t length;
    if (value == NULL || string_is_blank(value)) {
        snprintf(error, error_size, "%s is required", key);
        return false;
    }
    length = strlen(value);
    if (length < min_len || length > max_len) {
        snprintf(error, error_size, "%s must contain %zu to %zu characters", key, min_len, max_len);
        return false;
    }
    return true;
}

bool validate_email_address(const char *email) {
    const char *at;
    const char *dot;
    size_t length;
    if (email == NULL) return false;
    length = strlen(email);
    if (length < 5 || length > 150 || strchr(email, ' ') != NULL) return false;
    at = strchr(email, '@');
    if (at == NULL || at == email || strchr(at + 1, '@') != NULL) return false;
    dot = strrchr(at + 1, '.');
    return dot != NULL && dot > at + 1 && dot[1] != '\0';
}

bool validate_phone_number(const char *phone) {
    size_t digits = 0;
    if (phone == NULL || strlen(phone) > 20) return false;
    for (; *phone; phone++) {
        if (isdigit((unsigned char)*phone)) digits++;
        else if (*phone != '+' && *phone != '-' && *phone != ' ' && *phone != '(' && *phone != ')') return false;
    }
    return digits >= 10 && digits <= 15;
}

bool validate_pincode(const char *pincode) {
    size_t length;
    if (pincode == NULL) return false;
    length = strlen(pincode);
    if (length < 4 || length > 10) return false;
    while (*pincode) {
        if (!isdigit((unsigned char)*pincode++)) return false;
    }
    return true;
}

bool validate_password_strength(const char *password, char *error, size_t error_size) {
    bool upper = false, lower = false, digit = false;
    const char *cursor;
    if (password == NULL || strlen(password) < 8 || strlen(password) > 128) {
        set_error(error, error_size, "Password must be 8 to 128 characters long");
        return false;
    }
    for (cursor = password; *cursor; cursor++) {
        upper |= isupper((unsigned char)*cursor) != 0;
        lower |= islower((unsigned char)*cursor) != 0;
        digit |= isdigit((unsigned char)*cursor) != 0;
    }
    if (!upper || !lower || !digit) {
        set_error(error, error_size, "Password must contain uppercase, lowercase, and numeric characters");
        return false;
    }
    return true;
}

bool validate_delivery_type(const char *type) {
    return type != NULL && (strcmp(type, "Standard") == 0 || strcmp(type, "Express") == 0);
}

bool validate_package_type(const char *type) {
    return type != NULL && (strcmp(type, "Document") == 0 || strcmp(type, "Parcel") == 0 ||
                            strcmp(type, "Fragile") == 0 || strcmp(type, "Electronics") == 0 ||
                            strcmp(type, "Other") == 0);
}

bool validate_status(const char *status) {
    static const char *valid[] = {"BOOKED", "PICKED_UP", "IN_TRANSIT", "OUT_FOR_DELIVERY", "DELIVERED", "DELIVERY_FAILED", "CANCELLED"};
    size_t i;
    if (status == NULL) return false;
    for (i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) if (strcmp(status, valid[i]) == 0) return true;
    return false;
}

bool valid_status_transition(const char *from, const char *to) {
    if (from == NULL || to == NULL || !validate_status(from) || !validate_status(to) || strcmp(from, to) == 0) return false;
    if (strcmp(from, "BOOKED") == 0) return strcmp(to, "PICKED_UP") == 0 || strcmp(to, "CANCELLED") == 0;
    if (strcmp(from, "PICKED_UP") == 0) return strcmp(to, "IN_TRANSIT") == 0 || strcmp(to, "CANCELLED") == 0;
    if (strcmp(from, "IN_TRANSIT") == 0) return strcmp(to, "OUT_FOR_DELIVERY") == 0 || strcmp(to, "DELIVERY_FAILED") == 0;
    if (strcmp(from, "OUT_FOR_DELIVERY") == 0) return strcmp(to, "DELIVERED") == 0 || strcmp(to, "DELIVERY_FAILED") == 0;
    if (strcmp(from, "DELIVERY_FAILED") == 0) return strcmp(to, "OUT_FOR_DELIVERY") == 0 || strcmp(to, "CANCELLED") == 0;
    return false;
}

bool shipment_payload_valid(const cJSON *payload, char *error, size_t error_size) {
    static const char *required_fields[] = {
        "sender_name", "sender_phone", "sender_email", "sender_address", "sender_city", "sender_state", "sender_pincode",
        "receiver_name", "receiver_phone", "receiver_email", "receiver_address", "receiver_city", "receiver_state", "receiver_pincode",
        "package_description", "package_type", "delivery_type"
    };
    size_t i;
    double weight;
    double declared_value;
    if (payload == NULL || !cJSON_IsObject(payload)) { set_error(error, error_size, "A JSON shipment object is required"); return false; }
    for (i = 0; i < sizeof(required_fields) / sizeof(required_fields[0]); i++) {
        if (!validate_required_string(payload, required_fields[i], 1, 500, error, error_size)) return false;
    }
    if (!validate_phone_number(json_string(payload, "sender_phone")) || !validate_phone_number(json_string(payload, "receiver_phone"))) {
        set_error(error, error_size, "Phone numbers must contain 10 to 15 digits"); return false;
    }
    if (!validate_email_address(json_string(payload, "sender_email")) || !validate_email_address(json_string(payload, "receiver_email"))) {
        set_error(error, error_size, "A valid sender and receiver email address is required"); return false;
    }
    if (!validate_pincode(json_string(payload, "sender_pincode")) || !validate_pincode(json_string(payload, "receiver_pincode"))) {
        set_error(error, error_size, "PIN codes must contain 4 to 10 digits"); return false;
    }
    weight = json_number(payload, "package_weight", -1.0);
    if (weight < 0.05 || weight > 100.0) { set_error(error, error_size, "Package weight must be between 0.05 and 100 kg"); return false; }
    if (!validate_package_type(json_string(payload, "package_type"))) { set_error(error, error_size, "Invalid package type"); return false; }
    if (!validate_delivery_type(json_string(payload, "delivery_type"))) { set_error(error, error_size, "Delivery type must be Standard or Express"); return false; }
    declared_value = json_number(payload, "declared_value", 0.0);
    if (declared_value < 0 || declared_value > 10000000.0) { set_error(error, error_size, "Declared package value is invalid"); return false; }
    return true;
}

bool user_payload_valid(const cJSON *payload, bool require_password, char *error, size_t error_size) {
    const char *email;
    const char *phone;
    const char *password;
    if (payload == NULL || !cJSON_IsObject(payload)) { set_error(error, error_size, "A JSON user object is required"); return false; }
    if (!validate_required_string(payload, "name", 2, 120, error, error_size)) return false;
    email = json_string(payload, "email");
    phone = json_string(payload, "phone");
    if (!validate_email_address(email)) { set_error(error, error_size, "A valid email address is required"); return false; }
    if (!validate_phone_number(phone)) { set_error(error, error_size, "A valid phone number is required"); return false; }
    password = json_string(payload, "password");
    if (require_password && !validate_password_strength(password, error, error_size)) return false;
    if (!require_password && password != NULL && !validate_password_strength(password, error, error_size)) return false;
    return true;
}
