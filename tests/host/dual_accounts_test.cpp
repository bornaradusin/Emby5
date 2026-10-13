#include "app/accounts.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
static std::string root;
extern "C" {
const char *evo_data_dir() { return root.c_str(); }
const char *evo_data_path(const char *leaf) { static thread_local std::string p; p=root+"/"+leaf; return p.c_str(); }
int evo_mkdir(const char *path) { std::filesystem::create_directories(path); return 0; }
}
int main() {
    using accounts::Account;
    char dir[]="/tmp/emby5-account-test-XXXXXX"; assert(mkdtemp(dir)); root=dir;
    std::filesystem::create_directories(root+"/emby5");
    { std::ofstream f(root+"/emby5/accounts.json"); f << R"({"accounts":[{"server":"http://emby","serverName":"Home","userId":"u","userName":"Viewer","token":"e-token","serverId":"same-id"}],"lastServer":"http://emby","lastUser":"u"})"; }
    auto saved=accounts::load(); assert(saved.size()==1 && saved[0].server_type=="emby");
    auto a=saved[0]; Account last; assert(accounts::last(&last) && last.token=="e-token");
    auto b=a; b.server="http://jellyfin"; b.server_type="jellyfin"; b.token="j-token";
    assert(!accounts::same_server(a,b));
    assert(accounts::remember(b).empty()); saved=accounts::load(); assert(saved.size()==2);
    assert(saved[0].server_type=="emby" && saved[1].server_type=="jellyfin");
    assert(saved[0].token=="e-token" && saved[1].token=="j-token");
    Account old{"http://old","Old","same-user","V","","old-token",""};
    Account different=old; different.server="http://different";
    assert(!accounts::same_server(old,different)); // shared user ID is not proof of server identity
    accounts::set_ps5_user(1); accounts::set_last(a.server,a.user_id);
    accounts::set_ps5_user(2); accounts::set_last(b.server,b.user_id);
    assert(accounts::last(&last) && last.server_type=="jellyfin");
    accounts::set_ps5_user(1); assert(accounts::last(&last) && last.server_type=="emby");
    auto moved=b; moved.server="https://new-jellyfin/base";
    auto from=accounts::remember(moved); assert(from.size()==1 && from[0]==b.server);
    saved=accounts::load(); assert(saved.size()==2 && saved[0].server==a.server);
    assert(saved[1].server==moved.server && saved[1].token=="j-token");
    // A new explicit login at a reused address discards the old server's tokens.
    auto replacement=moved; replacement.server_id="replacement-id"; replacement.token="new-token";
    assert(accounts::remember(replacement).empty()); saved=accounts::load(); assert(saved.size()==2);
    assert(saved[1].token=="new-token");
    accounts::forget(replacement.server,replacement.user_id); saved=accounts::load();
    assert(saved.size()==1 && saved[0].token=="e-token");
    std::filesystem::remove_all(root);
    std::cout<<"PASS: legacy account migration, provider/server/user isolation, tokens, per-PS5-user selection and address changes\n";
}
