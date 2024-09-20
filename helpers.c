#include "helpers.h"
#include <unistd.h>
#include <libgen.h>
#include <yaml.h>

struct curl_slist *set_common_headers() {
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, USER_AGENT);
    headers = curl_slist_append(headers, CONTENT_TYPE);
    headers = curl_slist_append(headers, ACCEPT_ENCODING);
    return headers;
}

void set_cainfo (CURL *curl) {
    #if defined(__linux__)
            curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");
    #endif
}

void print_json_err(const char *error_message, const char *response) {
    fprintf(stderr, "%s%s%s\n", CRED, error_message, CRESET);
    printf("%sServer's response%s: %s\n", CBLUE, CRESET, response);
}

size_t header_search(void *data, size_t size, size_t nmemb, void *userp) {
    size_t header_size = size * nmemb;
    char *header_line = strndup(data, header_size);

    // Example header key to check
    const char *key_to_find = "Content-Encoding";

    // Check if the header line contains the key "Content-Encoding"
    // If it does, the login successes.
    // NOTE: This fuction will be excuted for many times, round and around
    // to check if the desired content appears.
    if (strstr(header_line, key_to_find) != NULL) {
        printf("%sLogin success%s\n", CBLUE, CRESET);
    }

    free(header_line);
    return header_size;
}

char *get_executable_dir() {
    #if defined(_WIN32) || defined(_WIN64)
        static char path[MAX_PATH];
        if (GetModuleFileName(NULL, path, MAX_PATH) != 0) {
            char *dir = dirname(path);
            return dir;
        }
        return NULL;
    #elif defined(__APPLE__) || defined(__MACH__)
        static char path[PATH_MAX];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            char *dir = dirname(path);
            return dir;
        }
        return NULL;
    #else
        static char path[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (len != -1) {
            path[len] = '\0';
            char *dir = dirname(path);
            return dir;
        }
        return NULL;
    #endif
}

char *parse_yaml(const char *filename, const char *key_to_find) {
    int found = 0;
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }

    yaml_parser_t parser;
    yaml_event_t event;

    // Initialize parser
    if (!yaml_parser_initialize(&parser)) {
        fprintf(stderr, "Failed to initialize parser!\n");
        fclose(file);
        return NULL;
    }

    yaml_parser_set_input_file(&parser, file);

    char key[256] = {0}; // Buffer to hold the current key

    // Parsing loop
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            fprintf(stderr, "Parser error: %d\n", parser.error);
            yaml_parser_delete(&parser);
            fclose(file);
            return NULL;
        }
        switch (event.type) {
            case YAML_SCALAR_EVENT:
                if (key[0] == '\0') {
                    // This is the key
                    strncpy(key, (char *)event.data.scalar.value, sizeof(key) - 1);
                } else {
                    // This is the value
                    if (strcmp(key, key_to_find) == 0) {
                        // strncpy(phone, (char *)event.data.scalar.value, 20 - 1);
                        yaml_parser_delete(&parser);
                        fclose(file);
                        found ++;
                        return (char *)event.data.scalar.value;
                    }
                    // } else if (strcmp(key, "uid") == 0) {
                    //     strncpy(uid, (char *)event.data.scalar.value, 20 - 1);
                    // } else if (strcmp(key, "isp") == 0) {
                    //     strncpy(isp, (char *)event.data.scalar.value, 20 - 1);
                    // } else if (strcmp(key, "oauth_url") == 0) {
                    //     strncpy(oauth_url, (char *)event.data.scalar.value, 512 - 1);
                    // } else if (strcmp(key, "user_index") == 0) {
                    //     strncpy(user_index, (char *)event.data.scalar.value, 512 - 1);
                    // }
                    // Reset key after processing
                    key[0] = '\0';
                }
                break;

            case YAML_STREAM_END_EVENT:
                // End of the stream
                yaml_parser_delete(&parser);
                fclose(file);
                if (!found) return (char *)"\0";
                break;

            default:
                // Ignore other events
                break;
        }
        yaml_event_delete(&event);
    }
}


// Helper function to check if a key exists in the parsed data
// int key_exists(const char *filename, const char *key_to_check) {
//     char phone[20] = {0};
//     char uid[20] = {0};
//     char isp[20] = {0};
//     char oauth_url[512] = {0};
//     char user_index[512] = {0};

//     // Parse the YAML file and fill the corresponding variables
//     parse_yaml(filename, phone, uid, isp, oauth_url, user_index);

//     // Check if the key exists
//     if (strcmp(key_to_check, "phone") == 0 && phone[0] != '\0') return 1;
//     if (strcmp(key_to_check, "uid") == 0 && uid[0] != '\0') return 1;
//     if (strcmp(key_to_check, "isp") == 0 && isp[0] != '\0') return 1;
//     if (strcmp(key_to_check, "oauth_url") == 0 && oauth_url[0] != '\0') return 1;
//     if (strcmp(key_to_check, "user_index") == 0 && user_index[0] != '\0') return 1;

//     return 0; // Key does not exist
// }

// Function to append a key-value pair if the key is missing

void reformat(FILE *file, const char *phone, const char *uid, const char *isp){
    fseek(file, 0, SEEK_SET); // Go to the start of the file
    fprintf(file, "phone: %s\n", phone);
    fprintf(file, "uid: %s\n", uid);
    fprintf(file, "isp: %s\n", isp);
}

int append_yaml_if_missing(const char *filename, const char *key, const char *value) {
    char *phone;
    char *uid;
    char *isp;
    char *uuid;
    char *oauth_url;
    char *user_index;

    // Parse the YAML file and fill the corresponding variables
    phone = parse_yaml(filename, "phone");
    uid = parse_yaml(filename, "uid");
    isp = parse_yaml(filename, "isp");
    uuid = parse_yaml(filename, "uuid");
    oauth_url = parse_yaml(filename, "oauth_url");
    user_index = parse_yaml(filename, "user_index");

    FILE *file = fopen(filename, "r+");
    if (!file) {
        perror("Error opening file for appending");
        return 1;
    }

    if (strcmp(key, "uuid") == 0 && uuid[0] == '\0'){
        // Reformat file 
        reformat(file, phone, uid, isp);
        fprintf(file, "%s: %s\n", key, value);
        fprintf(file, "oauth_url: %s\n", oauth_url);
        fprintf(file, "user_index: %s\n", user_index);
        printf("Key '%s' appended to YAML file.\n", key);
    }

    // Check if the key exists
    if (strcmp(key, "oauth_url") == 0 && oauth_url[0] == '\0'){
        // Still not figured out how to automatically obtain.
    }

    if (strcmp(key, "user_index") == 0 && user_index[0] == '\0'){
        // Reformat file 
        reformat(file, phone, uid, isp);
        fprintf(file, "uuid: %s\n", uuid);
        fprintf(file, "oauth_url: %s\n", oauth_url);
        fprintf(file, "%s: %s\n", key, value);
        printf("Key '%s' appended to YAML file.\n", key);
    }

    fclose(file);

    return 0;
}
