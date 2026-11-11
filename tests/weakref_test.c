// Tests of runtime/hle/pla_weakref.c (patches/0020): Legends: Arceus 1.1.1's f_70016e33ec locks the weak reference at
// x0 + 0x88 (a control block: strong count at +0xc, the object's +0x50 at +0x18) and reads +0x118 of what it gets
// without checking - a reference whose object is gone crashed a cutscene at 144 fps (signal 11 at 0x118). The hook
// gives the game's own "nothing" result (*x8 = 0, which its caller skips) for an empty or expired reference, and lets
// the game's function run for a live one. Fake objects at guest addresses (a fixed mapping at 0x7100000000).
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "cpu.h"

int pla_weakref_hook(Cpu *c, uint64_t addr);

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ok = 0; } \
    } while (0)

#define G 0x7100000000ull
#define FN 0x70016e33ecull
static uint8_t *g;
#define U64(off) (*(uint64_t *)(g + (off)))
#define U32(off) (*(uint32_t *)(g + (off)))
enum { OBJ = 0x0, CB = 0x1000, TARGET = 0x2000, RESULT = 0x3000 };

static Cpu c;
static void layout(uint32_t count, int link, int target) {  // the object, its reference's control block, the result
    memset(g, 0, 0x4000);
    memset(&c, 0, sizeof c);
    U64(OBJ + 0x88) = link ? G + CB : 0;
    U32(CB + 0xc) = count;
    U64(CB + 0x18) = target ? G + TARGET + 0x50 : 0;
    U64(RESULT) = 0x1234;  // (what was there before: the game's function or the hook writes it)
    c.x[0] = G + OBJ, c.x[1] = 3, c.x[8] = G + RESULT;
}

static int live_runs_the_game(void) {
    int ok = 1;
    layout(1, 1, 1);
    CHECK(pla_weakref_hook(&c, FN) == 0);  // (0: the game's function runs as usual)
    CHECK(U64(RESULT) == 0x1234);
    CHECK(c.x[0] == G + OBJ && c.x[8] == G + RESULT);
    return ok;
}

static int empty_link_gives_nothing(void) {
    int ok = 1;
    layout(1, 0, 1);
    CHECK(pla_weakref_hook(&c, FN) == 1);  // (1: handled, the function returns at once)
    CHECK(U64(RESULT) == 0);
    return ok;
}

static int expired_gives_nothing(void) {
    int ok = 1;
    layout(0, 1, 1);  // (no strong references left: the object is being or has been destroyed)
    CHECK(pla_weakref_hook(&c, FN) == 1);
    CHECK(U64(RESULT) == 0);
    return ok;
}

static int cleared_target_gives_nothing(void) {
    int ok = 1;
    layout(1, 1, 0);  // (the control block's object cleared: the game's own lock gives null too)
    CHECK(pla_weakref_hook(&c, FN) == 1);
    CHECK(U64(RESULT) == 0);
    return ok;
}

static int not_an_object_left_alone(void) {  // anything that doesn't look like the game's objects: the game decides
    int ok = 1;
    layout(0, 1, 1);
    c.x[0] = 0x10;
    CHECK(pla_weakref_hook(&c, FN) == 0);
    layout(0, 1, 1);
    c.x[8] = 0;  // (nowhere to write the result)
    CHECK(pla_weakref_hook(&c, FN) == 0);
    CHECK(U64(RESULT) == 0x1234);
    layout(1, 1, 1);
    U64(OBJ + 0x88) = 0x1001;  // (a misaligned control block)
    CHECK(pla_weakref_hook(&c, FN) == 0);
    return ok;
}

int main(void) {
    g = mmap((void *)G, 0x4000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (g != (void *)G) { printf("weakref: can't map the fake guest memory\n"); return 2; }
    struct { const char *name; int (*fn)(void); } tests[] = {
        {"live_runs_the_game", live_runs_the_game}, {"empty_link_gives_nothing", empty_link_gives_nothing},
        {"expired_gives_nothing", expired_gives_nothing}, {"cleared_target_gives_nothing", cleared_target_gives_nothing},
        {"not_an_object_left_alone", not_an_object_left_alone},
    };
    int n = sizeof tests / sizeof *tests, passed = 0;
    for (int i = 0; i < n; i++) {
        int ok = tests[i].fn();
        printf("%s %s\n", ok ? "ok  " : "FAIL", tests[i].name);
        passed += ok;
    }
    printf("weakref: %d/%d passed\n", passed, n);
    return passed == n ? 0 : 1;
}
