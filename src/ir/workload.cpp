#include "ir/irutil.h"
#include "ir/workload.h"
#include "util.h"

Json::Value IR::Workload::export_json() const{
    Json::Value ret;
    for(auto& data: buffer){
        ret["buffer"].append(data.export_json());
    }
    ret["ifmap"] = ifmap.export_json();
    ret["layer_name"] = layer_name;
    ret["layer_type"] = layer_type;
    ret["ofmap_size"] = workload.size()*WIDTH;
    ret["time"] = time;
    ret["weight"] = weight.export_json();
    ret["workload"] = array_range(workload);
    ret["workload_id"] = workload_id;
    return ret;
}