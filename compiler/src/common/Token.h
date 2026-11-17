#pragma once

#include "common/Diagnostics.h"

#include <cstdint>
#include <string>

namespace xb {

// Tokens are produced by the preprocessor's tokenizer and are shared by the
// preprocessor and the parser. The parser only ever sees preprocessed tokens.
enum class Tok : uint8_t {
  Eof,
  EndOfStmt,   // produced by the preprocessor between statements
  Semicolon,   // ';' statement separator (inside a line / rule results)
  Ident,
  Number,
  String,
  True,        // .T. .Y.
  False,       // .F. .N.
  // operators
  Assign,      // :=
  PlusEq, MinusEq, MulEq, DivEq, ModEq, PowEq,
  Plus, Minus, Star, Slash, Percent, Power,   // ^ or **
  Inc, Dec,    // ++ --
  Eq,          // =
  ExactEq,     // ==
  NotEq,       // != <> #
  Lt, Le, Gt, Ge,
  Dollar,      // $
  BitXor,      // ^^ (& and | are Amp and Pipe)
  Shl, Shr,    // << >>
  Tilde,       // ~
  AndEq, OrEq, XorEq, ShlEq, ShrEq, // &= |= ^^= <<= >>=
  And, Or, Not,   // .AND. .OR. .NOT. / !
  LParen, RParen, LBracket, RBracket, LBrace, RBrace,
  Comma, Colon, DoubleColon, Arrow, Pipe, At, Amp,
  Question, DoubleQuestion,
  FatArrow,    // =>
  Dot,         // .
  Ellipsis,    // ...
  Hash,        // # starting a directive or a dumb-stringify result marker
  Backslash,   // \ (escape in rule definitions)
  Other,       // any other character; rejected by the parser
  BlockBody,   // inside the preprocessor only: the statements of an extended codeblock, as one token (`ival`: which)
};

// The spelling of a kind. An operator or logical word takes it whatever the source wrote (`!=` and `<>` are `<>`,
// `**` is `^`, `.not.` is `!`, `.y.` is `.T.`): it is the text that stringifying and -E output show.
const char *tokName(Tok t);

// `:=` and the compound assignments (`+=` ... `>>=`).
inline bool isAssignment(Tok t) {
  switch (t) {
  case Tok::Assign: case Tok::PlusEq: case Tok::MinusEq: case Tok::MulEq: case Tok::DivEq: case Tok::ModEq:
  case Tok::PowEq: case Tok::AndEq: case Tok::OrEq: case Tok::XorEq: case Tok::ShlEq: case Tok::ShrEq:
    return true;
  default:
    return false;
  }
}

struct Token {
  Tok kind = Tok::Eof;
  SourceLoc loc;
  bool spaceBefore = false;  // whitespace precedes the token (stringify, #define f(...))
  std::string text;          // spelling (operators: tokName()); String: the value without quotes
  std::string upper;         // Ident: upper-cased text (xBase is case-insensitive)
  // Number
  bool isInt = false;
  int64_t ival = 0;
  double dval = 0.0;
  uint16_t dec = 0;          // digits after the decimal point
  uint16_t width = 0;        // display width of the integer part (0 = runtime default)
};

// Whether `t` is the operator AWAIT, given the two tokens after it (null past the statement): an operand starts right
// after it (a name, a literal, an array or codeblock, a message to Self `::`, a macro `&` stuck to its name).
// Otherwise AWAIT is a name: `AWAIT( ... )` calls Await(), as `YIELD( ... )` calls Yield(). The parser and the
// preprocessor (where an expression ends) both follow it.
inline bool isAwaitOperator(const Token &t, const Token *next, const Token *after) {
  if (t.kind != Tok::Ident || t.upper != "AWAIT" || !next) return false;
  switch (next->kind) {
  case Tok::Ident: case Tok::Number: case Tok::String: case Tok::True: case Tok::False: case Tok::LBrace:
  case Tok::DoubleColon:
    return true;
  case Tok::Amp:
    return after && !after->spaceBefore;
  default:
    return false;
  }
}

} // namespace xb
