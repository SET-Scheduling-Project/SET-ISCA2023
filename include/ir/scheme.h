#ifndef SCHEME_H
#define SCHEME_H

#include "ir/core.h"
#include "ir/dram.h"
#include "ir/metadata.h"
#include "json/json.h"
#include "util.h"

#include <vector>

namespace IR{
    class Scheme{
        std::vector<Core> cores;
        std::vector<Dram> drams;
        MetaData metadata;
    public:
        Json::Value export_json() const;
    };
}

#endif //SCHEME_H
