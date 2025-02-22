#ifndef IFMAP_H
#define IFMAP_H

#include "json/json.h"
#include "util.h"

#include <vector>

namespace IR{
    class Ifmap{
        fmap_range range;
        //noneed max_workload_id
        std::vector<tfid_t> transfer_ids;
    public:
        Json::Value export_json() const;
    };
}

#endif //IFMAP_H
