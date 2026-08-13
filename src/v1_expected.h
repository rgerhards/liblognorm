#ifndef LIBLOGNORM_V1_EXPECTED_H_INCLUDED
#define LIBLOGNORM_V1_EXPECTED_H_INCLUDED

#include <stddef.h>
#include <json.h>

#include "expected_next.h"
#include "v1_ptree.h"

void ln_v1_recordExpected(struct ln_ptree *tree, const char *str, size_t strLen,
		size_t offs, struct json_object *json, ln_expected_tracker_t *expected);
int ln_v1_normalizeExpected(ln_ctx ctx, const char *str, size_t strLen,
		struct json_object **json_p);

#endif
