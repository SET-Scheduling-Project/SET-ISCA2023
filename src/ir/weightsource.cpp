#include "ir/irutil.h"
#include "ir/weightsource.h"

Json::Value IR::WeightSource::export_json() const{
    Json::Value ret;
    for(auto& id: ids){
        ret["id"].append(id);
    }
    lowerupper(ret, crange);
    ret["size"] = size;
    ret["transfer_id"] = transfer_id;
    ret["type"] = type;
    return ret;
}