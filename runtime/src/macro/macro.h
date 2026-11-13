/* The macro compiler: an xBase expression in text, compiled when the code
 * runs (macro/lex.c, macro/compile.c) into the code of a small stack machine
 * (macro/run.c). Its grammar is the compiler's expression grammar; a macro
 * sees memory variables, the public functions by name and the runtime's
 * functions, never a function's LOCALs or STATICs. */
#ifndef XB_MACRO_H
#define XB_MACRO_H

#include "xbrt_int.h"

/* Tokens. */
typedef enum mtok {
  MT_END, MT_IDENT, MT_NUMBER, MT_STRING, MT_TRUE, MT_FALSE,
  MT_ASSIGN, MT_PLUSEQ, MT_MINUSEQ, MT_MULEQ, MT_DIVEQ, MT_MODEQ, MT_POWEQ, MT_ANDEQ, MT_OREQ, MT_XOREQ, MT_SHLEQ,
  MT_SHREQ,
  MT_PLUS, MT_MINUS, MT_STAR, MT_SLASH, MT_PERCENT, MT_POWER, MT_INC, MT_DEC,
  MT_EQ, MT_EXACTEQ, MT_NE, MT_LT, MT_LE, MT_GT, MT_GE, MT_DOLLAR, MT_BITXOR, MT_SHL, MT_SHR, MT_TILDE,
  MT_AND, MT_OR, MT_NOT,
  MT_LPAREN, MT_RPAREN, MT_LBRACKET, MT_RBRACKET, MT_LBRACE, MT_RBRACE, MT_COMMA, MT_COLON, MT_ARROW, MT_PIPE,
  MT_AT, MT_AMP, MT_FATARROW, MT_DOT,
  MT_BAD,         /* a character or token the grammar has no use for */
} mtok;

typedef struct mtoken {
  mtok kind;
  const char *text;         /* where it starts in the source */
  size_t len;
  int spaceBefore;          /* blanks precede it */
  /* MT_STRING: the value, between the delimiters (text + 1, len - 2) */
  /* MT_NUMBER */
  int isInt;
  int64_t ival;
  double dval;
  uint16_t dec, width;
} mtoken;

typedef struct mlexer {
  const char *end, *pos;
  mtoken tok;               /* the current token */
  mtoken prev;              /* the one before it */
} mlexer;

void xb_mlex_init(mlexer *lx, const char *src, size_t len);
/* Moves to the next token. */
void xb_mlex_next(mlexer *lx);

/* What compiling found, besides the code (Type() decides from it). */
#define XB_MACRO_SYNTAX  1u  /* not an expression of the grammar */
#define XB_MACRO_NO_VAR  2u  /* names a variable not visible now (or a field) */
#define XB_MACRO_NO_FUNC 4u  /* calls a function the program does not have */
#define XB_MACRO_UDF     8u  /* calls a function that is not one of Clipper's predefined (RESERVED) ones */
#define XB_MACRO_BLOCK  16u  /* the expression is a codeblock */

/* What the text must be. */
typedef enum mgoal {
  MG_VALUE,       /* an expression: its value */
  MG_ASSIGN,      /* something that can be assigned: the code stores its argument there and gives it back */
  MG_REF,         /* a variable or an element: the code gives a reference to it (@&text) */
} mgoal;

/* Compiles `len` bytes of `text` into `*code` (a function of the stack
 * machine: see compile.c), rooted by the caller. Returns XB_MACRO_* flags;
 * with XB_MACRO_SYNTAX `*code` is NIL. */
uint32_t xb_macro_compile(xb_ctx *ctx, xb_item *code, const char *text, size_t len, mgoal goal);

/* The stack machine's operations (see compile.c for their operands). */
/* An MO_BLOCK capture's source: a parameter of the creator, or (with this bit) one of the creator's captures. */
#define XB_MACRO_FROM_CAPTURE 0x80000000u
typedef enum mop {
  MO_NIL, MO_TRUE, MO_FALSE, MO_CONST,
  MO_MV_GET, MO_MV_SET, MO_MV_REF, MO_MV_FIELD, MO_FIELD, MO_FIELD_SET,
  MO_LOCAL_GET, MO_LOCAL_SET, MO_LOCAL_REF,
  MO_CAPTURE_GET, MO_CAPTURE_SET, MO_CAPTURE_REF,
  MO_UNARY, MO_BINARY, MO_STEP,
  MO_JUMP, MO_JUMP_FALSE, MO_JUMP_TRUE,
  MO_POP, MO_PICK, MO_INSERT,
  MO_ARRAY, MO_HASH, MO_INDEX, MO_INDEX_SET, MO_REF_INDEX,
  MO_CALL, MO_NO_FUNC, MO_SEND, MO_BLOCK, MO_MACRO,
  MO_AREA, MO_AREA_END,
  MO_RETURN,
} mop;

/* MO_FIELD's alias operand for the current work area; for an alias on the stack (`( expr )->name`). */
#define XB_MACRO_NO_ALIAS    UINT32_MAX
#define XB_MACRO_STACK_ALIAS (UINT32_MAX - 1)

/* MO_UNARY operands; MU_DEC_AT_RUN gives the double on the top SET DECIMALS decimals (a folded quotient's). */
enum { MU_NEG, MU_NOT, MU_BITNOT, MU_DEC_AT_RUN };
/* MO_BINARY operands. */
enum {
  MB_ADD, MB_SUB, MB_MUL, MB_DIV, MB_MOD, MB_POW, MB_EQ, MB_EXACTEQ, MB_NE, MB_LT, MB_LE, MB_GT, MB_GE, MB_IN,
  MB_BITAND, MB_BITOR, MB_BITXOR, MB_SHL, MB_SHR,
};

#endif
