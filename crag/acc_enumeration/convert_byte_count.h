//
// Created by dpantele on 6/3/16.
//

#ifndef ACC_CONVERT_BYTE_COUNT_H
#define ACC_CONVERT_BYTE_COUNT_H

#include <string>

std::string ToHumanReadableByteCount(size_t);
size_t FromHumanReadableByteCount(const std::string&);

#endif //ACC_CONVERT_BYTE_COUNT_H
