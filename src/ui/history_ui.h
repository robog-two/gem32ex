#ifndef HISTORY_UI_H
#define HISTORY_UI_H

#include <windows.h>
#include "history.h"

// Rendering and interaction functions for the history pane UI
void history_ui_draw(HDC hdc, history_tree_t *tree);
history_node_t* history_ui_hit_test(history_tree_t *tree, int hitX, int hitY);
void history_ui_fetch_favicon(history_node_t *node);

#endif // HISTORY_UI_H
