#include "mainwindow.h"
#include "Scanner.h"
#include "TreeMapLayout.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QStorageInfo>
#include <QtConcurrent/QtConcurrent>
#include <QFileInfo>
#include <QDir>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QFont>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QMenu>
#include <QProcess>
#include <QVariant>
#include <QDebug>
#include <QHash>
#include <algorithm>

namespace {

// une categorie de fichier pour la couleur (genre "images", "videos")
// avec la liste des extensions qui vont dedans
struct ColorCategory {
    QString label;
    std::vector<QString> extensions;
    QColor color;
};

const std::vector<ColorCategory>& colorCategories() {
    // toutes les categories qu'on gere, avec leurs extensions et leur couleur
    // c'est pas exhaustif du tout mais ca couvre deja pas mal de cas courants
    static const std::vector<ColorCategory> categories = {
        {"Exécutables/Système", {"exe", "msi", "bat", "cmd", "ps1", "sys", "dll", "com", "sh"}, QColor(76, 175, 80)},
        {"Vidéos", {"mp4", "mkv", "avi", "mov", "wmv", "flv", "webm", "m4v", "mpg", "mpeg"}, QColor(156, 39, 176)},
        {"Audio", {"mp3", "wav", "flac", "aac", "ogg", "m4a", "wma", "mid", "midi"}, QColor(233, 30, 99)},
        {"Images", {"png", "jpg", "jpeg", "gif", "bmp", "webp", "svg", "ico", "tga", "dds", "psd"}, QColor(255, 152, 0)},
        {"Archives", {"zip", "rar", "7z", "tar", "gz", "bz2", "iso", "cab", "xz"}, QColor(244, 67, 54)},
        {"Documents", {"pdf", "txt", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "rtf", "odt", "md", "csv"}, QColor(33, 150, 243)},
        {"Code / Web", {"cpp", "h", "hpp", "c", "py", "js", "ts", "java", "cs", "go", "rs", "php", "html", "css", "json", "xml", "yaml", "yml", "sql"}, QColor(0, 188, 212)},
        {"Données/Config", {"dat", "db", "sqlite", "sqlite3", "log", "ini", "cfg", "conf", "toml", "save", "bin"}, QColor(255, 193, 7)},
        {"Assets de jeu", {"pak", "rpf", "wad", "uasset", "vpk", "bsa", "esp", "esm", "bik", "bnk", "umap", "assets"}, QColor(63, 81, 181)},
        {"Polices", {"ttf", "otf", "woff", "woff2", "fon"}, QColor(139, 195, 74)},
    };
    return categories;
}

const QHash<QString, QColor>& extensionColorMap() {
    // on construit une table extension -> couleur une seule fois (static)
    // comme ca dans getFileColor on cherche direct au lieu de reboucler
    // sur toutes les categories a chaque fois, plus rapide si y'a
    // beaucoup de fichiers a colorer
    static const QHash<QString, QColor> map = [] {
        QHash<QString, QColor> m;
        for (const auto& category : colorCategories()) {
            for (const auto& ext : category.extensions) {
                m.insert(ext, category.color);
            }
        }
        return m;
    }();
    return map;
}

// gris clair et neutre, nettement plus clair que le gris des dossiers
// pour qu'un "fichier pas categorise" (clair) se distingue bien d'un
// "dossier" (fonce), sinon on arrivait plus a les differencier
const QColor kOtherColor(158, 158, 158);
const QColor kFolderColor(70, 72, 80);         // fait expres plus sombre que toutes les couleurs de fichier
const QColor kFolderBorderColor(210, 212, 218);

// le canevas logique est toujours de cette taille, peu importe la taille
// reelle de la fenetre. le calcul du treemap se fait dedans, puis
// QGraphicsView::fitInView() etire tout pour remplir la fenetre.
// comme ca y'a jamais de decalage entre la taille utilisee pour le
// calcul et la taille vraiment affichee (par exemple si on agrandit
// la fenetre apres le premier affichage)
constexpr double kLogicalWidth = 1600.0;
constexpr double kLogicalHeight = 900.0;

// au dela de cette profondeur un dossier est dessine comme un bloc plein
// (pareil qu'un fichier) au lieu d'etre subdivise, sinon l'affichage
// devient illisible sur les arborescences profondes. on peut quand
// meme cliquer dessus pour zoomer dedans, et l'arborescence a gauche
// permet aussi d'aller direct dans un dossier profond
constexpr int kMaxRenderDepth = 3;

QColor getFileColor(const std::string& fileName) {
    QString ext = QFileInfo(QString::fromStdString(fileName)).suffix().toLower();

    const auto& map = extensionColorMap();
    auto it = map.find(ext);
    return it != map.end() ? it.value() : kOtherColor;
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    detectDrives();

    // des que le scan (qui tourne dans un autre thread) a fini, on appelle onScanFinished
    connect(&watcher, &QFutureWatcher<std::unique_ptr<FileNode>>::finished, this, &MainWindow::onScanFinished);
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI() {
    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(centralWidget);

    // la barre du haut, avec le choix du disque et les boutons
    auto *topLayout = new QHBoxLayout();

    topLayout->addWidget(new QLabel("Sélectionner le disque:", this));
    driveComboBox = new QComboBox(this);
    topLayout->addWidget(driveComboBox);

    scanButton = new QPushButton("Lancer le scan", this);
    topLayout->addWidget(scanButton);

    backButton = new QPushButton("← Retour", this);
    backButton->setEnabled(false);
    topLayout->addWidget(backButton);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 0); // pas de vrai pourcentage, juste une animation qui tourne
    progressBar->setVisible(false);
    topLayout->addWidget(progressBar);

    mainLayout->addLayout(topLayout);

    // le chemin du dossier affiche actuellement
    pathLabel = new QLabel(this);
    pathLabel->setStyleSheet("color: palette(mid);");
    mainLayout->addWidget(pathLabel);

    // la legende des couleurs, une ligne avec un petit carre + le nom pour chaque categorie
    auto *legendLayout = new QHBoxLayout();
    for (const auto& category : colorCategories()) {
        auto *swatch = new QLabel(this);
        swatch->setFixedSize(12, 12);
        swatch->setStyleSheet(QString("background-color: %1; border: 1px solid #333;").arg(category.color.name()));
        legendLayout->addWidget(swatch);
        legendLayout->addWidget(new QLabel(category.label, this));
    }
    {
        // "autres" c'est pas une vraie categorie de colorCategories(), on l'ajoute a la main
        auto *swatch = new QLabel(this);
        swatch->setFixedSize(12, 12);
        swatch->setStyleSheet(QString("background-color: %1; border: 1px solid #333;").arg(kOtherColor.name()));
        legendLayout->addWidget(swatch);
        legendLayout->addWidget(new QLabel("Autres", this));
    }
    legendLayout->addStretch();
    mainLayout->addLayout(legendLayout);

    // la barre de statut en bas du haut de la fenetre (texte "pret", "scan en cours" etc)
    statusLabel = new QLabel("Prêt", this);
    mainLayout->addWidget(statusLabel);

    // la zone principale : l'arborescence a gauche, le treemap a droite
    auto *splitter = new QSplitter(Qt::Horizontal, this);

    treeWidget = new QTreeWidget(this);
    treeWidget->setColumnCount(2);
    treeWidget->setHeaderLabels({"Nom", "Taille"});
    treeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    treeWidget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    connect(treeWidget, &QTreeWidget::itemClicked, this, &MainWindow::onTreeItemClicked);
    splitter->addWidget(treeWidget);

    scene = new QGraphicsScene(this);
    view = new QGraphicsView(scene, this);
    // on refait tout l'affichage (scene->clear() puis on redessine tout)
    // ET on change le zoom (fitInView) a chaque fois qu'on affiche. avec
    // le mode de rafraichissement partiel par defaut de qt, ca peut
    // laisser des bouts de l'ancien dessin affiches par erreur (un genre
    // de fantome visuel). un rafraichissement complet evite ce probleme.
    view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    view->viewport()->installEventFilter(this);
    view->viewport()->setMouseTracking(true);
    splitter->addWidget(view);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({260, 840});
    // par defaut un QSplitter a une politique de taille "preferred", pas
    // "expanding". sans forcer ca explicitement, mainLayout partage
    // l'espace en trop a parts egales entre le splitter et les labels du
    // dessus (pathLabel, legende, statusLabel), et tout devient minuscule.
    splitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    mainLayout->addWidget(splitter, 1);

    setCentralWidget(centralWidget);
    resize(1100, 800);

    connect(scanButton, &QPushButton::clicked, this, &MainWindow::startScan);
    connect(backButton, &QPushButton::clicked, this, &MainWindow::navigateBack);
}

void MainWindow::detectDrives() {
    // on remplit la liste deroulante avec tout les disques disponibles
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
    backButton->setEnabled(false);
    progressBar->setVisible(true);
    statusLabel->setText("Scan en cours de : " + selectedPath);
    scene->clear();
    treeWidget->clear();
    nodeToTreeItem.clear();
    currentRoot = nullptr;

    std::string pathStr = selectedPath.toStdString();

    // le scan tourne dans un thread a part, sinon la fenetre gele
    // pendant les (potentiellement) plusieurs minutes que ca prend
    QFuture<FileNode*> future = QtConcurrent::run([this, pathStr]() {
        auto result = Scanner::scanDirectory(pathStr, [this](const std::string& folder) {
            // ce callback est appele depuis le thread de scan, du coup on
            // passe par invokeMethod pour revenir proprement sur le thread
            // principal avant de toucher l'interface
            QMetaObject::invokeMethod(this, "updateProgress", Qt::QueuedConnection,
                                      Q_ARG(QString, QString::fromStdString(folder)));
        });

        // on recupere le pointeur brut et on abandonne le unique_ptr, parce
        // que watcher.result() peut pas nous rendre un unique_ptr directement
        return result.release();
    });

    watcher.setFuture(future);
}

void MainWindow::updateProgress(const QString& currentFolder) {
    statusLabel->setText("Analyse : " + currentFolder);
}

void MainWindow::onScanFinished() {
    // on reprend possession du pointeur recupere plus haut, dans un unique_ptr
    rootNode.reset(watcher.result());

    scanButton->setEnabled(true);
    progressBar->setVisible(false);

    if (!rootNode || rootNode->size == 0) {
        statusLabel->setText("Aucune donnée ou accès refusé.");
        return;
    }

    statusLabel->setText(QString("Scan terminé. Taille totale : %1").arg(formatSize(rootNode->size)));

    currentRoot = rootNode.get();
    backButton->setEnabled(false);
    populateTree();
    rebuildTreemap();
}

QString MainWindow::formatSize(uint64_t bytes) {
    // convertit un nombre d'octets en texte lisible, genre "12.34 Go"
    static const char* units[] = {"o", "Ko", "Mo", "Go", "To"};
    double size = static_cast<double>(bytes);
    int unitIndex = 0;
    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        ++unitIndex;
    }
    return unitIndex == 0
               ? QString("%1 %2").arg(static_cast<qulonglong>(size)).arg(units[unitIndex])
               : QString("%1 %2").arg(size, 0, 'f', 2).arg(units[unitIndex]);
}

QString MainWindow::fullPathOf(FileNode* node) {
    // comme on garde plus le chemin complet dans FileNode (voir filenode.h)
    // on le reconstruit ici en remontant les parent jusqu'a la racine
    if (!node) return QString();

    std::vector<FileNode*> chain;
    for (FileNode* n = node; n != nullptr; n = n->parent) {
        chain.push_back(n);
    }

    // le dernier element de chain c'est la racine, et son name contient deja
    // le chemin complet du disque (genre "D:/"), on redescend en ajoutant les morceaux
    QString path = QString::fromStdString(chain.back()->name);
    for (auto it = chain.rbegin() + 1; it != chain.rend(); ++it) {
        if (!path.endsWith('/') && !path.endsWith('\\')) path += '\\';
        path += QString::fromStdString((*it)->name);
    }
    return path;
}

QString MainWindow::tooltipFor(FileNode* node) {
    // le texte qui s'affiche quand on passe la souris sur un element du treemap
    QString text = fullPathOf(node) + "\n" + formatSize(node->size);
    if (node->parent && node->parent->size > 0) {
        double pct = 100.0 * static_cast<double>(node->size) / static_cast<double>(node->parent->size);
        text += QString(" (%1% du dossier parent)").arg(pct, 0, 'f', 1);
    }
    return text;
}

void MainWindow::rebuildTreemap() {
    if (!currentRoot) return;

    // on jete l'ancien dessin et on recommence entierement a zero
    scene->clear();

    QRectF bounds(0, 0, kLogicalWidth, kLogicalHeight);
    scene->setSceneRect(bounds);

    TreeMapLayout::calculateLayout(currentRoot, bounds);

    for (const auto& child : currentRoot->children) {
        renderNode(child.get(), 1);
    }

    updatePathLabel();
    fitTreemapView();
}

void MainWindow::fitTreemapView() {
    // ajuste juste le zoom pour que tout rentre dans la fenetre, sans
    // refaire le calcul du layout (deja fait dans rebuildTreemap)
    if (view->viewport()->width() <= 0 || view->viewport()->height() <= 0) return;
    if (scene->sceneRect().isEmpty()) return;
    view->fitInView(scene->sceneRect(), Qt::IgnoreAspectRatio);
}

void MainWindow::renderNode(FileNode* node, int depth) {
    if (!node || node->rect.width() <= 1 || node->rect.height() <= 1) return;

    // on garde un pointeur vers le FileNode dans l'item graphique, comme
    // ca on peut retrouver de quel fichier/dossier il s'agit quand on clique dessus
    QVariant nodeData = QVariant::fromValue<qulonglong>(reinterpret_cast<qulonglong>(node));

    bool subdivide = node->isDirectory && !node->children.empty() && depth <= kMaxRenderDepth;

    if (!subdivide) {
        // soit c'est un fichier, soit un dossier trop profond pour etre
        // subdivise davantage : dans les deux cas on dessine juste un bloc plein
        QColor itemColor = node->isDirectory ? kFolderColor : getFileColor(node->name);
        QPen pen(Qt::black, 0.5);
        pen.setCosmetic(true); // garde une epaisseur de trait constante peu importe le zoom
        auto* rectItem = scene->addRect(node->rect, pen, QBrush(itemColor));
        rectItem->setData(0, nodeData);
        rectItem->setToolTip(tooltipFor(node));
        rectItem->setZValue(1);

        if (node->rect.width() > 50 && node->rect.height() > 16) {
            auto* textItem = scene->addText(QString::fromStdString(node->name));
            textItem->setDefaultTextColor(Qt::white);
            textItem->setFont(QFont("Segoe UI", 9, QFont::Bold));
            // garde une taille de texte constante a l'ecran, peu importe le zoom de la vue
            textItem->setFlag(QGraphicsItem::ItemIgnoresTransformations);
            textItem->setPos(node->rect.x() + 2, node->rect.y() + 2);
            textItem->setZValue(2);
        }
        return;
    }

    // dossier avec des enfants qu'on va afficher : on dessine un bandeau
    // de titre avec une bordure, pour bien le distinguer de son contenu
    QPen dirPen(kFolderBorderColor, 1);
    dirPen.setCosmetic(true);
    auto* rectItem = scene->addRect(node->rect, dirPen, QBrush(kFolderColor));
    rectItem->setData(0, nodeData);
    rectItem->setToolTip(tooltipFor(node));
    rectItem->setZValue(0);

    if (node->rect.width() > TreeMapLayout::kHeaderMinBounds && node->rect.height() > TreeMapLayout::kHeaderMinBounds) {
        auto* textItem = scene->addText(QString::fromStdString(node->name));
        textItem->setDefaultTextColor(Qt::white);
        textItem->setFont(QFont("Segoe UI", 9, QFont::Bold));
        textItem->setFlag(QGraphicsItem::ItemIgnoresTransformations);
        textItem->setPos(node->rect.x() + 3, node->rect.y() + 1);
        textItem->setZValue(1);
    }

    // et on refait pareil pour chacun de ses enfants, avec une profondeur en plus
    for (const auto& child : node->children) {
        renderNode(child.get(), depth + 1);
    }
}

FileNode* MainWindow::nodeAt(const QPointF& scenePos) const {
    // retrouve quel FileNode correspond a la position ou l'utilisateur a clique
    QGraphicsItem* item = scene->itemAt(scenePos, view->transform());
    if (!item) return nullptr;
    QVariant data = item->data(0);
    if (!data.isValid()) return nullptr;
    return reinterpret_cast<FileNode*>(data.toULongLong());
}

void MainWindow::navigateTo(FileNode* node) {
    // change le dossier affiche et redessine tout, plus la synchro avec l'arborescence
    if (!node || !currentRoot || node == currentRoot) return;
    currentRoot = node;
    backButton->setEnabled(currentRoot != rootNode.get());
    rebuildTreemap();
    syncTreeSelection(node);
}

void MainWindow::navigateBack() {
    // remonte d'un cran, tout simplement en suivant le pointeur parent
    if (!currentRoot || currentRoot == rootNode.get() || !currentRoot->parent) return;
    navigateTo(currentRoot->parent);
}

void MainWindow::updatePathLabel() {
    if (!currentRoot) {
        pathLabel->setText("");
        return;
    }
    pathLabel->setText(fullPathOf(currentRoot) + "   —   " + formatSize(currentRoot->size));
}

void MainWindow::populateTree() {
    // remplit l'arborescence a gauche a partir de zero, avec tout les dossiers du scan
    treeWidget->clear();
    nodeToTreeItem.clear();
    if (!rootNode) return;

    auto* rootItem = new QTreeWidgetItem(treeWidget);
    rootItem->setText(0, fullPathOf(rootNode.get()));
    rootItem->setText(1, formatSize(rootNode->size));
    rootItem->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(reinterpret_cast<qulonglong>(rootNode.get())));
    nodeToTreeItem[rootNode.get()] = rootItem;

    addTreeChildren(rootItem, rootNode.get());
    rootItem->setExpanded(true);
}

void MainWindow::addTreeChildren(QTreeWidgetItem* parentItem, FileNode* node) {
    // on met que les dossiers dans l'arborescence, pas les fichiers un par un
    // (sinon ca ferait des centaines de milliers d'items pour rien, et ce
    // serait super lent a construire sur un gros disque)
    std::vector<FileNode*> dirs;
    for (auto& child : node->children) {
        if (child->isDirectory && child->size > 0) {
            dirs.push_back(child.get());
        }
    }
    std::sort(dirs.begin(), dirs.end(), [](FileNode* a, FileNode* b) {
        return a->size > b->size;
    });

    for (auto* dir : dirs) {
        auto* item = new QTreeWidgetItem(parentItem);
        item->setText(0, QString::fromStdString(dir->name));
        item->setText(1, formatSize(dir->size));
        item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(reinterpret_cast<qulonglong>(dir)));
        nodeToTreeItem[dir] = item;
        addTreeChildren(item, dir);
    }
}

void MainWindow::syncTreeSelection(FileNode* node) {
    // deplie tout les parents du noeud dans l'arborescence, puis selectionne
    // le bon item, comme ca l'arbre reste synchronise avec ce qu'on voit dans le treemap
    std::vector<FileNode*> chain;
    for (FileNode* n = node; n != nullptr; n = n->parent) {
        chain.push_back(n);
    }
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        auto found = nodeToTreeItem.find(*it);
        if (found != nodeToTreeItem.end()) {
            found.value()->setExpanded(true);
        }
    }

    auto found = nodeToTreeItem.find(node);
    if (found != nodeToTreeItem.end()) {
        treeWidget->setCurrentItem(found.value());
        treeWidget->scrollToItem(found.value());
    }
}

void MainWindow::onTreeItemClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (!item) return;
    QVariant data = item->data(0, Qt::UserRole);
    if (!data.isValid()) return;
    FileNode* node = reinterpret_cast<FileNode*>(data.toULongLong());
    navigateTo(node);
}

void MainWindow::showContextMenu(FileNode* node, const QPoint& globalPos) {
    if (!node) return;

    QMenu menu(this);
    QAction* revealAction = menu.addAction("Afficher dans l'explorateur");
    QAction* chosen = menu.exec(globalPos);

    if (chosen == revealAction) {
        QString nativePath = QDir::toNativeSeparators(fullPathOf(node));
        QStringList args;
        if (!node->isDirectory) {
            // /select, sert a ouvrir explorer directement sur le fichier
            // selectionne, pour un dossier ca sert a rien on l'ouvre juste
            args << "/select,";
        }
        args << nativePath;
        QProcess::startDetached("explorer.exe", args);
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    // on intercepte les clics directement sur la vue graphique, plutot
    // que de passer par les evenements normaux de QGraphicsItem, parce
    // que c'est plus simple pour gerer le clic gauche (naviguer) et le
    // clic droit (menu contextuel) au meme endroit
    if (watched == view->viewport() && event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        QPointF scenePos = view->mapToScene(mouseEvent->pos());
        FileNode* node = nodeAt(scenePos);

        if (mouseEvent->button() == Qt::LeftButton) {
            if (node && node->isDirectory && !node->children.empty()) {
                navigateTo(node);
                return true;
            }
        } else if (mouseEvent->button() == Qt::RightButton) {
            if (node) {
                showContextMenu(node, mouseEvent->globalPosition().toPoint());
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    // pas besoin de refaire le calcul du layout ici, fitInView se
    // contente de re-etirer ce qui a deja ete calcule
    fitTreemapView();
}
