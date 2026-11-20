#pragma once
// To-do lists for CrossPoint (added by the Crosspoint-ToDoList patch).
//
// Pure C++17, no device code: parsing, saving, merging and page layout of a list file.
// Everything here runs and is tested on a PC (tests/test_listcore.cpp).
//
// File format, one entry per line:
//   [ ] open entry
//   [x] done entry
// Lines without a marker count as open entries; they get "[ ] " the first time the file is saved.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ListPath.h"

namespace todolist {

// The most text one "add" request may carry. (The lists folder itself is in ListPath.h.)
constexpr size_t kMaxAddBytes = 32 * 1024;
// Lists larger than this are not loaded (protects the small X3/X4 memory).
constexpr size_t kMaxListBytes = 256 * 1024;
// Longest list name (bytes, UTF-8), without ".txt".
constexpr size_t kMaxNameBytes = 60;

struct Item {
  bool done = false;
  std::string text;
};

namespace detail {

inline bool isSpace(const unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; }

inline std::string trim(const std::string& s) {
  size_t b = 0;
  size_t e = s.size();
  while (b < e && isSpace(static_cast<unsigned char>(s[b]))) b++;
  while (e > b && isSpace(static_cast<unsigned char>(s[e - 1]))) e--;
  // Non-breaking space (U+00A0, C2 A0) around an entry is noise from copy and paste.
  while (e - b >= 2 && static_cast<unsigned char>(s[b]) == 0xC2 && static_cast<unsigned char>(s[b + 1]) == 0xA0) {
    b += 2;
    while (b < e && isSpace(static_cast<unsigned char>(s[b]))) b++;
  }
  while (e - b >= 2 && static_cast<unsigned char>(s[e - 2]) == 0xC2 && static_cast<unsigned char>(s[e - 1]) == 0xA0) {
    e -= 2;
    while (e > b && isSpace(static_cast<unsigned char>(s[e - 1]))) e--;
  }
  return s.substr(b, e - b);
}

inline bool startsWith(const std::string& s, const char* prefix) {
  size_t i = 0;
  for (; prefix[i] != '\0'; i++) {
    if (i >= s.size() || s[i] != prefix[i]) return false;
  }
  return true;
}

// Removes one leading bullet ("-", "*", "+", "•", "◦", "▪", "–", "—") or number ("1." / "1)").
inline std::string stripBullet(const std::string& s) {
  static const char* const bullets[] = {"\xE2\x80\xA2", "\xE2\x97\xA6", "\xE2\x96\xAA", "\xE2\x80\x93",
                                        "\xE2\x80\x94", "\xE2\x81\x83", "-", "*", "+"};
  for (const char* b : bullets) {
    if (startsWith(s, b)) {
      const size_t n = std::char_traits<char>::length(b);
      // "-" etc. only counts as a bullet when a space follows ("-5 °C" stays as it is).
      if (s.size() > n && isSpace(static_cast<unsigned char>(s[n]))) return trim(s.substr(n));
      if (s.size() == n) return "";  // a bullet with nothing after it is no entry
    }
  }
  size_t i = 0;
  while (i < s.size() && i < 3 && s[i] >= '0' && s[i] <= '9') i++;
  if (i > 0 && i + 1 < s.size() && (s[i] == '.' || s[i] == ')') && isSpace(static_cast<unsigned char>(s[i + 1]))) {
    return trim(s.substr(i + 1));
  }
  return s;
}

// Reads a marker "[ ]", "[x]" or "[X]" at the start (after an optional bullet, so Markdown
// "- [ ] milk" works). Returns true and sets done/rest when one is found.
inline bool readMarker(const std::string& line, bool& done, std::string& rest) {
  std::string s = line;
  for (const char* b : {"- ", "* ", "+ "}) {
    if (startsWith(s, b)) {
      s = trim(s.substr(2));
      break;
    }
  }
  if (s.size() < 3 || s[0] != '[' || s[2] != ']') return false;
  if (s[1] == ' ') {
    done = false;
  } else if (s[1] == 'x' || s[1] == 'X') {
    done = true;
  } else {
    return false;
  }
  rest = trim(s.substr(3));
  return true;
}

// Case-insensitive key for duplicate checks: ASCII and the Latin-1 letters (ÄÖÜ, ÀÉ ...).
inline std::string foldKey(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  bool lastSpace = false;
  for (size_t i = 0; i < s.size(); i++) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (isSpace(c)) {
      if (!lastSpace && !out.empty()) out.push_back(' ');
      lastSpace = true;
      continue;
    }
    lastSpace = false;
    if (c >= 'A' && c <= 'Z') {
      out.push_back(static_cast<char>(c + 32));
    } else if (c == 0xC3 && i + 1 < s.size()) {
      unsigned char d = static_cast<unsigned char>(s[i + 1]);
      if (d >= 0x80 && d <= 0x9E && d != 0x97) d = static_cast<unsigned char>(d + 0x20);
      out.push_back(static_cast<char>(c));
      out.push_back(static_cast<char>(d));
      i++;
    } else if (c == 0xD0 && i + 1 < s.size()) {  // Cyrillic upper case -> lower case
      const unsigned char d = static_cast<unsigned char>(s[i + 1]);
      if (d >= 0x90 && d <= 0x9F) {  // А..П -> а..п
        out.push_back(static_cast<char>(0xD0));
        out.push_back(static_cast<char>(d + 0x20));
      } else if (d >= 0xA0 && d <= 0xAF) {  // Р..Я -> р..я
        out.push_back(static_cast<char>(0xD1));
        out.push_back(static_cast<char>(d - 0x20));
      } else if (d == 0x81) {  // Ё -> ё
        out.push_back(static_cast<char>(0xD1));
        out.push_back(static_cast<char>(0x91));
      } else {
        out.push_back(static_cast<char>(c));
        out.push_back(static_cast<char>(d));
      }
      i++;
    } else {
      out.push_back(static_cast<char>(c));
    }
  }
  if (!out.empty() && out.back() == ' ') out.pop_back();
  return out;
}

}  // namespace detail

// One incoming or stored line -> entry. Returns false for lines that hold no entry (empty).
inline bool parseLine(const std::string& raw, Item& out) {
  std::string line = detail::trim(raw);
  if (line.empty()) return false;
  bool done = false;
  std::string rest;
  if (detail::readMarker(line, done, rest)) {
    if (rest.empty()) return false;
    out.done = done;
    out.text = rest;
    return true;
  }
  rest = detail::stripBullet(line);
  if (rest.empty()) return false;
  out.done = false;
  out.text = rest;
  return true;
}

// Whole file (or incoming text) -> entries. Handles a UTF-8 BOM and \n, \r\n or \r line ends.
// Commas and semicolons separate entries, wherever the text comes from (a file, a phone, the
// reader's keyboard): "Milk, Bread; Cheese" is three entries. A comma between two digits stays,
// so "1,5 kg flour" is one entry. For a comma inside an entry, write a dash: "Apples - red ones".
inline bool isSeparatorAt(const std::string& text, const size_t i) {
  const char c = text[i];
  if (c == ';') return true;
  if (c != ',') return false;
  const auto digit = [](const char d) { return d >= '0' && d <= '9'; };
  return !(i > 0 && i + 1 < text.size() && digit(text[i - 1]) && digit(text[i + 1]));
}

inline std::string commasToLines(const std::string& typed) {
  std::string out = typed;
  for (size_t i = 0; i < out.size(); i++) {
    if (isSeparatorAt(typed, i)) out[i] = '\n';
  }
  return out;
}

inline std::vector<Item> parse(const std::string& text) {
  std::vector<Item> items;
  size_t pos = 0;
  if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB &&
      static_cast<unsigned char>(text[2]) == 0xBF) {
    pos = 3;
  }
  std::string line;
  auto flush = [&]() {
    Item item;
    if (parseLine(line, item)) {
      // One line may hold several entries ("[x] Milk, Bread"); each keeps the line's tick.
      const std::string parts = commasToLines(item.text);
      size_t start = 0;
      while (start <= parts.size()) {
        size_t end = parts.find('\n', start);
        if (end == std::string::npos) end = parts.size();
        Item part;
        if (parseLine(parts.substr(start, end - start), part)) {
          part.done = item.done;
          items.push_back(std::move(part));
        }
        start = end + 1;
      }
    }
    line.clear();
  };
  for (; pos < text.size(); pos++) {
    const char c = text[pos];
    if (c == '\n') {
      flush();
    } else if (c == '\r') {
      flush();
      if (pos + 1 < text.size() && text[pos + 1] == '\n') pos++;
    } else {
      line.push_back(c);
    }
  }
  flush();
  return items;
}

inline std::string serialize(const std::vector<Item>& items) {
  std::string out;
  size_t size = 0;
  for (const auto& item : items) size += item.text.size() + 5;
  out.reserve(size);
  for (const auto& item : items) {
    out += item.done ? "[x] " : "[ ] ";
    out += item.text;
    out += '\n';
  }
  return out;
}

struct AddResult {
  int added = 0;     // new entries appended at the end
  int reopened = 0;  // entries that were done and are open again
  int skipped = 0;   // entries that were already open
};

// Adds incoming text to a list without ever replacing it:
//  - new entry          -> appended at the end, open
//  - already open       -> skipped (no duplicates)
//  - already done       -> opened again ("milk" is back on the shopping list)
// The incoming marker ([x]) is ignored: what is sent is something to do.
// New entries go to the end, or from position insertAt on (used when an edit becomes several).
inline AddResult merge(std::vector<Item>& items, const std::string& incoming, size_t insertAt = SIZE_MAX) {
  AddResult result;
  if (insertAt > items.size()) insertAt = items.size();
  for (Item& add : parse(incoming)) {
    const std::string key = detail::foldKey(add.text);
    bool found = false;
    for (Item& existing : items) {
      if (detail::foldKey(existing.text) != key) continue;
      found = true;
      if (existing.done) {
        existing.done = false;
        result.reopened++;
      } else {
        result.skipped++;
      }
      break;
    }
    if (!found) {
      add.done = false;
      items.insert(items.begin() + static_cast<std::ptrdiff_t>(insertAt), std::move(add));
      insertAt++;
      result.added++;
    }
  }
  return result;
}

// Editing an entry on the reader. The first part replaces the entry's text; any further parts
// (separated as in commasToLines) become new entries right below it. Returns false when nothing
// usable was typed: the caller treats that as deleting the entry.
inline bool applyEdit(std::vector<Item>& items, const size_t index, const std::string& typed) {
  if (index >= items.size()) return false;
  const std::vector<Item> parts = parse(commasToLines(typed));
  if (parts.empty()) return false;
  items[index].text = parts[0].text;
  std::string rest;
  for (size_t i = 1; i < parts.size(); i++) rest += parts[i].text + "\n";
  if (!rest.empty()) merge(items, rest, index + 1);
  return true;
}

// List name -> safe file name part (without ".txt"). Empty result = not usable.
inline std::string sanitizeName(const std::string& raw) {
  std::string out;
  for (const char ch : raw) {
    const unsigned char c = static_cast<unsigned char>(ch);
    if (c < 0x20 || c == 0x7F) continue;
    if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
      continue;
    }
    out.push_back(ch);
  }
  out = detail::trim(out);
  // Strip a ".txt" the sender may have added, and leading/trailing dots (hidden or odd files).
  if (out.size() >= 4) {
    std::string tail = out.substr(out.size() - 4);
    for (char& c : tail) c = static_cast<char>((c >= 'A' && c <= 'Z') ? c + 32 : c);
    if (tail == ".txt") out = detail::trim(out.substr(0, out.size() - 4));
  }
  while (!out.empty() && out.front() == '.') out.erase(out.begin());
  while (!out.empty() && (out.back() == '.' || out.back() == ' ')) out.pop_back();
  if (out.size() > kMaxNameBytes) {
    size_t cut = kMaxNameBytes;
    while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) cut--;  // keep UTF-8 whole
    out = detail::trim(out.substr(0, cut));
  }
  return out;
}

inline std::string pathFor(const std::string& name) { return std::string(kListsDir) + "/" + name + ".txt"; }

inline std::string nameFromPath(const std::string& path) {
  const size_t slash = path.find_last_of('/');
  std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
  if (name.size() >= 4) name.erase(name.size() - 4);
  return name;
}

// ---------------------------------------------------------------------------------------------
// Page layout. The device passes a function that wraps one entry into screen lines; the layout
// then fills pages top to bottom. An entry is never split across two pages.

struct PlacedEntry {
  int item = 0;                    // index into the item list
  int top = 0;                     // y of the entry's first line, relative to the list area
  int height = 0;                  // height of the whole entry (all its lines)
  std::vector<std::string> lines;  // wrapped text
};

struct Page {
  std::vector<PlacedEntry> entries;
};

// wrap(text, maxLines) -> lines of at most the text width; at least one line.
using WrapFn = std::function<std::vector<std::string>(const std::string& text, int maxLines)>;

// areaHeight: height available for entries; lineHeight: one text line; gap: space between entries.
inline std::vector<Page> layout(const std::vector<Item>& items, const bool hideDone, const WrapFn& wrap,
                                const int areaHeight, const int lineHeight, const int gap) {
  std::vector<Page> pages;
  if (lineHeight <= 0 || areaHeight < lineHeight) return pages;
  const int maxLines = std::max(1, std::min(8, areaHeight / lineHeight));
  Page page;
  int y = 0;
  for (int i = 0; i < static_cast<int>(items.size()); i++) {
    if (hideDone && items[i].done) continue;
    PlacedEntry entry;
    entry.item = i;
    entry.lines = wrap(items[i].text, maxLines);
    if (entry.lines.empty()) entry.lines.emplace_back();
    entry.height = static_cast<int>(entry.lines.size()) * lineHeight;
    const int needed = (page.entries.empty() ? 0 : gap) + entry.height;
    if (!page.entries.empty() && y + needed > areaHeight) {
      pages.push_back(std::move(page));
      page = Page{};
      y = 0;
    }
    entry.top = page.entries.empty() ? 0 : y + gap;
    y = entry.top + entry.height;
    page.entries.push_back(std::move(entry));
  }
  if (!page.entries.empty()) pages.push_back(std::move(page));
  return pages;
}

// Which entry on a page is at height y (relative to the list area)? -1 = none.
// The space between two entries belongs to the nearer one, so every tap hits something.
inline int entryAt(const Page& page, const int y, const int gap) {
  for (size_t i = 0; i < page.entries.size(); i++) {
    const auto& e = page.entries[i];
    const int top = e.top - (i == 0 ? 0 : gap / 2);
    const int bottom = e.top + e.height + (i + 1 == page.entries.size() ? gap : (gap + 1) / 2);
    if (y >= top && y < bottom) return static_cast<int>(i);
  }
  return -1;
}

// List name as a headline: upper case for ASCII and the Latin-1 letters (ä -> Ä), ß -> SS.
inline std::string toHeadline(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 4);
  for (size_t i = 0; i < s.size(); i++) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c >= 'a' && c <= 'z') {
      out.push_back(static_cast<char>(c - 32));
    } else if (c == 0xC3 && i + 1 < s.size()) {
      const unsigned char d = static_cast<unsigned char>(s[i + 1]);
      if (d == 0x9F) {
        out += "SS";  // ß
      } else if (d >= 0xA0 && d <= 0xBE && d != 0xB7) {
        out.push_back(static_cast<char>(c));
        out.push_back(static_cast<char>(d - 0x20));
      } else {
        out.push_back(static_cast<char>(c));
        out.push_back(static_cast<char>(d));
      }
      i++;
    } else if ((c == 0xD0 || c == 0xD1) && i + 1 < s.size()) {  // Cyrillic lower case -> upper case
      const unsigned char d = static_cast<unsigned char>(s[i + 1]);
      if (c == 0xD0 && d >= 0xB0 && d <= 0xBF) {  // а..п -> А..П
        out.push_back(static_cast<char>(0xD0));
        out.push_back(static_cast<char>(d - 0x20));
      } else if (c == 0xD1 && d >= 0x80 && d <= 0x8F) {  // р..я -> Р..Я
        out.push_back(static_cast<char>(0xD0));
        out.push_back(static_cast<char>(d + 0x20));
      } else if (c == 0xD1 && d == 0x91) {  // ё -> Ё
        out.push_back(static_cast<char>(0xD0));
        out.push_back(static_cast<char>(0x81));
      } else {
        out.push_back(static_cast<char>(c));
        out.push_back(static_cast<char>(d));
      }
      i++;
    } else {
      out.push_back(static_cast<char>(c));
    }
  }
  return out;
}

inline int countOpen(const std::vector<Item>& items) {
  int n = 0;
  for (const auto& item : items) n += item.done ? 0 : 1;
  return n;
}

}  // namespace todolist
