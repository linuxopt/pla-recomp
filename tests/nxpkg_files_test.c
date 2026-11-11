// Unit tests for the patched runtime (patches/): built with upstream's own sources by tests/run_unit.sh.
// Fixtures live in a temp folder whose name has a space and an e-acute, as players' folders do.
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "nxpkg.h"

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

int main(void) {
    struct { const char *name; int (*fn)(void); } tests[] = {
        {"no_keys", no_keys}, {"no_game", no_game}, {"files_spaces", files_spaces}, {"files_order", files_order},
        {"default_none", default_none}, {"default_switch", default_switch},
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
