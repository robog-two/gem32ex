#include "history_ui.h"
#include <stdio.h>
#include <string.h>
#include <wininet.h>
#include "core/log.h"
#include "network/protocol.h"
#include "render.h"

// Fetch favicon for a history node
void history_ui_fetch_favicon(history_node_t *node) {
    if (!node || !node->url) return;

    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    char host[256] = {0};
    urlComp.lpszHostName = host;
    urlComp.dwHostNameLength = sizeof(host);
    urlComp.dwSchemeLength = 1;

    if (!InternetCrackUrl(node->url, 0, 0, &urlComp)) return;

    if (strncmp(node->url, "gemini://", 9) == 0) return;

    const char *scheme = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? "https" : "http";

    // Try multiple favicon formats
    const char *favicon_paths[] = {
        "/favicon.ico",     // Classic ICO format (widely supported)
        "/favicon.png",     // Modern PNG format
        NULL
    };

    for (int i = 0; favicon_paths[i]; i++) {
        char favicon_url[2048];
        snprintf(favicon_url, sizeof(favicon_url), "%s://%s%s", scheme, host, favicon_paths[i]);

        network_response_t *res = network_fetch(favicon_url);
        if (res && res->data && res->size > 0 && res->status_code == 200) {
            history_node_set_favicon(node, res->data, res->size);
            res->data = NULL;
            if (res) network_response_free(res);
            return;  // Success - stop trying other formats
        }
        if (res) network_response_free(res);
    }
}

// Draw the history tree (horizontal layout, left-to-right)
static void draw_history_tree_horizontal(HDC hdc, history_node_t *node, int *x, int y) {
    if (!node) return;

    int iconSize = 16;
    int spacing = 24;
    int curX = *x;

    if (node->favicon_data) {
        render_image_data(hdc, node->favicon_data, node->favicon_size, curX, y, iconSize, iconSize);
    } else {
        // Draw a globe-ish circle for Gemini or missing icons
        HBRUSH hBrush = CreateSolidBrush(RGB(100, 150, 255));
        HBRUSH oldBrush = SelectObject(hdc, hBrush);
        Ellipse(hdc, curX, y, curX + iconSize, y + iconSize);
        SelectObject(hdc, oldBrush);
        DeleteObject(hBrush);
    }

    *x += spacing;

    for (int i = 0; i < node->children_count; i++) {
        draw_history_tree_horizontal(hdc, node->children[i], x, y);
    }
}

// Hit test for history tree (horizontal layout)
static history_node_t* hit_test_horizontal(history_node_t *node, int *x, int y, int hitX, int hitY) {
    if (!node) return NULL;
    int iconSize = 16;
    int spacing = 24;
    int curX = *x;

    if (hitX >= curX && hitX <= curX + iconSize && hitY >= y && hitY <= y + iconSize) {
        return node;
    }

    *x += spacing;

    for (int i = 0; i < node->children_count; i++) {
        history_node_t *res = hit_test_horizontal(node->children[i], x, y, hitX, hitY);
        if (res) return res;
    }
    return NULL;
}

// Public interface: Draw history tree
void history_ui_draw(HDC hdc, history_tree_t *tree) {
    if (!tree || !tree->root) return;
    int x = 10;
    draw_history_tree_horizontal(hdc, tree->root, &x, 10);
}

// Public interface: Hit test history tree
history_node_t* history_ui_hit_test(history_tree_t *tree, int hitX, int hitY) {
    if (!tree || !tree->root) return NULL;
    int x = 10;
    return hit_test_horizontal(tree->root, &x, 10, hitX, hitY);
}
