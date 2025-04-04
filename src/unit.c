#include "unit.h"
#include "assembly.h"
#include "builder.h"
#include "stb_ds.h"
#include <string.h>

#if BL_PLATFORM_WIN
#include <windows.h>
#endif

// public
struct unit *unit_new(struct assembly *assembly, const str_t filepath, const str_t name, const hash_t hash, struct token *load_from, struct scope *parent_scope, struct module *module) {
	struct unit *unit = bmalloc(sizeof(struct unit)); // @Performance 2024-09-14 Use arena?
	bl_zeromem(unit, sizeof(struct unit));

	// blog("Create unit: " STR_FMT, STR_ARG(name));

	const u32             thread_index = get_worker_index();
	struct string_cache **string_cache = &assembly->thread_local_contexts[thread_index].string_cache;

	str_buf_t tmp = get_tmp_str();

	unit->filepath = scdup2(string_cache, filepath);
	unit->name     = scdup2(string_cache, name);
	unit->dirpath  = get_dir_from_filepath(unit->filepath);
	unit->filename = get_filename_from_filepath(unit->filepath);

	unit->hash         = hash;
	unit->loaded_from  = load_from;
	unit->parent_scope = parent_scope;

	unit->module = module;

	tokens_init(&unit->tokens);
	put_tmp_str(tmp);
	return unit;
}

void unit_delete(struct unit *unit) {
	arrfree(unit->ublock_ast);
	str_buf_free(&unit->file_docs_cache);
	bfree(unit->src);
	tokens_terminate(&unit->tokens);
	bfree(unit);
}

const char *unit_get_src_ln(struct unit *unit, s32 line, long *len) {
	if (line < 1) return NULL;
	// Iterate from begin of the file looking for a specific line.
	const char *c     = unit->src;
	const char *begin = c;
	while (true) {
		if (*c == '\n') {
			--line;
			if (line == 0) break; // Line found.
			begin = c + 1;
		}
		if (*c == '\0') {
			--line;
			break;
		}
		++c;
	}
	if (line > 0) return NULL; // Line not found.
	if (len) {
		long l = (long)(c - begin);
		if (l && begin[l - 1] == '\r') --l;
		bassert(l >= 0);
		*len = l;
	}
	return begin;
}
