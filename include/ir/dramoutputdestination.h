#ifndef DRAMOUTPUTDESTINATION_H
#define DRAMOUTPUTDESTINATION_H

#include "json/json.h"
#include "util.h"

#include <string>

namespace IR{
    class DramOutputDestination{
        cidx_t id;
        std::string type; //always core
        wlid_t workload_id;
    public:
        Json::Value export_json() const;
    };
}

#endif //DRAMOUTPUTDESTINATION_H
