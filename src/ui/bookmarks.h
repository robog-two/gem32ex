#ifndef BOOKMARKS_H
#define BOOKMARKS_H

typedef struct bookmark_s {
    char *url;
    char *title;
    struct bookmark_s *next;
} bookmark_t;

void bookmark_add(const char *url, const char *title);
bookmark_t* bookmark_get_all();
void bookmark_free_all();

#endif // BOOKMARKS_H
