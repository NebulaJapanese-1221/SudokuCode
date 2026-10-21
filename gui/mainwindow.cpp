#include "mainwindow.h"
#include "gridwidget.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QStatusBar>
#include <QSplitter>
#include <QComboBox>

#include <fstream>
#include "src/sudokucode.h"

using namespace sc;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("SudokuCode");
    resize(1000, 700);
    buildUI();
    setStatusBar(new QStatusBar(this));
    m_statusLabel = new QLabel("Ready");
    statusBar()->addWidget(m_statusLabel);
}

MainWindow::~MainWindow() = default;

static QStringList gridSizeItems() {
    return QStringList() << "4" << "9" << "16";
}

void MainWindow::buildUI() {
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout* root = new QVBoxLayout(central);

    m_tabs = new QTabWidget(this);
    root->addWidget(m_tabs);

    // ============ Encode tab ============
    m_encodeTab = new QWidget(this);
    QVBoxLayout* encLayout = new QVBoxLayout(m_encodeTab);

    QGroupBox* encOpts = new QGroupBox("Encode options", this);
    QGridLayout* encGrid = new QGridLayout(encOpts);

    int r = 0;
    encGrid->addWidget(new QLabel("Input file:", this), r, 0);
    m_inputEdit = new QLineEdit(this);
    QPushButton* btnIn = new QPushButton("Browse...", this);
    QHBoxLayout* inRow = new QHBoxLayout;
    inRow->addWidget(m_inputEdit);
    inRow->addWidget(btnIn);
    encGrid->addLayout(inRow, r, 1, 1, 3);
    connect(btnIn, &QPushButton::clicked, this, &MainWindow::onOpenEncodeInput);
    ++r;

    encGrid->addWidget(new QLabel("Output file:", this), r, 0);
    m_outputEdit = new QLineEdit("output.bmp", this);
    QPushButton* btnOut = new QPushButton("Browse...", this);
    QHBoxLayout* outRow = new QHBoxLayout;
    outRow->addWidget(m_outputEdit);
    outRow->addWidget(btnOut);
    encGrid->addLayout(outRow, r, 1, 1, 3);
    connect(btnOut, &QPushButton::clicked, this, &MainWindow::onSaveEncodeOutput);
    ++r;

    encGrid->addWidget(new QLabel("Grid size (N):", this), r, 0);
    m_gridSizeCombo = new QComboBox(this);
    m_gridSizeCombo->addItems(gridSizeItems());
    m_gridSizeCombo->setCurrentText("9");
    encGrid->addWidget(m_gridSizeCombo, r, 1);
    ++r;

    encGrid->addWidget(new QLabel("ECC level (0-3):", this), r, 0);
    m_eccSpin = new QSpinBox(this);
    m_eccSpin->setRange(0, 3);
    m_eccSpin->setValue(2);
    encGrid->addWidget(m_eccSpin, r, 1);
    ++r;

    m_encryptBox = new QCheckBox("Encrypt (XOR)", this);
    encGrid->addWidget(m_encryptBox, r, 0, 1, 2);
    connect(m_encryptBox, &QCheckBox::toggled, this, &MainWindow::onToggleEncrypt);
    m_keyEdit = new QLineEdit(this);
    m_keyEdit->setPlaceholderText("XOR key");
    m_keyEdit->setEnabled(false);
    encGrid->addWidget(m_keyEdit, r, 2, 1, 2);
    ++r;

    m_compressBox = new QCheckBox("Compress (reserved)", this);
    encGrid->addWidget(m_compressBox, r, 0, 1, 2);
    ++r;

    m_asciiBox = new QCheckBox("Show ASCII preview", this);
    encGrid->addWidget(m_asciiBox, r, 0, 1, 2);
    ++r;

    encGrid->addWidget(new QLabel("Image cell size (px):", this), r, 0);
    m_cellSizeSpin = new QSpinBox(this);
    m_cellSizeSpin->setRange(10, 200);
    m_cellSizeSpin->setValue(30);
    encGrid->addWidget(m_cellSizeSpin, r, 1);
    ++r;

    encLayout->addWidget(encOpts);

    QHBoxLayout* encBtns = new QHBoxLayout;
    m_btnEncode = new QPushButton("Generate SudokuCode", this);
    m_btnExportImage = new QPushButton("Export BMP image...", this);
    m_btnCopyAscii = new QPushButton("Copy ASCII preview", this);
    encBtns->addWidget(m_btnEncode);
    encBtns->addWidget(m_btnExportImage);
    encBtns->addWidget(m_btnCopyAscii);
    encBtns->addStretch();
    encLayout->addLayout(encBtns);

    connect(m_btnEncode, &QPushButton::clicked, this, &MainWindow::onEncode);
    connect(m_btnExportImage, &QPushButton::clicked, this, &MainWindow::onExportImage);
    connect(m_btnCopyAscii, &QPushButton::clicked, this, &MainWindow::onCopyAscii);

    QSplitter* encSplit = new QSplitter(Qt::Horizontal, this);
    m_gridWidget = new GridWidget(this);
    m_gridWidget->setMinimumSize(300, 300);
    encSplit->addWidget(m_gridWidget);

    QWidget* asciiPane = new QWidget(this);
    QVBoxLayout* asciiLayout = new QVBoxLayout(asciiPane);
    asciiLayout->addWidget(new QLabel("ASCII preview:", this));
    m_asciiEdit = new QTextEdit(this);
    m_asciiEdit->setReadOnly(true);
    m_asciiEdit->setFont(QFont("monospace", 10));
    asciiLayout->addWidget(m_asciiEdit);
    encSplit->addWidget(asciiPane);
    encSplit->setStretchFactor(0, 2);
    encSplit->setStretchFactor(1, 1);
    encLayout->addWidget(encSplit);

    m_tabs->addTab(m_encodeTab, "Encode");

    // ============ Decode tab ============
    m_decodeTab = new QWidget(this);
    QVBoxLayout* decLayout = new QVBoxLayout(m_decodeTab);

    QGroupBox* decOpts = new QGroupBox("Decode options", this);
    QGridLayout* decGrid = new QGridLayout(decOpts);

    r = 0;
    decGrid->addWidget(new QLabel("Input file:", this), r, 0);
    m_decodeInputEdit = new QLineEdit(this);
    QPushButton* btnDin = new QPushButton("Browse...", this);
    QHBoxLayout* dinRow = new QHBoxLayout;
    dinRow->addWidget(m_decodeInputEdit);
    dinRow->addWidget(btnDin);
    decGrid->addLayout(dinRow, r, 1, 1, 3);
    connect(btnDin, &QPushButton::clicked, this, &MainWindow::onOpenDecodeInput);
    ++r;

    decGrid->addWidget(new QLabel("Output file:", this), r, 0);
    m_decodeOutputEdit = new QLineEdit("recovered.bin", this);
    QPushButton* btnDout = new QPushButton("Browse...", this);
    QHBoxLayout* doutRow = new QHBoxLayout;
    doutRow->addWidget(m_decodeOutputEdit);
    doutRow->addWidget(btnDout);
    decGrid->addLayout(doutRow, r, 1, 1, 3);
    connect(btnDout, &QPushButton::clicked, this, &MainWindow::onSaveDecodeOutput);
    ++r;

    decGrid->addWidget(new QLabel("Grid size (N):", this), r, 0);
    m_decodeGridSizeCombo = new QComboBox(this);
    m_decodeGridSizeCombo->addItems(gridSizeItems());
    m_decodeGridSizeCombo->setCurrentText("9");
    decGrid->addWidget(m_decodeGridSizeCombo, r, 1);
    ++r;

    decGrid->addWidget(new QLabel("ECC level (0-3):", this), r, 0);
    m_decodeEccSpin = new QSpinBox(this);
    m_decodeEccSpin->setRange(0, 3);
    m_decodeEccSpin->setValue(2);
    decGrid->addWidget(m_decodeEccSpin, r, 1);
    ++r;

    m_decodeEncryptBox = new QCheckBox("Decrypt (XOR)", this);
    decGrid->addWidget(m_decodeEncryptBox, r, 0, 1, 2);
    m_decodeKeyEdit = new QLineEdit(this);
    m_decodeKeyEdit->setPlaceholderText("XOR key");
    decGrid->addWidget(m_decodeKeyEdit, r, 2, 1, 2);
    ++r;

    decGrid->addWidget(new QLabel("Image cell size (px):", this), r, 0);
    m_decodeCellSizeSpin = new QSpinBox(this);
    m_decodeCellSizeSpin->setRange(10, 200);
    m_decodeCellSizeSpin->setValue(30);
    decGrid->addWidget(m_decodeCellSizeSpin, r, 1);
    ++r;

    decLayout->addWidget(decOpts);

    QHBoxLayout* decBtns = new QHBoxLayout;
    m_decodeBtnDecode = new QPushButton("Decode SudokuCode", this);
    m_decodeBtnVerify = new QPushButton("Verify integrity", this);
    decBtns->addWidget(m_decodeBtnDecode);
    decBtns->addWidget(m_decodeBtnVerify);
    decBtns->addStretch();
    decLayout->addLayout(decBtns);

    connect(m_decodeBtnDecode, &QPushButton::clicked, this, &MainWindow::onDecode);
    connect(m_decodeBtnVerify, &QPushButton::clicked, this, [this]{
        // Verify: just attempt a decode; decode() already checks CRC + sudoku.
        onDecode();
    });

    QSplitter* decSplit = new QSplitter(Qt::Horizontal, this);
    QWidget* decGridPane = new QWidget(this);
    QVBoxLayout* decGridLayout = new QVBoxLayout(decGridPane);
    decGridLayout->addWidget(new QLabel("Grid preview:", this));
    m_decodeGridWidget = new GridWidget(this);
    m_decodeGridWidget->setMinimumSize(300, 300);
    decGridLayout->addWidget(m_decodeGridWidget);
    decSplit->addWidget(decGridPane);

    QWidget* decAsciiPane = new QWidget(this);
    QVBoxLayout* decAsciiLayout = new QVBoxLayout(decAsciiPane);
    decAsciiLayout->addWidget(new QLabel("ASCII preview:", this));
    m_decodeAsciiEdit = new QTextEdit(this);
    m_decodeAsciiEdit->setReadOnly(true);
    m_decodeAsciiEdit->setFont(QFont("monospace", 10));
    decAsciiLayout->addWidget(m_decodeAsciiEdit);
    decSplit->addWidget(decAsciiPane);
    decSplit->setStretchFactor(0, 2);
    decSplit->setStretchFactor(1, 1);
    decLayout->addWidget(decSplit);

    m_tabs->addTab(m_decodeTab, "Decode");

    root->addWidget(m_tabs);
}

sc::Params MainWindow::encodeParams() const {
    Params p;
    p.n = m_gridSizeCombo->currentText().toInt();
    p.eccLevel = m_eccSpin->value();
    p.encrypt = m_encryptBox->isChecked();
    p.key = m_keyEdit->text().toStdString();
    p.compress = m_compressBox->isChecked();
    return p;
}

sc::Params MainWindow::decodeParams() const {
    Params p;
    p.n = m_decodeGridSizeCombo->currentText().toInt();
    p.eccLevel = m_decodeEccSpin->value();
    p.encrypt = m_decodeEncryptBox->isChecked();
    p.key = m_decodeKeyEdit->text().toStdString();
    return p;
}

void MainWindow::updateStatus(const QString& msg) {
    if (m_statusLabel) m_statusLabel->setText(msg);
}

void MainWindow::populateCode(const sc::Code& code) {
    if (m_gridWidget) m_gridWidget->setCode(code);
    if (m_asciiEdit) m_asciiEdit->setPlainText(QString::fromStdString(asciiPreview(code)));
    if (m_decodeGridWidget) m_decodeGridWidget->setCode(code);
    if (m_decodeAsciiEdit) m_decodeAsciiEdit->setPlainText(QString::fromStdString(asciiPreview(code)));
}

void MainWindow::onToggleEncrypt(bool checked) {
    if (m_keyEdit) m_keyEdit->setEnabled(checked);
}

void MainWindow::onOpenEncodeInput() {
    QString f = QFileDialog::getOpenFileName(this, "Open input file", QString(), "All files (*.*)");
    if (!f.isEmpty() && m_inputEdit) m_inputEdit->setText(f);
}

void MainWindow::onSaveEncodeOutput() {
    QString f = QFileDialog::getSaveFileName(this, "Save output file", QString(), "BMP Image (*.bmp)");
    if (!f.isEmpty() && m_outputEdit) m_outputEdit->setText(f);
}

void MainWindow::onOpenDecodeInput() {
    QString f = QFileDialog::getOpenFileName(this, "Open input file", QString(), "All files (*.*)");
    if (!f.isEmpty() && m_decodeInputEdit) m_decodeInputEdit->setText(f);
}

void MainWindow::onSaveDecodeOutput() {
    QString f = QFileDialog::getSaveFileName(this, "Save output file", QString(), "All files (*.*)");
    if (!f.isEmpty() && m_decodeOutputEdit) m_decodeOutputEdit->setText(f);
}

void MainWindow::onEncode() {
    if (m_inputEdit->text().isEmpty()) {
        QMessageBox::warning(this, "SudokuCode", "Please select an input file.");
        return;
    }
    Params p = encodeParams();
    std::vector<uint8_t> data;
    {
        std::ifstream ifs(m_inputEdit->text().toStdString(), std::ios::binary);
        if (!ifs) {
            QMessageBox::critical(this, "SudokuCode", "Cannot open input file.");
            return;
        }
        data = std::vector<uint8_t>((std::istreambuf_iterator<char>(ifs)),
                                    std::istreambuf_iterator<char>());
    }

    Code code;
    try {
        encode(data, p, code);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "SudokuCode", QString("Encode failed: %1").arg(e.what()));
        return;
    }

    QString outPath = m_outputEdit->text();
    if (outPath.isEmpty()) outPath = "output.bmp";
    if (!writeBMP(outPath.toStdString(), code, m_cellSizeSpin->value())) {
        QMessageBox::critical(this, "SudokuCode", "Failed to write output image.");
        return;
    }

    populateCode(code);
    updateStatus(QString("Encoded %1 bytes -> %2 (grid %3x%3, ecc=%4)")
                 .arg(data.size()).arg(outPath).arg(p.n).arg(p.eccLevel));
}

void MainWindow::onDecode() {
    if (m_decodeInputEdit->text().isEmpty()) {
        QMessageBox::warning(this, "SudokuCode", "Please select an input file.");
        return;
    }
    Params p = decodeParams();

    Code code;
    try {
        if (!readBMP(m_decodeInputEdit->text().toStdString(), code, m_decodeCellSizeSpin->value())) {
            QMessageBox::critical(this, "SudokuCode", "Failed to read BMP image.");
            return;
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "SudokuCode", QString("Read failed: %1").arg(e.what()));
        return;
    }

    std::vector<uint8_t> data;
    try {
        data = decode(code, p);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "SudokuCode", QString("Decode failed: %1").arg(e.what()));
        return;
    }

    populateCode(code);
    QString outPath = m_decodeOutputEdit->text();
    if (!outPath.isEmpty()) {
        std::ofstream ofs(outPath.toStdString(), std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));
        updateStatus(QString("Decoded %1 bytes -> %2").arg(data.size()).arg(outPath));
    } else {
        updateStatus(QString("Decoded %1 bytes (no output file)").arg(data.size()));
    }
}

void MainWindow::onExportImage() {
    Code code = m_gridWidget->code();
    if (code.n <= 0) {
        QMessageBox::warning(this, "SudokuCode", "No grid to export. Generate first.");
        return;
    }
    QString f = QFileDialog::getSaveFileName(this, "Export BMP image", "code.bmp",
        "BMP Image (*.bmp)");
    if (f.isEmpty()) return;
    if (!writeBMP(f.toStdString(), code, m_cellSizeSpin->value())) {
        QMessageBox::critical(this, "SudokuCode", "Failed to export BMP image.");
        return;
    }
    updateStatus(QString("Exported image -> %1").arg(f));
}

void MainWindow::onCellClicked(int row, int col) {
    Q_UNUSED(row); Q_UNUSED(col);
}

void MainWindow::onCopyAscii() {
    if (m_asciiEdit) {
        QApplication::clipboard()->setText(m_asciiEdit->toPlainText());
        updateStatus("ASCII preview copied to clipboard.");
    }
}
