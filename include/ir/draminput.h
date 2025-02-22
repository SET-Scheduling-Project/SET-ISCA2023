#ifndef DRAMINPUT_H
#define DRAMINPUT_H

#include "json/json.h"
#include "util.h"

#include <vector>

namespace IR{
    class DramInput{
        cidx_t core_id;
        fmap_range range;
        std::vector<tfid_t> related_ofmaps;
        tfid_t transfer_id;
        wlid_t workload_id;
    public:
        Json::Value export_json() const;
    };
}

#endif //DRAMINPUT_H
