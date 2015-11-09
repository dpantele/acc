//
// Created by dpantele on 11/9/15.
//

#include "cycles.h"

#ifndef ACC_CYCLES_EXAMPLES_H
#define ACC_CYCLES_EXAMPLES_H

namespace crag {
namespace {

using PushReadCyclesParam = std::pair<std::vector<Cycle>, Cycle::Weight>;

PushReadCyclesParam push_read_cycles_params[] = {
    {{{"xyxYXY", 1}, {"X", 0}}, 0},
    {{{"xyxYXY", 1}, {"x", 0}}, 0},
    {{{"xyxYXY", 1}, {"y", 0}}, 0},
    {{{"xxY", 1}, {"y", 0}}, 0},
    {{{"xyxYXY", 1}, {"yy", 0}}, 0},
    {{{"x", 1}, {"X", 0}}, 1},
    {{{"Yx", 1}, {"XyxxYxY", 0}, {"yX", 0}}, 1},
    {{{"yXY", 1}, {"YYXYXyyyx", 0}, {"XXY", 0}}, 1},
    {{{"yXY", 1}, {"YYXYXyyyx", 0}, {"XXY", 0}}, 1},
    {{{"yX", 1}, {"xYxY", 1}, {"XYXyX", 1}}, 3},
    {{{"YY", 1}, {"yy", 0}, {"YXY", 1}}, 1},
    {{{"xxyXYY", 1}, {"XY", 0}, {"xyxyx", 0}}, 1},
    {{{"yXyy", 0}, {"XYY", 0}, {"Xyy", 0}}, 0},
    {{{"yyxY", 0}, {"yyXy", 0}, {"yxY", 0}}, 0},
    {{{"yXyxY", 0}, {"xxY", 0}, {"yXXX", 0}}, 0},
    {{{"xyx", 0}, {"xyyyxYXY", 0}, {"XYx", 0}}, 0},
    {{{"yxYxYxy", 0}, {"YYYYxYx", 0}, {"Xyyyy", 0}}, 0},
    {{{"YxY", 0}, {"YYxy", 1}, {"YY", 0}}, 2},
    {{{"yyy", 0}, {"YYYY", 1}, {"yyy", 1}}, 1},
    {{{"yx", 1}, {"xyx", 0}}, 0},
    {{{"xYYY", 1}, {"xY", 0}, {"Xyx", 0}}, 0},
    {{{"xx", 0}, {"XyXyy", 1}, {"YX", 1}}, 8},
    {{{"x", 1}, {"X", 0}}, 1},
    {{{"Yx", 1}, {"XyxxYxY", 0}, {"yX", 0}}, 1},
    {{{"yXY", 1}, {"YYXYXyyyx", 0}, {"XXY", 0}}, 1},
    {{{"xxyXYY", 1}, {"XY", 0}, {"xyxyx", 0}}, 1},
};

}
}

#endif //ACC_CYCLES_EXAMPLES_H
