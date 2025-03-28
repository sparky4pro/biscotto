#include "builder.h"
#include "tokens_inline_utils.h"
#include <ctype.h>
#include <float.h>
#include <setjmp.h>
#include <string.h>

#ifdef BL_USE_SIMD
#include <emmintrin.h>
#include <intrin.h>
#endif

#define is_ident(c) (isalnum(c) || (c) == '_')

struct context {
	struct assembly      *assembly;
	struct unit          *unit;
	struct tokens        *tokens;
	struct string_cache **string_cache;
	sarr_t(char, 64) strtmp; // @Cleanup: Use tmp string from builder.
	char *c;
	s32   line;
	s32   col;

	jmp_buf jmp_error;
};

static void       scan(struct context *ctx);
static bool       scan_comment(struct context *ctx, struct token *tok, const char *term);
static bool       scan_docs(struct context *ctx, struct token *tok);
static bool       scan_ident(struct context *ctx, struct token *tok);
static bool       scan_string(struct context *ctx, struct token *tok);
static bool       scan_char(struct context *ctx, struct token *tok);
static bool       scan_number(struct context *ctx, struct token *tok);
static inline int c_to_number(char c, s32 base);

static inline u32 add_token_value(struct context *ctx, union token_value value) {
	const u32 index = (u32)arrlenu(ctx->tokens->values);
	arrput(ctx->tokens->values, value);
	return index;
}

#define report_error(ctx, code, ln, cl, len, cursor_position, format, ...)                                       \
	{                                                                                                            \
		_report((ctx), MSG_ERR, ERR_##code, (ln), (cl), (s32)(len), (cursor_position), (format), ##__VA_ARGS__); \
		longjmp((ctx)->jmp_error, ERR_##code);                                                                   \
	}                                                                                                            \
	(void)0

static inline void _report(struct context *ctx, enum builder_msg_type type, s32 code, s32 ln, s32 cl, s32 len, enum builder_cur_pos cursor_position, const char *format, ...) {
	struct location loc = {
	    .line = ln,
	    .col  = cl,
	    .len  = len,
	    .unit = ctx->unit,
	};
	va_list args;
	va_start(args, format);
	builder_vmsg(type, code, &loc, cursor_position, format, args);
	va_end(args);
}

bool scan_comment(struct context *ctx, struct token *tok, const char *termminator) {
	if (tok->sym == SYM_SHEBANG && ctx->line != 1) {
		report_error(ctx, INVALID_TOKEN, ctx->line, ctx->col, 1, CARET_WORD, "Shebang is allowed only on the first line of the file.");
	}

	const s32 start_ln = ctx->line;
	const s32 start_cl = ctx->col;

	const usize terminator_len = strlen(termminator);

	while (true) {
		if (*ctx->c == '\n') {
			ctx->line++;
			ctx->col = 1;
		} else if (*ctx->c == SYM_EOF) {
			if (strcmp(termminator, "\n") == 0) {
				return true;
			}

			report_error(ctx, UNTERMINATED_COMMENT, start_ln, start_cl, 1, CARET_WORD, "Unterminated comment block starting here:");
		}

		if (strncmp(ctx->c, termminator, terminator_len) == 0) break;

		ctx->c++;
	}

	ctx->c += terminator_len;
	return true;
}

bool scan_docs(struct context *ctx, struct token *tok) {
	bassert(token_is(tok, SYM_DCOMMENT) || token_is(tok, SYM_DGCOMMENT));
	tok->location.line = ctx->line;
	tok->location.col  = ctx->col;

	sarrclear(&ctx->strtmp);
	char *begin      = ctx->c;
	s32   len_parsed = 0;
	s32   len_str    = 0;
	while (true) {
		if (*ctx->c == SYM_EOF) break;
		if (*ctx->c == '\r' || *ctx->c == '\n') break;
		if (!len_parsed && *ctx->c == ' ') {
			++begin;
		} else {
			len_str++;
		}
		len_parsed++;
		ctx->c++;
	}

	str_t str        = scdup2(ctx->string_cache, make_str(begin, len_str));
	tok->value_index = add_token_value(ctx, (union token_value){.str = str});

	tok->location.len = len_parsed + 3; // + 3 = '///'
	ctx->col += len_parsed;
	return true;
}

bool scan_ident(struct context *ctx, struct token *tok) {
	zone();
	tok->location.line = ctx->line;
	tok->location.col  = ctx->col;
	tok->sym           = SYM_IDENT;

	char *begin = ctx->c;

	s32 len = 0;

#ifdef BL_USE_SIMD
	while (true) {
		// Process by 16 chars
		const __m128i src = _mm_loadu_si128((__m128i *)(begin + len));

		// Convert to uppercase
		const __m128i range_shift = _mm_sub_epi8(src, _mm_set1_epi8((char)('a' + 128)));
		const __m128i no_modify   = _mm_cmpgt_epi8(range_shift, _mm_set1_epi8(-128 + 25));
		const __m128i flip        = _mm_andnot_si128(no_modify, _mm_set1_epi8(0x20));
		const __m128i src_upper   = _mm_xor_si128(src, flip);

		// Check for characters from A to Z.
		const __m128i a = _mm_set1_epi8('A' - 1);
		const __m128i z = _mm_set1_epi8('Z' + 1);

		const __m128i gta      = _mm_cmpgt_epi8(src_upper, a);
		const __m128i ltz      = _mm_cmplt_epi8(src_upper, z);
		__m128i       is_alpha = _mm_and_si128(gta, ltz);

		// Check for underscore.
		const __m128i underscore = _mm_cmpeq_epi8(src_upper, _mm_set1_epi8('_'));
		is_alpha                 = _mm_or_si128(is_alpha, underscore);

		// Check for numbers
		const __m128i zero = _mm_set1_epi8('0' - 1);
		const __m128i nine = _mm_set1_epi8('9' + 1);

		const __m128i gtzero    = _mm_cmpgt_epi8(src_upper, zero);
		const __m128i ltnine    = _mm_cmplt_epi8(src_upper, nine);
		const __m128i is_number = _mm_and_si128(gtzero, ltnine);

		// Finalize
		const __m128i is_ident = _mm_or_si128(is_alpha, is_number);

		const int mask = _mm_movemask_epi8(is_ident) ^ 0xFFFF;
		int       advance;
		if (mask) {
			advance = _tzcnt_u32(mask);
		} else {
			advance = 16;
		}

		len += advance;
		if (mask) break;
	}

	// Shift the global cursor.
	ctx->c += len;
#else
	while (true) {
		if (!is_ident(*ctx->c)) {
			break;
		}

		len++;
		ctx->c++;
	}
#endif

	if (len == 0) return_zone(false);
	// Note that we use the string identificators directly (no copy is done). That means those might
	// not to be zero terminated! This way we reduce amount of string duplication.
	str_t str         = make_str(begin, len);
	tok->value_index  = add_token_value(ctx, (union token_value){.str = str});
	tok->location.len = len;
	ctx->col += len;
	return_zone(true);
}

static char scan_specch(struct context *ctx) {
	++ctx->col;
	s32 start_col = ctx->col;

	const char c = *ctx->c++;
	switch (c) {
	case 'n':
		return '\n';
	case 'r':
		return '\r';
	case 't':
		return '\t';
	case '\"':
		return '\"';
	case '\'':
		return '\'';
	case '\\':
		return '\\';
	case 'x':
	case '0': {
		u64       v                = 0;
		const s32 base             = c == 'x' ? 16 : 8;
		bool      is_out_of_bounds = false;

		while (true) {
			const s32 d = c_to_number(*ctx->c, base);
			if (d == -1) break;
			v = v * base + d;
			if (v > 0xFF && !is_out_of_bounds) {
				is_out_of_bounds = true;
			}
			++ctx->c;
			++ctx->col;
		}

		if (is_out_of_bounds) {
			const s32 len = ctx->col - start_col;

			if (base == 16) {
				report_error(
				    ctx,
				    NUM_LIT_OVERFLOW,
				    ctx->line,
				    start_col,
				    len,
				    CARET_WORD,
				    "Hexadecimal character notation overflow, maximum possible value is 0xFF.",
				    v);
			} else if (base == 8) {
				report_error(
				    ctx,
				    NUM_LIT_OVERFLOW,
				    ctx->line,
				    start_col,
				    len,
				    CARET_WORD,
				    "Octal character notation overflow, maximum possible value is 0377.",
				    v);
			} else {
				babort("Invalid number base.");
			}
		}
		return (char)v;
	}

	default:
		return *ctx->c;
	}
}

bool scan_string(struct context *ctx, struct token *tok) {
	if (*ctx->c != '\"') return false;

	zone();
	sarrclear(&ctx->strtmp);
	tok->location.line = ctx->line;
	tok->location.col  = ctx->col;
	tok->sym           = SYM_STRING;
	char c;

	s32 start_col = ctx->col;

	// eat "
	++ctx->c;
	++ctx->col;

	while (true) {
		switch (*ctx->c) {
		case '\"': {
			++ctx->col;
			++ctx->c;
			goto DONE;
		}
		case SYM_EOF: {
			report_error(ctx, UNTERMINATED_STRING, ctx->line, start_col, 1, CARET_WORD, "Unterminated string.");
		}
		case '\\':
			++ctx->c; // Eat backslash.
			++ctx->col;
			c = scan_specch(ctx);
			break;

		default:
			c = *ctx->c;
			++ctx->col;
			++ctx->c;
		}
		sarrput(&ctx->strtmp, c);
	}

DONE: {
	str_t str         = scdup2(ctx->string_cache, make_str(sarrdata(&ctx->strtmp), sarrlenu(&ctx->strtmp)));
	tok->value_index  = add_token_value(ctx, (union token_value){.str = str});
	tok->location.len = ctx->col - start_col;
	return_zone(true);
}
}

bool scan_char(struct context *ctx, struct token *tok) {
	if (*ctx->c != '\'') return false;
	tok->location.line = ctx->line;
	tok->location.col  = ctx->col;
	tok->location.len  = 1;
	tok->sym           = SYM_CHAR;

	// eat '
	++ctx->c;
	++ctx->col;

	switch (*ctx->c) {
	case '\'': {
		report_error(ctx, EMPTY, ctx->line, ctx->col, 2, CARET_WORD, "Expected character in quotes.");
	}
	case '\0': {
		report_error(ctx, UNTERMINATED_STRING, ctx->line, ctx->col, 1, CARET_WORD, "Unterminated character.");
	}
	case '\\': {
		// special character
		s32 start_col = ctx->col;

		++ctx->c; // Eat backslash.
		++ctx->col;

		tok->value_index = add_token_value(ctx, (union token_value){
		                                            .character = scan_specch(ctx),
		                                        });

		tok->location.len += ctx->col - start_col;
		break;
	}
	default:
		tok->value_index = add_token_value(ctx, (union token_value){.character = *ctx->c});
		tok->location.len += 1;
		ctx->c++;
	}

	// eat '
	if (*ctx->c != '\'') {
		report_error(ctx, UNTERMINATED_STRING, ctx->line, ctx->col, 1, CARET_WORD, "Unterminated character.");
	}
	tok->location.len += 1;
	++ctx->c;

	return true;
}

inline s32 c_to_number(char c, s32 base) {
#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
	switch (base) {
	case 16:
		if (c >= 'a' && c <= 'f') {
			return c - 'a' + 10;
		}

		if (c >= 'A' && c <= 'F') {
			return c - 'A' + 10;
		}
	case 10:
		if (c == '8' || c == '9') {
			return c - '0';
		}
	case 8:
		if (c >= '2' && c <= '7') {
			return c - '0';
		}
	case 2:
		if (c == '0' || c == '1') {
			return c - '0';
		}
		break;
	default:
		babort("Invalid number base.");
	}

	return -1;
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif
}

static bool is_real(char *c) {
	while (*c != SYM_EOF) {
		if (*c == '.') return true;
		if (!isdigit(*c)) return false;
		++c;
	}
	return false;
}

bool scan_number(struct context *ctx, struct token *tok) {
	tok->location.line = ctx->line;
	tok->location.col  = ctx->col;

	s32 start_col = ctx->col;

	// Prescan to findout if we deal with floating point number.
	const bool is_floating_point = is_real(ctx->c);

	if (is_floating_point) {

		double n        = 0.0;
		double fraction = 0.0;
		double divider  = 1.0;

		if (!isdigit(*(ctx->c)) && *(ctx->c) != '.') {
			return false;
		}

		// Integral part.
		while (true) {
			s32 d = c_to_number(*(ctx->c), 10);
			if (d == -1) break;
			n = n * 10.0 + (double)d;
			++ctx->c;
			++ctx->col;
		}

		bassert(*(ctx->c) == '.' && "Expected floating point dot character!");
		++ctx->c;
		++ctx->col;

		// Fractional part.
		while (true) {
			s32 d = c_to_number(*(ctx->c), 10);
			if (d == -1) break;

			fraction = fraction * 10.0 + (double)d;
			divider *= 10.0;
			++ctx->c;
			++ctx->col;
		}

		// Exponent.
		s32 exp_sign = 0;
		u64 exp      = 0;

		if (*(ctx->c) == 'e' || *(ctx->c) == 'E') {
			++ctx->c;
			++ctx->col;

			if (*(ctx->c) == '-') {
				exp_sign = -1;
			} else if (*(ctx->c) == '+') {
				exp_sign = 1;
			} else {
				report_error(ctx, INVALID_TOKEN, ctx->line, ctx->col, 1, CARET_WORD, "Expected sign of floating point number exponent.");
			}

			++ctx->c;
			++ctx->col;

			bool      overflow = false;
			const u64 max_div  = ULLONG_MAX / 10;
			const u64 max_mod  = ULLONG_MAX % 10;

			const s32 exp_start_col = ctx->col;

			while (true) {
				s32 d = c_to_number(*(ctx->c), 10);
				if (d == -1) break;

				if (exp > max_div || (exp == max_div && (u64)d > max_mod)) {
					exp      = 1;
					overflow = true;
				} else {
					exp = exp * 10 + d;
				}

				++ctx->c;
				++ctx->col;
			}

			const s32 exp_len = ctx->col - exp_start_col;
			if (exp_len < 1) {
				report_error(ctx, INVALID_TOKEN, ctx->line, exp_start_col, 1, CARET_WORD, "Expected floating point exponent value.");
			}

			if (overflow) {
				report_error(ctx, NUM_LIT_OVERFLOW, ctx->line, exp_start_col, exp_len, CARET_WORD, "Exponent value is too big.");
			}
		}

		// Suffix.
		if (*(ctx->c) == 'f') {
			++ctx->c;
			++ctx->col;

			tok->sym = SYM_FLOAT;
		} else {
			tok->sym = SYM_DOUBLE;
		}

		double number = n + fraction / divider;
		if (exp_sign != 0) {
			number *= pow(10.0, (double)exp_sign * (double)exp);
		}
		tok->value_index = add_token_value(ctx, (union token_value){.double_number = number});

		tok->location.len = ctx->col - start_col;

		return true;

	} else {

		s32 base = 10;

		if (strncmp(ctx->c, "0x", 2) == 0) {
			base = 16;
			ctx->c += 2;
			ctx->col += 2;
		} else if (strncmp(ctx->c, "0b", 2) == 0) {
			base = 2;
			ctx->c += 2;
			ctx->col += 2;
		} else if (*ctx->c == '0') {
			base = 8;
			++ctx->c;
			++ctx->col;
		}

		bool      overflow = false;
		const u64 max_div  = ULLONG_MAX / base;

		u64 n = 0;

		while (true) {
			s32 d = c_to_number(*(ctx->c), base);
			if (d == -1) break;

			if (n > max_div || (n == max_div && (u64)d > ULLONG_MAX % base)) {
				n        = ULLONG_MAX;
				overflow = true;
			} else {
				n = n * base + d;
			}

			++ctx->c;
			++ctx->col;
		}

		const s32 len = ctx->col - start_col;
		if (len < 1) return false;

		tok->location.len = len;
		tok->sym          = SYM_NUM;
		tok->value_index  = add_token_value(ctx, (union token_value){.number = n});

		if (overflow) {
			builder_msg(MSG_ERR, ERR_NUM_LIT_OVERFLOW, &tok->location, CARET_WORD, "Number literal is too big for u64 type.");
		}

		return true;
	}

	return false;
}

void scan(struct context *ctx) {
	struct token tok = {0};
SCAN:
	tok.location.line = ctx->line;
	tok.location.col  = ctx->col;

	// Ignored characters
	switch (*ctx->c) {
	case '\0':
		tok.sym = SYM_EOF;
		tokens_push(ctx->tokens, tok);
		return;
	case '\r':
		ctx->c++;
		goto SCAN;
	case '\n':
		ctx->line++;
		ctx->col = 1;
		ctx->c++;
		goto SCAN;
	case '\t':
		ctx->col += 1;
		ctx->c++;
		goto SCAN;
	case ' ':
		ctx->col++;
		ctx->c++;
		goto SCAN;
	default:
		break;
	}

	// Scan symbols described directly as strings.
	usize      len                   = 0;
	const bool is_current_char_ident = is_ident(*ctx->c);
	const s32  start                 = is_current_char_ident ? SYM_IF : SYM_UNREACHABLE + 1;
	const s32  end                   = is_current_char_ident ? SYM_UNREACHABLE + 1 : SYM_NONE;
	for (s32 i = start; i < end; ++i) {
		len = sym_lens[i];
		bassert(len > 0);
		if (ctx->c[0] == sym_strings[i][0] && strncmp(ctx->c, sym_strings[i], len) == 0) {
			ctx->c += len;
			tok.sym          = (enum sym)i;
			tok.location.len = (s32)len;

			// Two joined symbols will be parsed as identifier.
			if (i >= SYM_IF && i <= SYM_UNREACHABLE && is_ident(*ctx->c)) {
				// roll back
				ctx->c -= len;
				break;
			}

#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
			switch (tok.sym) {
			case SYM_DCOMMENT:
			case SYM_DGCOMMENT:
				if (ctx->assembly && ctx->assembly->target->kind == ASSEMBLY_DOCS) {
					scan_docs(ctx, &tok);
					goto PUSH_TOKEN;
				}
			case SYM_SHEBANG:
			case SYM_LCOMMENT:
				scan_comment(ctx, &tok, "\n");
				goto SCAN;
			case SYM_LBCOMMENT:
				scan_comment(ctx, &tok, sym_strings[SYM_RBCOMMENT]);
				goto SCAN;
			case SYM_RBCOMMENT: {
				report_error(ctx, INVALID_TOKEN, ctx->line, ctx->col, 1, CARET_WORD, "Unexpected end of the comment block.");
			}
			default:
				ctx->col += (s32)len;
				goto PUSH_TOKEN;
			}
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif
		}
	}

	// Scan special tokens.
	if (scan_number(ctx, &tok)) goto PUSH_TOKEN;
	if (scan_ident(ctx, &tok)) goto PUSH_TOKEN;
	if (scan_string(ctx, &tok)) goto PUSH_TOKEN;
	if (scan_char(ctx, &tok)) goto PUSH_TOKEN;

	// When symbol is unknown report error
	report_error(ctx, INVALID_TOKEN, ctx->line, ctx->col, 1, CARET_WORD, "Unexpected symbol.");
PUSH_TOKEN:
	tok.location.unit = ctx->unit;
	tokens_push(ctx->tokens, tok);

	goto SCAN;
}

void lexer_run(struct assembly *assembly, struct unit *unit) {
	runtime_measure_begin(lex);

	const u32 thread_index = get_worker_index();

	struct context ctx = {
	    .assembly     = assembly,
	    .tokens       = &unit->tokens,
	    .unit         = unit,
	    .c            = unit->src,
	    .line         = 1,
	    .col          = 1,
	    .strtmp       = SARR_ZERO,
	    .string_cache = &assembly->thread_local_contexts[thread_index].string_cache,
	};

	zone();
	s32 error = 0;
	if ((error = setjmp(ctx.jmp_error))) {
		sarrfree(&ctx.strtmp);
		batomic_fetch_add_s32(&assembly->stats.lexing_ms, runtime_measure_end(lex));
		return_zone();
	}

	scan(&ctx);
	sarrfree(&ctx.strtmp);

	batomic_fetch_add_s32(&builder.total_lines, ctx.line);
	batomic_fetch_add_s32(&assembly->stats.lexing_ms, runtime_measure_end(lex));
	return_zone();
}
