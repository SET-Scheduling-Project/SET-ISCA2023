#include "ir/draminput.h"
#include "ir/irutil.h"

Json::Value IR::DramInput::export_json() const{
    Json::Value ret;
    ret["core_id"] = core_id;
    for(auto &related_ofmap: related_ofmaps){
        ret["related_ofmap"].append(related_ofmap);
    }
    ret["transfer_id"] = transfer_id;
    ret["workload_id"] = workload_id;
    lowerupper(ret, range);
    return ret;
}