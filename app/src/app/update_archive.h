/* Archive validation and extraction; shared by app and standalone updater. */
#pragma once
#include <string>
namespace update_archive {
/* Only app files under PPSA99515/, never user data, with strict ZIP parsing. */
bool unpack(const std::string &zip, const std::string &destination, std::string *error);
}
