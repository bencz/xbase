// Expressions, by precedence level from assignment down to primaries.
#include "parser/Parser.h"

namespace xb {

ExprPtr Parser::parseExpr() { return parseAssign(); }

static bool isLvalue(const Expr &e) { return e.kind == ExprKind::Var || e.kind == ExprKind::Index; }

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
  if (at(Tok::Not)) {
    SourceLoc loc = cur().loc;
    advance();
    return std::make_unique<UnaryExpr>(loc, UnOp::Not, parseNot());
  }
  return parseEquality();
}

ExprPtr Parser::parseEquality() {
  ExprPtr lhs = parseRelational();
  while (true) {
    BinOp op;
    switch (cur().kind) {
    case Tok::Eq: op = BinOp::Eq; break;
    case Tok::ExactEq: op = BinOp::ExactEq; break;
    case Tok::NotEq: op = BinOp::NotEq; break;
    default: return lhs;
    }
    SourceLoc loc = cur().loc;
    advance();
    lhs = std::make_unique<BinaryExpr>(loc, op, std::move(lhs), parseRelational());
  }
}

ExprPtr Parser::parseRelational() {
  ExprPtr lhs = parseAdditive();
  while (true) {
    BinOp op;
    switch (cur().kind) {
    case Tok::Lt: op = BinOp::Lt; break;
    case Tok::Le: op = BinOp::Le; break;
    case Tok::Gt: op = BinOp::Gt; break;
    case Tok::Ge: op = BinOp::Ge; break;
    case Tok::Dollar: op = BinOp::In; break;
    default: return lhs;
    }
    SourceLoc loc = cur().loc;
    advance();
    lhs = std::make_unique<BinaryExpr>(loc, op, std::move(lhs), parseAdditive());
  }
}

ExprPtr Parser::parseAdditive() {
  ExprPtr lhs = parseMultiplicative();
  while (at(Tok::Plus) || at(Tok::Minus)) {
    BinOp op = at(Tok::Plus) ? BinOp::Add : BinOp::Sub;
    SourceLoc loc = cur().loc;
    advance();
    lhs = std::make_unique<BinaryExpr>(loc, op, std::move(lhs), parseMultiplicative());
  }
  return lhs;
}

ExprPtr Parser::parseMultiplicative() {
  ExprPtr lhs = parsePower();
  while (at(Tok::Star) || at(Tok::Slash) || at(Tok::Percent)) {
    BinOp op = at(Tok::Star) ? BinOp::Mul : at(Tok::Slash) ? BinOp::Div : BinOp::Mod;
    SourceLoc loc = cur().loc;
    advance();
    lhs = std::make_unique<BinaryExpr>(loc, op, std::move(lhs), parsePower());
  }
  return lhs;
}

ExprPtr Parser::parsePower() {
  ExprPtr lhs = parseUnary();
  while (at(Tok::Power)) {
    SourceLoc loc = cur().loc;
    advance();
    lhs = std::make_unique<BinaryExpr>(loc, BinOp::Pow, std::move(lhs), parseUnary());
  }
  return lhs;
}

ExprPtr Parser::parseUnary() {
  SourceLoc loc = cur().loc;
  if (accept(Tok::Minus)) return std::make_unique<UnaryExpr>(loc, UnOp::Neg, parseUnary());
  if (accept(Tok::Plus)) return std::make_unique<UnaryExpr>(loc, UnOp::Plus, parseUnary());
  if (at(Tok::Inc) || at(Tok::Dec)) {
    bool inc = at(Tok::Inc);
    advance();
    ExprPtr t = parseUnary();
    if (!isLvalue(*t)) diag_.error(t->loc, "invalid operand for ++/--");
    return std::make_unique<IncDecExpr>(loc, inc, true, std::move(t));
  }
  return parsePostfix();
}

ExprPtr Parser::parsePostfix() {
  ExprPtr e = parsePrimary();
  while (at(Tok::LBracket)) { // a[i], a[i, j] == a[i][j]
    advance();
    do {
      SourceLoc loc = cur().loc;
      e = std::make_unique<IndexExpr>(loc, std::move(e), parseExpr());
    } while (accept(Tok::Comma));
    expect(Tok::RBracket, "']'");
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
  auto arr = std::make_unique<ArrayLitExpr>(loc);
  arr->elems = parseArgs(Tok::RBrace, false);
  expect(Tok::RBrace, "'}'");
  return arr;
}

// Empty arguments - f( a, , b ) - are passed as NIL. `refs`: the arguments of
// a function call, where `@` passes a variable or element by reference.
std::vector<ExprPtr> Parser::parseArgs(Tok close, bool refs) {
  std::vector<ExprPtr> args;
  if (at(close)) return args;
  while (true) {
    if (at(Tok::Comma) || at(close)) {
      args.push_back(std::make_unique<NilExpr>(cur().loc));
    } else if (at(Tok::At)) {
      if (!refs) diag_.error(cur().loc, "'@' (by reference) is supported only in function call arguments");
      args.push_back(parseRef());
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
  else if (!isLvalue(*target) || target->parenthesized)
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
  case Tok::LParen: {
    advance();
    ExprPtr e = parseExpr();
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
        auto args = parseArgs(Tok::RParen, false);
        expect(Tok::RParen, "')'");
        if (args.size() != 3) {
          diag_.error(loc, name + "() requires exactly 3 arguments");
          return std::make_unique<NilExpr>(loc);
        }
        return std::make_unique<IIfExpr>(loc, std::move(args[0]), std::move(args[1]), std::move(args[2]));
      }
      auto call = std::make_unique<CallExpr>(loc, name);
      call->args = parseArgs(Tok::RParen, true);
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
