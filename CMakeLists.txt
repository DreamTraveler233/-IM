cmake_minimum_required(VERSION 3.10)
project(IM)

# ==================== 依赖查找 ====================
# 查找 ccache 以加快编译速度
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# 必需的系统依赖
find_package(Threads REQUIRED)

# 第三方依赖 - 使用正确的包名和查找方式
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(jwt-cpp REQUIRED)

# 对于可能没有标准 CMake 配置的包，使用备用查找方式
find_package(PkgConfig REQUIRED)  # 启用 pkg-config 支持

# 使用 pkg-config 查找依赖
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(MYSQL REQUIRED mysqlclient)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
pkg_check_modules(LIBEVENT REQUIRED libevent)
pkg_check_modules(HIREDIS_VIP REQUIRED hiredis_vip)
pkg_check_modules(TINYXML2 REQUIRED tinyxml2)
# pkg_check_modules(MULTIPART_PARSER REQUIRED multipartparser)

# 查找 Ragel 状态机编译器
find_program(RAGEL_EXECUTABLE ragel)
if(NOT RAGEL_EXECUTABLE)
    message(FATAL_ERROR "Ragel not found. Please install ragel to compile this project.")
endif()

# ==================== 编译选项 ====================
# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optionally build the Qt Quick/QML client (XinYu-IM-QtClient)
option(ENABLE_QT_CLIENT "Build Qt Quick desktop client" OFF)

# 根据构建类型设置不同的编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -Wall -Wno-deprecated -Werror -Wno-unused-function")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG -Wall")
endif()

# 添加通用编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic")

# ==================== 代码生成 ====================
# 处理 .rl 文件，生成对应的 .cpp 文件
file(GLOB_RECURSE RL_SOURCES "src/*.rl")
foreach(RL_FILE ${RL_SOURCES})
    # 获取不带扩展名的文件名
    get_filename_component(FILE_DIR ${RL_FILE} DIRECTORY)
    get_filename_component(FILE_NAME ${RL_FILE} NAME_WE)
    set(CPP_FILE "${FILE_DIR}/${FILE_NAME}.rl.cpp")
    
    # 添加自定义命令，使用 ragel 生成 cpp 文件
    add_custom_command(
        OUTPUT ${CPP_FILE}
        COMMAND ${RAGEL_EXECUTABLE} -G2 -o ${CPP_FILE} ${RL_FILE}
        DEPENDS ${RL_FILE}
        COMMENT "Generating ${CPP_FILE} from ${RL_FILE}"
    )
    
    # 将生成的 cpp 文件添加到生成文件列表
    list(APPEND GENERATED_SOURCES ${CPP_FILE})
endforeach()

# 处理 .proto 文件，生成对应的 .pb.cc 和 .pb.h 文件
set(NS_PROTO_FILE ${PROJECT_SOURCE_DIR}/include/ns/ns_protobuf.proto)
set(NS_PROTO_SRC ${PROJECT_SOURCE_DIR}/include/ns/ns_protobuf.pb.cc)
set(NS_PROTO_HEADER ${PROJECT_SOURCE_DIR}/include/ns/ns_protobuf.pb.h)

add_custom_command(
    OUTPUT ${NS_PROTO_SRC} ${NS_PROTO_HEADER}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE} 
        --cpp_out=${PROJECT_SOURCE_DIR}/include/ns 
        -I=${PROJECT_SOURCE_DIR}/include/ns 
        ${NS_PROTO_FILE}
    DEPENDS ${NS_PROTO_FILE}
    COMMENT "Generating protobuf files from ${NS_PROTO_FILE}"
)

list(APPEND GENERATED_SOURCES ${NS_PROTO_SRC})

# ==================== 源文件收集 ====================
# 收集其他源文件（排除 .rl 文件）
file(GLOB_RECURSE OTHER_SOURCES "src/*.cpp")
list(FILTER OTHER_SOURCES EXCLUDE REGEX ".*\\.rl\\.cpp$")
# 排除包含可执行入口(main)的引导程序源码，避免被编进共享库
list(FILTER OTHER_SOURCES EXCLUDE REGEX ".*/bootstrap/.*")
# 排除测试源文件，避免被编进共享库（测试以单独可执行文件构建）
list(FILTER OTHER_SOURCES EXCLUDE REGEX ".*/tests/.*")

# 合并所有源文件
set(LIB_SRC ${OTHER_SOURCES} ${GENERATED_SOURCES})

# 显式添加 Roaring Bitmap 的 C 实现文件
list(APPEND LIB_SRC ${PROJECT_SOURCE_DIR}/src/ds/roaring.c)
# 显式添加 Multipart Parser 的 C 实现文件
list(APPEND LIB_SRC ${PROJECT_SOURCE_DIR}/src/infra/http/multipart_parser.c)

# ==================== 主库构建 ====================
# 生成动态库（.so）
add_library(IM SHARED ${LIB_SRC})

# 头文件搜索路径
target_include_directories(IM
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${SQLITE3_INCLUDE_DIRS}
        ${MYSQL_INCLUDE_DIRS}
        ${JSONCPP_INCLUDE_DIRS}
        ${LIBEVENT_INCLUDE_DIRS}
        ${HIREDIS_INCLUDE_DIRS}
        ${TINYXML2_INCLUDE_DIRS}
        # ${MULTIPART_PARSER_INCLUDE_DIRS}
)

# ==================== 依赖链接 ====================
# 使用现代 CMake 目标链接方式
target_link_libraries(IM
    PUBLIC
        Threads::Threads
        OpenSSL::SSL
        OpenSSL::Crypto
        ZLIB::ZLIB
        protobuf::libprotobuf
        yaml-cpp::yaml-cpp
        $<$<TARGET_EXISTS:jwt-cpp::jwt-cpp>:jwt-cpp::jwt-cpp>
    PRIVATE
        ${SQLITE3_LIBRARIES}
        ${MYSQL_LIBRARIES}
        ${JSONCPP_LIBRARIES}
        ${LIBEVENT_LIBRARIES}
        ${HIREDIS_VIP_LIBRARIES}
        ${TINYXML2_LIBRARIES}
        # ${MULTIPART_PARSER_LIBRARIES}
        /usr/lib/x86_64-linux-gnu/libzookeeper_mt.so
)

# 业务服务可执行文件
add_executable(im_server src/bootstrap/im_server.cpp)
add_dependencies(im_server IM)
target_link_libraries(im_server PRIVATE IM)

# ==================== 输出目录 ====================
# 指定可执行文件输出目录 按类型区分 (应用/测试)
set(BIN_APP_DIR ${PROJECT_SOURCE_DIR}/bin)
set(BIN_TEST_DIR ${PROJECT_SOURCE_DIR}/bin/tests)
file(MAKE_DIRECTORY ${BIN_APP_DIR})
file(MAKE_DIRECTORY ${BIN_TEST_DIR})

# 指定库文件输出路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 设置 app 可执行产物输出
set_target_properties(im_server PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BIN_APP_DIR})

# Add tests subdirectory (unit/integration), tests/ own CMakeLists handles targets
add_subdirectory(tests)

if(ENABLE_QT_CLIENT)
    # Look for sibling XinYu-IM-QtClient directory and add it
    if(EXISTS ${PROJECT_SOURCE_DIR}/../XinYu-IM-QtClient/CMakeLists.txt)
        message(STATUS "ENABLE_QT_CLIENT=ON: Adding XinYu-IM-QtClient subdirectory")
        add_subdirectory(${PROJECT_SOURCE_DIR}/../XinYu-IM-QtClient ${CMAKE_BINARY_DIR}/qtclient)
    else()
        message(WARNING "ENABLE_QT_CLIENT=ON but XinYu-IM-QtClient not found at ${PROJECT_SOURCE_DIR}/../XinYu-IM-QtClient")
    endif()
endif()