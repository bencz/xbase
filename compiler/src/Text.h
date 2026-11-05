#pragma once

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace xb {

inline std::string upperCase(std::string_view s) {
  std::string u(s);
  for (char &c : u) c = (char)std::toupper((unsigned char)c);
  return u;
}

// Shortest abbreviation dBASE-style keywords accept (#command words, directive names).
constexpr size_t kMinAbbreviation = 4;

// `input` (upper-cased) names `word`: the whole word, or a prefix of at least
// `minLen` characters. Keywords, directives and rule words all use this rule.
inline bool keywordMatches(std::string_view input, std::string_view word, size_t minLen = kMinAbbreviation) {
  if (input.size() > word.size()) return false;
  if (input.size() < word.size() && input.size() < minLen) return false;
  return word.compare(0, input.size(), input) == 0;
}

} // namespace xb
