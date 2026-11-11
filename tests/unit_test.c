// Unit tests for the patched runtime (patches/: nxpkg, gamesetup, settings): built with upstream's own sources by tests/run_unit.sh.
// Fixtures live in a temp folder whose name has a space and an e-acute, as players' folders do.
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gamesetup.h"
#include "nxpkg.h"
#include "settings.h"
#include "cheats.h"
#define ROOT_EXEFS "extracted/exefs"

static int g_run, g_fail;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ok = 0; } \
    } while (0)

static char g_tmp[PATH_MAX];
static const uint64_t TITLES[] = {0x01001F5010DFA000ull, 0};

static void path(char *out, const char *name) { snprintf(out, PATH_MAX, "%s/%s", g_tmp, name); }
static void write_file(const char *p, const void *data, size_t n) {
    FILE *f = fopen(p, "wb");
    if (!f) { perror(p); exit(2); }
    fwrite(data, 1, n, f);
    fclose(f);
}
static void write_zeros(const char *p, size_t n) {
    void *z = calloc(1, n);
    write_file(p, z, n);
    free(z);
}
static void write_keys(const char *p) {
    const char *k = "header_key = 0000000000000000000000000000000000000000000000000000000000000000\n";
    write_file(p, k, strlen(k));
}
static void fresh_tmp(void) {
    snprintf(g_tmp, sizeof g_tmp, "%s/nxpkg test é XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
    if (!mkdtemp(g_tmp)) { perror("mkdtemp"); exit(2); }
}

static int no_keys(void) {
    int ok = 1, err = -1;
    char why[256], keys[PATH_MAX], pkg[PATH_MAX];
    path(keys, "missing.keys");
    path(pkg, "a b.nsp");
    write_zeros(pkg, 64);
    const char *pk[] = {pkg};
    NxPkg *p = nxpkg_open_files(pk, 1, keys, TITLES, &err, why, sizeof why);
    CHECK(p == NULL);
    CHECK(err == NXPKG_NO_KEYS);
    return ok;
}

static int no_game(void) {
    int ok = 1, err = -1;
    char why[256], keys[PATH_MAX], pkg[PATH_MAX];
    path(keys, "prod.keys");
    write_keys(keys);
    path(pkg, "a b.nsp");
    write_zeros(pkg, 64);
    const char *pk[] = {pkg};
    NxPkg *p = nxpkg_open_files(pk, 1, keys, TITLES, &err, why, sizeof why);
    CHECK(p == NULL);
    CHECK(err == NXPKG_NO_GAME);
    return ok;
}

static int files_spaces(void) {
    int ok = 1, err = -1;
    char why[256], keys[PATH_MAX], pkg[PATH_MAX];
    path(keys, "my keys/prod.keys");
    char dir[PATH_MAX];
    path(dir, "my keys");
    mkdir(dir, 0755);
    write_keys(keys);
    path(pkg, "Pokémon Legends Arceus [01001F5010DFA000][v0].nsp");
    write_zeros(pkg, 64);
    const char *pk[] = {pkg};
    NxPkg *p = nxpkg_open_files(pk, 1, keys, TITLES, &err, why, sizeof why);
    CHECK(p == NULL);
    CHECK(err == NXPKG_NO_GAME);
    CHECK(strstr(why, "no packages") == NULL);  // (opened, not "no packages found")
    return ok;
}

static int files_order(void) {
    int ok = 1, e1 = -1, e2 = -1;
    char w1[256], w2[256], keys[PATH_MAX], base[PATH_MAX], upd[PATH_MAX];
    path(keys, "prod.keys");
    write_keys(keys);
    path(base, "base.nsp");
    path(upd, "update v1.1.1.nsp");
    write_zeros(base, 64);
    write_zeros(upd, 64);
    const char *a[] = {base, upd}, *b[] = {upd, base};
    NxPkg *p1 = nxpkg_open_files(a, 2, keys, TITLES, &e1, w1, sizeof w1);
    NxPkg *p2 = nxpkg_open_files(b, 2, keys, TITLES, &e2, w2, sizeof w2);
    CHECK(p1 == NULL && p2 == NULL);
    CHECK(e1 == e2);
    CHECK(strcmp(w1, w2) == 0);
    return ok;
}

static int default_none(void) {
    int ok = 1;
    char out[PATH_MAX] = "unchanged";
    setenv("HOME", g_tmp, 1);
    setenv("XDG_DATA_HOME", g_tmp, 1);
    if (chdir(g_tmp) != 0) return 0;
    CHECK(!nxpkg_keys_default(out, sizeof out));
    return ok;
}

static int default_switch(void) {
    int ok = 1;
    char out[PATH_MAX] = "", dir[PATH_MAX], keys[PATH_MAX];
    setenv("HOME", g_tmp, 1);
    setenv("XDG_DATA_HOME", g_tmp, 1);
    if (chdir(g_tmp) != 0) return 0;
    path(dir, ".switch");
    mkdir(dir, 0755);
    path(keys, ".switch/prod.keys");
    write_keys(keys);
    CHECK(nxpkg_keys_default(out, sizeof out));
    size_t n = strlen(out), s = strlen("/.switch/prod.keys");
    CHECK(n >= s && strcmp(out + n - s, "/.switch/prod.keys") == 0);
    return ok;
}

// ---- patch 0002: the 1.1.1 check, the saved picker paths
static void write_exefs_in(const char *sub, const uint8_t build_id[8], uint64_t program) {  // <tmp>/<sub>/exefs
    char dir[PATH_MAX], p[PATH_MAX], base[PATH_MAX];
    path(base, sub);
    mkdir(base, 0755);
    snprintf(dir, sizeof dir, "%s/exefs", base);
    mkdir(dir, 0755);
    uint8_t main_nso[0x60] = {'N', 'S', 'O', '0'};
    memcpy(main_nso + 0x40, build_id, 8);
    snprintf(p, sizeof p, "%s/main", dir);
    write_file(p, main_nso, sizeof main_nso);
    uint8_t npdm[0x98] = {'M', 'E', 'T', 'A'};
    uint32_t aci_off = 0x80;
    memcpy(npdm + 0x70, &aci_off, 4);
    memcpy(npdm + 0x80, "ACI0", 4);
    memcpy(npdm + 0x90, &program, 8);
    snprintf(p, sizeof p, "%s/main.npdm", dir);
    write_file(p, npdm, sizeof npdm);
}
static void write_exefs(const uint8_t build_id[8], uint64_t program) { write_exefs_in(".", build_id, program); }
static const uint8_t PLA_111[8] = {0xae, 0xe8, 0xf1, 0x50, 0xdd, 0xa1, 0xb5, 0xa8};

static int pla111_yes(void) {
    int ok = 1;
    char dir[PATH_MAX];
    write_exefs(PLA_111, 0x01001F5010DFA000ull);
    path(dir, "exefs");
    CHECK(game_is_pla_111(dir));
    return ok;
}

static int pla111_other_build(void) {
    int ok = 1;
    char dir[PATH_MAX];
    uint8_t other[8];
    memcpy(other, PLA_111, 8);
    other[0] ^= 1;
    write_exefs(other, 0x01001F5010DFA000ull);
    path(dir, "exefs");
    CHECK(!game_is_pla_111(dir));
    write_exefs(PLA_111, 0x0100187003A36000ull);  // (the build id, but another game's program id)
    CHECK(!game_is_pla_111(dir));
    return ok;
}

static int settings_roundtrip(void) {
    int ok = 1;
    char ini[PATH_MAX];
    path(ini, "larecomp.ini");
    setenv("LARECOMP_SETTINGS", ini, 1);
    settings_defaults(&g_settings);
    snprintf(g_settings.setup_base, sizeof g_settings.setup_base, "%s", "/home/me/My Games/Pokémon Legends Arceus [v0].nsp");
    snprintf(g_settings.setup_update, sizeof g_settings.setup_update, "%s", "/home/me/My Games/PLA update v1.1.1.nsp");
    snprintf(g_settings.setup_keys, sizeof g_settings.setup_keys, "%s", "/home/me/.switch/prod.keys");
    settings_save();
    memset(&g_settings, 0, sizeof g_settings);
    settings_load();
    CHECK(strcmp(g_settings.setup_base, "/home/me/My Games/Pokémon Legends Arceus [v0].nsp") == 0);
    CHECK(strcmp(g_settings.setup_update, "/home/me/My Games/PLA update v1.1.1.nsp") == 0);
    CHECK(strcmp(g_settings.setup_keys, "/home/me/.switch/prod.keys") == 0);
    return ok;
}

// ---- patch 0004: game files set up, game code missing or built by another version -> rebuilt
extern bool g_stub_gamelib_present;  // (tests/unit_stubs.c: what gamelib_present answers)
static void set_up_game_files(void) {  // <tmp>/extracted: exefs (Arceus 1.1.1) + romfs.bin, as a setup leaves them
    char p[PATH_MAX];
    write_exefs_in("extracted", PLA_111, 0x01001F5010DFA000ull);
    path(p, "extracted/romfs.bin");
    write_zeros(p, 64);
    if (chdir(g_tmp) != 0) exit(2);
}

static int needs_code_stale(void) {
    int ok = 1;
    set_up_game_files();
    g_stub_gamelib_present = false;  // (no library, or one with another id)
    CHECK(setup_needs_code());
    CHECK(!setup_ready());
    return ok;
}

static int needs_code_ready(void) {
    int ok = 1;
    set_up_game_files();
    g_stub_gamelib_present = true;
    CHECK(!setup_needs_code());
    CHECK(setup_ready());
    return ok;
}

static int needs_code_nothing(void) {
    int ok = 1;
    if (chdir(g_tmp) != 0) return 0;  // (no extracted/: never set up - the pickers, not a rebuild)
    g_stub_gamelib_present = false;
    CHECK(!setup_needs_code());
    return ok;
}

// ---- patches 0010 / 0013: code cheats need the game code built with them - offered, never started by itself
static int needs_code_cheats(void) {
    int ok = 1;
    set_up_game_files();
    g_stub_gamelib_present = true;
    CHECK(!setup_needs_code() && setup_code_cheats_built());  // (no cheats, no list: as built)
    mkdir("cheats", 0755);
    write_file("cheats/AEE8F150DDA1B5A8.variants.json", "{\"code\": {\"0x1000\": [\"0x52800c80\"]}}\n", 37);
    CHECK(!setup_needs_code());           // (a new cheat file: the game still plays - no rebuild unasked)
    CHECK(!setup_code_cheats_built());    // (but its code cheats aren't built in: the Cheats tab offers the rebuild)
    write_file("extracted/lasgame.so.variants", "hash 0000000000000000\n", 22);
    CHECK(!setup_code_cheats_built());    // (built for another cheat file)
    char h[17];
    CHECK(cheats_variants_hash(ROOT_EXEFS, h));
    char line[64];
    snprintf(line, sizeof line, "hash %s\n0x1000 0x52800c80\n", h);
    write_file("extracted/lasgame.so.variants", line, strlen(line));
    CHECK(setup_code_cheats_built());     // (built with them)
    g_stub_gamelib_present = false;
    CHECK(setup_needs_code());            // (the game code itself missing or another version's: rebuilt, as before)
    return ok;
}

int main(void) {
    struct { const char *name; int (*fn)(void); } tests[] = {
        {"no_keys", no_keys}, {"no_game", no_game}, {"files_spaces", files_spaces}, {"files_order", files_order},
        {"default_none", default_none}, {"default_switch", default_switch},
        {"pla111_yes", pla111_yes}, {"pla111_other_build", pla111_other_build}, {"settings_roundtrip", settings_roundtrip},
        {"needs_code_stale", needs_code_stale}, {"needs_code_ready", needs_code_ready}, {"needs_code_nothing", needs_code_nothing},
        {"needs_code_cheats", needs_code_cheats},
    };
    for (size_t i = 0; i < sizeof tests / sizeof *tests; i++) {
        fresh_tmp();  // (each test in its own folder: none sees another's files)
        g_run++;
        int ok = tests[i].fn();
        printf("%s %s\n", ok ? "ok  " : "FAIL", tests[i].name);
        if (!ok) g_fail++;
    }
    printf("%d/%d passed\n", g_run - g_fail, g_run);
    return g_fail ? 1 : 0;
}
