// Test harness orientation:
// Console program that exercises the DsonParser C ABI end to end: loads a DSON
// file via DsonParserAPI.h, then queries nodes, geometry, materials, skin, and
// morphs to sanity-check the parser. Links DsonParser.lib. Most execution is a
// manual smoke test/example consumer; selected in-memory regressions also have
// deterministic command-line modes that bypass the final keypress.
//

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include <thread>
#define NOMINMAX
#include <Windows.h>
#include "../DsonParser/DsonParserAPI.h"

#pragma comment(lib, "DsonParser.lib")

// gzip of:
//   {"asset_info":{"id":"/data/test/gzip_fixture.dsf","type":"figure"},"file_version":"0.6.0.0"}
static const unsigned char kGzipFixture[] = {
    0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x0d, 0x8a, 0x4b, 0x0a,
    0xc0, 0x20, 0x0c, 0x05, 0xef, 0xf2, 0xd6, 0x12, 0x5d, 0x75, 0xe1, 0x65, 0x44, 0x30,
    0x96, 0x40, 0xb1, 0xc5, 0xa4, 0xa5, 0x1f, 0xbc, 0x7b, 0xdd, 0x0d, 0x33, 0xf3, 0x21,
    0xab, 0xb2, 0x25, 0x69, 0x75, 0x47, 0xfc, 0x20, 0x05, 0x11, 0xbe, 0x64, 0xcb, 0xde,
    0x58, 0xcd, 0xaf, 0xaf, 0x1c, 0xa9, 0xca, 0x6d, 0x67, 0x67, 0x2a, 0x5a, 0xe1, 0x60,
    0xcf, 0xc1, 0x73, 0xaa, 0xb2, 0x4e, 0x87, 0xe1, 0x26, 0x6d, 0x9c, 0x2e, 0xee, 0x2a,
    0x7b, 0x9b, 0x21, 0xd0, 0x42, 0x81, 0x02, 0xc6, 0x0f, 0x69, 0x2a, 0x37, 0x53, 0x5c,
    0x00, 0x00, 0x00
};

static const char kGzipFixtureJson[] =
    "{\"asset_info\":{\"id\":\"/data/test/gzip_fixture.dsf\",\"type\":\"figure\"},"
    "\"file_version\":\"0.6.0.0\"}";

// Same as kGzipFixture but CRC32 byte at offset 93 corrupted (0x69 -> 0x68).
// Valid DEFLATE + valid length; ONLY the CRC32 is wrong. Must be rejected.
static const unsigned char kGzipFixtureBadCrc[] = {
    0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x0d, 0x8a, 0x4b, 0x0a,
    0xc0, 0x20, 0x0c, 0x05, 0xef, 0xf2, 0xd6, 0x12, 0x5d, 0x75, 0xe1, 0x65, 0x44, 0x30,
    0x96, 0x40, 0xb1, 0xc5, 0xa4, 0xa5, 0x1f, 0xbc, 0x7b, 0xdd, 0x0d, 0x33, 0xf3, 0x21,
    0xab, 0xb2, 0x25, 0x69, 0x75, 0x47, 0xfc, 0x20, 0x05, 0x11, 0xbe, 0x64, 0xcb, 0xde,
    0x58, 0xcd, 0xaf, 0xaf, 0x1c, 0xa9, 0xca, 0x6d, 0x67, 0x67, 0x2a, 0x5a, 0xe1, 0x60,
    0xcf, 0xc1, 0x73, 0xaa, 0xb2, 0x4e, 0x87, 0xe1, 0x26, 0x6d, 0x9c, 0x2e, 0xee, 0x2a,
    0x7b, 0x9b, 0x21, 0xd0, 0x42, 0x81, 0x02, 0xc6, 0x0f, 0x68, 0x2a, 0x37, 0x53, 0x5c,
    0x00, 0x00, 0x00
};

std::string GetWorkingDirectory() {
    char buffer[MAX_PATH];
    DWORD len = GetCurrentDirectoryA(MAX_PATH, buffer);
    if (len == 0) return "";
    return std::string(buffer, len);
}

bool FileExists(const char* filepath) {
    DWORD attrib = GetFileAttributesA(filepath);
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

// Resolve a file path for loading. Accepts either:
//   - an absolute path (e.g. D:/Content/file.duf)
//   - a filename that lives inside DsonTest2/TestFiles
// Depending on how the program is launched the working directory is one of:
//   repo/solution root (e.g. msbuild launch), project dir, or build output dir
// (x64/Debug). Try all three TestFiles locations. The direct/absolute path is
// always tried first.
std::string ResolveTestFile(const std::string& name) {
    const std::string candidates[] = {
        name,                                      // direct or absolute path
        "TestFiles/" + name,                       // cwd = project dir
        "DsonTest2/TestFiles/" + name,             // cwd = repo / solution root
        "../../DsonTest2/TestFiles/" + name        // cwd = x64/<Config>
    };
    for (const std::string& path : candidates) {
        if (FileExists(path.c_str())) {
            return path;
        }
    }
    return std::string();
}

void PrintGzipTestResult(const char* name, bool pass) {
    std::cout << "gzip " << name << ": " << (pass ? "PASS" : "FAIL");
    if (!pass) {
        std::cout << " (" << DsonParser_GetLastError() << ")";
    }
    std::cout << "\n";
}

void RunGzipFixtureTests() {
    std::cout << "=================================\n";
    std::cout << "GZIP LOAD TESTS\n";
    std::cout << "=================================\n\n";

    DsonDocumentHandle gzipDoc = DsonDocument_Create();
    DsonDocumentHandle plainDoc = DsonDocument_Create();
    bool happyPass = false;
    if (gzipDoc && plainDoc) {
        int gzipResult = DsonDocument_LoadFromBuffer(
            gzipDoc, reinterpret_cast<const char*>(kGzipFixture), static_cast<int>(sizeof(kGzipFixture)));
        int plainResult = DsonDocument_LoadFromString(plainDoc, kGzipFixtureJson);
        const char* gzipId = DsonDocument_GetAssetId(gzipDoc);
        const char* plainId = DsonDocument_GetAssetId(plainDoc);
        happyPass = gzipResult == 0
            && plainResult == 0
            && std::strcmp(gzipId, "/data/test/gzip_fixture.dsf") == 0
            && std::strcmp(plainId, "/data/test/gzip_fixture.dsf") == 0
            && std::strcmp(gzipId, plainId) == 0;
    }
    PrintGzipTestResult("happy path", happyPass);
    if (gzipDoc) DsonDocument_Destroy(gzipDoc);
    if (plainDoc) DsonDocument_Destroy(plainDoc);

    DsonDocumentHandle crcDoc = DsonDocument_Create();
    int crcResult = crcDoc
        ? DsonDocument_LoadFromBuffer(
            crcDoc, reinterpret_cast<const char*>(kGzipFixtureBadCrc), static_cast<int>(sizeof(kGzipFixtureBadCrc)))
        : 1;
    PrintGzipTestResult("CRC rejection", crcResult != 0);
    if (crcDoc) DsonDocument_Destroy(crcDoc);

    std::vector<unsigned char> corrupted(kGzipFixture, kGzipFixture + sizeof(kGzipFixture));
    corrupted[40] ^= 0x01;
    DsonDocumentHandle corruptDoc = DsonDocument_Create();
    int corruptResult = corruptDoc
        ? DsonDocument_LoadFromBuffer(
            corruptDoc, reinterpret_cast<const char*>(corrupted.data()), static_cast<int>(corrupted.size()))
        : 1;
    PrintGzipTestResult("body corruption rejection", corruptResult != 0);
    if (corruptDoc) DsonDocument_Destroy(corruptDoc);

    // Blank (all-zero) trailer on a clean DEFLATE stream must now be ACCEPTED.
    std::vector<unsigned char> blankFooter(kGzipFixture, kGzipFixture + sizeof(kGzipFixture));
    for (size_t i = blankFooter.size() - 8; i < blankFooter.size(); ++i) {
        blankFooter[i] = 0x00;
    }
    DsonDocumentHandle blankDoc = DsonDocument_Create();
    bool blankPass = false;
    if (blankDoc) {
        int blankResult = DsonDocument_LoadFromBuffer(
            blankDoc, reinterpret_cast<const char*>(blankFooter.data()),
            static_cast<int>(blankFooter.size()));
        const char* blankId = DsonDocument_GetAssetId(blankDoc);
        blankPass = blankResult == 0
            && std::strcmp(blankId, "/data/test/gzip_fixture.dsf") == 0;
    }
    PrintGzipTestResult("blank footer acceptance", blankPass);
    if (blankDoc) DsonDocument_Destroy(blankDoc);

    // A blank trailer must NOT rescue a genuinely truncated DEFLATE stream:
    // header + only the first half of the payload + a zero trailer. Inflate
    // can't terminate, so it fails before the trailer check.
    size_t payloadLen = sizeof(kGzipFixture) - 10 - 8;
    std::vector<unsigned char> truncated(kGzipFixture, kGzipFixture + 10 + payloadLen / 2);
    truncated.insert(truncated.end(), 8, 0x00);
    DsonDocumentHandle truncDoc = DsonDocument_Create();
    int truncResult = truncDoc
        ? DsonDocument_LoadFromBuffer(
            truncDoc, reinterpret_cast<const char*>(truncated.data()),
            static_cast<int>(truncated.size()))
        : 1;
    PrintGzipTestResult("blank footer + truncated DEFLATE rejection", truncResult != 0);
    if (truncDoc) DsonDocument_Destroy(truncDoc);

    std::cout << "\n";
}

// Verifies image map_size parsing and the image accessor family against a known
// fixture (HID_Nancy_9.duf): the g09_Nancy_head_* image_library entries carry
// map_size [4096, 4096]. Also confirms the out-of-range numeric sentinel (0).
void RunImageMapSizeTest() {
    std::cout << "=================================\n";
    std::cout << "IMAGE map_size TEST\n";
    std::cout << "=================================\n\n";

    const std::string filepath = ResolveTestFile("HID_Nancy_9.duf");
    if (filepath.empty()) {
        std::cout << "HID_Nancy_9.duf not found (tried direct path and TestFiles); skipping.\n\n";
        return;
    }

    DsonDocumentHandle doc = DsonDocument_Create();
    if (!doc) {
        std::cout << "Failed to create document; skipping.\n\n";
        return;
    }

    if (DsonDocument_LoadFromFile(doc, filepath.c_str()) != 0) {
        std::cout << "Error loading HID_Nancy_9.duf: " << DsonParser_GetLastError() << "\n\n";
        DsonDocument_Destroy(doc);
        return;
    }

    const char* prefix = "g09_Nancy_head_";
    int imgCount = DsonDocument_GetImageCount(doc);
    std::cout << "Images (" << imgCount << "), \"" << prefix << "\" entries (expect 4096 x 4096):\n";
    for (int i = 0; i < imgCount; i++) {
        const char* id = DsonDocument_GetImageId(doc, i);
        if (std::strncmp(id, prefix, std::strlen(prefix)) == 0) {
            std::cout << "  [" << i << "] " << id << ": "
                      << DsonDocument_GetImageMapWidth(doc, i) << " x "
                      << DsonDocument_GetImageMapHeight(doc, i) << "\n";
        }
    }

    // Out-of-range index: numeric getters must return the 0 sentinel (never -1).
    std::cout << "Out-of-range [" << imgCount << "] map size: "
              << DsonDocument_GetImageMapWidth(doc, imgCount) << " x "
              << DsonDocument_GetImageMapHeight(doc, imgCount) << " (expect 0 x 0)\n\n";

    DsonDocument_Destroy(doc);
}

// Verifies the library self-identifies over the C ABI: DsonParser_GetVersion() must
// be non-empty and agree with the compile-time DSONPARSER_VERSION_STRING macro (the
// runtime carrier vs. the header carrier). At the 1.0.0 baseline this is "1.0.0".
void RunVersionTest() {
    std::cout << "=================================\n";
    std::cout << "LIBRARY VERSION TEST\n";
    std::cout << "=================================\n\n";

    const char* version = DsonParser_GetVersion();
    bool pass = version != nullptr
        && version[0] != '\0'
        && std::strcmp(version, DSONPARSER_VERSION_STRING) == 0;
    std::cout << "DsonParser_GetVersion(): \"" << (version ? version : "")
              << "\" (expect \"" << DSONPARSER_VERSION_STRING << "\")\n";
    std::cout << "version check: " << (pass ? "PASS" : "FAIL") << "\n\n";
}

// Verifies the per-image LIE compositing metadata accessors (1.4.0) against
// HID_Nancy_9.duf: finds the two 4-layer head stacks by layer count and confirms
// blend ops, opacity, transforms, and color per the task spec.
void RunImageLayerCompositingTest() {
    std::cout << "=================================\n";
    std::cout << "IMAGE LAYER COMPOSITING TEST (1.4.0)\n";
    std::cout << "=================================\n\n";

    const std::string filepath = ResolveTestFile("HID_Nancy_9.duf");
    if (filepath.empty()) {
        std::cout << "HID_Nancy_9.duf not found; skipping.\n\n";
        return;
    }

    DsonDocumentHandle doc = DsonDocument_Create();
    if (!doc) { std::cout << "Failed to create document; skipping.\n\n"; return; }

    if (DsonDocument_LoadFromFile(doc, filepath.c_str()) != 0) {
        std::cout << "Error loading HID_Nancy_9.duf: " << DsonParser_GetLastError() << "\n\n";
        DsonDocument_Destroy(doc);
        return;
    }

    // Find the two 4-layer stacks by path suffix.
    int imgCount = DsonDocument_GetImageCount(doc);
    int diffuseIdx = -1, sssIdx = -1;
    for (int i = 0; i < imgCount; i++) {
        if (DsonDocument_GetImageLayerCount(doc, i) != 4) continue;
        const char* path = DsonDocument_GetImageLayerTexturePath(doc, i, 0);
        std::string p(path ? path : "");
        if (p.find("head_base.jpg") != std::string::npos && diffuseIdx < 0) diffuseIdx = i;
        if (p.find("head_sss.jpg")  != std::string::npos && sssIdx    < 0) sssIdx    = i;
    }
    std::cout << "Diffuse 4-layer index: " << diffuseIdx
              << ", SSS 4-layer index: "   << sssIdx << "\n";

    bool pass = true;

    // --- Diffuse stack ---
    if (diffuseIdx >= 0) {
        // L0: identity blend op, opacity 1, active true, all transforms 0/1, color 0
        bool l0 = std::strcmp(DsonDocument_GetImageLayerBlendMode(doc, diffuseIdx, 0), "blend_source_over") == 0
               && std::fabs(DsonDocument_GetImageLayerOpacity(doc, diffuseIdx, 0) - 1.0) < 1e-9
               && DsonDocument_GetImageLayerActive(doc, diffuseIdx, 0) == true
               && DsonDocument_GetImageLayerInvert(doc, diffuseIdx, 0) == false
               && std::fabs(DsonDocument_GetImageLayerOffsetX(doc, diffuseIdx, 0)) < 1e-9
               && std::fabs(DsonDocument_GetImageLayerOffsetY(doc, diffuseIdx, 0)) < 1e-9
               && std::fabs(DsonDocument_GetImageLayerScaleX(doc, diffuseIdx, 0) - 1.0) < 1e-9
               && std::fabs(DsonDocument_GetImageLayerScaleY(doc, diffuseIdx, 0) - 1.0) < 1e-9
               && std::fabs(DsonDocument_GetImageLayerRotation(doc, diffuseIdx, 0)) < 1e-9
               && DsonDocument_GetImageLayerMirrorX(doc, diffuseIdx, 0) == false
               && DsonDocument_GetImageLayerMirrorY(doc, diffuseIdx, 0) == false
               && std::fabs(DsonDocument_GetImageLayerColorR(doc, diffuseIdx, 0)) < 1e-9
               && std::fabs(DsonDocument_GetImageLayerColorG(doc, diffuseIdx, 0)) < 1e-9
               && std::fabs(DsonDocument_GetImageLayerColorB(doc, diffuseIdx, 0)) < 1e-9;
        std::cout << "Diffuse L0 (identity blend_source_over): " << (l0 ? "PASS" : "FAIL") << "\n";
        pass = pass && l0;

        // L1: label contains brows_base, blend_multiply, opacity 1
        bool l1 = std::strcmp(DsonDocument_GetImageLayerBlendMode(doc, diffuseIdx, 1), "blend_multiply") == 0
               && std::fabs(DsonDocument_GetImageLayerOpacity(doc, diffuseIdx, 1) - 1.0) < 1e-9;
        const char* l1label = DsonDocument_GetImageLayerLabel(doc, diffuseIdx, 1);
        std::cout << "Diffuse L1 label: \"" << (l1label ? l1label : "") << "\"\n";
        std::cout << "Diffuse L1 (blend_multiply, opacity 1): " << (l1 ? "PASS" : "FAIL") << "\n";
        pass = pass && l1;

        // L2/L3: blend_source_over, opacity 1
        for (int li = 2; li <= 3; li++) {
            bool ln = std::strcmp(DsonDocument_GetImageLayerBlendMode(doc, diffuseIdx, li), "blend_source_over") == 0
                   && std::fabs(DsonDocument_GetImageLayerOpacity(doc, diffuseIdx, li) - 1.0) < 1e-9;
            std::cout << "Diffuse L" << li << " (blend_source_over, opacity 1): " << (ln ? "PASS" : "FAIL") << "\n";
            pass = pass && ln;
        }
    } else {
        std::cout << "Diffuse 4-layer stack not found: FAIL\n";
        pass = false;
    }

    // --- SSS stack ---
    if (sssIdx >= 0) {
        // L0: blend_source_over, opacity 1
        bool s0 = std::strcmp(DsonDocument_GetImageLayerBlendMode(doc, sssIdx, 0), "blend_source_over") == 0
               && std::fabs(DsonDocument_GetImageLayerOpacity(doc, sssIdx, 0) - 1.0) < 1e-9;
        std::cout << "SSS L0 (blend_source_over, opacity 1): " << (s0 ? "PASS" : "FAIL") << "\n";
        pass = pass && s0;

        // L1..L3: opacity 0.5
        for (int li = 1; li <= 3; li++) {
            bool sn = std::fabs(DsonDocument_GetImageLayerOpacity(doc, sssIdx, li) - 0.5) < 1e-9;
            std::cout << "SSS L" << li << " (opacity 0.5): " << (sn ? "PASS" : "FAIL") << "\n";
            pass = pass && sn;
        }
    } else {
        std::cout << "SSS 4-layer stack not found: FAIL\n";
        pass = false;
    }

    std::cout << "\nPer-image compositing test overall: " << (pass ? "PASS" : "FAIL") << "\n\n";
    DsonDocument_Destroy(doc);
}

// Verifies the per-channel LIE compositing accessors (1.4.0) with a crafted
// inline DSON using an identity-matched "#lie0" image reference so layers copy
// onto the channel, then asserts all 14 new fields on both layers.
void RunChannelLayerCompositingTest() {
    std::cout << "=================================\n";
    std::cout << "CHANNEL LAYER COMPOSITING TEST (1.4.0)\n";
    std::cout << "=================================\n\n";

    // Minimal DSON: image_library entry "lie0" with 2-element LIE map; scene
    // material diffuse channel references "#lie0" for identity-match linkage.
    static const char kLieDson[] =
        "{\"file_version\":\"0.6.0.0\","
        "\"image_library\":[{"
          "\"id\":\"lie0\",\"name\":\"lie0\","
          "\"map\":["
            "{"
              "\"url\":\"/base.jpg\",\"label\":\"Base\","
              "\"operation\":\"blend_source_over\","
              "\"transparency\":1.0,\"active\":true,\"invert\":false,"
              "\"color\":[0,0,0],"
              "\"rotation\":0.0,\"xscale\":1.0,\"yscale\":1.0,"
              "\"xoffset\":0.0,\"yoffset\":0.0,\"xmirror\":false,\"ymirror\":false"
            "},"
            "{"
              "\"url\":\"/overlay.jpg\",\"label\":\"Overlay\","
              "\"operation\":\"blend_multiply\","
              "\"transparency\":0.75,\"active\":true,\"invert\":true,"
              "\"color\":[1.0,0.5,0.25],"
              "\"rotation\":45.0,\"xscale\":2.0,\"yscale\":3.0,"
              "\"xoffset\":0.1,\"yoffset\":0.2,\"xmirror\":true,\"ymirror\":false"
            "}"
          "]"
        "}],"
        "\"scene\":{\"materials\":[{"
          "\"id\":\"LIETestMat\",\"name\":\"LIETestMat\","
          "\"diffuse\":{"
            "\"channel\":{"
              "\"id\":\"diffuse\",\"type\":\"float_color\","
              "\"current_value\":[1,1,1],"
              "\"image\":\"#lie0\""
            "}"
          "}"
        "}]}}";

    DsonDocumentHandle doc = DsonDocument_Create();
    if (!doc) { std::cout << "Failed to create document; skipping.\n\n"; return; }

    if (DsonDocument_LoadFromString(doc, kLieDson) != 0) {
        std::cout << "Error loading crafted DSON: " << DsonParser_GetLastError() << "\n\n";
        DsonDocument_Destroy(doc);
        return;
    }

    // Scene material 0, channel 0 (diffuse) should have 2 layers after linkage.
    int layerCount = DsonDocument_GetSceneMaterialChannelLayerCount(doc, 0, 0);
    std::cout << "Channel layer count (expect 2): " << layerCount << "\n";
    bool pass = (layerCount == 2);

    if (layerCount >= 2) {
        // Layer 0: blend_source_over, opacity 1, active, not invert, color 0/0/0, identity transform
        bool l0 = std::strcmp(DsonDocument_GetSceneMaterialChannelLayerBlendMode(doc, 0, 0, 0), "blend_source_over") == 0
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerOpacity(doc, 0, 0, 0) - 1.0) < 1e-9
               && DsonDocument_GetSceneMaterialChannelLayerActive(doc, 0, 0, 0) == true
               && DsonDocument_GetSceneMaterialChannelLayerInvert(doc, 0, 0, 0) == false
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerColorR(doc, 0, 0, 0)) < 1e-9
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerColorG(doc, 0, 0, 0)) < 1e-9
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerColorB(doc, 0, 0, 0)) < 1e-9
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerRotation(doc, 0, 0, 0)) < 1e-9
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerScaleX(doc, 0, 0, 0) - 1.0) < 1e-9
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerScaleY(doc, 0, 0, 0) - 1.0) < 1e-9
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerOffsetX(doc, 0, 0, 0)) < 1e-9
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerOffsetY(doc, 0, 0, 0)) < 1e-9
               && DsonDocument_GetSceneMaterialChannelLayerMirrorX(doc, 0, 0, 0) == false
               && DsonDocument_GetSceneMaterialChannelLayerMirrorY(doc, 0, 0, 0) == false;
        std::cout << "Channel L0 (all fields identity): " << (l0 ? "PASS" : "FAIL") << "\n";
        pass = pass && l0;

        // Layer 1: blend_multiply, opacity 0.75, invert, color 1/0.5/0.25, non-identity transform
        bool l1 = std::strcmp(DsonDocument_GetSceneMaterialChannelLayerBlendMode(doc, 0, 0, 1), "blend_multiply") == 0
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerOpacity(doc, 0, 0, 1) - 0.75) < 1e-9
               && DsonDocument_GetSceneMaterialChannelLayerActive(doc, 0, 0, 1) == true
               && DsonDocument_GetSceneMaterialChannelLayerInvert(doc, 0, 0, 1) == true
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerColorR(doc, 0, 0, 1) - 1.0)  < 1e-9
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerColorG(doc, 0, 0, 1) - 0.5)  < 1e-9
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerColorB(doc, 0, 0, 1) - 0.25) < 1e-9
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerRotation(doc, 0, 0, 1) - 45.0) < 1e-9
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerScaleX(doc, 0, 0, 1) - 2.0) < 1e-9
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerScaleY(doc, 0, 0, 1) - 3.0) < 1e-9
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerOffsetX(doc, 0, 0, 1) - 0.1) < 1e-9
               && std::fabs(DsonDocument_GetSceneMaterialChannelLayerOffsetY(doc, 0, 0, 1) - 0.2) < 1e-9
               && DsonDocument_GetSceneMaterialChannelLayerMirrorX(doc, 0, 0, 1) == true
               && DsonDocument_GetSceneMaterialChannelLayerMirrorY(doc, 0, 0, 1) == false;
        std::cout << "Channel L1 (blend_multiply, non-identity): " << (l1 ? "PASS" : "FAIL") << "\n";
        pass = pass && l1;

        // Out-of-range sentinel checks: ScaleX/Y return 1.0; others return 0/false/"".
        bool oob = std::fabs(DsonDocument_GetSceneMaterialChannelLayerScaleX(doc, 0, 0, 99) - 1.0) < 1e-9
                && std::fabs(DsonDocument_GetSceneMaterialChannelLayerScaleY(doc, 0, 0, 99) - 1.0) < 1e-9
                && std::fabs(DsonDocument_GetSceneMaterialChannelLayerOpacity(doc, 0, 0, 99)) < 1e-9
                && DsonDocument_GetSceneMaterialChannelLayerActive(doc, 0, 0, 99) == false
                && std::strcmp(DsonDocument_GetSceneMaterialChannelLayerBlendMode(doc, 0, 0, 99), "") == 0;
        std::cout << "Out-of-range sentinels (ScaleX/Y=1, Opacity/Active/BlendMode=0/false/\"\"): "
                  << (oob ? "PASS" : "FAIL") << "\n";
        pass = pass && oob;
    }

    std::cout << "\nPer-channel compositing test overall: " << (pass ? "PASS" : "FAIL") << "\n\n";
    DsonDocument_Destroy(doc);
}

// Verifies the scene.animations surface against Genesis_9_Mouth_MAT.duf:
//   - Prints count + per-entry kind/url/value (eyeball check).
//   - Asserts three anchor values for the Mouth material.
//   - Asserts NO-MERGE: Mouth diffuse scene-material channel STILL reads its
//     gray placeholder (R6.4); the parser must not have applied the anim value.
void RunSceneAnimationsTest() {
    std::cout << "=================================\n";
    std::cout << "SCENE ANIMATIONS TEST\n";
    std::cout << "=================================\n\n";

    const std::string filepath = ResolveTestFile("Genesis_9_Mouth_MAT.duf");
    if (filepath.empty()) {
        std::cout << "Genesis_9_Mouth_MAT.duf not found (tried direct path and TestFiles); skipping.\n\n";
        return;
    }

    DsonDocumentHandle doc = DsonDocument_Create();
    if (!doc) {
        std::cout << "Failed to create document; skipping.\n\n";
        return;
    }

    if (DsonDocument_LoadFromFile(doc, filepath.c_str()) != 0) {
        std::cout << "Error loading Genesis_9_Mouth_MAT.duf: " << DsonParser_GetLastError() << "\n\n";
        DsonDocument_Destroy(doc);
        return;
    }

    // Print animation surface
    int animCount = DsonDocument_GetSceneAnimationCount(doc);
    std::cout << "scene.animations count: " << animCount << "\n";
    const char* kindNames[] = {"null", "number", "bool", "string", "color"};
    for (int i = 0; i < animCount; i++) {
        int k = DsonDocument_GetSceneAnimationValueKind(doc, i);
        const char* kName = (k >= 0 && k <= 4) ? kindNames[k] : "invalid";
        std::cout << "  [" << i << "] kind=" << kName << " url=" << DsonDocument_GetSceneAnimationUrl(doc, i);
        if (k == 1)       std::cout << " val=" << DsonDocument_GetSceneAnimationFloat(doc, i);
        else if (k == 2)  std::cout << " val=" << (DsonDocument_GetSceneAnimationBool(doc, i) ? "true" : "false");
        else if (k == 3)  std::cout << " val=\"" << DsonDocument_GetSceneAnimationString(doc, i) << "\"";
        else if (k == 4)  std::cout << " val=(" << DsonDocument_GetSceneAnimationColorR(doc, i)
                                    << "," << DsonDocument_GetSceneAnimationColorG(doc, i)
                                    << "," << DsonDocument_GetSceneAnimationColorB(doc, i) << ")";
        std::cout << "\n";
    }
    std::cout << "\n";

    // Anchor 1: diffuse/image_file → String == expected path
    const char* kImageFileSuffix  = "Mouth:?diffuse/image_file";
    const char* kExpectedImagePath = "/Runtime/Textures/DAZ/Characters/Genesis9/Base/Genesis9_Mouth_D_1001.jpg";
    // Anchor 2: Diffuse%20Roughness/value → Number == 0.3
    const char* kDiffRoughSuffix  = "Mouth:?extra/studio_material_channels/channels/Diffuse%20Roughness/value";
    // Anchor 3: Translucency%20Weight/value → Number == 0.8
    const char* kTransWeightSuffix = "Mouth:?extra/studio_material_channels/channels/Translucency%20Weight/value";

    bool anchor1Pass = false, anchor2Pass = false, anchor3Pass = false;
    for (int i = 0; i < animCount; i++) {
        std::string url = DsonDocument_GetSceneAnimationUrl(doc, i);
        if (url.find(kImageFileSuffix) != std::string::npos) {
            anchor1Pass = (DsonDocument_GetSceneAnimationValueKind(doc, i) == 3) &&
                          (std::strcmp(DsonDocument_GetSceneAnimationString(doc, i), kExpectedImagePath) == 0);
        }
        if (url.find(kDiffRoughSuffix) != std::string::npos) {
            anchor2Pass = (DsonDocument_GetSceneAnimationValueKind(doc, i) == 1) &&
                          (std::fabs(DsonDocument_GetSceneAnimationFloat(doc, i) - 0.3) < 1e-9);
        }
        if (url.find(kTransWeightSuffix) != std::string::npos) {
            anchor3Pass = (DsonDocument_GetSceneAnimationValueKind(doc, i) == 1) &&
                          (std::fabs(DsonDocument_GetSceneAnimationFloat(doc, i) - 0.8) < 1e-9);
        }
    }
    std::cout << "Anchor 1 (Mouth diffuse/image_file == expected path): " << (anchor1Pass ? "PASS" : "FAIL") << "\n";
    std::cout << "Anchor 2 (Diffuse Roughness/value == 0.3):            " << (anchor2Pass ? "PASS" : "FAIL") << "\n";
    std::cout << "Anchor 3 (Translucency Weight/value == 0.8):          " << (anchor3Pass ? "PASS" : "FAIL") << "\n\n";

    // NO-MERGE check: Mouth scene material diffuse channel must still read ≈ 0.7529 (the gray placeholder)
    int mouthMatIndex = -1;
    int sceneMatCount = DsonDocument_GetSceneMaterialCount(doc);
    for (int i = 0; i < sceneMatCount; i++) {
        if (std::strcmp(DsonDocument_GetSceneMaterialId(doc, i), "Mouth") == 0) {
            mouthMatIndex = i;
            break;
        }
    }

    bool noMergePass = false;
    if (mouthMatIndex >= 0) {
        int chanCount = DsonDocument_GetSceneMaterialChannelCount(doc, mouthMatIndex);
        for (int c = 0; c < chanCount; c++) {
            const char* chanId = DsonDocument_GetSceneMaterialChannelId(doc, mouthMatIndex, c);
            if (std::strcmp(chanId, "diffuse") == 0) {
                double r = DsonDocument_GetSceneMaterialChannelColorR(doc, mouthMatIndex, c);
                // Gray placeholder ≈ 0.7529; anim value would be 1.0. Accept within 0.001.
                noMergePass = (std::fabs(r - 0.7529414) < 0.001);
                break;
            }
        }
    }
    std::cout << "NO-MERGE (Mouth diffuse ColorR ≈ 0.7529, not 1.0 from anim): " << (noMergePass ? "PASS" : "FAIL") << "\n\n";

    DsonDocument_Destroy(doc);
}

void RunCatalogPresentationTests()
{
    std::cout << "=================================\n";
    std::cout << "CATALOG PRESENTATION TESTS (1.5.0 / 2.2.0)\n";
    std::cout << "=================================\n\n";

    // --- test.dsf: modifier_library[0].presentation.type == "Modifier/Shape" ---
    const std::string dsfPath = ResolveTestFile("test.dsf");
    if (dsfPath.empty()) {
        std::cout << "test.dsf not found; skipping modifier presentation test.\n\n";
    } else {
        DsonDocumentHandle dsfDoc = DsonDocument_Create();
        if (DsonDocument_LoadFromFile(dsfDoc, dsfPath.c_str()) == 0) {
            const char* modPresType  = DsonDocument_GetModifierPresentationType(dsfDoc, 0);
            const char* modPresLabel = DsonDocument_GetModifierPresentationLabel(dsfDoc, 0);
            bool pass = std::string(modPresType) == "Modifier/Shape";
            std::cout << "test.dsf modifier[0] presentation.type:  \"" << modPresType  << "\"  [expect Modifier/Shape] " << (pass ? "PASS" : "FAIL") << "\n";
            std::cout << "test.dsf modifier[0] presentation.label: \"" << modPresLabel << "\"\n";
        } else {
            std::cout << "test.dsf load error: " << DsonParser_GetLastError() << "\n";
        }
        DsonDocument_Destroy(dsfDoc);
    }

    // --- Genesis9.json: geometry[0].is_graft == false (empty graft), node[0] presentation ---
    const std::string g9Path = ResolveTestFile("Genesis9.json");
    if (g9Path.empty()) {
        std::cout << "Genesis9.json not found; skipping geometry/node presentation tests.\n\n";
    } else {
        DsonDocumentHandle g9Doc = DsonDocument_Create();
        if (DsonDocument_LoadFromFile(g9Doc, g9Path.c_str()) == 0) {
            bool isGraft = DsonDocument_GetGeometryIsGraft(g9Doc, 0);
            bool pass = !isGraft;
            std::cout << "Genesis9.json geom[0] is_graft: " << (isGraft ? "true" : "false") << "  [expect false] " << (pass ? "PASS" : "FAIL") << "\n";
            const char* nodePresType  = DsonDocument_GetNodePresentationType(g9Doc, 0);
            const char* nodePresLabel = DsonDocument_GetNodePresentationLabel(g9Doc, 0);
            std::cout << "Genesis9.json node[0] presentation.type:  \"" << nodePresType  << "\"\n";
            std::cout << "Genesis9.json node[0] presentation.label: \"" << nodePresLabel << "\"\n";
            // Sentinel checks
            std::cout << "geom[-1] is_graft (sentinel): " << (DsonDocument_GetGeometryIsGraft(g9Doc, -1) ? "true" : "false") << "  [expect false]\n";
            std::cout << "node[-1] presType (sentinel): \"" << DsonDocument_GetNodePresentationType(g9Doc, -1) << "\"  [expect empty]\n";
            std::cout << "mod[-1]  presType (sentinel): \"" << DsonDocument_GetModifierPresentationType(g9Doc, -1) << "\"  [expect empty]\n";
        } else {
            std::cout << "Genesis9.json load error: " << DsonParser_GetLastError() << "\n";
        }
        DsonDocument_Destroy(g9Doc);
    }

    // --- 2.9.0: raw geograft weld correspondence ---
    const std::string genitaliaPath = ResolveTestFile("Genesis9FemaleGenitalia.dsf");
    if (genitaliaPath.empty()) {
        std::cout << "Genesis9FemaleGenitalia.dsf not found; skipping geograft weld test.\n\n";
    } else {
        DsonDocumentHandle genitaliaDoc = DsonDocument_Create();
        if (DsonDocument_LoadFromFile(genitaliaDoc, genitaliaPath.c_str()) == 0) {
            const int pairCount = DsonDocument_GetGeometryGraftVertexPairCount(genitaliaDoc, 0);
            const bool pairPass = DsonDocument_GetGeometryIsGraft(genitaliaDoc, 0)
                && pairCount == 82
                && DsonDocument_GetGeometryGraftVertexPairGraftVertex(genitaliaDoc, 0, 0) == 1
                && DsonDocument_GetGeometryGraftVertexPairBaseVertex(genitaliaDoc, 0, 0) == 8463
                && DsonDocument_GetGeometryGraftVertexPairGraftVertex(genitaliaDoc, 0, 81) == 653
                && DsonDocument_GetGeometryGraftVertexPairBaseVertex(genitaliaDoc, 0, 81) == 20922;
            const bool hiddenAndBasePass = DsonDocument_GetGeometryGraftHiddenPolyCount(genitaliaDoc, 0) == 180
                && DsonDocument_GetGeometryGraftHiddenPoly(genitaliaDoc, 0, 0) == 21286
                && DsonDocument_GetGeometryGraftBaseVertexCount(genitaliaDoc, 0) == 25182
                && DsonDocument_GetGeometryGraftBasePolyCount(genitaliaDoc, 0) == 25156;
            const bool sentinelPass = DsonDocument_GetGeometryGraftVertexPairCount(genitaliaDoc, -1) == 0
                && DsonDocument_GetGeometryGraftVertexPairGraftVertex(genitaliaDoc, 0, -1) == -1
                && DsonDocument_GetGeometryGraftVertexPairGraftVertex(genitaliaDoc, 0, pairCount) == -1
                && DsonDocument_GetGeometryGraftBaseVertexCount(genitaliaDoc, -1) == 0;
            std::cout << "Genesis9FemaleGenitalia graft pairs (82, endpoints): " << (pairPass ? "PASS" : "FAIL") << "\n";
            std::cout << "Genesis9FemaleGenitalia hidden/base counts:       " << (hiddenAndBasePass ? "PASS" : "FAIL") << "\n";
            std::cout << "Genesis9FemaleGenitalia graft sentinels:          " << (sentinelPass ? "PASS" : "FAIL") << "\n";
        } else {
            std::cout << "Genesis9FemaleGenitalia.dsf load error: " << DsonParser_GetLastError() << "\n";
        }
        DsonDocument_Destroy(genitaliaDoc);
    }

    const std::string shortsPath = ResolveTestFile("BaseShortsGeoGraft_318.dsf");
    if (shortsPath.empty()) {
        std::cout << "BaseShortsGeoGraft_318.dsf not found; skipping additive geograft weld test.\n\n";
    } else {
        DsonDocumentHandle shortsDoc = DsonDocument_Create();
        if (DsonDocument_LoadFromFile(shortsDoc, shortsPath.c_str()) == 0) {
            const bool shortsPass = DsonDocument_GetGeometryIsGraft(shortsDoc, 0)
                && DsonDocument_GetGeometryGraftVertexPairCount(shortsDoc, 0) == 106
                && DsonDocument_GetGeometryGraftVertexPairGraftVertex(shortsDoc, 0, 0) == 0
                && DsonDocument_GetGeometryGraftVertexPairBaseVertex(shortsDoc, 0, 0) == 7291
                && DsonDocument_GetGeometryGraftHiddenPolyCount(shortsDoc, 0) == 0
                && DsonDocument_GetGeometryGraftBaseVertexCount(shortsDoc, 0) == 8256
                && DsonDocument_GetGeometryGraftBasePolyCount(shortsDoc, 0) == 8130;
            const bool shortsSentinelPass = DsonDocument_GetGeometryGraftHiddenPoly(shortsDoc, 0, 0) == -1;
            std::cout << "BaseShortsGeoGraft additive graft values:          " << (shortsPass ? "PASS" : "FAIL") << "\n";
            std::cout << "BaseShortsGeoGraft empty-hidden sentinel:          " << (shortsSentinelPass ? "PASS" : "FAIL") << "\n\n";
        } else {
            std::cout << "BaseShortsGeoGraft_318.dsf load error: " << DsonParser_GetLastError() << "\n";
        }
        DsonDocument_Destroy(shortsDoc);
    }

    // --- 2.2.0: modifier group/region/icon faithful passthrough ---
    // body_bs_NipplesFeminine_HD3.dsf modifier[0]: all three populated (icon is a real, percent-encoded path).
    const std::string nipPath = ResolveTestFile("body_bs_NipplesFeminine_HD3.dsf");
    if (nipPath.empty()) {
        std::cout << "body_bs_NipplesFeminine_HD3.dsf not found; skipping modifier group/region/icon test.\n";
    } else {
        DsonDocumentHandle nipDoc = DsonDocument_Create();
        if (DsonDocument_LoadFromFile(nipDoc, nipPath.c_str()) == 0) {
            const char* g  = DsonDocument_GetModifierGroup(nipDoc, 0);
            const char* r  = DsonDocument_GetModifierRegion(nipDoc, 0);
            const char* ic = DsonDocument_GetModifierPresentationIcon(nipDoc, 0);
            bool gp = std::string(g)  == "/Feminine";
            bool rp = std::string(r)  == "Chest";
            bool ip = std::string(ic) == "/data/Daz%203D/Genesis%209/Base/Morphs/Daz%203D/Base/body_bs_NipplesFeminine_HD3.png";
            std::cout << "Nipples modifier[0] group:  \"" << g  << "\"  [expect /Feminine] " << (gp ? "PASS" : "FAIL") << "\n";
            std::cout << "Nipples modifier[0] region: \"" << r  << "\"  [expect Chest] "     << (rp ? "PASS" : "FAIL") << "\n";
            std::cout << "Nipples modifier[0] icon:   \"" << ic << "\"  [expect .png path] " << (ip ? "PASS" : "FAIL") << "\n";
            std::cout << "Nipples modifier[-1] group (sentinel):  \"" << DsonDocument_GetModifierGroup(nipDoc, -1)            << "\"  [expect empty]\n";
            std::cout << "Nipples modifier[-1] region (sentinel): \"" << DsonDocument_GetModifierRegion(nipDoc, -1)           << "\"  [expect empty]\n";
            std::cout << "Nipples modifier[-1] icon (sentinel):   \"" << DsonDocument_GetModifierPresentationIcon(nipDoc, -1) << "\"  [expect empty]\n";
        } else {
            std::cout << "body_bs_NipplesFeminine_HD3.dsf load error: " << DsonParser_GetLastError() << "\n";
        }
        DsonDocument_Destroy(nipDoc);
    }

    // BaseJointCorrectives.dsf modifier[0] ("JCMs On"): group populated, region ABSENT -> "", icon present-but-empty -> "".
    const std::string bjcPath = ResolveTestFile("BaseJointCorrectives.dsf");
    if (bjcPath.empty()) {
        std::cout << "BaseJointCorrectives.dsf not found; skipping region-absent sentinel test.\n";
    } else {
        DsonDocumentHandle bjcDoc = DsonDocument_Create();
        if (DsonDocument_LoadFromFile(bjcDoc, bjcPath.c_str()) == 0) {
            const char* g  = DsonDocument_GetModifierGroup(bjcDoc, 0);
            const char* r  = DsonDocument_GetModifierRegion(bjcDoc, 0);
            const char* ic = DsonDocument_GetModifierPresentationIcon(bjcDoc, 0);
            bool gp = std::string(g)  == "/General/Misc";
            bool rp = std::string(r).empty();   // region key absent -> ""
            bool ip = std::string(ic).empty();  // icon_large present but "" -> ""
            std::cout << "BaseJC modifier[0] group:  \"" << g  << "\"  [expect /General/Misc] " << (gp ? "PASS" : "FAIL") << "\n";
            std::cout << "BaseJC modifier[0] region: \"" << r  << "\"  [expect empty/absent] "   << (rp ? "PASS" : "FAIL") << "\n";
            std::cout << "BaseJC modifier[0] icon:   \"" << ic << "\"  [expect empty] "          << (ip ? "PASS" : "FAIL") << "\n";
            // Bool channel coercion (2.2.1): "JCMs On" channel has type:"bool", value:true -> must read as 1.0
            double bjcVal = DsonDocument_GetModifierChannelValue(bjcDoc, 0);
            std::cout << "BaseJC modifier[0] channel_value: " << bjcVal << "  [expect 1.0] " << (bjcVal == 1.0 ? "PASS" : "FAIL") << "\n";
        } else {
            std::cout << "BaseJointCorrectives.dsf load error: " << DsonParser_GetLastError() << "\n";
        }
        DsonDocument_Destroy(bjcDoc);
    }

    std::cout << "\n";
}

// ---- Per-thread last-error verification ----
// Proves that DsonParser_GetLastError() is per-thread (function-local thread_local):
// Worker A loads invalid JSON -> expects non-empty error on thread A.
// Worker B loads valid JSON on a separate handle -> expects empty error on thread B.
// Prints PASS/FAIL for each invariant.
//
// NOTE: this harness load-time-links DsonParser.lib, so it proves per-thread
// SEMANTICS but does NOT reproduce the original failure mode (file-scope thread_local
// not initialized for a thread that predates a dynamically loaded DLL via
// GetDllHandle). The function-local thread_local is the standard remedy for that
// case, but final confirmation of the GetDllHandle / pre-existing-thread path can
// only happen in the UE host.
void RunPerThreadLastErrorTest() {
    std::cout << "=================================\n";
    std::cout << "PER-THREAD LAST-ERROR TEST (1.6.0)\n";
    std::cout << "=================================\n\n";

    std::string errorA;
    std::string errorB;
    int resultA = -1;
    int resultB = -1;

    DsonDocumentHandle handleA = DsonDocument_Create();
    DsonDocumentHandle handleB = DsonDocument_Create();

    std::thread workerA([&]() {
        resultA = DsonDocument_LoadFromString(handleA, "{ this is invalid json");
        errorA = DsonParser_GetLastError();
    });

    std::thread workerB([&]() {
        resultB = DsonDocument_LoadFromString(handleB, "{}");
        errorB = DsonParser_GetLastError();
    });

    workerA.join();
    workerB.join();

    DsonDocument_Destroy(handleA);
    DsonDocument_Destroy(handleB);

    bool passA = (resultA != 0) && !errorA.empty();
    bool passB = (resultB == 0) && errorB.empty() && errorB != errorA;

    std::cout << "  Worker A (invalid JSON): resultA=" << resultA
              << " errorA=\"" << errorA << "\"\n";
    std::cout << "  Worker B (valid JSON):   resultB=" << resultB
              << " errorB=\"" << errorB << "\"\n\n";
    std::cout << "  A sees its own error:         " << (passA ? "PASS" : "FAIL") << "\n";
    std::cout << "  B sees empty (not A's error): " << (passB ? "PASS" : "FAIL") << "\n\n";
}

// Helper: search a specific context for any trail entry containing "used default".
static bool TrailHasTypeMismatch(DsonDocumentHandle doc, const char* context) {
    int count = DsonDocument_GetUnknownKeyCount(doc, context);
    for (int i = 0; i < count; i++) {
        const char* key = DsonDocument_GetUnknownKey(doc, context, i);
        if (key && std::strstr(key, "used default") != nullptr) {
            return true;
        }
    }
    return false;
}

// Helper: return true iff NO context in the document has a "used default" entry.
static bool TrailHasNoTypeMismatch(DsonDocumentHandle doc) {
    int ctxCount = DsonDocument_GetContextCount(doc);
    for (int i = 0; i < ctxCount; i++) {
        const char* ctx = DsonDocument_GetContextName(doc, i);
        if (TrailHasTypeMismatch(doc, ctx)) return false;
    }
    return true;
}

// Print all decorated trail entries across all contexts (for diagnostics).
static void PrintTypeMismatchEntries(DsonDocumentHandle doc) {
    int ctxCount = DsonDocument_GetContextCount(doc);
    bool found = false;
    for (int i = 0; i < ctxCount; i++) {
        const char* ctx = DsonDocument_GetContextName(doc, i);
        int count = DsonDocument_GetUnknownKeyCount(doc, ctx);
        for (int j = 0; j < count; j++) {
            const char* key = DsonDocument_GetUnknownKey(doc, ctx, j);
            if (key && std::strstr(key, "used default") != nullptr) {
                std::cout << "  trail [" << ctx << "]: \"" << key << "\"\n";
                found = true;
            }
        }
    }
    if (!found) std::cout << "  trail: (no decorated entries)\n";
}

// Verifies the channel type-mismatch audit trail (2.2.2):
//  (a) Modifier channel value is a STRING  -> GetModifierChannelValue == 0.0, trail has decorated entry.
//  (b) Material channel value is a STRING  -> GetMaterialChannelValue == 0.0, trail has decorated entry.
//  (c) NEGATIVE: numeric and bool channel values produce NO decorated entry (no false positives),
//      and the 2.2.1 bool-coercion values (1.0/0.0) remain correct.
void RunChannelTypeMismatchTests() {
    std::cout << "=================================\n";
    std::cout << "CHANNEL TYPE-MISMATCH AUDIT (2.2.2)\n";
    std::cout << "=================================\n\n";

    bool allPass = true;

    // --- (a) Modifier with string channel value ---
    {
        static const char kModStrJson[] =
            "{\"modifier_library\":[{\"id\":\"str_chan\","
            "\"channel\":{\"id\":\"dial_str\",\"type\":\"string\",\"value\":\"abc\"}}]}";

        DsonDocumentHandle doc = DsonDocument_Create();
        DsonDocument_LoadFromString(doc, kModStrJson);

        double val = DsonDocument_GetModifierChannelValue(doc, 0);
        bool valOk = (val == 0.0);
        bool trailOk = TrailHasTypeMismatch(doc, "modifier_library");

        std::cout << "(a) Modifier string channel:\n";
        std::cout << "  GetModifierChannelValue==" << val << "  [expect 0.0] " << (valOk ? "PASS" : "FAIL") << "\n";
        PrintTypeMismatchEntries(doc);
        std::cout << "  Trail has decorated entry: " << (trailOk ? "PASS" : "FAIL") << "\n\n";

        allPass = allPass && valOk && trailOk;
        DsonDocument_Destroy(doc);
    }

    // --- (b) Material channel with string current_value ---
    {
        static const char kMatStrJson[] =
            "{\"material_library\":[{\"id\":\"TestMat\",\"extra\":[{"
            "\"type\":\"studio_material_channels\",\"channels\":[{"
            "\"channel\":{\"id\":\"Metallic Weight\",\"type\":\"string\","
            "\"current_value\":\"notanumber\"}}]}]}]}";

        DsonDocumentHandle doc = DsonDocument_Create();
        DsonDocument_LoadFromString(doc, kMatStrJson);

        double val = DsonDocument_GetMaterialChannelValue(doc, 0, 0);
        bool valOk = (val == 0.0);
        bool trailOk = TrailHasTypeMismatch(doc, "material_library");

        std::cout << "(b) Material string channel:\n";
        std::cout << "  GetMaterialChannelValue==" << val << "  [expect 0.0] " << (valOk ? "PASS" : "FAIL") << "\n";
        PrintTypeMismatchEntries(doc);
        std::cout << "  Trail has decorated entry: " << (trailOk ? "PASS" : "FAIL") << "\n\n";

        allPass = allPass && valOk && trailOk;
        DsonDocument_Destroy(doc);
    }

    // --- (c) NEGATIVE: numeric channel -> value correct, no decorated entry ---
    {
        static const char kNumJson[] =
            "{\"modifier_library\":[{\"id\":\"num_chan\","
            "\"channel\":{\"id\":\"dial_num\",\"type\":\"float\",\"value\":0.75}}]}";

        DsonDocumentHandle doc = DsonDocument_Create();
        DsonDocument_LoadFromString(doc, kNumJson);

        double val = DsonDocument_GetModifierChannelValue(doc, 0);
        bool valOk  = (std::fabs(val - 0.75) < 1e-9);
        bool noMismatch = TrailHasNoTypeMismatch(doc);

        std::cout << "(c) NEGATIVE - numeric channel:\n";
        std::cout << "  GetModifierChannelValue==" << val << "  [expect 0.75] " << (valOk ? "PASS" : "FAIL") << "\n";
        PrintTypeMismatchEntries(doc);
        std::cout << "  No decorated entry (no false positive): " << (noMismatch ? "PASS" : "FAIL") << "\n\n";

        allPass = allPass && valOk && noMismatch;
        DsonDocument_Destroy(doc);
    }

    // --- (c) NEGATIVE: bool channel (2.2.1 coercion) -> value 1.0, no decorated entry ---
    {
        static const char kBoolJson[] =
            "{\"modifier_library\":[{\"id\":\"bool_chan\","
            "\"channel\":{\"id\":\"dial_bool\",\"type\":\"bool\",\"value\":true}}]}";

        DsonDocumentHandle doc = DsonDocument_Create();
        DsonDocument_LoadFromString(doc, kBoolJson);

        double val = DsonDocument_GetModifierChannelValue(doc, 0);
        bool valOk  = (val == 1.0);
        bool noMismatch = TrailHasNoTypeMismatch(doc);

        std::cout << "(c) NEGATIVE - bool channel (2.2.1 regression):\n";
        std::cout << "  GetModifierChannelValue==" << val << "  [expect 1.0] " << (valOk ? "PASS" : "FAIL") << "\n";
        PrintTypeMismatchEntries(doc);
        std::cout << "  No decorated entry (no false positive): " << (noMismatch ? "PASS" : "FAIL") << "\n\n";

        allPass = allPass && valOk && noMismatch;
        DsonDocument_Destroy(doc);
    }

    std::cout << "Channel type-mismatch audit overall: " << (allPass ? "PASS" : "FAIL") << "\n\n";
}

static void RunModifierPushOffsetTest() {
    std::cout << "=================================\n";
    std::cout << "MODIFIER PUSH OFFSET TEST (2.7.0)\n";
    std::cout << "=================================\n\n";

    static const char kPushModifierJson[] = R"JSON(
{
  "modifier_library": [
    {
      "id": "Offset",
      "name": "Offset",
      "parent": "Genesis9_Shell.dsf#Genesis9_Shell",
      "extra": [
        { "type": "studio/modifier/push", "push_post_smooth": false },
        {
          "type": "studio_modifier_channels",
          "channels": [
            {
              "channel": {
                "id": "Value", "type": "float",
                "label": "Offset Distance (cm)",
                "value": 0, "current_value": 0.1,
                "min": -10000, "max": 10000, "step_size": 0.01
              },
              "group": "/General/Mesh Offset"
            }
          ]
        }
      ]
    }
  ]
}
)JSON";

    DsonDocumentHandle doc = DsonDocument_Create();
    const int loadResult = DsonDocument_LoadFromString(doc, kPushModifierJson);
    if (loadResult != 0) {
        std::cout << "Push modifier fixture load: FAIL (result " << loadResult << ")\n\n";
        DsonDocument_Destroy(doc);
        return;
    }

    const bool isPush = DsonDocument_GetModifierIsPush(doc, 0);
    const double offset = DsonDocument_GetModifierPushOffset(doc, 0);
    const bool typeUnchanged = std::strcmp(DsonDocument_GetModifierType(doc, 0), "") == 0;
    const bool channelUnchanged = DsonDocument_GetModifierChannelValue(doc, 0) == 0.0;
    const bool invalidPush = !DsonDocument_GetModifierIsPush(doc, -1);
    const bool invalidOffset = DsonDocument_GetModifierPushOffset(doc, -1) == 0.0;
    const bool offsetPass = std::fabs(offset - 0.1) < 1e-9;

    std::cout << "GetModifierIsPush: " << (isPush ? "PASS" : "FAIL") << "\n";
    std::cout << "GetModifierPushOffset==" << offset << "  [expect 0.1] "
              << (offsetPass ? "PASS" : "FAIL") << "\n";
    std::cout << "GetModifierType remains empty: " << (typeUnchanged ? "PASS" : "FAIL") << "\n";
    std::cout << "GetModifierChannelValue remains 0.0: " << (channelUnchanged ? "PASS" : "FAIL") << "\n";
    std::cout << "Invalid-index IsPush sentinel: " << (invalidPush ? "PASS" : "FAIL") << "\n";
    std::cout << "Invalid-index PushOffset sentinel: " << (invalidOffset ? "PASS" : "FAIL") << "\n";

    const bool allPass = isPush && offsetPass && typeUnchanged && channelUnchanged &&
        invalidPush && invalidOffset;
    std::cout << "Modifier push offset overall: " << (allPass ? "PASS" : "FAIL") << "\n\n";
    DsonDocument_Destroy(doc);
}

static const char* kSplineFixture = R"JSON(
{
  "asset_info": { "id": "/data/test/spline_fixture.dsf", "type": "modifier" },
  "modifier_library": [
    { "id": "body_cbs_forearm_y135n_l",
      "formulas": [
        { "output": "Genesis9:#body_cbs_forearm_y135n_l?value",
          "operations": [
            { "op": "push", "url": "Genesis9:l_forearm?rotation/y" },
            { "op": "push", "val": [-135, 1, 0, 0, 0] },
            { "op": "push", "val": [-75, 0, 0, 0, 0] },
            { "op": "push", "val": [0.5, 0, 0, 0, 0] },
            { "op": "push", "val": 3 },
            { "op": "spline_tcb" }
          ] } ] }
  ],
  "scene": {
    "modifiers": [
      { "id": "scene_forearm_drv",
        "formulas": [
          { "output": "Genesis9:#body_cbs_forearm_y135n_l?value",
            "operations": [
              { "op": "push", "url": "Genesis9:l_forearm?rotation/y" },
              { "op": "push", "val": [-75, 1, 0, 0, 0] },
              { "op": "push", "val": [0, 0, 0, 0, 0] },
              { "op": "push", "val": 2 },
              { "op": "spline_tcb" }
            ] } ] }
    ]
  }
}
)JSON";

static void TestFormulaValArray()
{
    std::cout << "=================================\n";
    std::cout << "FORMULA VAL_ARRAY TESTS (2.1.0)\n";
    std::cout << "=================================\n\n";

    DsonDocumentHandle doc = DsonDocument_Create();
    int loadResult = DsonDocument_LoadFromString(doc, kSplineFixture);
    if (loadResult != 0) {
        std::cout << "FAIL: LoadFromString returned " << loadResult << "\n\n";
        DsonDocument_Destroy(doc);
        return;
    }

    bool allPass = true;

    // --- modifier_library family ---
    // modifier 0, formula 0 should have 6 operations
    int opCount = DsonDocument_GetModifierFormulaOperationCount(doc, 0, 0);
    bool pass = (opCount == 6);
    allPass &= pass;
    std::cout << "mod[0] formula[0] OperationCount==" << opCount << "  [expect 6]  " << (pass ? "PASS" : "FAIL") << "\n";

    // op0: url push, no array
    {
        const char* opStr = DsonDocument_GetModifierFormulaOperationOp(doc, 0, 0, 0);
        const char* url   = DsonDocument_GetModifierFormulaOperationUrl(doc, 0, 0, 0);
        int arrCount      = DsonDocument_GetModifierFormulaOperationValArrayCount(doc, 0, 0, 0);
        bool p = (std::string(opStr) == "push") && (std::string(url) == "Genesis9:l_forearm?rotation/y") && (arrCount == 0);
        allPass &= p;
        std::cout << "  op0: Op=" << opStr << " Url=" << url << " ValArrayCount=" << arrCount << "  [expect push / Genesis9:l_forearm?rotation/y / 0]  " << (p ? "PASS" : "FAIL") << "\n";
    }

    // op1: [-135, 1, 0, 0, 0]
    {
        int arrCount = DsonDocument_GetModifierFormulaOperationValArrayCount(doc, 0, 0, 1);
        double e0 = DsonDocument_GetModifierFormulaOperationValArrayElement(doc, 0, 0, 1, 0);
        double e1 = DsonDocument_GetModifierFormulaOperationValArrayElement(doc, 0, 0, 1, 1);
        double e2 = DsonDocument_GetModifierFormulaOperationValArrayElement(doc, 0, 0, 1, 2);
        double e3 = DsonDocument_GetModifierFormulaOperationValArrayElement(doc, 0, 0, 1, 3);
        double e4 = DsonDocument_GetModifierFormulaOperationValArrayElement(doc, 0, 0, 1, 4);
        bool p = (arrCount == 5) && (e0 == -135) && (e1 == 1) && (e2 == 0) && (e3 == 0) && (e4 == 0);
        allPass &= p;
        std::cout << "  op1: ValArrayCount=" << arrCount << " elements={" << e0 << "," << e1 << "," << e2 << "," << e3 << "," << e4 << "}  [expect 5 / {-135,1,0,0,0}]  " << (p ? "PASS" : "FAIL") << "\n";
    }

    // op2: [-75, 0, 0, 0, 0]
    {
        int arrCount = DsonDocument_GetModifierFormulaOperationValArrayCount(doc, 0, 0, 2);
        double e0 = DsonDocument_GetModifierFormulaOperationValArrayElement(doc, 0, 0, 2, 0);
        double e1 = DsonDocument_GetModifierFormulaOperationValArrayElement(doc, 0, 0, 2, 1);
        bool p = (arrCount == 5) && (e0 == -75) && (e1 == 0);
        allPass &= p;
        std::cout << "  op2: ValArrayCount=" << arrCount << " e[0]=" << e0 << " e[1]=" << e1 << "  [expect 5 / -75 / 0]  " << (p ? "PASS" : "FAIL") << "\n";
    }

    // op3: [0.5, 0, 0, 0, 0]
    {
        int arrCount = DsonDocument_GetModifierFormulaOperationValArrayCount(doc, 0, 0, 3);
        double e0 = DsonDocument_GetModifierFormulaOperationValArrayElement(doc, 0, 0, 3, 0);
        bool p = (arrCount == 5) && (e0 == 0.5);
        allPass &= p;
        std::cout << "  op3: ValArrayCount=" << arrCount << " e[0]=" << e0 << "  [expect 5 / 0.5]  " << (p ? "PASS" : "FAIL") << "\n";
    }

    // op4: scalar val==3, no array
    {
        int arrCount = DsonDocument_GetModifierFormulaOperationValArrayCount(doc, 0, 0, 4);
        double scalarVal = DsonDocument_GetModifierFormulaOperationVal(doc, 0, 0, 4);
        const char* opStr = DsonDocument_GetModifierFormulaOperationOp(doc, 0, 0, 4);
        bool p = (arrCount == 0) && (scalarVal == 3.0) && (std::string(opStr) == "push");
        allPass &= p;
        std::cout << "  op4: Op=" << opStr << " ValArrayCount=" << arrCount << " Val=" << scalarVal << "  [expect push / 0 / 3]  " << (p ? "PASS" : "FAIL") << "\n";
    }

    // op5: spline_tcb token, no array
    {
        const char* opStr = DsonDocument_GetModifierFormulaOperationOp(doc, 0, 0, 5);
        int arrCount = DsonDocument_GetModifierFormulaOperationValArrayCount(doc, 0, 0, 5);
        bool p = (std::string(opStr) == "spline_tcb") && (arrCount == 0);
        allPass &= p;
        std::cout << "  op5: Op=" << opStr << " ValArrayCount=" << arrCount << "  [expect spline_tcb / 0]  " << (p ? "PASS" : "FAIL") << "\n";
    }

    // out-of-range element on an array op -> 0.0
    {
        double oob = DsonDocument_GetModifierFormulaOperationValArrayElement(doc, 0, 0, 1, 99);
        bool p = (oob == 0.0);
        allPass &= p;
        std::cout << "  op1 element[99] (out of range)=" << oob << "  [expect 0.0]  " << (p ? "PASS" : "FAIL") << "\n";
    }

    std::cout << "\n";

    // --- scene.modifiers family ---
    // scene modifier 0, formula 0 should have 5 operations
    int sceneOpCount = DsonDocument_GetSceneModifierFormulaOperationCount(doc, 0, 0);
    bool spass = (sceneOpCount == 5);
    allPass &= spass;
    std::cout << "scene.mod[0] formula[0] OperationCount==" << sceneOpCount << "  [expect 5]  " << (spass ? "PASS" : "FAIL") << "\n";

    // scene op1: [-75, 1, 0, 0, 0]
    {
        int arrCount = DsonDocument_GetSceneModifierFormulaOperationValArrayCount(doc, 0, 0, 1);
        double e0 = DsonDocument_GetSceneModifierFormulaOperationValArrayElement(doc, 0, 0, 1, 0);
        double e1 = DsonDocument_GetSceneModifierFormulaOperationValArrayElement(doc, 0, 0, 1, 1);
        bool p = (arrCount == 5) && (e0 == -75) && (e1 == 1);
        allPass &= p;
        std::cout << "  scene op1: ValArrayCount=" << arrCount << " {" << e0 << "," << e1 << ",...}  [expect 5 / {-75,1,...}]  " << (p ? "PASS" : "FAIL") << "\n";
    }

    // scene op2: [0, 0, 0, 0, 0]
    {
        int arrCount = DsonDocument_GetSceneModifierFormulaOperationValArrayCount(doc, 0, 0, 2);
        double e0 = DsonDocument_GetSceneModifierFormulaOperationValArrayElement(doc, 0, 0, 2, 0);
        bool p = (arrCount == 5) && (e0 == 0.0);
        allPass &= p;
        std::cout << "  scene op2: ValArrayCount=" << arrCount << " e[0]=" << e0 << "  [expect 5 / 0.0]  " << (p ? "PASS" : "FAIL") << "\n";
    }

    // scene op3: scalar val==2, no array
    {
        int arrCount = DsonDocument_GetSceneModifierFormulaOperationValArrayCount(doc, 0, 0, 3);
        double scalarVal = DsonDocument_GetSceneModifierFormulaOperationVal(doc, 0, 0, 3);
        bool p = (arrCount == 0) && (scalarVal == 2.0);
        allPass &= p;
        std::cout << "  scene op3: ValArrayCount=" << arrCount << " Val=" << scalarVal << "  [expect 0 / 2.0]  " << (p ? "PASS" : "FAIL") << "\n";
    }

    // scene op4: spline_tcb, no array
    {
        const char* opStr = DsonDocument_GetSceneModifierFormulaOperationOp(doc, 0, 0, 4);
        int arrCount = DsonDocument_GetSceneModifierFormulaOperationValArrayCount(doc, 0, 0, 4);
        bool p = (std::string(opStr) == "spline_tcb") && (arrCount == 0);
        allPass &= p;
        std::cout << "  scene op4: Op=" << opStr << " ValArrayCount=" << arrCount << "  [expect spline_tcb / 0]  " << (p ? "PASS" : "FAIL") << "\n";
    }

    std::cout << "\nFormula val_array: " << (allPass ? "PASS" : "FAIL") << "\n\n";

    DsonDocument_Destroy(doc);
}

static bool NearlyEqual(double actual, double expected, double tolerance = 1e-6)
{
    return std::fabs(actual - expected) <= tolerance;
}

static void RunSceneNodeAuthoredFieldsTest()
{
    static const char kSceneNodeAuthoredFieldsJson[] = R"JSON(
{
  "node_library": [
    {
      "id": "definition",
      "translation": [21, 22, 23],
      "rotation": [24, 25, 26],
      "scale": [2, 2, 2],
      "general_scale": {"value": 2},
      "rotation_order": "XYZ",
      "center_point": [11, 12, 13],
      "orientation": [14, 15, 16],
      "inherits_scale": true
    }
  ],
  "scene": {
    "nodes": [
      {
        "id": "full",
        "translation": [
          {"id":"x", "value":99, "current_value":0},
          {"id":"y", "value":2},
          {"id":"z", "current_value":3}
        ],
        "rotation": [
          {"id":"x", "current_value":4},
          {"id":"y", "value":5},
          {"id":"z", "value":99, "current_value":0}
        ],
        "scale": [
          {"id":"x", "value":9, "current_value":1},
          {"id":"y", "value":2},
          {"id":"z", "current_value":3}
        ],
        "general_scale": {"value":1, "current_value":9},
        "rotation_order": "YXZ",
        "center_point": [
          {"id":"x", "value":99, "current_value":0},
          {"id":"y", "value":2},
          {"id":"z", "current_value":3}
        ],
        "orientation": [
          {"id":"x", "current_value":4},
          {"id":"y", "value":5},
          {"id":"z", "value":99, "current_value":0}
        ],
        "inherits_scale": true
      },
      {
        "id": "partial",
        "translation": [
          {"id":"x", "value":0},
          {"id":"y", "value":8, "current_value":"not-numeric"},
          {"id":"z", "current_value":-3}
        ],
        "rotation": [0, "not-numeric", 7],
        "scale": [1, "not-numeric", 3],
        "general_scale": {"value":"not-numeric", "current_value":2},
        "rotation_order": 42,
        "center_point": [
          {"id":"x", "value":0},
          {"id":"y", "value":"not-numeric"},
          {"id":"z", "current_value":-3}
        ],
        "orientation": [0, "not-numeric", 7],
        "inherits_scale": false
      },
      {
        "id": "instance-without-authored-fields",
        "url": "#definition"
      }
    ]
  }
}
)JSON";

    std::cout << "=================================\n";
    std::cout << "SCENE NODE AUTHORED FIELDS TEST (2.6.0)\n";
    std::cout << "=================================\n\n";

    DsonDocumentHandle doc = DsonDocument_Create();
    if (!doc || DsonDocument_LoadFromString(doc, kSceneNodeAuthoredFieldsJson) != 0) {
        std::cout << "Scene-node authored-fields fixture load: FAIL\n\n";
        DsonDocument_Destroy(doc);
        return;
    }

    const int xyzMask = DSONPARSER_VECTOR_COMPONENT_X
        | DSONPARSER_VECTOR_COMPONENT_Y
        | DSONPARSER_VECTOR_COMPONENT_Z;
    const int xzMask = DSONPARSER_VECTOR_COMPONENT_X
        | DSONPARSER_VECTOR_COMPONENT_Z;

    const bool fullPass =
        NearlyEqual(DsonDocument_GetSceneNodeTranslationX(doc, 0), 0.0)
        && NearlyEqual(DsonDocument_GetSceneNodeTranslationY(doc, 0), 2.0)
        && NearlyEqual(DsonDocument_GetSceneNodeTranslationZ(doc, 0), 3.0)
        && DsonDocument_GetSceneNodeTranslationPresenceMask(doc, 0) == xyzMask
        && NearlyEqual(DsonDocument_GetSceneNodeRotationX(doc, 0), 4.0)
        && NearlyEqual(DsonDocument_GetSceneNodeRotationY(doc, 0), 5.0)
        && NearlyEqual(DsonDocument_GetSceneNodeRotationZ(doc, 0), 0.0)
        && DsonDocument_GetSceneNodeRotationPresenceMask(doc, 0) == xyzMask
        && NearlyEqual(DsonDocument_GetSceneNodeScaleX(doc, 0), 1.0)
        && NearlyEqual(DsonDocument_GetSceneNodeScaleY(doc, 0), 2.0)
        && NearlyEqual(DsonDocument_GetSceneNodeScaleZ(doc, 0), 3.0)
        && DsonDocument_GetSceneNodeScalePresenceMask(doc, 0) == xyzMask
        && NearlyEqual(DsonDocument_GetSceneNodeGeneralScale(doc, 0), 1.0)
        && DsonDocument_GetSceneNodeHasGeneralScale(doc, 0)
        && std::strcmp(DsonDocument_GetSceneNodeRotationOrder(doc, 0), "YXZ") == 0
        && DsonDocument_GetSceneNodeHasRotationOrder(doc, 0)
        && NearlyEqual(DsonDocument_GetSceneNodeCenterPointX(doc, 0), 0.0)
        && NearlyEqual(DsonDocument_GetSceneNodeCenterPointY(doc, 0), 2.0)
        && NearlyEqual(DsonDocument_GetSceneNodeCenterPointZ(doc, 0), 3.0)
        && DsonDocument_GetSceneNodeCenterPointPresenceMask(doc, 0) == xyzMask
        && NearlyEqual(DsonDocument_GetSceneNodeOrientationX(doc, 0), 4.0)
        && NearlyEqual(DsonDocument_GetSceneNodeOrientationY(doc, 0), 5.0)
        && NearlyEqual(DsonDocument_GetSceneNodeOrientationZ(doc, 0), 0.0)
        && DsonDocument_GetSceneNodeOrientationPresenceMask(doc, 0) == xyzMask
        && DsonDocument_GetSceneNodeInheritsScale(doc, 0)
        && DsonDocument_GetSceneNodeHasInheritsScale(doc, 0);

    const bool partialPass =
        NearlyEqual(DsonDocument_GetSceneNodeTranslationX(doc, 1), 0.0)
        && NearlyEqual(DsonDocument_GetSceneNodeTranslationY(doc, 1), 0.0)
        && NearlyEqual(DsonDocument_GetSceneNodeTranslationZ(doc, 1), -3.0)
        && DsonDocument_GetSceneNodeTranslationPresenceMask(doc, 1) == xzMask
        && NearlyEqual(DsonDocument_GetSceneNodeRotationX(doc, 1), 0.0)
        && NearlyEqual(DsonDocument_GetSceneNodeRotationY(doc, 1), 0.0)
        && NearlyEqual(DsonDocument_GetSceneNodeRotationZ(doc, 1), 7.0)
        && DsonDocument_GetSceneNodeRotationPresenceMask(doc, 1) == xzMask
        && NearlyEqual(DsonDocument_GetSceneNodeScaleX(doc, 1), 1.0)
        && NearlyEqual(DsonDocument_GetSceneNodeScaleY(doc, 1), 1.0)
        && NearlyEqual(DsonDocument_GetSceneNodeScaleZ(doc, 1), 3.0)
        && DsonDocument_GetSceneNodeScalePresenceMask(doc, 1) == xzMask
        && NearlyEqual(DsonDocument_GetSceneNodeGeneralScale(doc, 1), 1.0)
        && !DsonDocument_GetSceneNodeHasGeneralScale(doc, 1)
        && std::strcmp(DsonDocument_GetSceneNodeRotationOrder(doc, 1), "YXZ") == 0
        && !DsonDocument_GetSceneNodeHasRotationOrder(doc, 1)
        && NearlyEqual(DsonDocument_GetSceneNodeCenterPointX(doc, 1), 0.0)
        && NearlyEqual(DsonDocument_GetSceneNodeCenterPointY(doc, 1), 0.0)
        && NearlyEqual(DsonDocument_GetSceneNodeCenterPointZ(doc, 1), -3.0)
        && DsonDocument_GetSceneNodeCenterPointPresenceMask(doc, 1) == xzMask
        && NearlyEqual(DsonDocument_GetSceneNodeOrientationX(doc, 1), 0.0)
        && NearlyEqual(DsonDocument_GetSceneNodeOrientationY(doc, 1), 0.0)
        && NearlyEqual(DsonDocument_GetSceneNodeOrientationZ(doc, 1), 7.0)
        && DsonDocument_GetSceneNodeOrientationPresenceMask(doc, 1) == xzMask
        && !DsonDocument_GetSceneNodeInheritsScale(doc, 1)
        && DsonDocument_GetSceneNodeHasInheritsScale(doc, 1);

    const bool separationPass =
        std::strcmp(DsonDocument_GetSceneNodeUrl(doc, 2), "#definition") == 0
        && DsonDocument_GetSceneNodeTranslationPresenceMask(doc, 2) == 0
        && DsonDocument_GetSceneNodeRotationPresenceMask(doc, 2) == 0
        && DsonDocument_GetSceneNodeScalePresenceMask(doc, 2) == 0
        && !DsonDocument_GetSceneNodeHasGeneralScale(doc, 2)
        && !DsonDocument_GetSceneNodeHasRotationOrder(doc, 2)
        && DsonDocument_GetSceneNodeCenterPointX(doc, 2) == 0.0
        && DsonDocument_GetSceneNodeCenterPointY(doc, 2) == 0.0
        && DsonDocument_GetSceneNodeCenterPointZ(doc, 2) == 0.0
        && DsonDocument_GetSceneNodeCenterPointPresenceMask(doc, 2) == 0
        && DsonDocument_GetSceneNodeOrientationX(doc, 2) == 0.0
        && DsonDocument_GetSceneNodeOrientationY(doc, 2) == 0.0
        && DsonDocument_GetSceneNodeOrientationZ(doc, 2) == 0.0
        && DsonDocument_GetSceneNodeOrientationPresenceMask(doc, 2) == 0
        && !DsonDocument_GetSceneNodeInheritsScale(doc, 2)
        && !DsonDocument_GetSceneNodeHasInheritsScale(doc, 2);

    const bool invalidIndexPass =
        DsonDocument_GetSceneNodeTranslationPresenceMask(doc, -1) == 0
        && DsonDocument_GetSceneNodeRotationPresenceMask(doc, -1) == 0
        && DsonDocument_GetSceneNodeScalePresenceMask(doc, -1) == 0
        && !DsonDocument_GetSceneNodeHasGeneralScale(doc, -1)
        && !DsonDocument_GetSceneNodeHasRotationOrder(doc, -1)
        && DsonDocument_GetSceneNodeCenterPointX(doc, -1) == 0.0
        && DsonDocument_GetSceneNodeCenterPointY(doc, -1) == 0.0
        && DsonDocument_GetSceneNodeCenterPointZ(doc, -1) == 0.0
        && DsonDocument_GetSceneNodeCenterPointPresenceMask(doc, -1) == 0
        && DsonDocument_GetSceneNodeOrientationX(doc, -1) == 0.0
        && DsonDocument_GetSceneNodeOrientationY(doc, -1) == 0.0
        && DsonDocument_GetSceneNodeOrientationZ(doc, -1) == 0.0
        && DsonDocument_GetSceneNodeOrientationPresenceMask(doc, -1) == 0
        && !DsonDocument_GetSceneNodeInheritsScale(doc, -1)
        && !DsonDocument_GetSceneNodeHasInheritsScale(doc, -1);

    const bool invalidHandlePass =
        DsonDocument_GetSceneNodeTranslationPresenceMask(nullptr, 0) == 0
        && DsonDocument_GetSceneNodeRotationPresenceMask(nullptr, 0) == 0
        && DsonDocument_GetSceneNodeScalePresenceMask(nullptr, 0) == 0
        && !DsonDocument_GetSceneNodeHasGeneralScale(nullptr, 0)
        && !DsonDocument_GetSceneNodeHasRotationOrder(nullptr, 0)
        && DsonDocument_GetSceneNodeCenterPointX(nullptr, 0) == 0.0
        && DsonDocument_GetSceneNodeCenterPointY(nullptr, 0) == 0.0
        && DsonDocument_GetSceneNodeCenterPointZ(nullptr, 0) == 0.0
        && DsonDocument_GetSceneNodeCenterPointPresenceMask(nullptr, 0) == 0
        && DsonDocument_GetSceneNodeOrientationX(nullptr, 0) == 0.0
        && DsonDocument_GetSceneNodeOrientationY(nullptr, 0) == 0.0
        && DsonDocument_GetSceneNodeOrientationZ(nullptr, 0) == 0.0
        && DsonDocument_GetSceneNodeOrientationPresenceMask(nullptr, 0) == 0
        && !DsonDocument_GetSceneNodeInheritsScale(nullptr, 0)
        && !DsonDocument_GetSceneNodeHasInheritsScale(nullptr, 0);

    const bool knownKeyPass = DsonDocument_GetUnknownKeyCount(doc, "scene") == 0;
    const bool allPass = fullPass && partialPass && separationPass
        && invalidIndexPass && invalidHandlePass && knownKeyPass;

    std::cout << "Full authored fields/current-value preference/value fallback: "
              << (fullPass ? "PASS" : "FAIL") << "\n";
    std::cout << "Partial vectors/identity values/non-numeric selections: "
              << (partialPass ? "PASS" : "FAIL") << "\n";
    std::cout << "Scene/library non-merge and absent fields: "
              << (separationPass ? "PASS" : "FAIL") << "\n";
    std::cout << "Invalid index sentinels: " << (invalidIndexPass ? "PASS" : "FAIL") << "\n";
    std::cout << "Invalid handle sentinels: " << (invalidHandlePass ? "PASS" : "FAIL") << "\n";
    std::cout << "Scene authored-field keys recognized: " << (knownKeyPass ? "PASS" : "FAIL") << "\n";
    std::cout << "Scene-node authored fields overall: " << (allPass ? "PASS" : "FAIL") << "\n\n";

    DsonDocument_Destroy(doc);
}

static void RunSceneNodePlacementTest()
{
    const std::string filepath = ResolveTestFile("JB Jewel Bikini Bottom And Wrap.duf");
    if (filepath.empty()) {
        std::cout << "JB Jewel Bikini Bottom And Wrap.duf not found; skipping scene-node placement test.\n\n";
        return;
    }

    DsonDocumentHandle doc = DsonDocument_Create();
    if (!doc) {
        std::cout << "Failed to create document; skipping scene-node placement test.\n\n";
        return;
    }
    if (DsonDocument_LoadFromFile(doc, filepath.c_str()) != 0) {
        std::cout << "Scene-node placement fixture failed to load: FAIL\n\n";
        DsonDocument_Destroy(doc);
        return;
    }

    struct ExpectedPlacement {
        const char* id;
        const char* parent;
        double translation[3];
        double rotation[3];
    };
    const ExpectedPlacement expected[] = {
        { "Genesis9_JewelBikini._Bottom_66554", "name://@selection:",
          { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 } },
        { "Genesis9_JewelBikini_GemDrop", "#Gem%20Drop%2013-1",
          { 0.01051213, -121.412, -15.98289 },
          { -0.002171162, 0.001429191, -0.000699376 } },
        { "Genesis9_JewelBikini_GemDrop-2", "#Gem%20Drop%2014-1",
          { -0.004216239, -121.4067, -15.98666 }, { 0.0, 0.0, 0.0 } },
        { "Genesis9_JewelBikini_GemDrop-28", "#Gem%20Drop%2026-3",
          { -0.01594961, -121.3986, -15.94817 },
          { 2.966018e-10, -178.6344, 1.69906e-10 } }
    };

    bool allPass = true;
    const int nodeCount = DsonDocument_GetSceneNodeCount(doc);
    for (const ExpectedPlacement& item : expected) {
        int nodeIndex = -1;
        for (int i = 0; i < nodeCount; ++i) {
            if (std::strcmp(DsonDocument_GetSceneNodeId(doc, i), item.id) == 0) {
                nodeIndex = i;
                break;
            }
        }

        bool pass = nodeIndex >= 0;
        if (pass) {
            pass = std::strcmp(DsonDocument_GetSceneNodeParent(doc, nodeIndex), item.parent) == 0
                && NearlyEqual(DsonDocument_GetSceneNodeTranslationX(doc, nodeIndex), item.translation[0])
                && NearlyEqual(DsonDocument_GetSceneNodeTranslationY(doc, nodeIndex), item.translation[1])
                && NearlyEqual(DsonDocument_GetSceneNodeTranslationZ(doc, nodeIndex), item.translation[2])
                && NearlyEqual(DsonDocument_GetSceneNodeRotationX(doc, nodeIndex), item.rotation[0])
                && NearlyEqual(DsonDocument_GetSceneNodeRotationY(doc, nodeIndex), item.rotation[1])
                && NearlyEqual(DsonDocument_GetSceneNodeRotationZ(doc, nodeIndex), item.rotation[2])
                && NearlyEqual(DsonDocument_GetSceneNodeScaleX(doc, nodeIndex), 1.0)
                && NearlyEqual(DsonDocument_GetSceneNodeScaleY(doc, nodeIndex), 1.0)
                && NearlyEqual(DsonDocument_GetSceneNodeScaleZ(doc, nodeIndex), 1.0)
                && NearlyEqual(DsonDocument_GetSceneNodeGeneralScale(doc, nodeIndex), 1.0)
                && std::strcmp(DsonDocument_GetSceneNodeRotationOrder(doc, nodeIndex), "YXZ") == 0;
        }
        allPass = allPass && pass;
        std::cout << "Scene node \"" << item.id << "\" placement: "
                  << (pass ? "PASS" : "FAIL") << "\n";
    }

    int authoredGemIndex = -1;
    for (int i = 0; i < nodeCount; ++i) {
        if (std::strcmp(DsonDocument_GetSceneNodeId(doc, i),
                        "Genesis9_JewelBikini_GemDrop") == 0) {
            authoredGemIndex = i;
            break;
        }
    }
    const bool authoredGemCenterPass = authoredGemIndex >= 0
        && NearlyEqual(DsonDocument_GetSceneNodeCenterPointX(doc, authoredGemIndex), 0.0)
        && NearlyEqual(DsonDocument_GetSceneNodeCenterPointY(doc, authoredGemIndex), 121.4198)
        && NearlyEqual(DsonDocument_GetSceneNodeCenterPointZ(doc, authoredGemIndex), 0.0)
        && DsonDocument_GetSceneNodeCenterPointPresenceMask(doc, authoredGemIndex)
            == DSONPARSER_VECTOR_COMPONENT_Y;
    allPass = allPass && authoredGemCenterPass;
    std::cout << "Gem scene-node authored Y center + presence: "
              << (authoredGemCenterPass ? "PASS" : "FAIL") << "\n";

    const bool invalidPass = std::strcmp(DsonDocument_GetSceneNodeParent(doc, -1), "") == 0
        && DsonDocument_GetSceneNodeTranslationX(doc, -1) == 0.0
        && DsonDocument_GetSceneNodeRotationX(doc, -1) == 0.0
        && DsonDocument_GetSceneNodeScaleX(doc, -1) == 0.0
        && DsonDocument_GetSceneNodeGeneralScale(doc, -1) == 1.0
        && std::strcmp(DsonDocument_GetSceneNodeRotationOrder(doc, -1), "") == 0;
    allPass = allPass && invalidPass;
    std::cout << "Scene-node invalid-input sentinels: " << (invalidPass ? "PASS" : "FAIL") << "\n";

    int gemDrop13Index = -1;
    const int libraryNodeCount = DsonDocument_GetNodeCount(doc);
    for (int i = 0; i < libraryNodeCount; ++i) {
        if (std::strcmp(DsonDocument_GetNodeId(doc, i), "Gem Drop 13") == 0) {
            gemDrop13Index = i;
            break;
        }
    }
    const bool libraryTransformPass = gemDrop13Index >= 0
        && NearlyEqual(DsonDocument_GetNodeTranslationX(doc, gemDrop13Index), -2.057522)
        && NearlyEqual(DsonDocument_GetNodeTranslationY(doc, gemDrop13Index), 81.97084)
        && NearlyEqual(DsonDocument_GetNodeTranslationZ(doc, gemDrop13Index), 7.88664);
    allPass = allPass && libraryTransformPass;
    std::cout << "Library Gem Drop 13 current-value translation: "
              << (libraryTransformPass ? "PASS" : "FAIL") << "\n";
    std::cout << "Scene-node placement test overall: " << (allPass ? "PASS" : "FAIL") << "\n\n";

    DsonDocument_Destroy(doc);

    const std::string genesisPath = ResolveTestFile("Genesis9.json");
    if (genesisPath.empty()) {
        std::cout << "Genesis9.json not found; skipping base-figure transform regression test.\n\n";
        return;
    }

    DsonDocumentHandle genesisDoc = DsonDocument_Create();
    if (!genesisDoc) {
        std::cout << "Failed to create document; skipping base-figure transform regression test.\n\n";
        return;
    }
    if (DsonDocument_LoadFromFile(genesisDoc, genesisPath.c_str()) != 0) {
        std::cout << "Genesis9.json failed to load for base-figure transform regression: FAIL\n\n";
        DsonDocument_Destroy(genesisDoc);
        return;
    }

    int hipIndex = -1;
    const int genesisNodeCount = DsonDocument_GetNodeCount(genesisDoc);
    for (int i = 0; i < genesisNodeCount; ++i) {
        if (std::strcmp(DsonDocument_GetNodeId(genesisDoc, i), "hip") == 0) {
            hipIndex = i;
            break;
        }
    }
    const bool baseFigurePass = hipIndex >= 0
        && NearlyEqual(DsonDocument_GetNodeCenterPointX(genesisDoc, hipIndex), 0.0)
        && NearlyEqual(DsonDocument_GetNodeCenterPointY(genesisDoc, hipIndex), 97.13693)
        && NearlyEqual(DsonDocument_GetNodeCenterPointZ(genesisDoc, hipIndex), 0.4865389);
    std::cout << "Genesis9 hip value-only center_point regression: "
              << (baseFigurePass ? "PASS" : "FAIL") << "\n\n";
    DsonDocument_Destroy(genesisDoc);
}

static void RunRigidFollowRigidityTest()
{
    std::cout << "=================================\n";
    std::cout << "RIGID-FOLLOW RIGIDITY TEST (2.8.0)\n";
    std::cout << "=================================\n\n";

    const std::string filepath = ResolveTestFile("JB Jewel Bikini Bottom And Wrap.duf");
    DsonDocumentHandle doc = DsonDocument_Create();
    if (filepath.empty() || !doc || DsonDocument_LoadFromFile(doc, filepath.c_str()) != 0) {
        std::cout << "Rigid-follow fixture load: FAIL";
        if (doc) std::cout << " (" << DsonParser_GetLastError() << ")";
        std::cout << "\n\n";
        if (doc) DsonDocument_Destroy(doc);
        return;
    }

    static const int kExpectedCounts[] = { 33, 40, 40, 39, 4, 4, 4, 6, 6, 6, 15, 4, 4, 4, 4 };
    static const int kFirstVertices[] = { 9241, 9242, 9243, 9244, 9245 };
    const int expectedNodeCount = static_cast<int>(sizeof(kExpectedCounts) / sizeof(kExpectedCounts[0]));
    int rigidNodeCount = 0;
    int totalVertices = 0;
    bool fieldsPass = true;
    bool countsPass = true;
    bool firstVerticesPass = true;

    const int nodeCount = DsonDocument_GetNodeCount(doc);
    for (int nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
        if (!DsonDocument_GetNodeHasRigidFollow(doc, nodeIndex)) continue;

        const int refCount = DsonDocument_GetNodeRigidFollowReferenceVertexCount(doc, nodeIndex);
        const int modeCount = DsonDocument_GetNodeRigidFollowScaleModeCount(doc, nodeIndex);
        std::cout << "  [" << rigidNodeCount << "] node=" << DsonDocument_GetNodeId(doc, nodeIndex)
                  << " rotation=" << DsonDocument_GetNodeRigidFollowRotationMode(doc, nodeIndex)
                  << " scale_modes=[";
        for (int modeIndex = 0; modeIndex < modeCount; ++modeIndex) {
            if (modeIndex > 0) std::cout << ",";
            const char* mode = DsonDocument_GetNodeRigidFollowScaleMode(doc, nodeIndex, modeIndex);
            std::cout << mode;
            fieldsPass = fieldsPass && std::strcmp(mode, "none") == 0;
        }
        std::cout << "] reference_vertices=" << refCount << "\n";

        fieldsPass = fieldsPass
            && std::strcmp(DsonDocument_GetNodeRigidFollowRotationMode(doc, nodeIndex), "full") == 0
            && modeCount == 3;
        countsPass = countsPass
            && rigidNodeCount < expectedNodeCount
            && refCount == kExpectedCounts[rigidNodeCount];
        if (rigidNodeCount == 0) {
            for (int i = 0; i < static_cast<int>(sizeof(kFirstVertices) / sizeof(kFirstVertices[0])); ++i) {
                firstVerticesPass = firstVerticesPass
                    && DsonDocument_GetNodeRigidFollowReferenceVertex(doc, nodeIndex, i) == kFirstVertices[i];
            }
        }
        totalVertices += refCount;
        ++rigidNodeCount;
    }

    const bool sentinelsPass = !DsonDocument_GetNodeHasRigidFollow(doc, -1)
        && std::strcmp(DsonDocument_GetNodeRigidFollowRotationMode(doc, -1), "") == 0
        && DsonDocument_GetNodeRigidFollowScaleModeCount(doc, -1) == 0
        && std::strcmp(DsonDocument_GetNodeRigidFollowScaleMode(doc, -1, 0), "") == 0
        && DsonDocument_GetNodeRigidFollowReferenceVertexCount(doc, -1) == 0
        && DsonDocument_GetNodeRigidFollowReferenceVertex(doc, -1, 0) == -1;
    const bool allPass = rigidNodeCount == expectedNodeCount && totalVertices == 213
        && countsPass && fieldsPass && firstVerticesPass && sentinelsPass;

    std::cout << "Rigid-follow nodes=" << rigidNodeCount << " [expect 15], total references="
              << totalVertices << " [expect 213]\n";
    std::cout << "Counts: " << (countsPass ? "PASS" : "FAIL")
              << "; modes: " << (fieldsPass ? "PASS" : "FAIL")
              << "; first vertices: " << (firstVerticesPass ? "PASS" : "FAIL")
              << "; sentinels: " << (sentinelsPass ? "PASS" : "FAIL") << "\n";
    std::cout << "Rigid-follow rigidity overall: " << (allPass ? "PASS" : "FAIL") << "\n\n";
    DsonDocument_Destroy(doc);
}

static bool RunGeometryRigidityTest()
{
    std::cout << "=================================\n";
    std::cout << "GEOMETRY RIGIDITY TEST (2.10.0)\n";
    std::cout << "=================================\n\n";

    static const char kRigidityJson[] =
        "{\"geometry_library\":["
        "{\"id\":\"populated\",\"rigidity\":{"
          "\"weights\":{\"count\":99,\"values\":[[7,0.125],[\"bad\",0.5],[9,0.875],[11,\"bad\"],null]},"
          "\"groups\":["
            "{\"id\":\"first\",\"rotation_mode\":\"full\","
             "\"scale_modes\":[\"primary\",\"secondary\",\"none\"],"
             "\"reference_vertices\":{\"count\":3,\"values\":[2,4,\"bad\",6]},"
             "\"mask_vertices\":[8,10,null],\"reference\":\"#pelvis\","
             "\"transform_nodes\":[\"#Gen1\",7,\"#Gen2\"],"
             "\"use_tranform_bones_for_scale\":true},"
            "\"malformed-group\","
            "{\"id\":\"second\",\"rotation_mode\":\"none\",\"scale_modes\":[],"
             "\"reference_vertices\":[],\"mask_vertices\":[],\"transform_nodes\":[],"
             "\"use_transform_bones_for_scale\":true}"
          "]}},"
        "{\"id\":\"authored-empty\",\"rigidity\":{}},"
        "{\"id\":\"absent\"}"
        "]}";

    DsonDocumentHandle doc = DsonDocument_Create();
    if (!doc || DsonDocument_LoadFromString(doc, kRigidityJson) != 0) {
        std::cout << "Geometry rigidity fixture load: FAIL";
        if (doc) std::cout << " (" << DsonParser_GetLastError() << ")";
        std::cout << "\n\n";
        if (doc) DsonDocument_Destroy(doc);
        return false;
    }

    const bool presencePass = DsonDocument_GetGeometryHasRigidity(doc, 0)
        && DsonDocument_GetGeometryHasRigidity(doc, 1)
        && !DsonDocument_GetGeometryHasRigidity(doc, 2);
    const bool weightsPass = DsonDocument_GetGeometryRigidityWeightCount(doc, 0) == 2
        && DsonDocument_GetGeometryRigidityWeightVertexIndex(doc, 0, 0) == 7
        && NearlyEqual(DsonDocument_GetGeometryRigidityWeight(doc, 0, 0), 0.125)
        && DsonDocument_GetGeometryRigidityWeightVertexIndex(doc, 0, 1) == 9
        && NearlyEqual(DsonDocument_GetGeometryRigidityWeight(doc, 0, 1), 0.875);
    const bool firstGroupPass = DsonDocument_GetGeometryRigidityGroupCount(doc, 0) == 2
        && std::strcmp(DsonDocument_GetGeometryRigidityGroupId(doc, 0, 0), "first") == 0
        && std::strcmp(DsonDocument_GetGeometryRigidityGroupRotationMode(doc, 0, 0), "full") == 0
        && DsonDocument_GetGeometryRigidityGroupScaleModeCount(doc, 0, 0) == 3
        && std::strcmp(DsonDocument_GetGeometryRigidityGroupScaleMode(doc, 0, 0, 0), "primary") == 0
        && std::strcmp(DsonDocument_GetGeometryRigidityGroupScaleMode(doc, 0, 0, 1), "secondary") == 0
        && std::strcmp(DsonDocument_GetGeometryRigidityGroupScaleMode(doc, 0, 0, 2), "none") == 0
        && DsonDocument_GetGeometryRigidityGroupReferenceVertexCount(doc, 0, 0) == 3
        && DsonDocument_GetGeometryRigidityGroupReferenceVertex(doc, 0, 0, 2) == 6
        && DsonDocument_GetGeometryRigidityGroupMaskVertexCount(doc, 0, 0) == 2
        && DsonDocument_GetGeometryRigidityGroupMaskVertex(doc, 0, 0, 1) == 10
        && std::strcmp(DsonDocument_GetGeometryRigidityGroupReference(doc, 0, 0), "#pelvis") == 0
        && DsonDocument_GetGeometryRigidityGroupTransformNodeCount(doc, 0, 0) == 2
        && std::strcmp(DsonDocument_GetGeometryRigidityGroupTransformNode(doc, 0, 0, 1), "#Gen2") == 0
        && DsonDocument_GetGeometryRigidityGroupUseTransformBonesForScale(doc, 0, 0);
    const bool secondGroupPass = std::strcmp(DsonDocument_GetGeometryRigidityGroupId(doc, 0, 1), "second") == 0
        && std::strcmp(DsonDocument_GetGeometryRigidityGroupRotationMode(doc, 0, 1), "none") == 0
        && DsonDocument_GetGeometryRigidityGroupScaleModeCount(doc, 0, 1) == 0
        && DsonDocument_GetGeometryRigidityGroupReferenceVertexCount(doc, 0, 1) == 0
        && DsonDocument_GetGeometryRigidityGroupMaskVertexCount(doc, 0, 1) == 0
        && DsonDocument_GetGeometryRigidityGroupTransformNodeCount(doc, 0, 1) == 0
        && !DsonDocument_GetGeometryRigidityGroupUseTransformBonesForScale(doc, 0, 1);

    // Exercise the sentinel of every function in the new family.
    const bool sentinelsPass = !DsonDocument_GetGeometryHasRigidity(nullptr, 0)
        && DsonDocument_GetGeometryRigidityWeightCount(doc, -1) == 0
        && DsonDocument_GetGeometryRigidityWeightVertexIndex(doc, 0, 2) == -1
        && NearlyEqual(DsonDocument_GetGeometryRigidityWeight(doc, 0, 2), 0.0)
        && DsonDocument_GetGeometryRigidityGroupCount(doc, -1) == 0
        && std::strcmp(DsonDocument_GetGeometryRigidityGroupId(doc, 0, 2), "") == 0
        && std::strcmp(DsonDocument_GetGeometryRigidityGroupRotationMode(doc, 0, 2), "") == 0
        && DsonDocument_GetGeometryRigidityGroupScaleModeCount(doc, 0, 2) == 0
        && std::strcmp(DsonDocument_GetGeometryRigidityGroupScaleMode(doc, 0, 0, 3), "") == 0
        && DsonDocument_GetGeometryRigidityGroupReferenceVertexCount(doc, 0, 2) == 0
        && DsonDocument_GetGeometryRigidityGroupReferenceVertex(doc, 0, 0, 3) == -1
        && DsonDocument_GetGeometryRigidityGroupMaskVertexCount(doc, 0, 2) == 0
        && DsonDocument_GetGeometryRigidityGroupMaskVertex(doc, 0, 0, 2) == -1
        && std::strcmp(DsonDocument_GetGeometryRigidityGroupReference(doc, 0, 2), "") == 0
        && DsonDocument_GetGeometryRigidityGroupTransformNodeCount(doc, 0, 2) == 0
        && std::strcmp(DsonDocument_GetGeometryRigidityGroupTransformNode(doc, 0, 0, 2), "") == 0
        && !DsonDocument_GetGeometryRigidityGroupUseTransformBonesForScale(doc, 0, 2);
    bool proofAssetPass = true;
    bool proofAssetTested = false;
    const std::string proofPath = ResolveTestFile(
        "D:/Daz_content/data/DAZ 3D/Genesis 9/Anatomical Elements Male/Genesis9MaleGenitalia.dsf");
    if (!proofPath.empty()) {
        proofAssetTested = true;
        DsonDocumentHandle proof = DsonDocument_Create();
        proofAssetPass = proof && DsonDocument_LoadFromFile(proof, proofPath.c_str()) == 0
            && DsonDocument_GetGeometryHasRigidity(proof, 0)
            && DsonDocument_GetGeometryRigidityWeightCount(proof, 0) == 1354
            && DsonDocument_GetGeometryRigidityGroupCount(proof, 0) == 1
            && std::strcmp(DsonDocument_GetGeometryRigidityGroupId(proof, 0, 0), "Gens") == 0
            && std::strcmp(DsonDocument_GetGeometryRigidityGroupRotationMode(proof, 0, 0), "none") == 0
            && DsonDocument_GetGeometryRigidityGroupScaleModeCount(proof, 0, 0) == 3
            && std::strcmp(DsonDocument_GetGeometryRigidityGroupScaleMode(proof, 0, 0, 0), "primary") == 0
            && std::strcmp(DsonDocument_GetGeometryRigidityGroupScaleMode(proof, 0, 0, 1), "primary") == 0
            && std::strcmp(DsonDocument_GetGeometryRigidityGroupScaleMode(proof, 0, 0, 2), "primary") == 0
            && DsonDocument_GetGeometryRigidityGroupReferenceVertexCount(proof, 0, 0) == 26
            && DsonDocument_GetGeometryRigidityGroupMaskVertexCount(proof, 0, 0) == 1322
            && std::strcmp(DsonDocument_GetGeometryRigidityGroupReference(proof, 0, 0), "#pelvis") == 0
            && DsonDocument_GetGeometryRigidityGroupTransformNodeCount(proof, 0, 0) == 9
            && std::strcmp(DsonDocument_GetGeometryRigidityGroupTransformNode(proof, 0, 0, 0), "#Gen1") == 0
            && std::strcmp(DsonDocument_GetGeometryRigidityGroupTransformNode(proof, 0, 0, 8), "#lTeste") == 0
            && DsonDocument_GetGeometryRigidityGroupUseTransformBonesForScale(proof, 0, 0);
        if (proof) DsonDocument_Destroy(proof);
    }

    const bool allPass = presencePass && weightsPass && firstGroupPass
        && secondGroupPass && sentinelsPass && proofAssetPass;

    std::cout << "presence: " << (presencePass ? "PASS" : "FAIL")
              << "; weights/malformed rows: " << (weightsPass ? "PASS" : "FAIL")
              << "; groups/order/fields: " << ((firstGroupPass && secondGroupPass) ? "PASS" : "FAIL")
              << "; all sentinels: " << (sentinelsPass ? "PASS" : "FAIL") << "\n";
    std::cout << "real Genesis9MaleGenitalia proof: "
              << (proofAssetTested ? (proofAssetPass ? "PASS" : "FAIL") : "not installed (synthetic remains sufficient)")
              << "\n";
    std::cout << "Geometry rigidity overall: " << (allPass ? "PASS" : "FAIL") << "\n\n";
    DsonDocument_Destroy(doc);
    return allPass;
}

int main(int argc, char* argv[])
{
    if (argc == 2 && std::strcmp(argv[1], "--rigidity-regression") == 0) {
        return RunGeometryRigidityTest() ? 0 : 1;
    }

    std::cout << "DSON Parser Test\n";
    std::cout << "================\n\n";

    // Display current working directory
    std::cout << "Current working directory: " << GetWorkingDirectory() << "\n\n";

    RunVersionTest();
    RunGzipFixtureTests();
    RunImageMapSizeTest();
    RunImageLayerCompositingTest();
    RunChannelLayerCompositingTest();
    RunSceneAnimationsTest();
    RunCatalogPresentationTests();
    RunChannelTypeMismatchTests();
    RunModifierPushOffsetTest();
    RunPerThreadLastErrorTest();
    RunSceneNodeAuthoredFieldsTest();
    RunSceneNodePlacementTest();
    RunRigidFollowRigidityTest();
    RunGeometryRigidityTest();

    // Create a DSON document
    DsonDocumentHandle doc = DsonDocument_Create();
    if (!doc) {
        std::cerr << "Failed to create DSON document\n";
        return 1;
    }

    // File to load: first command-line argument, or a default test asset.
    // Always resolved from the TestFiles folder (the argument is a file name).
    const std::string fileName = (argc > 1) ? argv[1] : "Genesis3.duf";
    std::cout << "Looking for: " << fileName << "\n";

    std::string filepath = ResolveTestFile(fileName);
    if (filepath.empty()) {
        std::cerr << "Error: " << fileName << " not found (tried direct path and TestFiles).\n";
        std::cerr << "Current working directory: " << GetWorkingDirectory() << "\n";
        std::cerr << "Usage: DsonTest2 [filepath-or-filename-in-TestFiles]\n";
        DsonDocument_Destroy(doc);
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    if (DsonDocument_LoadFromFile(doc, filepath.c_str()) != 0) {
        std::cerr << "Error loading file: " << DsonParser_GetLastError() << "\n";
        DsonDocument_Destroy(doc);
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::ifstream ifs("D:/Daz_content/People/Genesis 9/Genesis 9.duf");
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    printf("File content length: %zu\n", content.size());

    DsonDocumentHandle doc2 = DsonDocument_Create();
    int result = DsonDocument_LoadFromString(doc2, content.c_str());
    printf("LoadFromString result: %d\n", result);
    printf("LastError: '%s'\n", DsonParser_GetLastError());
    DsonDocument_Destroy(doc2);

    // Display file information
    std::cout << "File Version: " << DsonDocument_GetFileVersion(doc) << "\n";
    std::cout << "Asset ID: " << DsonDocument_GetAssetId(doc) << "\n";
    std::cout << "Asset Type: " << DsonDocument_GetAssetType(doc) << "\n\n";

    // Display node information
    int nodeCount = DsonDocument_GetNodeCount(doc);
    std::cout << "Nodes (" << nodeCount << "):\n";
    for (int i = 0; i < nodeCount; i++) {
        std::cout << "  [" << i << "] ID: " << DsonDocument_GetNodeId(doc, i)
                  << ", Name: " << DsonDocument_GetNodeName(doc, i)
                  << ", Type: " << DsonDocument_GetNodeType(doc, i)
                  << ", Center: (" << DsonDocument_GetNodeCenterPointX(doc, i)
                  << ", " << DsonDocument_GetNodeCenterPointY(doc, i)
                  << ", " << DsonDocument_GetNodeCenterPointZ(doc, i) << ")\n";
    }
    std::cout << "\n";

    // Display scene node information (separate from the node library).
    // Scene nodes are instances: show Label and the Url they reference.
    int sceneNodeCount = DsonDocument_GetSceneNodeCount(doc);
    std::cout << "Scene Nodes (" << sceneNodeCount << "):\n";
    for (int i = 0; i < sceneNodeCount; i++) {
        std::cout << "  [" << i << "] ID: " << DsonDocument_GetSceneNodeId(doc, i)
                  << ", Label: " << DsonDocument_GetSceneNodeLabel(doc, i)
                  << ", Url: " << DsonDocument_GetSceneNodeUrl(doc, i) << "\n";
    }
    std::cout << "\n";

    // Display scene modifier instances
    int sceneModCount = DsonDocument_GetSceneModifierCount(doc);
    std::cout << "Scene Modifiers (" << sceneModCount << "):\n";
    for (int i = 0; i < sceneModCount; i++) {
        std::cout << "  [" << i << "] ID: " << DsonDocument_GetSceneModifierId(doc, i)
                  << ", Url: " << DsonDocument_GetSceneModifierUrl(doc, i) << "\n";
    }
    std::cout << "\n";

    // Display scene material instances
    int sceneMatCount = DsonDocument_GetSceneMaterialCount(doc);
    std::cout << "Scene Materials (" << sceneMatCount << "):\n";
    for (int i = 0; i < sceneMatCount; i++) {
        std::cout << "  [" << i << "] ID: " << DsonDocument_GetSceneMaterialId(doc, i)
                  << ", Url: " << DsonDocument_GetSceneMaterialUrl(doc, i) << "\n";
    }
    std::cout << "\n";

    // Display scene UV set instances
    int sceneUvCount = DsonDocument_GetSceneUVSetCount(doc);
    std::cout << "Scene UV Sets (" << sceneUvCount << "):\n";
    for (int i = 0; i < sceneUvCount; i++) {
        std::cout << "  [" << i << "] ID: " << DsonDocument_GetSceneUVSetId(doc, i)
                  << ", Url: " << DsonDocument_GetSceneUVSetUrl(doc, i) << "\n";
    }
    std::cout << "\n";

    // Display geometry information
    int geomCount = DsonDocument_GetGeometryCount(doc);
    std::cout << "Geometries (" << geomCount << "):\n";
    for (int i = 0; i < geomCount; i++) {
        std::cout << "  [" << i << "] ID: " << DsonDocument_GetGeometryId(doc, i)
                  << ", Name: " << DsonDocument_GetGeometryName(doc, i)
                  << ", Vertices: " << DsonDocument_GetGeometryVertexCount(doc, i)
                  << ", Polygons: " << DsonDocument_GetGeometryPolygonCount(doc, i) << "\n";
    }
    std::cout << "\n";

    // Display material information
    int matCount = DsonDocument_GetMaterialCount(doc);
    std::cout << "Materials (" << matCount << "):\n";
    for (int i = 0; i < matCount; i++) {
        std::cout << "  [" << i << "] ID: " << DsonDocument_GetMaterialId(doc, i) << "\n";
        if (i == 0) {
            std::cout << "    type: \"" << DsonDocument_GetMaterialType(doc, i) << "\"  shader_type: \"" << DsonDocument_GetMaterialShaderType(doc, i) << "\"\n";
            std::cout << "    channels (" << DsonDocument_GetMaterialChannelCount(doc, 0) << "):\n";
            for (int c = 0; c < std::min(DsonDocument_GetMaterialChannelCount(doc, 0), 10); c++) {
                std::cout << "      [" << c << "] id=\"" << DsonDocument_GetMaterialChannelId(doc, 0, c)
                          << "\" type=\"" << DsonDocument_GetMaterialChannelType(doc, 0, c) << "\"\n";
            }
        }
    }
    std::cout << "\n";

    // Display modifier information
    int modCount = DsonDocument_GetModifierCount(doc);
    std::cout << "Modifiers (" << modCount << "):\n";
    for (int i = 0; i < modCount; i++) {
        std::cout << "  [" << i << "] ID: " << DsonDocument_GetModifierId(doc, i)
                  << ", Name: " << DsonDocument_GetModifierName(doc, i)
                  << ", Type: " << DsonDocument_GetModifierType(doc, i)
                  << ", SkinJoints: " << DsonDocument_GetModifierSkinJointCount(doc, i)
                  << ", SkinVerts: " << DsonDocument_GetModifierSkinVertexCount(doc, i) << "\n";
    }
    std::cout << "\n";

    // Display image information
    int imgCount = DsonDocument_GetImageCount(doc);
    std::cout << "Images (" << imgCount << "):\n\n";

    // Display UV set information
    int uvCount = DsonDocument_GetUVSetCount(doc);
    std::cout << "UV Sets (" << uvCount << "):\n\n";

    // Display unknown keys report
    std::cout << "=================================\n";
    std::cout << "UNKNOWN KEYS DIAGNOSTIC REPORT\n";
    std::cout << "=================================\n\n";

    int contextCount = DsonDocument_GetContextCount(doc);
    if (contextCount == 0) {
        std::cout << "No unknown keys found! All keys are recognized.\n\n";
    } else {
        std::cout << "Found " << contextCount << " context(s) with unknown keys:\n\n";
        
        for (int i = 0; i < contextCount; i++) {
            const char* context = DsonDocument_GetContextName(doc, i);
            int unknownCount = DsonDocument_GetUnknownKeyCount(doc, context);
            
            std::cout << "Context: " << context << " (" << unknownCount << " unknown key";
            if (unknownCount != 1) std::cout << "s";
            std::cout << ")\n";
            
            for (int j = 0; j < unknownCount; j++) {
                std::cout << "  - " << DsonDocument_GetUnknownKey(doc, context, j) << "\n";
            }
            std::cout << "\n";
        }
    }

    // Shader type tests
    std::cout << "=================================\n";
    std::cout << "SHADER TYPE TESTS\n";
    std::cout << "=================================\n\n";

    // G9.duf mat[0] — expect uber_iray (the shader actually in the fixture)
    std::cout << "G9.duf mat[0]:  \"" << DsonDocument_GetMaterialShaderType(doc, 0) << "\"  [expect studio/material/uber_iray]\n";

    // Out-of-range indices
    std::cout << "mat[-1]:        \"" << DsonDocument_GetMaterialShaderType(doc, -1) << "\"  [expect empty]\n";
    std::cout << "mat[99999]:     \"" << DsonDocument_GetMaterialShaderType(doc, 99999) << "\"  [expect empty]\n\n";

    // pbr_skin via minimal inline JSON
    {
        const char* pbrJson = "{\"material_library\":[{\"id\":\"pbr-mat\",\"extra\":[{\"type\":\"studio/material/pbr_skin\"}]}]}";
        DsonDocumentHandle pbrDoc = DsonDocument_Create();
        DsonDocument_LoadFromString(pbrDoc, pbrJson);
        std::cout << "pbr_skin test:  \"" << DsonDocument_GetMaterialShaderType(pbrDoc, 0) << "\"  [expect studio/material/pbr_skin]\n";
        DsonDocument_Destroy(pbrDoc);
    }

    // Material with no extra block
    {
        const char* noExtraJson = "{\"material_library\":[{\"id\":\"no-extra\"}]}";
        DsonDocumentHandle noExtraDoc = DsonDocument_Create();
        DsonDocument_LoadFromString(noExtraDoc, noExtraJson);
        std::cout << "no-extra test:  \"" << DsonDocument_GetMaterialShaderType(noExtraDoc, 0) << "\"  [expect empty]\n\n";
        DsonDocument_Destroy(noExtraDoc);
    }

    // Bool channel coercion (2.2.1): material channel with type:"bool" value:true -> 1.0, value:false -> 0.0
    {
        const char* boolTrueJson  = "{\"material_library\":[{\"id\":\"m\",\"extra\":[{\"type\":\"studio_material_channels\",\"channels\":[{\"channel\":{\"id\":\"gate\",\"type\":\"bool\",\"value\":true}}]}]}]}";
        const char* boolFalseJson = "{\"material_library\":[{\"id\":\"m\",\"extra\":[{\"type\":\"studio_material_channels\",\"channels\":[{\"channel\":{\"id\":\"gate\",\"type\":\"bool\",\"value\":false}}]}]}]}";
        DsonDocumentHandle trueDoc  = DsonDocument_Create();
        DsonDocumentHandle falseDoc = DsonDocument_Create();
        DsonDocument_LoadFromString(trueDoc,  boolTrueJson);
        DsonDocument_LoadFromString(falseDoc, boolFalseJson);
        double tv = DsonDocument_GetMaterialChannelValue(trueDoc,  0, 0);
        double fv = DsonDocument_GetMaterialChannelValue(falseDoc, 0, 0);
        std::cout << "Mat bool-true  channel_value: " << tv << "  [expect 1.0] " << (tv == 1.0 ? "PASS" : "FAIL") << "\n";
        std::cout << "Mat bool-false channel_value: " << fv << "  [expect 0.0] " << (fv == 0.0 ? "PASS" : "FAIL") << "\n";
        DsonDocument_Destroy(trueDoc);
        DsonDocument_Destroy(falseDoc);
    }

    std::cout << "\nScene materials (" << DsonDocument_GetSceneMaterialCount(doc) << "):\n";
    for (int i = 0; i < DsonDocument_GetSceneMaterialCount(doc); i++) {
        std::cout << "  [" << i << "] id=\"" << DsonDocument_GetSceneMaterialId(doc, i)
                  << "\"  channels=" << DsonDocument_GetSceneMaterialChannelCount(doc, i) << "\n";
    }
    std::cout << "\n";

    // Clean up
    DsonDocument_Destroy(doc);

    TestFormulaValArray();

    std::cout << "Press Enter to exit...";
    std::cin.get();
    return 0;
}
