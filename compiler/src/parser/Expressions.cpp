// Expressions, by precedence level from assignment down to primaries.
#include "parser/Parser.h"

namespace xb {

ExprPtr Parser::parseExpr() { return parseAssign(); }

ExprPtr Parser::parseAssign() {
  ExprPtr lhs = parseOr();
  BinOp op = BinOp::Add;
  bool compound = true;
  switch (cur().kind) {
  case Tok::Assign: compound = false; break;
  case Tok::PlusEq: op = BinOp::Add; break;
  case Tok::MinusEq: op = BinOp::Sub; break;
  case Tok::MulEq: op = BinOp::Mul; break;
  case Tok::DivEq: op = BinOp::Div; break;
  case Tok::ModEq: op = BinOp::Mod; break;
  case Tok::PowEq: op = BinOp::Pow; break;
  case Tok::AndEq: op = BinOp::BitAnd; break;
  case Tok::OrEq: op = BinOp::BitOr; break;
  case Tok::XorEq: op = BinOp::BitXor; break;
  case Tok::ShlEq: op = BinOp::Shl; break;
  case Tok::ShrEq: op = BinOp::Shr; break;
  default: return lhs;
  }
  SourceLoc loc = cur().loc;
  advance();
  if (!isLvalue(*lhs)) diag_.error(lhs->loc, "invalid assignment target");
  ExprPtr rhs = parseAssign(); // right associative
  auto a = std::make_unique<AssignExpr>(loc, std::move(lhs), std::move(rhs));
  a->compound = compound;
  a->op = op;
  return a;
}

ExprPtr Parser::parseOr() {
  ExprPtr lhs = parseAnd();
  while (at(Tok::Or)) {
    SourceLoc loc = cur().loc;
    advance();
    lhs = std::make_unique<LogicExpr>(loc, ExprKind::Or, std::move(lhs), parseAnd());
  }
  return lhs;
}

ExprPtr Parser::parseAnd() {
  ExprPtr lhs = parseNot();
  while (at(Tok::And)) {
    SourceLoc loc = cur().loc;
    advance();
    lhs = std::make_unique<LogicExpr>(loc, ExprKind::And, std::move(lhs), parseNot());
  }
  return lhs;
}

ExprPtr Parser::parseNot() {
  if (!at(Tok::Not)) return parseBinary(0);
  SourceLoc loc = cur().loc;
  if (!enterExpr()) return std::make_unique<NilExpr>(loc);
  advance();
  ExprPtr e = std::make_unique<UnaryExpr>(loc, UnOp::Not, parseNot());
  --exprDepth_;
  return e;
}

namespace {

struct BinaryToken {
  Tok tok;
  BinOp op;
};

// The binary operators below .NOT., one precedence level per row from the
// lowest, each level left-associative. The bit operators sit where C has
// them: | below ^^ below & below the comparisons, the shifts between the
// comparisons and + -.
const std::vector<std::vector<BinaryToken>> &binaryLevels() {
  static const std::vector<std::vector<BinaryToken>> levels = {
      {{Tok::Pipe, BinOp::BitOr}},
      {{Tok::BitXor, BinOp::BitXor}},
      {{Tok::Amp, BinOp::BitAnd}},
      {{Tok::Eq, BinOp::Eq}, {Tok::ExactEq, BinOp::ExactEq}, {Tok::NotEq, BinOp::NotEq}},
      {{Tok::Lt, BinOp::Lt}, {Tok::Le, BinOp::Le}, {Tok::Gt, BinOp::Gt}, {Tok::Ge, BinOp::Ge},
       {Tok::Dollar, BinOp::In}},
      {{Tok::Shl, BinOp::Shl}, {Tok::Shr, BinOp::Shr}},
      {{Tok::Plus, BinOp::Add}, {Tok::Minus, BinOp::Sub}},
      {{Tok::Star, BinOp::Mul}, {Tok::Slash, BinOp::Div}, {Tok::Percent, BinOp::Mod}},
      {{Tok::Power, BinOp::Pow}},
  };
  return levels;
}

} // namespace

ExprPtr Parser::parseBinary(size_t level) {
  const auto &levels = binaryLevels();
  if (level == levels.size()) return parseUnary();
  ExprPtr lhs = parseBinary(level + 1);
  for (;;) {
    const BinaryToken *found = nullptr;
    for (const BinaryToken &bt : levels[level])
      if (at(bt.tok)) found = &bt;
    if (!found) return lhs;
    SourceLoc loc = cur().loc;
    advance();
    lhs = std::make_unique<BinaryExpr>(loc, found->op, std::move(lhs), parseBinary(level + 1));
  }
}

// One more level of expression nesting (each nested expression and each
// prefix operator passes through parseUnary or parseNot once per level); at
// the limit the rest of the statement is skipped.
bool Parser::enterExpr() {
  if (exprDepth_ == kMaxExprNesting) {
    tooDeep("expression", kMaxExprNesting);
    skipToEndOfStmt();
    return false;
  }
  ++exprDepth_;
  return true;
}

ExprPtr Parser::parseUnary() {
  SourceLoc loc = cur().loc;
  if (!enterExpr()) return std::make_unique<NilExpr>(loc);
  ExprPtr e;
  if (accept(Tok::Minus)) {
    e = std::make_unique<UnaryExpr>(loc, UnOp::Neg, parseUnary());
  } else if (accept(Tok::Plus)) {
    e = std::make_unique<UnaryExpr>(loc, UnOp::Plus, parseUnary());
  } else if (accept(Tok::Tilde)) {
    e = std::make_unique<UnaryExpr>(loc, UnOp::BitNot, parseUnary());
  } else if (at(Tok::Inc) || at(Tok::Dec)) {
    bool inc = at(Tok::Inc);
    advance();
    ExprPtr t = parseUnary();
    if (!isLvalue(*t)) diag_.error(t->loc, "invalid operand for ++/--");
    e = std::make_unique<IncDecExpr>(loc, inc, true, std::move(t));
  } else {
    e = parsePostfix();
  }
  --exprDepth_;
  return e;
}

ExprPtr Parser::parsePostfix() {
  ExprPtr e = parsePrimary();
  while (true) {
    if (accept(Tok::LBracket)) { // a[i], a[i, j] == a[i][j]
      do {
        SourceLoc loc = cur().loc;
        e = std::make_unique<IndexExpr>(loc, std::move(e), parseExpr());
      } while (accept(Tok::Comma));
      expect(Tok::RBracket, "']'");
    } else if (at(Tok::Colon)) { // o:message, o:message( args )
      SourceLoc loc = cur().loc;
      advance();
      if (!at(Tok::Ident)) {
        expect(Tok::Ident, "message name");
        return e;
      }
      auto send = std::make_unique<SendExpr>(loc, std::move(e), cur().upper);
      advance();
      if (accept(Tok::LParen)) {
        send->withArgs = true;
        send->args = parseArgs(Tok::RParen, false, true);
        expect(Tok::RParen, "')'");
      }
      e = std::move(send);
    } else {
      break;
    }
  }
  while (at(Tok::Inc) || at(Tok::Dec)) {
    bool inc = at(Tok::Inc);
    SourceLoc loc = cur().loc;
    advance();
    if (!isLvalue(*e)) diag_.error(e->loc, "invalid operand for ++/--");
    e = std::make_unique<IncDecExpr>(loc, inc, false, std::move(e));
  }
  return e;
}

// {| params | expr, ... } or { elem, ... } (the opening brace is current).
ExprPtr Parser::parseBraced() {
  SourceLoc loc = cur().loc;
  advance();
  if (accept(Tok::Pipe)) {
    auto blk = std::make_unique<BlockExpr>(loc);
    if (!at(Tok::Pipe)) {
      do {
        if (!at(Tok::Ident)) {
          expect(Tok::Ident, "codeblock parameter");
          break;
        }
        blk->params.push_back({cur().loc, cur().upper});
        advance();
      } while (accept(Tok::Comma));
    }
    expect(Tok::Pipe, "'|'");
    do { // the body has at least one expression, as in Clipper
      blk->body.push_back(parseExpr());
    } while (accept(Tok::Comma));
    expect(Tok::RBrace, "'}'");
    return blk;
  }
  if (accept(Tok::FatArrow)) { // {=>}: an empty hash
    expect(Tok::RBrace, "'}'");
    return std::make_unique<HashLitExpr>(loc);
  }
  auto arr = std::make_unique<ArrayLitExpr>(loc);
  if (at(Tok::Comma) || at(Tok::RBrace) || at(Tok::At) || at(Tok::Ellipsis)) {
    arr->elems = parseArgs(Tok::RBrace, false, true);
    expect(Tok::RBrace, "'}'");
    return arr;
  }
  ExprPtr first = parseExpr();
  if (accept(Tok::FatArrow)) { // { k => v, ... }
    auto hash = std::make_unique<HashLitExpr>(loc);
    hash->keys.push_back(std::move(first));
    hash->values.push_back(parseExpr());
    while (accept(Tok::Comma)) {
      hash->keys.push_back(parseExpr());
      expect(Tok::FatArrow, "'=>'");
      hash->values.push_back(parseExpr());
    }
    expect(Tok::RBrace, "'}'");
    return hash;
  }
  arr->elems.push_back(std::move(first));
  if (accept(Tok::Comma)) { // the rest as arguments: { a, } has a NIL last
    std::vector<ExprPtr> rest = parseArgs(Tok::RBrace, false, true);
    if (rest.empty()) rest.push_back(std::make_unique<NilExpr>(cur().loc));
    for (ExprPtr &e : rest) arr->elems.push_back(std::move(e));
  }
  expect(Tok::RBrace, "'}'");
  return arr;
}

// Empty arguments - f( a, , b ) - are passed as NIL. `refs`: the arguments of
// a function call, where `@` passes a variable or element by reference.
// `spread`: `...` may stand for the arguments of the function past its
// parameters (a call, a send, an array literal).
std::vector<ExprPtr> Parser::parseArgs(Tok close, bool refs, bool spread) {
  std::vector<ExprPtr> args;
  bool spreadSeen = false;
  if (at(close)) return args;
  while (true) {
    if (at(Tok::Comma) || at(close)) {
      args.push_back(std::make_unique<NilExpr>(cur().loc));
    } else if (at(Tok::At)) {
      if (!refs) diag_.error(cur().loc, "'@' (by reference) is supported only in function call arguments");
      args.push_back(parseRef());
    } else if (at(Tok::Ellipsis)) {
      if (!spread) diag_.error(cur().loc, "'...' is supported only in the arguments of a call or in an array");
      else if (spreadSeen) diag_.error(cur().loc, "'...' may appear once in a list");
      spreadSeen = true;
      args.push_back(std::make_unique<SpreadExpr>(cur().loc));
      advance();
    } else {
      args.push_back(parseExpr());
    }
    if (!accept(Tok::Comma)) break;
  }
  return args;
}

// @name or @name[i]... (the '@' is current).
ExprPtr Parser::parseRef() {
  SourceLoc loc = cur().loc;
  advance();
  ExprPtr target = parsePostfix();
  if (target->kind == ExprKind::Call)
    diag_.error(target->loc, "function references (@name()) are not supported yet");
  else if (!isLvalue(*target) || target->kind == ExprKind::Send || target->parenthesized)
    diag_.error(target->loc, "only a variable or an array element can be passed by reference");
  return std::make_unique<RefExpr>(loc, std::move(target));
}

ExprPtr Parser::parsePrimary() {
  const Token &t = cur();
  SourceLoc loc = t.loc;
  switch (t.kind) {
  case Tok::Number: {
    auto e = std::make_unique<NumberExpr>(loc, t.isInt, t.ival, t.dval, t.dec, t.width);
    advance();
    return e;
  }
  case Tok::String: {
    auto e = std::make_unique<StringExpr>(loc, t.text);
    advance();
    return e;
  }
  case Tok::True: advance(); return std::make_unique<LogicalExpr>(loc, true);
  case Tok::False: advance(); return std::make_unique<LogicalExpr>(loc, false);
  case Tok::LBrace:
    return parseBraced();
  case Tok::LParen: { // ( expr ) or the expression list ( expr, expr, ... )
    advance();
    ExprPtr e = parseExpr();
    if (at(Tok::Comma)) {
      auto list = std::make_unique<ListExpr>(loc);
      list->exprs.push_back(std::move(e));
      while (accept(Tok::Comma)) list->exprs.push_back(parseExpr());
      e = std::move(list);
    }
    expect(Tok::RParen, "')'");
    e->parenthesized = true;
    return e;
  }
  case Tok::Ident: {
    std::string name = t.upper;
    if (ahead().kind == Tok::LParen) {
      advance();
      advance();
      if (name == "IIF" || name == "IF") {
        auto args = parseArgs(Tok::RParen, false, false);
        expect(Tok::RParen, "')'");
        if (args.size() != 3) {
          diag_.error(loc, name + "() requires exactly 3 arguments");
          return std::make_unique<NilExpr>(loc);
        }
        return std::make_unique<IIfExpr>(loc, std::move(args[0]), std::move(args[1]), std::move(args[2]));
      }
      auto call = std::make_unique<CallExpr>(loc, name);
      call->args = parseArgs(Tok::RParen, true, true);
      expect(Tok::RParen, "')'");
      return call;
    }
    advance();
    if (name == "NIL") return std::make_unique<NilExpr>(loc);
    return std::make_unique<VarExpr>(loc, name);
  }
  default:
    diag_.error(loc, std::string("expected expression, found ") + tokName(t.kind));
    // A closing bracket is left for the construct it closes (no second error).
    if (!atEndOfStmt() && t.kind != Tok::RParen && t.kind != Tok::RBracket && t.kind != Tok::RBrace) advance();
    return std::make_unique<NilExpr>(loc);
  }
}

} // namespace xb
