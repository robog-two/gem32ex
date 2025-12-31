#include <stdio.h>
#include <core/html.h>

void print_tree(node_t *node, int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
    if (node->type == NODE_ELEMENT) {
        printf("<%s>\n", node->tag_name);
        attr_t *attr = node->attributes;
        while (attr) {
            for (int i = 0; i < depth + 1; i++) printf("  ");
            printf("@%s=%s\n", attr->name, attr->value ? attr->value : "NULL");
            attr = attr->next;
        }
    } else {
        printf("TEXT: %s\n", node->content);
    }

    node_t *child = node->first_child;
    while (child) {
        print_tree(child, depth + 1);
        child = child->next_sibling;
    }
}

int main() {
    const char *html = "<html><head><title>Test</title></head><body><h1>Hello</h1><p class=\"main\">World<img src=\"img.png\"/></p></body></html>";
    node_t *root = html_parse(html);
    if (root) {
        print_tree(root, 0);
        node_free(root);
    } else {
        printf("Failed to parse HTML\n");
        return 1;
    }
    return 0;
}

