/*
 * Module: evo_data_path - see include/evo_data_path.h.
 */

#include "evo_data_path.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef EVO_APP_MODULE
/* libc's mkdir() is not in the clean-room libc.prx surface (imports.txt lists
 * sceKernelMkdir, not the POSIX name). Go straight to the libkernel export. */
int sceKernelMkdir(const char *path, int mode);
#endif

/* Persistent storage is provided by ShadowMountPlus. Never use /download0. */
static const char *resolve_root(void)
{
#ifndef EVO_APP_MODULE
    const char *over = getenv("EVO_DATA_DIR_OVERRIDE");
    if (over && *over) return over;
#endif
    return "/data/Emby5";
}
void evo_data_path_rebind(void) { }
const char *evo_data_dir(void) { return resolve_root(); }
const char *evo_data_path(const char *leaf)
{
    static _Thread_local char buf[512];
    const char *root = resolve_root();
    if (!leaf) leaf = "";
    while (*leaf == '/') ++leaf;
    /* Older Emby5 code stores configuration below the 'emby5' subfolder. */
    if (strcmp(leaf, "emby5") == 0) leaf = "config";
    else if (strncmp(leaf, "emby5/", 6) == 0) {
        snprintf(buf, sizeof buf, "%s/config/%s", root, leaf + 6);
        return buf;
    }
    if (snprintf(buf, sizeof buf, "%s/%s", root, leaf) >= (int)sizeof buf) return root;
    return buf;
}

int evo_mkdir(const char *path)
{
#ifdef EVO_APP_MODULE
    int r = sceKernelMkdir(path, 0777);
    if (r == 0 || r == (int)0x80020011 /* SCE_KERNEL_ERROR_EEXIST */)
        return 0;
    return r;
#else
    if (mkdir(path, 0777) == 0 || errno == EEXIST)
        return 0;
    return -1;
#endif
}
