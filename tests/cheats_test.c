// Tests of runtime/cheats.c (patches/0006): the Atmosphère cheat file parser, the cheat VM against fake memory, the
// player's cheat files (import, on / off). Built by tests/run_unit.sh. The codes here are made up for the tests.
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cheats.h"
#include "miniz/miniz.h"

static int g_run, g_fail;
#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ok = 0; } \
    } while (0)

// ---- fake memory: "main" (its first 0x4000 bytes are code: not writable) and "heap"
static uint8_t g_main[0x10000] __attribute__((aligned(16))), g_heap[0x10000] __attribute__((aligned(16)));
#define MAIN ((uint64_t)(uintptr_t)g_main)
#define HEAP ((uint64_t)(uintptr_t)g_heap)
static bool in(uint64_t a, uint64_t n, uint64_t lo, uint64_t size) { return a >= lo && n <= size && a - lo <= size - n; }
static bool fake_readable(uint64_t a, uint64_t n) { return in(a, n, MAIN, sizeof g_main) || in(a, n, HEAP, sizeof g_heap); }
static bool fake_writable(uint64_t a, uint64_t n) {
    return in(a, n, MAIN + 0x4000, sizeof g_main - 0x4000) || in(a, n, HEAP, sizeof g_heap);
}
static CheatEnv env(uint64_t keys) {
    CheatEnv e = {{MAIN, HEAP, 0, 0}, {sizeof g_main, sizeof g_heap, 0, 0}, keys, fake_readable, fake_writable};
    return e;
}
static int run_keys(uint64_t keys, const uint32_t *op, int n) {
    CheatEnv e = env(keys);
    static CheatState st;
    return cheat_vm_run(op, n, &e, &st);
}
#define RUN(...) ({ static const uint32_t ops_[] = {__VA_ARGS__}; run_keys(0, ops_, (int)(sizeof ops_ / 4)); })
#define RUNK(k, ...) ({ static const uint32_t ops_[] = {__VA_ARGS__}; run_keys((k), ops_, (int)(sizeof ops_ / 4)); })
static uint32_t h32(uint64_t off) { uint32_t v; memcpy(&v, g_heap + off, 4); return v; }
static uint64_t h64(uint64_t off) { uint64_t v; memcpy(&v, g_heap + off, 8); return v; }
static void clear(void) { memset(g_main, 0, sizeof g_main), memset(g_heap, 0, sizeof g_heap); }

// ---- the file
static const char *FILE_TEXT =
    "{Master Code}\r\n"
    "04000000 00005000 00000001\r\n"
    "\r\n"
    "[Max Money] - someone\n"
    "04100000 00000100 0098967F\n"
    "[ Inf Health ]\n"
    "04000000 00001000 52800020\n"
    "[Empty]\n"
    "[--SectionStart:Items--]\n"
    "[Lots of items]\n"
    "040e0000 00000200 000003e7\n";

static int parse_file(void) {
    int ok = 1;
    CheatList l;
    CHECK(cheats_parse(FILE_TEXT, &l));
    CHECK(l.n == 6);
    if (l.n == 6) {
        CHECK(l.c[0].master && !strcmp(l.c[0].name, "Master Code") && l.c[0].nop == 3);
        CHECK(!l.c[1].master && !strcmp(l.c[1].name, "Max Money") && l.c[1].nop == 3 && l.c[1].op[2] == 0x0098967F);
        CHECK(!strcmp(l.c[2].name, "Inf Health"));
        CHECK(l.c[3].nop == 0 && l.c[4].nop == 0);  // (an empty cheat, a section label: listed, nothing to run)
        CHECK(l.c[5].op[0] == 0x040E0000);           // (lower-case hex)
    }
    cheats_free(&l);
    CHECK(!cheats_parse("just some text\n", &l));
    return ok;
}

// ---- the VM, opcode by opcode
static int op0_store_static(void) {  // 0TMR00AA AAAAAAAA VVVVVVVV (VVVVVVVV)
    int ok = 1;
    clear();
    CHECK(RUN(0x01100000, 0x00000010, 0x000000AB) == CHEAT_OK);                   // 1 byte, heap + 0x10
    CHECK(g_heap[0x10] == 0xAB && g_heap[0x11] == 0);
    CHECK(RUN(0x02100000, 0x00000020, 0x0000BEEF) == CHEAT_OK && h32(0x20) == 0xBEEF);
    CHECK(RUN(0x04100000, 0x00000030, 0x12345678) == CHEAT_OK && h32(0x30) == 0x12345678);
    CHECK(RUN(0x08100000, 0x00000040, 0x11223344, 0x55667788) == CHEAT_OK && h64(0x40) == 0x1122334455667788ull);
    // offset register: r3 = 0x100 (type 4), then heap + 0x50 + r3
    CHECK(RUN(0x40030000, 0, 0x100, 0x04130000, 0x00000050, 7) == CHEAT_OK && h32(0x150) == 7);
    return ok;
}

static int op0_code_blocked(void) {  // a write into main's code: refused, nothing written, reported
    int ok = 1;
    clear();
    CHECK(RUN(0x04000000, 0x00001000, 0x52800020) & CHEAT_WRITE_BLOCKED);
    uint32_t v;
    memcpy(&v, g_main + 0x1000, 4);
    CHECK(v == 0);
    CHECK(RUN(0x04000000, 0x00005000, 9) == CHEAT_OK);  // (main's data part: written)
    memcpy(&v, g_main + 0x5000, 4);
    CHECK(v == 9);
    return ok;
}

static int op1_conditional(void) {  // 1TMC00AA AAAAAAAA VVVVVVVV, 21000000 else, 20000000 end
    int ok = 1;
    clear();
    g_heap[0] = 5;
    // if [heap] == 5 { [heap+4] = 1 } else { [heap+4] = 2 }
    CHECK(RUN(0x11150000, 0, 5, 0x04100000, 4, 1, 0x21000000, 0x04100000, 4, 2, 0x20000000) == CHEAT_OK);
    CHECK(h32(4) == 1);
    g_heap[0] = 6;
    CHECK(RUN(0x11150000, 0, 5, 0x04100000, 4, 1, 0x21000000, 0x04100000, 4, 2, 0x20000000) == CHEAT_OK);
    CHECK(h32(4) == 2);
    // nested: if [heap] > 3 { if [heap] < 4 { [8] = 1 } [12] = 1 }  ([heap] = 6: inner false, outer true)
    CHECK(RUN(0x11110000, 0, 3, 0x11130000, 0, 4, 0x04100000, 8, 1, 0x20000000, 0x04100000, 12, 1, 0x20000000) == CHEAT_OK);
    CHECK(h32(8) == 0 && h32(12) == 1);
    // conditions: > >= < <= == != (1..6), on 4-byte values
    g_heap[0x100] = 10;
    static const uint32_t want[7][2] = {{0}, {9, 10}, {10, 11}, {11, 10}, {10, 9}, {10, 11}, {11, 10}};  // (true, false)
    for (uint32_t c = 1; c <= 6; c++)
        for (int t = 0; t < 2; t++) {
            memset(g_heap + 0x200, 0, 4);
            uint32_t ops[] = {0x14100000 | c << 16, 0x100, want[c][t], 0x04100000, 0x200, 1, 0x20000000};
            CheatEnv e = env(0);
            CheatState st = {{0}};
            CHECK(cheat_vm_run(ops, 7, &e, &st) == CHEAT_OK);
            CHECK(h32(0x200) == (t == 0 ? 1u : 0u));
        }
    return ok;
}

static int op3_loop_op6_store(void) {  // 300R0000 VVVVVVVV loop / 310R0000 end; 6T0RIor0 VVVVVVVV VVVVVVVV
    int ok = 1;
    clear();
    // r1 = heap + 0x80 (5T M=1 R=1 from [heap+0] is a pointer -> here: type 4 load of the address)
    uint32_t lo = (uint32_t)(HEAP + 0x80), hi = (uint32_t)((HEAP + 0x80) >> 32);
    static uint32_t ops[] = {0x40010000, 0, 0, 0x30020000, 4, 0x64011000, 0, 0x2A, 0x31020000};
    ops[1] = hi, ops[2] = lo;
    CheatEnv e = env(0);
    CheatState st = {{0}};
    CHECK(cheat_vm_run(ops, 9, &e, &st) == CHEAT_OK);  // 4 times: [r1] = 0x2A, r1 += 4
    CHECK(h32(0x80) == 0x2A && h32(0x84) == 0x2A && h32(0x88) == 0x2A && h32(0x8C) == 0x2A && h32(0x90) == 0);
    return ok;
}

static int op5_pointer_chain(void) {  // 5TMRI0AA AAAAAAAA: r = [base + A], then r = [r + A]; 7T0RC000 add; A store
    int ok = 1;
    clear();
    uint64_t p1 = HEAP + 0x400, p2 = HEAP + 0x800;
    memcpy(g_main + 0x6000, &p1, 8);  // main+0x6000 -> heap+0x400 -> (at +0x18) heap+0x800
    memcpy(g_heap + 0x400 + 0x18, &p2, 8);
    // r0 = [main+0x6000]; r0 = [r0+0x18]; r0 += 0x10; [r0] = 99 (6: static to [r0])
    CHECK(RUN(0x58000000, 0x00006000, 0x58001000, 0x00000018, 0x78000000, 0x10, 0x64000000, 0, 99) == CHEAT_OK);
    CHECK(h32(0x810) == 99);
    // an unreadable pointer: the load gives 0, the write after it is refused (no crash)
    memset(g_main + 0x6000, 0xEE, 8);
    CHECK(RUN(0x58000000, 0x00006000, 0x64000000, 0, 1) & CHEAT_WRITE_BLOCKED);
    return ok;
}

static int op8_keys(void) {  // 8kkkkkkk: the block runs only while all those buttons are held (HID bits: A = 0, ZL = 8...)
    int ok = 1;
    clear();
    const uint64_t A = 1, ZL = 1 << 8;
    CHECK(RUNK(A | ZL, 0x80000101, 0x04100000, 0, 1, 0x20000000) == CHEAT_OK && h32(0) == 1);
    clear();
    CHECK(RUNK(A, 0x80000101, 0x04100000, 0, 1, 0x20000000) == CHEAT_OK && h32(0) == 0);  // (ZL not held)
    // C4: the 64-bit form - C4000000 kkkkkkkk kkkkkkkk
    clear();
    CHECK(RUNK(ZL, 0xC4000000, 0, 0x100, 0x04100000, 0, 3, 0x20000000) == CHEAT_OK && h32(0) == 3);
    return ok;
}

static int op9_arith(void) {  // 9TCRS0s0 (registers) / 9TCRS100 VVVVVVVV (a value); width masks the result
    int ok = 1;
    clear();
    // r1 = 0xFF; r2 = r1 + 1 at width 1 (wraps to 0); r3 = r1 * 3 (4 bytes); r4 = r3 ^ r1; store r2, r3, r4
    CHECK(RUN(0x40010000, 0, 0xFF, 0x91021100, 1, 0x94231100, 3, 0x94843010, 0xA4200410, 0x0, 0xA4300410, 0x4,
              0xA4400410, 0x8) == CHEAT_OK);
    CHECK(h32(0) == 0 && h32(4) == 0x2FD && h32(8) == (0x2FDu ^ 0xFFu));
    // lsl / lsr / and / or / not / move
    CHECK(RUN(0x40010000, 0, 0xF0, 0x94331100, 4, 0x94441100, 4, 0x94551100, 0x30, 0x94661100, 0x0F, 0x94771000,
              0x94981000, 0xA4300410, 0x10, 0xA4400410, 0x14, 0xA4500410, 0x18, 0xA4600410, 0x1C, 0xA4700410,
              0x20, 0xA4800410, 0x24) == CHEAT_OK);
    CHECK(h32(0x10) == 0xF00 && h32(0x14) == 0xF && h32(0x18) == 0x30 && h32(0x1C) == 0xFF && h32(0x20) == ~0xF0u &&
          h32(0x24) == 0xF0);
    return ok;
}

static int opA_store_reg(void) {  // ATSRIOxa (aaaaaaaa): where to store register S, by the offset type O
    int ok = 1;
    clear();
    uint64_t a = HEAP + 0x300;
    static uint32_t ops[] = {
        0x40010000, 0, 0,             // r1 = heap + 0x300
        0x40020000, 0, 0x11,          // r2 = 0x11 (the value)
        0x40030000, 0, 0x8,           // r3 = 8 (an offset)
        0x40040000, 0, 0x40,          // r4 = 0x40 (an offset from heap's base)
        0xA4210000,                   // O=0: [r1]
        0xA4210130,                   // O=1: [r1 + r3]
        0xA4210200, 0x10,             // O=2: [r1 + 0x10]
        0xA4240310,                   // O=3: [heap + r4]
        0xA4200410, 0x50,             // O=4: [heap + 0x50]
        0xA4240510, 0x8,              // O=5: [heap + r4 + 8]
        0xA4211000,                   // I=1: [r1], then r1 += 4
        0xA4210000,                   // [r1] (now +4)
    };
    ops[1] = (uint32_t)(a >> 32), ops[2] = (uint32_t)a;
    CheatEnv e = env(0);
    CheatState st = {{0}};
    CHECK(cheat_vm_run(ops, (int)(sizeof ops / 4), &e, &st) == CHEAT_OK);
    CHECK(h32(0x300) == 0x11 && h32(0x308) == 0x11 && h32(0x310) == 0x11 && h32(0x40) == 0x11 && h32(0x50) == 0x11 &&
          h32(0x48) == 0x11 && h32(0x304) == 0x11);
    return ok;
}

static int opC_registers(void) {  // C0 register conditionals, C1 / C2 save + restore, C3 static registers
    int ok = 1;
    clear();
    // r1 = 7; if r1 == 7 (C0 static: C0TcS400 V) { [0] = 1 }; r2 = 7; if r1 != r2 (C0 reg: C0TcS5X0) { [4] = 1 }
    CHECK(RUN(0x40010000, 0, 7, 0xC0451400, 7, 0x04100000, 0, 1, 0x20000000, 0x40020000, 0, 7, 0xC0461520, 0x04100000,
              4, 1, 0x20000000) == CHEAT_OK);
    CHECK(h32(0) == 1 && h32(4) == 0);
    // C1: save r1 into saved[3]; clear r1; restore r1 from saved[3]; store
    CHECK(RUN(0x40010000, 0, 0x55, 0xC1030110, 0xC1010030, 0xC1010300, 0xA4100410, 0x8) == CHEAT_OK);
    CHECK(h32(8) == 0x55);
    // C3: write static register 0x80 from r5 (0x80-0xFF: written by cheats); read static 0x10 (0x00-0x7F: set by the
    // host) into r6 and store it - in another run with the same state, the statics kept
    CheatEnv e = env(0);
    CheatState st = {{0}};
    uint32_t w[] = {0x40050000, 0, 0x77, 0xC3000805};
    uint32_t r[] = {0xC3000106, 0xA4600410, 0x10};
    CHECK(cheat_vm_run(w, 4, &e, &st) == CHEAT_OK && st.statics[0x80] == 0x77);
    st.statics[0x10] = 0x66;
    CHECK(cheat_vm_run(r, 3, &e, &st) == CHEAT_OK && h32(0x10) == 0x66 && st.statics[0x80] == 0x77);
    return ok;
}

static int bad_and_bounded(void) {  // unknown opcodes, cut-off codes and endless loops end the run, never the game
    int ok = 1;
    clear();
    CHECK(RUN(0xE0000000) & CHEAT_BAD_OPCODE);
    CHECK(RUN(0x04100000, 0x10) & CHEAT_BAD_OPCODE);                        // (its value missing)
    CHECK(RUN(0x03100000, 0x00000010, 1) & CHEAT_BAD_OPCODE);               // (width 3)
    CHECK(RUN(0x30010000, 0, 0x04100000, 0, 1, 0x31010000) & CHEAT_TOO_LONG);  // (count 0: wraps, ~4 billion turns)
    CHECK(RUN(0x20000000) == CHEAT_OK);                                     // (a stray end: ignored)
    return ok;
}

static int code_patch_detect(void) {  // a fixed write into main's code: the launcher labels the cheat
    int ok = 1;
    CheatList l;
    CHECK(cheats_parse(FILE_TEXT, &l) && l.n == 6);
    CHECK(!cheat_patches_code(&l.c[1], 0, 0x4000));  // (heap)
    CHECK(cheat_patches_code(&l.c[2], 0, 0x4000));   // (main + 0x1000)
    CHECK(!cheat_patches_code(&l.c[0], 0, 0x4000));  // (main + 0x5000: data)
    cheats_free(&l);
    return ok;
}

// ---- the player's cheat files, in a data root
static char g_tmp[PATH_MAX];
static void fresh_tmp(void) {
    snprintf(g_tmp, sizeof g_tmp, "%s/cheats test é XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
    if (!mkdtemp(g_tmp) || chdir(g_tmp) != 0) { perror("tmp"); exit(2); }
}
static void write_file(const char *p, const void *d, size_t n) {
    FILE *f = fopen(p, "wb");
    if (!f) { perror(p); exit(2); }
    fwrite(d, 1, n, f);
    fclose(f);
}
static void exefs_with_build(const uint8_t id[8]) {  // extracted/exefs/main with that build id (NSO header +0x40)
    mkdir("extracted", 0755), mkdir("extracted/exefs", 0755);
    uint8_t nso[0x60] = {'N', 'S', 'O', '0'};
    memcpy(nso + 0x40, id, 8);
    write_file("extracted/exefs/main", nso, sizeof nso);
}
static const uint8_t PLA_111[8] = {0xae, 0xe8, 0xf1, 0x50, 0xdd, 0xa1, 0xb5, 0xa8};

static int build_id_name(void) {
    int ok = 1;
    fresh_tmp();
    exefs_with_build(PLA_111);
    char bid[17];
    CHECK(cheats_build_id("extracted/exefs", bid) && !strcmp(bid, "AEE8F150DDA1B5A8"));
    return ok;
}

static int import_txt(void) {
    int ok = 1;
    fresh_tmp();
    exefs_with_build(PLA_111);
    write_file("AEE8F150DDA1B5A8.txt", FILE_TEXT, strlen(FILE_TEXT));
    char why[256] = "";
    CHECK(cheats_import("AEE8F150DDA1B5A8.txt", "extracted/exefs", why, sizeof why));
    CheatList l;
    CHECK(cheats_load("extracted/exefs", &l) == 6);
    cheats_free(&l);
    // another build's file: refused, with the reason
    write_file("7FCAD279539DE183.txt", FILE_TEXT, strlen(FILE_TEXT));
    CHECK(!cheats_import("7FCAD279539DE183.txt", "extracted/exefs", why, sizeof why));
    CHECK(strstr(why, "AEE8F150DDA1B5A8") != NULL);
    return ok;
}

static int import_zip(void) {  // the database's zip: every version's file in it, this build's taken
    int ok = 1;
    fresh_tmp();
    exefs_with_build(PLA_111);
    mz_zip_archive z;
    memset(&z, 0, sizeof z);
    CHECK(mz_zip_writer_init_file(&z, "Legends.zip", 0));
    const char *other = "[Old]\n04100000 00000000 00000001\n";
    mz_zip_writer_add_mem(&z, "Legends/7FCAD279539DE183.txt", other, strlen(other), MZ_DEFAULT_COMPRESSION);
    mz_zip_writer_add_mem(&z, "Legends/aee8f150dda1b5a8.txt", FILE_TEXT, strlen(FILE_TEXT), MZ_DEFAULT_COMPRESSION);
    mz_zip_writer_finalize_archive(&z);
    mz_zip_writer_end(&z);
    char why[256] = "";
    CHECK(cheats_import("Legends.zip", "extracted/exefs", why, sizeof why));
    CheatList l;
    CHECK(cheats_load("extracted/exefs", &l) == 6);
    cheats_free(&l);
    return ok;
}

static int toggles_saved(void) {  // on / off by name, kept across loads and across a re-import of a newer file
    int ok = 1;
    fresh_tmp();
    exefs_with_build(PLA_111);
    write_file("c.txt", FILE_TEXT, strlen(FILE_TEXT));
    char why[256];
    CHECK(cheats_import("c.txt", "extracted/exefs", why, sizeof why));
    CHECK(!cheats_is_on("extracted/exefs", "Max Money"));  // (all off at first)
    cheats_set_on("extracted/exefs", "Max Money", true);
    cheats_set_on("extracted/exefs", "Lots of items", true);
    cheats_set_on("extracted/exefs", "Lots of items", false);
    CHECK(cheats_is_on("extracted/exefs", "Max Money") && !cheats_is_on("extracted/exefs", "Lots of items"));
    CHECK(cheats_import("c.txt", "extracted/exefs", why, sizeof why));  // (re-imported)
    CHECK(cheats_is_on("extracted/exefs", "Max Money"));
    cheats_all_off("extracted/exefs");
    CHECK(!cheats_is_on("extracted/exefs", "Max Money"));
    return ok;
}

// ---- code cheats (patches/0010): fixed instruction writes become variants compiled into the game code
static const char *CODE_TEXT =
    "[Always Level 100]\n"
    "04000000 00001000 52800C80\n"           // (main + 0x1000: an instruction)
    "[Level Off]\n"
    "04000000 00001000 B9400000\n"           // (the same instruction, another word: the "Off" cheat)
    "[Two at once]\n"
    "08000000 00002000 D503201F 52800020\n"  // (8 bytes: main + 0x2000 = 0x52800020, main + 0x2004 = 0xD503201F)
    "[Data only]\n"
    "04000000 00005000 00000001\n";          // (main's data: not a variant)

static int variants_list(void) {
    int ok = 1;
    CheatList l;
    CHECK(cheats_parse(CODE_TEXT, &l));
    CheatVariant v[16];
    int n = cheats_code_variants(&l, 0, 0x4000, v, 16);
    CHECK(n == 4);
    bool a = false, b = false, c = false, d = false;
    for (int i = 0; i < n; i++) {
        a |= v[i].off == 0x1000 && v[i].word == 0x52800C80;
        b |= v[i].off == 0x1000 && v[i].word == 0xB9400000;
        c |= v[i].off == 0x2000 && v[i].word == 0x52800020;
        d |= v[i].off == 0x2004 && v[i].word == 0xD503201F;
    }
    CHECK(a && b && c && d);
    CHECK(cheats_code_variants(&l, 0, 0x4000, v, 2) == 2);  // (room for 2: the first 2)
    cheats_free(&l);
    return ok;
}

static int variants_file(void) {  // <data>/cheats/<build id>.variants.json, as tools/gen.py --code-variants reads it
    int ok = 1;
    fresh_tmp();
    exefs_with_build(PLA_111);
    uint8_t text_size[4] = {0x00, 0x40, 0x00, 0x00};  // (main's text: offset 0, 0x4000 bytes)
    FILE *f = fopen("extracted/exefs/main", "r+b");
    fseek(f, 0x18, SEEK_SET), fwrite(text_size, 1, 4, f), fclose(f);
    write_file("c.txt", CODE_TEXT, strlen(CODE_TEXT));
    char why[256] = "";
    CHECK(cheats_import("c.txt", "extracted/exefs", why, sizeof why));  // (import writes the variants too)
    char p[PATH_MAX];
    CHECK(cheats_variants_path("extracted/exefs", p, sizeof p));
    FILE *j = fopen(p, "rb");
    CHECK(j != NULL);
    char buf[2048] = "";
    if (j) { buf[fread(buf, 1, sizeof buf - 1, j)] = 0; fclose(j); }
    CHECK(strstr(buf, "\"code\"") && strstr(buf, "\"0x1000\"") && strstr(buf, "\"0x52800c80\"") && strstr(buf, "\"0xb9400000\"") &&
          strstr(buf, "\"0x2004\"") && !strstr(buf, "\"0x5000\""));
    char h1[17], h2[17];
    CHECK(cheats_variants_hash("extracted/exefs", h1) && strlen(h1) == 16);
    write_file("d.txt", FILE_TEXT, strlen(FILE_TEXT));  // (another file: other variants, another hash)
    CHECK(cheats_import("d.txt", "extracted/exefs", why, sizeof why) && cheats_variants_hash("extracted/exefs", h2) && strcmp(h1, h2));
    return ok;
}

static int code_table(void) {  // which word is on at each variant site
    int ok = 1;
    cheat_code_reset();
    CHECK(cheat_code_add(0x7000001000ull, 0x52800C80) && cheat_code_add(0x7000001000ull, 0xB9400000));
    CHECK(cheat_code_word(0x7000001000ull) == CHEAT_CODE_ORIGINAL);   // (nothing on: the game's own)
    CHECK(cheat_code_word(0x7000009000ull) == CHEAT_CODE_ORIGINAL);   // (not a site)
    uint32_t w = 0x52800C80;
    CHECK(cheat_code_write(0x7000001000ull, &w, 4) && cheat_code_word(0x7000001000ull) == 0x52800C80);
    w = 0x11111111;  // (a word that isn't compiled in - e.g. the original, restored by an "Off" cheat): back to the game's
    CHECK(cheat_code_write(0x7000001000ull, &w, 4) && cheat_code_word(0x7000001000ull) == CHEAT_CODE_ORIGINAL);
    CHECK(!cheat_code_write(0x7000009000ull, &w, 4));                 // (not a site: refused)
    CHECK(!cheat_code_write(0x7000001001ull, &w, 4));                 // (unaligned)
    uint64_t two = 0xD503201F52800020ull;
    CHECK(cheat_code_add(0x7000002000ull, 0x52800020) && cheat_code_add(0x7000002004ull, 0xD503201F));
    CHECK(cheat_code_write(0x7000002000ull, &two, 8) && cheat_code_word(0x7000002000ull) == 0x52800020 &&
          cheat_code_word(0x7000002004ull) == 0xD503201F);
    cheat_code_reset();
    CHECK(cheat_code_word(0x7000002000ull) == CHEAT_CODE_ORIGINAL);
    return ok;
}

static bool g_redirect_seen;
static bool fake_code_write(uint64_t a, const void *v, unsigned n) {
    uint32_t w;
    memcpy(&w, v, 4);
    g_redirect_seen = a == MAIN + 0x1000 && n == 4 && w == 0x52800C80;
    return true;
}
static int vm_code_redirect(void) {  // the VM: a write into main's code goes to code_write, not refused
    int ok = 1;
    clear();
    CheatEnv e = env(0);
    e.code_write = fake_code_write;
    CheatState st = {{0}};
    uint32_t ops[] = {0x04000000, 0x00001000, 0x52800C80};
    g_redirect_seen = false;
    CHECK(cheat_vm_run(ops, 3, &e, &st) == CHEAT_OK && g_redirect_seen);
    uint32_t v;
    memcpy(&v, g_main + 0x1000, 4);
    CHECK(v == 0);  // (memory untouched: the variant is picked instead)
    return ok;
}

static int code_built_list(void) {  // the list the game code was built with: loaded only if it is for this cheat file
    int ok = 1;
    fresh_tmp();
    exefs_with_build(PLA_111);
    uint8_t text_size[4] = {0x00, 0x40, 0x00, 0x00};
    FILE *f = fopen("extracted/exefs/main", "r+b");
    fseek(f, 0x18, SEEK_SET), fwrite(text_size, 1, 4, f), fclose(f);
    write_file("c.txt", CODE_TEXT, strlen(CODE_TEXT));
    char why[256], h[17], list[512];
    CHECK(cheats_import("c.txt", "extracted/exefs", why, sizeof why) && cheats_variants_hash("extracted/exefs", h));
    // the generator left 0x2004 out (say): the list has the other three
    snprintf(list, sizeof list, "hash %s\n0x1000 0x52800c80\n0x1000 0xb9400000\n0x2000 0x52800020\n", h);
    write_file("extracted/lasgame.so.variants", list, strlen(list));
    CHECK(cheats_code_load("extracted/exefs", "extracted/lasgame.so.variants", 0x7000000000ull) == 3);
    CheatList l;
    CHECK(cheats_load("extracted/exefs", &l) == 4);
    CHECK(cheat_code_supported(&l.c[0], 0x7000000000ull, 0, 0x4000));   // (Always Level 100: compiled in)
    CHECK(cheat_code_supported(&l.c[1], 0x7000000000ull, 0, 0x4000));   // (its Off)
    CHECK(!cheat_code_supported(&l.c[2], 0x7000000000ull, 0, 0x4000));  // (Two at once: half of it left out)
    CHECK(cheat_code_supported(&l.c[3], 0x7000000000ull, 0, 0x4000));   // (no code writes: nothing needed)
    cheats_free(&l);
    // a list for another cheat file (an older import): not loaded, nothing on
    write_file("extracted/lasgame.so.variants", "hash 0123456789abcdef\n0x1000 0x52800c80\n", 40);
    CHECK(cheats_code_load("extracted/exefs", "extracted/lasgame.so.variants", 0x7000000000ull) == -1);
    CHECK(cheat_code_word(0x7000001000ull) == CHEAT_CODE_ORIGINAL);
    uint32_t w = 0x52800C80;
    CHECK(!cheat_code_write(0x7000001000ull, &w, 4));  // (no sites)
    return ok;
}

static int prepare_copied_file(void) {  // a cheat file copied in (the AppImage's bundled one): its variants made on demand
    int ok = 1;
    fresh_tmp();
    exefs_with_build(PLA_111);
    uint8_t text_size[4] = {0x00, 0x40, 0x00, 0x00};
    FILE *f = fopen("extracted/exefs/main", "r+b");
    fseek(f, 0x18, SEEK_SET), fwrite(text_size, 1, 4, f), fclose(f);
    mkdir("cheats", 0755);
    write_file("cheats/AEE8F150DDA1B5A8.txt", CODE_TEXT, strlen(CODE_TEXT));
    char h[17], p[PATH_MAX];
    CHECK(!cheats_variants_hash("extracted/exefs", h));  // (none yet)
    CHECK(cheats_prepare("extracted/exefs"));
    CHECK(cheats_variants_hash("extracted/exefs", h));
    CHECK(cheats_variants_path("extracted/exefs", p, sizeof p));
    struct stat st1, st2;
    stat(p, &st1);
    CHECK(cheats_prepare("extracted/exefs"));  // (up to date: left as it is)
    stat(p, &st2);
    CHECK(st1.st_mtime == st2.st_mtime && st1.st_ino == st2.st_ino);  // (not written again)
    fresh_tmp();
    exefs_with_build(PLA_111);
    CHECK(cheats_prepare("extracted/exefs"));  // (no cheat file: nothing to do, not an error)
    CHECK(!cheats_variants_hash("extracted/exefs", h));
    return ok;
}

int main(void) {
    struct { const char *name; int (*fn)(void); } tests[] = {
        {"parse_file", parse_file}, {"op0_store_static", op0_store_static}, {"op0_code_blocked", op0_code_blocked},
        {"op1_conditional", op1_conditional}, {"op3_loop_op6_store", op3_loop_op6_store},
        {"op5_pointer_chain", op5_pointer_chain}, {"op8_keys", op8_keys}, {"op9_arith", op9_arith},
        {"opA_store_reg", opA_store_reg}, {"opC_registers", opC_registers}, {"bad_and_bounded", bad_and_bounded},
        {"code_patch_detect", code_patch_detect}, {"build_id_name", build_id_name}, {"import_txt", import_txt},
        {"import_zip", import_zip}, {"toggles_saved", toggles_saved},
        {"variants_list", variants_list}, {"variants_file", variants_file}, {"code_table", code_table},
        {"vm_code_redirect", vm_code_redirect}, {"code_built_list", code_built_list},
        {"prepare_copied_file", prepare_copied_file},
    };
    for (size_t i = 0; i < sizeof tests / sizeof *tests; i++) {
        g_run++;
        int ok = tests[i].fn();
        printf("%s %s\n", ok ? "ok  " : "FAIL", tests[i].name);
        if (!ok) g_fail++;
    }
    printf("cheats: %d/%d passed\n", g_run - g_fail, g_run);
    return g_fail ? 1 : 0;
}
