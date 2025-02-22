#include "ir/ifmapsource.h"
#include "ir/irutil.h"
#include "util.h"

Json::Value IR::IfmapSource::export_json() const{
    Json::Value ret;
    ret["channel"].append(dest_crange.from);
    ret["channel"].append(dest_crange.to-1);
    ret["id"] = id;
    ret["layer_name"] = layer_name;
    lowerupper(ret, range);
    ret["size"] = range.size()*WIDTH;
    ret["transfer_id"] = transfer_id;
    ret["type"] = type;
    return ret;
}