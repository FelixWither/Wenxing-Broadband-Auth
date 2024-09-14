#ifndef HELPERS_H
#define HELPERS_H

#include <curl/curl.h> // Ensure this is included for curl functions
#include <cjson/cJSON.h>

#define CLIENT_ID "6d6bc6f3b5f04107a5fc1c62e39dd5f4"
#define USER_AGENT "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) " \
                     "AppleWebKit/605.1.15 (KHTML, like Gecko) " \
                     "Version/17.1 Safari/605.1.15"
#define CONTENT_TYPE "Content-Type: application/json"
#define ACCEPT_ENCODING "Accept-Encoding: gzip, deflate, br"
#define CRED "\033[31m"
#define CBLUE "\033[34m"
#define CRESET "\033[0m"

struct memory {
    char *response;
    size_t size;
};

struct curl_slist *set_common_headers();
struct memory;
void print_json_err(const char *error_message, const char *response);
char *parse_yaml(const char *filename, char *key_to_find);
int append_yaml_if_missing(const char *filename, const char *key, const char *value);
// int key_exists(const char *filename, const char *key_to_check);
void reformat(FILE *file, const char *phone, const char *uid, const char *isp);
char *get_executable_dir();
size_t header_search(void *data, size_t size, size_t nmemb, void *userp);

#endif // HELPERS_H
