#include "ir_util.h"
#include "json/json.h"

void append_range_bch1(Json::Value& parent, const fmap_range& range){
    parent["lower"].append(range.b.from);
    parent["lower"].append(range.c.from);
    parent["lower"].append(range.h.from);
    parent["lower"].append(0);
    parent["upper"].append(range.b.to-1);
    parent["upper"].append(range.c.to-1);
    parent["upper"].append(range.h.to-1);
    parent["upper"].append(0);
}

void append_range_bchw(Json::Value& parent, const fmap_range& range){
    parent["lower"].append(range.b.from);
    parent["lower"].append(range.c.from);
    parent["lower"].append(range.h.from);
    parent["lower"].append(range.w.from);
    parent["upper"].append(range.b.to-1);
    parent["upper"].append(range.c.to-1);
    parent["upper"].append(range.h.to-1);
    parent["upper"].append(range.w.to-1);
}
