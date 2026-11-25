#ifndef COURIER_VALIDATION_H
#define COURIER_VALIDATION_H

#include <stdbool.h>
#include <stddef.h>
#include <cjson/cJSON.h>

bool validate_required_string(const cJSON *object, const char *key, size_t min_len, size_t max_len, char *error, size_t error_size);
bool validate_email_address(const char *email);
bool validate_phone_number(const char *phone);
bool validate_pincode(const char *pincode);
bool validate_password_strength(const char *password, char *error, size_t error_size);
bool validate_delivery_type(const char *type);
bool validate_package_type(const char *type);
bool validate_status(const char *status);
bool valid_status_transition(const char *from, const char *to);
bool shipment_payload_valid(const cJSON *payload, char *error, size_t error_size);
bool user_payload_valid(const cJSON *payload, bool require_password, char *error, size_t error_size);

#endif
