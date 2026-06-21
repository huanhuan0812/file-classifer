# pragma once
#include <rapidyaml.hpp>
#include <vector>
#include <string>

enum class FileHandlingMode {
    MOVE,
    COPY
};

enum class ConflictResolution {
    RENAME,
    SKIP
};

enum class LowConfidenceAction {
    IGNORE,
    MOVE_TO_UNCERTAIN
};

enum class TempFileHandling {
    DELETE,
    MOVE
};

enum class TempFileHandlTime{
    BEFORE_CLASSIFICATION,
    AFTER_CLASSIFICATION
};

enum class DuplicateHandling {
    DELETE,
    SKIP,
    KEEP_BOTH
}; 

enum class HashAlgorithm {
    MD5,
    SHA1,
    SHA256
};

class RunConfig {
private:
    // 路径
    std::string source_dir;
    std::string target_dir;

    // 分类阈值
    float threshold=0.98;

    // 文件处理方式
    FileHandlingMode file_handling_mode=FileHandlingMode::MOVE; // 文件处理方式: "move" 或 "copy"
    bool create_category_dirs=true; // 是否为每个类别创建子目录
    bool overwrite=false; // 是否覆盖已存在的文件
    bool keep_original_name=true; // 是否保持原文件名
    ConflictResolution conflict_resolution=ConflictResolution::SKIP; // 文件名冲突时的处理方式: "rename" 或 "skip"

    // 分类配置
    std::vector<std::string> categories; // 需要处理的类别（空=所有类别）
    std::vector<std::string> exclude_categories; // 需要排除的类别（空=不排除任何类别）
    LowConfidenceAction low_confidence_action=LowConfidenceAction::IGNORE; // 未达阈值文件的处理方式: "ignore" 或 "move_to_uncertain"
    std::string uncertain_dir_name="uncertain"; // 低置信度文件的目标子目录名称（仅当 low_confidence_action=MOVE_TO_UNCERTAIN 时有效）

    // 临时文件处理
    bool delete_temp_files=true; // 是否删除临时文件
    TempFileHandling temp_file_handling=TempFileHandling::DELETE; // 临时文件处理方式: "delete" 或 "move"
    std::string temp_dir_name="temp"; // 临时文件的目标子目录名称（仅当 temp_file_handling=MOVE 时有效）
    std::vector<std::string> patterns; // 临时文件的匹配模式
    int min_file_age=0; // 最小文件年龄 （min） ，=0表示不限制
    TempFileHandlTime temp_file_handl_time=TempFileHandlTime::BEFORE_CLASSIFICATION; // 临时文件处理时间点: "before_classification" 或 "after_classification"
    bool keep_empty_dirs=false; // 是否保留空目录

    // 重复文件处理
    bool handle_duplicates=false; // 是否处理重复文件
    DuplicateHandling duplicate_handling=DuplicateHandling::DELETE; // 重复文件处理方式: "delete"、"skip" 或 "keep_both"
    bool check_by_hash=true; // 是否通过文件哈希值检查重复（仅当 handle_duplicates=true 时有效）
    HashAlgorithm hash_algorithm=HashAlgorithm::MD5; // 用于重复检查的哈希算法: "md5"、"sha1" 或 "sha256"（仅当 check_by_hash=true 时有效）
    bool scan_all_flies=true; // 是否扫描所有文件检测重复（包括不同类别的）

};