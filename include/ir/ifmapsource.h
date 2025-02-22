#ifndef IFMAPSOURCE_H
#define IFMAPSOURCE_H

#include "json/json.h"
#include "util.h"

#include <string>

namespace IR{
    class IfmapSource{
        fmap_range::dim_range dest_crange;
        cidx_t id;
        std::string layer_name;
        fmap_range range;
        tfid_t transfer_id;
        std::string type;
    public:
        Json::Value export_json() const;
    };
}

#endif //IFMAPSOURCE_H
