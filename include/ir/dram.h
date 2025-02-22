#ifndef DRAM_H
#define DRAM_H

#include "ir/draminput.h"
#include "ir/dramoutput.h"
#include "json/json.h"
#include "util.h"

#include <vector>

namespace IR{
    class Dram{
        cidx_t dram_id;
        std::vector<DramInput> inputs;
        std::vector<DramOutput> outputs;
    public:
        Json::Value export_json() const;
        cidx_t getid() const;
    };
}

#endif //DRAM_H
