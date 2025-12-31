#include <stdio.h>
#include <string.h>
#include <core/html.h>
#include <core/layout.h>
#include <core/platform.h>

// Note: platform_measure_text is provided by src/ui/render.c (real Win32 implementation)
// To compile this test: gcc tests/test_forms.c src/ui/render.c src/core/*.c -Isrc -lgdi32 -o test_forms.exe

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
    const char *html = "<html><body>"
                       "<form>"
                       "  <input type='text' value='Hello'>"
                       "  <input type='submit' value='Go'>"
                       "  <button>Click Me</button>"
                       "  <input type='text' size='20'>"
                       "  <textarea></textarea>"
                       "</form>"
                       "</body></html>";

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
