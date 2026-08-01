#include "Scanner.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

bool Scanner::isSystemOrProtected(const std::string& path) {
    std::string lowerPath = path;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

    // Liste des dossiers/fichiers système à ignorer absolument
    static const std::unordered_set<std::string> protectedPaths = {
        "$recycle.bin",
        "system volume information",
        "pagefile.sys",
        "hiberfil.sys",
        "swapfile.sys",
        "c:\\windows\\system32",
        "c:\\windows\\winsxs"
    };

    for (const auto& prot : protectedPaths) {
        if (lowerPath.find(prot) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::unique_ptr<FileNode> Scanner::scanDirectory(const std::string& path, std::function<void(const std::string&)> onFolderScanned) {
    auto root = std::make_unique<FileNode>();
    root->fullPath = path;
    root->name = fs::path(path).filename().string();
    if (root->name.empty()) root->name = path;
    root->isDirectory = true;

    uint64_t totalSize = 0;

    if (onFolderScanned) {
        onFolderScanned(path);
    }

    try {
        for (const auto& entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied)) {
            std::string entryPath = entry.path().string();

            // Ignorer les dossiers et fichiers protégés
            if (isSystemOrProtected(entryPath) || entry.is_symlink()) {
                continue;
            }

            if (entry.is_directory()) {
                auto childNode = scanDirectory(entryPath, onFolderScanned);
                childNode->parent = root.get();
                totalSize += childNode->size;
                root->children.push_back(std::move(childNode));
            } else if (entry.is_regular_file()) {
                auto childNode = std::make_unique<FileNode>();
                childNode->name = entry.path().filename().string();
                childNode->fullPath = entryPath;
                childNode->size = entry.file_size();
                childNode->isDirectory = false;
                childNode->parent = root.get();

                totalSize += childNode->size;
                root->children.push_back(std::move(childNode));
            }
        }
    } catch (...) {
        // Ignorer les erreurs de droits d'accès
    }

    root->size = totalSize;
    return root;
}