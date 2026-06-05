#pragma once

#include <cstddef>
#include <string>

// Inflate orientation:
// Internal gzip/DEFLATE support for DsonDocument loading. This module is not
// part of the public C ABI; it detects gzip-wrapped DAZ JSON files and inflates
// one gzip member into an in-memory byte string before RapidJSON parses it.
//
// Responsibilities:
// - Parse RFC 1952 gzip framing for a single member.
// - Inflate RFC 1951 stored, fixed-Huffman, and dynamic-Huffman blocks.
// - Verify mandatory gzip CRC32 and ISIZE trailer fields.
// - Fail safely with an error string on malformed, truncated, or unsupported
//   input; never read beyond the supplied buffer.

namespace Dson {

bool IsGzip(const char* data, size_t size);
bool TryGunzip(const char* data, size_t size, std::string& out, std::string& errorMsg);

} // namespace Dson
