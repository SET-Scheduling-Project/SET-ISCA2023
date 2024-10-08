#ifndef DRAM_IR_H
#define DRAM_IR_H

#include "util.h"
#include "json/json.h"

class DRAM{
    Json::Value IR;
    std::map<Json::Value,jsonindex_t> ofmap_pos;
    std::map<Json::Value,jsonindex_t> weight_pos;
    std::map<tfid_t,jsonindex_t> ifmap_pos;
    std::vector<std::unordered_map<tfid_t,bool> > related_ofmap;

    public:
    const Json::Value& get_IR() const;
    void append_weight(const Json::Value& key, const Json::Value& weight);
    void append_weight_destination(const Json::Value& key, const Json::Value& destination);
    void append_ofmap(const Json::Value& key, const Json::Value& ofmap);
    void append_ofmap_destination(const Json::Value& key, const Json::Value& destination);
};

#endif //DRAM_IR_H
