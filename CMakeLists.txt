cmake_minimum_required(VERSION 3.15)
project(GenericDatabaseTests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- 依存ライブラリの検索 ---
find_package(GTest REQUIRED)
find_package(PkgConfig REQUIRED)

# PostgreSQL
pkg_check_modules(PQXX REQUIRED libpqxx)
pkg_check_modules(PQ REQUIRED libpq)

# MongoDB
pkg_check_modules(MONGOCXX REQUIRED libmongocxx)

# base64pp (追加)
pkg_check_modules(BASE64PP REQUIRED base64pp)

# Redis++ (追加)
# hiredis と redis++ の両方が必要
pkg_check_modules(REDISPP REQUIRED redis++)
pkg_check_modules(HIREDIS REQUIRED hiredis)

# --- 共通のコンパイルオプション定義 ---
set(COMMON_COMPILE_OPTIONS 
    -Wall 
    -Wextra 
    -Werror 
    -Wno-unused-parameter
    $<$<CONFIG:Release>:-O3 -pedantic-errors>
)

set(COMMON_COMPILE_DEFINITIONS
    $<$<CONFIG:Debug>:DEBUG>
)

# --- ターゲット（実行ファイル）の定義 ---

# 1. PostgreSQL テストランナー
add_executable(pqxx_template_test_runner src/pqxx_template_test.cpp src/VarNode.cpp)
target_include_directories(pqxx_template_test_runner PRIVATE 
    inc 
    ${PQXX_INCLUDE_DIRS}
    ${PQ_INCLUDE_DIRS}
    ${BASE64PP_INCLUDE_DIRS} # 追加
    ${GTEST_INCLUDE_DIRS}
)
target_link_libraries(pqxx_template_test_runner PRIVATE 
    ${PQXX_LIBRARIES} 
    ${PQ_LIBRARIES} 
    ${BASE64PP_LIBRARIES}    # 追加
    GTest::gtest_main 
)
target_compile_options(pqxx_template_test_runner PRIVATE ${COMMON_COMPILE_OPTIONS})
target_compile_definitions(pqxx_template_test_runner PRIVATE ${COMMON_COMPILE_DEFINITIONS})


# 2. MySQL テストランナー
add_executable(mysql_template_test_runner src/mysql_template_test.cpp src/VarNode.cpp)
target_include_directories(mysql_template_test_runner PRIVATE 
    inc
    /usr/include/mysql-cppconn/ 
    /usr/local/include          
    ${BASE64PP_INCLUDE_DIRS} # 追加
    ${GTEST_INCLUDE_DIRS}
)
target_link_libraries(mysql_template_test_runner PRIVATE 
    mysqlcppconn                
    mysqlcppconnx               
    ${BASE64PP_LIBRARIES}    # 追加 (-lbase64pp)
    GTest::gtest_main
)
target_compile_options(mysql_template_test_runner PRIVATE ${COMMON_COMPILE_OPTIONS})
target_compile_definitions(mysql_template_test_runner PRIVATE ${COMMON_COMPILE_DEFINITIONS})


# 3. MongoDB テストランナー
add_executable(mongo_template_test_runner src/mongo_template_test.cpp src/VarNode.cpp)
target_include_directories(mongo_template_test_runner PRIVATE 
    inc
    /usr/local/include/bson-2.1.2/ 
    ${MONGOCXX_INCLUDE_DIRS}
    ${BASE64PP_INCLUDE_DIRS} # 追加
    ${GTEST_INCLUDE_DIRS}
)
target_link_libraries(mongo_template_test_runner PRIVATE 
    ${MONGOCXX_LIBRARIES}
    bson2                          
    ${BASE64PP_LIBRARIES}    # 追加
    GTest::gtest_main
)
target_compile_options(mongo_template_test_runner PRIVATE ${COMMON_COMPILE_OPTIONS})
target_compile_definitions(mongo_template_test_runner PRIVATE ${COMMON_COMPILE_DEFINITIONS})


# 4. ObjectPool 単体テストランナー
add_executable(object_pool_test_runner src/object_pool_test.cpp)
target_include_directories(object_pool_test_runner PRIVATE 
    inc 
    ${GTEST_INCLUDE_DIRS}
)
target_link_libraries(object_pool_test_runner PRIVATE 
    GTest::gtest_main 
)
target_compile_options(object_pool_test_runner PRIVATE ${COMMON_COMPILE_OPTIONS})
target_compile_definitions(object_pool_test_runner PRIVATE ${COMMON_COMPILE_DEFINITIONS})


# 5. VarNode 単体テストランナー
add_executable(varnode_test_runner src/varnode_test.cpp src/VarNode.cpp)
target_include_directories(varnode_test_runner PRIVATE 
    inc 
    ${BASE64PP_INCLUDE_DIRS} # 追加
    ${GTEST_INCLUDE_DIRS}
)
target_link_libraries(varnode_test_runner PRIVATE 
    ${BASE64PP_LIBRARIES}    # 追加
    GTest::gtest_main 
)
target_compile_options(varnode_test_runner PRIVATE ${COMMON_COMPILE_OPTIONS})
target_compile_definitions(varnode_test_runner PRIVATE ${COMMON_COMPILE_DEFINITIONS})


# 6. Redis++ テストランナー
add_executable(redis_template_test_runner src/redis_template_test.cpp src/VarNode.cpp)
target_include_directories(redis_template_test_runner PRIVATE 
    inc 
    ${REDISPP_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIRS}
    ${BASE64PP_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS}
)
target_link_libraries(redis_template_test_runner PRIVATE 
    ${REDISPP_LIBRARIES}
    ${HIREDIS_LIBRARIES}
    ${BASE64PP_LIBRARIES}
    pthread             # redis++ は通常 pthread を必要とします
    GTest::gtest_main 
)
target_compile_options(redis_template_test_runner PRIVATE ${COMMON_COMPILE_OPTIONS})
target_compile_definitions(redis_template_test_runner PRIVATE ${COMMON_COMPILE_DEFINITIONS})

# --- 共有ライブラリの定義 ---
add_library(varnode_bridge SHARED 
    src/VarNode.cpp 
    src/bridge.cpp
)

target_link_libraries(varnode_bridge PRIVATE 
    ${PQXX_LIBRARIES} 
    ${PQ_LIBRARIES} 
    mysqlcppconn 
    mysqlcppconnx
    ${MONGOCXX_LIBRARIES}
    bson2
    ${BASE64PP_LIBRARIES}
    ${REDISPP_LIBRARIES}    # 追加
    ${HIREDIS_LIBRARIES}    # 追加
)

target_include_directories(varnode_bridge PRIVATE 
    inc
    /usr/include/mysql-cppconn/
    /usr/local/include
    ${PQXX_INCLUDE_DIRS}
    ${PQ_INCLUDE_DIRS}
    ${MONGOCXX_INCLUDE_DIRS}
    ${BASE64PP_INCLUDE_DIRS}
    ${REDISPP_INCLUDE_DIRS}  # 追加
    ${HIREDIS_INCLUDE_DIRS}  # 追加
)

target_compile_options(varnode_bridge PRIVATE ${COMMON_COMPILE_OPTIONS})
target_compile_definitions(varnode_bridge PRIVATE ${COMMON_COMPILE_DEFINITIONS})

set_target_properties(varnode_bridge PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
