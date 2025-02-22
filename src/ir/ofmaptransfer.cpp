#include "ir/irutil.h"
#include "ir/ofmaptransfer.h"
#include "util.h"

Json::Value IR::OfmapTransfer::export_json() const{
    Json::Value ret;
    for(auto &destination: destinations){
        ret["destination"].append(destination.export_json());
    }
    lowerupper(ret, range);
    ret["size"] = range.size()*WIDTH;
    ret["transfer_id"] = transfer_id;
    return ret;
}