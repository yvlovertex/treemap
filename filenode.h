// FileNode.h
#ifndef FILENODE_H
#define FILENODE_H

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <QRect>

struct FileNode {
    std::string name;
    std::string fullPath;
    uint64_t size = 0;
    bool isDirectory = false;
    QRectF rect;

    FileNode* parent = nullptr;
    std::vector<std::unique_ptr<FileNode>> children;
};

#endif // FILENODE_H