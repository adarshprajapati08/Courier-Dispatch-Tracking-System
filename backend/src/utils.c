#include "utils.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

void copy_string(char *destination, size_t destination_size, const char *source) {
    if (destination == NULL || destination_size == 0) return;
    if (source == NULL) {
        destination[0] = '\0';
        return;
    }
    snprintf(destination, destination_size, "%s", source);
}

void trim_whitespace(char *text) {
    char *start;
    char *end;
    if (text == NULL) return;
    start = text;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != text) memmove(text, start, strlen(start) + 1);
    if (*text == '\0') return;
    end = text + strlen(text) - 1;
    while (end >= text && isspace((unsigned char)*end)) *end-- = '\0';
}

bool string_is_blank(const char *text) {
    if (text == NULL) return true;
    while (*text) {
        if (!isspace((unsigned char)*text)) return false;
        text++;
    }
    return true;
}

bool constant_time_equals(const unsigned char *a, const unsigned char *b, size_t length) {
    unsigned char difference = 0;
    size_t i;
    if (a == NULL || b == NULL) return false;
    for (i = 0; i < length; i++) difference |= (unsigned char)(a[i] ^ b[i]);
    return difference == 0;
}

bool random_bytes(unsigned char *buffer, size_t length) {
    return buffer != NULL && length > 0 && RAND_bytes(buffer, (int)length) == 1;
}

bool random_hex(char *output, size_t output_size, size_t bytes) {
    unsigned char raw[128];
    static const char hex[] = "0123456789abcdef";
    size_t i;
    if (bytes == 0 || bytes > sizeof(raw) || output == NULL || output_size < bytes * 2 + 1) return false;
    if (!random_bytes(raw, bytes)) return false;
    for (i = 0; i < bytes; i++) {
        output[i * 2] = hex[raw[i] >> 4];
        output[i * 2 + 1] = hex[raw[i] & 0x0f];
    }
    output[bytes * 2] = '\0';
    return true;
}

bool sha256_hex(const char *input, char *output, size_t output_size) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    size_t i;
    if (input == NULL || output == NULL || output_size < 65) return false;
    if (EVP_Digest(input, strlen(input), digest, &digest_len, EVP_sha256(), NULL) != 1 || digest_len != 32) return false;
    for (i = 0; i < digest_len; i++) snprintf(output + i * 2, output_size - i * 2, "%02x", digest[i]);
    return true;
}

void timestamp_now(char *output, size_t output_size) {
    time_t now = time(NULL);
    struct tm value;
    if (output == NULL || output_size == 0) return;
#if defined(_WIN32)
    gmtime_s(&value, &now);
#else
    gmtime_r(&now, &value);
#endif
    strftime(output, output_size, "%Y-%m-%dT%H:%M:%SZ", &value);
}

void date_days_from_now(int days, char *output, size_t output_size) {
    time_t target = time(NULL) + (time_t)days * 86400;
    struct tm value;
    if (output == NULL || output_size == 0) return;
#if defined(_WIN32)
    gmtime_s(&value, &target);
#else
    gmtime_r(&target, &value);
#endif
    strftime(output, output_size, "%Y-%m-%d", &value);
}

void json_add_string_or_null(cJSON *object, const char *key, const char *value) {
    if (value == NULL) cJSON_AddNullToObject(object, key);
    else cJSON_AddStringToObject(object, key, value);
}

const char *json_string(const cJSON *object, const char *key) {
    cJSON *item;
    if (object == NULL || key == NULL) return NULL;
    item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(item) && item->valuestring != NULL ? item->valuestring : NULL;
}

double json_number(const cJSON *object, const char *key, double fallback) {
    cJSON *item;
    if (object == NULL || key == NULL) return fallback;
    item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsNumber(item) ? item->valuedouble : fallback;
}

int json_integer(const cJSON *object, const char *key, int fallback) {
    cJSON *item;
    if (object == NULL || key == NULL) return fallback;
    item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsNumber(item) ? item->valueint : fallback;
}

bool json_boolean(const cJSON *object, const char *key, bool fallback) {
    cJSON *item;
    if (object == NULL || key == NULL) return fallback;
    item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (cJSON_IsTrue(item)) return true;
    if (cJSON_IsFalse(item)) return false;
    return fallback;
}

void url_decode(const char *source, char *output, size_t output_size) {
    size_t write_index = 0;
    if (output == NULL || output_size == 0) return;
    output[0] = '\0';
    if (source == NULL) return;
    while (*source && write_index + 1 < output_size) {
        if (*source == '+' ) {
            output[write_index++] = ' ';
        } else if (*source == '%' && isxdigit((unsigned char)source[1]) && isxdigit((unsigned char)source[2])) {
            char encoded[3] = {source[1], source[2], '\0'};
            output[write_index++] = (char)strtol(encoded, NULL, 16);
            source += 2;
        } else {
            output[write_index++] = *source;
        }
        source++;
    }
    output[write_index] = '\0';
}

void query_value(const char *query, const char *name, char *output, size_t output_size) {
    size_t name_len;
    const char *cursor;
    if (output == NULL || output_size == 0) return;
    output[0] = '\0';
    if (query == NULL || name == NULL) return;
    name_len = strlen(name);
    cursor = query;
    while (*cursor) {
        const char *equals = strchr(cursor, '=');
        const char *end = strchr(cursor, '&');
        size_t key_len;
        if (end == NULL) end = cursor + strlen(cursor);
        if (equals != NULL && equals < end) {
            key_len = (size_t)(equals - cursor);
            if (key_len == name_len && strncmp(cursor, name, name_len) == 0) {
                char encoded[1024];
                size_t value_len = (size_t)(end - equals - 1);
                if (value_len >= sizeof(encoded)) value_len = sizeof(encoded) - 1;
                memcpy(encoded, equals + 1, value_len);
                encoded[value_len] = '\0';
                url_decode(encoded, output, output_size);
                return;
            }
        }
        cursor = *end ? end + 1 : end;
    }
}

void escape_like_pattern(const char *input, char *output, size_t output_size) {
    size_t i = 0, j = 0;
    if (output == NULL || output_size == 0) return;
    if (input == NULL) { output[0] = '\0'; return; }
    while (input[i] && j + 2 < output_size) {
        if (input[i] == '%' || input[i] == '_' || input[i] == '\\') output[j++] = '\\';
        output[j++] = input[i++];
    }
    output[j] = '\0';
}
