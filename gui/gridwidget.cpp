#include "gridwidget.h"
#include <QPainter>
#include <QFontMetrics>
#include <QApplication>
#include <cmath>

GridWidget::GridWidget(QWidget* parent)
    : QWidget(parent) {
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void GridWidget::setCode(const sc::Code& code) {
    m_code = code;
    update();
}

sc::Code GridWidget::code() const {
    return m_code;
}

void GridWidget::setCellSize(int px) {
    m_cellSize = qMax(10, px);
    update();
    adjustSize();
}

void GridWidget::setHighlightCell(int row, int col) {
    m_highlight = QPoint(col, row);
    update();
}

QPoint GridWidget::cellAt(const QPoint& pos) const {
    int n = m_code.n;
    if (n <= 0) return QPoint(-1, -1);
    int cw = m_cellSize + m_border;
    int x0 = m_border;
    int y0 = m_border;
    int col = (pos.x() - x0) / cw;
    int row = (pos.y() - y0) / cw;
    if (col < 0 || col >= n || row < 0 || row >= n) return QPoint(-1, -1);
    // check inside cell (not border)
    int cx = x0 + col * cw;
    int cy = y0 + row * cw;
    if (pos.x() < cx || pos.x() >= cx + m_cellSize) return QPoint(-1, -1);
    if (pos.y() < cy || pos.y() >= cy + m_cellSize) return QPoint(-1, -1);
    return QPoint(col, row);
}

void GridWidget::drawCell(QPainter& p, int row, int col, const QRect& rect) const {
    int n = m_code.n;
    uint8_t v = m_code.grid[row * n + col];
    bool isData = row * n + col < static_cast<int>(m_code.mask.size()) && m_code.mask[row * n + col];

    // fill
    QRect fillRect = rect.adjusted(1, 1, -1, -1);
    if (m_highlight == QPoint(col, row)) {
        p.fillRect(fillRect, m_hoverColor);
    } else if (isData) {
        p.fillRect(fillRect, m_dataColor);
    } else {
        p.fillRect(fillRect, m_solvedColor);
    }

    // value text
    QString text;
    if (v == 0) text = "..";
    else text = QString("%1").arg(static_cast<int>(v), 2, 16, QChar('0')).toUpper();
    p.setPen(m_textColor);
    QFont font = p.font();
    font.setPointSize(qMax(8, m_cellSize / 3));
    font.setBold(isData);
    p.setFont(font);
    QFontMetrics fm(font);
    QRect textRect = fm.boundingRect(text);
    p.drawText(rect.center() - textRect.center(), text);
}

void GridWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    int n = m_code.n;
    if (n <= 0) {
        p.fillRect(rect(), palette().window());
        p.setPen(m_textColor);
        p.drawText(rect(), Qt::AlignCenter, "No grid loaded");
        return;
    }

    int bs = static_cast<int>(std::sqrt(static_cast<double>(n)));
    int cw = m_cellSize + m_border;
    int w = n * cw + m_border;
    int h = n * cw + m_border;

    // center content
    int dx = (width() - w) / 2;
    int dy = (height() - h) / 2;
    QRect origin(dx, dy, w, h);

    p.fillRect(rect(), palette().window());

    // draw background grid area
    p.fillRect(origin, QColor(245, 245, 245));

    // outer border
    p.setPen(m_boxColor);
    p.drawRect(origin);

    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            QRect cellRect(origin.left() + c * cw + m_border,
                           origin.top() + r * cw + m_border,
                           m_cellSize, m_cellSize);
            drawCell(p, r, c, cellRect);
        }
    }

    // box lines (thicker every bs)
    p.setPen(m_boxColor);
    for (int i = 0; i <= n; i += bs) {
        int x = origin.left() + i * cw + m_border;
        int y = origin.top() + i * cw + m_border;
        p.drawLine(x, origin.top(), x, origin.bottom());
        p.drawLine(origin.left(), y, origin.right(), y);
    }
}

void GridWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    QPoint cell = cellAt(event->pos());
    if (cell.x() >= 0 && cell.y() >= 0) {
        m_hover = cell;
        emit cellClicked(cell.y(), cell.x());
        update();
    }
    QWidget::mousePressEvent(event);
}

void GridWidget::resizeEvent(QResizeEvent* event) {
    Q_UNUSED(event);
    update();
}
