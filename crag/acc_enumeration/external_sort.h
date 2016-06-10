//
// Created by dpantele on 5/30/16.
//

#ifndef ACC_EXTERNAL_SORT_H
#define ACC_EXTERNAL_SORT_H

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

// for now we sort only strings
void ExternalSort(const fs::path& input, const fs::path& output, const fs::path& temp_dir, const size_t kMemoryLimit);

#endif //ACC_EXTERNAL_SORT_H
