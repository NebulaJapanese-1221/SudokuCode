#include <QApplication>
#include "mainwindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("SudokuCode");
    app.setOrganizationName("SudokuCode");

    MainWindow w;
    w.show();
    return app.exec();
}
