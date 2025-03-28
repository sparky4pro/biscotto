#include "builder.h"
#include "stb_ds.h"

#if !BL_PLATFORM_WIN
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

typedef s32 (*LinkerFn)(struct assembly *);

s32 lld_link(struct assembly *assembly);
s32 lld_ld(struct assembly *assembly);

static void copy_user_libs(struct assembly *assembly) {
	str_buf_t            dest_path = get_tmp_str();
	const struct target *target    = assembly->target;
	const str_t          out_dir   = str_buf_view(target->out_dir);
	for (usize i = 0; i < arrlenu(assembly->libs); ++i) {
		struct native_lib *lib = &assembly->libs[i];
		if ((lib->flags & NATIVE_LIB_FLAG_RUNTIME) == 0) continue;
		if ((lib->flags & NATIVE_LIB_IS_SYSTEM) == NATIVE_LIB_IS_SYSTEM) continue;
		if (!lib->user_name.len) continue;

		str_buf_clr(&dest_path);
		str_t lib_dest_name = lib->filename;
#if BL_PLATFORM_LINUX || BL_PLATFORM_MACOS
		struct stat statbuf;
		str_buf_t   tmp_filepath  = get_tmp_str();
		const char *clib_filepath = str_to_c(&tmp_filepath, lib->filepath);
		lstat(clib_filepath, &statbuf);
		if (S_ISLNK(statbuf.st_mode)) {
			char buf[PATH_MAX] = {0};
			if (readlink(clib_filepath, buf, static_arrlenu(buf)) == -1) {
				builder_error("Cannot follow symlink '" STR_FMT "' with error: %d",
				              STR_ARG(lib->filepath),
				              errno);
				put_tmp_str(tmp_filepath);
				continue;
			}
			lib_dest_name = make_str_from_c(buf);
		}
		put_tmp_str(tmp_filepath);
#endif

		str_buf_append_fmt(&dest_path, "{str}/{str}", out_dir, lib_dest_name);
		if (file_exists(dest_path)) continue;

		builder_info("Copy '" STR_FMT "' to '" STR_FMT "'.", STR_ARG(lib->filepath), STR_ARG(dest_path));

		if (!copy_file(lib->filepath, str_buf_view(dest_path))) {
			builder_error("Cannot copy '" STR_FMT "' to '" STR_FMT "'.", STR_ARG(lib->filepath), STR_ARG(dest_path));
		}
	}
	put_tmp_str(dest_path);
}

void native_bin_run(struct assembly *assembly) {
	builder_log("Running native runtime linker...");
	LinkerFn linker = NULL;
#if BL_PLATFORM_WIN
	linker = &lld_link;
#elif BL_PLATFORM_LINUX || BL_PLATFORM_MACOS
	linker = &lld_ld;
#else
#error "Unknown platform"
#endif

	const str_t out_dir = str_buf_view(assembly->target->out_dir);
	zone();
	if (linker(assembly) != 0) {
		builder_msg(MSG_ERR, ERR_LIB_NOT_FOUND, NULL, CARET_WORD, "Native link execution failed.");
		goto DONE;
	}

	if (assembly->target->copy_deps) {
		builder_log("Copy assembly dependencies into '" STR_FMT "'.", STR_ARG(out_dir));
		copy_user_libs(assembly);
	}
DONE:
	return_zone();
}
