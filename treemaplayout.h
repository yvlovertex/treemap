#ifndef TREEMAPLAYOUT_H
#define TREEMAPLAYOUT_H

#include <QRectF>
#include <vector>
#include "FileNode.h"

class TreeMapLayout {
public:
    static void calculateLayout(FileNode* root, const QRectF& bounds);

private:
    static void squarify(std::vector<FileNode*>& children, std::vector<FileNode*>& row, double w, QRectF& bounds);
    static double worstRatio(const std::vector<FileNode*>& row, double w);
    static void layoutRow(const std::vector<FileNode*>& row, double w, QRectF& bounds);
};

#endif // TREEMAPLAYOUT_H