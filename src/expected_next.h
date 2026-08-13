#ifndef LIBLOGNORM_EXPECTED_NEXT_H_INCLUDED
#define LIBLOGNORM_EXPECTED_NEXT_H_INCLUDED

#include <stddef.h>
#include <json.h>

#define LN_EXPECTED_MAX_CANDIDATES 32
#define LN_EXPECTED_LITERAL_PREVIEW 64

typedef struct ln_expected_tracker_s ln_expected_tracker_t;

ln_expected_tracker_t *ln_expected_new(void);
void ln_expected_free(ln_expected_tracker_t *tracker);
void ln_expected_add_parser(ln_expected_tracker_t *tracker, size_t offset,
		const char *parser, size_t parser_len, const char *field, size_t field_len);
void ln_expected_add_literal(ln_expected_tracker_t *tracker, size_t offset,
		const void *literal, size_t literal_len);
void ln_expected_add_literal_parts(ln_expected_tracker_t *tracker, size_t offset,
		const void *first, size_t first_len, const void *second, size_t second_len);
void ln_expected_add_end(ln_expected_tracker_t *tracker, size_t offset);
int ln_expected_add_json(ln_expected_tracker_t *tracker, size_t message_len,
		struct json_object *json);

#endif
