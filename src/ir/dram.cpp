#include "ir/dram.h"

Json::Value IR::Dram::export_json() const{
    Json::Value ret;
    for(auto &input: inputs){
        ret["in"].append(input.export_json());
    }
    for(auto &output: outputs){
        ret["out"].append(output.export_json());
    }
    return ret;
}

cidx_t IR::Dram::getid() const{
    return dram_id;
}
