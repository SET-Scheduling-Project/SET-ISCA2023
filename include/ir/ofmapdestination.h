#ifndef OFMAPDESTINATION_H
#define OFMAPDESTINATION_H

#include "json/json.h"
#include "util.h"

#include <string>
#include <vector>

namespace IR{
    class OfmapDestination{
        std::vector<cidx_t> ids;
        std::string layer_name;
        std::string type;
        wlid_t workload_id; //only type=core
    public:
        Json::Value export_json() const;
    };
}

#endif //OFMAPDESTINATION_H
