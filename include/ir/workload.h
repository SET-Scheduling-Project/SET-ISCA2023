#ifndef WORKLOAD_H
#define WORKLOAD_H

#include "ir/dataonbuffer.h"
#include "ir/ifmap.h"
#include "ir/weight.h"
#include "json/json.h"
#include "util.h"

#include <array>
#include <string>
#include <vector>

namespace IR{
    class Workload{
        std::vector<DataOnBuffer> buffer;
        Ifmap ifmap;
        std::string layer_name;
        std::string layer_type;
        // noneed ofmap_size
        cycle_t time;
        Weight weight;
        fmap_range workload;
        wlid_t workload_id;
    public:
        Json::Value export_json() const;
    };
}

#endif //WORKLOAD_H
