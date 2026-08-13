#include "config.h"
#include <string.h>
#include <libestr.h>

#define LOGNORM_V1_SUBSYSTEM
#include "v1_liblognorm.h"
#include "internal.h"
#include "lognorm.h"
#include "v1_expected.h"
#include "v1_parser.h"

static inline unsigned char *
prefixBase(struct ln_ptree *const tree)
{
	return tree->lenPrefix <= sizeof(tree->prefix)
		? tree->prefix.data : tree->prefix.ptr;
}

static const char *
parserName(const ln_fieldList_t *const node)
{
	if(node->isIPTables) return "iptables";
	if(node->parser == ln_parseRFC3164Date) return "date-rfc3164";
	if(node->parser == ln_parseRFC5424Date) return "date-rfc5424";
	if(node->parser == ln_parseNumber) return "number";
	if(node->parser == ln_parseFloat) return "float";
	if(node->parser == ln_parseHexNumber) return "hexnumber";
	if(node->parser == ln_parseKernelTimestamp) return "kernel-timestamp";
	if(node->parser == ln_parseWhitespace) return "whitespace";
	if(node->parser == ln_parseIPv4) return "ipv4";
	if(node->parser == ln_parseIPv6) return "ipv6";
	if(node->parser == ln_parseWord) return "word";
	if(node->parser == ln_parseAlpha) return "alpha";
	if(node->parser == ln_parseRest) return "rest";
	if(node->parser == ln_parseOpQuotedString) return "op-quoted-string";
	if(node->parser == ln_parseQuotedString) return "quoted-string";
	if(node->parser == ln_parseISODate) return "date-iso";
	if(node->parser == ln_parseTime24hr) return "time-24hr";
	if(node->parser == ln_parseTime12hr) return "time-12hr";
	if(node->parser == ln_parseDuration) return "duration";
	if(node->parser == ln_parseCiscoInterfaceSpec) return "cisco-interface-spec";
	if(node->parser == ln_parseJSON) return "json";
	if(node->parser == ln_parseCEESyslog) return "cee-syslog";
	if(node->parser == ln_parseMAC48) return "mac48";
	if(node->parser == ln_parseNameValue) return "name-value-list";
	if(node->parser == ln_parseCEF) return "cef";
	if(node->parser == ln_parseCheckpointLEA) return "checkpoint-lea";
	if(node->parser == ln_parsev2IPTables) return "v2-iptables";
	if(node->parser == ln_parseStringTo) return "string-to";
	if(node->parser == ln_parseCharTo) return "char-to";
	if(node->parser == ln_parseCharSeparated) return "char-sep";
	if(node->parser == ln_parseTokenized) return "tokenized";
#ifdef FEATURE_REGEXP
	if(node->parser == ln_parseRegex) return "regex";
#endif
	if(node->parser == ln_parseRecursive) return "recursive";
	if(node->parser == ln_parseInterpret) return "interpret";
	if(node->parser == ln_parseSuffixed) return "suffixed";
	return "unknown";
}

static void
recordNode(ln_expected_tracker_t *const expected,
		struct ln_ptree *const tree, const size_t offset)
{
	if(tree->flags.isTerminal)
		ln_expected_add_end(expected, offset);
	for(ln_fieldList_t *node = tree->froot ; node != NULL ; node = node->next) {
		const char *const type = parserName(node);
		const char *field = (const char *)es_getBufAddr(node->name);
		size_t field_len = es_strlen(node->name);
		if(field_len == 1 && field[0] == '-') {
			field = NULL;
			field_len = 0;
		}
		ln_expected_add_parser(expected, offset, type, strlen(type),
			field, field_len);
	}
	for(size_t i = 0 ; i < 256 ; ++i) {
		if(tree->subtree[i] != NULL) {
			const unsigned char first = (unsigned char)i;
			ln_expected_add_literal_parts(expected, offset, &first, 1,
				prefixBase(tree->subtree[i]), tree->subtree[i]->lenPrefix);
		}
	}
}

void
ln_v1_recordExpected(struct ln_ptree *tree, const char *str, const size_t strLen,
		size_t offs, struct json_object *json, ln_expected_tracker_t *expected)
{
	const unsigned char *const prefix = prefixBase(tree);
	ln_fieldList_t *rest = NULL;
	size_t prefix_off = 0;

	while(offs < strLen && prefix_off < tree->lenPrefix) {
		if((unsigned char)str[offs] != prefix[prefix_off]) {
			ln_expected_add_literal(expected, offs, prefix + prefix_off,
				tree->lenPrefix - prefix_off);
			return;
		}
		++offs;
		++prefix_off;
	}
	if(prefix_off != tree->lenPrefix) {
		ln_expected_add_literal(expected, offs, prefix + prefix_off,
			tree->lenPrefix - prefix_off);
		return;
	}

	for(ln_fieldList_t *node = tree->froot ; node != NULL ; node = node->next) {
		size_t parser_off = offs;
		size_t parsed = 0;
		struct json_object *value = NULL;
		if(node->isIPTables) {
			/* The legacy iptables parser writes multiple values directly into
			 * the event. Report the motif at this node without replaying it. */
			continue;
		}
		if(node->parser == ln_parseRest) {
			rest = node;
		} else {
			const int parser_result = node->parser(str, strLen, &parser_off, node,
				&parsed, &value);
			if(parser_result == 0)
				ln_v1_recordExpected(node->subtree, str, strLen,
					parser_off + parsed, json, expected);
		}
		if(value != NULL)
			json_object_put(value);
	}

	if(offs < strLen && tree->subtree[(unsigned char)str[offs]] != NULL)
		ln_v1_recordExpected(tree->subtree[(unsigned char)str[offs]], str,
			strLen, offs + 1, json, expected);
	if(rest != NULL) {
		size_t parser_off = offs;
		size_t parsed = 0;
		struct json_object *value = NULL;
		if(rest->parser(str, strLen, &parser_off, rest, &parsed, &value) == 0)
			ln_v1_recordExpected(rest->subtree, str, strLen,
				parser_off + parsed, json, expected);
		if(value != NULL)
			json_object_put(value);
	}
	recordNode(expected, tree, offs);
}

int
ln_v1_normalizeExpected(ln_ctx ctx, const char *str, const size_t strLen,
		struct json_object **json_p)
{
	struct json_object *unparsed = NULL;
	struct json_object *existing_unparsed = NULL;
	ln_expected_tracker_t *expected;
	struct json_object *diagnostic_json;
	int had_existing = 0;
	int r;

	if(*json_p != NULL && json_object_object_get_ex(*json_p,
			UNPARSED_DATA_KEY, &existing_unparsed)) {
		had_existing = 1;
		json_object_get(existing_unparsed);
		json_object_object_del(*json_p, UNPARSED_DATA_KEY);
	}
	r = ln_v1_normalize(ctx, str, strLen, json_p);

	if(r != 0 || *json_p == NULL
			|| !json_object_object_get_ex(*json_p, UNPARSED_DATA_KEY, &unparsed)) {
		if(had_existing && *json_p != NULL) {
			json_object_object_add(*json_p, UNPARSED_DATA_KEY, existing_unparsed);
			existing_unparsed = NULL;
		}
		if(existing_unparsed != NULL)
			json_object_put(existing_unparsed);
		return r;
	}
	if(existing_unparsed != NULL)
		json_object_put(existing_unparsed);
	expected = ln_expected_new();
	if(expected == NULL)
		return r;
	diagnostic_json = json_object_new_object();
	if(diagnostic_json != NULL) {
		ln_v1_recordExpected(ctx->ptree, str, strLen, 0, diagnostic_json, expected);
		json_object_put(diagnostic_json);
	}
	(void)ln_expected_add_json(expected, strLen, *json_p);
	ln_expected_free(expected);
	return r;
}
