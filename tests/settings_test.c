// tests/settings_test.c: frame rates above 60 are ordinary settings (patches/0019) - kept as saved, with or without the
// old "experimental" switch. Its own program: settings_load() reads larecomp.ini once per process.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "settings.h"

int main(void) {
    char ini[] = "/tmp/settings_test_XXXXXX";
    int fd = mkstemp(ini);
    if (fd < 0) return 2;
    const char *t = "fps = 144\nexperimental = 0\n";  // (an ini saved with the old switch off and 144 fps chosen)
    if (write(fd, t, strlen(t)) != (ssize_t)strlen(t)) return 2;
    close(fd);
    setenv("LARECOMP_SETTINGS", ini, 1);
    settings_load();
    int ok = g_settings.fps == 144;
    printf("%s high_rate_kept (fps %d)\n", ok ? "ok  " : "FAIL", g_settings.fps);
    remove(ini);
    printf("settings: %d/1 passed\n", ok);
    return ok ? 0 : 1;
}
