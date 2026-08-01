#ifndef SCANNER_H
#define SCANNER_H

#include "FileNode.h"
#include <memory>
#include <string>
#include <unordered_set>
#include <functional>

class Scanner {
public:
    // Fonction principale de scan avec callback d'avancement
    static std::unique_ptr<FileNode> scanDirectory(
        const std::string& path,
        std::function<void(const std::string&)> onFolderScanned = nullptr
        );

private:
    static bool isSystemOrProtected(const std::string& path);
};

#endif // SCANNER_H