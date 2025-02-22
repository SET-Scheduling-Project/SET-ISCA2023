#ifndef WEIGHT_H
#define WEIGHT_H

#include "json/json.h"
#include "irutil.h"

#include <vector>

namespace IR{
    class Weight{
        fmap_range::dim_range crange;
        std::vector<tfid_t> transfer_ids;
    public:
        Json::Value export_json() const;
    };
}

#endif //WEIGHT_H
