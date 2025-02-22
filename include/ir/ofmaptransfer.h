#ifndef OFMAPTRANSFER_H
#define OFMAPTRANSFER_H

#include "ir/ofmapdestination.h"
#include "json/json.h"
#include "util.h"

#include <vector>

namespace IR{
    class OfmapTransfer{
        std::vector<OfmapDestination> destinations;
        fmap_range range;
        tfid_t transfer_id;
    public:
        Json::Value export_json() const;
    };
}

#endif //OFMAPTRANSFER_H
