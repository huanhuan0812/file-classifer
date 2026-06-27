#include "python_extractor.h"
#include "text_processor.h"
#include "text_vocab_hash.h"
#include "filename_vocab_hash.h"
#include "classifier.h"
#include <iostream>
#include <stdlib.h>
#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    mainwindow window;
    window.setWindowTitle("文件分类器");
    window.resize(600, 400);
    window.show();

    return app.exec();
}