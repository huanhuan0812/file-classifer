#pragma once
#include <string>
#include <vector>
#include <set>
#include <memory>
#include "cppjieba/Jieba.hpp"

namespace TextProcessor {
    
    /**
     * 基础文本清洗（使用ICU库）
     */
    std::string cleanText(const std::string& text);

    /**
     * 清洗文件名：只保留中英文、数字，其他转空格
     */
    std::string cleanFilename(const std::string& filename);
    
    /**
     * 将词列表合并为空格分隔的字符串
     */
    std::string joinWords(const std::vector<std::string>& words, const std::string& delimiter = " ");
    
    /**
     * 文本分词器（使用cppjieba库）
     */
    class TextSegmenter {
    public:
        TextSegmenter(
            const std::string& dict_path,
            const std::string& hmm_path,
            const std::string& user_dict_path,
            const std::string& idf_path,
            const std::string& stop_word_path
        );
        
        ~TextSegmenter() = default;
        
        /**
         * 获取jieba实例（供FilenameSegmenter共用）
         */
        cppjieba::Jieba* getJieba() const { return jieba_.get(); }
        
        /**
         * 获取停用词表（供FilenameSegmenter共用）
         */
        const std::set<std::string>& getStopwords() const { return stopwords_; }
        
        /**
         * 获取有意义单字表（供FilenameSegmenter共用）
         */
        const std::set<std::string>& getMeaningfulSingleChars() const { return meaningful_single_chars_; }
        
        /**
         * 文本分词 - 返回词列表（已过滤停用词）
         */
        std::vector<std::string> cutWords(const std::string& text, bool use_hmm = false);
        
        /**
         * 处理文本：清洗 + 分词 + 过滤 + 合并
         */
        std::string processText(const std::string& text, bool use_hmm = false);
        
        /**
         * 检查分词器是否初始化成功
         */
        bool isReady() const { return jieba_ != nullptr; }
        
    private:
        std::unique_ptr<cppjieba::Jieba> jieba_;
        std::set<std::string> stopwords_;
        std::set<std::string> meaningful_single_chars_;
        
        void initStopwords();
        void initMeaningfulSingleChars();
        std::vector<std::string> doSegment(const std::string& text, bool use_hmm);
        std::vector<std::string> filterStopwords(const std::vector<std::string>& words);
        std::string trim(const std::string& str);
    };
    
    /**
     * 文件名分词器（共用TextSegmenter的jieba词典）
     */
    class FilenameSegmenter {
    public:
        /**
         * 构造函数：传入TextSegmenter指针以共用jieba实例
         */
        explicit FilenameSegmenter(TextSegmenter* text_segmenter);
        
        ~FilenameSegmenter() = default;
        
        /**
         * 文件名分词 - 返回词列表（已过滤停用词，保留有意义单字）
         */
        std::vector<std::string> cutFilename(const std::string& filename, bool use_hmm = false);
        
        /**
         * 处理文件名：清洗 + 分词 + 过滤 + 合并，返回空格分隔的词序列
         * 这是主要的对外接口，直接返回string
         */
        std::string processFilename(const std::string& filepath, bool use_hmm = false);
        
    private:
        TextSegmenter* text_segmenter_;  // 共用jieba实例
        std::set<std::string> stopwords_;  // 引用停用词表
        std::set<std::string> meaningful_single_chars_;  // 引用有意义单字表
        
        std::string trim(const std::string& str);
        std::vector<std::string> filterStopwordsForFilename(const std::vector<std::string>& words);
    };
    
} // namespace TextProcessor