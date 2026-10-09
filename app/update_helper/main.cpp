/* Emby5's independent updater payload. No ProsperoTV updater code reused.
 * The PS5 ELF loader on localhost:9021 starts this payload before Emby5 exits.
 * The installer does not touch application files until the old PID is gone. */
#include "app/update_install.h"
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

int main(){
    constexpr const char *pidfile="/data/homebrew/PPSA99515/.emby-update-stage/parent.pid";
    FILE *file=fopen(pidfile,"rb");
    if(!file)return 1;
    long old_pid=0;
    const bool parsed=fscanf(file,"%ld",&old_pid)==1 && old_pid>1 && old_pid<10000000;
    fclose(file);
    if(!parsed)return 2;
    /* ACK only after a valid prepared handoff. */
    if(write(STDOUT_FILENO,"READY\n",6)!=6)return 3;
    /* Fail closed if the old application cannot be observed exiting. */
    bool closed=false;
    for(int n=0;n<600;++n){
        if(kill((pid_t)old_pid,0)<0 && errno==ESRCH){closed=true;break;}
        usleep(100000);
    }
    if(!closed)return 4;
    std::string error;
    if(!update_install::apply("/data/homebrew/PPSA99515/.emby-update-stage/unpacked/PPSA99515",&error))return 5;
    return 0;
}
