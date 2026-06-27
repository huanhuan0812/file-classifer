// mainwindow.h
#pragma once

#include "python_extractor.h"
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <memory>
#include <vector>
#include <string>
#include "text_processor.h"
#include "text_vocab_hash.h"
#include "filename_vocab_hash.h"

class mainwindow : public QMainWindow {
    Q_OBJECT

public:
    explicit mainwindow(QWidget *parent = nullptr);
    ~mainwindow();

private slots:
    void onBrowseClicked();
    void onProcessClicked();

private:
    bool initializeComponents();
    void updateUIForError(bool hasError);

private:
    QLabel *FileLabel;
    QLabel *ResultLabel;
    QPushButton *BrowseButton;
    QPushButton *ProcessButton;
    
    std::unique_ptr<PythonTextExtractor> extractor;
    std::unique_ptr<TextProcessor::TextSegmenter> text_segmenter;
    std::unique_ptr<Vocab::TextVocab> text_vocab;
    std::unique_ptr<Vocab::FilenameVocab> filename_vocab;
    
    bool initialized_ = false;
    std::vector<std::string> init_errors_;
    QString appDir;
};