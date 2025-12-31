#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "core/dom.h"
#include "core/html.h"
#include "core/style.h"
#include "core/layout.h"
#include "core/platform.h"
#include "core/log.h"
#include "test_ui.h"

// Note: platform_measure_text is now provided by src/ui/render.c (real Win32 implementation)

// Global needed by render.c for focus tracking (not used in tests)
node_t *g_focused_node = NULL;

static int test_dom_impl() {
    const char *html = "<html><body><h1>Title</h1></body></html>";
    node_t *dom = html_parse(html);
    if (!dom) { LOG_ERROR("html_parse failed"); return 0; }
    node_t *h1 = dom->first_child->first_child;
    if (!h1 || strcmp(h1->tag_name, "h1") != 0) {
        LOG_ERROR("DOM tree structure unexpected");
        node_free(dom);
        return 0;
    }
    LOG_INFO("DOM parsed successfully with h1 node");
    node_free(dom);
    return 1;
}

static int test_style_impl() {
    const char *html = "<html><body><h1>Title</h1></body></html>";
    node_t *dom = html_parse(html);
    if (!dom) return 0;
    style_compute(dom);
    node_t *h1 = dom->first_child->first_child;
    if (!h1 || h1->style->font_size != 32) {
        LOG_ERROR("Style computation failed for h1 font-size");
        node_free(dom);
        return 0;
    }
    LOG_INFO("Style computation successful (h1 font-size=32)");
    node_free(dom);
    return 1;
}

static int test_layout_impl() {
    const char *html = "<html><body><div></div></body></html>";
    node_t *dom = html_parse(html);
    if (!dom) return 0;
    style_compute(dom);
    layout_box_t *layout = layout_create_tree(dom, 800);
    if (!layout || layout->fragment.border_box.width != 800) {
        LOG_ERROR("Layout width mismatch");
        if (layout) layout_free(layout);
        node_free(dom);
        return 0;
    }
    LOG_INFO("Layout tree created with correct root width");
    layout_free(layout);
    node_free(dom);
    return 1;
}

typedef struct {
    const char *selector;  // tag name for simple matching
    int expected_width;
    int expected_height;
} layout_expectation_t;

static int test_layout_accuracy_impl() {
    // Firefox reference sizes for testdocument.html at 863px viewport width
    layout_expectation_t expectations[] = {
        {"html", 863, 624},
        {"body", 847, 526},
        {"h1", 847, 58},
        {"p", 847, 134},
        {"h3", 847, 20},
        {"form", 847, 44},
        {NULL, 0, 0}  // sentinel
    };

    // Read test document
    FILE *f = fopen("tests/res/testdocument.html", "r");
    if (!f) {
        LOG_ERROR("Failed to open testdocument.html");
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *html = malloc(fsize + 1);
    if (!html) {
        fclose(f);
        LOG_ERROR("Failed to allocate memory for HTML");
        return 0;
    }

    fread(html, 1, fsize, f);
    html[fsize] = '\0';
    fclose(f);

    // Parse and layout
    node_t *dom = html_parse(html);
    free(html);

    if (!dom) {
        LOG_ERROR("HTML parsing failed");
        return 0;
    }

    style_compute(dom);
    layout_box_t *layout = layout_create_tree(dom, 863);  // Firefox viewport width

    if (!layout) {
        LOG_ERROR("Layout creation failed");
        node_free(dom);
        return 0;
    }

    // Check layout accuracy
    int passed = 1;
    int total_checks = 0;
    int passed_checks = 0;

    for (int i = 0; expectations[i].selector != NULL; i++) {
        // Find element by tag name (simplified selector matching)
        layout_box_t *box = NULL;

        // Traverse layout tree to find matching element
        layout_box_t *queue[256];
        int queue_head = 0, queue_tail = 0;
        queue[queue_tail++] = layout;

        while (queue_head < queue_tail && !box) {
            layout_box_t *current = queue[queue_head++];

            if (current->node && current->node->tag_name &&
                strcmp(current->node->tag_name, expectations[i].selector) == 0) {
                box = current;
                break;
            }

            for (layout_box_t *child = current->first_child; child; child = child->next_sibling) {
                if (queue_tail < 256) {
                    queue[queue_tail++] = child;
                }
            }
        }

        if (!box) {
            LOG_ERROR("Element <%s> not found in layout tree", expectations[i].selector);
            passed = 0;
            total_checks++;
            continue;
        }

        int actual_width = box->fragment.border_box.width;
        int actual_height = box->fragment.border_box.height;
        int expected_width = expectations[i].expected_width;
        int expected_height = expectations[i].expected_height;

        // Allow 5% tolerance for layout differences
        int width_diff = actual_width > expected_width ?
                        actual_width - expected_width : expected_width - actual_width;
        int height_diff = actual_height > expected_height ?
                         actual_height - expected_height : expected_height - actual_height;

        int width_tolerance = expected_width * 5 / 100;
        int height_tolerance = expected_height * 5 / 100;

        total_checks++;
        if (width_diff <= width_tolerance && height_diff <= height_tolerance) {
            LOG_INFO("<%s> size OK: %dx%d (expected %dx%d)",
                    expectations[i].selector, actual_width, actual_height,
                    expected_width, expected_height);
            passed_checks++;
        } else {
            LOG_ERROR("<%s> size MISMATCH: %dx%d (expected %dx%d, diff: %dx%d)",
                     expectations[i].selector, actual_width, actual_height,
                     expected_width, expected_height, width_diff, height_diff);
            passed = 0;
        }
    }

    LOG_INFO("Layout accuracy: %d/%d checks passed", passed_checks, total_checks);

    layout_free(layout);
    node_free(dom);

    return passed;
}

void run_core_tests(int *total_failed) {
    run_test_case("DOM Parsing", test_dom_impl, total_failed);
    run_test_case("Style Computation", test_style_impl, total_failed);
    run_test_case("Layout Engine", test_layout_impl, total_failed);
    run_test_case("Layout Accuracy (Firefox Reference)", test_layout_accuracy_impl, total_failed);
}
