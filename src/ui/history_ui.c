#include "history_ui.h"
#include <stdio.h>
#include <string.h>
#include <wininet.h>
#include "core/log.h"
#include "core/cache.h"
#include "network/protocol.h"
#include "render.h"

// Layout orientation: currently using VERTICAL_LEFT
#define HISTORY_LAYOUT_VERTICAL_LEFT 1

// Global panel height for vertical layout
static int g_panel_height = 600;

void history_ui_set_panel_height(int height) {
    if (height > 0) g_panel_height = height;
}

// Extract root domain from hostname (e.g., "sub.example.com" -> "example.com")
static void get_root_domain(const char *host, char *root_domain, size_t max_len) {
    if (!host || !root_domain) return;

    strncpy(root_domain, host, max_len - 1);
    root_domain[max_len - 1] = '\0';

    // Count dots to determine if this is a subdomain
    int dot_count = 0;
    for (const char *p = host; *p; p++) {
        if (*p == '.') dot_count++;
    }

    // If there are 2+ dots, it's likely a subdomain (sub.example.com)
    // Extract just the last two parts (example.com)
    if (dot_count >= 2) {
        // Find the last two dots
        const char *last_dot = strrchr(host, '.');
        if (last_dot) {
            const char *second_last_dot = NULL;
            for (const char *p = host; p < last_dot; p++) {
                if (*p == '.') second_last_dot = p;
            }
            if (second_last_dot) {
                strncpy(root_domain, second_last_dot + 1, max_len - 1);
                root_domain[max_len - 1] = '\0';
            }
        }
    }
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

    // Try favicon paths on multiple hosts
    const char *favicon_paths[] = {
        "/favicon.ico",
        "/favicon.png",
        NULL
    };

    // Build list of hosts to try (subdomain first, then root domain)
    char hosts_to_try[2][256];
    strncpy(hosts_to_try[0], host, sizeof(hosts_to_try[0]) - 1);
    hosts_to_try[0][sizeof(hosts_to_try[0]) - 1] = '\0';

    get_root_domain(host, hosts_to_try[1], sizeof(hosts_to_try[1]));

    // Try each host
    for (int h = 0; h < 2; h++) {
        // Skip if this host is the same as the previous one
        if (h > 0 && strcmp(hosts_to_try[h], hosts_to_try[h-1]) == 0) {
            continue;
        }

        // Try each favicon format on this host
        for (int i = 0; favicon_paths[i]; i++) {
            char favicon_url[2048];
            snprintf(favicon_url, sizeof(favicon_url), "%s://%s%s", scheme, hosts_to_try[h], favicon_paths[i]);

            // Try cache first
            size_t cached_size = 0;
            void *cached_data = cache_get_image(favicon_url, &cached_size);
            if (cached_data && cached_size > 0) {
                history_node_set_favicon(node, cached_data, cached_size);
                return;  // Success - stop trying other hosts/formats
            }

            // Fetch from network
            network_response_t *res = network_fetch(favicon_url);
            if (res && res->data && res->size > 0 && res->status_code == 200) {
                // Cache for later
                cache_put_image(favicon_url, res->data, res->size);
                history_node_set_favicon(node, res->data, res->size);
                res->data = NULL;
                if (res) network_response_free(res);
                return;  // Success - stop trying other hosts/formats
            }
            if (res) network_response_free(res);
        }
    }
}

// Calculate the width needed for a node and all its descendants
static int calculate_node_width(history_node_t *node) {
    if (!node) return 24;

    int iconSize = 16;
    int spacing = 24;  // Horizontal spacing between siblings

    if (node->children_count == 0) {
        // Leaf node
        return iconSize;
    } else if (node->children_count == 1) {
        // Single child - width is same as child's width
        return calculate_node_width(node->children[0]);
    } else {
        // Multiple children - sum their widths plus spacing
        int total_width = 0;
        for (int i = 0; i < node->children_count; i++) {
            total_width += calculate_node_width(node->children[i]);
            if (i < node->children_count - 1) {
                total_width += spacing;  // Space between siblings
            }
        }
        return total_width;
    }
}

// Position nodes in a grid layout
// Returns the next available x position
typedef struct {
    int x;
    int y;
} node_pos_t;

static int layout_node_positions(history_node_t *node, node_pos_t *positions, int node_count,
                                 int start_x, int start_y, int *pos_index) {
    if (!node || *pos_index >= node_count) return start_x;

    int iconSize = 16;
    int spacing = 24;

    int node_idx = *pos_index;
    positions[node_idx].x = start_x;
    positions[node_idx].y = start_y;
    (*pos_index)++;

    int current_x = start_x;

    if (node->children_count > 1) {
        // Multiple children laid out horizontally below
        // First, calculate the total width needed
        int total_width = 0;
        for (int i = 0; i < node->children_count; i++) {
            total_width += calculate_node_width(node->children[i]);
            if (i < node->children_count - 1) {
                total_width += spacing;
            }
        }

        // Center this subtree under the parent
        int child_start_x = start_x - (total_width / 2);

        // Layout each child
        for (int i = 0; i < node->children_count; i++) {
            int child_width = calculate_node_width(node->children[i]);
            current_x = layout_node_positions(node->children[i], positions, node_count,
                                             child_start_x, start_y - spacing, pos_index);
            child_start_x = current_x + spacing;
        }
    } else if (node->children_count == 1) {
        // Single child goes straight down, same x position
        current_x = layout_node_positions(node->children[0], positions, node_count,
                                         start_x, start_y - spacing, pos_index);
    }

    return current_x;
}

// Get position index of a node
static int find_node_position(history_node_t *root, history_node_t *target, int *index) {
    if (!root) return -1;

    static int current_index = 0;
    if (root == target) {
        int result = current_index;
        current_index = 0;
        return result;
    }

    current_index++;

    for (int i = 0; i < root->children_count; i++) {
        int result = find_node_position(root->children[i], target, index);
        if (result >= 0) return result;
    }

    current_index--;
    return -1;
}

// Count total nodes in tree
static int count_tree_nodes(history_node_t *node) {
    if (!node) return 0;
    int count = 1;
    for (int i = 0; i < node->children_count; i++) {
        count += count_tree_nodes(node->children[i]);
    }
    return count;
}

// Draw lines between nodes (drawn first, so they appear behind icons)
static void draw_connections(HDC hdc, history_node_t *node, node_pos_t *positions, int node_count,
                             int base_x) {
    if (!node) return;

    int iconSize = 16;
    int spacing = 24;

    // Find this node's position
    int my_idx = -1;
    for (int i = 0; i < node_count; i++) {
        // We'll use a simpler approach - just iterate through tree to find node
        // For now, we'll draw connections as we go
    }

    if (node->children_count > 0) {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(128, 128, 128));
        HPEN oldPen = SelectObject(hdc, hPen);

        // Find this node's position in our positions array
        // This is inefficient but works for now
        for (int n = 0; n < node_count; n++) {
            if (positions[n].x == -1 && positions[n].y == -1) continue;

            if (node->children_count == 1) {
                // Single child - vertical line straight down
                int child_y = positions[n].y - spacing;
                MoveToEx(hdc, base_x + positions[n].x + iconSize/2, positions[n].y + iconSize, NULL);
                LineTo(hdc, base_x + positions[n].x + iconSize/2, child_y + iconSize/2);
            } else if (node->children_count > 1) {
                // Multiple children - draw lines to each
                for (int i = 0; i < node->children_count; i++) {
                    // Draw line from parent to child area
                    int child_y = positions[n].y - spacing;
                    MoveToEx(hdc, base_x + positions[n].x + iconSize/2, positions[n].y + iconSize, NULL);
                    LineTo(hdc, base_x + positions[n].x + iconSize/2, child_y + iconSize/2);
                }
            }
        }

        SelectObject(hdc, oldPen);
        DeleteObject(hPen);
    }

    // Recursively draw connections for children
    for (int i = 0; i < node->children_count; i++) {
        draw_connections(hdc, node->children[i], positions, node_count, base_x);
    }
}

// Forward declaration
static void draw_nodes_at_positions(HDC hdc, history_node_t *node, node_pos_t *positions, int node_count,
                                    history_node_t *current_node, int *pos_index, int base_x);

// Draw the history tree as a proper grid layout
static void draw_history_tree_grid(HDC hdc, history_tree_t *tree) {
    if (!tree || !tree->root) return;

    int iconSize = 16;
    int base_x = 10;

    // Count nodes
    int node_count = count_tree_nodes(tree->root);
    if (node_count <= 0) return;

    // Allocate position array
    node_pos_t *positions = malloc(sizeof(node_pos_t) * node_count);
    for (int i = 0; i < node_count; i++) {
        positions[i].x = -1;
        positions[i].y = -1;
    }

    // Layout nodes
    int pos_index = 0;
    layout_node_positions(tree->root, positions, node_count, 0, g_panel_height - 10, &pos_index);

    // Draw connection lines first (so they appear behind icons)
    draw_connections(hdc, tree->root, positions, node_count, base_x);

    // Draw icons and borders second
    pos_index = 0;
    draw_nodes_at_positions(hdc, tree->root, positions, node_count, tree->current, &pos_index, base_x);

    free(positions);
}

// Helper function to draw nodes at their calculated positions
static void draw_nodes_at_positions(HDC hdc, history_node_t *node, node_pos_t *positions, int node_count,
                                    history_node_t *current_node, int *pos_index, int base_x) {
    if (!node || *pos_index >= node_count) return;

    int iconSize = 16;
    int my_pos = *pos_index;
    (*pos_index)++;

    int x = base_x + positions[my_pos].x;
    int y = positions[my_pos].y;

    // Draw favicon
    if (node->favicon_data) {
        render_image_data(hdc, node->favicon_data, node->favicon_size, x, y, iconSize, iconSize);
    } else {
        HBRUSH hBrush = CreateSolidBrush(RGB(100, 150, 255));
        HBRUSH oldBrush = SelectObject(hdc, hBrush);
        Ellipse(hdc, x, y, x + iconSize, y + iconSize);
        SelectObject(hdc, oldBrush);
        DeleteObject(hBrush);
    }

    // Draw red border for current node
    if (node == current_node) {
        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
        HPEN oldPen = SelectObject(hdc, hPen);
        HBRUSH oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, x - 2, y - 2, x + iconSize + 2, y + iconSize + 2);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(hPen);
    }

    // Draw children
    for (int i = 0; i < node->children_count; i++) {
        draw_nodes_at_positions(hdc, node->children[i], positions, node_count, current_node, pos_index, base_x);
    }
}

// Hit test against grid positions
static history_node_t* hit_test_grid(history_node_t *node, node_pos_t *positions, int node_count,
                                     int *pos_index, int base_x, int hitX, int hitY) {
    if (!node || *pos_index >= node_count) return NULL;

    int iconSize = 16;
    int my_pos = *pos_index;
    (*pos_index)++;

    int x = base_x + positions[my_pos].x;
    int y = positions[my_pos].y;

    // Check if hit this node
    if (hitX >= x && hitX <= x + iconSize && hitY >= y && hitY <= y + iconSize) {
        return node;
    }

    // Check children
    for (int i = 0; i < node->children_count; i++) {
        history_node_t *res = hit_test_grid(node->children[i], positions, node_count,
                                           pos_index, base_x, hitX, hitY);
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

// Public interface: Draw history tree using grid layout
void history_ui_draw(HDC hdc, history_tree_t *tree) {
    if (!tree || !tree->root) return;

    draw_history_tree_grid(hdc, tree);
}

// Public interface: Hit test history tree using grid layout
history_node_t* history_ui_hit_test(history_tree_t *tree, int hitX, int hitY) {
    if (!tree || !tree->root) return NULL;

    int node_count = count_tree_nodes(tree->root);
    if (node_count <= 0) return NULL;

    // Allocate position array
    node_pos_t *positions = malloc(sizeof(node_pos_t) * node_count);
    for (int i = 0; i < node_count; i++) {
        positions[i].x = -1;
        positions[i].y = -1;
    }

    // Layout nodes
    int pos_index = 0;
    layout_node_positions(tree->root, positions, node_count, 0, g_panel_height - 10, &pos_index);

    // Hit test
    pos_index = 0;
    history_node_t *result = hit_test_grid(tree->root, positions, node_count, &pos_index, 10, hitX, hitY);

    free(positions);
    return result;
}
