#ifndef METADATA_H
#define METADATA_H

#include "json/json.h"
#include "util.h"

#include <vector>

namespace IR{
    class DramMetaData{
        cidx_t dram_id;
        pos_t dram_pos;
    public:
        Json::Value export_json() const;
        cidx_t getid() const;
    };
    class MetaData{
        std::vector<DramMetaData> drams;
        len_t top_batch_cut;
        mlen_t xlen;
        mlen_t ylen;
    public:
        Json::Value export_json() const;
    };
}

#endif //METADATA_H
