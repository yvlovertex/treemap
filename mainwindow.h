#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QComboBox>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QFutureWatcher>
#include <memory>
#include "FileNode.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void startScan();
    void onScanFinished();
    void updateProgress(const QString& currentFolder);

private:
    void setupUI();
    void detectDrives();
    void buildTreeMap(FileNode* node, const QRectF& bounds);

    // Widgets UI
    QComboBox *driveComboBox;
    QPushButton *scanButton;
    QProgressBar *progressBar;
    QLabel *statusLabel;

    QGraphicsView *view;
    QGraphicsScene *scene;

    // Données & Threading
    std::unique_ptr<FileNode> rootNode;
    QFutureWatcher<FileNode*> watcher;
};

#endif // MAINWINDOW_H