#ifndef BL_UNIT_H
#define BL_UNIT_H

#include "ast.h"
#include "common.h"
#include "config.h"
#include "scope.h"
#include "tokens.h"

struct token;
struct assembly;

struct unit_docs_entry {
	u64   hash;
	str_t text;
};

struct unit {
	hash_t        hash;
	struct tokens tokens;
	struct ast   *ast;
	array(struct ast *) ublock_ast;
	struct scope   *parent_scope;
	struct scope   *private_scope;
	struct module  *module;
	str_t           filepath;
	str_t           dirpath;
	str_t           name;
	str_t           filename;
	char           *src;
	struct token   *loaded_from;
	LLVMMetadataRef llvm_file_meta;

	// @Note 2026-01-30: AST documentation (generated when -doc argument is passed to the compiler) is decoupled
	//                   from AST nodes and stored in separate hash table. Thus we don't need to keep pointer
	//                   to documentation in every AST node. Mapping is: AST Node ID -> documentation string.
	str_buf_t global_docs_cache;
	hash_table(struct unit_docs_entry) docs;
};

// The inject_to_scope is supposed to be valid scope (parent scope of the #load directive or global scope).
struct unit *unit_new(struct assembly *assembly, const str_t filepath, const str_t name, const hash_t hash, struct token *load_from, struct scope *parent_scope, struct module *module);
void         unit_delete(struct unit *unit);
// Returns single line from the unit source code, len does not count last new line char.
const char *unit_get_src_ln(struct unit *unit, s32 line, long *len);
hash_t      unit_get_hash(const str_t filepath);

#endif
