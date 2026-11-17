// Tests of runtime/utf8_text.c (patches/0021): the game's name entry counts and edits characters, not bytes, so
// Japanese (3 bytes a character in UTF-8) is kept whole: a name of up to the game's length in characters, never cut
// inside a character, typed or pasted text appended up to that length, Delete taking one character.
#include <stdio.h>
#include <string.h>

#include "utf8_text.h"

static int g_ok = 1, g_n;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        g_n++;                                                                         \
        if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_ok = 0; } \
    } while (0)

int main(void) {
    // counting: characters, whatever their size
    CHECK(utf8_chars("") == 0);
    CHECK(utf8_chars("Rei") == 3);
    CHECK(utf8_chars("ショウ") == 3);
    CHECK(utf8_chars("Aあ漢😀") == 4);
    CHECK(utf8_prefix("ショウ", 2) == 6);
    CHECK(utf8_prefix("ショウ", 9) == 9);

    // cutting to a length: at a character's end
    char s[64];
    strcpy(s, "テルたろう");
    utf8_cut(s, 3);
    CHECK(!strcmp(s, "テルた"));
    strcpy(s, "Rei");
    utf8_cut(s, 6);
    CHECK(!strcmp(s, "Rei"));

    // appending: up to the length in characters and the buffer's size, control characters dropped
    strcpy(s, "あ");
    CHECK(utf8_append(s, sizeof s, 6, "いうえおかき") == 5);
    CHECK(!strcmp(s, "あいうえおか"));
    strcpy(s, "");
    CHECK(utf8_append(s, sizeof s, 12, "Ak\ni\tra") == 5);
    CHECK(!strcmp(s, "Akira"));
    char small[8] = "";  // (room for 7 bytes: two Japanese characters, not a third cut in half)
    CHECK(utf8_append(small, sizeof small, 12, "ショウ") == 2);
    CHECK(!strcmp(small, "ショ"));
    strcpy(s, "");
    CHECK(utf8_append(s, sizeof s, 12, "a\xE3\x81") == 1);  // (a sequence cut short: left out)
    CHECK(!strcmp(s, "a"));
    strcpy(s, "");
    CHECK(utf8_append(s, sizeof s, 12, "\xFF" "b") == 1);  // (not UTF-8: left out)
    CHECK(!strcmp(s, "b"));

    // one code point (the on-screen keyboard's typed characters)
    strcpy(s, "x");
    CHECK(utf8_append_cp(s, sizeof s, 6, 0x30A2) == 1);
    CHECK(!strcmp(s, "xア"));
    CHECK(utf8_append_cp(s, sizeof s, 6, 0x07) == 0);
    CHECK(!strcmp(s, "xア"));

    // deleting: the last character
    strcpy(s, "Reiテ");
    utf8_pop(s);
    CHECK(!strcmp(s, "Rei"));
    utf8_pop(s);
    CHECK(!strcmp(s, "Re"));
    s[0] = 0;
    utf8_pop(s);
    CHECK(!strcmp(s, ""));

    // the paste's pieces: the longest start of the text that fits, ending on a character
    CHECK(utf8_fit("あいう", 7) == 6);
    CHECK(utf8_fit("abc", 7) == 3);
    CHECK(utf8_fit("あ", 2) == 0);

    printf("utf8: %d checks, %s\n", g_n, g_ok ? "all passed" : "FAILED");
    return g_ok ? 0 : 1;
}
