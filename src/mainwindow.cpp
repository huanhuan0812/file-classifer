// mainwindow.cpp
#include <Python.h>
#include "mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QScopeGuard>  // 添加 Qt 的 ScopeGuard 头文件
#include "classifier.h"

// 常量定义
namespace {
    constexpr int MIN_FILE_SIZE = 1024;  // 1KB
    constexpr int MAX_FILE_SIZE = 100 * 1024 * 1024;  // 100MB
    constexpr const char* DEFAULT_MODEL_NAME = "/textcnn_classifier.onnx";
    constexpr const char* PYTHON_SITE_PACKAGES = "/.venv/lib/python3.14/site-packages";
}

mainwindow::mainwindow(QWidget *parent)
    : QMainWindow{parent}
{
    setupUI();
    connectSignals();
    
    // 异步初始化，避免阻塞 UI
    QTimer::singleShot(0, this, [this]() {
        if (!initializeComponents()) {
            showInitializationError();
        }
    });
}

mainwindow::~mainwindow() {
    cleanupResources();
}

void mainwindow::setupUI() {
    setWindowTitle("文件分类器");
    resize(600, 450);
    
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // 文件选择区域
    QHBoxLayout* fileLayout = new QHBoxLayout();
    fileLayout->setSpacing(5);
    
    FileLabel = new QLabel("请选择文件", this);
    FileLabel->setStyleSheet(
        "border: 1px solid #ccc; "
        "padding: 5px; "
        "border-radius: 3px;"
    );
    FileLabel->setMinimumHeight(30);
    FileLabel->setWordWrap(true);
    FileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    
    BrowseButton = new QPushButton("浏览...", this);
    BrowseButton->setFixedSize(80, 30);
    BrowseButton->setCursor(Qt::PointingHandCursor);
    
    fileLayout->addWidget(FileLabel, 1);
    fileLayout->addWidget(BrowseButton);
    mainLayout->addLayout(fileLayout);
    
    // 处理按钮
    ProcessButton = new QPushButton("开始分类", this);
    ProcessButton->setFixedHeight(40);
    ProcessButton->setCursor(Qt::PointingHandCursor);
    ProcessButton->setStyleSheet(
        "QPushButton { font-weight: bold; background-color: #4CAF50; color: white; border-radius: 5px; }"
        "QPushButton:hover { background-color: #45a049; }"
        "QPushButton:disabled { background-color: #cccccc; color: #666666; }"
    );
    mainLayout->addWidget(ProcessButton);
    
    // 结果显示区域
    ResultLabel = new QLabel("正在初始化...", this);
    ResultLabel->setStyleSheet(
        "border: 1px solid #ccc; "
        "padding: 10px; "
        "background-color: #f9f9f9; "
        "border-radius: 3px;"
    );
    ResultLabel->setMinimumHeight(150);
    ResultLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    ResultLabel->setWordWrap(true);
    ResultLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mainLayout->addWidget(ResultLabel, 1);
}

void mainwindow::connectSignals() {
    connect(BrowseButton, &QPushButton::clicked, this, &mainwindow::onBrowseClicked);
    connect(ProcessButton, &QPushButton::clicked, this, &mainwindow::onProcessClicked);
}

bool mainwindow::initializeComponents() {
    appDir = QCoreApplication::applicationDirPath();
    cached_model_path_ = (appDir + DEFAULT_MODEL_NAME).toStdString();
    
    qDebug() << "========================================";
    qDebug() << "程序目录:" << appDir;
    qDebug() << "模型路径:" << QString::fromStdString(cached_model_path_);
    qDebug() << "========================================";
    
    bool success = true;
    
    // 设置 Python 环境
    if (!setupPythonEnvironment()) {
        init_errors_.push_back("Python环境设置失败");
        success = false;
    }
    
    // 延迟初始化核心组件（不在这里初始化，而是在使用时按需初始化）
    extractor_initialized_ = false;
    segmenter_initialized_ = false;
    vocab_initialized_ = false;
    
    qDebug() << "初始化结果:" << (success ? "成功 ✅" : "失败 ❌");
    qDebug() << "========================================";
    
    if (success) {
        ResultLabel->setText("✅ 就绪\n\n请选择文件并点击\"开始分类\"进行处理。");
        updateUIForError(false);
    }
    
    return success;
}

bool mainwindow::setupPythonEnvironment() {
    QString pythonPath = appDir + PYTHON_SITE_PACKAGES;
    std::string pythonPathStr = pythonPath.toStdString();
    
#ifdef _WIN32
    _putenv_s("PYTHONPATH", pythonPathStr.c_str());
#else
    setenv("PYTHONPATH", pythonPathStr.c_str(), 1);
#endif
    
    qDebug() << "PYTHONPATH:" << pythonPath;
    return true;
}

bool mainwindow::ensureExtractorInitialized() {
    if (extractor_initialized_ && extractor && extractor->isInitialized()) {
        return true;
    }
    
    try {
        QString pythonScriptDir = appDir + "/python";
        qDebug() << "初始化 Python 提取器，脚本目录:" << pythonScriptDir;
        
        extractor = std::make_unique<PythonTextExtractor>(pythonScriptDir.toStdString());
        
        if (extractor && extractor->isInitialized()) {
            extractor_initialized_ = true;
            qDebug() << "Python 提取器初始化成功";
            return true;
        }
        
        std::string error = extractor ? extractor->getInitError() : "提取器创建失败";
        if (error.empty()) {
            error = extractor ? extractor->getLastError() : "未知错误";
        }
        init_errors_.push_back("Python提取器: " + error);
        
    } catch (const std::exception& e) {
        init_errors_.push_back("Python提取器异常: " + std::string(e.what()));
        qCritical() << "Python提取器异常:" << e.what();
    }
    
    extractor_initialized_ = false;
    return false;
}

bool mainwindow::ensureSegmenterInitialized() {
    if (segmenter_initialized_ && text_segmenter && text_segmenter->isReady()) {
        return true;
    }
    
    // 检查词典文件
    QString dictDir = appDir + "/dict";
    QDir dictPath(dictDir);
    if (!dictPath.exists()) {
        init_errors_.push_back("词典目录不存在: " + dictDir.toStdString());
        return false;
    }
    
    // 检查必需的词典文件
    QStringList requiredFiles = {
        "jieba.dict.utf8",
        "hmm_model.utf8",
        "user.dict.utf8",
        "idf.utf8",
        "stop_words.utf8"
    };
    
    for (const QString& filename : requiredFiles) {
        QFileInfo fileInfo(dictDir + "/" + filename);
        if (!fileInfo.exists()) {
            init_errors_.push_back("词典文件不存在: " + fileInfo.absoluteFilePath().toStdString());
            return false;
        }
        if (!fileInfo.isReadable()) {
            init_errors_.push_back("词典文件不可读: " + fileInfo.absoluteFilePath().toStdString());
            return false;
        }
    }
    
    try {
        qDebug() << "正在初始化分词器...";
        
        text_segmenter = std::make_unique<TextProcessor::TextSegmenter>(
            (dictDir + "/jieba.dict.utf8").toStdString(),
            (dictDir + "/hmm_model.utf8").toStdString(),
            (dictDir + "/user.dict.utf8").toStdString(),
            (dictDir + "/idf.utf8").toStdString(),
            (dictDir + "/stop_words.utf8").toStdString()
        );
        
        if (text_segmenter && text_segmenter->isReady()) {
            segmenter_initialized_ = true;
            qDebug() << "分词器初始化成功";
            return true;
        }
        
        init_errors_.push_back("分词器: " + text_segmenter->getInitError());
        
    } catch (const std::exception& e) {
        init_errors_.push_back("分词器异常: " + std::string(e.what()));
        qCritical() << "分词器异常:" << e.what();
    }
    
    segmenter_initialized_ = false;
    return false;
}

bool mainwindow::ensureVocabInitialized() {
    if (vocab_initialized_ && text_vocab && filename_vocab) {
        return true;
    }
    
    try {
        text_vocab = std::make_unique<Vocab::TextVocab>();
        filename_vocab = std::make_unique<Vocab::FilenameVocab>();
        vocab_initialized_ = true;
        qDebug() << "词汇表初始化成功";
        return true;
    } catch (const std::exception& e) {
        init_errors_.push_back("词汇表初始化失败: " + std::string(e.what()));
        qCritical() << "词汇表异常:" << e.what();
    }
    
    vocab_initialized_ = false;
    return false;
}

void mainwindow::showInitializationError() {
    QString errorMsg = "初始化失败:\n\n";
    for (const auto& err : init_errors_) {
        errorMsg += "• " + QString::fromStdString(err) + "\n";
    }
    errorMsg += "\n请检查配置后重新启动程序。";
    
    QMessageBox::critical(this, "初始化错误", errorMsg);
    updateUIForError(true);
    ResultLabel->setStyleSheet(
        "border: 1px solid #ff0000; "
        "padding: 10px; "
        "background-color: #fff5f5; "
        "border-radius: 3px;"
    );
    ResultLabel->setText("❌ 初始化失败\n\n" + errorMsg);
}

void mainwindow::updateUIForError(bool hasError) {
    BrowseButton->setEnabled(!hasError);
    ProcessButton->setEnabled(!hasError);
    
    if (hasError) {
        BrowseButton->setToolTip("程序初始化失败，请重启");
        ProcessButton->setToolTip("程序初始化失败，请重启");
    } else {
        BrowseButton->setToolTip("");
        ProcessButton->setToolTip("");
    }
}

void mainwindow::cleanupResources() {
    // 按依赖顺序清理
    filename_vocab.reset();
    text_vocab.reset();
    text_segmenter.reset();
    extractor.reset();
    
    // 清理缓存
    last_processed_file_.clear();
    cached_model_path_.clear();
    init_errors_.clear();
}

bool mainwindow::validateFile(const QString& filePath, QString& errorMsg) const {
    if (filePath.isEmpty() || filePath == "请选择文件") {
        errorMsg = "请先选择一个文件！";
        return false;
    }
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        errorMsg = "文件不存在，请重新选择！";
        return false;
    }
    
    if (!fileInfo.isFile()) {
        errorMsg = "选择的路径不是文件！";
        return false;
    }
    
    qint64 fileSize = fileInfo.size();
    if (fileSize < MIN_FILE_SIZE) {
        errorMsg = "文件太小（< 1KB），可能无法提取有效内容！";
        return false;
    }
    
    if (fileSize > MAX_FILE_SIZE) {
        errorMsg = QString("文件太大（> %1MB），请选择更小的文件！")
                      .arg(MAX_FILE_SIZE / 1024 / 1024);
        return false;
    }
    
    return true;
}

QString mainwindow::formatResult(const ClassificationResult& result) const {
    QString resultText = QString(
        "✅ 分类完成!\n\n"
        "📂 预测类别: %1\n"
        "📊 置信度: %2%\n"
        "📝 单词数: %3\n\n"
        "📈 详细概率:\n"
    ).arg(QString::fromStdString(result.predicted_class))
     .arg(result.confidence * 100, 0, 'f', 2)
     .arg(result.word_count);
    
    // 添加所有类别的概率
    size_t maxCategories = std::min(result.probabilities.size(), result.categories.size());
    for (size_t i = 0; i < maxCategories; ++i) {
        resultText += QString("  %1: %2%\n")
            .arg(QString::fromStdString(result.categories[i]))
            .arg(result.probabilities[i] * 100, 0, 'f', 2);
    }
    
    return resultText;
}

void mainwindow::resetUIState() {
    ProcessButton->setEnabled(true);
    ProcessButton->setText("开始分类");
    ProcessButton->setStyleSheet(
        "QPushButton { font-weight: bold; background-color: #4CAF50; color: white; border-radius: 5px; }"
        "QPushButton:hover { background-color: #45a049; }"
        "QPushButton:disabled { background-color: #cccccc; color: #666666; }"
    );
    is_processing_ = false;
}

void mainwindow::onBrowseClicked() {
    // 如果正在处理，不允许选择
    if (is_processing_) {
        return;
    }
    
    // 延迟初始化检查
    if (!ensureExtractorInitialized()) {
        QMessageBox::warning(this, "警告", 
            "Python 文本提取器未初始化，请检查配置后重启程序。");
        return;
    }
    
    if (!ensureSegmenterInitialized()) {
        QMessageBox::warning(this, "警告", 
            "分词器未初始化，请检查词典文件是否存在。");
        return;
    }
    
    if (!ensureVocabInitialized()) {
        QMessageBox::warning(this, "警告", 
            "词汇表未初始化，请检查配置文件。");
        return;
    }
    
    QString filePath = QFileDialog::getOpenFileName(
        this, 
        "选择文件", 
        last_processed_file_.isEmpty() ? "" : QFileInfo(last_processed_file_).path(),
        "PPTX文件 (*.ppt *.pptx);;所有文件 (*.*)"
    );
    
    if (!filePath.isEmpty()) {
        FileLabel->setText(filePath);
        ResultLabel->setText("已选择文件，点击\"开始分类\"进行处理");
        ResultLabel->setStyleSheet(
            "border: 1px solid #ccc; padding: 10px; background-color: #f9f9f9; border-radius: 3px;"
        );
        qDebug() << "用户选择了文件:" << filePath;
    }
}

void mainwindow::onProcessClicked() {
    // 防止重复点击
    if (is_processing_) {
        return;
    }
    
    // 确保所有组件已初始化
    if (!ensureExtractorInitialized()) {
        QMessageBox::critical(this, "错误", 
            "Python 文本提取器未初始化，请检查配置后重启程序。");
        return;
    }
    
    if (!ensureSegmenterInitialized()) {
        QMessageBox::critical(this, "错误", 
            "分词器未初始化，请检查词典文件是否存在。");
        return;
    }
    
    if (!ensureVocabInitialized()) {
        QMessageBox::critical(this, "错误", 
            "词汇表未初始化，请检查配置文件。");
        return;
    }
    
    // 验证文件
    QString filePath = FileLabel->text();
    QString errorMsg;
    if (!validateFile(filePath, errorMsg)) {
        QMessageBox::warning(this, "警告", errorMsg);
        return;
    }
    
    qDebug() << "处理文件:" << filePath;
    
    // 更新 UI 状态
    is_processing_ = true;
    ProcessButton->setEnabled(false);
    ProcessButton->setText("处理中...");
    ProcessButton->setStyleSheet(
        "QPushButton { font-weight: bold; background-color: #ff9800; color: white; border-radius: 5px; }"
    );
    ResultLabel->setText("⏳ 正在处理文件，请稍候...");
    ResultLabel->setStyleSheet(
        "border: 1px solid #ff9800; padding: 10px; background-color: #fff3e0; border-radius: 3px;"
    );
    QApplication::processEvents();
    
    // 使用 Qt 的 qScopeGuard 确保状态恢复
    auto scopeGuard = qScopeGuard([this]() {
        resetUIState();
    });
    
    // 减少字符串转换和拷贝
    const std::string filePathStd = filePath.toStdString();
    
    // 处理文件 - 使用 FileProcessor::processFile
    ClassificationResult result = FileProcessor::processFile(
        filePathStd,
        *extractor,
        *text_segmenter,
        *text_vocab,
        *filename_vocab,
        cached_model_path_
    );
    
    // 显示结果
    if (!result.success) {
        QString errorMsgResult = QString::fromStdString(result.error_message);
        ResultLabel->setText("❌ 处理失败:\n" + errorMsgResult);
        ResultLabel->setStyleSheet(
            "border: 1px solid #ff0000; padding: 10px; background-color: #fff5f5; border-radius: 3px;"
        );
        QMessageBox::warning(this, "处理失败", errorMsgResult);
        qWarning() << "处理失败:" << errorMsgResult;
    } else {
        ResultLabel->setText(formatResult(result));
        ResultLabel->setStyleSheet(
            "border: 1px solid #4CAF50; padding: 10px; background-color: #f0fff0; border-radius: 3px;"
        );
        last_processed_file_ = filePath;
        qDebug() << "分类成功:" << QString::fromStdString(result.predicted_class)
                 << "置信度:" << result.confidence;
    }
}