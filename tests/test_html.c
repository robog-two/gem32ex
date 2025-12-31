#include <stdio.h>
#include <core/html.h>
#include <core/layout.h>

void print_layout(layout_box_t *box, int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
    printf("<%s> @ (%d, %d) size %dx%d\n", 
           box->node->tag_name ? box->node->tag_name : "TEXT",
           box->dimensions.x, box->dimensions.y,
           box->dimensions.width, box->dimensions.height);

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

