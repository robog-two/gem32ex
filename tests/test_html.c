#include <stdio.h>
#include <string.h>
#include <core/html.h>
#include <core/layout.h>
#include <core/platform.h>

// Mock implementation for testing
void platform_measure_text(const char *text, style_t *style, int width_constraint, int *out_width, int *out_height, int *out_baseline) {
    if (!text) { *out_width = 0; *out_height = 0; if (out_baseline) *out_baseline = 0; return; }
    
    int len = strlen(text);
    int char_w = (style && style->font_size > 0) ? style->font_size / 2 : 8; // Approx width
    int char_h = (style && style->font_size > 0) ? style->font_size : 16;
    
    int total_w = len * char_w;
    
    if (width_constraint > 0 && total_w > width_constraint) {
        // Mock wrapping
        *out_width = width_constraint;
        int lines = (total_w / width_constraint) + 1;
        *out_height = lines * char_h;
    } else {
        *out_width = total_w;
        *out_height = char_h;
    }
    if (out_baseline) *out_baseline = (char_h * 8) / 10; // 80% baseline
}

void print_layout(layout_box_t *box, int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
    printf("<%s> @ (%d, %d) size %dx%d\n", 
           box->node->tag_name ? box->node->tag_name : "TEXT",
           box->fragment.border_box.x, box->fragment.border_box.y,
           box->fragment.border_box.width, box->fragment.border_box.height);

    layout_box_t *child = box->first_child;
    while (child) {
        print_layout(child, depth + 1);
        child = child->next_sibling;
    }
}

int main() {
    const char *html = "<html><body><table><tr><td>Cell 1</td><td>Cell 2</td></tr></table></body></html>";
    node_t *dom = html_parse(html);
    if (dom) {
        style_compute(dom);
        layout_box_t *layout = layout_create_tree(dom, 800);
        if (layout) {
            print_layout(layout, 0);
            layout_free(layout);
        }
        node_free(dom);
    }
    return 0;
}

