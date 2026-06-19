#include "python_extractor.h"
#include "text_processor.h"
#include "text_vocab_hash.h"
#include "filename_vocab_hash.h"
#include "classifier.h"
#include <iostream>
#include <stdlib.h>

int main() {
    // 1. 设置 Python 模块路径（只运行一次）
    const char* venv_site_packages = ".venv/lib/python3.14/site-packages";
    #ifdef _WIN32
        _putenv_s("PYTHONPATH", venv_site_packages);
    #else
        setenv("PYTHONPATH", venv_site_packages, 1);
    #endif

    // 2. 初始化 Python 文本提取器（只运行一次）
    PythonTextExtractor extractor;
    if (!extractor.isInitialized()) {
        std::cerr << "PythonTextExtractor 初始化失败: " << extractor.getLastError() << std::endl;
        return 1;
    }
    
    // 3. 初始化分词器（只运行一次）
    TextProcessor::TextSegmenter text_segmenter(
        "./dict/jieba.dict.utf8",
        "./dict/hmm_model.utf8",
        "./dict/user.dict.utf8",
        "./dict/idf.utf8",
        "./dict/stop_words.utf8"
    );
    
    // 4. 初始化词汇表（只运行一次）
    Vocab::TextVocab text_vocab;
    Vocab::FilenameVocab filename_vocab;

    // 5. 处理文件
    std::string filepath = "拓展9.pptx";
    auto result = FileProcessor::processFile(
        filepath, 
        extractor, 
        text_segmenter, 
        text_vocab, 
        filename_vocab
    );
    
    // 6. 打印结果
    FileProcessor::printResult(result, filepath);

    filepath = "高二期中考试202605.pptx";
    result = FileProcessor::processFile(
        filepath, 
        extractor, 
        text_segmenter, 
        text_vocab, 
        filename_vocab
    );
    FileProcessor::printResult(result, filepath);

    return 0;
}