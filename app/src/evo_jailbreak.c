#include "evo_jailbreak.h"
#include "evo_boot_trace.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define JB_TMP  "/download0/etahen_jailbreak.tmp"
#define JB_FILE "/download0/etahen_jailbreak"

static int request_jailbreak(void)
{
    char json[64];
    const int n = snprintf(json, sizeof json, "{\"PID\":\"%d\"}", (int)getpid());
    if (n <= 0 || n >= (int)sizeof json)
        return 0;

    int fd = open(JB_TMP, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return 0;

    const ssize_t wr = write(fd, json, (size_t)n);
    close(fd);

    if (wr != n) {
        unlink(JB_TMP);
        return 0;
    }

    if (rename(JB_TMP, JB_FILE) != 0) {
        unlink(JB_TMP);
        return 0;
    }

    return 1;
}

static int wait_for_open(int tries, useconds_t delay_us)
{
    if (evo_jailbreak_is_open())
        return 1;

    for (int i = 0; i < tries; i++) {
        request_jailbreak();

        for (int j = 0; j < 4; j++) {
            usleep(delay_us);
            if (evo_jailbreak_is_open()) {
                evo_bt("emby5: sandbox promoted; /data is available");
                return 1;
            }
        }
    }

    evo_bt("emby5: sandbox promotion did not land");
    return 0;
}

int evo_jailbreak_self(void)
{
    return wait_for_open(4, 250000);
}

int evo_jailbreak_ensure(void)
{
    return wait_for_open(8, 250000);
}

int evo_jailbreak_poll(void)
{
    static int seen_open = 0;

    if (seen_open)
        return 0;

    if (evo_jailbreak_is_open()) {
        seen_open = 1;
        return 1;
    }

    request_jailbreak();
    return 0;
}
