// Test harness orientation:
// Console program that exercises the DsonParser C ABI end to end: loads a DSON
// file via DsonParserAPI.h, then queries nodes, geometry, materials, skin, and
// morphs to sanity-check the parser. Links DsonParser.lib. This is a manual
// smoke test / example consumer, not an automated unit test suite.
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

int main(int argc, char* argv[])
{
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
    RunPerThreadLastErrorTest();

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
