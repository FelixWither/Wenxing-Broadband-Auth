#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <libgen.h>
#include <yaml.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#define BASE_URL "https://api.215123.cn/ac"
#define LOGIN_PATH "/auth/loginByPhoneAndUid"
#define GET_QRCODE "/auth/qrCodeLogin"
#define REDIRECT_PATH "/auth/oauthRedirect"
#define CONFIG_FILE "config.yaml"
#define BASE_POSTLOGIN_URL "http://10.10.16.101:8080/eportal/InterFace.do"
#define LOGOUT_PATH "?method=logout"

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#elif defined(__APPLE__) || defined(__MACH__)
    #include <mach-o/dyld.h>
#endif

// Should be read from config files, not hard coded.
char *phone;
char *uid;
char *isp;
char *oauth_url;
char *final_url = NULL;
char *user_index;

static size_t write_callback(void *data, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct memory *mem = (struct memory *)userp;

    char *ptr = realloc(mem->response, mem->size + realsize + 1);
    if(ptr == NULL)
        return 0;

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), data, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;

    return realsize;
}

static char *get_uuid() {
    printf("%sGetting uuid.%s\n", CBLUE, CRESET);
    CURL *curl;
    CURLcode res;
    struct memory chunk = {0};
    char *uuid = NULL;

    curl = curl_easy_init();

    if(curl) {
        char url[512];
        snprintf(url, sizeof(url), "%s%s?clientId=%s", BASE_URL, GET_QRCODE, CLIENT_ID);
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, set_common_headers());

        res = curl_easy_perform(curl);

        if(res == CURLE_OK) {
            cJSON *json = cJSON_Parse(chunk.response);
            if (json != NULL) {
                cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
                if (cJSON_IsObject(data)) {
                    cJSON *uuid_item = cJSON_GetObjectItemCaseSensitive(data, "uuid");
                    if (cJSON_IsString(uuid_item) && (uuid_item->valuestring != NULL)) {
                        uuid = strdup(uuid_item->valuestring);
                        printf("%sGot uuid%s: %s\n", CBLUE, CRESET, uuid);
                    } else {
                        print_json_err("No uuid provided by server.", chunk.response);
                    }
                } else {
                    print_json_err("No data section provided by server", chunk.response);
                }
                cJSON_Delete(json);
            } else {
                print_json_err("No valid JSON response for oauth request.", chunk.response);
            }
        } else {
            fprintf(stderr, "%sRequest failed%s: %s\n", CRED, CRESET, curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
    }

    if(chunk.response) {
        free(chunk.response);
    }

    return uuid;
}

static char *login(const char *uuid) {
    printf("%sObtaining token.%s\n", CBLUE, CRESET);
    CURL *curl;
    CURLcode res;
    struct memory chunk = {0};
    char *token = NULL;

    curl = curl_easy_init();

    if(curl) {
        char url[512];
        char payload[512];
        snprintf(url, sizeof(url), "%s%s", BASE_URL, LOGIN_PATH);
        snprintf(payload, sizeof(payload), "{\"phone\":\"%s\",\"uid\":\"%s\",\"captchaKey\":\"%s\"}", phone, uid, uuid);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, set_common_headers());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        res = curl_easy_perform(curl);

        if(res == CURLE_OK) {
            cJSON *json = cJSON_Parse(chunk.response);
            if (json != NULL) {
                cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
                if (cJSON_IsObject(data)) {
                    cJSON *token_item = cJSON_GetObjectItemCaseSensitive(data, "token");
                    if (cJSON_IsString(token_item) && (token_item->valuestring != NULL)) {
                        token = strdup(token_item->valuestring);
                        printf("%sToken obtained%s: %s\n", CBLUE, CRESET, token);
                    } else {
                        print_json_err("No token provided by server.", chunk.response);
                    }
                } else {
                    print_json_err("No data section provided by server.", chunk.response);
                }
                cJSON_Delete(json);
            } else {
                print_json_err("No valid JSON response for oauth request.", chunk.response);
            }
        } else {
            fprintf(stderr, "%sLogin failed%s: %s\n", CRED, CRESET, curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
    }

    if (chunk.response) {
        free(chunk.response);
    }

    return token;
}

static char *oauth(const char *token, const char *client_id, const char *redirect_start_uri, const char *isp_name) {
    printf("%sObtaining login key (code).%s\n", CBLUE, CRESET);
    CURL *curl;
    CURLcode res;
    struct memory chunk = {0};
    char *login_key = NULL;

    curl = curl_easy_init();

    if(curl) {
        char url[1024];
        snprintf(url, sizeof(url), "%s%s?response_type=code&client_id=%s&redirect_uri=%s&serviceName=%s",
                 BASE_URL, REDIRECT_PATH, client_id, redirect_start_uri, isp_name);

        struct curl_slist *headers = NULL;
        headers = set_common_headers();
        char satoken[256];
        snprintf(satoken, sizeof(satoken), "satoken: %s", token);
        headers = curl_slist_append(headers, satoken);
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        res = curl_easy_perform(curl);

        if(res == CURLE_OK) {
            cJSON *json = cJSON_Parse(chunk.response);
            if (json != NULL) {
                cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "data");
                if (cJSON_IsString(data) && (data->valuestring != NULL)) {
                    char *parsed_url = data->valuestring;
                    char *query = strchr(parsed_url, '?');
                    if (query) {
                        query++;
                        char *token = strstr(query, "code=");
                        if (token) {
                            token += 5;
                            login_key = strdup(token);
                            char *end = strchr(login_key, '&');
                            if (end) *end = '\0';
                            printf("%slogin key (code) obtained%s: %s\n", CBLUE, CRESET, login_key);
                        } else {
                            print_json_err("No login key (code) provided by server.", chunk.response);
                        }
                    }
                } else {
                    print_json_err("No data section provided by server.", chunk.response);
                }
                cJSON_Delete(json);
            } else {
                print_json_err("No valid JSON response for oauth request.", chunk.response);
            }
        } else {
            fprintf(stderr, "%sOAuth failed%s: %s\n", CRED, CRESET, curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
    }

    if(chunk.response) {
        free(chunk.response);
    }

    return login_key;
}

static void final_step(const char *login_key, const char *isp_name, const char *user_index, const char *file_name) {
    printf("%sFinishing final authentication.%s\n", CBLUE, CRESET);
    CURL *curl;
    CURLcode res;
    struct memory chunk = {0};

    curl = curl_easy_init();

    if(curl) {
        char url[1024];
        snprintf(url, sizeof(url), "%s&code=%s&serviceName=%s", oauth_url, login_key, isp_name);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, set_common_headers());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        // Allow curl to follow redirects
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        // Assign header search function to curl
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_search);
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        res = curl_easy_perform(curl);

        if(res == CURLE_OK) {
            res = curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &final_url);
            if (final_url){
                char *query = strchr(final_url, '?');
                    if (query) {
                        query++;
                        char *uindex = strstr(query, "userIndex=");
                        if (uindex) {
                            uindex += 10;
                            user_index = strdup(uindex);
                            char *end = strchr(user_index, '&');
                            if (end) *end = '\0';
                            printf("%sUser index obtained%s: %s\n", CBLUE, CRESET, user_index);
                            append_yaml_if_missing(file_name, "user_index", user_index);
                        } else {
                            print_json_err("No user index provided by server.", chunk.response);
                        }
                    }
            } else {
                fprintf(stderr, "%sNo redirect url.%s\n", CRED, CRESET);
            }
            
        } else {
            fprintf(stderr, "%sAuthentication failed%s: %s\n", CRED, CRESET, curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
    }

    if(chunk.response) {
        free(chunk.response);
    }
}

static int logout(const char *user_index){
    printf("%sLogging out.%s\n", CBLUE, CRESET);
    CURL *curl;
    CURLcode res;
    struct memory chunk = {0};
    
    curl = curl_easy_init();

    if (curl){
        char url[256];
        char payload [512];
        snprintf(url, sizeof(url), "%s%s", BASE_POSTLOGIN_URL, LOGOUT_PATH);
        snprintf(payload, sizeof(payload), "{\"userIndex\":\"%s\"}", user_index);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, set_common_headers());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        res = curl_easy_perform(curl);

        if(res == CURLE_OK) {
            cJSON *json = cJSON_Parse(chunk.response);
            if (json != NULL) {
                cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "result");
                char *result;
                if (cJSON_IsString(data) && (data->valuestring != NULL)) {
                    result = strdup(data->valuestring);
                    printf("%sLogout%s: %s\n", CBLUE, CRESET, result);
                } else {
                    print_json_err("No result provided by server.", chunk.response);
                    return 1;
                }
                cJSON_Delete(json);
            } else {
                print_json_err("No valid JSON response for oauth request.", chunk.response);
                return 1;
            }
        } else {
            fprintf(stderr, "%sLogout failed%s: %s\n", CRED, CRESET, curl_easy_strerror(res));
            return 1;
        }
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    if(chunk.response) {
        free(chunk.response);
    }
    return 0;
}

static void free_vars(){
    free(phone);
    free(uid);
    free(isp);
    free(oauth_url);
    free(user_index);
}

int main(int argc, char *argv[]) {
    char config_path[PATH_MAX] = "";
    curl_global_init(CURL_GLOBAL_DEFAULT);
    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        // Case handling for options
        if (argv[i][0] == '-') {  // Check if it's an option
            switch (argv[i][1]) {  // Check the first character after '-'
                case 'c':
                    if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
                        snprintf(config_path, sizeof(config_path), "%s", argv[++i]);
                    }
                    break;
                default:
                    fprintf(stderr, "Unknown option: %s\n", argv[i]);
                    break;
            }
        } else {
            fprintf(stderr, "Unexpected argument: %s\n", argv[i]);
        }

        // Case handling for command
        if (strcmp(argv[i], "logout") == 0){
            int res;
            res = logout(user_index);
            free(user_index);
            exit(res);
        }
    }
    
    if (config_path[0] == '\0') {
        char *exec_dir = get_executable_dir();
        if (!exec_dir) {
            fprintf(stderr, "Failed to get the directory of the executable.\n");
            return EXIT_FAILURE;
        }
        snprintf(config_path, sizeof(config_path), "%s/%s", exec_dir, CONFIG_FILE);
    }

    // if (parse_yaml(config_path, phone, uid, isp, oauth_url, user_index) != 0) {
    //     fprintf(stderr, "Failed to parse YAML file\n");
    //     return 1;
    // }
    phone = parse_yaml(config_path, "phone");
    if (phone[0] == '\0') { printf("No phone number filled."); return 1; }
    uid = parse_yaml(config_path, "uid");
    if (uid[0] == '\0') { printf("No uid filled."); return 1; }
    isp = parse_yaml(config_path, "isp");
    if (isp[0] == '\0') { printf("No ISP specified."); return 1; }
    oauth_url = parse_yaml(config_path, "oauth_url");
    if (oauth_url[0] == '\0') { printf("No oauth url found."); return 1; }
    user_index = parse_yaml(config_path, "user_index");
    if (user_index[0] == '\0') { printf("No user index found."); return 1; }

    char *uuid = get_uuid();
    if (uuid == NULL) {
        printf("Failed to get uuid, check details above.");
        free_vars();
        curl_global_cleanup();
        return 1;
    }

    char *token = login(uuid);
    if (token == NULL) {
        printf("Failed to get satoken, check details above.");
        free(uuid);
        free_vars();
        curl_global_cleanup();
        return 1;
    }

    char *code = oauth(token, CLIENT_ID, oauth_url, isp);
    if (code == NULL) {
        printf("Failed to get login code, check details above.");
        free(uuid);
        free(token);
        free_vars();
        curl_global_cleanup();
        return 1;
    }

    final_step(code, isp, user_index, config_path);
    free(code);
    free(uuid);
    free(token);
    free_vars();
    curl_global_cleanup();
    return 0;
}
