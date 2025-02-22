#ifndef IR_CORE_H
#define IR_CORE_H

#include "json/json.h"
#include "ir/workload.h"
#include "util.h"

#include <ostream>
#include <vector>

namespace IR{
    class Core{
        cidx_t core_id;
        std::vector<Workload> workloads;
    public:
        Json::Value export_json() const;
        cidx_t getid() const;
    };
}

#endif //IR_CORE_H
