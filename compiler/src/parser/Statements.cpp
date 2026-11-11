// Statements: declarations, IF, DO CASE, SWITCH, loops.
#include "parser/Parser.h"

namespace xb {

// Every statement records where it ends: Clipper shows that line for all of
// it, also one continued over several lines.
StmtPtr Parser::parseStmt() {
  uint32_t line = stmtEndLine();
  StmtPtr s = parseStmtKind();
  if (s) s->line = line;
  return s;
}

uint32_t Parser::stmtEndLine() const {
  size_t p = pos_;
  while (p + 1 < toks_.size() && toks_[p].kind != Tok::EndOfStmt && toks_[p].kind != Tok::Eof) ++p;
  return toks_[p].loc.line;
}

StmtPtr Parser::parseStmtKind() {
  SourceLoc loc = cur().loc;

  if (atExtern()) { // only nested blocks get here: a function body ends at it
    diag_.error(loc, "EXTERN FUNCTION must be outside of functions");
    skipToEndOfStmt();
    endOfStmt();
    return nullptr;
  }
  if (atLinkDecl()) {
    parseLinkDecl();
    return nullptr;
  }
  if (atStmtKw("LOCAL")) return parseDecl(StmtKind::Local);
  if (atStmtKw("STATIC")) return parseDecl(StmtKind::Static);
  if (atStmtKw("DO") && isKw(ahead(), "CASE")) return parseDoCase();
  if (atStmtKw("SWITCH")) return parseSwitch();
  if (atStmtKw("IF") && !atInlineIf()) return parseIf();
  if (atStmtKw("WHILE")) return parseWhile();
  if (atStmtKw("BEGIN") && isKw(ahead(), "SEQUENCE")) return parseSequence();
  if (atStmtKw("TRY") && ahead().kind == Tok::EndOfStmt) return parseTry();
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
                             "ENDSWITCH", "RECOVER"}) {
    if (atStmtKw(closer)) {
      diag_.error(loc, std::string(closer) + " does not match any open block");
      skipToEndOfStmt();
      return nullptr;
    }
  }
  if (atStmtKw("PRIVATE") || atStmtKw("DECLARE")) return parseDecl(StmtKind::Private); // DECLARE: Clipper's PRIVATE
  if (atStmtKw("PUBLIC")) return parseDecl(StmtKind::Public);
  if (atStmtKw("MEMVAR")) return parseDecl(StmtKind::Memvar);
  if (atStmtKw("PARAMETERS")) return parseDecl(StmtKind::Parameters);
  if (atStmtKw("FIELD")) {
    diag_.error(loc, "'" + cur().text + "' is not supported yet");
    skipToEndOfStmt();
    return nullptr;
  }

  // A statement `operand = expr` is an assignment (`x = y = 3` is
  // `x := ( y = 3 )`), whose target must be an lvalue (isLvalue), also in
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
  case ExprKind::Macro: case ExprKind::MacroCall: // a macro runs code
    break;
  case ExprKind::Send:
    if (static_cast<const SendExpr &>(*e).withArgs) break;
    [[fallthrough]];
  default:
    diag_.warning(e->loc, "meaningless use of expression");
  }
  endOfStmt();
  return std::make_unique<ExprStmt>(loc, std::move(e));
}

// LOCAL, STATIC, PRIVATE (or DECLARE) and PUBLIC: names, each with an initializer or array
// dimensions; MEMVAR and PARAMETERS: names only.
StmtPtr Parser::parseDecl(StmtKind kind) {
  auto st = std::make_unique<LocalStmt>(cur().loc, kind);
  std::string keyword = cur().upper;
  bool namesOnly = kind == StmtKind::Memvar || kind == StmtKind::Parameters;
  advance();
  bool macros = kind == StmtKind::Private || kind == StmtKind::Public; // PRIVATE &name: named when the code runs
  do {
    LocalDecl d;
    d.loc = cur().loc;
    if (macros && atMacroParen()) {
      advance();
      advance();
      d.macro = parseExpr();
      expect(Tok::RParen, "')'");
    } else if (macros && atMacroName()) {
      d.macro = parseMacroText();
    } else if (!at(Tok::Ident)) {
      expect(Tok::Ident, "variable name");
      skipToEndOfStmt();
      break;
    } else {
      d.name = cur().upper;
      advance();
    }
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
    if (namesOnly && d.init) diag_.error(d.loc, keyword + " takes names only");
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
  first.line = stmtEndLine();
  first.cond = parseExpr();
  endOfStmt();
  first.body = parseBlock({"ELSEIF", "ELSE", "ENDIF", "END"});
  st->arms.push_back(std::move(first));
  while (atStmtKw("ELSEIF")) {
    advance();
    IfStmt::Arm arm;
    arm.line = stmtEndLine();
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
    arm.line = stmtEndLine();
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
  bool ok = true;
  auto loopVar = [&] {
    if (!at(Tok::Ident)) {
      expect(Tok::Ident, "loop variable");
      ok = false;
      return;
    }
    st->vars.push_back(std::make_unique<VarExpr>(cur().loc, cur().upper));
    advance();
  };
  loopVar();
  if (ok && accept(Tok::FatArrow)) { // FOR EACH k => v: the first is the key
    st->key = std::move(st->vars.back());
    st->vars.clear();
    loopVar();
    const auto &key = static_cast<const VarExpr &>(*st->key);
    if (ok && key.name == static_cast<const VarExpr &>(*st->vars[0]).name)
      diag_.error(key.loc, "FOR EACH k => v needs two different variables");
    if (ok && at(Tok::Comma)) {
      diag_.error(cur().loc, "FOR EACH k => v walks one collection");
      ok = false;
    }
  } else {
    while (ok && accept(Tok::Comma)) loopVar();
  }
  if (ok && atKw("IN")) {
    advance();
    do {
      st->collections.push_back(parseExpr());
    } while (accept(Tok::Comma));
    if (atKw("DESCEND")) {
      advance();
      st->descend = true;
    }
    if (st->key && st->collections.size() != 1)
      diag_.error(openLoc, "FOR EACH k => v walks one collection");
    else if (st->vars.size() != st->collections.size())
      diag_.error(openLoc, "FOR EACH needs one collection per loop variable");
    endOfStmt();
  } else {
    if (ok) diag_.error(cur().loc, "expected IN, " + found());
    ok = false;
    skipToEndOfStmt();
    endOfStmt();
  }
  st->body = parseBlock({"NEXT", "END"});
  closeBlock({"NEXT", "END"}, "FOR EACH", openLoc);
  if (!ok) return nullptr;
  return st;
}

// BEGIN SEQUENCE [WITH expr] ... [RECOVER [USING var]] ... [ALWAYS] ... END (std.ch makes END SEQUENCE and
// ENDSEQUENCE an END)
StmtPtr Parser::parseSequence() {
  SourceLoc openLoc = cur().loc;
  advance(); // BEGIN
  advance(); // SEQUENCE
  auto st = std::make_unique<SequenceStmt>(openLoc);
  if (at(Tok::Ident) && cur().upper == "WITH") {
    advance();
    st->handler = parseExpr();
  }
  endOfStmt();
  parseSequenceParts(*st, "RECOVER", "ALWAYS", "BEGIN SEQUENCE", openLoc);
  return st;
}

// TRY ... [CATCH [var]] ... [FINALLY] ... END (std.ch makes END TRY an END): a sequence whose error block breaks
// with the error, {| e | Break( e ) }.
StmtPtr Parser::parseTry() {
  SourceLoc openLoc = cur().loc;
  advance(); // TRY
  endOfStmt();
  auto st = std::make_unique<SequenceStmt>(openLoc);
  st->isTry = true;
  auto block = std::make_unique<BlockExpr>(openLoc);
  block->params.push_back({openLoc, "E"});
  auto call = std::make_unique<CallExpr>(openLoc, "BREAK");
  call->args.push_back(std::make_unique<VarExpr>(openLoc, "E"));
  block->body.push_back(std::move(call));
  st->handler = std::move(block);
  parseSequenceParts(*st, "CATCH", "FINALLY", "TRY", openLoc);
  return st;
}

// The body, then `recoverWord` [USING] var (CATCH takes the variable alone), then `alwaysWord`, then END.
void Parser::parseSequenceParts(SequenceStmt &st, const char *recoverWord, const char *alwaysWord, const char *what,
                                SourceLoc openLoc) {
  st.body = parseBlock({recoverWord, alwaysWord, "END"});
  if (atStmtKw(recoverWord)) {
    advance();
    st.hasRecover = true;
    bool named = st.isTry ? at(Tok::Ident) : atKw("USING");
    if (named && !st.isTry) advance(); // USING
    if (named && at(Tok::Ident)) {
      st.usingVar = std::make_unique<VarExpr>(cur().loc, cur().upper);
      advance();
    } else if (named) {
      expect(Tok::Ident, "variable name");
    }
    endOfStmt();
    st.recover = parseBlock({alwaysWord, "END"});
  }
  if (atStmtKw(alwaysWord)) {
    advance();
    endOfStmt();
    st.hasAlways = true;
    st.always = parseBlock({"END"});
  }
  closeBlock({"END"}, what, openLoc);
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
