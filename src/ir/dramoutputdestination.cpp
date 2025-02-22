#include "ir/dramoutputdestination.h"

Json::Value IR::DramOutputDestination::export_json() const{
    Json::Value ret;
    ret["id"] = id;
    ret["type"] =type;
    ret["workload_id"] = workload_id;
    return ret;
}