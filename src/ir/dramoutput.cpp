#include "ir/dramoutput.h"
#include "ir/irutil.h"

Json::Value IR::DramOutput::export_json() const{
    Json::Value ret;
    for(auto &destination: destinations){
        ret["destination"].append(destination.export_json());
    }
    ret["layer_name"] = layer_name;
    if(type=="weight"){
        lowerupper(ret,weight_crange);
    }
    else{
        lowerupper(ret,range);
    }
    ret["priority"] = priority;
    for(auto &related_ifmap: related_ifmaps){
        ret["related_ifmap"].append(related_ifmap);
    }
    ret["size"] = size;
    ret["transfer_id"] = transfer_id;
    ret["type"] = type;
    return ret;
}