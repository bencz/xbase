#pragma once

#include "common/Diagnostics.h"
#include "common/Token.h"

#include <string>
#include <vector>

namespace xb {

// The preprocessor's tokenizer. Delivers one *logical* line at a time:
// physical lines joined by a trailing ';', comments removed. Inside a line,
// ';' is returned as Tok::Semicolon (statement separator).
//
// Lines starting with '#' are directive lines: there '[' is never a string
// delimiter (it marks optional clauses) and '\' is an escape.
class Lexer {
public:
  Lexer(uint32_t fileId, std::string src, Diagnostics &diag);

  // Returns false at end of file. `line` may be empty (blank line).
  bool nextLine(std::vector<Token> &line);

private:
  char peek(size_t o = 0) const { return pos_ + o < src_.size() ? src_[pos_ + o] : '\0'; }
  char get();
  SourceLoc here() const { return {file_, line_, col_}; }
  Token &add(Tok k, SourceLoc loc, std::string text);
  void skipToEol();
  size_t continuation() const;
  bool bracketStringAllowed() const;
  void lexNumber();
  void lexString(char close);
  bool lexDotWord();
  void lexIdent();
  bool lexOperator();

  uint32_t file_;
  std::string src_;
  Diagnostics &diag_;
  size_t pos_ = 0;
  uint32_t line_ = 1, col_ = 1;

  // per logical line
  std::vector<Token> *out_ = nullptr;
  bool space_ = false;        // whitespace seen since the last token
  bool stmtStart_ = true;     // next token starts a statement (line start or after ';')
  bool directive_ = false;    // the line is a '#' directive
};

} // namespace xb
