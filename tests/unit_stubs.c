// Stand-ins for the runtime functions gamesetup.c / settings.c link against but the unit tests don't exercise.
// The key-binding hooks are called by settings_load / settings_save: no bindings here (harmless no-ops). Everything
// else aborts, so a test that reaches one fails loudly instead of passing by accident.
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

bool bind_settings_load(const char *key, const char *val) { (void)key, (void)val; return false; }
void bind_settings_save(FILE *f) { (void)f; }

bool g_stub_gamelib_present;  // (the game library's presence, as the test sets it)
bool gamelib_present(const char *game_dir) { (void)game_dir; return g_stub_gamelib_present; }

#define NOT_IN_TESTS(name) \
    void name(void) { fprintf(stderr, "unit: %s called (a stub)\n", #name); abort(); }
NOT_IN_TESTS(gamebuild_available)
NOT_IN_TESTS(gamebuild_run)
NOT_IN_TESTS(plat_remove_tree)
NOT_IN_TESTS(romfs_build)
NOT_IN_TESTS(romfs_copy_file)
NOT_IN_TESTS(romfs_copy_tree)
NOT_IN_TESTS(rt_remove_game_files)
