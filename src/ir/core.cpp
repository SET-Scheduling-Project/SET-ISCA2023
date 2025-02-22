#include "ir/core.h"

Json::Value IR::Core::export_json() const{
    Json::Value ret;
    for(auto& workload: workloads){
        ret.append(workload.export_json());
    }
    return ret;
}

cidx_t IR::Core::getid() const{
    return core_id;
}
