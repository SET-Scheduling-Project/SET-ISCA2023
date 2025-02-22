#ifndef IR_UTIL_H
#define IR_UTIL_H

#include "json/json.h"
#include "util.h"

namespace IR{
    void lowerupper(Json::Value& parent, const fmap_range& range);
    void lowerupper(Json::Value& parent, const fmap_range::dim_range& crange);
    Json::Value array_range(const fmap_range &range);
}

#endif //IR_UTIL_H
