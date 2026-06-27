cmake_minimum_required(VERSION 3.20)
project(FileClassifier VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# ========== 构建类型设置 ==========
# 默认使用 Release 模式（对于分类器性能更重要）
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

# ========== Qt6 设置 ==========
# 设置 CMAKE_PREFIX_PATH 以便 find_package 能找到 Qt6
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # 使用 Homebrew 的 Qt6 路径（同时支持 Apple Silicon 和 Intel）
    set(QT6_HOME "/opt/homebrew/opt/qt6")
    if(NOT EXISTS ${QT6_HOME})
        set(QT6_HOME "/usr/local/opt/qt6")
    endif()
    list(APPEND CMAKE_PREFIX_PATH "${QT6_HOME}")
endif()

# 查找 Qt6
find_package(Qt6 REQUIRED COMPONENTS 
    Core          # 核心功能
    Concurrent    # 并发支持（用于多线程处理）
    Widgets       # GUI 应用程序 (如果需要图形界面)
    GUI
)

# 启用 Qt6 的 CMake 自动特性
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

message(STATUS "========================================")
message(STATUS "Qt6 Configuration:")
message(STATUS "  Qt6 Version: ${Qt6_VERSION}")
message(STATUS "  Qt6 Prefix: ${Qt6_DIR}")
message(STATUS "  Qt6 Components: Core, Concurrent, Widgets, GUI")
message(STATUS "========================================")

# ========== 编译器优化设置 ==========
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    # 通用优化标志
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    
    # Release 模式优化
    set(CMAKE_CXX_FLAGS_RELEASE 
        "-O3 -DNDEBUG -march=native -mtune=native -funroll-loops -fomit-frame-pointer -pipe")
    
    # Debug 模式
    set(CMAKE_CXX_FLAGS_DEBUG 
        "-O0 -g -DDEBUG -fno-inline -fno-omit-frame-pointer")
    
    # RelWithDebInfo (带调试信息的优化)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 
        "-O2 -g -DNDEBUG -march=native -mtune=native")
    
    # MinSizeRel (最小体积)
    set(CMAKE_CXX_FLAGS_MINSIZEREL 
        "-Os -DNDEBUG -march=native -mtune=native")
    
    # 链接器优化
    if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-dead_strip -Wl,-no_implicit_dylibs")
    else()
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
    endif()
    
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # Windows MSVC
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Oi /GL /Gy /GT /DNDEBUG /MD /arch:AVX2")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /RTC1 /MDd /DDEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG")
endif()

# ========== 查找依赖 ==========
find_package(Python 3.14 REQUIRED COMPONENTS Development)

# ========== OpenMP ==========
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    message(STATUS "✅ OpenMP found: ${OpenMP_CXX_FLAGS}")
    set(USE_OPENMP TRUE)
else()
    message(STATUS "⚠️ OpenMP not found, compiling without OpenMP support")
    set(USE_OPENMP FALSE)
endif()

# ========== onnxruntime ==========
find_package(onnxruntime CONFIG)
if(NOT onnxruntime_FOUND)
    message(STATUS "⚠️ onnxruntime not found via CMake, trying pkg-config...")
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(ONNXRUNTIME onnxruntime)
    endif()
endif()

# ========== ICU ==========
# 在 macOS 上使用 Homebrew ICU
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # 尝试多个可能的 ICU 路径
    set(ICU_PATHS
        "/opt/homebrew/opt/icu4c@78"
        "/opt/homebrew/opt/icu4c"
        "/usr/local/opt/icu4c@78"
        "/usr/local/opt/icu4c"
    )
    foreach(ICU_PATH ${ICU_PATHS})
        if(EXISTS ${ICU_PATH})
            set(ICU_ROOT ${ICU_PATH})
            set(CMAKE_PREFIX_PATH ${ICU_PATH} ${CMAKE_PREFIX_PATH})
            set(ICU_INCLUDE_DIR ${ICU_PATH}/include CACHE PATH "ICU include directory" FORCE)
            break()
        endif()
    endforeach()
endif()

find_package(ICU COMPONENTS uc data i18n)
if(NOT ICU_FOUND)
    message(STATUS "⚠️ ICU not found, some features may be disabled")
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
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/yaml)

# ========== 源文件列表 ==========
set(SOURCES
    src/main.cpp
    src/python_extractor.cpp
    src/text_processor.cpp
    src/mainwindow.cpp
)

set(HEADERS
    src/python_extractor.h
    src/text_processor.h
    src/classifier.h
    src/mainwindow.h
)

# ========== 主程序 ==========
add_executable(file_classifier ${SOURCES} ${HEADERS})

# ========== 链接库 ==========
target_link_libraries(file_classifier
    ${Python_LIBRARIES}
    Qt6::Core
    Qt6::Concurrent
    Qt6::Widgets
    Qt6::Gui
)

target_include_directories(file_classifier PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

# 链接 onnxruntime
if(onnxruntime_FOUND)
    target_link_libraries(file_classifier onnxruntime::onnxruntime)
elseif(ONNXRUNTIME_FOUND)
    target_link_libraries(file_classifier ${ONNXRUNTIME_LIBRARIES})
    target_include_directories(file_classifier PRIVATE ${ONNXRUNTIME_INCLUDE_DIRS})
else()
    find_library(ONNXRUNTIME_LIB onnxruntime)
    if(ONNXRUNTIME_LIB)
        target_link_libraries(file_classifier ${ONNXRUNTIME_LIB})
        target_include_directories(file_classifier PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
    else()
        message(WARNING "⚠️ onnxruntime library not found, please ensure it's installed")
    endif()
endif()

# 链接 OpenMP
if(USE_OPENMP)
    target_link_libraries(file_classifier OpenMP::OpenMP_CXX)
    target_compile_options(file_classifier PRIVATE ${OpenMP_CXX_FLAGS})
endif()

# 链接 ICU
if(ICU_FOUND)
    target_link_libraries(file_classifier 
        ICU::uc 
        ICU::data 
        ICU::i18n
    )
endif()

# ========== 优化标志（针对特定目标） ==========
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(file_classifier PRIVATE
        $<$<CONFIG:Release>:-O3 -DNDEBUG -march=native -mtune=native>
        $<$<CONFIG:Debug>:-O0 -g>
    )
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

# ========== 打印配置信息 ==========
message(STATUS "========================================")
message(STATUS "Build Summary:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  C++ Flags (Release): ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "  C++ Flags (Debug): ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "  Qt6: ${Qt6_VERSION}")
message(STATUS "  OpenMP: ${USE_OPENMP}")
message(STATUS "  ONNX Runtime: ${onnxruntime_FOUND}")
message(STATUS "  ICU: ${ICU_FOUND}")
message(STATUS "========================================")

# ========== 添加编译时版本信息 ==========
target_compile_definitions(file_classifier PRIVATE
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:Debug>:DEBUG>
    PROJECT_VERSION="${PROJECT_VERSION}"
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
)

# ========== 安装目标 ==========
install(TARGETS file_classifier 
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
)