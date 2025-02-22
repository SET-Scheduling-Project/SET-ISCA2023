#ifndef WEIGHTSOURCE_H
#define WEIGHTSOURCE_H

#include "json/json.h"
#include "util.h"

#include <vector>

namespace IR{
    class WeightSource{
        std::vector<cidx_t> ids;
        fmap_range::dim_range crange;
        vol_t size; // in bits
        tfid_t transfer_id;
        std::string type;
    public:
        Json::Value export_json() const;
    };
}

#endif //WEIGHTSOURCE_H
