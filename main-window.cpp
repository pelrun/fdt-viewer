#include "main-window.hpp"
#include "ui_main-window.h"

#include <QByteArray>
#include <QDir>
#include <QFile>

#include <fdt-parser.hpp>
#include <iostream>
#include <stack>

using namespace Window;

struct qt_fdt_property {
    QString name;
    QByteArray data;
};

using qt_fdt_properties = QList<qt_fdt_property>;

Q_DECLARE_METATYPE(qt_fdt_property)
Q_DECLARE_METATYPE(qt_fdt_properties)

using string = QString;

#include <endian-conversions.hpp>

using u8 = std::uint8_t;

string present_u32be(const QByteArray &data) {
    string ret;

    auto array = reinterpret_cast<u8 *>(const_cast<char *>(data.data()));
    for (auto i = 0; i < data.size(); ++i) {
        ret += "0x" + QString::number(array[i], 16).rightJustified(2, '0').toUpper() + " ";
    }
    ret.remove(ret.size() - 1, 1);

    return ret;
}

string present(const qt_fdt_property &property) {
    string ret;
    ret += property.name + " = ";
    ret += "<" + present_u32be(property.data) + ">;";
    return ret;
}

MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent)
        , m_ui(std::make_unique<Ui::MainWindow>()) {
    m_ui->setupUi(this);

    m_ui->splitter->setStretchFactor(0, 2);
    m_ui->splitter->setStretchFactor(1, 5);

    auto file_menu = m_ui->menubar->addMenu(tr("&File"));
    auto file_menu_open = new QAction("Open");
    auto file_menu_close = new QAction("Close");
    file_menu->addAction(file_menu_open);
    file_menu->addAction(file_menu_close);
    file_menu_open->setShortcut(QKeySequence::Open);
    file_menu_close->setShortcut(QKeySequence::Close);
    connect(file_menu_open, &QAction::triggered, this, &MainWindow::open_dialog);
    connect(file_menu_close, &QAction::triggered, this, &MainWindow::close);

    connect(m_ui->treeWidget, &QTreeWidget::itemClicked, [this](QTreeWidgetItem *item, auto...) {
        m_ui->textBrowser->clear();
        update_fdt_path(item);

        QVariant values = item->data(0, Qt::UserRole);
        auto properties = values.value<qt_fdt_properties>();
        for (auto &&property : properties) {
            m_ui->textBrowser->append(present(property));
        }
    });
}

#include <QFileDialog>

void MainWindow::open_dialog() {
    const QStringList filters{
        tr("FDT files (*.dtb *.dtbo)"),
        tr("FDT overlay files (*.dtbo)"),
        tr("Any files (*.*)"),
    };

    auto fileName = QFileDialog::getOpenFileName(this,
        tr("Open Flattened Device Tree"), QDir::homePath(), filters.join(";;"));

    if (fileName.isEmpty())
        return;

    open(fileName);
}

bool MainWindow::open(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    auto datamap = file.readAll();

    fdt_generator generator;

    auto root = new QTreeWidgetItem(m_ui->treeWidget);
    root->setText(0, "simple.dtb");

    std::stack<QTreeWidgetItem *> tree_stack;
    tree_stack.emplace(root);

    generator.begin_node = [&tree_stack](std::string_view &&name) {
        auto child = new QTreeWidgetItem(tree_stack.top());
        child->setText(0, QString::fromStdString(name.data()));
        tree_stack.emplace(child);
    };

    generator.end_node = [&tree_stack]() {
        tree_stack.pop();
    };

    generator.insert_property = [&tree_stack](std::string_view &&name, std::string_view &&data) {
        auto current = tree_stack.top();
        QVariant values = current->data(0, Qt::UserRole);
        auto properties = values.value<qt_fdt_properties>();
        qt_fdt_property property;
        property.name = QString::fromStdString(name.data());
        property.data = QByteArray(data.data(), data.size());
        properties << property;
        current->setData(0, Qt::UserRole, QVariant::fromValue(properties));
    };

    fdt_parser parser(datamap.data(), datamap.size(), generator);

    return parser.is_valid();
}

void MainWindow::update_fdt_path(QTreeWidgetItem *item) {
    if (nullptr == item) {
        m_ui->path->clear();
        return;
    }

    QString path = item->text(0);
    auto root = item;
    while (root = root->parent())
        path = root->text(0) + "/" + path;

    m_ui->path->setText("fdt://" + path);
}

MainWindow::~MainWindow() = default;
