#include "TreeMapLayout.h"
#include <algorithm>
#include <cmath>

void TreeMapLayout::calculateLayout(FileNode* node, const QRectF& bounds) {
    if (!node || bounds.width() <= 1 || bounds.height() <= 1) return;

    // Filtrer et trier les enfants par taille décroissante (essentiel pour Squarify)
    std::vector<FileNode*> validChildren;
    for (auto& child : node->children) {
        if (child->size > 0) {
            validChildren.push_back(child.get());
        }
    }

    std::sort(validChildren.begin(), validChildren.end(), [](FileNode* a, FileNode* b) {
        return a->size > b->size;
    });

    if (validChildren.empty()) return;

    QRectF currentBounds = bounds;
    std::vector<FileNode*> row;
    double w = std::min(currentBounds.width(), currentBounds.height());

    squarify(validChildren, row, w, currentBounds);

    // Découpe récursive pour les sous-dossiers
    for (auto* child : validChildren) {
        if (child->isDirectory && !child->children.empty()) {
            calculateLayout(child, child->rect);
        }
    }
}

double TreeMapLayout::worstRatio(const std::vector<FileNode*>& row, double w) {
    if (row.empty() || w == 0) return 0.0;

    double sum = 0;
    double maxArea = 0;
    double minArea = std::numeric_limits<double>::max();

    for (auto* node : row) {
        double area = node->rect.width() * node->rect.height(); // Ou basé sur node->size
        sum += area;
        if (area > maxArea) maxArea = area;
        if (area < minArea) minArea = area;
    }

    if (sum == 0 || minArea == 0) return std::numeric_limits<double>::max();

    double w2 = w * w;
    double sum2 = sum * sum;

    return std::max((w2 * maxArea) / sum2, sum2 / (w2 * minArea));
}

void TreeMapLayout::squarify(std::vector<FileNode*>& children, std::vector<FileNode*>& row, double w, QRectF& bounds) {
    if (children.empty()) {
        if (!row.empty()) layoutRow(row, w, bounds);
        return;
    }

    FileNode* c = children.front();

    // Calcul de l'aire relative
    double totalParentSize = 0;
    for (auto* ch : children) totalParentSize += ch->size;
    for (auto* r : row) totalParentSize += r->size;

    double parentArea = bounds.width() * bounds.height();
    c->rect.setWidth((static_cast<double>(c->size) / totalParentSize) * parentArea); // Stockage temporaire de l'aire

    std::vector<FileNode*> testRow = row;
    testRow.push_back(c);

    if (row.empty() || worstRatio(row, w) >= worstRatio(testRow, w)) {
        children.erase(children.begin());
        squarify(children, testRow, w, bounds);
    } else {
        layoutRow(row, w, bounds);
        double newW = std::min(bounds.width(), bounds.height());
        squarify(children, row, newW, bounds);
    }
}

void TreeMapLayout::layoutRow(const std::vector<FileNode*>& row, double w, QRectF& bounds) {
    double rowArea = 0;
    for (auto* node : row) rowArea += node->rect.width(); // On avait stocké l'aire dans width

    bool horizontal = (bounds.width() == w);
    double rowThickness = rowArea / w;

    double offset = 0;
    for (auto* node : row) {
        double nodeLength = node->rect.width() / rowThickness;
        if (horizontal) {
            node->rect = QRectF(bounds.x() + offset, bounds.y(), nodeLength, rowThickness);
        } else {
            node->rect = QRectF(bounds.x(), bounds.y() + offset, rowThickness, nodeLength);
        }
        offset += nodeLength;
    }

    if (horizontal) {
        bounds.setY(bounds.y() + rowThickness);
        bounds.setHeight(bounds.height() - rowThickness);
    } else {
        bounds.setX(bounds.x() + rowThickness);
        bounds.setWidth(bounds.width() - rowThickness);
    }
}