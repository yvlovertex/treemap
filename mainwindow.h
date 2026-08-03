#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QComboBox>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QTreeWidget>
#include <QFutureWatcher>
#include <QHash>
#include <memory>
#include <vector>
#include "FileNode.h"

// la fenetre principale de l'appli
// gere l'interface, lance le scan, et affiche le resultat en treemap
// plus une arborescence a cote
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void startScan();
    void onScanFinished();
    void updateProgress(const QString& currentFolder);
    void onTreeItemClicked(QTreeWidgetItem* item, int column);

private:
    void setupUI();
    void detectDrives();

    void rebuildTreemap();
    void renderNode(FileNode* node, int depth);
    void navigateTo(FileNode* node);
    void navigateBack();
    void showContextMenu(FileNode* node, const QPoint& globalPos);
    FileNode* nodeAt(const QPointF& scenePos) const;
    void updatePathLabel();
    void fitTreemapView();

    void populateTree();
    void addTreeChildren(QTreeWidgetItem* parentItem, FileNode* node);
    void syncTreeSelection(FileNode* node);

    static QString formatSize(uint64_t bytes);
    static QString tooltipFor(FileNode* node);
    static QString fullPathOf(FileNode* node);

    // les widgets de l'interface
    QComboBox *driveComboBox;
    QPushButton *scanButton;
    QPushButton *backButton;
    QProgressBar *progressBar;
    QLabel *statusLabel;
    QLabel *pathLabel;

    QTreeWidget *treeWidget;
    QGraphicsView *view;
    QGraphicsScene *scene;

    // les donnees et le thread de scan
    std::unique_ptr<FileNode> rootNode;             // la racine de tout l'arbre scanne
    FileNode* currentRoot = nullptr;                 // le dossier affiche actuellement (pas forcement rootNode si on a zoome dedans)
    QHash<FileNode*, QTreeWidgetItem*> nodeToTreeItem; // pour retrouver vite l'item de l'arbre a partir d'un noeud
    QFutureWatcher<FileNode*> watcher;
};

#endif // MAINWINDOW_H
