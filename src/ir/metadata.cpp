#include "ir/metadata.h"

#include <string>

Json::Value IR::DramMetaData::export_json() const{
    Json::Value ret;
    ret["x"] = dram_pos.x;
    ret["y"] = dram_pos.y;
    return ret;
}
cidx_t IR::DramMetaData::getid() const{
    return dram_id;
}
Json::Value IR::MetaData::export_json() const{
    Json::Value ret;
    for(auto &dram: drams){
        ret["DRAM"][std::to_string(dram.getid())] = dram.export_json();
    }
    ret["top_batch_cut"] = top_batch_cut;
    ret["xlen"] = xlen;
    ret["ylen"] = ylen;
    return ret;
}
