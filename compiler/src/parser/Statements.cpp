// Statements: declarations, IF, DO CASE, SWITCH, loops.
#include "parser/Parser.h"

namespace xb {

StmtPtr Parser::parseStmt() {
  SourceLoc loc = cur().loc;

  if (atExtern()) { // only nested blocks get here: a function body ends at it
    diag_.error(loc, "EXTERN FUNCTION must be outside of functions");
    skipToEndOfStmt();
    endOfStmt();
    return nullptr;
  }
  if (atStmtKw("LOCAL")) return parseDecl(StmtKind::Local);
  if (atStmtKw("STATIC")) return parseDecl(StmtKind::Static);
  if (atStmtKw("DO") && isKw(ahead(), "CASE")) return parseDoCase();
  if (atStmtKw("SWITCH")) return parseSwitch();
  if (atStmtKw("IF") && !atInlineIf()) return parseIf();
  if (atStmtKw("WHILE")) return parseWhile();
  if (atStmtKw("FOR")) return isKw(ahead(), "EACH") && !isAssignAfterEach() ? parseForEach() : parseFor();
  if (atStmtKw("RETURN")) {
    advance();
    ExprPtr v;
    if (!atEndOfStmt()) v = parseExpr();
    endOfStmt();
    return std::make_unique<ReturnStmt>(loc, std::move(v));
  }
  if (atStmtKw("EXIT") || atStmtKw("LOOP")) {
    StmtKind k = atKw("EXIT") ? StmtKind::Exit : StmtKind::Loop;
    advance();
    endOfStmt();
    return std::make_unique<JumpStmt>(loc, k);
  }
  if (atStmtKw("DO") && ahead().kind == Tok::Ident) { // DO proc [WITH args]
    advance();
    auto call = std::make_unique<CallExpr>(cur().loc, cur().upper);
    advance();
    if (atKw("WITH")) { // empty arguments pass NIL, as in a call; a variable is passed by reference
      advance();
      do {
        if (at(Tok::Comma) || atEndOfStmt()) {
          call->args.push_back(std::make_unique<NilExpr>(cur().loc));
        } else if (at(Tok::At)) {
          call->args.push_back(parseRef());
        } else {
          ExprPtr arg = parseExpr();
          if (arg->kind == ExprKind::Var && !arg->parenthesized) {
            SourceLoc argLoc = arg->loc;
            arg = std::make_unique<RefExpr>(argLoc, std::move(arg));
          }
          call->args.push_back(std::move(arg));
        }
      } while (accept(Tok::Comma));
    }
    endOfStmt();
    return std::make_unique<ExprStmt>(loc, std::move(call));
  }
  for (const char *closer : {"ENDIF", "ENDDO", "NEXT", "END", "ELSE", "ELSEIF", "CASE", "OTHERWISE", "ENDCASE",
                             "ENDSWITCH"}) {
    if (atStmtKw(closer)) {
      diag_.error(loc, std::string(closer) + " does not match any open block");
      skipToEndOfStmt();
      return nullptr;
    }
  }
  if (atStmtKw("PRIVATE") || atStmtKw("PUBLIC") || atStmtKw("MEMVAR") ||
      atStmtKw("FIELD") || atStmtKw("PARAMETERS")) {
    diag_.error(loc, "'" + cur().text + "' is not supported yet");
    skipToEndOfStmt();
    return nullptr;
  }

  // A statement `operand = expr` is an assignment (`x = y = 3` is
  // `x := ( y = 3 )`), whose target must be a variable or an element, also in
  // parentheses: the statement is read as a postfix expression first, and from
  // its start again when no `=` follows. The first reading's errors are the
  // statement's.
  if (at(Tok::Ident) || at(Tok::LParen) || at(Tok::String) || at(Tok::Number) || at(Tok::True) ||
      at(Tok::False) || at(Tok::LBrace)) {
    size_t start = pos_;
    unsigned errors = diag_.errorCount();
    ExprPtr target = parsePostfix();
    if (diag_.errorCount() != errors) {
      skipToEndOfStmt();
      return nullptr;
    }
    if (at(Tok::Eq)) {
      if (!isLvalue(*target)) {
        diag_.error(target->loc, "invalid assignment target");
        skipToEndOfStmt();
        return nullptr;
      }
      SourceLoc aloc = cur().loc;
      advance();
      ExprPtr value = parseExpr();
      endOfStmt();
      return std::make_unique<ExprStmt>(loc, std::make_unique<AssignExpr>(aloc, std::move(target), std::move(value)));
    }
    pos_ = start;
  }

  // A statement whose expression has an error says nothing more: the rest of
  // it is skipped (one error per mistake).
  unsigned errors = diag_.errorCount();
  ExprPtr e = parseExpr();
  if (diag_.errorCount() != errors) {
    skipToEndOfStmt();
    accept(Tok::EndOfStmt);
    return std::make_unique<ExprStmt>(loc, std::move(e));
  }
  switch (e->kind) {
  case ExprKind::Call: case ExprKind::Assign: case ExprKind::IncDec: case ExprKind::IIf: case ExprKind::List:
    break;
  default:
    diag_.warning(e->loc, "meaningless use of expression");
  }
  endOfStmt();
  return std::make_unique<ExprStmt>(loc, std::move(e));
}

StmtPtr Parser::parseDecl(StmtKind kind) {
  auto st = std::make_unique<LocalStmt>(cur().loc, kind);
  advance();
  do {
    if (!at(Tok::Ident)) {
      expect(Tok::Ident, "variable name");
      skipToEndOfStmt();
      break;
    }
    LocalDecl d;
    d.loc = cur().loc;
    d.name = cur().upper;
    advance();
    if (at(Tok::LBracket)) { // LOCAL a[n1, n2] or a[n1][n2] is LOCAL a := Array( n1, n2 )
      auto dims = std::make_unique<CallExpr>(cur().loc, "ARRAY");
      dims->declaredOnly = true; // Clipper creates the array: a program's own Array() is not used
      while (accept(Tok::LBracket)) {
        do {
          dims->args.push_back(parseExpr());
        } while (accept(Tok::Comma));
        expect(Tok::RBracket, "']'");
      }
      d.init = std::move(dims);
    } else if (accept(Tok::Assign)) {
      d.init = parseExpr();
    }
    st->decls.push_back(std::move(d));
  } while (accept(Tok::Comma));
  endOfStmt();
  return st;
}

StmtPtr Parser::parseIf() {
  auto st = std::make_unique<IfStmt>(cur().loc);
  SourceLoc openLoc = cur().loc;
  advance();
  IfStmt::Arm first;
  first.cond = parseExpr();
  endOfStmt();
  first.body = parseBlock({"ELSEIF", "ELSE", "ENDIF", "END"});
  st->arms.push_back(std::move(first));
  while (atStmtKw("ELSEIF")) {
    advance();
    IfStmt::Arm arm;
    arm.cond = parseExpr();
    endOfStmt();
    arm.body = parseBlock({"ELSEIF", "ELSE", "ENDIF", "END"});
    st->arms.push_back(std::move(arm));
  }
  if (atStmtKw("ELSE")) {
    advance();
    endOfStmt();
    st->elseBody = parseBlock({"ENDIF", "END"});
  }
  closeBlock({"ENDIF", "END"}, "IF", openLoc);
  return st;
}

// Statements before the first CASE of DO CASE / SWITCH are not allowed.
void Parser::rejectBeforeFirstCase(const char *construct, std::initializer_list<const char *> starts) {
  skipBlankStmts();
  if (at(Tok::Eof) || atBlockEnd(starts)) return;
  diag_.error(cur().loc, std::string("statements are not allowed between ") + construct + " and its first CASE");
  parseBlock(starts);
}

StmtPtr Parser::parseDoCase() {
  auto st = std::make_unique<IfStmt>(cur().loc);
  SourceLoc openLoc = cur().loc;
  advance(); // DO
  advance(); // CASE
  endOfStmt();
  rejectBeforeFirstCase("DO CASE", {"CASE", "OTHERWISE", "ENDCASE", "END"});
  while (atStmtKw("CASE")) {
    advance();
    IfStmt::Arm arm;
    arm.cond = parseExpr();
    endOfStmt();
    arm.body = parseBlock({"CASE", "OTHERWISE", "ENDCASE", "END"});
    st->arms.push_back(std::move(arm));
  }
  if (atStmtKw("OTHERWISE")) {
    advance();
    endOfStmt();
    st->elseBody = parseBlock({"ENDCASE", "END"});
  }
  closeBlock({"ENDCASE", "END"}, "DO CASE", openLoc);
  return st;
}

StmtPtr Parser::parseSwitch() {
  SourceLoc openLoc = cur().loc;
  advance();
  auto st = std::make_unique<SwitchStmt>(openLoc, parseExpr());
  endOfStmt();
  rejectBeforeFirstCase("SWITCH", {"CASE", "OTHERWISE", "ENDSWITCH", "END"});
  while (atStmtKw("CASE")) {
    advance();
    SwitchStmt::Case c;
    c.value = parseExpr();
    endOfStmt();
    c.body = parseBlock({"CASE", "OTHERWISE", "ENDSWITCH", "END"});
    st->cases.push_back(std::move(c));
  }
  if (atStmtKw("OTHERWISE")) {
    advance();
    endOfStmt();
    SwitchStmt::Case c;
    c.body = parseBlock({"ENDSWITCH", "END"});
    st->cases.push_back(std::move(c));
  }
  closeBlock({"ENDSWITCH", "END"}, "SWITCH", openLoc);
  return st;
}

StmtPtr Parser::parseForEach() {
  SourceLoc openLoc = cur().loc;
  auto st = std::make_unique<ForEachStmt>(openLoc);
  advance(); // FOR
  advance(); // EACH
  do {
    if (!at(Tok::Ident)) {
      expect(Tok::Ident, "loop variable");
      skipToEndOfStmt();
      return nullptr;
    }
    st->vars.push_back(std::make_unique<VarExpr>(cur().loc, cur().upper));
    advance();
  } while (accept(Tok::Comma));
  bool hasIn = atKw("IN");
  if (hasIn) {
    advance();
    do {
      st->collections.push_back(parseExpr());
    } while (accept(Tok::Comma));
    if (atKw("DESCEND")) {
      advance();
      st->descend = true;
    }
    if (st->vars.size() != st->collections.size())
      diag_.error(openLoc, "FOR EACH needs one collection per loop variable");
    endOfStmt();
  } else {
    diag_.error(cur().loc, "expected IN, " + found());
    skipToEndOfStmt();
    endOfStmt();
  }
  st->body = parseBlock({"NEXT", "END"});
  closeBlock({"NEXT", "END"}, "FOR EACH", openLoc);
  if (!hasIn) return nullptr;
  return st;
}

StmtPtr Parser::parseWhile() {
  SourceLoc openLoc = cur().loc;
  advance(); // WHILE
  auto st = std::make_unique<WhileStmt>(openLoc, parseExpr());
  endOfStmt();
  st->body = parseBlock({"ENDDO", "END"});
  closeBlock({"ENDDO", "END"}, "WHILE", openLoc);
  return st;
}

StmtPtr Parser::parseFor() {
  SourceLoc openLoc = cur().loc;
  auto st = std::make_unique<ForStmt>(openLoc);
  advance();
  if (!at(Tok::Ident)) {
    expect(Tok::Ident, "loop variable");
    skipToEndOfStmt();
    return nullptr;
  }
  st->var = std::make_unique<VarExpr>(cur().loc, cur().upper);
  advance();
  if (!accept(Tok::Assign) && !accept(Tok::Eq)) expect(Tok::Assign, "':='");
  st->start = parseExpr();
  if (atKw("TO")) advance();
  else expect(Tok::Ident, "TO");
  st->limit = parseExpr();
  if (atKw("STEP")) {
    advance();
    st->step = parseExpr();
  }
  endOfStmt();
  st->body = parseBlock({"NEXT", "END"});
  closeBlock({"NEXT", "END"}, "FOR", openLoc);
  return st;
}

} // namespace xb
