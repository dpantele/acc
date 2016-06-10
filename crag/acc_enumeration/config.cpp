//
// Created by dpantele on 5/23/16.
//

#include "config.h"

#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/device/file.hpp>

#if defined(__unix__)

#include <sys/sysinfo.h>

static size_t GetTotalRAM() {
  struct sysinfo info;
  sysinfo(&info);

  return info.totalram;
}


#else

static size_t GetTotalRAM() {
      auto buffer = std::get_temporary_buffer<char>(64ull << 30);
      std::return_temporary_buffer(buffer.first);
      memory_limit_ = return buffer.second;
}

#endif

void Config::DumpConfig() const {
  fs::ofstream config_file(config_path_);
  if (config_file.fail()) {
    throw fmt::SystemError(errno, "Can't write to {}", config_path_);
  }
  config_file << ConfigAsString();
}

void Config::LoadFromJson(path json_path) {
  try {
    json_path = boost::filesystem::canonical(json_path);
  } catch (const boost::filesystem::filesystem_error&) {
    // file does no exist
    // try to create it
    config_path_ = json_path;
    base_dir_ = json_path.parent_path();
    if (memory_limit_ == 0) {
      // by default memory limit is a half size of get_temporary_buffer
      // but it is not more then 32Gb
      // memory limit is used only in dump processes
      memory_limit_ = GetTotalRAM() / 2;
    }
    DumpConfig();
    return;
  }
  fs::ifstream config_file(json_path);
  if (!config_file) {
    throw std::runtime_error("Can\'t open config file");
  }

  json config;
  config_file >> config;
  if (!config.count("base_dir")) {
    config["base_dir"] = json_path.parent_path().native();
  }

  config_path_ = json_path;

  LoadFromJson(config);
}

BoostFilteringIStream Config::ifstream(const path& p, std::ios_base::openmode mode) const {
  BoostFilteringIStream result;
  if (p.extension() == ".gz") {
    result.push(io::gzip_decompressor());
    result.push(io::file_source(p.c_str(), mode | std::ios_base::binary));
  } else {
    result.push(io::file_source(p.c_str(), mode));
  }
  assert(result.size() >= 1);
  if (!result.component<io::file_source>((int) (result.size() - 1))->is_open()) {
    result.clear(std::ios_base::failbit);
  }
  return result;
}

BoostFilteringOStream Config::ofstream(const path& p, std::ios_base::openmode mode) const {
  BoostFilteringOStream result;
  if (p.extension() == ".gz") {
    result.push(io::gzip_compressor(9));
    result.push(io::file_sink(p.c_str(), mode | std::ios_base::binary));
  } else {
    result.push(io::file_sink(p.c_str(), mode));
  }
  assert(result.size() >= 1);
  if (!result.component<io::file_sink>((int) (result.size() - 1))->is_open()) {
    result.clear(std::ios_base::failbit);
  }
  return result;
}
