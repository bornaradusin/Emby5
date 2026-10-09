/* Emby5 independent ZIP extraction for trusted application-only packages.
   Zip64/encryption/symlinks/paths outside PPSA99515 are rejected. */
#include "app/update_archive.h"
#include <zlib.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <dirent.h>
namespace update_archive {
namespace {
uint16_t r16(const unsigned char *p){return uint16_t(p[0]) | uint16_t(p[1])<<8;}
uint32_t r32(const unsigned char *p){return uint32_t(r16(p)) | uint32_t(r16(p+2))<<16;}
bool error(std::string *e,const char *s){if(e)*e=s;return false;}
bool synced(FILE *file){
    if(fflush(file)!=0)return false;
    if(fsync(fileno(file))==0)return true;
    return errno==EINVAL || errno==ENOSYS || errno==ENOTSUP || errno==EOPNOTSUPP;
}
bool mkdirs(const std::string &path){
    for(size_t i=1;i<=path.size();++i){
        if(i!=path.size() && path[i]!='/') continue;
        const std::string p=path.substr(0,i);
        if(!p.empty() && mkdir(p.c_str(),0755)!=0 && errno!=EEXIST)return false;
    }return true;
}
bool clear_stage(const std::string &path){
    struct stat st{};
    if(lstat(path.c_str(),&st)!=0)return errno==ENOENT;
    if(!S_ISDIR(st.st_mode))return unlink(path.c_str())==0;
    DIR *d=opendir(path.c_str());if(!d)return false;
    struct dirent *entry;bool ok=true;
    while((entry=readdir(d))!=nullptr){
        if(!strcmp(entry->d_name,".") || !strcmp(entry->d_name,".."))continue;
        if(!clear_stage(path+"/"+entry->d_name)){ok=false;break;}
    }
    closedir(d);
    return ok && rmdir(path.c_str())==0;
}
struct File {std::string name;uint32_t offset,packed,unpacked,crc;uint16_t method;};
bool valid_name(const std::string &name){
    const std::string root="PPSA99515/";
    if(name.compare(0,root.size(),root)!=0 || name.size()==root.size() || name.size()>240 || name.back()=='/')return false;
    for(unsigned char ch:name)if(ch<32 || ch==127)return false;
    if(name.find('\\')!=std::string::npos || name.find(':')!=std::string::npos || name.find("//")!=std::string::npos)return false;
    if(name.find("../")!=std::string::npos || name.find("/..")!=std::string::npos || name.find("/./")!=std::string::npos)return false;
    /* Limit the package to app assets. Never extract files from/to saved data. */
    const std::string child=name.substr(root.size());
    return child=="eboot.bin" || child=="update-helper.elf" || child.compare(0,8,"sce_sys/")==0 || child.compare(0,11,"sce_module/")==0 || child.compare(0,9,"licenses/")==0;
}
}
bool unpack(const std::string &archive,const std::string &out,std::string *err){
    /* This entry point only writes to our one owned stage, never arbitrary directories. */
    #ifdef EMBY5_TEST_STAGE
    if(out!=EMBY5_TEST_STAGE)
#else
    if(out!="/data/homebrew/PPSA99515/.emby-update-stage/unpacked")
#endif
        return error(err,"Unexpected installation staging directory");
    if(!clear_stage(out))return error(err,"Cannot clear previous update staging");
    FILE *fp=fopen(archive.c_str(),"rb");if(!fp)return error(err,"Cannot open update ZIP");
    if(fseek(fp,0,SEEK_END)!=0){fclose(fp);return error(err,"Cannot size update ZIP");}
    const long len=ftell(fp);
    if(len<22 || len>512L*1024*1024){fclose(fp);return error(err,"Invalid archive size");}
    const long back=std::min<long>(len,65557);
    std::vector<unsigned char> tail(back);
    if(fseek(fp,len-back,SEEK_SET)!=0 || fread(tail.data(),1,back,fp)!=(size_t)back){fclose(fp);return error(err,"Cannot read ZIP index");}
    long eocd=-1;
    for(long i=back-22;i>=0;--i){if(r32(tail.data()+i)==0x06054b50){eocd=i;break;}}
    if(eocd<0){fclose(fp);return error(err,"ZIP index missing");}
    const unsigned char *e=tail.data()+eocd;
    const uint16_t count=r16(e+10);
    const uint32_t dirsize=r32(e+12),diroff=r32(e+16);
    if(!count || count>256 || uint64_t(diroff)+dirsize>(uint64_t)len || r16(e+8)!=count || r16(e+4)!=0 || r16(e+6)!=0){
        fclose(fp);return error(err,"Unsupported ZIP directory");
    }
    std::vector<unsigned char> directory(dirsize);
    if(fseek(fp,diroff,SEEK_SET)!=0 || fread(directory.data(),1,dirsize,fp)!=dirsize){fclose(fp);return error(err,"Cannot read ZIP directory");}
    std::vector<File> files;
    size_t pos=0;bool has_eboot=false,has_param=false;
    uint64_t expanded=0;
    for(unsigned n=0;n<count;++n){
        if(pos+46>directory.size() || r32(directory.data()+pos)!=0x02014b50){fclose(fp);return error(err,"Invalid ZIP entry");}
        const unsigned char *h=directory.data()+pos;
        const uint16_t flags=r16(h+8),method=r16(h+10),namelen=r16(h+28),extra=r16(h+30),comment=r16(h+32);
        if(uint64_t(pos)+46+namelen+extra+comment>directory.size()){fclose(fp);return error(err,"Invalid ZIP lengths");}
        const std::string name((const char*)h+46,namelen);
        pos+=46+namelen+extra+comment;
        /* Directory entries are allowed but do not result in filesystem writes. */
        if(!name.empty() && name.back()=='/')continue;
        const uint32_t unpacked=r32(h+24),packed=r32(h+20);
        const uint32_t external=r32(h+38);
        const uint32_t unixmode=(external>>16)&0170000;
        if((flags&9)!=0 || (method!=0 && method!=8) || !valid_name(name) || unixmode==0120000 || unpacked>128u*1024*1024){
            fclose(fp);return error(err,"Unsupported or unsafe ZIP member");
        }
        expanded+=unpacked;
        if(expanded>384ull*1024*1024){fclose(fp);return error(err,"Expanded update too large");}
        for(const auto &file:files)if(file.name==name){fclose(fp);return error(err,"Duplicate ZIP filename");}
        files.push_back({name,r32(h+42),packed,unpacked,r32(h+16),method});
        if(name=="PPSA99515/eboot.bin")has_eboot=true;
        if(name=="PPSA99515/sce_sys/param.json")has_param=true;
    }
    if(!has_eboot || !has_param){fclose(fp);return error(err,"Not a complete Emby5 release");}
    if(!mkdirs(out)){fclose(fp);return error(err,"Cannot create update staging directory");}
    for(const auto &file:files){
        unsigned char hdr[30];
        if(fseek(fp,file.offset,SEEK_SET)!=0 || fread(hdr,1,30,fp)!=30 || r32(hdr)!=0x04034b50){fclose(fp);return error(err,"Invalid ZIP local header");}
        const uint64_t dataoff=uint64_t(file.offset)+30u+r16(hdr+26)+r16(hdr+28);
        if(uint64_t(dataoff)+file.packed>(uint64_t)len){fclose(fp);return error(err,"ZIP data outside archive");}
        std::vector<unsigned char> in(file.packed);
        if(fseek(fp,(long)dataoff,SEEK_SET)!=0 || fread(in.data(),1,in.size(),fp)!=in.size()){fclose(fp);return error(err,"Cannot read compressed data");}
        std::vector<unsigned char> decoded(file.unpacked ? file.unpacked : 1);
        if(file.method==0){
            if(file.packed!=file.unpacked){fclose(fp);return error(err,"ZIP size mismatch");}
            if(file.unpacked)memcpy(decoded.data(),in.data(),file.unpacked);
        }else{
            z_stream z{};
            z.next_in=in.data();z.avail_in=file.packed;
            z.next_out=decoded.data();z.avail_out=decoded.size();
            if(inflateInit2(&z,-MAX_WBITS)!=Z_OK){fclose(fp);return error(err,"Cannot initialize ZIP decoder");}
            const int result=inflate(&z,Z_FINISH);
            const bool valid=result==Z_STREAM_END && z.total_out==file.unpacked && z.total_in==file.packed;
            inflateEnd(&z);
            if(!valid){fclose(fp);return error(err,"ZIP decompression failed");}
        }
        if(crc32(0,decoded.data(),file.unpacked)!=file.crc){fclose(fp);return error(err,"ZIP CRC mismatch");}
        const std::string path=out+"/"+file.name;
        const size_t slash=path.find_last_of('/');
        if(!mkdirs(path.substr(0,slash))){fclose(fp);return error(err,"Cannot create staging folders");}
        FILE *to=fopen(path.c_str(),"wb");
        if(!to){fclose(fp);return error(err,"Cannot stage update file");}
        const bool wrote=fwrite(decoded.data(),1,file.unpacked,to)==file.unpacked && synced(to);
        fclose(to);
        if(!wrote){fclose(fp);return error(err,"Cannot save update file");}
    }
    fclose(fp);
    /* The title must be the same on an installed update. */
    const std::string param=out+"/PPSA99515/sce_sys/param.json";
    FILE *pf=fopen(param.c_str(),"rb");if(!pf)return error(err,"Release manifest missing");
    char text[65536];const size_t got=fread(text,1,sizeof(text)-1,pf);fclose(pf);text[got]=0;
    if(!strstr(text,"\"PPSA99515\""))return error(err,"Release title ID is incorrect");
    return true;
}
}
