#pragma once

#include "common/Diagnostics.h"
#include "common/Token.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace xb {

// The xBase preprocessor. It owns tokenization and produces the token
// stream the parser consumes (statements separated by Tok::EndOfStmt).
//
// Directives: #define (object and function-like, case-sensitive), #undef,
// #ifdef, #ifndef, #if, #elif, #else, #endif, #include, #command, #xcommand,
// #translate, #xtranslate, #uncommand, #xuncommand, #untranslate,
// #xuntranslate, #error, #warning, #stdout, #pragma (ignored for now).
//
// Rules follow Clipper semantics:
//   match markers  <x> <x,...> <x:w1,w2> <*x*> <(x)> <!x!>, optional [ ]
//                  clauses (matched in any order and repeatedly, those
//                  without a keyword after the next one with one);
//   result markers <x> #<x> <"x"> <(x)> <{x}> <.x.>, repeating [ ] clauses.
// #command/#translate keywords may be abbreviated to 4 letters (dBASE
// style); the #x... variants require the full word. Commands match whole
// statements, translates match anywhere. Results are rescanned.
class Preprocessor {
public:
  explicit Preprocessor(Diagnostics &diag) : diag_(diag) {}

  void addIncludeDir(const std::string &dir) { includeDirs_.push_back(dir); }
  // -D on the command line: NAME or NAME=value (Directives.cpp)
  void defineFromCommandLine(const std::string &spec);

  // Processes a file; statements it produces go to the output stream.
  bool processFile(const std::string &path, SourceLoc from = {});
  std::string findInclude(const std::string &name, const std::string &currentFile) const;

  // Every file opened so far (sources and includes), for dependency files.
  const std::vector<std::string> &filesRead() const { return filesRead_; }

  // Tokens produced so far (the driver marks where the program's own output starts).
  size_t outputSize() const { return out_.size(); }
  // Terminates the stream with Tok::Eof and hands it over.
  std::vector<Token> takeOutput();
  // Text form of a token stream (.ppo files, stringification).
  static std::string render(const std::vector<Token> &toks, size_t from = 0, size_t to = SIZE_MAX);

private:
  using Toks = std::vector<Token>;

  enum class Cmp { Case, Std, DBase };
  enum class MK { Regular, List, Restricted, Wild, Extended, Name };
  enum class RK { Regular, Dumb, Normal, Smart, Blockify, Logify };

  struct Marker {
    std::string name;            // upper-cased
    MK kind = MK::Regular;
    bool repeatable = false;     // may take several values: used in the result only inside optional clauses
    std::vector<Toks> words;     // Restricted: accepted words
  };
  struct PNode {                 // pattern (match) node
    enum K { Lit, Mark, Opt } k = Lit;
    Token tok;
    int marker = -1;
    std::vector<PNode> kids;
  };
  struct RNode {                 // result node
    enum K { Lit, Mark, Opt } k = Lit;
    Token tok;
    int marker = -1;
    RK rk = RK::Regular;
    std::vector<RNode> kids;
  };
  struct Rule {
    bool command = true;         // #command (whole statement) vs #translate
    bool exact = false;          // #x... variants
    std::vector<Marker> markers;
    std::vector<PNode> pattern;
    std::vector<RNode> result;
    std::string patternKey;      // normalized pattern text (#un... directives)
  };
  struct Define {
    bool funcLike = false;
    std::vector<std::string> params;
    Toks body;
    SourceLoc loc;
  };
  struct Cond {
    bool parentActive, active, taken, seenElse;
    SourceLoc loc;
  };
  using Caps = std::vector<std::vector<Toks>>; // per marker: list of matches

  // Preprocessor.cpp: lines and statements
  void processLine(Toks &line);
  struct Rewrite {              // shared by everything one source statement expands into
    int budget;
    bool failed = false;
  };
  void processStatement(Toks st, int depth, Rewrite &rw);
  bool splitStatements(Toks &st, int depth, Rewrite &rw);
  void rewriteFailed(Rewrite &rw, SourceLoc loc, const char *msg);
  void emit(const Toks &st);
  bool expandDefines(Toks &st, int &budget);
  bool applyTranslates(Toks &st, int &budget);
  bool applyCommand(Toks &st, int &budget);
  static std::string quoteString(const std::string &value);

  // Directives.cpp
  void directive(Toks &line);
  bool active() const { return conds_.empty() || conds_.back().active; }
  bool evalCondition(Toks expr, SourceLoc loc);

  // RuleParse.cpp
  void defineRule(Toks &line, bool command, bool exact);
  void undefineRule(Toks &line, bool command, bool exact);
  bool parseRuleParts(Toks &line, Rule &r, bool withResult);
  bool parsePattern(const Toks &t, size_t from, size_t to, Rule &r, std::vector<PNode> &out);
  bool parseMatchMarker(const Toks &t, size_t &i, size_t to, Rule &r, PNode &node);
  bool parseResult(const Toks &t, size_t from, size_t to, Rule &r, std::vector<RNode> &out);
  int findMarker(const Rule &r, const std::string &name) const;

  // RuleMatch.cpp
  bool matchSeq(const Rule &r, const std::vector<PNode> &nodes, const Toks &in, size_t &pos, size_t end,
                const PNode *outerStop, Caps &caps) const;
  bool matchMarker(const Marker &m, const Toks &in, size_t &pos, size_t end, const PNode *stop, Cmp mode) const;
  static bool tokEqual(const Token &a, const Token &pattern, Cmp mode);
  static bool canStartExp(const Toks &in, size_t pos, size_t end);
  static bool skipExp(const Toks &in, size_t &pos, size_t end, const PNode *stop, Cmp mode, bool *fStop);
  static bool skipMacro(const Toks &in, size_t &pos, size_t end);

  // RuleResult.cpp
  void buildResult(const Rule &r, const std::vector<RNode> &nodes, const Caps &caps, int rep, SourceLoc loc,
                   Toks &out) const;
  static size_t repeatCount(const std::vector<RNode> &nodes, const Caps &caps);
  static std::vector<Toks> splitList(const Toks &t);
  static Token makeString(const std::string &value, SourceLoc loc, bool space);

  Diagnostics &diag_;
  // Set by matchSeq when a marker that cannot repeat met a second value: the
  // rule does not match, however its optional clauses are tried.
  mutable bool overflow_ = false;
  std::vector<std::string> includeDirs_;
  std::vector<std::string> fileStack_;
  std::vector<std::string> filesRead_;
  std::unordered_map<std::string, Define> defines_;
  std::vector<Rule> rules_;
  std::vector<Cond> conds_;
  Toks out_;
};

} // namespace xb
