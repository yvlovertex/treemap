#ifndef SCANNER_H
#define SCANNER_H

#include "FileNode.h"
#include <memory>
#include <string>
#include <unordered_set>
#include <functional>
#include <filesystem>

// cette classe s'occupe de parcourir le disque et de remplir l'arbre
// de FileNode avec ce qu'elle trouve
class Scanner {
public:
    // fonction principale, on donne un chemin (genre "D:/") et ca retourne
    // tout l'arbre scanne. le callback sert juste a dire a l'interface
    // "je suis en train de scanner tel dossier", pour l'affichage pendant
    // que ca tourne (le scan peut prendre plusieurs minutes)
    static std::unique_ptr<FileNode> scanDirectory(
        const std::string& path,
        std::function<void(const std::string&)> onFolderScanned = nullptr
        );

private:
    // pareil que scanDirectory mais avec un compteur de profondeur en plus
    // pour pas partir en boucle infinie si jamais y'a un cycle de dossiers
    static std::unique_ptr<FileNode> scanDirectoryImpl(
        const std::string& path,
        const std::function<void(const std::string&)>& onFolderScanned,
        int depth
        );
    static bool isSystemOrProtected(const std::string& path);
    static bool isReparsePoint(const std::filesystem::path& path);
};

#endif // SCANNER_H
