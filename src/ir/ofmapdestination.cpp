#include "ir/ofmapdestination.h"

Json::Value IR::OfmapDestination::export_json() const{
    Json::Value ret;
    for(auto& id: ids){
        ret["id"].append(id);
    }
    ret["layer_name"] = layer_name;
    ret["type"] = type;
    return ret;
}