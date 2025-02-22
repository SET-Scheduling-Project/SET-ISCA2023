#include "ir/irutil.h"
#include "irutil.h"

void IR::lowerupper(Json::Value& parent, const fmap_range &range){
    parent["lower"].append(range.b.from);
    parent["lower"].append(range.c.from);
    parent["lower"].append(range.h.from);
    parent["lower"].append(range.w.from);
    parent["upper"].append(range.b.to-1);
    parent["upper"].append(range.c.to-1);
    parent["upper"].append(range.h.to-1);
    parent["upper"].append(range.w.to-1);
}
void IR::lowerupper(Json::Value& parent, const fmap_range::dim_range &crange){
    parent["lower"] = crange.from;
    parent["upper"] = crange.to-1;
}

Json::Value IR::array_range(const fmap_range& range){
    Json::Value lower,upper;
    lower.append(range.b.from);
    lower.append(range.c.from);
    lower.append(range.h.from);
    lower.append(range.w.from);
    upper.append(range.b.to-1);
    upper.append(range.c.to-1);
    upper.append(range.h.to-1);
    upper.append(range.w.to-1);
    Json::Value ret;
    ret.append(lower);
    ret.append(upper);
    return ret;
}
