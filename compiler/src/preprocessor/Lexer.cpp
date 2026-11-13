#include "preprocessor/Lexer.h"

#include "common/Text.h"
#include "xbrt.h" // shared numeric width rule

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace xb {

const char *tokName(Tok t) {
  switch (t) {
  case Tok::Eof: return "end of file";
  case Tok::EndOfStmt: return "end of statement";
  case Tok::Semicolon: return ";";
  case Tok::Ident: return "identifier";
  case Tok::Number: return "number";
  case Tok::String: return "string";
  case Tok::True: return ".T.";
  case Tok::False: return ".F.";
  case Tok::Assign: return ":=";
  case Tok::PlusEq: return "+=";
  case Tok::MinusEq: return "-=";
  case Tok::MulEq: return "*=";
  case Tok::DivEq: return "/=";
  case Tok::ModEq: return "%=";
  case Tok::PowEq: return "^=";
  case Tok::Plus: return "+";
  case Tok::Minus: return "-";
  case Tok::Star: return "*";
  case Tok::Slash: return "/";
  case Tok::Percent: return "%";
  case Tok::Power: return "^";
  case Tok::Inc: return "++";
  case Tok::Dec: return "--";
  case Tok::Eq: return "=";
  case Tok::ExactEq: return "==";
  case Tok::NotEq: return "<>";
  case Tok::Lt: return "<";
  case Tok::Le: return "<=";
  case Tok::Gt: return ">";
  case Tok::Ge: return ">=";
  case Tok::Dollar: return "$";
  case Tok::BitXor: return "^^";
  case Tok::Shl: return "<<";
  case Tok::Shr: return ">>";
  case Tok::Tilde: return "~";
  case Tok::AndEq: return "&=";
  case Tok::OrEq: return "|=";
  case Tok::XorEq: return "^^=";
  case Tok::ShlEq: return "<<=";
  case Tok::ShrEq: return ">>=";
  case Tok::And: return ".AND.";
  case Tok::Or: return ".OR.";
  case Tok::Not: return "!";
  case Tok::LParen: return "(";
  case Tok::RParen: return ")";
  case Tok::LBracket: return "[";
  case Tok::RBracket: return "]";
  case Tok::LBrace: return "{";
  case Tok::RBrace: return "}";
  case Tok::Comma: return ",";
  case Tok::Colon: return ":";
  case Tok::DoubleColon: return "::";
  case Tok::Arrow: return "->";
  case Tok::Pipe: return "|";
  case Tok::At: return "@";
  case Tok::Amp: return "&";
  case Tok::Question: return "?";
  case Tok::DoubleQuestion: return "??";
  case Tok::FatArrow: return "=>";
  case Tok::Dot: return ".";
  case Tok::Ellipsis: return "...";
  case Tok::Hash: return "#";
  case Tok::Backslash: return "\\";
  case Tok::Other: return "character";
  case Tok::BlockBody: return "codeblock statements";
  }
  return "?";
}

Lexer::Lexer(uint32_t fileId, std::string src, Diagnostics &diag, SourceLoc start)
    : file_(fileId), src_(std::move(src)), diag_(diag) {
  if (start.line) {
    line_ = start.line;
    col_ = start.col;
  } else if (src_.compare(0, 3, "\xEF\xBB\xBF") == 0) {
    pos_ = 3; // UTF-8 byte order mark
  }
}

char Lexer::get() {
  char c = src_[pos_++];
  if (c == '\n') {
    ++line_;
    col_ = 1;
  } else {
    ++col_;
  }
  return c;
}

Token &Lexer::add(Tok k, SourceLoc loc, std::string text) {
  Token t;
  t.kind = k;
  t.loc = loc;
  t.spaceBefore = space_;
  t.text = std::move(text);
  out_->push_back(std::move(t));
  space_ = false;
  stmtStart_ = false;
  return out_->back();
}

void Lexer::skipToEol() {
  while (pos_ < src_.size() && peek() != '\n') get();
}

// Blanks between tokens; Ctrl-Z, the end-of-file mark of DOS editors, is one.
static bool isBlank(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\x1a'; }

// At a ';': where the logical line goes on when the ';' continues it, or npos
// when it separates statements. It continues the line when only blanks and
// comments follow up to the newline, which it skips; a block comment that
// crosses a newline continues the line too, right after the comment.
size_t Lexer::continuation() const {
  size_t p = pos_ + 1;
  bool crossed = false;
  for (;;) {
    while (p < src_.size() && isBlank(src_[p])) ++p;
    if (src_.compare(p, 2, "/*") != 0) break;
    size_t end = src_.find("*/", p + 2);
    if (end == std::string::npos) return std::string::npos; // unterminated: reported where it is lexed
    crossed |= src_.find('\n', p) < end;
    p = end + 2;
  }
  if (p < src_.size() && src_[p] != '\n' && src_.compare(p, 2, "//") != 0 && src_.compare(p, 2, "&&") != 0)
    return crossed ? p : std::string::npos;
  size_t eol = src_.find('\n', p);
  return eol == std::string::npos ? src_.size() : eol + 1;
}

// '[' opens a string unless it follows something that can be indexed
// (identifier, closing bracket, literal) - the Clipper rule - and only
// if a closing ']' exists on the same line.
bool Lexer::bracketStringAllowed() const {
  if (directive_) return false;
  if (!out_->empty()) {
    switch (out_->back().kind) {
    case Tok::Ident: case Tok::RParen: case Tok::RBracket: case Tok::RBrace: case Tok::String:
    case Tok::Number: case Tok::True: case Tok::False:
      return false;
    default:
      break;
    }
  }
  for (size_t p = pos_ + 1; p < src_.size() && src_[p] != '\n'; ++p)
    if (src_[p] == ']') return true;
  return false;
}

// Numeric literal. Besides the value, the compiler computes the display
// width the way Clipper does, so `? 12345678901` prints like Clipper.
void Lexer::lexNumber() {
  SourceLoc loc = here();
  size_t start = pos_;

  if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X') && std::isxdigit((unsigned char)peek(2))) {
    get();
    get();
    size_t hs = pos_;
    while (std::isxdigit((unsigned char)peek())) get();
    std::string h = src_.substr(hs, pos_ - hs);
    errno = 0;
    unsigned long long v = std::strtoull(h.c_str(), nullptr, 16);
    if (errno == ERANGE || v > (unsigned long long)INT64_MAX) diag_.error(loc, "hexadecimal literal out of range");
    Token &t = add(Tok::Number, loc, src_.substr(start, pos_ - start));
    t.isInt = true;
    t.ival = (int64_t)v;
    return;
  }

  while (std::isdigit((unsigned char)peek())) get();
  size_t intDigits = pos_ - start;
  bool hasDot = false;
  uint16_t dec = 0;
  if (peek() == '.' && std::isdigit((unsigned char)peek(1))) {
    hasDot = true;
    get();
    size_t ds = pos_;
    while (std::isdigit((unsigned char)peek())) get();
    dec = (uint16_t)(pos_ - ds);
  }
  Token &t = add(Tok::Number, loc, src_.substr(start, pos_ - start));
  t.dec = dec;
  bool isDouble = hasDot;
  if (!hasDot) {
    errno = 0;
    long long v = std::strtoll(t.text.c_str(), nullptr, 10);
    if (errno != ERANGE) {
      t.isInt = true;
      t.ival = v;
    } else {
      isDouble = true; // too large for int64: becomes a double
    }
  }
  if (isDouble) t.dval = std::strtod(t.text.c_str(), nullptr);

  t.width = xb_num_literal_width(isDouble, t.ival, t.dval, intDigits, dec);
}

void Lexer::lexString(char close) {
  SourceLoc loc = here();
  get(); // opening delimiter
  std::string val;
  while (true) {
    if (pos_ >= src_.size() || peek() == '\n') {
      diag_.error(loc, "unterminated string literal");
      break;
    }
    char c = get();
    if (c == close) break;
    val.push_back(c);
  }
  add(Tok::String, loc, std::move(val));
}

// .T. .F. .Y. .N. .AND. .OR. .NOT. (case-insensitive)
bool Lexer::lexDotWord() {
  size_t p = pos_ + 1;
  while (p < src_.size() && std::isalpha((unsigned char)src_[p])) ++p;
  if (p >= src_.size() || src_[p] != '.') return false;
  std::string w = upperCase(std::string_view(src_).substr(pos_ + 1, p - pos_ - 1));
  Tok k;
  if (w == "T" || w == "Y") k = Tok::True;
  else if (w == "F" || w == "N") k = Tok::False;
  else if (w == "AND") k = Tok::And;
  else if (w == "OR") k = Tok::Or;
  else if (w == "NOT") k = Tok::Not;
  else return false;
  SourceLoc loc = here();
  while (pos_ <= p) get();
  add(k, loc, tokName(k));
  return true;
}

void Lexer::lexIdent() {
  SourceLoc loc = here();
  bool atStmt = stmtStart_;
  size_t start = pos_;
  while (isIdentChar(peek())) get();
  std::string w = src_.substr(start, pos_ - start);
  std::string up = upperCase(w);
  if (atStmt && up == "NOTE") { // NOTE comment: first token of a statement
    skipToEol();
    return;
  }
  Token &t = add(Tok::Ident, loc, std::move(w));
  t.upper = std::move(up);
}

bool Lexer::lexOperator() {
  struct Op {
    const char *s;
    Tok k;
  };
  static const Op ops[] = {
      {"^^=", Tok::XorEq},  {"<<=", Tok::ShlEq},    {">>=", Tok::ShrEq},  {"^^", Tok::BitXor},
      {"<<", Tok::Shl},     {">>", Tok::Shr},       {"&=", Tok::AndEq},   {"|=", Tok::OrEq},
      {"**=", Tok::PowEq},  {"...", Tok::Ellipsis}, {":=", Tok::Assign}, {"+=", Tok::PlusEq},
      {"-=", Tok::MinusEq}, {"*=", Tok::MulEq},     {"/=", Tok::DivEq},   {"%=", Tok::ModEq},
      {"^=", Tok::PowEq},   {"==", Tok::ExactEq},   {"=>", Tok::FatArrow}, {"!=", Tok::NotEq},
      {"<>", Tok::NotEq},   {"<=", Tok::Le},        {">=", Tok::Ge},      {"++", Tok::Inc},
      {"--", Tok::Dec},     {"->", Tok::Arrow},     {"**", Tok::Power},   {"::", Tok::DoubleColon},
      {"??", Tok::DoubleQuestion},
      {"+", Tok::Plus},     {"-", Tok::Minus},      {"*", Tok::Star},     {"/", Tok::Slash},
      {"%", Tok::Percent},  {"^", Tok::Power},      {"=", Tok::Eq},       {"<", Tok::Lt},
      {">", Tok::Gt},       {"!", Tok::Not},        {"$", Tok::Dollar},   {"(", Tok::LParen},
      {")", Tok::RParen},   {"[", Tok::LBracket},   {"]", Tok::RBracket}, {"{", Tok::LBrace},
      {"}", Tok::RBrace},   {",", Tok::Comma},      {":", Tok::Colon},    {"|", Tok::Pipe},
      {"@", Tok::At},       {"&", Tok::Amp},        {"?", Tok::Question}, {".", Tok::Dot},
      {"~", Tok::Tilde},
  };
  for (const Op &op : ops) {
    size_t n = std::strlen(op.s);
    if (src_.compare(pos_, n, op.s) == 0) {
      SourceLoc loc = here();
      for (size_t i = 0; i < n; ++i) get();
      add(op.k, loc, tokName(op.k));
      return true;
    }
  }
  return false;
}

bool Lexer::rawLine(std::string &text, SourceLoc &loc) {
  if (pos_ >= src_.size()) return false;
  loc = here();
  size_t start = pos_;
  skipToEol();
  text.assign(src_, start, pos_ - start);
  if (!text.empty() && text.back() == '\r') text.pop_back();
  if (pos_ < src_.size()) get(); // the newline
  return true;
}

bool Lexer::nextLine(std::vector<Token> &line) {
  line.clear();
  if (pos_ >= src_.size()) return false;
  out_ = &line;
  space_ = false;
  stmtStart_ = true;
  directive_ = false;

  while (pos_ < src_.size()) {
    char c = peek();
    SourceLoc loc = here();

    if (c == '\n') {
      get();
      return true;
    }
    if (isBlank(c)) {
      get();
      space_ = true;
      continue;
    }
    if ((c == '/' && peek(1) == '/') || (c == '&' && peek(1) == '&')) {
      skipToEol();
      continue;
    }
    if (c == '/' && peek(1) == '*') {
      get();
      get();
      while (pos_ < src_.size() && !(peek() == '*' && peek(1) == '/')) get();
      if (pos_ >= src_.size()) diag_.error(loc, "unterminated /* comment");
      else { get(); get(); }
      space_ = true;
      continue;
    }
    if (c == '*' && line.empty()) { // '*' comment line
      skipToEol();
      continue;
    }
    if (c == ';') {
      if (size_t to = continuation(); to != std::string::npos) {
        while (pos_ < to) get(); // through get(), which counts the lines
        space_ = true;
        continue;
      }
      get();
      add(Tok::Semicolon, loc, ";");
      stmtStart_ = true;
      continue;
    }
    if (c == '#') {
      if (line.empty()) {
        get();
        add(Tok::Hash, loc, "#");
        directive_ = true;
        continue;
      }
      if (directive_) { // '#<marker>' in a rule result: dumb stringify
        size_t p = pos_ + 1;
        while (p < src_.size() && (src_[p] == ' ' || src_[p] == '\t')) ++p;
        if (p < src_.size() && src_[p] == '<') {
          get();
          add(Tok::Hash, loc, "#");
          continue;
        }
      }
      get();
      add(Tok::NotEq, loc, "#");
      continue;
    }
    if (std::isdigit((unsigned char)c) || (c == '.' && std::isdigit((unsigned char)peek(1)))) {
      lexNumber();
      continue;
    }
    if (isIdentStart(c)) {
      lexIdent();
      continue;
    }
    if (c == '"' || c == '\'') {
      lexString(c);
      continue;
    }
    if (c == '[' && bracketStringAllowed()) {
      lexString(']');
      continue;
    }
    if (c == '.' && lexDotWord()) continue;
    if (c == '\\' && directive_) {
      get();
      add(Tok::Backslash, loc, "\\");
      continue;
    }
    if (lexOperator()) continue;
    get();
    add(Tok::Other, loc, std::string(1, c));
  }
  return true;
}

} // namespace xb
