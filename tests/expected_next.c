#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "liblognorm.h"
#include "lognorm-turbo.h"

struct warning_state {
	unsigned count;
	int mentionsTurbo;
	int mentionsSlowWalker;
};

static void
warning_callback(void *cookie, const char *msg, size_t lenMsg)
{
	struct warning_state *const state = cookie;
	static const char prefix[] = "warning: ";

	if(lenMsg < sizeof(prefix) - 1 || strncmp(msg, prefix, sizeof(prefix) - 1) != 0)
		return;
	++state->count;
	state->mentionsTurbo |= strstr(msg, "disables TurboVM") != NULL;
	state->mentionsSlowWalker |= strstr(msg, "slower recursive walker") != NULL;
	(void)lenMsg;
}

static struct json_object *
normalize(const char *const rulebase, const char *const message, const unsigned opts)
{
	char path[] = "/tmp/liblognorm-expected-next-XXXXXX";
	ln_ctx ctx;
	struct json_object *json = NULL;
	FILE *file = NULL;
	int fd = -1;
	char *serialized = NULL;

	ctx = ln_initCtx();
	if(ctx == NULL)
		return NULL;
	if(opts != 0)
		ln_setCtxOpts(ctx, opts);
	if((fd = mkstemp(path)) == -1 || (file = fdopen(fd, "w")) == NULL)
		goto done;
	fd = -1;
	if(fwrite(rulebase, 1, strlen(rulebase), file) != strlen(rulebase))
		goto done;
	if(fclose(file) != 0) {
		file = NULL;
		goto done;
	}
	file = NULL;
	if(ln_loadSamples(ctx, path) != 0)
		goto done;
	(void)ln_normalize(ctx, message, strlen(message), &json);
	if(json != NULL) {
		serialized = strdup(json_object_to_json_string(json));
		json_object_put(json);
		json = NULL;
	}

done:
	if(file != NULL)
		(void)fclose(file);
	else if(fd != -1)
		(void)close(fd);
	(void)unlink(path);
	ln_exitCtx(ctx);
	if(serialized != NULL) {
		json = json_tokener_parse(serialized);
		free(serialized);
	}
	return json;
}

static struct json_object *
get_parse_error(struct json_object *const json)
{
	struct json_object *error = NULL;
	if(json != NULL)
		(void)json_object_object_get_ex(json, "parse-error", &error);
	return error;
}

static int
has_candidate(struct json_object *const error, const char *const type,
		const char *const key, const char *const expected)
{
	struct json_object *array;
	if(error == NULL || !json_object_object_get_ex(error, "expected-next", &array))
		return 0;
	for(size_t i = 0 ; i < (size_t)json_object_array_length(array) ; ++i) {
		struct json_object *item = json_object_array_get_idx(array, i);
		struct json_object *value;
		if(!json_object_object_get_ex(item, "type", &value)
				|| strcmp(json_object_get_string(value), type) != 0)
			continue;
		if(key == NULL)
			return 1;
		if(json_object_object_get_ex(item, key, &value)
				&& strcmp(json_object_get_string(value), expected) == 0)
			return 1;
	}
	return 0;
}

static int
check_common_failure(const char *const rulebase, const char *const message,
		const char *const parser, const char *const field)
{
	struct json_object *json;
	struct json_object *error;
	struct json_object *value;
	int ok;

	json = normalize(rulebase, message, LN_CTXOPT_ADD_EXPECTED_NEXT);
	error = get_parse_error(json);
	ok = error != NULL
		&& json_object_object_get_ex(error, "offset", &value)
		&& (size_t)json_object_get_int64(value) == strlen(message)
		&& json_object_object_get_ex(error, "at-eof", &value)
		&& json_object_get_boolean(value)
		&& has_candidate(error, "parser", "parser", parser)
		&& has_candidate(error, "parser", "field", field);
	if(json != NULL)
		json_object_put(json);
	return ok;
}

static int
test_disabled_and_success(void)
{
	static const char rulebase[] =
		"version=2\n"
		"rule=:gateway=%gateway:word% source=%source:ipv4% result=%result:number%\n";
	static const char failed[] =
		"gateway=edgerouter17 source=10.33.245.213 result=";
	static const char success[] =
		"gateway=edgerouter17 source=10.33.245.213 result=42";
	struct json_object *json;
	int ok;

	json = normalize(rulebase, failed, 0);
	ok = json != NULL && get_parse_error(json) == NULL;
	if(json != NULL)
		json_object_put(json);
	json = normalize(rulebase, success, LN_CTXOPT_ADD_EXPECTED_NEXT);
	ok = ok && json != NULL && get_parse_error(json) == NULL;
	if(json != NULL)
		json_object_put(json);
	return ok;
}

static int
test_v1_preserves_existing_unparsed_on_success(void)
{
	static const char rulebase[] = "rule=:source=%source:ipv4% result=%result:number%\n";
	static const char message[] = "source=10.33.245.213 result=42";
	char path[] = "/tmp/liblognorm-expected-existing-XXXXXX";
	struct json_object *json = json_object_new_object();
	struct json_object *value = NULL;
	ln_ctx ctx = ln_initCtx();
	FILE *file = NULL;
	int fd = -1;
	int ok = 0;

	if(ctx == NULL || json == NULL)
		goto done;
	ln_setCtxOpts(ctx, LN_CTXOPT_ADD_EXPECTED_NEXT);
	json_object_object_add(json, "unparsed-data", json_object_new_string("keep"));
	if((fd = mkstemp(path)) == -1 || (file = fdopen(fd, "w")) == NULL)
		goto done;
	fd = -1;
	if(fwrite(rulebase, 1, sizeof(rulebase) - 1, file) != sizeof(rulebase) - 1)
		goto done;
	if(fclose(file) != 0) {
		file = NULL;
		goto done;
	}
	file = NULL;
	if(ln_loadSamples(ctx, path) != 0)
		goto done;
	(void)ln_normalize(ctx, message, sizeof(message) - 1, &json);
	ok = get_parse_error(json) == NULL
		&& json_object_object_get_ex(json, "unparsed-data", &value)
		&& strcmp(json_object_get_string(value), "keep") == 0;

done:
	if(file != NULL)
		(void)fclose(file);
	else if(fd != -1)
		(void)close(fd);
	(void)unlink(path);
	if(json != NULL)
		json_object_put(json);
	if(ctx != NULL)
		ln_exitCtx(ctx);
	return ok;
}

static int
test_crlf_rulebase(void)
{
	static const char message[] =
		"gateway=edgerouter17 tenant=customerblue operation=policycommit "
		"enrichment_section: fromhost-ip=10.33.245.213";
	static const char v2[] =
		"version=2\r\n"
		"rule=:gateway=%gateway:word% tenant=%tenant:word% operation=%operation:word% "
		"enrichment_section: fromhost-ip=%source:ipv4%\r\n";
	static const char v1[] =
		"rule=:gateway=%gateway:word% tenant=%tenant:word% operation=%operation:word% "
		"enrichment_section: fromhost-ip=%source:ipv4%\r\n";
	const char *rulebases[] = { v1, v2 };
	int ok = 1;

	for(size_t i = 0 ; i < sizeof(rulebases) / sizeof(rulebases[0]) ; ++i) {
		struct json_object *json = normalize(rulebases[i], message,
			LN_CTXOPT_ADD_EXPECTED_NEXT);
		struct json_object *error = get_parse_error(json);
		ok = ok && error != NULL && has_candidate(error, "literal", "hex", "0d");
		if(json != NULL)
			json_object_put(json);
	}
	return ok;
}

static int
test_typed_motif(void)
{
	static const char message[] =
		"gateway=edgerouter17 tenant=customerblue source=10.33.245.213 "
		"operation=policycommit result=";
	static const char v2[] =
		"version=2\n"
		"rule=:gateway=%gateway:word% tenant=%tenant:word% source=%source:ipv4% "
		"operation=%operation:alpha% result=%result:number%\n";
	static const char v1[] =
		"rule=:gateway=%gateway:word% tenant=%tenant:word% source=%source:ipv4% "
		"operation=%operation:alpha% result=%result:number%\n";

	return check_common_failure(v1, message, "number", "result")
		&& check_common_failure(v2, message, "number", "result");
}

static int
test_delayed_conflict(void)
{
	static const char message[] =
		"event=policy-update gateway=edgerouter tenant=customer-blue "
		"checkpoint=10.33.245.213 actor=automation";
	static const char v2[] =
		"version=2\n"
		"rule=:event=policy-update gateway=%route:word% tenant=customer-blue "
		"checkpoint=%source:ipv4% actor=%actor:word% status=approved\n"
		"rule=:event=policy-update gateway=%route:alpha% tenant=customer-blue "
		"checkpoint=%source:ipv4% actor=%actor:word% count=%count:number%\n";
	static const char v1[] =
		"rule=:event=policy-update gateway=%route:word% tenant=customer-blue "
		"checkpoint=%source:ipv4% actor=%actor:word% status=approved\n"
		"rule=:event=policy-update gateway=%route:alpha% tenant=customer-blue "
		"checkpoint=%source:ipv4% actor=%actor:word% count=%count:number%\n";
	const char *rulebases[] = { v1, v2 };
	int ok = 1;

	for(size_t i = 0 ; i < sizeof(rulebases) / sizeof(rulebases[0]) ; ++i) {
		struct json_object *json = normalize(rulebases[i], message,
			LN_CTXOPT_ADD_EXPECTED_NEXT);
		struct json_object *error = get_parse_error(json);
		ok = ok && has_candidate(error, "literal", "value", " status=approved")
			&& has_candidate(error, "literal", "value", " count=");
		if(json != NULL)
			json_object_put(json);
	}
	return ok;
}

static int
test_terminal_choice(void)
{
	static const char rulebase[] =
		"version=2\n"
		"rule=:audit gateway=%gateway:word% source=%source:ipv4%\n"
		"rule=:audit gateway=%gateway:word% source=%source:ipv4% status=%status:word%\n";
	static const char message[] =
		"audit gateway=edge-router source=10.33.245.213!unexpected-tail";
	struct json_object *json;
	struct json_object *error;
	int ok;

	json = normalize(rulebase, message, LN_CTXOPT_ADD_EXPECTED_NEXT);
	error = get_parse_error(json);
	ok = has_candidate(error, "end-of-input", NULL, NULL)
		&& has_candidate(error, "literal", "value", " status=");
	if(json != NULL)
		json_object_put(json);
	return ok;
}

static int
test_bounds(void)
{
	char rulebase[8192];
	char long_literal[81];
	size_t used;
	struct json_object *json;
	struct json_object *error;
	struct json_object *array;
	struct json_object *value;
	int ok;

	used = (size_t)snprintf(rulebase, sizeof(rulebase), "version=2\n");
	for(int i = 0 ; i < 33 ; ++i) {
		const char choice = i < 10 ? (char)('0' + i) : (char)('A' + i - 10);
		used += (size_t)snprintf(rulebase + used, sizeof(rulebase) - used,
			"rule=:diagnostic gateway=edge-router tenant=customer-blue %c-tail\n", choice);
	}
	memset(long_literal, 'x', sizeof(long_literal) - 1);
	long_literal[sizeof(long_literal) - 1] = '\0';
	(void)snprintf(rulebase + used, sizeof(rulebase) - used,
		"rule=:diagnostic gateway=edge-router tenant=customer-blue %s\n", long_literal);
	json = normalize(rulebase,
		"diagnostic gateway=edge-router tenant=customer-blue ",
		LN_CTXOPT_ADD_EXPECTED_NEXT);
	error = get_parse_error(json);
	ok = error != NULL
		&& json_object_object_get_ex(error, "expected-next", &array)
		&& json_object_array_length(array) == 32
		&& json_object_object_get_ex(error, "expected-next-truncated", &value)
		&& json_object_get_boolean(value);
	if(!ok && json != NULL)
		fprintf(stderr, "bounded candidates: %s\n", json_object_to_json_string(json));
	if(json != NULL)
		json_object_put(json);

	(void)snprintf(rulebase, sizeof(rulebase),
		"version=2\nrule=:diagnostic gateway=edge-router tenant=customer-blue %s\n",
		long_literal);
	json = normalize(rulebase,
		"diagnostic gateway=edge-router tenant=customer-blue ",
		LN_CTXOPT_ADD_EXPECTED_NEXT);
	error = get_parse_error(json);
	ok = ok && error != NULL && json_object_object_get_ex(error, "expected-next", &array);
	if(ok) {
		struct json_object *item = json_object_array_get_idx(array, 0);
		ok = json_object_object_get_ex(item, "length", &value)
			&& json_object_get_int64(value) == 80
			&& json_object_object_get_ex(item, "truncated", &value)
			&& json_object_get_boolean(value);
	}
	if(!ok && json != NULL)
		fprintf(stderr, "literal truncation: %s\n", json_object_to_json_string(json));
	if(json != NULL)
		json_object_put(json);
	return ok;
}

static int
test_turbo_compatibility(void)
{
	static const char rulebase[] =
		"version=2\n"
		"rule=:gateway=%gateway:word% source=%source:ipv4% result=%result:number%\n";
	static const char message[] =
		"gateway=edge-router-17 source=10.33.245.213 result=";
	ln_ctx ctx;
	ln_ctx reverseCtx;
	struct json_object *json = NULL;
	struct warning_state warning = {0}, reverseWarning = {0};
	int ok;

	ctx = ln_initCtx();
	if(ctx == NULL)
		return 0;
	ln_setErrMsgCB(ctx, warning_callback, &warning);
	ln_setCtxOpts(ctx, LN_CTXOPT_TURBO);
	ln_setCtxOpts(ctx, LN_CTXOPT_ADD_EXPECTED_NEXT);
	ln_setCtxOpts(ctx, LN_CTXOPT_TURBO | LN_CTXOPT_ADD_EXPECTED_NEXT);
	ok = ln_loadSamplesFromString(ctx, rulebase) == 0
		&& !ln_turbo_is_available(ctx);
#ifdef ENABLE_TURBO
	ok = ok && warning.count == 1 && warning.mentionsTurbo && warning.mentionsSlowWalker;
#endif
	(void)ln_normalize(ctx, message, strlen(message), &json);
	ok = ok && get_parse_error(json) != NULL
		&& has_candidate(get_parse_error(json), "parser", "parser", "number");
	if(json != NULL)
		json_object_put(json);
	ln_exitCtx(ctx);

	reverseCtx = ln_initCtx();
	if(reverseCtx == NULL)
		return 0;
	ln_setErrMsgCB(reverseCtx, warning_callback, &reverseWarning);
	ln_setCtxOpts(reverseCtx, LN_CTXOPT_ADD_EXPECTED_NEXT);
	ln_setCtxOpts(reverseCtx, LN_CTXOPT_TURBO);
#ifdef ENABLE_TURBO
	ok = ok && reverseWarning.count == 1 && reverseWarning.mentionsTurbo
		&& reverseWarning.mentionsSlowWalker;
#endif
	ln_exitCtx(reverseCtx);
	return ok;
}

int
main(void)
{
	int ret = 0;
#define RUN_TEST(test) do { \
	if(!(test())) { \
		fprintf(stderr, "%s failed\n", #test); \
		ret = 1; \
	} \
} while(0)
	RUN_TEST(test_disabled_and_success);
	RUN_TEST(test_v1_preserves_existing_unparsed_on_success);
	RUN_TEST(test_crlf_rulebase);
	RUN_TEST(test_typed_motif);
	RUN_TEST(test_delayed_conflict);
	RUN_TEST(test_terminal_choice);
	RUN_TEST(test_bounds);
	RUN_TEST(test_turbo_compatibility);
#undef RUN_TEST
	return ret;
}
