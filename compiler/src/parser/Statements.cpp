// Statements: declarations, IF, DO CASE, SWITCH, loops.
#include "parser/Parser.h"

#include "xbrt.h" // the literal width rule

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
  if (atStmtKw("WITH") && isKw(ahead(), "OBJECT")) return parseWithObject();
  if (atStmtKw("USING") && aheadOperand()) return parseUsing(); // USING( ... ) is a call, as YIELD( ... )
  if (atStmtKw("FOR")) return isKw(ahead(), "EACH") && !isAssignAfterEach() ? parseForEach() : parseFor();
  if (atStmtKw("AWAIT") && isKw(ahead(), "FOR") && isKw(ahead(2), "EACH")) return parseAwaitEach();
  // YIELD( ... ) stays a call of a function Yield(), as in Clipper
  if (atStmtKw("YIELD") && aheadOperand()) {
    advance();
    ExprPtr v = parseExpr();
    endOfStmt();
    if (braceBlocks_ || !parsingFunc_) diag_.error(loc, "YIELD is valid only in a function or method");
    else parsingFunc_->generator = true;
    return std::make_unique<YieldStmt>(loc, std::move(v));
  }
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
  bool doMacro = atStmtKw("DO") && ahead().kind == Tok::Amp &&
                 ((ahead(2).kind == Tok::Ident && !ahead(2).spaceBefore) || ahead(2).kind == Tok::LParen);
  if (atStmtKw("DO") && (ahead().kind == Tok::Ident || doMacro)) { // DO proc [WITH args], DO &name [WITH args]
    advance();
    ExprPtr call;
    std::vector<ExprPtr> *args;
    if (doMacro) { // the procedure the text names when the statement runs
      SourceLoc callLoc = cur().loc;
      auto m = std::make_unique<MacroCallExpr>(callLoc, parseMacroOperand());
      args = &m->args;
      call = std::move(m);
    } else {
      auto c = std::make_unique<CallExpr>(cur().loc, cur().upper);
      advance();
      args = &c->args;
      call = std::move(c);
    }
    if (atKw("WITH")) { // empty arguments pass NIL, as in a call; a variable is passed by reference
      advance();
      do {
        if (at(Tok::Comma) || atEndOfStmt()) {
          args->push_back(std::make_unique<NilExpr>(cur().loc));
        } else if (at(Tok::At) && !atFunctionRef()) {
          args->push_back(parseRef());
        } else { // a function reference is a value, which an operator or a message may follow
          ExprPtr arg = parseExpr();
          if (arg->kind == ExprKind::Var && !arg->parenthesized) {
            SourceLoc argLoc = arg->loc;
            arg = std::make_unique<RefExpr>(argLoc, std::move(arg));
          }
          args->push_back(std::move(arg));
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
  if (atStmtKw("FIELD")) return parseDecl(StmtKind::Field);

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
  case ExprKind::Macro: case ExprKind::MacroCall: case ExprKind::Await: // a macro runs code; AWAIT waits
    break;
  case ExprKind::Send:
    if (static_cast<const SendExpr &>(*e).withArgs || static_cast<const SendExpr &>(*e).messageText) break;
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
  bool namesOnly = kind == StmtKind::Memvar || kind == StmtKind::Parameters || kind == StmtKind::Field;
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
    }
    d.declared = parseDeclaredType(); // LOCAL n AS NUMERIC := 0, LOCAL a[ 3 ] AS ARRAY
    if (!d.init && accept(Tok::Assign)) d.init = parseExpr();
    if (namesOnly && d.init) diag_.error(d.loc, keyword + " takes names only");
    st->decls.push_back(std::move(d));
  } while (accept(Tok::Comma));
  if (kind == StmtKind::Field && atKw("IN")) { // FIELD names IN alias
    advance();
    if (at(Tok::Ident)) {
      st->fieldAlias = cur().upper;
      advance();
    } else {
      expect(Tok::Ident, "an alias");
    }
  }
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

// AWAIT FOR EACH [k =>] x IN expr ... NEXT: the statement AwaitEachStmt builds (AST.h); one variable (with its key)
// and one collection, not DESCEND.
StmtPtr Parser::parseAwaitEach() {
  SourceLoc openLoc = cur().loc;
  advance(); // AWAIT
  advance(); // FOR
  advance(); // EACH
  auto st = std::make_unique<AwaitEachStmt>(openLoc);
  std::string n = std::to_string(++awaitEachCount_);
  auto hidden = [&](const std::string &what) { return "(AWAIT EACH " + what + " " + n + ")"; };
  auto var = [&](const std::string &name) { return std::make_unique<VarExpr>(openLoc, name); };
  std::string x, k;
  bool ok = true;
  auto loopVar = [&](std::string &name) {
    if (ok && at(Tok::Ident)) {
      name = cur().upper;
      advance();
    } else if (ok) {
      expect(Tok::Ident, "loop variable");
      ok = false;
    }
  };
  loopVar(x);
  if (ok && accept(Tok::FatArrow)) {
    k = x;
    loopVar(x);
    if (ok && k == x) diag_.error(openLoc, "AWAIT FOR EACH k => v needs two different variables");
  }
  ExprPtr source;
  if (ok && at(Tok::Comma)) {
    diag_.error(cur().loc, "AWAIT FOR EACH walks one collection with one variable");
    ok = false;
  }
  if (ok && atKw("IN")) {
    advance();
    source = parseExpr();
    if (at(Tok::Comma)) {
      diag_.error(cur().loc, "AWAIT FOR EACH walks one collection with one variable");
      ok = false;
    } else if (atKw("DESCEND")) {
      diag_.error(cur().loc, "AWAIT FOR EACH cannot DESCEND: its elements come one at a time");
      ok = false;
    }
  } else if (ok) {
    diag_.error(cur().loc, "expected IN, " + found());
    ok = false;
  }
  if (!ok) {
    skipToEndOfStmt();
    endOfStmt();
    parseBlock({"NEXT", "END"});
    closeBlock({"NEXT", "END"}, "AWAIT FOR EACH", openLoc);
    return nullptr;
  }
  endOfStmt();

  auto declare = [&](const std::string &name, ExprPtr init) {
    LocalDecl d;
    d.loc = openLoc;
    d.name = name;
    d.init = std::move(init);
    st->hidden.push_back(std::move(d));
  };
  auto each = std::make_unique<CallExpr>(openLoc, "__AWAITEACH");
  each->args.push_back(std::move(source));
  declare(hidden("ENUMERATOR"), std::move(each));
  declare(hidden("VALUE"), var(x));
  if (!k.empty()) {
    declare(hidden("KEY"), var(k));
    declare(hidden("POSITION"),
            std::make_unique<NumberExpr>(openLoc, true, 0, 0.0, 0, xb_num_literal_width(0, 0, 0.0, 1, 0)));
  }

  auto seq = std::make_unique<SequenceStmt>(openLoc);
  seq->leavable = true;
  seq->hasAlways = true;
  seq->restoresOnly = true;
  auto send = [&](const char *message) {
    auto s = std::make_unique<SendExpr>(openLoc, var(hidden("ENUMERATOR")), message);
    s->withArgs = true;
    return s;
  };
  auto await = [&](ExprPtr operand) {
    auto e = std::make_unique<AwaitExpr>(openLoc, std::move(operand));
    e->generated = true;
    return e;
  };
  auto loop = std::make_unique<WhileStmt>(openLoc, await(send("MOVENEXTASYNC")));
  auto current = [&] { return std::make_unique<SendExpr>(openLoc, var(hidden("ENUMERATOR")), "CURRENT"); };
  auto awaits = std::make_unique<CallExpr>(openLoc, "__AWAITEACHAWAITS");
  awaits->args.push_back(var(hidden("ENUMERATOR")));
  auto value = std::make_unique<IIfExpr>(openLoc, std::move(awaits), await(current()), current());
  loop->body.push_back(
      std::make_unique<ExprStmt>(openLoc, std::make_unique<AssignExpr>(openLoc, var(x), std::move(value))));
  if (!k.empty()) { // k: the position, or a hash's key (__AwaitEachKey)
    auto key = std::make_unique<CallExpr>(openLoc, "__AWAITEACHKEY");
    key->args.push_back(var(hidden("ENUMERATOR")));
    key->args.push_back(std::make_unique<IncDecExpr>(openLoc, true, true, var(hidden("POSITION"))));
    loop->body.push_back(
        std::make_unique<ExprStmt>(openLoc, std::make_unique<AssignExpr>(openLoc, var(k), std::move(key))));
  }
  for (StmtPtr &b : parseBlock({"NEXT", "END"})) loop->body.push_back(std::move(b));
  closeBlock({"NEXT", "END"}, "AWAIT FOR EACH", openLoc);
  seq->body.push_back(std::move(loop));
  seq->always.push_back(std::make_unique<ExprStmt>(
      openLoc, std::make_unique<AssignExpr>(openLoc, var(x), var(hidden("VALUE")))));
  if (!k.empty())
    seq->always.push_back(std::make_unique<ExprStmt>(
        openLoc, std::make_unique<AssignExpr>(openLoc, var(k), var(hidden("KEY")))));
  st->seq.push_back(std::move(seq));
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

// WITH OBJECT expr ... END (std.ch makes END WITH an END); `:name` inside names the object (parsePrimary).
StmtPtr Parser::parseWithObject() {
  SourceLoc openLoc = cur().loc;
  advance(); // WITH
  advance(); // OBJECT
  auto st = std::make_unique<WithObjectStmt>(openLoc);
  st->object = parseExpr();
  endOfStmt();
  st->hidden.loc = openLoc;
  st->hidden.name = "(WITH " + std::to_string(++withCount_) + ")";
  withObjects_.push_back(st->hidden.name);
  st->body = parseBlock({"END"});
  withObjects_.pop_back();
  closeBlock({"END"}, "WITH OBJECT", openLoc);
  return st;
}

// Whether the keyword at hand is followed by an operand, not by what ends the statement or calls a function of its name
// (an assignment, an index, a message or an alias makes it a variable already: atStmtKw).
bool Parser::aheadOperand() const {
  Tok k = ahead().kind;
  return k != Tok::EndOfStmt && k != Tok::Eof && k != Tok::Semicolon && k != Tok::LParen;
}

// USING [v :=] expr ... END (std.ch makes END USING an END). `v := expr` becomes an assignment from the hidden
// variable inside the sequence: an assignment that fails still disposes the value.
StmtPtr Parser::parseUsing() {
  SourceLoc openLoc = cur().loc;
  advance(); // USING
  auto st = std::make_unique<UsingStmt>(openLoc);
  st->hidden.loc = openLoc;
  st->hidden.name = "(USING " + std::to_string(++usingCount_) + ")";
  auto seq = std::make_unique<SequenceStmt>(openLoc);
  ExprPtr value = parseExpr();
  if (value->kind == ExprKind::Assign && !static_cast<AssignExpr &>(*value).compound) {
    auto &assign = static_cast<AssignExpr &>(*value);
    st->value = std::move(assign.value);
    assign.value = std::make_unique<VarExpr>(openLoc, st->hidden.name);
    seq->body.push_back(std::make_unique<ExprStmt>(openLoc, std::move(value)));
  } else {
    st->value = std::move(value);
  }
  endOfStmt();
  seq->leavable = true;
  seq->hasAlways = true;
  auto dispose = std::make_unique<CallExpr>(openLoc, "__DISPOSE");
  dispose->args.push_back(std::make_unique<VarExpr>(openLoc, st->hidden.name));
  seq->always.push_back(std::make_unique<ExprStmt>(openLoc, std::move(dispose)));
  for (StmtPtr &b : parseBlock({"END"})) seq->body.push_back(std::move(b));
  closeBlock({"END"}, "USING", openLoc);
  st->seq.push_back(std::move(seq));
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
  st->leavable = true;
  auto block = std::make_unique<BlockExpr>(openLoc);
  block->params.push_back({openLoc, "E", {}});
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
