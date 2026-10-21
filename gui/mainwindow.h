#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QTabWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>

#include "src/sudokucode.h"

class GridWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onEncode();
    void onDecode();
    void onOpenEncodeInput();
    void onSaveEncodeOutput();
    void onOpenDecodeInput();
    void onSaveDecodeOutput();
    void onExportImage();
    void onToggleEncrypt(bool checked);
    void onCellClicked(int row, int col);
    void onCopyAscii();

private:
    void buildUI();
    void updateStatus(const QString& msg);
    sc::Params encodeParams() const;
    sc::Params decodeParams() const;
    void populateCode(const sc::Code& code);

    QTabWidget* m_tabs = nullptr;
    QWidget* m_encodeTab = nullptr;
    QWidget* m_decodeTab = nullptr;

    QLineEdit* m_inputEdit = nullptr;
    QLineEdit* m_outputEdit = nullptr;
    QLineEdit* m_keyEdit = nullptr;
    QCheckBox* m_encryptBox = nullptr;
    QCheckBox* m_compressBox = nullptr;
    QCheckBox* m_asciiBox = nullptr;
    QComboBox* m_gridSizeCombo = nullptr;
    QSpinBox* m_eccSpin = nullptr;
    QSpinBox* m_cellSizeSpin = nullptr;
    QCheckBox* m_autoEccBox = nullptr;
    QPushButton* m_btnEncode = nullptr;
    QPushButton* m_btnExportImage = nullptr;
    QPushButton* m_btnCopyAscii = nullptr;
    QLineEdit* m_passEdit = nullptr;

    GridWidget* m_gridWidget = nullptr;
    QTextEdit* m_asciiEdit = nullptr;
    QLabel* m_statusLabel = nullptr;

    QLineEdit* m_decodeInputEdit = nullptr;
    QLineEdit* m_decodeOutputEdit = nullptr;
    QCheckBox* m_decodeEncryptBox = nullptr;
    QLineEdit* m_decodeKeyEdit = nullptr;
    QLineEdit* m_decodePassEdit = nullptr;
    QComboBox* m_decodeGridSizeCombo = nullptr;
    QSpinBox* m_decodeEccSpin = nullptr;
    QCheckBox* m_decodeAutoEccBox = nullptr;
    QCheckBox* m_decodeCompressBox = nullptr;
    QSpinBox* m_decodeCellSizeSpin = nullptr;
    QTextEdit* m_decodeAsciiEdit = nullptr;
    QPushButton* m_decodeBtnDecode = nullptr;
    QPushButton* m_decodeBtnVerify = nullptr;
    GridWidget* m_decodeGridWidget = nullptr;
};
