cmake_minimum_required(VERSION 3.20)
project(FileClassifier VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 查找依赖
find_package(Python 3.14 REQUIRED COMPONENTS Development)
find_package(OpenMP REQUIRED)
find_package(onnxruntime CONFIG REQUIRED)

# ========== 强制使用 Homebrew ICU ==========
set(ICU_ROOT "/opt/homebrew/opt/icu4c@78")
set(CMAKE_PREFIX_PATH "/opt/homebrew/opt/icu4c@78" ${CMAKE_PREFIX_PATH})
# 强制设置 ICU 头文件路径
set(ICU_INCLUDE_DIR "/opt/homebrew/opt/icu4c@78/include" CACHE PATH "ICU include directory" FORCE)
# ============================================

find_package(ICU REQUIRED COMPONENTS uc data i18n)

# ========== 添加 ICU 调试信息 ==========
message(STATUS "========================================")
message(STATUS "ICU Configuration:")
message(STATUS "  ICU_ROOT: ${ICU_ROOT}")
message(STATUS "  ICU_FOUND: ${ICU_FOUND}")
message(STATUS "  ICU_INCLUDE_DIR: ${ICU_INCLUDE_DIR}")
message(STATUS "  ICU_LIBRARIES: ${ICU_LIBRARIES}")
message(STATUS "  ICU_UC_LIBRARY: ${ICU_UC_LIBRARY}")
message(STATUS "  ICU_DATA_LIBRARY: ${ICU_DATA_LIBRARY}")
message(STATUS "  ICU_I18N_LIBRARY: ${ICU_I18N_LIBRARY}")
message(STATUS "========================================")

# 验证头文件路径
if(ICU_INCLUDE_DIR AND EXISTS ${ICU_INCLUDE_DIR}/unicode/utypes.h)
    message(STATUS "  ✅ ICU header found: ${ICU_INCLUDE_DIR}/unicode/utypes.h")
else()
    message(WARNING "  ❌ ICU header NOT found in ${ICU_INCLUDE_DIR}")
endif()
# =====================================

# cppjieba 作为子模块
set(CPPJIEGBA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/cppjieba)
if(NOT EXISTS ${CPPJIEGBA_DIR}/include/cppjieba/Jieba.hpp)
    message(FATAL_ERROR "cppjieba not found. Please run: git submodule update --init --recursive")
endif()

# 包含目录
include_directories(${CPPJIEGBA_DIR}/include)
include_directories(${Python_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/models)

# 主程序
add_executable(file_classifier
    src/main.cpp
    src/python_extractor.cpp
    src/text_processor.cpp
    src/python_extractor.h
    src/text_processor.h
)

target_link_libraries(file_classifier
    ${Python_LIBRARIES}
    ${Python_NumPy_LIBRARIES}
    onnxruntime::onnxruntime
    OpenMP::OpenMP_CXX
    ICU::uc
    ICU::data
    ICU::i18n
)

# 复制模型文件
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/models/ DESTINATION ${CMAKE_BINARY_DIR}/models/)