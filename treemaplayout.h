#ifndef TREEMAPLAYOUT_H
#define TREEMAPLAYOUT_H

#include <QRectF>
#include <vector>
#include "FileNode.h"

// s'occupe de calculer ou dessiner chaque dossier et fichier dans le
// treemap. c'est l'algorithme "squarify" qui essaie de faire des
// rectangles les plus carres possible (pas des rectangles tout fins
// et illisibles)
class TreeMapLayout {
public:
    // pose le rectangle de node, et recursivement de tout ses descendants, dans bounds
    static void calculateLayout(FileNode* node, const QRectF& bounds);

    // publiques pour que mainwindow sache ou le bandeau avec le nom du
    // dossier a ete place, et dessine le texte au meme endroit
    static constexpr double kMargin = 1.0;
    static constexpr double kHeaderHeight = 14.0;
    static constexpr double kHeaderMinBounds = 30.0;

private:
    // version avec une boucle, pas recursive. un dossier peut avoir des
    // milliers de fichiers dedans (cache de navigateur, dossier
    // d'installation windows...) et une version recursive classique
    // fait une frame de pile par fichier consomme, ce qui peut faire
    // planter le programme (stack overflow) sur un gros dossier.
    // children est vide petit a petit par la fonction.
    static void squarify(std::vector<FileNode*>& children, QRectF bounds);
    static double worstRatio(const std::vector<FileNode*>& row, double w);
    static void layoutRow(const std::vector<FileNode*>& row, double w, QRectF& bounds);

    // repartit la place disponible (canvasArea) entre les elements de
    // items, proportionnellement a leur taille, mais en garantissant
    // quand meme un minimum de place a chacun (sinon les petits fichiers
    // deviennent juste invisibles a l'ecran a cote des gros)
    static void computeLayoutWeights(std::vector<FileNode*>& items, double canvasArea);
};

#endif // TREEMAPLAYOUT_H
