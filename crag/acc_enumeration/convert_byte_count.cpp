//
// Created by dpantele on 6/3/16.
//

#include "convert_byte_count.h"

#include <algorithm>
#include <cmath>

#include <fmt/format.h>
#include <cstdlib>
#include <cerrno>

// @dpantele: code below is adapted from
// http://agentzlerich.blogspot.com/2011/01/converting-to-and-from-human-readable.html

// Adapted from http://stackoverflow.com/questions/3758606/
// how-to-convert-byte-size-into-human-readable-format-in-java
void to_human_readable_byte_count(long bytes,
    int si,
    double *coeff,
    const char **units)
{
  // Static lookup table of byte-based SI units
  static const char *suffix[][2] = {
      { "B",  "B"   },
      { "KB", "KiB" },
      { "MB", "MiB" },
      { "GB", "GiB" },
      { "TB", "TiB" },
      { "EB", "EiB" },
      { "ZB", "ZiB" },
      { "YB", "YiB" } };
  int unit = si ? 1000 : 1024;
  int exp = 0;
  if (bytes > 0) {
    exp = std::min( (int) (log(bytes) / log(unit)),
        (int) (sizeof(suffix) / sizeof(suffix[0])) - 1);
  }
  *coeff = bytes / pow(unit, exp);
  *units = suffix[exp][si != 0];
}

// Convert strings like the following into byte counts
//    5MB, 5 MB, 5M, 3.7GB, 123b, 456kB
// with some amount of forgiveness baked into the parsing.
long from_human_readable_byte_count(const char *str)
{
  // Parse leading numeric factor
  char *endptr;
  errno = 0;
  const double coeff = std::strtod(str, &endptr);
  if (errno) {
    return -1;
  }

  // Skip any intermediate white space
  while (isspace(*endptr)) ++endptr;

  // Read off first character which should be an SI prefix
  int exp  = 0;
  int unit = 1024;

  assert(*endptr != '\t' && *endptr != ' ');

  switch (toupper(*endptr)) {
    case 'B':  exp =  0; break;
    case 'K':  exp =  3; break;
    case 'M':  exp =  6; break;
    case 'G':  exp =  9; break;
    case 'T':  exp = 12; break;
    case 'E':  exp = 15; break;
    case 'Z':  exp = 18; break;
    case 'Y':  exp = 21; break;

    case '\0': exp =  0; break;

    default:   return -1;
  }

  if (*endptr != '\0') {
    ++endptr;
  }

  // If an 'i' or 'I' is present use SI factor-of-1000 units
  if (toupper(*endptr) == 'I') {
    ++endptr;
    unit = 1000;
  }

  // Ignore B after that
  if (toupper(*endptr) == 'B') {
    ++endptr;
  }

  // Skip any remaining white space
  while (isspace(*endptr)) ++endptr;

  // Parse error on anything but a null terminator
  if (*endptr) {
    return -1;
  }

  return static_cast<long>(coeff * pow(unit, exp / 3));
}

std::string ToHumanReadableByteCount(size_t s) {
  double coeff;
  const char* suffix;
  to_human_readable_byte_count(s, false, &coeff, &suffix);
  auto int_coeff = static_cast<int>(std::ceil(coeff * 100));
  double rounded = double(int_coeff) / 100;
  auto precision = 2;
  if (int_coeff % 10 == 0) {
    --precision;
  }

  if (int_coeff % 100 == 0) {
    --precision;
  }

  return fmt::format("{0:.{1}f}{2}", rounded, precision, suffix);
}

size_t FromHumanReadableByteCount(const std::string& str) {
  auto result = from_human_readable_byte_count(str.c_str());
  if (result < 0) {
    throw std::runtime_error(fmt::format("'{}' can't be parsed as byte count", str));
  }

  return static_cast<size_t>(result);
}



