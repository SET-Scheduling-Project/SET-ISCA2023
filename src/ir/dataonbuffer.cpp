#include "ir/dataonbuffer.h"
#include "ir/irutil.h"

Json::Value IR::DataOnBuffer::export_json() const{
    Json::Value ret;
    ret["address"] = address;
    ret["block"] = block;
    ret["layer"] = layer_name;
    ret["priority"] = priority;
    ret["type"] = type;
    if(type == "ifmap"){
        const auto &ex = extra.ifmapextra;
        for(auto &source: ex.sources){
            ret["source"].append(source.export_json());
        }
        ret["start_loading"] = ex.start_loading;
        ret["end_loading"] = ex.end_loading;
        lowerupper(ret, ex.range);
    }
    if(type == "weight"){
        const auto &ex = extra.weightextra;
        for(auto &source: ex.sources){
            ret["source"].append(source.export_json());
        }
        ret["start_loading"] = ex.start_loading;
        ret["end_loading"] = ex.end_loading;
        lowerupper(ret, ex.crange);
    }
    if(type == "ofmap"){
        const auto &ex = extra.ofmapextra;
        for(auto &transfer: ex.transfers){
            ret["destinations"].append(transfer.export_json());
        }
        ret["start_sending"] = ex.start_sending;
        ret["end_sending"] = ex.end_sending;
        lowerupper(ret, ex.range);
    }
    return ret;
}