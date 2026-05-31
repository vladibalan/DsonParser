// DsonTest2.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
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

    // Load the file
    std::cout << "Loading " << filepath << "...\n";
    if (!DsonDocument_LoadFromFile(doc, filepath.c_str())) {
        std::cerr << "Failed to load file: " << DsonParser_GetLastError() << "\n";
        DsonDocument_Destroy(doc);
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::cout << "Successfully loaded DSON file!\n\n";

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

    // Clean up
    DsonDocument_Destroy(doc);

    std::cout << "Press Enter to exit...";
    std::cin.get();
    return 0;
}
