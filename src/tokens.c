#include "tokens.h"
#include "stb_ds.h"

char *sym_strings[] = {
#define sm(tok, str, len) str,
#include "tokens.def"
#undef sm
};

s32 sym_lens[] = {
#define sm(tok, str, len) len,
#include "tokens.def"
#undef sm
};

struct token *token_end = &(struct token){.sym = SYM_EOF};

void tokens_init(struct tokens *tokens) {
	tokens->buf  = NULL;
	tokens->iter = 0;
	arrsetcap(tokens->buf, 2048);
	arrsetcap(tokens->values, 256);
}

void tokens_terminate(struct tokens *tokens) {
	arrfree(tokens->buf);
	arrfree(tokens->values);
}

bool token_is_unary(struct token *token) {
	switch (token->sym) {
	case SYM_MINUS:
	case SYM_PLUS:
	case SYM_NOT:
	case SYM_BIT_NOT:
		return true;
	default:
		return false;
	}
}

struct token_precedence token_prec(struct token *token) {
	switch (token->sym) {
		// . [ (
	case SYM_DOT:
	case SYM_LBRACKET:
	case SYM_LPAREN:
		return (struct token_precedence){.priority = 60, .associativity = TOKEN_ASSOC_LEFT};

		// cast sizeof alignof typeinfo
	case SYM_CAST:
	case SYM_TESTCASES:
	case SYM_NOT:
	case SYM_BIT_NOT:
		return (struct token_precedence){.priority = 50, .associativity = TOKEN_ASSOC_RIGHT};

		// * ^ / % @
	case SYM_ASTERISK:
	case SYM_SLASH:
	case SYM_PERCENT:
	case SYM_AT:
		return (struct token_precedence){.priority = 40, .associativity = TOKEN_ASSOC_LEFT};

		// + -
	case SYM_PLUS:
	case SYM_MINUS:
		return (struct token_precedence){.priority = 20, .associativity = TOKEN_ASSOC_LEFT};

		// >> <<
	case SYM_SHR:
	case SYM_SHL:
		return (struct token_precedence){.priority = 18, .associativity = TOKEN_ASSOC_LEFT};

		// < > <= >=
	case SYM_LESS:
	case SYM_GREATER:
	case SYM_LESS_EQ:
	case SYM_GREATER_EQ:
		return (struct token_precedence){.priority = 15, .associativity = TOKEN_ASSOC_LEFT};

		// == !=
	case SYM_EQ:
	case SYM_NEQ:
		return (struct token_precedence){.priority = 10, .associativity = TOKEN_ASSOC_LEFT};

		// &
	case SYM_AND:
		return (struct token_precedence){.priority = 9, .associativity = TOKEN_ASSOC_LEFT};

		// ^
	case SYM_XOR:
		return (struct token_precedence){.priority = 8, .associativity = TOKEN_ASSOC_LEFT};

		// |
	case SYM_OR:
		return (struct token_precedence){.priority = 7, .associativity = TOKEN_ASSOC_LEFT};

		// &&
	case SYM_LOGIC_AND:
		return (struct token_precedence){.priority = 5, .associativity = TOKEN_ASSOC_LEFT};

		// ||
	case SYM_LOGIC_OR:
		return (struct token_precedence){.priority = 4, .associativity = TOKEN_ASSOC_LEFT};

		// = += -= *= /=
	case SYM_ASSIGN:
	case SYM_PLUS_ASSIGN:
	case SYM_MINUS_ASSIGN:
	case SYM_ASTERISK_ASSIGN:
	case SYM_SLASH_ASSIGN:
	case SYM_PERCENT_ASSIGN:
	case SYM_AND_ASSIGN:
	case SYM_OR_ASSIGN:
	case SYM_XOR_ASSIGN:
		return (struct token_precedence){.priority = 3, .associativity = TOKEN_ASSOC_RIGHT};

	default:
		return (struct token_precedence){.priority = 0, .associativity = TOKEN_ASSOC_NONE};
	}
}
