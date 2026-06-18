import pickle
import json
import struct
from pathlib import Path

# 词汇表最大大小
MAX_VOCAB_SIZE = 20000

def convert_config_to_json(pkl_path, output_path):
    """转换config_optimized.pkl到JSON"""
    with open(pkl_path, 'rb') as f:
        config = pickle.load(f)
    
    def convert_keys(obj):
        if isinstance(obj, dict):
            return {k.decode('utf-8') if isinstance(k, bytes) else k: convert_keys(v) 
                    for k, v in obj.items()}
        elif isinstance(obj, list):
            return [convert_keys(item) for item in obj]
        elif isinstance(obj, bytes):
            return obj.decode('utf-8')
        else:
            return obj
    
    config = convert_keys(config)
    
    for key in config:
        if isinstance(config[key], (set, frozenset)):
            config[key] = list(config[key])
    
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(config, f, ensure_ascii=False, indent=2)
    
    print(f"配置已保存到: {output_path}")

def convert_tokenizer_to_json(pkl_path, output_path):
    """转换tokenizer pkl到JSON"""
    with open(pkl_path, 'rb') as f:
        tokenizer = pickle.load(f)
    
    result = {}
    
    if 'word_index' in tokenizer:
        word_index = {}
        # 按值排序并截断到MAX_VOCAB_SIZE
        sorted_items = sorted(tokenizer['word_index'].items(), key=lambda x: x[1])
        for k, v in sorted_items[:MAX_VOCAB_SIZE]:
            key = k.decode('utf-8') if isinstance(k, bytes) else str(k)
            word_index[key] = int(v) if isinstance(v, (int, float)) else v
        result['word_index'] = word_index
    
    if 'index_word' in tokenizer:
        index_word = {}
        sorted_items = sorted(tokenizer['index_word'].items(), key=lambda x: x[0])
        for k, v in sorted_items[:MAX_VOCAB_SIZE]:
            key = int(k) if isinstance(k, (int, float)) else str(k)
            val = v.decode('utf-8') if isinstance(v, bytes) else str(v)
            index_word[key] = val
        result['index_word'] = index_word
    
    if 'word_counts' in tokenizer:
        word_counts = {}
        # 按词频从高到低排序，取前MAX_VOCAB_SIZE
        sorted_items = sorted(tokenizer['word_counts'].items(), key=lambda x: x[1], reverse=True)
        for k, v in sorted_items[:MAX_VOCAB_SIZE]:
            key = k.decode('utf-8') if isinstance(k, bytes) else str(k)
            word_counts[key] = int(v) if isinstance(v, (int, float)) else v
        result['word_counts'] = word_counts
    
    for key in ['document_count', 'vocab_size', 'max_words', 'is_truncated', 
                'original_vocab_size', 'max_sequence_length', 'max_filename_length']:
        if key in tokenizer:
            val = tokenizer[key]
            if isinstance(val, bytes):
                val = val.decode('utf-8')
            elif hasattr(val, 'item'):
                val = val.item()
            result[key] = val
    
    # 更新vocab_size为截断后的大小
    result['vocab_size'] = len(result.get('word_index', {}))
    
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
    
    print(f"Tokenizer已保存到: {output_path} (词汇表大小: {result['vocab_size']})")

def convert_categories_to_json(pkl_path, output_path):
    """转换categories.pkl到JSON"""
    with open(pkl_path, 'rb') as f:
        categories = pickle.load(f)
    
    if isinstance(categories, (list, tuple)):
        categories_list = list(categories)
    else:
        categories_list = [categories]
    
    categories_list = [
        c.decode('utf-8') if isinstance(c, bytes) else str(c) 
        for c in categories_list
    ]
    
    result = {
        'categories': categories_list,
        'num_categories': len(categories_list)
    }
    
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
    
    print(f"类别已保存到: {output_path}")

def get_truncated_word_index(data):
    """从数据中提取并截断word_index到MAX_VOCAB_SIZE"""
    if 'word_index' not in data:
        return None, 0
    
    # 提取所有词条
    word_index = {}
    for k, v in data['word_index'].items():
        key = k.decode('utf-8') if isinstance(k, bytes) else str(k)
        word_index[key] = int(v)
    
    # 按ID排序并截断
    sorted_items = sorted(word_index.items(), key=lambda x: x[1])
    truncated_items = sorted_items[:MAX_VOCAB_SIZE]
    
    return dict(truncated_items), len(truncated_items)

def create_binary_vocab(pkl_path, output_path, name="vocab"):
    """创建二进制词汇表（截断到MAX_VOCAB_SIZE）"""
    with open(pkl_path, 'rb') as f:
        data = pickle.load(f)
    
    word_index, vocab_size = get_truncated_word_index(data)
    if word_index is None:
        print(f"警告: {pkl_path} 中没有 word_index")
        return False
    
    with open(output_path, 'wb') as f:
        f.write(struct.pack('<I', vocab_size))
        f.write(struct.pack('<I', 1))  # UNK索引
        
        for word, idx in word_index.items():
            f.write(struct.pack('<I', idx))
            word_bytes = word.encode('utf-8')
            f.write(struct.pack('<H', len(word_bytes)))
            f.write(word_bytes)
    
    print(f"二进制词汇表已保存到: {output_path} (词汇表大小: {vocab_size})")
    return True

def create_hash_map_header(pkl_path, output_path, struct_name, namespace="Vocab"):
    """生成哈希映射头文件 - 包含完整的word_index映射表（截断到MAX_VOCAB_SIZE）"""
    with open(pkl_path, 'rb') as f:
        data = pickle.load(f)
    
    word_index, vocab_size = get_truncated_word_index(data)
    if word_index is None:
        print(f"警告: {pkl_path} 中没有 word_index")
        return False
    
    # 生成头文件内容
    header_content = f'''#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace {namespace} {{

// {struct_name} - 词汇表哈希映射
// 自动生成，包含 {vocab_size} 个词条（截断至 {MAX_VOCAB_SIZE}）
class {struct_name} {{
public:
    {struct_name}() {{
        init();
    }}
    
    // 获取词对应的ID
    inline int getId(const std::string& word) const {{
        auto it = word_to_id_.find(word);
        if (it != word_to_id_.end()) {{
            return it->second;
        }}
        return UNK_ID;  // <UNK>
    }}
    
    // 获取ID对应的词
    inline const std::string& getWord(int id) const {{
        if (id >= 0 && id < static_cast<int>(id_to_word_.size())) {{
            return id_to_word_[id];
        }}
        return UNK_WORD;
    }}
    
    // 检查词是否在词汇表中
    inline bool contains(const std::string& word) const {{
        return word_to_id_.find(word) != word_to_id_.end();
    }}
    
    // 获取词汇表大小
    inline size_t size() const {{
        return word_to_id_.size();
    }}
    
    // 获取UNK ID
    static constexpr int UNK_ID = 1;
    static const std::string UNK_WORD;
    
private:
    void init() {{
        // 预留空间
        word_to_id_.reserve({vocab_size});
        id_to_word_.resize({vocab_size + 1});
        id_to_word_[0] = "<PAD>";
        id_to_word_[1] = "<UNK>";
        
'''
    
    # 添加所有词条（按ID排序）
    for word, idx in sorted(word_index.items(), key=lambda x: x[1]):
        # 转义特殊字符
        escaped_word = word.replace('\\', '\\\\').replace('"', '\\"')
        header_content += f'        word_to_id_["{escaped_word}"] = {idx};\n'
        header_content += f'        id_to_word_[{idx}] = "{escaped_word}";\n'
    
    header_content += f'''    }}
    
    std::unordered_map<std::string, int> word_to_id_;
    std::vector<std::string> id_to_word_;
}};

// 静态成员定义
const std::string {struct_name}::UNK_WORD = "<UNK>";

}} // namespace {namespace}
'''
    
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(header_content)
    
    print(f"哈希映射头文件已保存到: {output_path}")
    print(f"  词汇表大小: {vocab_size}")
    return True

def create_compact_hash_header(pkl_path, output_path, struct_name, namespace="Vocab"):
    """生成紧凑型哈希映射头文件 - 使用数组和二分查找（节省内存，截断到MAX_VOCAB_SIZE）"""
    with open(pkl_path, 'rb') as f:
        data = pickle.load(f)
    
    word_index, vocab_size = get_truncated_word_index(data)
    if word_index is None:
        print(f"警告: {pkl_path} 中没有 word_index")
        return False
    
    # 按ID排序
    sorted_words = sorted(word_index.items(), key=lambda x: x[1])
    
    # 生成头文件
    header_content = f'''#pragma once
#include <string>
#include <algorithm>
#include <vector>

namespace {namespace} {{

// {struct_name} - 紧凑型词汇表
// 使用排序数组 + 二分查找，内存占用更小
// 截断至 {MAX_VOCAB_SIZE} 个词条
class {struct_name} {{
public:
    {struct_name}() {{
        init();
    }}
    
    inline int getId(const std::string& word) const {{
        auto it = std::lower_bound(words_.begin(), words_.end(), word,
            [](const Entry& a, const std::string& b) {{
                return a.word < b;
            }});
        if (it != words_.end() && it->word == word) {{
            return it->id;
        }}
        return UNK_ID;
    }}
    
    inline const std::string& getWord(int id) const {{
        if (id >= 0 && id < static_cast<int>(words_.size())) {{
            return words_[id].word;
        }}
        return UNK_WORD;
    }}
    
    inline bool contains(const std::string& word) const {{
        return getId(word) != UNK_ID;
    }}
    
    inline size_t size() const {{ return words_.size(); }}
    
    static constexpr int UNK_ID = 1;
    static const std::string UNK_WORD;
    
private:
    struct Entry {{
        std::string word;
        int id;
        
        bool operator<(const std::string& other) const {{
            return word < other;
        }}
    }};
    
    void init() {{
        words_.reserve({vocab_size});
'''
    
    # 添加所有词条（按ID排序）
    for word, idx in sorted_words:
        escaped_word = word.replace('\\', '\\\\').replace('"', '\\"')
        header_content += f'        words_.push_back({{"{escaped_word}", {idx}}});\n'
    
    header_content += f'''    }}
    
    std::vector<Entry> words_;
}};

const std::string {struct_name}::UNK_WORD = "<UNK>";

}} // namespace {namespace}
'''
    
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(header_content)
    
    print(f"紧凑型哈希头文件已保存到: {output_path} (词汇表大小: {vocab_size})")
    return True

def create_cpp_header(json_path, header_path, namespace):
    """生成C++头文件，包含JSON数据的字符串定义"""
    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    json_str = json.dumps(data, ensure_ascii=False, indent=2)
    
    header_content = f'''#pragma once
#include <string>

namespace {namespace} {{
    const std::string JSON_DATA = R"({json_str})";
}};
'''
    with open(header_path, 'w', encoding='utf-8') as f:
        f.write(header_content)
    
    print(f"C++头文件已保存到: {header_path}")

def main():
    print(f"词汇表最大大小: {MAX_VOCAB_SIZE}")
    print("-" * 60)
    
    # 转换配置文件
    convert_config_to_json('config_optimized.pkl', 'config.json')
    
    # 转换categories
    convert_categories_to_json('categories.pkl', 'categories.json')
    
    # 转换tokenizers
    convert_tokenizer_to_json('filename_tokenizer_none.pkl', 'filename_tokenizer.json')
    convert_tokenizer_to_json('text_tokenizer_none.pkl', 'text_tokenizer.json')
    
    # 创建二进制词汇表
    create_binary_vocab('filename_tokenizer_none.pkl', 'filename_vocab.bin', 'filename_vocab')
    create_binary_vocab('text_tokenizer_none.pkl', 'text_vocab.bin', 'text_vocab')
    
    # 生成哈希映射头文件（完整版）
    create_hash_map_header('filename_tokenizer_none.pkl', 'filename_vocab_hash.h', 
                           'FilenameVocab', 'Vocab')
    create_hash_map_header('text_tokenizer_none.pkl', 'text_vocab_hash.h', 
                           'TextVocab', 'Vocab')
    
    # 生成紧凑型哈希映射头文件（节省内存）
    create_compact_hash_header('filename_tokenizer_none.pkl', 'filename_vocab_compact.h', 
                               'FilenameVocabCompact', 'Vocab')
    create_compact_hash_header('text_tokenizer_none.pkl', 'text_vocab_compact.h', 
                               'TextVocabCompact', 'Vocab')
    
    # 生成C++头文件（JSON数据）
    create_cpp_header('config.json', 'model_config.h', 'ModelConfig')
    create_cpp_header('categories.json', 'categories_data.h', 'CategoriesData')
    create_cpp_header('filename_tokenizer.json', 'filename_tokenizer_data.h', 'FilenameTokenizer')
    create_cpp_header('text_tokenizer.json', 'text_tokenizer_data.h', 'TextTokenizer')
    
    print("\n" + "="*60)
    print("转换完成！生成的文件：")
    print("="*60)
    print("\n📄 JSON格式文件：")
    print("  - config.json")
    print("  - categories.json")
    print("  - filename_tokenizer.json")
    print("  - text_tokenizer.json")
    print("\n💾 二进制格式文件：")
    print("  - filename_vocab.bin")
    print("  - text_vocab.bin")
    print("\n📦 哈希映射头文件（完整版，O(1)查找）：")
    print("  - filename_vocab_hash.h")
    print("  - text_vocab_hash.h")
    print("\n📦 紧凑型哈希头文件（节省内存）：")
    print("  - filename_vocab_compact.h")
    print("  - text_vocab_compact.h")
    print("\n📋 C++ JSON数据头文件：")
    print("  - model_config.h")
    print("  - categories_data.h")
    print("  - filename_tokenizer_data.h")
    print("  - text_tokenizer_data.h")
    print("="*60)
    print(f"\n⚠️  所有词汇表已截断至 {MAX_VOCAB_SIZE} 个词条")

if __name__ == '__main__':
    main()