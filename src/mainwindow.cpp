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
#include <QDebug>  // 添加 qDebug 头文件
#include "classifier.h"

mainwindow::mainwindow(QWidget *parent)
    : QMainWindow{parent}
{
    // 设置窗口
    setWindowTitle("文件分类器");
    resize(600, 450);
    
    // 创建 UI
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    
    // 文件选择区域
    QHBoxLayout *fileLayout = new QHBoxLayout();
    FileLabel = new QLabel("请选择文件", this);
    FileLabel->setStyleSheet("border: 1px solid #ccc; padding: 5px;");
    FileLabel->setMinimumHeight(30);
    FileLabel->setWordWrap(true);
    
    BrowseButton = new QPushButton("浏览...", this);
    BrowseButton->setFixedWidth(80);
    
    fileLayout->addWidget(FileLabel, 1);
    fileLayout->addWidget(BrowseButton);
    mainLayout->addLayout(fileLayout);
    
    // 处理按钮
    ProcessButton = new QPushButton("开始分类", this);
    ProcessButton->setFixedHeight(40);
    ProcessButton->setStyleSheet("QPushButton { font-weight: bold; }");
    mainLayout->addWidget(ProcessButton);
    
    // 结果显示区域
    ResultLabel = new QLabel("正在初始化...", this);
    ResultLabel->setStyleSheet("border: 1px solid #ccc; padding: 10px; background-color: #f9f9f9;");
    ResultLabel->setMinimumHeight(150);
    ResultLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    ResultLabel->setWordWrap(true);
    mainLayout->addWidget(ResultLabel, 1);
    
    mainLayout->addStretch();

    // 连接信号槽
    connect(BrowseButton, &QPushButton::clicked, this, &mainwindow::onBrowseClicked);
    connect(ProcessButton, &QPushButton::clicked, this, &mainwindow::onProcessClicked);
    
    // 初始化组件
    if (!initializeComponents()) {
        // 显示所有错误
        QString errorMsg = "初始化失败:\n\n";
        for (const auto& err : init_errors_) {
            errorMsg += "• " + QString::fromStdString(err) + "\n";
        }
        errorMsg += "\n请检查配置后重新启动程序。";
        
        QMessageBox::critical(this, "初始化错误", errorMsg);
        updateUIForError(true);
        ResultLabel->setStyleSheet("border: 1px solid #ff0000; padding: 10px; background-color: #fff5f5;");
        ResultLabel->setText("❌ 初始化失败\n\n" + errorMsg);
        return;
    }
    
    updateUIForError(false);
    ResultLabel->setText("✅ 初始化成功\n\n请选择文件并点击\"开始分类\"进行处理。");
}

mainwindow::~mainwindow() {
    // 资源清理
}

bool mainwindow::initializeComponents() {
    bool success = true;
    
    // 获取程序所在目录
    appDir = QCoreApplication::applicationDirPath();
    
    qDebug() << "========================================";
    qDebug() << "程序所在目录:" << appDir;
    qDebug() << "========================================";
    
    // 1. 设置 Python 路径 - 使用程序所在目录下的 .venv
    QString pythonPath = appDir + "/.venv/lib/python3.14/site-packages";
    std::string pythonPathStr = pythonPath.toStdString();
    
    #ifdef _WIN32
        _putenv_s("PYTHONPATH", pythonPathStr.c_str());
    #else
        setenv("PYTHONPATH", pythonPathStr.c_str(), 1);
    #endif
    
    qDebug() << "PYTHONPATH:" << pythonPath;
    
    // 2. 初始化 Python 文本提取器
    try {
        QString pythonScriptDir = appDir + "/python";
        qDebug() << "Python 脚本目录:" << pythonScriptDir;
        
        extractor = std::make_unique<PythonTextExtractor>(pythonScriptDir.toStdString());
        
        if (!extractor->isInitialized()) {
            std::string initError = extractor->getInitError();
            if (initError.empty()) {
                initError = extractor->getLastError();
            }
            init_errors_.push_back("Python 文本提取器: " + initError);
            success = false;
            qWarning() << "Python 文本提取器初始化失败:" << QString::fromStdString(initError);
        } else {
            qDebug() << "Python 文本提取器初始化成功";
        }
    } catch (const std::exception& e) {
        QString error = QString::fromStdString(e.what());
        init_errors_.push_back("Python 文本提取器异常: " + error.toStdString());
        success = false;
        qCritical() << "Python 文本提取器异常:" << error;
    }
    
    // 3. 检查词典文件并初始化分词器
    std::vector<std::pair<std::string, std::string>> dict_files = {
        {"jieba.dict.utf8", "jieba.dict.utf8"},
        {"hmm_model.utf8", "hmm_model.utf8"},
        {"user.dict.utf8", "user.dict.utf8"},
        {"idf.utf8", "idf.utf8"},
        {"stop_words.utf8", "stop_words.utf8"}
    };
    
    QString dictDir = appDir + "/dict";
    qDebug() << "词典目录:" << dictDir;
    
    bool allDictExist = true;
    for (const auto& [name, filename] : dict_files) {
        QString fullPath = dictDir + "/" + QString::fromStdString(filename);
        QFileInfo fileInfo(fullPath);
        
        qDebug() << "检查文件:" << fullPath;
        qDebug() << "  是否存在:" << (fileInfo.exists() ? "是" : "否");
        qDebug() << "  是否可读:" << (fileInfo.isReadable() ? "是" : "否");
        
        if (!fileInfo.exists()) {
            init_errors_.push_back("词典文件不存在: " + fullPath.toStdString() + " (" + name + ")");
            allDictExist = false;
            success = false;
            qWarning() << "词典文件不存在:" << fullPath;
        } else if (!fileInfo.isReadable()) {
            init_errors_.push_back("词典文件不可读: " + fullPath.toStdString() + " (" + name + ")");
            allDictExist = false;
            success = false;
            qWarning() << "词典文件不可读:" << fullPath;
        }
    }
    
    if (allDictExist) {
        try {
            qDebug() << "正在初始化分词器...";
            
            QString dictPath = dictDir + "/jieba.dict.utf8";
            QString hmmPath = dictDir + "/hmm_model.utf8";
            QString userDictPath = dictDir + "/user.dict.utf8";
            QString idfPath = dictDir + "/idf.utf8";
            QString stopWordPath = dictDir + "/stop_words.utf8";
            
            text_segmenter = std::make_unique<TextProcessor::TextSegmenter>(
                dictPath.toStdString(),
                hmmPath.toStdString(),
                userDictPath.toStdString(),
                idfPath.toStdString(),
                stopWordPath.toStdString()
            );
            
            if (!text_segmenter->isReady()) {
                QString error = QString::fromStdString(text_segmenter->getInitError());
                init_errors_.push_back("分词器: " + error.toStdString());
                success = false;
                qWarning() << "分词器初始化失败:" << error;
            } else {
                qDebug() << "分词器初始化成功";
            }
        } catch (const std::exception& e) {
            QString error = QString::fromStdString(e.what());
            init_errors_.push_back("分词器异常: " + error.toStdString());
            success = false;
            qCritical() << "分词器异常:" << error;
        }
    }
    
    // 4. 初始化词汇表
    try {
        text_vocab = std::make_unique<Vocab::TextVocab>();
        filename_vocab = std::make_unique<Vocab::FilenameVocab>();
        qDebug() << "词汇表初始化成功";
    } catch (const std::exception& e) {
        QString error = QString::fromStdString(e.what());
        init_errors_.push_back("词汇表初始化失败: " + error.toStdString());
        success = false;
        qCritical() << "词汇表异常:" << error;
    }
    
    qDebug() << "========================================";
    qDebug() << "初始化结果:" << (success ? "成功 ✅" : "失败 ❌");
    qDebug() << "========================================";
    
    return success;
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

void mainwindow::onBrowseClicked() {
    if (!extractor || !extractor->isInitialized()) {
        QMessageBox::warning(this, "警告", "Python 文本提取器未初始化，请重启程序。");
        return;
    }
    
    if (!text_segmenter || !text_segmenter->isReady()) {
        QMessageBox::warning(this, "警告", "分词器未初始化，请重启程序。");
        return;
    }
    
    QString filePath = QFileDialog::getOpenFileName(
        this, 
        "选择文件", 
        "", 
        "PPTX文件 (*.ppt *.pptx);;所有文件 (*.*)"
    );
    
    if (!filePath.isEmpty()) {
        FileLabel->setText(filePath);
        ResultLabel->setText("已选择文件，点击\"开始分类\"进行处理");
        qDebug() << "用户选择了文件:" << filePath;
    }
}

void mainwindow::onProcessClicked() {
    if (!extractor || !extractor->isInitialized()) {
        QMessageBox::critical(this, "错误", 
            "Python 文本提取器未初始化，请检查 Python 环境配置。");
        return;
    }
    
    if (!text_segmenter || !text_segmenter->isReady()) {
        QMessageBox::critical(this, "错误", 
            "分词器未初始化，请检查词典文件是否存在。");
        return;
    }
    
    QString filePath = FileLabel->text();
    
    if (filePath.isEmpty() || filePath == "请选择文件") {
        QMessageBox::warning(this, "警告", "请先选择一个文件！");
        return;
    }
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, "警告", "文件不存在，请重新选择！");
        return;
    }
    
    qDebug() << "处理文件:" << filePath;
    qDebug() << "文件大小:" << fileInfo.size() << "字节";
    
    // 禁用按钮
    ProcessButton->setEnabled(false);
    ProcessButton->setText("处理中...");
    ResultLabel->setText("正在处理文件，请稍候...");
    QApplication::processEvents();
    
    // 处理文件
    auto result = FileProcessor::processFile(
        filePath.toStdString(),
        *extractor,
        *text_segmenter,
        *text_vocab,
        *filename_vocab,
        appDir.toStdString() + "/textcnn_classifier.onnx"
    );
    
    // 显示结果
    if (!result.success) {
        QString errorMsg = QString::fromStdString(result.error_message);
        ResultLabel->setText("❌ 处理失败:\n" + errorMsg);
        QMessageBox::warning(this, "处理失败", errorMsg);
        qWarning() << "处理失败:" << errorMsg;
    } else {
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
        for (size_t i = 0; i < result.probabilities.size() && i < result.categories.size(); ++i) {
            resultText += QString("  %1: %2%\n")
                .arg(QString::fromStdString(result.categories[i]))
                .arg(result.probabilities[i] * 100, 0, 'f', 2);
        }
        
        ResultLabel->setText(resultText);
        qDebug() << "分类成功:" << QString::fromStdString(result.predicted_class)
                 << "置信度:" << result.confidence;
    }
    
    // 恢复按钮
    ProcessButton->setEnabled(true);
    ProcessButton->setText("开始分类");
}