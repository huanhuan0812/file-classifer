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

// 前向声明
struct ClassificationResult;

class mainwindow : public QMainWindow {
    Q_OBJECT

public:
    explicit mainwindow(QWidget *parent = nullptr);
    ~mainwindow();

private slots:
    void onBrowseClicked();
    void onProcessClicked();

private:
    // UI 初始化
    void setupUI();
    void connectSignals();
    
    // 初始化相关
    bool initializeComponents();
    bool setupPythonEnvironment();
    void showInitializationError();
    void updateUIForError(bool hasError);
    
    // 延迟初始化方法
    bool ensureExtractorInitialized();
    bool ensureSegmenterInitialized();
    bool ensureVocabInitialized();
    
    // 资源清理
    void cleanupResources();
    
    // 辅助方法
    bool validateFile(const QString& filePath, QString& errorMsg) const;
    QString formatResult(const ClassificationResult& result) const;
    void resetUIState();

private:
    // UI 组件
    QLabel* FileLabel = nullptr;
    QLabel* ResultLabel = nullptr;
    QPushButton* BrowseButton = nullptr;
    QPushButton* ProcessButton = nullptr;
    
    // 核心组件（使用智能指针管理）
    std::unique_ptr<PythonTextExtractor> extractor;
    std::unique_ptr<TextProcessor::TextSegmenter> text_segmenter;
    std::unique_ptr<Vocab::TextVocab> text_vocab;
    std::unique_ptr<Vocab::FilenameVocab> filename_vocab;
    
    // 状态标志
    bool extractor_initialized_ = false;
    bool segmenter_initialized_ = false;
    bool vocab_initialized_ = false;
    bool is_processing_ = false;
    
    // 配置信息
    QString appDir;
    std::string cached_model_path_;
    std::vector<std::string> init_errors_;
    
    // 缓存（减少重复分配）
    QString last_processed_file_;
};