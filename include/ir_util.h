#ifndef IR_UTIL_H
#define IR_UTIL_H

#include "util.h"

namespace Json{
    class Value;
}

void append_range_bch1(Json::Value& parent, const fmap_range& range);
void append_range_bchw(Json::Value& parent, const fmap_range& range);

#endif //IR_UTIL_H
