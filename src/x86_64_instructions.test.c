// Testing of x64 instruction encoding.

#define STB_DS_IMPLEMENTATION

#undef BL_DEBUG
#undef BL_ASSERT_ENABLE

#include "x86_64_instructions.h"
#include "stb_ds.h"
#include <stdio.h>

struct thread_context {
	u8 *code;
};

void add_code(struct thread_context *tctx, const void *buf, s32 len) {
	const u64 i = arrlenu(tctx->code);
	arrsetlen(tctx->code, i + len);
	memcpy(&tctx->code[i], buf, len);
}

enum x64_register get_temporary_register(struct thread_context *tctx, const enum x64_register exclude[], s32 exclude_num) {
	return RAX;
}

static void test(struct thread_context *tctx, const char *instr, u8 expected[], u32 expected_num) {
	u32  l              = (u32)arrlenu(tctx->code);
	bool print_encoding = false;
	if (l != expected_num) {
		printf("[FAILED]: %s Invalid len of instruction, expected: %u, got: %u.", instr, expected_num, l);
		print_encoding = true;
	} else {
		for (u32 i = 0; i < l; ++i) {
			if (expected[i] != tctx->code[i]) {
				printf("[FAILED]: %s Invalid encoding of instruction.", instr);

				print_encoding = true;
				break;
			}
		}
	}

	if (print_encoding) {
		printf("\n\tExpected: ");
		for (u32 j = 0; j < expected_num; ++j) {
			printf("0x%02X ", expected[j]);
		}
		printf("\n\tGot:      ");
		for (u32 j = 0; j < l; ++j) {
			printf("0x%02X ", tctx->code[j]);
		}
		printf("\n");
	} else {
		printf("[  OK  ]: %s\n", instr);
	}
}

static void clear(struct thread_context *tctx) {
	arrsetlen(tctx->code, 0);
}

#define _GET_NTH_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, N, ...) N
#define COUNT_ARGS(...)                                                                                                 _GET_NTH_ARG(__VA_ARGS__, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)

#define TEST(instr, ...)                                            \
	(instr);                                                        \
	test(&t, #instr, (u8[]){__VA_ARGS__}, COUNT_ARGS(__VA_ARGS__)); \
	clear(&t)

int main(s32 argc, char *argv[]) {
	struct thread_context t = {0};

	TEST(nop(&t), 0x90);

	TEST(push64_r(&t, RBP), 0x55);
	TEST(push64_r(&t, RAX), 0x50);
	TEST(push64_r(&t, RCX), 0x51);
	TEST(push64_r(&t, RDX), 0x52);
	TEST(push64_r(&t, R8), 0x41, 0x50);
	TEST(push64_r(&t, R9), 0x41, 0x51);
	TEST(push64_r(&t, R10), 0x41, 0x52);
	TEST(push64_r(&t, R11), 0x41, 0x53);

	TEST(pop64_r(&t, RBP), 0x5d);
	TEST(pop64_r(&t, RAX), 0x58);
	TEST(pop64_r(&t, RCX), 0x59);
	TEST(pop64_r(&t, RDX), 0x5a);
	TEST(pop64_r(&t, R8), 0x41, 0x58);
	TEST(pop64_r(&t, R9), 0x41, 0x59);
	TEST(pop64_r(&t, R10), 0x41, 0x5a);
	TEST(pop64_r(&t, R11), 0x41, 0x5b);

	TEST(lea_rm(&t, RAX, RBP, 10, 8), 0x48, 0x8D, 0x45, 0x0A);
	TEST(lea_rm(&t, RCX, RBP, 10, 8), 0x48, 0x8D, 0x4d, 0x0A);
	TEST(lea_rm(&t, RDX, RBP, 10, 8), 0x48, 0x8D, 0x55, 0x0A);
	TEST(lea_rm(&t, R8, RBP, 10, 8), 0x4c, 0x8D, 0x45, 0x0A);
	TEST(lea_rm(&t, R9, RBP, 10, 8), 0x4c, 0x8D, 0x4d, 0x0A);
	TEST(lea_rm(&t, R10, RBP, 10, 8), 0x4c, 0x8D, 0x55, 0x0A);
	TEST(lea_rm(&t, R11, RBP, 10, 8), 0x4c, 0x8D, 0x5d, 0x0A);
	TEST(lea_rm(&t, RAX, RBP, -1, 8), 0x48, 0x8D, 0x45, 0xFF);
	TEST(lea_rm(&t, RAX, RBP, 256, 8), 0x48, 0x8D, 0x85, 0x00, 0x01, 0x00, 0x00);
	TEST(lea_rm(&t, RAX, RBP, 256, 4), 0x8D, 0x85, 0x00, 0x01, 0x00, 0x00);

	TEST(lea_rm_indirect(&t, RAX, 0, 8), 0x48, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00);
	TEST(lea_rm_indirect(&t, R8, 0, 8), 0x4C, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00);
	TEST(lea_rm_indirect(&t, RAX, 0, 4), 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00);

	TEST(movabs64_ri(&t, RAX, 0), 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	TEST(movabs64_ri(&t, RAX, 10), 0x48, 0xB8, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	TEST(movabs64_ri(&t, RAX, 0xFFFFFFFFFF), 0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00);
	TEST(movabs64_ri(&t, RCX, 0xFFFFFFFFFF), 0x48, 0xB9, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00);
	TEST(movabs64_ri(&t, R8, 0xFFFFFFFFFF), 0x49, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00);

	TEST(mov_mr(&t, RBP, 10, RAX, 8), 0x48, 0x89, 0x45, 0x0A);
	TEST(mov_mr(&t, RBP, 10, R8, 8), 0x4C, 0x89, 0x45, 0x0A);
	TEST(mov_mr(&t, RBP, 10, RAX, 4), 0x89, 0x45, 0x0A);
	TEST(mov_mr(&t, RBP, 10, RAX, 2), 0x66, 0x89, 0x45, 0x0A);
	TEST(mov_mr(&t, RBP, 10, RAX, 1), 0x88, 0x45, 0x0A);

	TEST(mov_mi(&t, RBP, 10, 0xFF, 8), 0x48, 0xC7, 0x45, 0x0A, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_mi(&t, RBP, 10, 0x7FFFFFFF, 8), 0x48, 0xC7, 0x45, 0x0A, 0xFF, 0xFF, 0xFF, 0x7F);
	TEST(mov_mi(&t, RBP, 10, 0xFFFFFFFF, 8), 0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x48, 0x89, 0x45, 0x0A);

	TEST(mov_mi_sib(&t, RBP, RAX, 0, 10, 8), 0x48, 0xC7, 0x44, 0x05, 0x00, 0x0A, 0x00, 0x00, 0x00);
	TEST(mov_mi_sib(&t, RBP, RAX, 0xFF, 10, 8), 0x48, 0xC7, 0x84, 0x05, 0xFF, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00);
	TEST(mov_mi_sib(&t, RBP, R8, 0xFF, 10, 8), 0x4A, 0xC7, 0x84, 0x05, 0xFF, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00);
	TEST(mov_mi_sib(&t, RSP, R8, 0xFF, 10, 8), 0x4A, 0xC7, 0x84, 0x04, 0xFF, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00);
	TEST(mov_mi_sib(&t, RBP, R8, 0xFF, 0x7FFFFFFF, 8), 0x4A, 0xC7, 0x84, 0x05, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x7F);
	TEST(mov_mi_sib(&t, RBP, R8, 0xFF, 0xFFFFFFFF, 8), 0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x4A, 0x89, 0x84, 0x05, 0xFF, 0x00, 0x00, 0x00);

	TEST(mov_mr_sib(&t, RBP, R8, 0xFF, RAX, 8), 0x4A, 0x89, 0x84, 0x05, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_mr_sib(&t, RBP, RAX, 0xFF, RDX, 8), 0x48, 0x89, 0x94, 0x05, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_mr_sib(&t, RBP, RAX, 0xFF, R8, 8), 0x4C, 0x89, 0x84, 0x05, 0xFF, 0x00, 0x00, 0x00);

	TEST(mov_mi_indirect(&t, 0, 0xFF, 8), 0x48, 0xC7, 0x05, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00);
	// TEST(mov_mi_indirect(&t, 0, 0xFFFFFFFF, 8), 0x48, 0xC7, 0x05, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00);

	TEST(mov_ri(&t, RAX, 0xFF, 8), 0x48, 0xC7, 0xC0, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_ri(&t, R8, 0xFF, 8), 0x49, 0xC7, 0xC0, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_ri(&t, RAX, 0xFFFFFFFF, 8), 0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00);
	TEST(mov_ri(&t, RAX, 0xFF, 4), 0xB8, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_ri(&t, RAX, 0xFF, 2), 0x66, 0xB8, 0xFF, 0x00);
	TEST(mov_ri(&t, RDX, 0xFF, 4), 0xBA, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_ri(&t, R8, 0xFF, 4), 0x41, 0xB8, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_ri(&t, RAX, 0xFF, 1), 0xB0, 0xFF);
	TEST(mov_ri(&t, RCX, 0xFF, 1), 0xB1, 0xFF);
	TEST(mov_ri(&t, RDX, 0xFF, 1), 0xB2, 0xFF);

	TEST(mov_rm(&t, RAX, RBP, 0xFF, 8), 0x48, 0x8B, 0x85, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_rm(&t, RAX, RBP, 0x7FFFFFFF, 8), 0x48, 0x8B, 0x85, 0xFF, 0xFF, 0xFF, 0x7F);
	TEST(mov_rm(&t, RAX, RBP, 0xFF, 4), 0x8B, 0x85, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_rm(&t, RAX, RBP, 0xFF, 2), 0x66, 0x8B, 0x85, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_rm(&t, RAX, RBP, 0xFF, 1), 0x8A, 0x85, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_rm(&t, RAX, RAX, 0xFF, 8), 0x48, 0x8B, 0x80, 0xFF, 0x00, 0x00, 0x00);

	TEST(mov_rm_indirect(&t, RAX, 0, 8), 0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00);
	TEST(mov_rm_indirect(&t, R8, 0xFF, 8), 0x4C, 0x8B, 0x05, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_rm_indirect(&t, RAX, 0, 4), 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00);
	TEST(mov_rm_indirect(&t, RAX, 0, 2), 0x66, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00);
	TEST(mov_rm_indirect(&t, RAX, 0, 1), 0x8A, 0x05, 0x00, 0x00, 0x00, 0x00);

	TEST(mov_mr_indirect(&t, 0xFF, RAX, 8), 0x48, 0x89, 0x05, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_mr_indirect(&t, 0xFF, RAX, 4), 0x89, 0x05, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_mr_indirect(&t, 0xFF, RAX, 2), 0x66, 0x89, 0x05, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_mr_indirect(&t, 0xFF, RAX, 1), 0x88, 0x05, 0xFF, 0x00, 0x00, 0x00);

	TEST(mov_rr(&t, RAX, RCX, 8), 0x48, 0x89, 0xC8);
	TEST(mov_rr(&t, RAX, R8, 8), 0x4C, 0x89, 0xC0);
	TEST(mov_rr(&t, RAX, RCX, 4), 0x89, 0xC8);
	TEST(mov_rr(&t, RAX, RCX, 2), 0x66, 0x89, 0xC8);
	TEST(mov_rr(&t, RAX, RCX, 1), 0x88, 0xC8);

	TEST(xor_rr(&t, RAX, RCX, 8), 0x48, 0x31, 0xC8);
	TEST(xor_rr(&t, RAX, RCX, 4), 0x31, 0xC8);
	TEST(xor_rr(&t, RAX, RCX, 2), 0x66, 0x31, 0xC8);
	TEST(xor_rr(&t, RAX, RCX, 1), 0x30, 0xC8);

	TEST(add_rr(&t, RAX, RCX, 8), 0x48, 0x01, 0xC8);
	TEST(add_rr(&t, RAX, RCX, 4), 0x01, 0xC8);
	TEST(add_rr(&t, RAX, RCX, 2), 0x66, 0x01, 0xC8);
	TEST(add_rr(&t, RAX, RCX, 1), 0x00, 0xC8);

	TEST(add_ri(&t, RAX, 0xFF, 8), 0x48, 0x05, 0xff, 0x00, 0x00, 0x00);
	TEST(add_ri(&t, RSP, 0xFF, 8), 0x48, 0x81, 0xC4, 0xFF, 0x00, 0x00, 0x00);
	TEST(add_ri(&t, R8, 0xFF, 8), 0x49, 0x81, 0xC0, 0xFF, 0x00, 0x00, 0x00);
	TEST(add_ri(&t, RAX, 0xFF, 4), 0x05, 0xFF, 0x00, 0x00, 0x00);
	TEST(add_ri(&t, RAX, 0xFF, 2), 0x66, 0x05, 0xFF, 0x00);
	TEST(add_ri(&t, RAX, 0xFF, 1), 0x04, 0xFF);

	TEST(sub_rr(&t, RAX, RCX, 8), 0x48, 0x29, 0xC8);
	TEST(sub_rr(&t, RAX, RCX, 4), 0x29, 0xC8);
	TEST(sub_rr(&t, RAX, RCX, 2), 0x66, 0x29, 0xC8);
	TEST(sub_rr(&t, RAX, RCX, 1), 0x28, 0xC8);

	TEST(sub_ri(&t, RAX, 0xFF, 8), 0x48, 0x2D, 0xFF, 0x00, 0x00, 0x00);
	TEST(sub_ri(&t, RSP, 0xFF, 8), 0x48, 0x81, 0xEC, 0xFF, 0x00, 0x00, 0x00);
	TEST(sub_ri(&t, R8, 0xFF, 8), 0x49, 0x81, 0xE8, 0xFF, 0x00, 0x00, 0x00);
	TEST(sub_ri(&t, RAX, 0xFF, 4), 0x2D, 0xFF, 0x00, 0x00, 0x00);
	TEST(sub_ri(&t, RAX, 0xFF, 2), 0x66, 0x2D, 0xFF, 0x00);
	TEST(sub_ri(&t, RAX, 0xFF, 1), 0x2C, 0xFF);

	TEST(imul_rr(&t, RAX, RCX, 8), 0x48, 0x0F, 0xAF, 0xC1);
	TEST(imul_rr(&t, RAX, RCX, 4), 0x0F, 0xAF, 0xC1);
	TEST(imul_rr(&t, RAX, RCX, 2), 0x66, 0x0F, 0xAF, 0xC1);
	// TEST(imul_rr(&t, RAX, RCX, 1), 0x28, 0xC8);

	TEST(imul_ri(&t, RAX, RCX, 0xff, 8), 0x48, 0x69, 0xC1, 0xFF, 0x00, 0x00, 0x00);
	TEST(imul_ri(&t, R8, R9, 0xFF, 8), 0x4D, 0x69, 0xC1, 0xFF, 0x00, 0x00, 0x00);
	TEST(imul_ri(&t, RAX, RCX, 0xFF, 4), 0x69, 0xC1, 0xFF, 0x00, 0x00, 0x00);
	TEST(imul_ri(&t, RAX, RCX, 0xFF, 2), 0x66, 0x69, 0xC1, 0xFF, 0x00);
	// TEST(imul_ri(&t, RAX, RCX, 0xFF, 1), 0x2C, 0xFF);

	TEST(cmp_rr(&t, RAX, RCX, 8), 0x48, 0x39, 0xC8);
	TEST(cmp_rr(&t, RAX, R8, 8), 0x4C, 0x39, 0xC0);
	TEST(cmp_rr(&t, RAX, RCX, 4), 0x39, 0xC8);
	TEST(cmp_rr(&t, RAX, RCX, 2), 0x66, 0x39, 0xC8);
	TEST(cmp_rr(&t, RAX, RCX, 1), 0x38, 0xC8);

	TEST(cmp_ri(&t, RAX, 0xff, 8), 0x48, 0x3D, 0xFF, 0x00, 0x00, 0x00);
	TEST(cmp_ri(&t, R8, 0xff, 8), 0x49, 0x81, 0xF8, 0xFF, 0x00, 0x00, 0x00);
	TEST(cmp_ri(&t, RAX, 0xff, 4), 0x3D, 0xFF, 0x00, 0x00, 0x00);
	TEST(cmp_ri(&t, RAX, 0xff, 2), 0x66, 0x3D, 0xFF, 0x00);
	TEST(cmp_ri(&t, RAX, 0xff, 1), 0x3C, 0xFF);

	TEST(mov_rm_sib(&t, RAX, RBP, RCX, 0xff, 8), 0x48, 0x8B, 0x84, 0x0D, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_rm_sib(&t, R8, RBP, RCX, 0xff, 8), 0x4C, 0x8B, 0x84, 0x0D, 0xFF, 0x00, 0x00, 0x00);
	TEST(mov_rm_sib(&t, RCX, RBP, R8, 0xff, 8), 0x4A, 0x8B, 0x8C, 0x05, 0xFF, 0x00, 0x00, 0x00);

	TEST(movsx_rr(&t, RAX, RAX, 8, 4), 0x48, 0x63, 0xC0);
	TEST(movsx_rr(&t, RAX, RCX, 8, 4), 0x48, 0x63, 0xC1);
	TEST(movsx_rr(&t, R8, RAX, 8, 4), 0x4C, 0x63, 0xC0);
	TEST(movsx_rr(&t, R8, R9, 8, 4), 0x4D, 0x63, 0xC1);
	TEST(movsx_rr(&t, RAX, RAX, 4, 1), 0x0F, 0xBE, 0xC0);
	TEST(movsx_rr(&t, R8, RAX, 4, 1), 0x44, 0x0F, 0xBE, 0xC0);
	TEST(movsx_rr(&t, RAX, R8, 4, 1), 0x0F, 0xBE, 0xC0); // ambiguous operand size!

	TEST(movsx_rm(&t, RAX, RBP, 0xFF, 8, 4), 0x48, 0x63, 0x85, 0xFF, 0x00, 0x00, 0x00);
	TEST(movsx_rm(&t, R8, RBP, 0xFF, 8, 4), 0x4C, 0x63, 0x85, 0xFF, 0x00, 0x00, 0x00);
	TEST(movsx_rm(&t, RAX, RBP, 0xFF, 4, 1), 0x0F, 0xBE, 0x85, 0xFF, 0x00, 0x00, 0x00);
	TEST(movsx_rm(&t, R8, RBP, 0xFF, 4, 1), 0x44, 0x0F, 0xBE, 0x85, 0xFF, 0x00, 0x00, 0x00);
	TEST(movsx_rm(&t, RAX, RBP, 0xFF, 4, 2), 0x0F, 0xBF, 0x85, 0xFF, 0x00, 0x00, 0x00);
	TEST(movsx_rm(&t, R8, RBP, 0xFF, 4, 2), 0x44, 0x0F, 0xBF, 0x85, 0xFF, 0x00, 0x00, 0x00);

	TEST(sete(&t, RAX), 0x0F, 0x94, 0xC0);
	TEST(sete(&t, R8), 0x41, 0x0F, 0x94, 0xC0);

	TEST(and_ri(&t, RAX, 0xFF, 1), 0x24, 0xFF);
	TEST(and_ri(&t, RAX, 0xFF, 2), 0x66, 0x25, 0xFF, 0x00);
	TEST(and_ri(&t, RAX, 0xFF, 4), 0x25, 0xFF, 0x00, 0x00, 0x00);
	TEST(and_ri(&t, RAX, 0xFF, 8), 0x48, 0x25, 0xFF, 0x00, 0x00, 0x00);

	TEST(and_ri(&t, R8, 0xFF, 1), 0x41, 0x80, 0xE0, 0xFF);
	TEST(and_ri(&t, R8, 0xFF, 2), 0x66, 0x41, 0x81, 0xE0, 0xFF, 0x00);
	TEST(and_ri(&t, R8, 0xFF, 4), 0x41, 0x81, 0xE0, 0xFF, 0x00, 0x00, 0x00);
	TEST(and_ri(&t, R8, 0xFF, 8), 0x49, 0x81, 0xE0, 0xFF, 0x00, 0x00, 0x00);

	TEST(call_r(&t, RAX), 0xFF, 0xD0);
	TEST(call_r(&t, R8), 0x41, 0xFF, 0xD0);

	return 0;
}