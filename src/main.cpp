#include "python_extractor.h"
#include "text_processor.h"
#include "text_vocab_hash.h"
#include "filename_vocab_hash.h"
#include "model_config.h"
#include <iostream>
#include <stdlib.h>
#include <onnxruntime_cxx_api.h>
#include <cpu_provider_factory.h>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <algorithm>

const int MAX_TEXT_WORDS = 1000;
const int MAX_FILENAME_WORDS = 32;

// 读取类别文件
std::vector<std::string> loadCategories(const std::string& filepath) {
    std::vector<std::string> categories;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "无法打开类别文件: " << filepath << std::endl;
        return categories;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            categories.push_back(line);
        }
    }
    file.close();
    return categories;
}

// 打印概率分布
void printProbabilities(const std::vector<float>& probs, 
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

int main() {
    // 1. 设置 Python 模块路径
    const char* venv_site_packages = ".venv/lib/python3.14/site-packages";
    #ifdef _WIN32
        _putenv_s("PYTHONPATH", venv_site_packages);
    #else
        setenv("PYTHONPATH", venv_site_packages, 1);
    #endif

    // 2. 初始化 Python 文本提取器
    PythonTextExtractor extractor;
    if (!extractor.isInitialized()) {
        std::cerr << "PythonTextExtractor 初始化失败: " << extractor.getLastError() << std::endl;
        return 1;
    }
    
    std::string filepath = "拓展9.pptx";
    std::string text = extractor.extractText(filepath);
    
    if (text.empty()) {
        std::cerr << "文本提取失败: " << extractor.getLastError() << std::endl;
        return 1;
    }

    // 3. 清洗文本
    text = TextProcessor::cleanText(text);

    // 4. 初始化分词器并分词
    TextProcessor::TextSegmenter text_segmenter(
        "./dict/jieba.dict.utf8",
        "./dict/hmm_model.utf8",
        "./dict/user.dict.utf8",
        "./dict/idf.utf8",
        "./dict/stop_words.utf8"
    );

    std::vector<std::string> words = text_segmenter.cutWords(text);
    // 注释掉调试输出
    // std::cout << "提取的文本分词结果:" << std::endl;
    // std::cout << "词数: " << words.size() << std::endl;

    // 5. 文本转索引
    Vocab::TextVocab text_vocab;
    std::vector<int32_t> text_hashes;
    text_hashes.reserve(MAX_TEXT_WORDS);
    int32_t hash;

    for (int i = 0; i < MAX_TEXT_WORDS; ++i) {
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
    // std::cout << "文本索引完成" << std::endl;

    // 6. 文件名处理
    std::filesystem::path p(filepath);
    std::string filename = p.stem().string();

    TextProcessor::FilenameSegmenter filename_segmenter(&text_segmenter);
    std::vector<std::string> filename_words = filename_segmenter.cutFilename(filename);

    Vocab::FilenameVocab filename_vocab;
    std::vector<int32_t> filename_hashes;
    filename_hashes.reserve(MAX_FILENAME_WORDS);
    for (int i = 0; i < MAX_FILENAME_WORDS; ++i) {
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
    // std::cout << "文件名索引完成" << std::endl;

    // ============ 7. ONNX Runtime 推理 ============
    // std::cout << "\n正在加载ONNX模型..." << std::endl;

    try {
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "file_classifier");
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(
            GraphOptimizationLevel::ORT_ENABLE_ALL);  // 改为 ALL 优化
        
        // 设置CPU执行提供者
        OrtSessionOptionsAppendExecutionProvider_CPU(session_options, 0);
        
        const char* model_path = "./textcnn_classifier.onnx";
        Ort::Session session(env, model_path, session_options);

        // ============================================================
        // 直接使用已知的名称，避免动态获取
        // ============================================================
        std::vector<const char*> input_names = {"text_input", "filename_input"};
        std::vector<const char*> output_names = {"output"};
        
        // 注释掉调试信息获取
        // Ort::AllocatorWithDefaultOptions allocator;
        // size_t num_inputs = session.GetInputCount();
        // size_t num_outputs = session.GetOutputCount();
        // std::cout << "模型输入数量: " << num_inputs << std::endl;
        // std::cout << "模型输出数量: " << num_outputs << std::endl;
        // 
        // for (size_t i = 0; i < num_inputs; ++i) {
        //     auto name = session.GetInputNameAllocated(i, allocator);
        //     std::cout << "输入 " << i << ": '" << name.get() << "'" << std::endl;
        // }
        // for (size_t i = 0; i < num_outputs; ++i) {
        //     auto name = session.GetOutputNameAllocated(i, allocator);
        //     std::cout << "输出 " << i << ": '" << name.get() << "'" << std::endl;
        // }

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
        // std::cout << "\n开始推理..." << std::endl;
        
        Ort::RunOptions run_options;
        run_options.SetRunLogVerbosityLevel(0);
        run_options.SetRunTag("inference");
        
        // 注释掉调试信息
        // std::cout << "输入名称数组: ";
        // for (auto name : input_names) std::cout << name << " ";
        // std::cout << std::endl;
        // std::cout << "输出名称数组: ";
        // for (auto name : output_names) std::cout << name << " ";
        // std::cout << std::endl;
        // std::cout << "输入张量数量: " << input_tensors.size() << std::endl;
        
        // 执行推理
        auto output_tensors = session.Run(
            run_options,
            input_names.data(),
            input_tensors.data(),
            input_tensors.size(),
            output_names.data(),
            output_names.size()
        );
        
        // 获取输出
        if (output_tensors.empty()) {
            std::cerr << "推理失败：没有输出" << std::endl;
            return 1;
        }
        
        auto& output_tensor = output_tensors[0];
        auto output_shape = output_tensor.GetTensorTypeAndShapeInfo().GetShape();
        // 注释掉输出形状信息
        // std::cout << "输出形状: ";
        // for (auto dim : output_shape) std::cout << dim << " ";
        // std::cout << std::endl;
        
        float* output_data = output_tensor.GetTensorMutableData<float>();
        size_t output_size = output_tensor.GetTensorTypeAndShapeInfo().GetElementCount();
        // std::cout << "输出大小: " << output_size << std::endl;
        
        std::vector<float> probabilities(output_data, output_data + output_size);
        
        // 注释掉原始输出
        // std::cout << "原始输出: ";
        // for (size_t i = 0; i < std::min(size_t(10), output_size); ++i) {
        //     std::cout << probabilities[i] << " ";
        // }
        // std::cout << std::endl;
        
        // 找到预测类别
        int pred_idx = 0;
        float max_prob = probabilities[0];
        for (size_t i = 1; i < probabilities.size(); ++i) {
            if (probabilities[i] > max_prob) {
                max_prob = probabilities[i];
                pred_idx = i;
            }
        }
        
        // 打印结果
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "文件: " << filepath << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        // 注释掉文本长度
        // std::cout << "📝 文本长度: " << words.size() << " 词" << std::endl;
        
        std::string pred_class = (pred_idx < Config::ModelConfig::CATEGORIES.size()) ? 
                                 Config::ModelConfig::CATEGORIES[pred_idx] : "未知";
        std::cout << "\n🎯 预测结果: " << pred_class << std::endl;
        std::cout << "📊 置信度: " << std::fixed << std::setprecision(2) 
                  << max_prob * 100 << "%" << std::endl;
        
        printProbabilities(probabilities, Config::ModelConfig::CATEGORIES);
        
    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX Runtime错误: " << e.what() << std::endl;
        std::cerr << "错误代码: " << e.GetOrtErrorCode() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "标准错误: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}