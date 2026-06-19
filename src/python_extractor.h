#pragma once

#include <Python.h>
#include <string>
#include <vector>

/**
 * Python文本提取器类
 * 封装了对 extract.py 中 extract_text_from_file 函数的调用
 */
class PythonTextExtractor {
public:
    /**
     * 构造函数 - 初始化 Python 解释器
     * @param python_script_dir: extract.py 所在目录，默认为 "python"
     */
    PythonTextExtractor(const std::string& python_script_dir = "python");
    
    /**
     * 析构函数 - 释放资源
     */
    ~PythonTextExtractor();
    
    /**
     * 从文件中提取文本
     * @param filepath: 文件路径
     * @param extract_embedded: 是否提取嵌入的DOCX内容（默认true）
     * @return: 提取的文本内容，失败时返回空字符串
     */
    std::string extractText(const std::string& filepath, bool extract_embedded = true);
    
    /**
     * 检查是否支持该文件格式
     * @param filepath: 文件路径
     * @return: 是否支持
     */
    bool isSupported(const std::string& filepath);
    
    /**
     * 获取支持的文件扩展名列表
     * @return: 扩展名列表
     */
    std::vector<std::string> getSupportedExtensions();
    
    /**
     * 获取支持的文件扩展名列表（字符串形式）
     * @return: 扩展名列表字符串
     */
    std::string getSupportedExtensionsString();
    
    /**
     * 检查模块是否加载成功
     */
    bool isInitialized() const { return m_initialized; }
    
    /**
     * 获取最后的错误信息
     */
    std::string getLastError() const { return m_lastError; }

private:
    PyObject* m_pModule;        // extract 模块对象
    PyObject* m_pFunc;          // extract_text_from_file 函数对象
    bool m_initialized;         // 初始化状态
    std::string m_lastError;    // 最后的错误信息
    std::string m_scriptDir;    // extract.py 所在目录
    
    /**
     * 初始化 Python 环境和模块
     */
    bool initPython();
    
    /**
     * 清理 Python 资源
     */
    void cleanupPython();
    
    /**
     * 设置错误信息
     */
    void setError(const std::string& error);
};