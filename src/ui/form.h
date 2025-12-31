#ifndef FORM_H
#define FORM_H

#include "core/dom.h"
#include "network/protocol.h"

// Submits the form enclosing the given node (e.g., a submit button).
// base_url is needed to resolve relative action URLs.
// out_url will be filled with the final URL used (useful for GET params).
// Returns the network response of the submission.
network_response_t* form_submit(node_t *submit_node, const char *base_url, char *out_url, size_t out_url_size);

#endif // FORM_H
