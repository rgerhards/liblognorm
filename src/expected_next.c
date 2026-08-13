#include "config.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "expected_next.h"

enum expected_kind {
	EXPECTED_LITERAL,
	EXPECTED_PARSER,
	EXPECTED_END
};

struct expected_candidate {
	enum expected_kind kind;
	char *parser;
	char *field;
	unsigned char literal[LN_EXPECTED_LITERAL_PREVIEW];
	size_t literal_len;
	size_t literal_preview_len;
};

struct ln_expected_tracker_s {
	size_t offset;
	size_t count;
	int have_offset;
	int truncated;
	struct expected_candidate candidates[LN_EXPECTED_MAX_CANDIDATES];
};

static char *
duplicate_string(const char *str, const size_t len)
{
	char *copy = malloc(len + 1);
	if(copy != NULL) {
		memcpy(copy, str, len);
		copy[len] = '\0';
	}
	return copy;
}

static void
clear_candidates(ln_expected_tracker_t *const tracker)
{
	for(size_t i = 0 ; i < tracker->count ; ++i) {
		free(tracker->candidates[i].parser);
		free(tracker->candidates[i].field);
	}
	tracker->count = 0;
	tracker->truncated = 0;
}

static int
select_offset(ln_expected_tracker_t *const tracker, const size_t offset)
{
	if(tracker == NULL)
		return 0;
	if(!tracker->have_offset || offset > tracker->offset) {
		clear_candidates(tracker);
		tracker->offset = offset;
		tracker->have_offset = 1;
	} else if(offset < tracker->offset) {
		return 0;
	}
	return 1;
}

static int
candidate_equal(const struct expected_candidate *const left,
		const struct expected_candidate *const right)
{
	if(left->kind != right->kind)
		return 0;
	if(left->kind == EXPECTED_END)
		return 1;
	if(left->kind == EXPECTED_LITERAL)
		return left->literal_len == right->literal_len
			&& left->literal_preview_len == right->literal_preview_len
			&& memcmp(left->literal, right->literal, left->literal_preview_len) == 0;
	return strcmp(left->parser, right->parser) == 0
		&& ((left->field == NULL && right->field == NULL)
			|| (left->field != NULL && right->field != NULL
				&& strcmp(left->field, right->field) == 0));
}

static void
store_candidate(ln_expected_tracker_t *const tracker,
		struct expected_candidate *const candidate)
{
	for(size_t i = 0 ; i < tracker->count ; ++i) {
		if(candidate_equal(&tracker->candidates[i], candidate)) {
			free(candidate->parser);
			free(candidate->field);
			return;
		}
	}
	if(tracker->count == LN_EXPECTED_MAX_CANDIDATES) {
		tracker->truncated = 1;
		free(candidate->parser);
		free(candidate->field);
		return;
	}
	tracker->candidates[tracker->count++] = *candidate;
}

ln_expected_tracker_t *
ln_expected_new(void)
{
	return calloc(1, sizeof(ln_expected_tracker_t));
}

void
ln_expected_free(ln_expected_tracker_t *const tracker)
{
	if(tracker != NULL) {
		clear_candidates(tracker);
		free(tracker);
	}
}

void
ln_expected_add_parser(ln_expected_tracker_t *const tracker, const size_t offset,
		const char *const parser, const size_t parser_len,
		const char *const field, const size_t field_len)
{
	struct expected_candidate candidate = { .kind = EXPECTED_PARSER };
	if(!select_offset(tracker, offset))
		return;
	if((candidate.parser = duplicate_string(parser, parser_len)) == NULL)
		return;
	if(field != NULL && (candidate.field = duplicate_string(field, field_len)) == NULL) {
		free(candidate.parser);
		return;
	}
	store_candidate(tracker, &candidate);
}

void
ln_expected_add_literal_parts(ln_expected_tracker_t *const tracker, const size_t offset,
		const void *const first, const size_t first_len,
		const void *const second, const size_t second_len)
{
	struct expected_candidate candidate = { .kind = EXPECTED_LITERAL };
	size_t first_copy;
	if(!select_offset(tracker, offset))
		return;
	candidate.literal_len = first_len + second_len;
	first_copy = first_len < LN_EXPECTED_LITERAL_PREVIEW
		? first_len : LN_EXPECTED_LITERAL_PREVIEW;
	if(first_copy != 0)
		memcpy(candidate.literal, first, first_copy);
	candidate.literal_preview_len = first_copy;
	if(candidate.literal_preview_len < LN_EXPECTED_LITERAL_PREVIEW) {
		const size_t available = LN_EXPECTED_LITERAL_PREVIEW - candidate.literal_preview_len;
		const size_t second_copy = second_len < available ? second_len : available;
		if(second_copy != 0)
			memcpy(candidate.literal + candidate.literal_preview_len, second, second_copy);
		candidate.literal_preview_len += second_copy;
	}
	store_candidate(tracker, &candidate);
}

void
ln_expected_add_literal(ln_expected_tracker_t *const tracker, const size_t offset,
		const void *const literal, const size_t literal_len)
{
	ln_expected_add_literal_parts(tracker, offset, literal, literal_len, NULL, 0);
}

void
ln_expected_add_end(ln_expected_tracker_t *const tracker, const size_t offset)
{
	struct expected_candidate candidate = { .kind = EXPECTED_END };
	if(select_offset(tracker, offset))
		store_candidate(tracker, &candidate);
}

static struct json_object *
literal_hex(const unsigned char *const literal, const size_t len)
{
	static const char hex[] = "0123456789abcdef";
	char encoded[LN_EXPECTED_LITERAL_PREVIEW * 2 + 1];
	for(size_t i = 0 ; i < len ; ++i) {
		encoded[i * 2] = hex[literal[i] >> 4];
		encoded[i * 2 + 1] = hex[literal[i] & 0x0f];
	}
	encoded[len * 2] = '\0';
	return json_object_new_string_len(encoded, len * 2);
}

int
ln_expected_add_json(ln_expected_tracker_t *const tracker, const size_t message_len,
		struct json_object *const json)
{
	struct json_object *error = NULL;
	struct json_object *array = NULL;
	int r = -1;
	if(tracker == NULL || !tracker->have_offset || tracker->count == 0)
		return 0;
	if((error = json_object_new_object()) == NULL
			|| (array = json_object_new_array()) == NULL)
		goto done;
	json_object_object_add(error, "offset", json_object_new_int64((int64_t)tracker->offset));
	json_object_object_add(error, "at-eof", json_object_new_boolean(tracker->offset >= message_len));
	for(size_t i = 0 ; i < tracker->count ; ++i) {
		const struct expected_candidate *const candidate = &tracker->candidates[i];
		struct json_object *item = json_object_new_object();
		if(item == NULL)
			goto done;
		if(candidate->kind == EXPECTED_END) {
			json_object_object_add(item, "type", json_object_new_string("end-of-input"));
		} else if(candidate->kind == EXPECTED_PARSER) {
			json_object_object_add(item, "type", json_object_new_string("parser"));
			json_object_object_add(item, "parser", json_object_new_string(candidate->parser));
			if(candidate->field != NULL)
				json_object_object_add(item, "field", json_object_new_string(candidate->field));
		} else {
			json_object_object_add(item, "type", json_object_new_string("literal"));
			json_object_object_add(item, "value", json_object_new_string_len(
					(const char *)candidate->literal, candidate->literal_preview_len));
			json_object_object_add(item, "hex", literal_hex(candidate->literal,
					candidate->literal_preview_len));
			json_object_object_add(item, "length", json_object_new_int64(
					(int64_t)candidate->literal_len));
			if(candidate->literal_len > candidate->literal_preview_len)
				json_object_object_add(item, "truncated", json_object_new_boolean(1));
		}
		json_object_array_add(array, item);
	}
	json_object_object_add(error, "expected-next", array);
	array = NULL;
	if(tracker->truncated)
		json_object_object_add(error, "expected-next-truncated", json_object_new_boolean(1));
	json_object_object_add(json, "parse-error", error);
	error = NULL;
	r = 0;
done:
	if(array != NULL)
		json_object_put(array);
	if(error != NULL)
		json_object_put(error);
	return r;
}
