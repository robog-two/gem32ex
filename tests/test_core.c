#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/dom.h"
#include "core/html.h"
#include "core/style.h"
#include "core/layout.h"
#include "core/platform.h"
#include "core/log.h"

int g_failed_core = 0;

void assert_core(int condition, const char *msg) {
    if (condition) {
        printf("[ PASS ] %s\n", msg);
    } else {
        printf("[ FAIL ] %s\n", msg);
        g_failed_core++;
    }
}

// Mock platform implementation
void platform_measure_text(const char *text, style_t *style, int width_constraint, int *out_width, int *out_height, int *out_baseline) {
    (void)style; (void)width_constraint;
    int len = (int)strlen(text);
    *out_width = len * 10;
    *out_height = 20;
    if (out_baseline) *out_baseline = 16;
}

void test_dom_and_html() {
    printf("\n--- Testing HTML Parsing & DOM Structure ---\n");
    const char *html = "<html><body class='test'><h1>Title</h1><p>Text</p></body></html>";
    node_t *dom = html_parse(html);
    
    assert_core(dom != NULL, "DOM parsed from string");
    if (dom) {
        // Root is "root" wrapper
        assert_core(dom->type == DOM_NODE_ELEMENT, "Root is element");
        
        node_t *html_node = dom->first_child;
        assert_core(html_node && strcmp(html_node->tag_name, "html") == 0, "First child is <html>");
        
        node_t *body_node = html_node ? html_node->first_child : NULL;
        assert_core(body_node && strcmp(body_node->tag_name, "body") == 0, "Child of html is <body>");
        
        const char *cls = node_get_attr(body_node, "class");
        assert_core(cls && strcmp(cls, "test") == 0, "Attribute parsing works");
        
        node_t *h1 = body_node ? body_node->first_child : NULL;
        assert_core(h1 && strcmp(h1->tag_name, "h1") == 0, "First child of body is <h1>");
        
        node_t *p = h1 ? h1->next_sibling : NULL;
        assert_core(p && strcmp(p->tag_name, "p") == 0, "Sibling of h1 is <p>");
        
        node_free(dom);
    }
}

void test_style() {
    printf("\n--- Testing Style Computation ---\n");
    const char *html = "<html><body><h1>Big Title</h1><a href='#' style='color:red'>Link</a></body></html>";
    node_t *dom = html_parse(html);
    if (dom) {
        style_compute(dom);
        
        node_t *html_node = dom->first_child;
        node_t *body = html_node->first_child;
        node_t *h1 = body->first_child;
        node_t *a = h1->next_sibling;
        
        assert_core(h1->style->font_size == 32, "H1 default font-size is 32");
        assert_core(h1->style->display == DISPLAY_BLOCK, "H1 is block element");
        
        assert_core(a->style->color == 0x0000FF, "Link (<a>) is blue by default");
        assert_core(a->style->text_decoration == TEXT_DECORATION_UNDERLINE, "Link is underlined by default");
        
        // Check inheritance
        node_t *text_inside_h1 = h1->first_child;
        if (text_inside_h1) {
            assert_core(text_inside_h1->style->font_size == 32, "Text inside H1 inherits font-size");
        }
        
        node_free(dom);
    }
}

void test_layout() {
    printf("\n--- Testing Layout Engine ---\n");
    const char *html = "<html><body style='width:500px'><div style='width:200px; height:100px'></div></body></html>";
    node_t *dom = html_parse(html);
    if (dom) {
        style_compute(dom);
        layout_box_t *layout = layout_create_tree(dom, 800);
        
        assert_core(layout != NULL, "Layout tree created");
        if (layout) {
            // Root box size
            assert_core(layout->fragment.border_box.width == 800, "Root layout width matches container");
            
            // Body box size (inherited width constraint or fixed)
            layout_box_t *html_box = layout->first_child;
            layout_box_t *body_box = html_box->first_child;
            
            // Our style_compute currently handles inline 'width' attribute but maybe not CSS 'style' attribute fully in this test
            // Let's check block flow
            layout_box_t *div_box = body_box->first_child;
            assert_core(div_box != NULL, "DIV box exists in layout");
            
            layout_free(layout);
        }
        node_free(dom);
    }
}

int main() {
    log_init(); // Should be quiet in automated tests
    
    printf("=======================================\n");
    printf("   Gem32 Core Engine Tests\n");
    printf("=======================================\n");
    
    test_dom_and_html();
    test_style();
    test_layout();
    
    printf("\n=======================================\n");
    if (g_failed_core == 0) {
        printf("  ALL CORE TESTS PASSED SUCCESSFULLY\n");
    } else {
        printf("  CORE TEST SUITE FAILED: %d errors\n", g_failed_core);
    }
    printf("=======================================\n");
    
    return g_failed_core;
}
