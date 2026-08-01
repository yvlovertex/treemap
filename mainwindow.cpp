#include "mainwindow.h"
#include "Scanner.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStorageInfo>
#include <QtConcurrent/QtConcurrent>
#include <QFileInfo>
#include <QGraphicsTextItem>
#include <QFont>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    detectDrives();

    // Connexion de la fin du scan asynchrone
    connect(&watcher, &QFutureWatcher<std::unique_ptr<FileNode>>::finished, this, &MainWindow::onScanFinished);
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI() {
    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    // Barre d'outils supérieure
    auto *topLayout = new QHBoxLayout();

    topLayout->addWidget(new QLabel("Sélectionner le disque:", this));
    driveComboBox = new QComboBox(this);
    topLayout->addWidget(driveComboBox);

    scanButton = new QPushButton("Lancer le scan", this);
    topLayout->addWidget(scanButton);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 0); // Animation indéterminée
    progressBar->setVisible(false);
    topLayout->addWidget(progressBar);

    mainLayout->addLayout(topLayout);

    // Barre de statut
    statusLabel = new QLabel("Prêt", this);
    mainLayout->addWidget(statusLabel);

    // Zone d'affichage Treemap
    scene = new QGraphicsScene(this);
    view = new QGraphicsView(scene, this);
    mainLayout->addWidget(view);

    setCentralWidget(centralWidget);
    resize(1100, 800);

    connect(scanButton, &QPushButton::clicked, this, &MainWindow::startScan);
}

void MainWindow::detectDrives() {
    driveComboBox->clear();
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (storage.isValid() && storage.isReady()) {
            QString name = storage.displayName().isEmpty() ? storage.rootPath() : storage.displayName();
            driveComboBox->addItem(QString("%1 (%2)").arg(name, storage.rootPath()), storage.rootPath());
        }
    }
}

void MainWindow::startScan() {
    QString selectedPath = driveComboBox->currentData().toString();
    if (selectedPath.isEmpty()) return;

    scanButton->setEnabled(false);
    progressBar->setVisible(true);
    statusLabel->setText("Scan en cours de : " + selectedPath);
    scene->clear();

    std::string pathStr = selectedPath.toStdString();

    // Lancement du scan dans un thread en arrière-plan
    QFuture<FileNode*> future = QtConcurrent::run([this, pathStr]() {
        auto result = Scanner::scanDirectory(pathStr, [this](const std::string& folder) {
            QMetaObject::invokeMethod(this, "updateProgress", Qt::QueuedConnection,
                                      Q_ARG(QString, QString::fromStdString(folder)));
        });

        // Transfert de propriété en libérant le unique_ptr
        return result.release();
    });

    watcher.setFuture(future);
}

void MainWindow::updateProgress(const QString& currentFolder) {
    statusLabel->setText("Analyse : " + currentFolder);
}

void MainWindow::onScanFinished() {
    // Récupération du pointeur et réattribution à notre unique_ptr
    rootNode.reset(watcher.result());

    scanButton->setEnabled(true);
    progressBar->setVisible(false);

    if (!rootNode || rootNode->size == 0) {
        statusLabel->setText("Aucune donnée ou accès refusé.");
        return;
    }

    double totalGB = static_cast<double>(rootNode->size) / (1024.0 * 1024.0 * 1024.0);
    statusLabel->setText(QString("Scan terminé. Taille totale : %1 Go").arg(totalGB, 0, 'f', 2));

    // Dessin de la TreeMap
    QRectF totalBounds(0, 0, view->width() - 20, view->height() - 20);
    scene->setSceneRect(totalBounds);
    buildTreeMap(rootNode.get(), totalBounds);
}

QColor getFileColor(const std::string& filePath) {
    QString ext = QFileInfo(QString::fromStdString(filePath)).suffix().toLower();

    if (ext == "exe" || ext == "msi" || ext == "bat") return QColor(76, 175, 80);    // Vert : Executables
    if (ext == "mp4" || ext == "mkv" || ext == "avi") return QColor(156, 39, 176);   // Violet : Vidéos
    if (ext == "mp3" || ext == "wav" || ext == "flac") return QColor(233, 30, 99);   // Rose : Audio
    if (ext == "png" || ext == "jpg" || ext == "jpeg") return QColor(255, 152, 0);   // Orange : Images
    if (ext == "zip" || ext == "rar" || ext == "7z")  return QColor(244, 67, 54);    // Rouge : Archives
    if (ext == "pdf" || ext == "txt" || ext == "doc")  return QColor(33, 150, 243);   // Bleu : Docs
    if (ext == "cpp" || ext == "h" || ext == "py")     return QColor(0, 188, 212);   // Cyan : Code

    return QColor(120, 144, 156);
}

void MainWindow::buildTreeMap(FileNode* node, const QRectF& bounds) {
    if (!node || bounds.width() <= 2 || bounds.height() <= 2) return;

    if (!node->isDirectory || node->children.empty()) {
        QColor itemColor = getFileColor(node->fullPath);

        auto rectItem = scene->addRect(bounds, QPen(Qt::black, 0.5), QBrush(itemColor));

        if (bounds.width() > 50 && bounds.height() > 25) {
            QString labelText = QString::fromStdString(node->name);
            auto textItem = scene->addText(labelText);
            textItem->setDefaultTextColor(Qt::white);

            int fontSize = std::min(static_cast<int>(bounds.height() / 4), 12);
            fontSize = std::max(fontSize, 8);
            textItem->setFont(QFont("Segoe UI", fontSize, QFont::Bold));
            textItem->setPos(bounds.x() + 2, bounds.y() + 2);
            textItem->setTextWidth(bounds.width() - 4);
        }
        return;
    }

    bool splitHorizontally = bounds.width() > bounds.height();
    double currentPos = splitHorizontally ? bounds.x() : bounds.y();

    for (const auto& child : node->children) {
        if (child->size == 0) continue;

        double ratio = static_cast<double>(child->size) / node->size;
        QRectF childBounds;

        if (splitHorizontally) {
            double childWidth = bounds.width() * ratio;
            childBounds = QRectF(currentPos, bounds.y(), childWidth, bounds.height());
            currentPos += childWidth;
        } else {
            double childHeight = bounds.height() * ratio;
            childBounds = QRectF(bounds.x(), currentPos, bounds.width(), childHeight);
            currentPos += childHeight;
        }

        buildTreeMap(child.get(), childBounds);
    }
}