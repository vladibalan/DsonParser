// DsonTest2.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <algorithm>
#define NOMINMAX
#include <Windows.h>
#include "../DsonParser/DsonParserAPI.h"

#pragma comment(lib, "DsonParser.lib")

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

int main(int argc, char* argv[])
{
    std::cout << "DSON Parser Test\n";
    std::cout << "================\n\n";

    // Display current working directory
    std::cout << "Current working directory: " << GetWorkingDirectory() << "\n\n";

    // Create a DSON document
    DsonDocumentHandle doc = DsonDocument_Create();
    if (!doc) {
        std::cerr << "Failed to create DSON document\n";
        return 1;
    }

    // File to load: first command-line argument, or a default test asset.
    // Always resolved from the TestFiles folder (the argument is a file name).
    const std::string fileName = (argc > 1) ? argv[1] : "G9.duf";
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
