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
};

const char *tokName(Tok t);

struct Token {
  Tok kind = Tok::Eof;
  SourceLoc loc;
  bool spaceBefore = false;  // whitespace precedes the token (stringify, #define f(...))
  std::string text;          // spelling as written; String: the value without quotes
  std::string upper;         // Ident: upper-cased text (xBase is case-insensitive)
  // Number
  bool isInt = false;
  int64_t ival = 0;
  double dval = 0.0;
  uint16_t dec = 0;          // digits after the decimal point
  uint16_t width = 0;        // display width of the integer part (0 = runtime default)
};

} // namespace xb
