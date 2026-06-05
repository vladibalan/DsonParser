#include "pch.h"
#include "DsonInflate.h"

#include <algorithm>
#include <cstring>
#include <vector>

// Inflate orientation:
// Self-contained gzip decompressor used by DsonDocument loading. It implements
// enough RFC 1952/RFC 1951 to handle single-member gzip streams wrapping DSON
// JSON, including stored, fixed-Huffman, and dynamic-Huffman DEFLATE blocks.
// No third-party libraries or OS compression APIs are used.

namespace Dson {
namespace {

const int kMaxBits = 15;

const int kLengthBase[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};

const int kLengthExtra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
    2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};

const int kDistanceBase[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
    6145, 8193, 12289, 16385, 24577
};

const int kDistanceExtra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

struct BitReader {
    const unsigned char* data;
    size_t size;
    size_t pos;
    unsigned int bitbuf;
    int bitcount;

    BitReader(const unsigned char* bytes, size_t byteCount)
        : data(bytes), size(byteCount), pos(0), bitbuf(0), bitcount(0) {}

    bool ReadBits(int count, unsigned int& value) {
        value = 0;
        if (count < 0 || count > 24) {
            return false;
        }
        while (bitcount < count) {
            if (pos >= size) {
                return false;
            }
            bitbuf |= static_cast<unsigned int>(data[pos++]) << bitcount;
            bitcount += 8;
        }
        if (count == 0) {
            return true;
        }
        value = bitbuf & ((1u << count) - 1u);
        bitbuf >>= count;
        bitcount -= count;
        return true;
    }

    bool AlignToByte() {
        bitbuf = 0;
        bitcount = 0;
        return true;
    }

    bool ReadByte(unsigned int& value) {
        return ReadBits(8, value);
    }

    bool ReadLe16(unsigned int& value) {
        unsigned int lo = 0;
        unsigned int hi = 0;
        if (!ReadByte(lo) || !ReadByte(hi)) {
            return false;
        }
        value = lo | (hi << 8);
        return true;
    }
};

struct HuffmanNode {
    int child[2];
    int symbol;

    HuffmanNode() : symbol(-1) {
        child[0] = -1;
        child[1] = -1;
    }
};

struct HuffmanTree {
    std::vector<HuffmanNode> nodes;

    bool Build(const std::vector<int>& lengths, bool allowEmpty, std::string& errorMsg) {
        int counts[kMaxBits + 1] = {};
        int nextCode[kMaxBits + 1] = {};

        for (size_t i = 0; i < lengths.size(); i++) {
            int len = lengths[i];
            if (len < 0 || len > kMaxBits) {
                errorMsg = "invalid Huffman code length";
                return false;
            }
            if (len > 0) {
                counts[len]++;
            }
        }

        int left = 1;
        for (int bits = 1; bits <= kMaxBits; bits++) {
            left <<= 1;
            left -= counts[bits];
            if (left < 0) {
                errorMsg = "oversubscribed Huffman tree";
                return false;
            }
        }

        int code = 0;
        for (int bits = 1; bits <= kMaxBits; bits++) {
            code = (code + counts[bits - 1]) << 1;
            nextCode[bits] = code;
        }

        nodes.clear();
        nodes.push_back(HuffmanNode());

        for (size_t symbol = 0; symbol < lengths.size(); symbol++) {
            int len = lengths[symbol];
            if (len == 0) {
                continue;
            }

            int canonical = nextCode[len]++;
            int node = 0;
            for (int bitIndex = 0; bitIndex < len; bitIndex++) {
                int bit = (canonical >> bitIndex) & 1;
                if (nodes[node].child[bit] < 0) {
                    nodes[node].child[bit] = static_cast<int>(nodes.size());
                    nodes.push_back(HuffmanNode());
                }
                node = nodes[node].child[bit];
            }

            if (nodes[node].symbol >= 0) {
                errorMsg = "duplicate Huffman code";
                return false;
            }
            nodes[node].symbol = static_cast<int>(symbol);
        }

        if (nodes.size() == 1 && !allowEmpty) {
            errorMsg = "empty Huffman tree";
            return false;
        }
        return true;
    }

    bool Decode(BitReader& reader, int& symbol, std::string& errorMsg) const {
        if (nodes.size() == 1) {
            errorMsg = "empty Huffman tree";
            return false;
        }

        int node = 0;
        for (int depth = 0; depth <= kMaxBits; depth++) {
            if (nodes[node].symbol >= 0) {
                symbol = nodes[node].symbol;
                return true;
            }

            unsigned int bit = 0;
            if (!reader.ReadBits(1, bit)) {
                errorMsg = "truncated Huffman code";
                return false;
            }
            int next = nodes[node].child[bit ? 1 : 0];
            if (next < 0) {
                errorMsg = "invalid Huffman code";
                return false;
            }
            node = next;
        }

        errorMsg = "Huffman code too long";
        return false;
    }
};

struct Crc32Table {
    unsigned int values[256];

    Crc32Table() {
        for (unsigned int i = 0; i < 256; i++) {
            unsigned int crc = i;
            for (int j = 0; j < 8; j++) {
                crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
            }
            values[i] = crc;
        }
    }
};

static unsigned int ReadLe32(const unsigned char* data) {
    return static_cast<unsigned int>(data[0])
        | (static_cast<unsigned int>(data[1]) << 8)
        | (static_cast<unsigned int>(data[2]) << 16)
        | (static_cast<unsigned int>(data[3]) << 24);
}

static unsigned int UpdateCrc32(const unsigned char* data, size_t size) {
    static const Crc32Table table;

    unsigned int crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; i++) {
        crc = table.values[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

static bool CopyDistance(std::string& out, size_t distance, size_t length, std::string& errorMsg) {
    if (distance == 0 || distance > out.size()) {
        errorMsg = "invalid DEFLATE distance";
        return false;
    }

    size_t start = out.size() - distance;
    for (size_t i = 0; i < length; i++) {
        out.push_back(out[start + i]);
    }
    return true;
}

static bool InflateHuffmanBlock(BitReader& reader, const HuffmanTree& litLenTree,
                                const HuffmanTree& distTree, std::string& out,
                                std::string& errorMsg) {
    for (;;) {
        int symbol = 0;
        if (!litLenTree.Decode(reader, symbol, errorMsg)) {
            return false;
        }

        if (symbol < 256) {
            out.push_back(static_cast<char>(symbol));
            continue;
        }
        if (symbol == 256) {
            return true;
        }
        if (symbol < 257 || symbol > 285) {
            errorMsg = "invalid DEFLATE length symbol";
            return false;
        }

        int lengthIndex = symbol - 257;
        size_t length = static_cast<size_t>(kLengthBase[lengthIndex]);
        unsigned int extra = 0;
        if (!reader.ReadBits(kLengthExtra[lengthIndex], extra)) {
            errorMsg = "truncated DEFLATE length";
            return false;
        }
        length += extra;

        int distSymbol = 0;
        if (!distTree.Decode(reader, distSymbol, errorMsg)) {
            return false;
        }
        if (distSymbol < 0 || distSymbol >= 30) {
            errorMsg = "invalid DEFLATE distance symbol";
            return false;
        }

        size_t distance = static_cast<size_t>(kDistanceBase[distSymbol]);
        if (!reader.ReadBits(kDistanceExtra[distSymbol], extra)) {
            errorMsg = "truncated DEFLATE distance";
            return false;
        }
        distance += extra;

        if (!CopyDistance(out, distance, length, errorMsg)) {
            return false;
        }
    }
}

static bool BuildFixedTrees(HuffmanTree& litLenTree, HuffmanTree& distTree, std::string& errorMsg) {
    std::vector<int> litLenLengths(288, 0);
    for (int i = 0; i <= 143; i++) litLenLengths[i] = 8;
    for (int i = 144; i <= 255; i++) litLenLengths[i] = 9;
    for (int i = 256; i <= 279; i++) litLenLengths[i] = 7;
    for (int i = 280; i <= 287; i++) litLenLengths[i] = 8;

    std::vector<int> distLengths(32, 5);
    return litLenTree.Build(litLenLengths, false, errorMsg)
        && distTree.Build(distLengths, false, errorMsg);
}

static bool BuildDynamicTrees(BitReader& reader, HuffmanTree& litLenTree,
                              HuffmanTree& distTree, std::string& errorMsg) {
    unsigned int hlit = 0;
    unsigned int hdist = 0;
    unsigned int hclen = 0;
    if (!reader.ReadBits(5, hlit) || !reader.ReadBits(5, hdist) || !reader.ReadBits(4, hclen)) {
        errorMsg = "truncated dynamic Huffman header";
        return false;
    }

    const int codeLengthOrder[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };

    std::vector<int> codeLengthLengths(19, 0);
    for (unsigned int i = 0; i < hclen + 4; i++) {
        unsigned int len = 0;
        if (!reader.ReadBits(3, len)) {
            errorMsg = "truncated code-length alphabet";
            return false;
        }
        codeLengthLengths[codeLengthOrder[i]] = static_cast<int>(len);
    }

    HuffmanTree codeLengthTree;
    if (!codeLengthTree.Build(codeLengthLengths, false, errorMsg)) {
        return false;
    }

    size_t litLenCount = static_cast<size_t>(hlit) + 257;
    size_t distCount = static_cast<size_t>(hdist) + 1;
    std::vector<int> lengths;
    lengths.reserve(litLenCount + distCount);

    while (lengths.size() < litLenCount + distCount) {
        int symbol = 0;
        if (!codeLengthTree.Decode(reader, symbol, errorMsg)) {
            return false;
        }

        if (symbol <= 15) {
            lengths.push_back(symbol);
        } else if (symbol == 16) {
            if (lengths.empty()) {
                errorMsg = "repeat code length without previous length";
                return false;
            }
            unsigned int repeatExtra = 0;
            if (!reader.ReadBits(2, repeatExtra)) {
                errorMsg = "truncated repeated code length";
                return false;
            }
            size_t repeat = static_cast<size_t>(repeatExtra) + 3;
            if (lengths.size() + repeat > litLenCount + distCount) {
                errorMsg = "too many repeated code lengths";
                return false;
            }
            int previous = lengths.back();
            for (size_t i = 0; i < repeat; i++) {
                lengths.push_back(previous);
            }
        } else if (symbol == 17 || symbol == 18) {
            unsigned int repeatExtra = 0;
            int extraBits = (symbol == 17) ? 3 : 7;
            size_t base = (symbol == 17) ? 3 : 11;
            if (!reader.ReadBits(extraBits, repeatExtra)) {
                errorMsg = "truncated zero code-length repeat";
                return false;
            }
            size_t repeat = base + repeatExtra;
            if (lengths.size() + repeat > litLenCount + distCount) {
                errorMsg = "too many zero code lengths";
                return false;
            }
            for (size_t i = 0; i < repeat; i++) {
                lengths.push_back(0);
            }
        } else {
            errorMsg = "invalid code-length symbol";
            return false;
        }
    }

    std::vector<int> litLenLengths(lengths.begin(), lengths.begin() + litLenCount);
    std::vector<int> distLengths(lengths.begin() + litLenCount, lengths.end());
    if (litLenLengths.size() <= 256 || litLenLengths[256] == 0) {
        errorMsg = "dynamic Huffman tree missing end-of-block symbol";
        return false;
    }

    return litLenTree.Build(litLenLengths, false, errorMsg)
        && distTree.Build(distLengths, true, errorMsg);
}

static bool InflateStoredBlock(BitReader& reader, std::string& out, std::string& errorMsg) {
    reader.AlignToByte();

    unsigned int len = 0;
    unsigned int nlen = 0;
    if (!reader.ReadLe16(len) || !reader.ReadLe16(nlen)) {
        errorMsg = "truncated stored DEFLATE block";
        return false;
    }
    if (((len ^ 0xFFFFu) & 0xFFFFu) != nlen) {
        errorMsg = "invalid stored DEFLATE block length";
        return false;
    }

    for (unsigned int i = 0; i < len; i++) {
        unsigned int byteValue = 0;
        if (!reader.ReadByte(byteValue)) {
            errorMsg = "truncated stored DEFLATE data";
            return false;
        }
        out.push_back(static_cast<char>(byteValue));
    }
    return true;
}

static bool InflateDeflate(const unsigned char* data, size_t size, std::string& out, std::string& errorMsg) {
    BitReader reader(data, size);
    bool finalBlock = false;

    while (!finalBlock) {
        unsigned int bfinal = 0;
        unsigned int btype = 0;
        if (!reader.ReadBits(1, bfinal) || !reader.ReadBits(2, btype)) {
            errorMsg = "truncated DEFLATE block header";
            return false;
        }

        finalBlock = (bfinal != 0);
        if (btype == 0) {
            if (!InflateStoredBlock(reader, out, errorMsg)) {
                return false;
            }
        } else if (btype == 1 || btype == 2) {
            HuffmanTree litLenTree;
            HuffmanTree distTree;
            bool built = (btype == 1)
                ? BuildFixedTrees(litLenTree, distTree, errorMsg)
                : BuildDynamicTrees(reader, litLenTree, distTree, errorMsg);
            if (!built) {
                return false;
            }
            if (!InflateHuffmanBlock(reader, litLenTree, distTree, out, errorMsg)) {
                return false;
            }
        } else {
            errorMsg = "reserved DEFLATE block type";
            return false;
        }
    }

    return true;
}

static bool SkipZeroTerminated(const unsigned char* bytes, size_t size, size_t& pos, std::string& errorMsg) {
    while (pos < size) {
        if (bytes[pos++] == 0) {
            return true;
        }
    }
    errorMsg = "truncated gzip optional field";
    return false;
}

} // namespace

bool IsGzip(const char* data, size_t size) {
    if (!data || size < 2) {
        return false;
    }
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data);
    return bytes[0] == 0x1F && bytes[1] == 0x8B;
}

bool TryGunzip(const char* data, size_t size, std::string& out, std::string& errorMsg) {
    out.clear();
    errorMsg.clear();

    if (!IsGzip(data, size)) {
        errorMsg = "input is not gzip";
        return false;
    }
    if (size < 18) {
        errorMsg = "truncated gzip stream";
        return false;
    }

    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data);
    if (bytes[2] != 8) {
        errorMsg = "unsupported gzip compression method";
        return false;
    }

    unsigned int flags = bytes[3];
    if ((flags & 0xE0u) != 0) {
        errorMsg = "unsupported gzip flags";
        return false;
    }

    size_t pos = 10;
    if (flags & 0x04u) {
        if (pos + 2 > size) {
            errorMsg = "truncated gzip extra field length";
            return false;
        }
        unsigned int extraLen = static_cast<unsigned int>(bytes[pos])
            | (static_cast<unsigned int>(bytes[pos + 1]) << 8);
        pos += 2;
        if (pos + extraLen > size) {
            errorMsg = "truncated gzip extra field";
            return false;
        }
        pos += extraLen;
    }
    if ((flags & 0x08u) && !SkipZeroTerminated(bytes, size, pos, errorMsg)) {
        return false;
    }
    if ((flags & 0x10u) && !SkipZeroTerminated(bytes, size, pos, errorMsg)) {
        return false;
    }
    if (flags & 0x02u) {
        if (pos + 2 > size) {
            errorMsg = "truncated gzip header CRC";
            return false;
        }
        pos += 2;
    }

    if (pos + 8 > size) {
        errorMsg = "truncated gzip payload";
        return false;
    }

    size_t payloadSize = size - pos - 8;
    if (!InflateDeflate(bytes + pos, payloadSize, out, errorMsg)) {
        return false;
    }

    unsigned int expectedCrc = ReadLe32(bytes + size - 8);
    unsigned int expectedSize = ReadLe32(bytes + size - 4);
    unsigned int actualCrc = UpdateCrc32(reinterpret_cast<const unsigned char*>(out.data()), out.size());
    unsigned int actualSize = static_cast<unsigned int>(out.size() & 0xFFFFFFFFu);

    if (actualCrc != expectedCrc) {
        errorMsg = "gzip CRC32 mismatch";
        out.clear();
        return false;
    }
    if (actualSize != expectedSize) {
        errorMsg = "gzip ISIZE mismatch";
        out.clear();
        return false;
    }

    errorMsg.clear();
    return true;
}

} // namespace Dson
