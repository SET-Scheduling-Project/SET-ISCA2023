#include "ir/ifmap.h"
#include "ir/irutil.h"

Json::Value IR::Ifmap::export_json() const{
    Json::Value ret;
    lowerupper(ret,range);
    for(auto& transfer_id: transfer_ids){
        ret["transfer_id"].append(transfer_id);
    }
    return ret;
}