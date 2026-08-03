// FileNode.h
#ifndef FILENODE_H
#define FILENODE_H

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <QRect>

// represente soit un fichier soit un dossier dans l'arbre qu'on scanne
// on garde pas le chemin complet ici, ca prendrait trop de memoire
// si y'a des millions de fichiers (genre pour un scan de C:), on le
// recalcule a la demande en remontant les parent
struct FileNode {
    std::string name; // juste le nom du fichier/dossier, pas le chemin en entier
    uint64_t size = 0; // taille en octets (pour un dossier c'est la somme des enfants)
    bool isDirectory = false;
    QRectF rect; // le rectangle ou ce noeud est dessine dans le treemap

    double layoutArea = 0.0;   // sert juste pendant le calcul, dans TreeMapLayout::squarify
    double layoutWeight = 0.0; // le poids utilise pour calculer la taille du rectangle
                                // (pas toujours pareil que size, si on a du forcer un minimum)

    FileNode* parent = nullptr;
    std::vector<std::unique_ptr<FileNode>> children;
};

#endif // FILENODE_H
