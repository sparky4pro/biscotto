#include "assembly.h"
#include "builder.h"
#include "conf.h"
#include "stb_ds.h"

// Wrapper for lld.exe on Windows platform.

#define LLD_FLAVOR     "link"
#define EXECUTABLE_EXT "exe"
#define DLL_EXT        "dll"
#define LIB_EXT        "lib"
#define OBJECT_EXT     "obj"

#define FLAG_LIBPATH "/LIBPATH"
#define FLAG_OUT     "/OUT"
#define FLAG_ENTRY   "/ENTRY"
#define FLAG_DEBUG   "/DEBUG"

static const char *get_out_extension(struct assembly *assembly) {
	switch (assembly->target->kind) {
	case ASSEMBLY_EXECUTABLE:
		return EXECUTABLE_EXT;
	case ASSEMBLY_SHARED_LIB:
		return DLL_EXT;
	default:
		babort("Unknown output kind!");
	}
}

static void append_lib_paths(struct assembly *assembly, str_buf_t *buf) {
	for (usize i = 0; i < arrlenu(assembly->lib_paths); ++i) {
		str_buf_append_fmt(buf, "{s}:\"{s}\" ", FLAG_LIBPATH, assembly->lib_paths[i]);
	}
}

static void append_libs(struct assembly *assembly, str_buf_t *buf) {
	for (usize i = 0; i < arrlenu(assembly->libs); ++i) {
		struct native_lib *lib = &assembly->libs[i];
		if ((lib->flags & NATIVE_LIB_FLAG_RUNTIME) == 0) continue;
		if (!lib->user_name.len) continue;
		str_buf_append_fmt(buf, "{str}.{s} ", lib->user_name, LIB_EXT);
	}
}

static void append_default_opt(struct assembly *assembly, str_buf_t *buf) {
	const bool is_debug = assembly->target->opt == ASSEMBLY_OPT_DEBUG ||
	                      assembly->target->opt == ASSEMBLY_OPT_RELEASE_WITH_DEBUG_INFO;
	if (is_debug) str_buf_append_fmt(buf, "{s} ", FLAG_DEBUG);
	const char *default_opt = "";
	switch (assembly->target->kind) {
	case ASSEMBLY_EXECUTABLE:
		default_opt = read_config(builder.config, assembly->target, "linker_opt_exec", NULL);
		break;
	case ASSEMBLY_SHARED_LIB:
		default_opt = read_config(builder.config, assembly->target, "linker_opt_shared", NULL);
		break;
	default:
		babort("Unknown output kind!");
	}
	str_buf_append_fmt(buf, "{s} ", default_opt);
}

static void append_custom_opt(struct assembly *assembly, str_buf_t *buf) {
	const str_buf_t custom_opt = assembly->custom_linker_opt;
	if (custom_opt.len) str_buf_append_fmt(buf, "{str} ", custom_opt);
}

static void append_linker_exec(struct assembly *assembly, str_buf_t *buf) {
	const char *custom_linker = read_config(builder.config, assembly->target, "linker_executable", "");
	if (strlen(custom_linker)) {
		str_buf_append_fmt(buf, "\"{s}\" ", custom_linker);
		return;
	}
	// Use LLD as default.
	str_buf_append_fmt(buf, "\"{str}/{s}\" -flavor {s} ", builder.exec_dir, BL_LINKER, LLD_FLAVOR);
}

s32 lld_link(struct assembly *assembly) {
	runtime_measure_begin(linking);
	str_buf_t            buf     = get_tmp_str();
	const struct target *target  = assembly->target;
	const str_t          out_dir = str_buf_view(target->out_dir);
	const char          *name    = target->name;

	str_buf_append(&buf, cstr("call "));

	// set executable
	append_linker_exec(assembly, &buf);
	// set input file
	str_buf_append_fmt(&buf, "\"{str}/{s}.{s}\" ", out_dir, name, OBJECT_EXT);
	// set output file
	str_buf_append_fmt(&buf, "{s}:\"{str}/{s}.{s}\" ", FLAG_OUT, out_dir, name, get_out_extension(assembly));
	append_lib_paths(assembly, &buf);
	append_libs(assembly, &buf);
	append_default_opt(assembly, &buf);
	append_custom_opt(assembly, &buf);

	builder_log(STR_FMT, STR_ARG(buf));
	s32 state = system(str_buf_to_c(buf));
	put_tmp_str(buf);

	batomic_fetch_add_s32(&assembly->stats.linking_ms, runtime_measure_end(linking));
	return state;
}
