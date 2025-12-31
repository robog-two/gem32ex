#include "protocol.h"
#include "http.h"
#include "gemini.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Mock for now, in a real implementation this would show a dialog
static char* platform_input_prompt(const char *prompt) {
    // Return NULL or a mock string to test logic
    return NULL; 
}

network_response_t* network_fetch(const char *url) {
    int redirect_count = 0;
    const int max_redirects = 5;
    char current_url[2048];
    strncpy(current_url, url, sizeof(current_url)-1);
    current_url[sizeof(current_url)-1] = '\0';

    while (redirect_count < max_redirects) {
        network_response_t *res = NULL;
        if (strncmp(current_url, "gemini://", 9) == 0) {
            res = gemini_fetch(current_url);
        } else {
            res = http_fetch(current_url);
        }

        if (!res) return NULL;

        // Handle Gemini Input (1x)
        if (res->status_code >= 10 && res->status_code <= 19) {
            char *user_input = platform_input_prompt(res->content_type ? res->content_type : "Input required:");
            if (user_input) {
                char new_url[2048];
                snprintf(new_url, sizeof(new_url), "%s?%s", current_url, user_input);
                free(user_input);
                network_response_free(res);
                strncpy(current_url, new_url, sizeof(current_url)-1);
                redirect_count++;
                continue;
            }
        }

        // Handle Redirects
        // HTTP: 301, 302, 303, 307, 308
        // Gemini: 30, 31
        int is_redirect = 0;
        if (res->status_code >= 300 && res->status_code <= 308) is_redirect = 1;
        if (res->status_code == 30 || res->status_code == 31) is_redirect = 2; // Gemini type

        if (is_redirect) {
            const char *new_url = NULL;
            if (is_redirect == 1 && res->data) new_url = res->data;
            else if (is_redirect == 2) new_url = res->content_type;

            if (new_url && strlen(new_url) > 0 && strlen(new_url) < 2048) {
                strncpy(current_url, new_url, sizeof(current_url)-1);
                current_url[sizeof(current_url)-1] = '\0';
                network_response_free(res);
                redirect_count++;
                continue;
            }
        }

        if (!res->final_url) res->final_url = strdup(current_url);
        return res;
    }

    return NULL;
}

network_response_t* network_post(const char *url, const char *body, const char *content_type) {
    if (strncmp(url, "gemini://", 9) == 0) {
        return NULL; // Gemini doesn't have POST
    }
    network_response_t *res = http_post(url, body, content_type);
    if (res) {
        // Handle post-redirect (PRG pattern)
        if (res->status_code >= 301 && res->status_code <= 308) {
            char *new_url = res->data; // http_post puts Location in data for redirects
            if (new_url && strlen(new_url) > 0) {
                char redirect_target[2048];
                strncpy(redirect_target, new_url, sizeof(redirect_target)-1);
                redirect_target[sizeof(redirect_target)-1] = '\0';
                network_response_free(res);
                return network_fetch(redirect_target);
            }
        }
        if (!res->final_url) res->final_url = strdup(url);
    }
    return res;
}
