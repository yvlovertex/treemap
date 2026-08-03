#include "TreeMapLayout.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
// le rectangle d'un noeud (FileNode::rect) est jamais remis a zero entre
// deux calculs de layout. du coup si on zoom dans un dossier (il prend
// alors tout le grand canevas) puis qu'on revient en arriere et qu'il
// redevient tout petit, ses enfants garderaient leur ancienne position
// (calculee pour le grand canevas) et on les verrait dessines n'importe
// ou, en fantomes, par dessus le reste. alors on efface bien tout ca
// des qu'on redescend pas dedans dans la passe courante.
void clearDescendantRects(FileNode* node) {
    for (auto& child : node->children) {
        child->rect = QRectF();
        if (child->isDirectory) clearDescendantRects(child.get());
    }
}
}

void TreeMapLayout::calculateLayout(FileNode* node, const QRectF& bounds) {
    if (!node || bounds.width() <= 0 || bounds.height() <= 0) return;

    node->rect = bounds;

    if (!node->isDirectory || node->children.empty()) return;

    // on enleve une petite marge autour, et on garde de la place en haut
    // pour ecrire le nom du dossier (un peu comme dans winDirStat)
    QRectF innerBounds = bounds.adjusted(kMargin, kMargin, -kMargin, -kMargin);
    if (innerBounds.width() > kHeaderMinBounds && innerBounds.height() > kHeaderMinBounds) {
        innerBounds.setTop(innerBounds.top() + kHeaderHeight);
    }
    if (innerBounds.width() <= 1 || innerBounds.height() <= 1) {
        // trop petit pour dessiner quoi que ce soit dedans, on efface les enfants
        clearDescendantRects(node);
        return;
    }

    std::vector<FileNode*> validChildren;
    for (auto& child : node->children) {
        if (child->size > 0) {
            validChildren.push_back(child.get());
        } else {
            // taille 0, ca sert a rien de l'afficher, on efface son rectangle au cas ou
            child->rect = QRectF();
            if (child->isDirectory) clearDescendantRects(child.get());
        }
    }

    // du plus gros au plus petit, sinon l'algo squarify marche pas bien du tout
    std::sort(validChildren.begin(), validChildren.end(), [](FileNode* a, FileNode* b) {
        return a->size > b->size;
    });

    if (validChildren.empty()) return;

    computeLayoutWeights(validChildren, innerBounds.width() * innerBounds.height());

    // squarify va vider validChildren petit a petit pendant qu'il travaille
    // donc on garde une copie a part, pour pouvoir redescendre dans les
    // sous dossiers juste apres
    std::vector<FileNode*> childrenForRecursion = validChildren;

    squarify(validChildren, innerBounds);

    // pour chaque sous dossier on refait exactement pareil avec ses propres enfants
    for (auto* child : childrenForRecursion) {
        if (child->isDirectory && !child->children.empty()) {
            calculateLayout(child, child->rect);
        }
    }
}

void TreeMapLayout::computeLayoutWeights(std::vector<FileNode*>& items, double canvasArea) {
    if (items.empty() || canvasArea <= 0) return;

    // la taille minimum qu'on veut donner a chaque element, histoire
    // qu'on puisse au moins le voir et cliquer dessus. plafonnee pour
    // pas depasser ce que la place dispo peut vraiment donner a tout le monde
    constexpr double kMinSide = 14.0;
    double minArea = kMinSide * kMinSide;
    double maxPossibleMinArea = canvasArea / static_cast<double>(items.size());
    if (minArea > maxPossibleMinArea) minArea = maxPossibleMinArea;

    // cette fois du plus petit au plus grand, pour donner en premier
    // le minimum aux plus petits fichiers
    std::vector<FileNode*> bySize = items;
    std::sort(bySize.begin(), bySize.end(), [](FileNode* a, FileNode* b) {
        return a->size < b->size;
    });

    double remainingArea = canvasArea;
    double remainingWeight = 0;
    for (auto* n : bySize) remainingWeight += static_cast<double>(n->size);

    std::vector<FileNode*> unclamped;
    for (auto* n : bySize) {
        if (remainingWeight <= 0) {
            n->layoutWeight = 0.0;
            continue;
        }
        // la part que cet element aurait normalement eu, juste proportionnel a sa taille
        double proportional = (static_cast<double>(n->size) / remainingWeight) * remainingArea;
        if (proportional < minArea) {
            // trop petit tout seul, on lui donne le minimum et on enleve
            // ca de ce qu'il reste a partager entre les autres
            n->layoutWeight = minArea;
            remainingArea -= minArea;
            remainingWeight -= static_cast<double>(n->size);
        } else {
            unclamped.push_back(n);
        }
    }

    // les elements qu'on a pas bride au minimum se partagent ce qui
    // reste, toujours proportionnellement a leur taille reelle
    for (auto* n : unclamped) {
        n->layoutWeight = remainingWeight > 0
                               ? (static_cast<double>(n->size) / remainingWeight) * remainingArea
                               : 0.0;
    }
}

double TreeMapLayout::worstRatio(const std::vector<FileNode*>& row, double w) {
    if (row.empty() || w <= 0) return 0.0;

    // le pire ratio d'aspect de la rangee, on veut des rectangles le
    // plus carre possible, pas des rectangles tout fins et illisibles
    double sum = 0;
    double maxArea = 0;
    double minArea = std::numeric_limits<double>::max();

    for (auto* node : row) {
        double area = node->layoutArea;
        sum += area;
        if (area > maxArea) maxArea = area;
        if (area < minArea) minArea = area;
    }

    if (sum == 0 || minArea == 0) return std::numeric_limits<double>::max();

    double w2 = w * w;
    double sum2 = sum * sum;

    return std::max((w2 * maxArea) / sum2, sum2 / (w2 * minArea));
}

void TreeMapLayout::squarify(std::vector<FileNode*>& children, QRectF bounds) {
    // children est deja trie du plus gros au plus petit. on le retourne
    // (reverse) pour pouvoir enlever le dernier element en O(1) avec
    // pop_back, plutot que d'enlever le premier avec erase (qui est en
    // O(n), donc lent si y'a beaucoup de fichiers dans le dossier)
    std::reverse(children.begin(), children.end());

    double totalRemainingWeight = 0;
    for (auto* c : children) totalRemainingWeight += c->layoutWeight;

    std::vector<FileNode*> row; // la rangee qu'on est en train de construire
    double w = std::min(bounds.width(), bounds.height());

    while (!children.empty()) {
        FileNode* c = children.back(); // le plus gros qui reste

        double parentArea = bounds.width() * bounds.height();
        c->layoutArea = (totalRemainingWeight > 0)
                             ? (c->layoutWeight / totalRemainingWeight) * parentArea
                             : 0.0;

        // on regarde si ca ameliore ou empire le ratio d'ajouter cet
        // element a la rangee en cours
        std::vector<FileNode*> testRow = row;
        testRow.push_back(c);

        if (row.empty() || worstRatio(row, w) >= worstRatio(testRow, w)) {
            // ca ameliore (ou c'est le premier element), on l'ajoute a la rangee
            children.pop_back();
            row.push_back(c);
        } else {
            // ca empirerait, du coup on arrete la rangee ici et on la pose pour de vrai
            double rowWeight = 0;
            for (auto* r : row) rowWeight += r->layoutWeight;

            layoutRow(row, w, bounds);
            totalRemainingWeight -= rowWeight;
            w = std::min(bounds.width(), bounds.height());
            row.clear();
        }
    }

    // il reste peut etre une derniere rangee pas encore posee, faut pas l'oublier
    if (!row.empty()) layoutRow(row, w, bounds);
}

void TreeMapLayout::layoutRow(const std::vector<FileNode*>& row, double w, QRectF& bounds) {
    double rowArea = 0;
    for (auto* node : row) rowArea += node->layoutArea;

    // si la largeur du rectangle correspond a w on pose la rangee a
    // l'horizontale, sinon a la verticale (des colonnes cote a cote)
    bool horizontal = (bounds.width() == w);
    double rowThickness = (w > 0) ? rowArea / w : 0;

    double offset = 0;
    for (auto* node : row) {
        double nodeLength = (rowThickness > 0) ? node->layoutArea / rowThickness : 0;
        if (horizontal) {
            node->rect = QRectF(bounds.x() + offset, bounds.y(), nodeLength, rowThickness);
        } else {
            node->rect = QRectF(bounds.x(), bounds.y() + offset, rowThickness, nodeLength);
        }
        offset += nodeLength;
    }

    // attention ici, setX/setY de QRectF garde le bord oppose fixe, ca
    // change donc deja width/height tout seul en faisant ca. si on
    // rappelle setWidth/setHeight juste apres, ca retrecit une deuxieme
    // fois pour rien. du coup on reconstruit carrement le rectangle a
    // la main pour etre sur de pas se planter
    if (horizontal) {
        bounds = QRectF(bounds.x(), bounds.y() + rowThickness, bounds.width(), bounds.height() - rowThickness);
    } else {
        bounds = QRectF(bounds.x() + rowThickness, bounds.y(), bounds.width() - rowThickness, bounds.height());
    }
}
