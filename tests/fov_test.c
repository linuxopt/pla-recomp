// Tests of runtime/hle/pla_fov.c (patches/0008): the field-of-view setting for Legends: Arceus 1.1.1. The camera
// function's render camera is found at [[x0 + 0x3d0] + 0x78] + 0x1ec (found on a running 1.1.1: tests' fake objects
// are laid out the same way, at guest addresses - a fixed mapping at 0x7100000000).
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "cpu.h"
#include "settings.h"

float pla_fov_scaled(float game_fov, int target_deg);
bool pla_fov_record_ok(const float *fov);
int pla_fov_hook(Cpu *c, uint64_t addr);

static int g_run, g_fail, g_original_calls;
#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ok = 0; } \
    } while (0)
#define DEG(r) ((r) * 180.0 / M_PI)
#define RAD(d) ((d) * M_PI / 180.0)

// ---- fake guest memory: the camera object, the object it points to, the render camera
#define G 0x7100000000ull
static uint8_t *g;
static float *fov_at(void) { return (float *)(g + 0x2000 + 0x1ec); }
static void layout(void) {
    memset(g, 0, 0x4000);
    *(uint64_t *)(g + 0x3d0) = G + 0x1000;          // x0 + 0x3d0 -> obj
    *(uint64_t *)(g + 0x1000 + 0x78) = G + 0x2000;  // obj + 0x78 -> cam
    float *f = fov_at();
    f[-2] = 0.1f, f[-1] = 5000.0f, f[0] = (float)RAD(50), f[1] = 16.0f / 9, f[2] = -960.0f;  // near far fov aspect
}
// the game's function, run by the hook: it writes the frame's fov (50 degrees) as the game does
void rt_call_original(Cpu *c, uint64_t addr) {
    (void)addr;
    g_original_calls++;
    if (*(uint64_t *)(g + 0x3d0)) *fov_at() = (float)RAD(50);
    c->x[0] = 0xdead, c->x[30] = 0;  // (as a real call leaves them: x0 = its result - the hook keeps its own copy)
}

static int scaled_off(void) {
    int ok = 1;
    CHECK(pla_fov_scaled((float)RAD(50), 0) == (float)RAD(50));   // (0: the game's own)
    CHECK(pla_fov_scaled((float)RAD(50), 50) == (float)RAD(50));  // (50: the game's)
    CHECK(pla_fov_scaled((float)RAD(30), 0) == (float)RAD(30));
    return ok;
}

static int scaled_target(void) {
    int ok = 1;
    CHECK(fabs(DEG(pla_fov_scaled((float)RAD(50), 70)) - 70) < 0.01);
    CHECK(fabs(DEG(pla_fov_scaled((float)RAD(50), 90)) - 90) < 0.01);
    return ok;
}

static int scaled_keeps_zoom(void) {  // aiming (a narrower fov than 50): narrower still, by the same ratio of tangents
    int ok = 1;
    double n = pla_fov_scaled((float)RAD(30), 70);
    CHECK(DEG(n) > 30 && DEG(n) < 70);
    CHECK(fabs(tan(n / 2) / tan(RAD(15)) - tan(RAD(35)) / tan(RAD(25))) < 1e-4);
    return ok;
}

static int scaled_capped(void) {  // never 150 degrees or wider, never NaN
    int ok = 1;
    double n = pla_fov_scaled((float)RAD(120), 110);
    CHECK(DEG(n) <= 150 && DEG(n) > 120);
    CHECK(isnan(pla_fov_scaled(NAN, 70)));      // (not a fov: left as the game wrote it)
    CHECK(pla_fov_scaled(-1.0f, 70) == -1.0f);  // (not a fov: left as it is)
    return ok;
}

static int record_ok(void) {
    int ok = 1;
    layout();
    CHECK(pla_fov_record_ok(fov_at()));
    float *f = fov_at();
    f[1] = 0;  // (no aspect)
    CHECK(!pla_fov_record_ok(f));
    layout(), f[-2] = 0;  // (no near plane)
    CHECK(!pla_fov_record_ok(f));
    layout(), f[0] = NAN;
    CHECK(!pla_fov_record_ok(f));
    layout(), f[-1] = 0.05f;  // (far nearer than near)
    CHECK(!pla_fov_record_ok(f));
    return ok;
}

static int hook_widens(void) {
    int ok = 1;
    Cpu c;
    memset(&c, 0, sizeof c);
    c.x[0] = G;
    layout();
    g_settings.fov = 70, g_original_calls = 0;
    CHECK(pla_fov_hook(&c, 0x700193b65c) == 1);  // (handled: the game's function ran inside it)
    CHECK(g_original_calls == 1);
    CHECK(fabs(DEG(*fov_at()) - 70) < 0.01);
    return ok;
}

static int hook_off(void) {  // the setting off: the game's function runs as usual, nothing touched
    int ok = 1;
    Cpu c;
    memset(&c, 0, sizeof c);
    c.x[0] = G;
    layout();
    g_settings.fov = 0, g_original_calls = 0;
    CHECK(pla_fov_hook(&c, 0x700193b65c) == 0);
    CHECK(g_original_calls == 0);
    CHECK(*fov_at() == (float)RAD(50));
    return ok;
}

static int hook_bad_chain(void) {  // a pointer that isn't there (another scene, another object): the game's function
    int ok = 1;                    // runs, nothing is written, no crash
    Cpu c;
    memset(&c, 0, sizeof c);
    layout();
    g_settings.fov = 70;
    c.x[0] = G;
    *(uint64_t *)(g + 0x3d0) = 0;  // (obj missing)
    g_original_calls = 0;
    CHECK(pla_fov_hook(&c, 0x700193b65c) == 1 && g_original_calls == 1);
    layout();
    *(uint64_t *)(g + 0x1000 + 0x78) = 0x12345;  // (outside the guest window)
    CHECK(pla_fov_hook(&c, 0x700193b65c) == 1);
    layout();
    fov_at()[1] = 0;  // (not the render camera's layout)
    CHECK(pla_fov_hook(&c, 0x700193b65c) == 1 && *fov_at() == (float)RAD(50));
    c.x[0] = 0;  // (no camera object)
    CHECK(pla_fov_hook(&c, 0x700193b65c) == 1);
    return ok;
}

static int setting_saved(void) {  // larecomp.ini keeps it
    int ok = 1;
    char ini[] = "/tmp/fov_test_ini_XXXXXX";
    int fd = mkstemp(ini);
    if (fd < 0) return 0;
    setenv("LARECOMP_SETTINGS", ini, 1);
    settings_defaults(&g_settings);
    CHECK(g_settings.fov == 0);  // (a new install: the game's own)
    g_settings.fov = 85;
    settings_save();
    g_settings.fov = 0;
    settings_load();
    CHECK(g_settings.fov == 85);
    remove(ini);
    return ok;
}

int main(void) {
    g = mmap((void *)G, 0x4000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (g != (void *)G) { printf("fov: can't map the fake guest memory\n"); return 2; }
    struct { const char *name; int (*fn)(void); } tests[] = {
        {"scaled_off", scaled_off}, {"scaled_target", scaled_target}, {"scaled_keeps_zoom", scaled_keeps_zoom},
        {"scaled_capped", scaled_capped}, {"record_ok", record_ok}, {"hook_widens", hook_widens},
        {"hook_off", hook_off}, {"hook_bad_chain", hook_bad_chain}, {"setting_saved", setting_saved},
    };
    for (size_t i = 0; i < sizeof tests / sizeof *tests; i++) {
        g_run++;
        int ok = tests[i].fn();
        printf("%s %s\n", ok ? "ok  " : "FAIL", tests[i].name);
        if (!ok) g_fail++;
    }
    printf("fov: %d/%d passed\n", g_run - g_fail, g_run);
    return g_fail ? 1 : 0;
}
