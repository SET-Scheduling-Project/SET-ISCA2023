#include "dram_ir.h"

const Json::Value& DRAM::get_IR() const{
    return IR;
}

void DRAM::append_weight(const Json::Value& key, const Json::Value& weight){
    weight_pos[key]=IR["out"].size();
    IR["out"].append(weight);
}

void DRAM::append_weight_destination(const Json::Value& key, const Json::Value& destination){
    IR["out"][weight_pos[key]]["destination"].append(destination);
}

void DRAM::append_ofmap(const Json::Value& key, const Json::Value& ofmap){
    ofmap_pos[key]=IR["out"].size();
    IR["out"].append(ofmap);
}

void DRAM::append_ofmap_destination(const Json::Value& key, const Json::Value& destination){
    IR["out"][ofmap_pos[key]]["destination"].append(destination);
}
