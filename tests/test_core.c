#include <string.h>
#include "core/dom.h"
#include "core/html.h"
#include "core/style.h"
#include "core/layout.h"
#include "core/platform.h"
#include "core/log.h"
#include "test_ui.h"

// Mock platform implementation
void platform_measure_text(const char *text, style_t *style, int width_constraint, int *out_width, int *out_height, int *out_baseline) {
    (void)style; (void)width_constraint;
    if (!text) return;
    int len = (int)strlen(text);
    *out_width = len * 10;
    *out_height = 20;
    if (out_baseline) *out_baseline = 16;
}

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

void run_core_tests(int *total_failed) {
    run_test_case("DOM Parsing", test_dom_impl, total_failed);
    run_test_case("Style Computation", test_style_impl, total_failed);
    run_test_case("Layout Engine", test_layout_impl, total_failed);
}
