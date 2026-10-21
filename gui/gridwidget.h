#pragma once

#include <QWidget>
#include <QColor>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QVector>
#include <QPoint>

#include "src/sudokucode.h"

class GridWidget : public QWidget {
    Q_OBJECT
public:
    explicit GridWidget(QWidget* parent = nullptr);

    void setCode(const sc::Code& code);
    sc::Code code() const;
    void setCellSize(int px);
    void setHighlightCell(int row, int col);

signals:
    void cellClicked(int row, int col);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    sc::Code m_code;
    int m_cellSize = 30;
    int m_border = 4;
    QPoint m_hover{-1, -1};
    QPoint m_highlight{-1, -1};
    QColor m_dataColor = QColor(220, 240, 255);
    QColor m_solvedColor = QColor(250, 250, 250);
    QColor m_gridColor = QColor(60, 60, 60);
    QColor m_boxColor = QColor(30, 30, 30);
    QColor m_textColor = QColor(20, 20, 20);
    QColor m_hoverColor = QColor(255, 230, 150);

    QPoint cellAt(const QPoint& pos) const;
    void drawCell(QPainter& p, int row, int col, const QRect& rect) const;
};
