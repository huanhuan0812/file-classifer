#include "python_extractor.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <stdlib.h>

// 静态变量用于跟踪 Python 是否已初始化
static bool g_python_initialized = false;

PythonTextExtractor::PythonTextExtractor(const std::string& python_script_dir)
    : m_pModule(nullptr)
    , m_pFunc(nullptr)
    , m_initialized(false)
    , m_scriptDir(python_script_dir)
{
    m_initialized = initPython();
}

PythonTextExtractor::~PythonTextExtractor()
{
    cleanupPython();
}

void PythonTextExtractor::setError(const std::string& error)
{
    m_lastError = error;
    std::cerr << "PythonTextExtractor 错误: " << error << std::endl;
}

bool PythonTextExtractor::initPython()
{
    // 如果已经初始化过，直接返回
    if (g_python_initialized) {
        // 重新获取模块引用
        PyObject* pModuleName = PyUnicode_FromString("extract");
        if (pModuleName) {
            m_pModule = PyImport_Import(pModuleName);
            Py_DECREF(pModuleName);
        }
        
        if (m_pModule) {
            m_pFunc = PyObject_GetAttrString(m_pModule, "extract_text_from_file");
            if (m_pFunc && PyCallable_Check(m_pFunc)) {
                return true;
            }
        }
        setError("无法重新获取 extract 模块");
        return false;
    }
    
    // 初始化 Python 解释器
    Py_Initialize();
    if (!Py_IsInitialized()) {
        setError("Python 初始化失败");
        return false;
    }
    
    g_python_initialized = true;
    
    // 添加 Python 脚本目录到 sys.path
    std::string addPathCmd = "import sys; sys.path.append('" + m_scriptDir + "')";
    PyRun_SimpleString("import sys");
    PyRun_SimpleString(addPathCmd.c_str());
    
    // 也添加当前目录作为备选
    PyRun_SimpleString("sys.path.append('.')");
    
    // 调试：打印 sys.path
    // PyRun_SimpleString("print('sys.path:', sys.path)");
    
    // 导入 extract 模块
    PyObject* pModuleName = PyUnicode_FromString("extract");
    if (!pModuleName) {
        setError("无法创建模块名");
        return false;
    }
    
    m_pModule = PyImport_Import(pModuleName);
    Py_DECREF(pModuleName);
    
    if (!m_pModule) {
        setError("无法导入 extract 模块，请确保 " + m_scriptDir + "/extract.py 存在");
        PyErr_Print();
        return false;
    }
    
    // 获取 extract_text_from_file 函数
    m_pFunc = PyObject_GetAttrString(m_pModule, "extract_text_from_file");
    if (!m_pFunc || !PyCallable_Check(m_pFunc)) {
        setError("无法获取 extract_text_from_file 函数");
        PyErr_Print();
        Py_DECREF(m_pModule);
        m_pModule = nullptr;
        return false;
    }
    
    return true;
}

void PythonTextExtractor::cleanupPython()
{
    // 释放 Python 对象
    if (m_pFunc) {
        Py_DECREF(m_pFunc);
        m_pFunc = nullptr;
    }
    
    if (m_pModule) {
        Py_DECREF(m_pModule);
        m_pModule = nullptr;
    }
    
    // 注意: 不要在析构函数中调用 Py_Finalize()
    // 因为其他实例可能还在使用 Python
    // 如果需要，可以在程序退出时调用 Py_Finalize()
}

std::string PythonTextExtractor::extractText(const std::string& filepath, bool extract_embedded)
{
    if (!m_initialized || !m_pFunc) {
        setError("Python 环境未初始化");
        return "";
    }
    
    PyObject* pArgs = nullptr;
    PyObject* pResult = nullptr;
    std::string result_text;
    
    try {
        // 创建参数 (filepath, extract_embedded)
        pArgs = Py_BuildValue("(si)", filepath.c_str(), extract_embedded ? 1 : 0);
        if (!pArgs) {
            setError("无法创建参数");
            PyErr_Print();
            return "";
        }
        
        // 调用函数
        pResult = PyObject_CallObject(m_pFunc, pArgs);
        if (!pResult) {
            setError("函数调用失败");
            PyErr_Print();
            Py_DECREF(pArgs);
            return "";
        }
        
        // 检查返回值类型
        if (!PyUnicode_Check(pResult)) {
            setError("返回值不是字符串");
            Py_DECREF(pResult);
            Py_DECREF(pArgs);
            return "";
        }
        
        // 转换为 C++ 字符串
        const char* utf8_str = PyUnicode_AsUTF8(pResult);
        if (utf8_str) {
            result_text = utf8_str;
        }
        
        // 清理
        Py_DECREF(pResult);
        Py_DECREF(pArgs);
        
    } catch (const std::exception& e) {
        setError(std::string("异常: ") + e.what());
        Py_XDECREF(pResult);
        Py_XDECREF(pArgs);
        return "";
    }
    
    return result_text;
}

bool PythonTextExtractor::isSupported(const std::string& filepath)
{
    if (!m_initialized || !m_pModule) {
        return false;
    }
    
    // 获取 get_supported_extensions 函数
    PyObject* pFunc = PyObject_GetAttrString(m_pModule, "get_supported_extensions");
    if (!pFunc || !PyCallable_Check(pFunc)) {
        Py_XDECREF(pFunc);
        return false;
    }
    
    PyObject* pResult = PyObject_CallObject(pFunc, nullptr);
    Py_DECREF(pFunc);
    
    if (!pResult || !PyList_Check(pResult)) {
        Py_XDECREF(pResult);
        return false;
    }
    
    // 检查文件扩展名是否在支持列表中
    std::string ext;
    size_t dot_pos = filepath.find_last_of('.');
    if (dot_pos != std::string::npos) {
        ext = filepath.substr(dot_pos);
        // 转换为小写以进行不区分大小写的比较
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    } else {
        Py_DECREF(pResult);
        return false;
    }
    
    Py_ssize_t list_size = PyList_Size(pResult);
    bool supported = false;
    for (Py_ssize_t i = 0; i < list_size; i++) {
        PyObject* item = PyList_GetItem(pResult, i);
        if (item && PyUnicode_Check(item)) {
            const char* item_str = PyUnicode_AsUTF8(item);
            if (item_str) {
                std::string ext_item = item_str;
                std::transform(ext_item.begin(), ext_item.end(), ext_item.begin(), ::tolower);
                if (ext == ext_item) {
                    supported = true;
                    break;
                }
            }
        }
    }
    
    Py_DECREF(pResult);
    return supported;
}

std::vector<std::string> PythonTextExtractor::getSupportedExtensions()
{
    std::vector<std::string> extensions;
    
    if (!m_initialized || !m_pModule) {
        return extensions;
    }
    
    PyObject* pFunc = PyObject_GetAttrString(m_pModule, "get_supported_extensions");
    if (!pFunc || !PyCallable_Check(pFunc)) {
        Py_XDECREF(pFunc);
        return extensions;
    }
    
    PyObject* pResult = PyObject_CallObject(pFunc, nullptr);
    Py_DECREF(pFunc);
    
    if (!pResult || !PyList_Check(pResult)) {
        Py_XDECREF(pResult);
        return extensions;
    }
    
    Py_ssize_t list_size = PyList_Size(pResult);
    for (Py_ssize_t i = 0; i < list_size; i++) {
        PyObject* item = PyList_GetItem(pResult, i);
        if (item && PyUnicode_Check(item)) {
            const char* item_str = PyUnicode_AsUTF8(item);
            if (item_str) {
                extensions.push_back(item_str);
            }
        }
    }
    
    Py_DECREF(pResult);
    return extensions;
}

std::string PythonTextExtractor::getSupportedExtensionsString()
{
    std::vector<std::string> exts = getSupportedExtensions();
    std::string result;
    for (size_t i = 0; i < exts.size(); i++) {
        if (i > 0) result += ", ";
        result += exts[i];
    }
    return result;
}