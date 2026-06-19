cmake_minimum_required(VERSION 3.20)
project(FileClassifier VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ========== 构建类型设置 ==========
# 默认使用 Release 模式
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING 
        "Choose the type of build: Debug Release RelWithDebInfo MinSizeRel" 
        FORCE)
endif()

message(STATUS "========================================")
message(STATUS "Build Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "========================================")

# ========== 编译器优化设置 ==========
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    # 通用优化标志
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    
    # Release 模式优化
    set(CMAKE_CXX_FLAGS_RELEASE 
        "-O3 -DNDEBUG -march=native -mtune=native -flto -funroll-loops -fomit-frame-pointer -pipe")
    
    # Debug 模式
    set(CMAKE_CXX_FLAGS_DEBUG 
        "-O0 -g -DDEBUG -fno-inline -fno-omit-frame-pointer")
    
    # RelWithDebInfo (带调试信息的优化)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 
        "-O2 -g -DNDEBUG -march=native -mtune=native")
    
    # MinSizeRel (最小体积)
    set(CMAKE_CXX_FLAGS_MINSIZEREL 
        "-Os -DNDEBUG -march=native -mtune=native")
    
    # 针对 Intel 7代 i7 的特定优化
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mavx2 -mfma -m64")
    endif()
    
    # macOS 特定优化 (Apple Silicon)
    if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
        # 检测是否为 Apple Silicon
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mcpu=apple-m1 -mtune=apple-m1")
        endif()
        # 启用 LTO (macOS 使用 thin LTO)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto=thin")
    endif()
    
    # Windows MSVC
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # Release 模式
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Oi /GL /Gy /GT /DNDEBUG /MD")
    
    # Debug 模式
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /RTC1 /MDd /DDEBUG")
    
    # 开启 LTCG (链接时代码生成)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG")
endif()

# ========== 跨平台链接时优化 ==========
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    # 添加 LTO 链接标志
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
    endif()
endif()

# ========== 查找依赖 ==========
find_package(Python 3.14 REQUIRED COMPONENTS Development)

# ========== OpenMP (可选) ==========
# macOS 上 OpenMP 可能不可用，设为可选
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    message(STATUS "✅ OpenMP found: ${OpenMP_CXX_FLAGS}")
    set(USE_OPENMP TRUE)
else()
    message(STATUS "⚠️ OpenMP not found, compiling without OpenMP support")
    set(USE_OPENMP FALSE)
endif()

# ========== onnxruntime ==========
# 尝试查找 onnxruntime，如果找不到则提示
find_package(onnxruntime CONFIG)
if(NOT onnxruntime_FOUND)
    message(STATUS "⚠️ onnxruntime not found via CMake, trying pkg-config...")
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(ONNXRUNTIME onnxruntime)
    endif()
endif()

# ========== ICU ==========
# 只在 macOS 上强制使用 Homebrew ICU
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # 检测是否在 Apple Silicon 上
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(ICU_ROOT "/opt/homebrew/opt/icu4c@78")
        set(CMAKE_PREFIX_PATH "/opt/homebrew/opt/icu4c@78" ${CMAKE_PREFIX_PATH})
        set(ICU_INCLUDE_DIR "/opt/homebrew/opt/icu4c@78/include" CACHE PATH "ICU include directory" FORCE)
    else()
        # Intel Mac
        set(ICU_ROOT "/usr/local/opt/icu4c")
        set(CMAKE_PREFIX_PATH "/usr/local/opt/icu4c" ${CMAKE_PREFIX_PATH})
        set(ICU_INCLUDE_DIR "/usr/local/opt/icu4c/include" CACHE PATH "ICU include directory" FORCE)
    endif()
endif()

find_package(ICU COMPONENTS uc data i18n)
if(NOT ICU_FOUND)
    message(STATUS "⚠️ ICU not found, some features may be disabled")
endif()

# ========== 添加 ICU 调试信息 ==========
if(ICU_FOUND)
    message(STATUS "========================================")
    message(STATUS "ICU Configuration:")
    message(STATUS "  ICU_ROOT: ${ICU_ROOT}")
    message(STATUS "  ICU_INCLUDE_DIR: ${ICU_INCLUDE_DIR}")
    message(STATUS "  ICU_LIBRARIES: ${ICU_LIBRARIES}")
    message(STATUS "========================================")
    
    if(ICU_INCLUDE_DIR AND EXISTS ${ICU_INCLUDE_DIR}/unicode/utypes.h)
        message(STATUS "  ✅ ICU header found")
    else()
        message(WARNING "  ❌ ICU header NOT found")
    endif()
endif()

# ========== cppjieba 作为子模块 ==========
set(CPPJIEGBA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/cppjieba)
if(NOT EXISTS ${CPPJIEGBA_DIR}/include/cppjieba/Jieba.hpp)
    message(FATAL_ERROR "cppjieba not found. Please run: git submodule update --init --recursive")
endif()

# ========== 包含目录 ==========
include_directories(${CPPJIEGBA_DIR}/include)
include_directories(${Python_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/models)

# ========== 源文件列表 ==========
set(SOURCES
    src/main.cpp
    src/python_extractor.cpp
    src/text_processor.cpp
)

set(HEADERS
    src/python_extractor.h
    src/text_processor.h
)

# ========== 主程序 ==========
add_executable(file_classifier ${SOURCES} ${HEADERS})

# ========== 链接库 ==========
target_link_libraries(file_classifier
    ${Python_LIBRARIES}
)

# 链接 onnxruntime
if(onnxruntime_FOUND)
    target_link_libraries(file_classifier onnxruntime::onnxruntime)
elseif(ONNXRUNTIME_FOUND)
    target_link_libraries(file_classifier ${ONNXRUNTIME_LIBRARIES})
    target_include_directories(file_classifier PRIVATE ${ONNXRUNTIME_INCLUDE_DIRS})
else()
    # 尝试直接链接
    find_library(ONNXRUNTIME_LIB onnxruntime)
    if(ONNXRUNTIME_LIB)
        target_link_libraries(file_classifier ${ONNXRUNTIME_LIB})
        target_include_directories(file_classifier PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
    else()
        message(WARNING "⚠️ onnxruntime library not found, please ensure it's installed")
    endif()
endif()

# 链接 OpenMP (如果找到)
if(USE_OPENMP)
    target_link_libraries(file_classifier OpenMP::OpenMP_CXX)
    target_compile_options(file_classifier PRIVATE ${OpenMP_CXX_FLAGS})
endif()

# 链接 ICU (如果找到)
if(ICU_FOUND)
    target_link_libraries(file_classifier 
        ICU::uc 
        ICU::data 
        ICU::i18n
    )
endif()

# ========== 优化标志（针对特定目标） ==========
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    # 目标特定的优化
    target_compile_options(file_classifier PRIVATE
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
        $<$<CONFIG:Debug>:-O0 -g>
    )
    
    # 针对 Intel 7代 i7 优化
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        target_compile_options(file_classifier PRIVATE
            $<$<CONFIG:Release>:-march=skylake -mtune=skylake>
        )
    endif()
    
    # macOS Apple Silicon 优化
    if(CMAKE_SYSTEM_NAME MATCHES "Darwin" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        target_compile_options(file_classifier PRIVATE
            $<$<CONFIG:Release>:-mcpu=apple-m1 -mtune=apple-m1>
        )
    endif()
endif()

# ========== 复制必要文件到构建目录 ==========
# 模型文件
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/models/)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/models/ 
         DESTINATION ${CMAKE_BINARY_DIR}/models/)
endif()

# 词典文件
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/dict/)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/dict/ 
         DESTINATION ${CMAKE_BINARY_DIR}/dict/)
endif()

# ONNX 模型
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/textcnn_classifier.onnx)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/textcnn_classifier.onnx 
         DESTINATION ${CMAKE_BINARY_DIR}/)
endif()

# tokenizer 文件
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/text_tokenizer_none.pkl)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/text_tokenizer_none.pkl 
         DESTINATION ${CMAKE_BINARY_DIR}/)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/filename_tokenizer_none.pkl)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/filename_tokenizer_none.pkl 
         DESTINATION ${CMAKE_BINARY_DIR}/)
endif()

# categories 文件
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/categories.pkl)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/categories.pkl 
         DESTINATION ${CMAKE_BINARY_DIR}/)
endif()

# config 文件
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config_optimized.pkl)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config_optimized.pkl 
         DESTINATION ${CMAKE_BINARY_DIR}/)
endif()

# ========== 打印配置信息 ==========
message(STATUS "========================================")
message(STATUS "Build Summary:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  C++ Flags (Release): ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "  C++ Flags (Debug): ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "  OpenMP: ${USE_OPENMP}")
message(STATUS "  ONNX Runtime: ${onnxruntime_FOUND}")
message(STATUS "  ICU: ${ICU_FOUND}")
message(STATUS "========================================")

# ========== 添加编译时版本信息 ==========
target_compile_definitions(file_classifier PRIVATE
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:Debug>:DEBUG>
    PROJECT_VERSION="${PROJECT_VERSION}"
)

# ========== 安装目标（可选） ==========
install(TARGETS file_classifier 
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
)