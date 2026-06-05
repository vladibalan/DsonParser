// Test harness orientation:
// Console program that exercises the DsonParser C ABI end to end: loads a DSON
// file via DsonParserAPI.h, then queries nodes, geometry, materials, skin, and
// morphs to sanity-check the parser. Links DsonParser.lib. This is a manual
// smoke test / example consumer, not an automated unit test suite.
//

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <vector>
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
// Depending on how the program is launched the working directory is either the
// project dir or the build output dir (x64/Debug), so try both TestFiles
// locations. The direct/absolute path is always tried first.
std::string ResolveTestFile(const std::string& name) {
    const std::string candidates[] = {
        name,                                      // direct or absolute path
        "TestFiles/" + name,                       // cwd = project dir
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

int main(int argc, char* argv[])
{
    std::cout << "DSON Parser Test\n";
    std::cout << "================\n\n";

    // Display current working directory
    std::cout << "Current working directory: " << GetWorkingDirectory() << "\n\n";

    RunGzipFixtureTests();

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

    std::cout << "Press Enter to exit...";
    std::cin.get();
    return 0;
}
