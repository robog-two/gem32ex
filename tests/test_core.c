#include <stdio.h>
#include <string.h>
#include "core/dom.h"
#include "core/html.h"
#include "core/style.h"
#include "core/layout.h"
#include "core/platform.h"

static int assert_core(int condition, const char *msg, int *failed) {
    if (condition) {
        printf("[ PASS ] %s\n", msg);
        fflush(stdout);
        return 0;
    } else {
        printf("[ FAIL ] %s\n", msg);
        fflush(stdout);
        (*failed)++;
        return 1;
    }
}

// Mock platform implementation
void platform_measure_text(const char *text, style_t *style, int width_constraint, int *out_width, int *out_height, int *out_baseline) {
    (void)style; (void)width_constraint;
    if (!text) return;
    int len = (int)strlen(text);
    *out_width = len * 10;
    *out_height = 20;
    if (out_baseline) *out_baseline = 16;
}

void run_core_tests(int *total_failed) {
    int local_failed = 0;
    printf("Running core engine tests...\n");
    fflush(stdout);

    // HTML/DOM Test
    const char *html = "<html><body><h1 id='t'>Title</h1></body></html>";
    node_t *dom = html_parse(html);
    if (assert_core(dom != NULL, "HTML parse works", &local_failed) == 0) {
        node_t *html_n = dom->first_child;
        node_t *body_n = html_n ? html_n->first_child : NULL;
        node_t *h1_n = body_n ? body_n->first_child : NULL;
        assert_core(h1_n && strcmp(h1_n->tag_name, "h1") == 0, "DOM tree structure correct", &local_failed);
        assert_core(h1_n && strcmp(node_get_attr(h1_n, "id"), "t") == 0, "Attribute parsing works", &local_failed);
        
        // Style Test
        style_compute(dom);
        assert_core(h1_n->style->font_size == 32, "Default styles applied (h1 font-size)", &local_failed);
        assert_core(h1_n->style->display == DISPLAY_BLOCK, "Default styles applied (h1 display)", &local_failed);
        
        // Layout Test
        layout_box_t *layout = layout_create_tree(dom, 800);
        if (assert_core(layout != NULL, "Layout tree creation works", &local_failed) == 0) {
            assert_core(layout->fragment.border_box.width == 800, "Layout root width correct", &local_failed);
            layout_free(layout);
        }
        node_free(dom);
    }

    if (local_failed == 0) printf("[ PASS ] All core engine tests passed.\n");
    *total_failed += local_failed;
}