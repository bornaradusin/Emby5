/* Incremental, bounded JSON object-array framing. SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <utility>

namespace jf {
// Frames objects without holding the entire catalogue in memory. Each framed
// object must still be validated by a JSON parser before it is published.
class JsonArrayStream {
public:
    using Item = std::function<bool(const std::string &)>;
    explicit JsonArrayStream(Item item, size_t limit = 2u << 20)
        : item_(std::move(item)), limit_(limit) {}
    bool feed(const char *data, size_t size) {
        for (size_t i=0; i<size; ++i) {
            const unsigned char c=(unsigned char)data[i];
            if (failed_) return false;
            if (bom_) {
                if ((bom_==1&&c!=0xbb) || (bom_==2&&c!=0xbf)) return fail("Invalid JSON prefix");
                if (++bom_==3) bom_=0;
                continue;
            }
            if (first_) { first_=false; if(c==0xef){bom_=1;continue;} }
            if (state_==Object) {
                if (object_.size()>=limit_) return fail("Catalogue item exceeds size limit");
                object_+=(char)c;
                if (quoted_) {
                    if (escaped_) escaped_=false;
                    else if(c=='\\') escaped_=true;
                    else if(c=='"') quoted_=false;
                    continue;
                }
                if(c=='"'){quoted_=true;continue;}
                if(c=='{'||c=='['){if(stack_.size()>=64)return fail("Catalogue JSON nesting limit");stack_+=(char)c;}
                else if(c=='}'||c==']') {
                    if(stack_.empty() || (c=='}'?stack_.back()!='{':stack_.back()!='['))return fail("Invalid catalogue JSON");
                    stack_.pop_back();
                    if(stack_.empty()) {
                        if(!item_(object_))return fail("Invalid catalogue item");
                        ++items_;object_.clear();state_=Separator;
                    }
                }
                continue;
            }
            if(c==' '||c=='\t'||c=='\r'||c=='\n')continue;
            switch(state_) {
            case Start:
                if(c!='[')return fail("Server did not return a catalogue array");
                state_=First;break;
            case First:
                if(c==']'){state_=Done;break;}
                // fall through
            case Next:
                if(c!='{')return fail("Catalogue entry is not an object");
                object_="{";stack_="{";quoted_=escaped_=false;state_=Object;break;
            case Separator:
                if(c==',')state_=Next;
                else if(c==']')state_=Done;
                else return fail("Invalid catalogue separator");
                break;
            case Done:return fail("Unexpected data after catalogue array");
            default:return fail("Invalid catalogue state");
            }
        }
        return !failed_;
    }
    bool finish() {
        return !failed_ && !bom_ && state_==Done ? true : fail("Incomplete catalogue response");
    }
    const std::string &error() const {return error_;}
    size_t items() const {return items_;}
    size_t buffered_bytes() const {return object_.size();}
private:
    enum Phase {Start,First,Next,Object,Separator,Done};
    bool fail(const char *s){if(error_.empty())error_=s;failed_=true;return false;}
    Item item_;
    size_t limit_,items_=0;
    Phase state_=Start;
    bool quoted_=false,escaped_=false,failed_=false,first_=true;
    int bom_=0;
    std::string object_,stack_,error_;
};
}
