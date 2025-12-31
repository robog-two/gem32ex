#include "bookmarks.h"
#include <stdlib.h>
#include <string.h>

static bookmark_t *bookmarks_head = NULL;

void bookmark_add(const char *url, const char *title) {
    bookmark_t *bm = malloc(sizeof(bookmark_t));
    if (bm) {
        bm->url = strdup(url);
        bm->title = title ? strdup(title) : strdup(url);
        bm->next = bookmarks_head;
        bookmarks_head = bm;
    }
}

bookmark_t* bookmark_get_all() {
    return bookmarks_head;
}

void bookmark_free_all() {
    bookmark_t *curr = bookmarks_head;
    while (curr) {
        bookmark_t *next = curr->next;
        free(curr->url);
        free(curr->title);
        free(curr);
        curr = next;
    }
    bookmarks_head = NULL;
}
