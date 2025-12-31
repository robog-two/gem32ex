#include "history_ui.h"
#include <stdio.h>
#include <string.h>
#include <wininet.h>
#include "core/log.h"
#include "network/protocol.h"
#include "render.h"

// Layout orientation: currently using VERTICAL_LEFT
#define HISTORY_LAYOUT_VERTICAL_LEFT 1

// Global panel height for vertical layout
static int g_panel_height = 600;

void history_ui_set_panel_height(int height) {
    if (height > 0) g_panel_height = height;
}

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

// Count total depth of tree for layout calculation
static int count_tree_depth(history_node_t *node) {
    if (!node) return 0;
    int max_depth = 0;
    for (int i = 0; i < node->children_count; i++) {
        int child_depth = count_tree_depth(node->children[i]);
        if (child_depth > max_depth) max_depth = child_depth;
    }
    return max_depth + 1;
}

// Draw the history tree (vertical layout on left, items bottom-to-top)
// y_pos tracks current vertical position; items appear from bottom up
static void draw_history_tree_vertical(HDC hdc, history_node_t *node, history_node_t *current_node,
                                       int x, int *y_pos, int panel_height, int depth) {
    if (!node) return;

    int iconSize = 16;
    int spacing = 24;  // Vertical spacing between items

    // Draw current node
    int curY = *y_pos;

    // Don't draw if off-screen
    if (curY >= 0 && curY <= panel_height) {
        if (node->favicon_data) {
            render_image_data(hdc, node->favicon_data, node->favicon_size, x, curY, iconSize, iconSize);
        } else {
            // Draw a globe-ish circle for Gemini or missing icons
            HBRUSH hBrush = CreateSolidBrush(RGB(100, 150, 255));
            HBRUSH oldBrush = SelectObject(hdc, hBrush);
            Ellipse(hdc, x, curY, x + iconSize, curY + iconSize);
            SelectObject(hdc, oldBrush);
            DeleteObject(hBrush);
        }

        // Draw red border around current node
        if (node == current_node) {
            HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
            HPEN oldPen = SelectObject(hdc, hPen);
            HBRUSH oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, x - 2, curY - 2, x + iconSize + 2, curY + iconSize + 2);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(hPen);
        }
    }

    *y_pos -= spacing;  // Move up for next item

    // Draw children at increased horizontal offset (indentation)
    for (int i = 0; i < node->children_count; i++) {
        draw_history_tree_vertical(hdc, node->children[i], current_node,
                                  x + 24, y_pos, panel_height, depth + 1);
    }
}

// Hit test for history tree (vertical layout on left)
static history_node_t* hit_test_vertical(history_node_t *node, int x, int *y_pos,
                                         int panel_height, int hitX, int hitY) {
    if (!node) return NULL;

    int iconSize = 16;
    int spacing = 24;
    int curY = *y_pos;

    if (hitX >= x && hitX <= x + iconSize && hitY >= curY && hitY <= curY + iconSize) {
        return node;
    }

    *y_pos -= spacing;

    for (int i = 0; i < node->children_count; i++) {
        history_node_t *res = hit_test_vertical(node->children[i], x + 24, y_pos,
                                               panel_height, hitX, hitY);
        if (res) return res;
    }
    return NULL;
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
// Uses vertical layout with items appearing from bottom-to-top on the left
void history_ui_draw(HDC hdc, history_tree_t *tree) {
    if (!tree || !tree->root) return;

    #ifdef HISTORY_LAYOUT_VERTICAL_LEFT
    int y_pos = g_panel_height - 10;  // Start from bottom
    draw_history_tree_vertical(hdc, tree->root, tree->current, 10, &y_pos, g_panel_height, 0);
    #else
    int x = 10;
    draw_history_tree_horizontal(hdc, tree->root, &x, 10);
    #endif
}

// Public interface: Hit test history tree
// Uses vertical layout with items appearing from bottom-to-top on the left
history_node_t* history_ui_hit_test(history_tree_t *tree, int hitX, int hitY) {
    if (!tree || !tree->root) return NULL;

    #ifdef HISTORY_LAYOUT_VERTICAL_LEFT
    int y_pos = g_panel_height - 10;  // Start from bottom
    return hit_test_vertical(tree->root, 10, &y_pos, g_panel_height, hitX, hitY);
    #else
    int x = 10;
    return hit_test_horizontal(tree->root, &x, 10, hitX, hitY);
    #endif
}
