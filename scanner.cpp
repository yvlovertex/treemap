#include "Scanner.h"
#include <filesystem>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {
// limite de securite. si jamais on tombe sur un cycle de dossiers (des
// jonctions windows par exemple, ca arrive plus souvent qu'on croit) on
// s'arrete au bout d'un moment plutot que de scanner a l'infini
constexpr int kMaxDepth = 256;
}

bool Scanner::isSystemOrProtected(const std::string& path) {
    std::string lowerPath = path;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

    // la liste des trucs qu'on scanne jamais
    // soit parce que windows nous laisse pas rentrer dedans de toute facon
    // soit parce que ca sert a rien pour l'utilisateur (fichier de pagination etc)
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

bool Scanner::isReparsePoint(const fs::path& path) {
#ifdef _WIN32
    // is_symlink de std::filesystem detecte pas toujours les jonctions
    // windows (par exemple certains dossiers dans onedrive ou appdata),
    // et ca peut nous faire tourner en rond pour rien. du coup on verifie
    // directement l'attribut windows a la place.
    // on utilise la version W (large, pas ansi) sinon ca peut planter ou
    // se tromper sur un nom de fichier avec des caracteres bizarres
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT);
#else
    (void)path;
    return false;
#endif
}

std::unique_ptr<FileNode> Scanner::scanDirectory(const std::string& path, std::function<void(const std::string&)> onFolderScanned) {
    return scanDirectoryImpl(path, onFolderScanned, 0);
}

std::unique_ptr<FileNode> Scanner::scanDirectoryImpl(const std::string& path, const std::function<void(const std::string&)>& onFolderScanned, int depth) {
    auto root = std::make_unique<FileNode>();
    root->name = fs::path(path).filename().string();
    if (root->name.empty()) root->name = path; // pour la racine (genre "D:/") filename() renvoie rien
    root->isDirectory = true;

    uint64_t totalSize = 0;

    if (onFolderScanned) {
        onFolderScanned(path);
    }

    if (depth >= kMaxDepth) {
        // trop profond, on arrete la et on remonte vide plutot que planter
        root->size = 0;
        return root;
    }

    try {
        for (const auto& entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied)) {
            // si une seule entree pose probleme (nom pas lisible, fichier
            // supprime entre temps par exemple) ca doit pas arreter tout
            // le reste du dossier. du coup on catch juste ici et on continue
            try {
                const fs::path& entryFsPath = entry.path();

                if (isReparsePoint(entryFsPath) || entry.is_symlink()) {
                    continue;
                }

                std::string entryPath = entryFsPath.string();
                if (isSystemOrProtected(entryPath)) {
                    continue;
                }

                if (entry.is_directory()) {
                    // on se rappelle nous meme pour descendre dans le sous dossier
                    auto childNode = scanDirectoryImpl(entryPath, onFolderScanned, depth + 1);
                    childNode->parent = root.get();
                    totalSize += childNode->size;
                    root->children.push_back(std::move(childNode));
                } else if (entry.is_regular_file()) {
                    auto childNode = std::make_unique<FileNode>();
                    childNode->name = entryFsPath.filename().string();
                    childNode->size = entry.file_size();
                    childNode->isDirectory = false;
                    childNode->parent = root.get();

                    totalSize += childNode->size;
                    root->children.push_back(std::move(childNode));
                }
            } catch (...) {
                // entree illisible ou instable, on saute juste celle la et on continue
                continue;
            }
        }
    } catch (...) {
        // dossier pas accessible (droits admin, etc), on garde ce qu'on a deja trouve
    }

    root->size = totalSize;
    return root;
}
