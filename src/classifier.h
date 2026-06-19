#pragma once

#include "python_extractor.h"
#include "text_processor.h"
#include "text_vocab_hash.h"
#include "filename_vocab_hash.h"
#include "model_config.h"
#include <onnxruntime_cxx_api.h>
#include <cpu_provider_factory.h>
#include <vector>
#include <string>
#include <filesystem>
#include <iomanip>
#include <algorithm>
#include <iostream>

struct ClassificationResult {
    std::string predicted_class;
    float confidence;
    std::vector<float> probabilities;
    std::vector<std::string> categories;
    bool success;
    std::string error_message;
    size_t word_count;
    
    ClassificationResult() : success(false), confidence(0.0f), word_count(0) {}
};

class FileProcessor {
public:
    static const int MAX_TEXT_WORDS = 1000;
    static const int MAX_FILENAME_WORDS = 32;
    
    /**
     * 处理单个文件并返回分类结果
     * @param filepath 文件路径
     * @param extractor Python文本提取器实例
     * @param text_segmenter 文本分词器实例
     * @param text_vocab 文本词汇表
     * @param filename_vocab 文件名词汇表
     * @param model_path ONNX模型路径
     * @return ClassificationResult 分类结果
     */
    static ClassificationResult processFile(
        const std::string& filepath,
        PythonTextExtractor& extractor,
        TextProcessor::TextSegmenter& text_segmenter,
        Vocab::TextVocab& text_vocab,
        Vocab::FilenameVocab& filename_vocab,
        const std::string& model_path = "./textcnn_classifier.onnx"
    ) {
        ClassificationResult result;
        
        try {
            // 1. 提取文本
            std::string text = extractor.extractText(filepath);
            if (text.empty()) {
                result.error_message = "文本提取失败: " + extractor.getLastError();
                return result;
            }
            
            // 2. 清洗文本
            text = TextProcessor::cleanText(text);
            
            // 3. 分词
            std::vector<std::string> words = text_segmenter.cutWords(text);
            result.word_count = words.size();
            
            // 4. 文本转索引
            std::vector<int32_t> text_hashes;
            text_hashes.reserve(MAX_TEXT_WORDS);
            for (int i = 0; i < MAX_TEXT_WORDS; ++i) {
                int32_t hash;
                if (i >= words.size()) {
                    hash = 0;
                } else {
                    hash = text_vocab.getId(words[i]);
                    if (hash >= 20000 || hash < 0) {
                        hash = 1;
                    }
                }
                text_hashes.push_back(hash);
            }
            
            // 5. 文件名处理
            std::filesystem::path p(filepath);
            std::string filename = p.stem().string();
            
            TextProcessor::FilenameSegmenter filename_segmenter(&text_segmenter);
            std::vector<std::string> filename_words = filename_segmenter.cutFilename(filename);
            
            std::vector<int32_t> filename_hashes;
            filename_hashes.reserve(MAX_FILENAME_WORDS);
            for (int i = 0; i < MAX_FILENAME_WORDS; ++i) {
                int32_t hash;
                if (i >= filename_words.size()) {
                    hash = 0;
                } else {
                    hash = filename_vocab.getId(filename_words[i]);
                    if (hash >= 20000 || hash < 0) {
                        hash = 1;
                    }
                }
                filename_hashes.push_back(hash);
            }
            
            // 6. ONNX Runtime 推理
            Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "file_classifier");
            Ort::SessionOptions session_options;
            session_options.SetIntraOpNumThreads(1);
            session_options.SetGraphOptimizationLevel(
                GraphOptimizationLevel::ORT_ENABLE_ALL);
            
            OrtSessionOptionsAppendExecutionProvider_CPU(session_options, 0);
            
            Ort::Session session(env, model_path.c_str(), session_options);
            
            // 输入输出名称
            std::vector<const char*> input_names = {"text_input", "filename_input"};
            std::vector<const char*> output_names = {"output"};
            
            // 创建内存信息
            Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
                OrtArenaAllocator, OrtMemTypeDefault);
            
            // 准备输入张量
            std::vector<int64_t> text_shape = {1, MAX_TEXT_WORDS};
            std::vector<int64_t> filename_shape = {1, MAX_FILENAME_WORDS};
            
            Ort::Value text_tensor = Ort::Value::CreateTensor<int32_t>(
                memory_info, 
                text_hashes.data(), 
                text_hashes.size(),
                text_shape.data(), 
                text_shape.size()
            );
            
            Ort::Value filename_tensor = Ort::Value::CreateTensor<int32_t>(
                memory_info,
                filename_hashes.data(),
                filename_hashes.size(),
                filename_shape.data(),
                filename_shape.size()
            );
            
            std::vector<Ort::Value> input_tensors;
            input_tensors.reserve(2);
            input_tensors.push_back(std::move(text_tensor));
            input_tensors.push_back(std::move(filename_tensor));
            
            // 执行推理
            Ort::RunOptions run_options;
            run_options.SetRunLogVerbosityLevel(0);
            run_options.SetRunTag("inference");
            
            auto output_tensors = session.Run(
                run_options,
                input_names.data(),
                input_tensors.data(),
                input_tensors.size(),
                output_names.data(),
                output_names.size()
            );
            
            if (output_tensors.empty()) {
                result.error_message = "推理失败：没有输出";
                return result;
            }
            
            auto& output_tensor = output_tensors[0];
            float* output_data = output_tensor.GetTensorMutableData<float>();
            size_t output_size = output_tensor.GetTensorTypeAndShapeInfo().GetElementCount();
            
            result.probabilities.assign(output_data, output_data + output_size);
            result.categories = Config::ModelConfig::CATEGORIES;
            
            // 找到预测类别
            int pred_idx = 0;
            float max_prob = result.probabilities[0];
            for (size_t i = 1; i < result.probabilities.size(); ++i) {
                if (result.probabilities[i] > max_prob) {
                    max_prob = result.probabilities[i];
                    pred_idx = i;
                }
            }
            
            result.predicted_class = (pred_idx < Config::ModelConfig::CATEGORIES.size()) ? 
                                     Config::ModelConfig::CATEGORIES[pred_idx] : "未知";
            result.confidence = max_prob;
            result.success = true;
            
        } catch (const Ort::Exception& e) {
            result.error_message = "ONNX Runtime错误: " + std::string(e.what()) + 
                                  " (代码: " + std::to_string(e.GetOrtErrorCode()) + ")";
        } catch (const std::exception& e) {
            result.error_message = "标准错误: " + std::string(e.what());
        }
        
        return result;
    }
    
    /**
     * 打印分类结果
     */
    static void printResult(const ClassificationResult& result, const std::string& filepath) {
        if (!result.success) {
            std::cerr << "❌ 处理失败: " << result.error_message << std::endl;
            return;
        }
        
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "文件: " << filepath << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        std::cout << "\n🎯 预测结果: " << result.predicted_class << std::endl;
        std::cout << "📊 置信度: " << std::fixed << std::setprecision(2) 
                  << result.confidence * 100 << "%" << std::endl;
        
        printProbabilities(result.probabilities, result.categories);
    }
    
    /**
     * 打印概率分布
     */
    static void printProbabilities(const std::vector<float>& probs, 
                                   const std::vector<std::string>& categories) {
        std::cout << "\n📈 详细分类概率:" << std::endl;
        
        std::vector<int> indices(probs.size());
        for (size_t i = 0; i < indices.size(); ++i) indices[i] = i;
        
        std::sort(indices.begin(), indices.end(), 
                  [&probs](int a, int b) { return probs[a] > probs[b]; });
        
        for (int idx : indices) {
            float prob = probs[idx];
            std::string cat = (idx < categories.size()) ? categories[idx] : "未知";
            int bar_length = static_cast<int>(prob * 30);
            std::cout << "   " << cat << ": " 
                      << std::fixed << std::setprecision(2) << prob * 100 << "% ";
            for (int i = 0; i < bar_length; ++i) std::cout << "█";
            std::cout << std::endl;
        }
    }
};
