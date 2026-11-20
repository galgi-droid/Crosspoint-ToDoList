// PC test for ListCore.h:  g++ -std=c++17 -Wall -Wextra -I../patch/files test_listcore.cpp && ./a.out
#include "ListCore.h"
#include "ListCover.h"
#include "ListPaper.h"

#include <cstdio>
#include <cstdlib>

using namespace todolist;

static int failures = 0;
#define CHECK(cond)                                                    \
  do {                                                                 \
    if (!(cond)) {                                                     \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
      failures++;                                                      \
    }                                                                  \
  } while (0)

static void testParse() {
  auto items = parse("\xEF\xBB\xBF[x] Milch\r\n[ ] Brot\n\n   \nKaffee\r- Käse\n• Eier\n1. Butter\n2) Salz\n- [ ] Tee\n* [X] Zucker\n[ ]\n-\n");
  CHECK(items.size() == 9);
  CHECK(items[0].done && items[0].text == "Milch");
  CHECK(!items[1].done && items[1].text == "Brot");
  CHECK(!items[2].done && items[2].text == "Kaffee");
  CHECK(items[3].text == "Käse");
  CHECK(items[4].text == "Eier");
  CHECK(items[5].text == "Butter");
  CHECK(items[6].text == "Salz");
  CHECK(!items[7].done && items[7].text == "Tee");
  CHECK(items[8].done && items[8].text == "Zucker");

  // Not bullets: a minus sign with a number, a year, a bracket that is no marker.
  items = parse("-5 Grad\n2026 planen\n[a] Notiz\n");
  CHECK(items.size() == 3);
  CHECK(items[0].text == "-5 Grad");
  CHECK(items[1].text == "2026 planen");
  CHECK(items[2].text == "[a] Notiz");
}

static void testSerialize() {
  auto items = parse("Milch\n[x] Brot\n");
  CHECK(serialize(items) == "[ ] Milch\n[x] Brot\n");
  CHECK(serialize(parse(serialize(items))) == serialize(items));  // stable round trip
}

static void testMerge() {
  {
    auto ru = parse("Молоко\nЁжик\n");
    const auto rr = merge(ru, "молоко\nЁЖИК\nХлеб\n");
    CHECK(rr.added == 1 && rr.skipped == 2);
  }
  auto items = parse("[x] Milch\n[ ] Brot\n");
  const auto r = merge(items, "milch\nBROT\nKäse\nkäse\n");
  CHECK(r.reopened == 1);
  CHECK(r.skipped == 2);  // Brot, and the second Käse
  CHECK(r.added == 1);
  CHECK(serialize(items) == "[ ] Milch\n[ ] Brot\n[ ] Käse\n");
  CHECK(merge(items, commasToLines("Eier; Butter;Milch")).added == 2);
  CHECK(items.size() == 5 && items[3].text == "Eier" && items[4].text == "Butter");

  // Commas and semicolons split everywhere, also in files; a decimal comma stays.
  items = parse("Milch, brot; wurst\n[x] Tee, Kaffee\n1,5 kg Mehl\nAepfel - die roten\n");
  CHECK(serialize(items) == "[ ] Milch\n[ ] brot\n[ ] wurst\n[x] Tee\n[x] Kaffee\n[ ] 1,5 kg Mehl\n[ ] Aepfel - die roten\n");
  items = parse("Milch\nbtrot\nwurst\n");
  CHECK(serialize(items) == "[ ] Milch\n[ ] btrot\n[ ] wurst\n");

  // Editing on the reader: the first part replaces the entry, the rest go right below it.
  items = parse("[ ] A\n[x] B\n[ ] C\n");
  CHECK(applyEdit(items, 0, "A1, X; Y, C"));
  CHECK(serialize(items) == "[ ] A1\n[ ] X\n[ ] Y\n[x] B\n[ ] C\n");
  CHECK(applyEdit(items, 4, "C2"));
  CHECK(items[4].text == "C2" && items.size() == 5);
  CHECK(!applyEdit(items, 0, " , ; "));  // nothing usable: the caller deletes the entry
  CHECK(!applyEdit(items, 9, "Z"));

  // Umlauts fold for duplicates (ÄPFEL == äpfel), and incoming [x] is still added as open.
  items = parse("[ ] äpfel\n");
  const auto r2 = merge(items, "ÄPFEL\n[x] Birnen\n");
  CHECK(r2.skipped == 1 && r2.added == 1);
  CHECK(items[1].text == "Birnen" && !items[1].done);

  // Typed on the reader: commas separate items.
  items = parse("");
  CHECK(merge(items, commasToLines("Milch, Brot ,Käse,, ")).added == 3);
  CHECK(serialize(items) == "[ ] Milch\n[ ] Brot\n[ ] Käse\n");
  CHECK(merge(items, commasToLines("Eier; Butter;Milch")).added == 2);
  CHECK(items.size() == 5 && items[3].text == "Eier" && items[4].text == "Butter");

  // Editing on the reader: the first part replaces the entry, the rest go right below it.
  items = parse("[ ] A\n[x] B\n[ ] C\n");
  CHECK(applyEdit(items, 0, "A1, X; Y, C"));
  CHECK(serialize(items) == "[ ] A1\n[ ] X\n[ ] Y\n[x] B\n[ ] C\n");
  CHECK(applyEdit(items, 4, "C2"));
  CHECK(items[4].text == "C2" && items.size() == 5);
  CHECK(!applyEdit(items, 0, " , ; "));  // nothing usable: the caller deletes the entry
  CHECK(!applyEdit(items, 9, "Z"));

  // Extra spaces inside an entry do not make a new one.
  items = parse("[ ] Batterien AA\n");
  CHECK(merge(items, "Batterien   AA").skipped == 1);
}

static void testNames() {
  CHECK(sanitizeName("Einkauf") == "Einkauf");
  CHECK(sanitizeName("  Ein/kauf: Wien?  ") == "Einkauf Wien");
  CHECK(sanitizeName("Einkauf.TXT") == "Einkauf");
  CHECK(sanitizeName("..versteckt") == "versteckt");
  CHECK(sanitizeName("///").empty());
  CHECK(sanitizeName(std::string(100, 'a')).size() == kMaxNameBytes);
  // A cut never splits a UTF-8 letter.
  const std::string longUml = std::string(59, 'a') + "ä";
  CHECK(sanitizeName(longUml) == std::string(59, 'a'));
  CHECK(pathFor("Einkauf") == "/Lists/Einkauf.txt");
  CHECK(isListPath("/Lists/Einkauf.txt"));
  CHECK(isListPath("/lists/Einkauf.TXT"));
  CHECK(!isListPath("/Lists/sub/Einkauf.txt"));
  CHECK(!isListPath("/Books/Einkauf.txt"));
  CHECK(!isListPath("/Lists/Einkauf.epub"));
  CHECK(!isListPath("/Lists/.txt"));
  CHECK(!isListPath("/Lists/._Einkauf.txt"));  // macOS metadata file
  CHECK(!isListPath("/Lists/.hidden.txt"));
  CHECK(nameFromPath("/Lists/Urlaub packen.txt") == "Urlaub packen");
  CHECK(toHeadline("Einkauf") == "EINKAUF");
  CHECK(toHeadline("Покупки на ёлку, яблоки") == "ПОКУПКИ НА ЁЛКУ, ЯБЛОКИ");
  CHECK(toHeadline("Дела Todo") == "ДЕЛА TODO");
  CHECK(toHeadline("Größe & Maße 2") == "GRÖSSE & MASSE 2");
  CHECK(toHeadline("ÄÖÜ äöü é") == "ÄÖÜ ÄÖÜ É");
}

static void testLayout() {
  // Fake wrap: 10 characters per line.
  const WrapFn wrap = [](const std::string& text, int maxLines) {
    std::vector<std::string> lines;
    for (size_t i = 0; i < text.size() && static_cast<int>(lines.size()) < maxLines; i += 10) {
      lines.push_back(text.substr(i, 10));
    }
    return lines;
  };
  std::vector<Item> items = parse("[ ] a\n[x] b\n[ ] 0123456789abc\n[ ] d\n[ ] e\n");
  // area 100 px, line 20 px, gap 10 px: a(0-20) b(30-50) c(60-100) | d e
  auto pages = layout(items, false, wrap, 100, 20, 10);
  CHECK(pages.size() == 2);
  CHECK(pages[0].entries.size() == 3);
  CHECK(pages[0].entries[2].top == 60 && pages[0].entries[2].height == 40);
  CHECK(pages[1].entries.size() == 2 && pages[1].entries[0].top == 0);

  // Hidden done entries take no space.
  pages = layout(items, true, wrap, 100, 20, 10);
  CHECK(pages[0].entries.size() == 3);
  CHECK(pages[0].entries[1].item == 2);

  // Taps between entries belong to the nearer one; below the last entry counts too.
  pages = layout(items, false, wrap, 100, 20, 10);
  CHECK(entryAt(pages[0], 0, 10) == 0);
  CHECK(entryAt(pages[0], 24, 10) == 0);
  CHECK(entryAt(pages[0], 26, 10) == 1);
  CHECK(entryAt(pages[0], 99, 10) == 2);
  CHECK(entryAt(pages[0], 200, 10) == -1);

  // Empty list, and a list where everything is done and hidden.
  CHECK(layout({}, false, wrap, 100, 20, 10).empty());
  CHECK(layout(parse("[x] a\n"), true, wrap, 100, 20, 10).empty());
  CHECK(countOpen(items) == 4);
}

static void testPaperAndCover() {
  // Portrait X4 / X4 Pro screen: the page must leave room for several lines and keep the
  // tick margin (everything left of marginX) wide enough for a finger.
  const auto g = listpaper::geometry(480, 800, 58, 0, 0, 0, 0);
  CHECK(g.lineCount >= 8);
  CHECK(g.marginX >= 100 && g.marginX < 200);  // tick margin: about 1.2 cm or more
  CHECK(g.boxX > g.paperLeft + 30);
  CHECK(g.textX > g.marginX);
  CHECK(listpaper::ruleY(g, g.lineCount - 1) < 800);
  int black = 0;
  listpaper::paintPaper(g, 7u, [&](int x, int y, bool b) {
    if (x < 0 || y < 0 || x >= 480 || y >= 800) failures += 1000;  // never draws off screen
    black += b ? 1 : 0;
  });
  CHECK(black > 1000);
  // Five covers, each a 480x800 PNG (3:5, the shape of every cover thumbnail).
  for (int i = 0; i < kCoverCount; i++) {
    const CoverImage& c = coverImage(i);
    CHECK(c.size > 1000u && c.size < 40000u);
    CHECK(c.data[1] == 'P' && c.data[2] == 'N' && c.data[3] == 'G');
    const uint32_t w = (c.data[16] << 24) | (c.data[17] << 16) | (c.data[18] << 8) | c.data[19];
    const uint32_t h = (c.data[20] << 24) | (c.data[21] << 16) | (c.data[22] << 8) | c.data[23];
    CHECK(w == 480u && h == 800u);
  }
  CHECK(coverPath(0) == "/.crosspoint/todolist/cover_1.png");
  // A list keeps its cover whatever the letter case; different lists use different covers.
  CHECK(coverIndexFor("/Lists/Einkauf.txt") == coverIndexFor("/lists/EINKAUF.TXT"));
  bool seen[kCoverCount] = {};
  const char* names[] = {"To do", "Einkauf", "Baumarkt", "Urlaub", "Geschenke", "Arbeit", "Garten", "Ideen"};
  for (const char* n : names) {
    const int i = coverIndexFor(pathFor(n));
    CHECK(i >= 0 && i < kCoverCount);
    seen[i] = true;
  }
  int used = 0;
  for (const bool s : seen) used += s ? 1 : 0;
  CHECK(used >= 3);
}

int main() {
  testPaperAndCover();
  testParse();
  testSerialize();
  testMerge();
  testNames();
  testLayout();
  if (failures == 0) std::printf("All ListCore tests passed.\n");
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
