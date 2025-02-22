#ifndef DATAONBUFFER_H
#define DATAONBUFFER_H

#include "ifmapsource.h"
#include "json/json.h"
#include "ofmaptransfer.h"
#include "util.h"
#include "weightsource.h"

#include <string>
#include <vector>

namespace IR{
    class DataOnBuffer{
        vol_t address;
        vol_t block;
        std::string layer_name;
        tfid_t priority;
        std::string type;
        wlid_t workload_id;
        struct IfmapExtra{
            bool start_loading;
            bool end_loading;
            fmap_range range;
            std::vector<IfmapSource> sources;
        };
        struct WeightExtra{
            bool start_loading;
            bool end_loading;
            fmap_range::dim_range crange;
            std::vector<WeightSource> sources;
        };
        struct OfmapExtra{
            bool start_sending;
            bool end_sending;
            fmap_range range;
            std::vector<OfmapTransfer> transfers;
        };
        union Extra{
            IfmapExtra ifmapextra;
            WeightExtra weightextra;
            OfmapExtra ofmapextra;
        };
        Extra extra;
    public:
        Json::Value export_json() const;
    };
}

#endif //DATAONBUFFER_H
