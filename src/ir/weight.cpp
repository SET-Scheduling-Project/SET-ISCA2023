#include "ir/weight.h"
#include "ir/irutil.h"

Json::Value IR::Weight::export_json() const{
    Json::Value ret;
    lowerupper(ret, crange);
    for(auto& transfer_id: transfer_ids){
        ret["transfer_id"].append(transfer_id);
    }
    return ret;
}