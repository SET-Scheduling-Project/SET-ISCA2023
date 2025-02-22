#ifndef DRAMOUTPUT_H
#define DRAMOUTPUT_H

#include "ir/dramoutputdestination.h"
#include "json/json.h"
#include "util.h"

#include <string>
#include <vector>

namespace IR{
    class DramOutput{
        std::vector<DramOutputDestination> destinations;
        std::string layer_name;
        fmap_range range;
        fmap_range::dim_range weight_crange;
        tfid_t priority;
        std::vector<tfid_t> related_ifmaps;
        vol_t size; // in bits
        tfid_t transfer_id;
        std::string type;
    public:
        Json::Value export_json() const;
    };
}

#endif //DRAMOUTPUT_H
