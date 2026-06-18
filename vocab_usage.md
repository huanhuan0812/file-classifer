# 生成文件使用方法

## 文件总览

转换脚本会生成以下几类文件：

| 类型 | 文件 | 用途 |
|------|------|------|
| JSON | `config.json` | 模型配置参数 |
| JSON | `categories.json` | 类别标签列表 |
| JSON | `filename_tokenizer.json` | 文件名分词器配置 |
| JSON | `text_tokenizer.json` | 文本分词器配置 |
| 二进制 | `filename_vocab.bin` | 文件名词汇表（C++加载用） |
| 二进制 | `text_vocab.bin` | 文本词汇表（C++加载用） |
| C++头文件 | `filename_vocab_hash.h` | 文件名词汇表（哈希表，O(1)） |
| C++头文件 | `text_vocab_hash.h` | 文本词汇表（哈希表，O(1)） |
| C++头文件 | `filename_vocab_compact.h` | 文件名词汇表（二分查找，省内存） |
| C++头文件 | `text_vocab_compact.h` | 文本词汇表（二分查找，省内存） |
| C++头文件 | `model_config.h` | 模型配置（JSON字符串） |
| C++头文件 | `categories_data.h` | 类别数据（JSON字符串） |
| C++头文件 | `filename_tokenizer_data.h` | 文件名分词器（JSON字符串） |
| C++头文件 | `text_tokenizer_data.h` | 文本分词器（JSON字符串） |

---

## 一、JSON 文件使用（Python）

### 1. 加载配置
```python
import json

# 加载模型配置
with open('config.json', 'r', encoding='utf-8') as f:
    config = json.load(f)

print(f"模型类型: {config.get('model_type')}")
print(f"类别数: {config.get('num_classes')}")
```

### 2. 加载类别
```python
with open('categories.json', 'r', encoding='utf-8') as f:
    data = json.load(f)

categories = data['categories']
print(f"类别列表: {categories}")
print(f"类别数量: {data['num_categories']}")
```

### 3. 加载分词器
```python
with open('filename_tokenizer.json', 'r', encoding='utf-8') as f:
    tokenizer = json.load(f)

word_index = tokenizer['word_index']  # 词→ID 映射
index_word = tokenizer['index_word']  # ID→词 映射
vocab_size = tokenizer['vocab_size']  # 词汇表大小（截断后）

# 示例：将文本转换为ID序列
text = "hello world"
ids = [word_index.get(word, 1) for word in text.split()]  # 1 是 <UNK>
print(ids)
```

---

## 二、二进制词汇表使用（C++）

### 文件格式
```
| 字段 | 类型 | 说明 |
|------|------|------|
| vocab_size | uint32_t | 词汇表大小（小端序） |
| unk_id | uint32_t | UNK ID（固定为1） |
| 词条1 | uint32_t + uint16_t + bytes | ID + 词长度 + 词数据 |
| 词条2 | ... | ... |
```

### C++ 加载代码
```cpp
#include <fstream>
#include <vector>
#include <string>

struct VocabEntry {
    uint32_t id;
    std::string word;
};

class BinaryVocab {
public:
    bool load(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        
        // 读取头
        uint32_t vocab_size, unk_id;
        file.read(reinterpret_cast<char*>(&vocab_size), sizeof(uint32_t));
        file.read(reinterpret_cast<char*>(&unk_id), sizeof(uint32_t));
        
        // 读取词条
        for (size_t i = 0; i < vocab_size; i++) {
            uint32_t id;
            uint16_t len;
            file.read(reinterpret_cast<char*>(&id), sizeof(uint32_t));
            file.read(reinterpret_cast<char*>(&len), sizeof(uint16_t));
            
            std::string word(len, '\0');
            file.read(&word[0], len);
            
            word_to_id_[word] = id;
            id_to_word_[id] = word;
        }
        
        return true;
    }
    
    int getId(const std::string& word) const {
        auto it = word_to_id_.find(word);
        return it != word_to_id_.end() ? it->second : 1;  // UNK
    }
    
    const std::string& getWord(int id) const {
        auto it = id_to_word_.find(id);
        return it != id_to_word_.end() ? it->second : "<UNK>";
    }
    
private:
    std::unordered_map<std::string, uint32_t> word_to_id_;
    std::unordered_map<uint32_t, std::string> id_to_word_;
};

// 使用示例
BinaryVocab vocab;
if (vocab.load("filename_vocab.bin")) {
    int id = vocab.getId("main");
    std::cout << "ID: " << id << std::endl;
}
```

---

## 三、C++ 头文件使用

### 1. 哈希映射头文件（O(1) 查找）

```cpp
#include "filename_vocab_hash.h"

// 使用 Vocab::FilenameVocab
Vocab::FilenameVocab vocab;

// 获取词ID
int id = vocab.getId("main.cpp");  // 返回词ID，不存在则返回1（UNK）

// 获取ID对应的词
const std::string& word = vocab.getWord(42);

// 检查词是否存在
if (vocab.contains("config")) {
    // 处理...
}

// 获取词汇表大小
size_t size = vocab.size();
```

### 2. 紧凑型头文件（二分查找，省内存）

```cpp
#include "filename_vocab_compact.h"

// 使用 Vocab::FilenameVocabCompact
Vocab::FilenameVocabCompact vocab;

// 接口与哈希版本完全一致
int id = vocab.getId("main.cpp");
const std::string& word = vocab.getWord(42);
bool exists = vocab.contains("config");
```

**两种头文件对比：**

| 特性 | 哈希版 | 紧凑版 |
|------|--------|--------|
| 查找速度 | O(1) | O(log n) |
| 内存占用 | 较大 | 较小 |
| 适用场景 | 频繁查找 | 内存受限 |

---

## 四、JSON 数据头文件使用（C++）

这些头文件将 JSON 数据作为字符串嵌入到 C++ 代码中：

```cpp
#include "model_config.h"
#include "categories_data.h"
#include "filename_tokenizer_data.h"
#include "text_tokenizer_data.h"

#include <nlohmann/json.hpp>  // 需要 JSON 库

// 1. 加载配置
auto config = nlohmann::json::parse(ModelConfig::JSON_DATA);
std::string model_type = config["model_type"];
int num_classes = config["num_classes"];

// 2. 加载类别
auto categories_data = nlohmann::json::parse(CategoriesData::JSON_DATA);
std::vector<std::string> categories = categories_data["categories"];

// 3. 加载分词器
auto filename_tokenizer = nlohmann::json::parse(FilenameTokenizer::JSON_DATA);
auto word_index = filename_tokenizer["word_index"];

// 示例：文本转ID
std::string text = "hello world";
std::vector<int> ids;
for (const auto& word : split(text, ' ')) {
    int id = word_index.value(word, 1);  // 默认1为UNK
    ids.push_back(id);
}
```

---

## 五、完整使用示例（C++ 推理）

```cpp
#include <iostream>
#include <vector>
#include <nlohmann/json.hpp>
#include "filename_vocab_hash.h"
#include "text_vocab_hash.h"
#include "model_config.h"
#include "categories_data.h"

using json = nlohmann::json;

// 文本预处理
std::vector<std::string> tokenize(const std::string& text) {
    // 简单分词，实际可使用更复杂的分词器
    std::vector<std::string> tokens;
    std::string word;
    for (char c : text) {
        if (std::isspace(c) || std::ispunct(c)) {
            if (!word.empty()) {
                tokens.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) tokens.push_back(word);
    return tokens;
}

int main() {
    // 1. 加载词汇表
    Vocab::FilenameVocab filename_vocab;
    Vocab::TextVocab text_vocab;
    
    // 2. 加载配置
    json config = json::parse(ModelConfig::JSON_DATA);
    int max_seq_len = config["max_sequence_length"];
    
    // 3. 加载类别
    json cats = json::parse(CategoriesData::JSON_DATA);
    std::vector<std::string> categories = cats["categories"];
    
    // 4. 推理示例
    std::string filename = "main.cpp";
    std::string content = "int main() { return 0; }";
    
    // 转换为ID序列
    std::vector<int> filename_ids;
    for (const auto& token : tokenize(filename)) {
        filename_ids.push_back(filename_vocab.getId(token));
    }
    
    std::vector<int> text_ids;
    for (const auto& token : tokenize(content)) {
        text_ids.push_back(text_vocab.getId(token));
    }
    
    // 填充到固定长度
    while (filename_ids.size() < max_seq_len) {
        filename_ids.push_back(0);  // PAD
    }
    while (text_ids.size() < max_seq_len) {
        text_ids.push_back(0);
    }
    
    std::cout << "Filename IDs: ";
    for (int id : filename_ids) std::cout << id << " ";
    std::cout << std::endl;
    
    return 0;
}
```

---

## 六、注意事项

1. **词汇表截断**：所有词汇表已截断至 20000 词，超出部分的词会被映射为 `<UNK>`（ID=1）

2. **<PAD> 和 <UNK>**：
   - `<PAD>` ID = 0，用于填充序列
   - `<UNK>` ID = 1，用于未知词

3. **头文件选择**：
   - 嵌入式设备或内存受限 → 使用 `*_compact.h`
   - 服务器或性能优先 → 使用 `*_hash.h`

4. **JSON 解析**：使用 C++ 时需要引入 JSON 库（如 nlohmann/json）