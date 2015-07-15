/**
 * @file rulebase.h
 * @brief Object to process log rules.
 * @author Rainer Gerhards
 */
// TODO: license (to be decided)
#ifndef LIBLOGNORM_RULEBASE_H_INCLUDED
#define	LIBLOGNORM_RULEBASE_H_INCLUDED
#include <stdio.h>	/* we need es_size_t */


/**
 * Object that represents a rule repository (file).
 *
 * Doing this via an objects helps with abstraction and future
 * changes inside the module (which are anticipated).
 */
struct ln_rbRepos {
	FILE *fp;
};

/**
 * A single log rule.
 */
struct ln_rb {
	es_str_t *msg;
};

/**
 * Open a Sample Repository.
 *
 * @param[in] ctx current library context
 * @param[in] name file name
 * @return repository object or NULL if failure
 */
struct ln_rbRepos *
ln_rbOpen(ln_ctx ctx, const char *name);


/**
 * Close rule file.
 *
 * @param[in] ctx current library context
 * @param[in] fd file descriptor of open rule file
 */
void
ln_rbClose(ln_ctx ctx, struct ln_rbRepos *repo);


/**
 * Reads a rule stored in buffer buf and creates a new ln_rb object
 * out of it.
 *
 * @note
 * It is the caller's responsibility to delete the newly
 * created ln_rb object if it is no longer needed.
 *
 * @param[ctx] ctx current library context
 * @param[buf] cstr buffer containing the string contents of the rule
 * @param[lenBuf] length of the rule contained within buf
 * @return Newly create object or NULL if an error occured.
 */
struct ln_rb *
ln_processSamp(ln_ctx ctx, const char *buf, es_size_t lenBuf);


/**
 * Read a rule from repository (sequentially).
 *
 * Reads a rule starting with the current file position and
 * creates a new ln_rb object out of it.
 *
 * @note
 * It is the caller's responsibility to delete the newly
 * created ln_rb object if it is no longer needed.
 *
 * @param[in] ctx current library context
 * @param[in] repo repository descriptor
 * @param[out] isEof must be set to 0 on entry and is switched to 1 if EOF occured.
 * @return Newly create object or NULL if an error or EOF occured.
 */
struct ln_rb *
ln_rbRead(ln_ctx ctx, struct ln_rbRepos *repo, int *isEof);


/**
 * Free ln_rb object.
 */
void
ln_rbFree(ln_ctx ctx, struct ln_rb *samp);


/**
 * Parse a given rule
 *
 * @param[in] ctx current library context
 * @param[in] rule string (with prefix and suffix '%' markers)
 * @param[in] offset in rule-string to start at (it should be pointed to
 *  starting character: '%')
 * @param[in] temp string buffer(working space),
 *  externalized for efficiency reasons
 * @param[out] return code (0 means success)
 * @return newly created node, which can be added to rule tree.
 */
ln_fieldList_t*
ln_parseFieldDescr(ln_ctx ctx, es_str_t *rule, es_size_t *bufOffs,
				   es_str_t **str, int* ret);

#endif /* #ifndef LIBLOGNORM_RULEBASE_H_INCLUDED */
