/* Transactional on-console installer used only by independent helper process. */
#pragma once
#include <string>
namespace update_install {
/* Requires application process to have exited. Never call from Emby5. */
bool apply(const std::string &staging_root, std::string *error);
/* Restore an interrupted transaction before another attempt. */
bool recover(std::string *error);
}
