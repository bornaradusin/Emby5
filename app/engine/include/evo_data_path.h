#ifndef EVO_DATA_PATH_H
#define EVO_DATA_PATH_H
/* ShadowMountPlus storage; no savedata fallback or automatic migration. */
#define EVO_DATA_DIR "/data/Emby5"
#ifdef __cplusplus
extern "C" {
#endif
const char *evo_data_dir(void);
const char *evo_data_path(const char *leaf);
void evo_data_path_rebind(void);
int evo_mkdir(const char *path);
#ifdef __cplusplus
}
#endif
#endif
