#include "scheme.h"

#include <string>

Json::Value IR::Scheme::export_json() const{
    Json::Value ret;
    for(auto &core: cores){
        ret[std::to_string(core.getid())] = core.export_json();
    }
    for(auto &dram: drams){
        ret["DRAM"][std::to_string(dram.getid())] = dram.export_json();
    }
    ret["metadata"] = metadata.export_json();
    return ret;
}