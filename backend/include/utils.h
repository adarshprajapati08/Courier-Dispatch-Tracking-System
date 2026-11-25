#ifndef COURIER_UTILS_H
#define COURIER_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <cjson/cJSON.h>

void copy_string(char *destination, size_t destination_size, const char *source);
void trim_whitespace(char *text);
bool string_is_blank(const char *text);
bool constant_time_equals(const unsigned char *a, const unsigned char *b, size_t length);
bool random_bytes(unsigned char *buffer, size_t length);
bool random_hex(char *output, size_t output_size, size_t bytes);
bool sha256_hex(const char *input, char *output, size_t output_size);
void timestamp_now(char *output, size_t output_size);
void date_days_from_now(int days, char *output, size_t output_size);
void json_add_string_or_null(cJSON *object, const char *key, const char *value);
const char *json_string(const cJSON *object, const char *key);
double json_number(const cJSON *object, const char *key, double fallback);
int json_integer(const cJSON *object, const char *key, int fallback);
bool json_boolean(const cJSON *object, const char *key, bool fallback);
void query_value(const char *query, const char *name, char *output, size_t output_size);
void url_decode(const char *source, char *output, size_t output_size);
void escape_like_pattern(const char *input, char *output, size_t output_size);

#endif
