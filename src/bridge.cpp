/**
 * Foreign Function & Memory API (Project Panama) の動作確認用サンプル。
 * 
 * @see CMakeLists.txt ... varnode_bridge
 * e.g. cmake コマンド
 * mkdir build && cd build
 * cmake ..
 * make varnode_bridge
 */
#include "Repository.hpp" // 既存のヘッダ
#include "mysql_template.hpp"
#include <iostream>
#include <string>
#include <vector>

constexpr std::string primaryKeyName = "id";

// 名前修飾を防ぎ、外部から見えるように定義
extern "C" {

    // 1. VarNodeの生成 (Java側にはポインタを返す)
    __attribute__((visibility("default")))
    void* create_varnode(const char* key) {
        try {
            // key, data (std::monostate), parent (nullptr) で初期化
            return new tmp::VarNode(std::string(key), std::monostate{}, nullptr);
        } catch (...) {
            return nullptr;
        }
    }

    // 2. 子要素を追加 (String限定版)
    __attribute__((visibility("default")))
    void add_child_str(void* node_ptr, const char* key, const char* value) {
        auto* node = static_cast<tmp::VarNode*>(node_ptr);
        if (node) {
            // C++側の addChild を呼び出し、ValueTypeとしてstd::stringを渡す
            node->addChild(std::string(key), std::string(value));
        }
    }

    // 2.a 子要素を追加 (int64_t)
    __attribute__((visibility("default")))
    void add_child_int64_t(void* node_ptr, const char* key, const int64_t value) {
        auto* node = static_cast<tmp::VarNode*>(node_ptr);
        if (node) {
            node->addChild(std::string(key), value);
        }
    }

    // 2.b 子要素を追加 (bool)
    __attribute__((visibility("default")))
    void add_child_bool(void* node_ptr, const char* key, const bool value) {
        auto* node = static_cast<tmp::VarNode*>(node_ptr);
        if (node) {
            node->addChild(std::string(key), value);
        }
    }

    // 2.c 子要素を追加 (float)
    __attribute__((visibility("default")))
    void add_child_float(void* node_ptr, const char* key, const float value) {
        auto* node = static_cast<tmp::VarNode*>(node_ptr);
        if (node) {
            node->addChild(std::string(key), value);
        }
    }

    // 2.d 子要素を追加 (double)
    __attribute__((visibility("default")))
    void add_child_double(void* node_ptr, const char* key, const double value) {
        auto* node = static_cast<tmp::VarNode*>(node_ptr);
        if (node) {
            node->addChild(std::string(key), value);
        }
    }

    // 2.e 子要素を追加 (binary ... std::vector<uint8_t>)
    __attribute__((visibility("default")))
    void add_child_binary(void* node_ptr, const char* key, const uint8_t* data, const size_t data_len)
    {
        auto* node = static_cast<tmp::VarNode*>(node_ptr);
        if (node && data) {
            // ポインタとサイズを指定してベクタを初期化
            std::vector<uint8_t> buffer(data, data + data_len); 
            node->addChild(std::string(key), buffer);
        }
    }

    // 3. デバッグ表示の実行
    __attribute__((visibility("default")))
    void debug_varnode(void* node_ptr) {
        auto* node = static_cast<tmp::VarNode*>(node_ptr);
        if (node) {
            // 共有されたグローバルなインライン関数を呼び出し
            debug_print_varnode(node, std::cout, 0);
        }
    }

    // 4. RDBMS(MySQL X Dev API) のコネクション取得・生成
    __attribute__((visibility("default")))
    void* create_session_mysql(const char* host, const int32_t port, const char* user, const char* pass)
    {
        try {
            return new mysqlx::Session(host, port, user, pass);
        } catch(...) {
            return nullptr;
        }
    }

    // 5. Insert 処理 RDBMS(MySQL X Dev API)
    __attribute__((visibility("default")))
    int64_t insert_varnode_mysql(void* session_ptr, void* node_ptr, const char* dbName, const char* tableName)
    {
        using Derived = tmp::mysql::r3::VarNodeRepository<int64_t>;
        try {
            auto* sess = static_cast<mysqlx::Session*>(session_ptr);
            auto* node = static_cast<tmp::VarNode*>(node_ptr);
            if(sess && node) {
                Derived repo{sess, dbName, tableName, primaryKeyName};
                sess->startTransaction();
                int64_t id = repo.insert(std::move(*node));
                sess->commit();
                return id;
            }
            return 0L;
        } catch(std::exception& e) {
            tmp::ptr_print_error<decltype(e)&>(e);
            return 0L;
        } catch(...) {
            std::cerr << "Unknown Exception Occurred." << std::endl;
            return 0L;
        }
    }

    // 6. Update 処理 RDBMS(MySQL X Dev API)
    __attribute__((visibility("default")))
    void update_varnode_mysql(void* session_ptr, void* node_ptr, const int64_t primaryKey, const char* dbName, const char* tableName)
    {
        using Derived = tmp::mysql::r3::VarNodeRepository<int64_t>;
        try {
            auto* sess = static_cast<mysqlx::Session*>(session_ptr);
            auto* node = static_cast<tmp::VarNode*>(node_ptr);
            if(sess && node) {
                Derived repo{sess, dbName, tableName, primaryKeyName};
                sess->startTransaction();
                repo.update(primaryKey, std::move(*node));
                sess->commit();
            } else {
                throw std::runtime_error("session_ptr or node_ptr is null.");
            }
        } catch(std::exception& e) {
            tmp::ptr_print_error<decltype(e)&>(e);
        } catch(...) {
            std::cerr << "Unknown Exception Occurred." << std::endl;
        }
    }

    // 7. Find 処理 RDBMS(MySQL X Dev API)
    __attribute__((visibility("default")))
    char* find_varnode_mysql(void* session_ptr, const int64_t primaryKey, const char* dbName, const char* tableName)
    {
        using Derived = tmp::mysql::r3::VarNodeRepository<int64_t>;
        try {
            auto* sess = static_cast<mysqlx::Session*>(session_ptr);
            if(sess) {
                Derived repo{sess, dbName, tableName, primaryKeyName};
                sess->startTransaction();
                std::optional<tmp::VarNode> data = repo.findById(primaryKey);
                sess->commit();
                if(data) {
                    nlohmann::json j;
                    tmp::to_json(j, data.value());
                    std::string* result = new std::string(j.dump());
                    // return result;
                    return const_cast<char*>(result->c_str());
                    // return const_cast<char*>(j.dump().c_str());
                    // return j.dump().c_str();
                }
                return nullptr;
            } else {
                throw std::runtime_error("session_ptr is null.");
            }
        } catch(std::exception& e) {
            tmp::ptr_print_error<decltype(e)&>(e);
        } catch(...) {
            std::cerr << "Unknown Exception Occurred." << std::endl;
        }
        return nullptr;
    }

    // 7.a サイズ計算
    __attribute__((visibility("default")))
    size_t find_result_size(char* str_ptr)
    {
        try {
            if(str_ptr) {
                std::string tmp = std::string(str_ptr);
                return tmp.size();
            }
        } catch(std::exception& e) {
            tmp::ptr_print_error<decltype(e)&>(e);
        } catch(...) {
            std::cerr << "Unknown Exception Occurred." << std::endl;
        }
        return 0UL;
    }

    // 8. Remove 処理 RDBMS(MySQL X Dev API)
    __attribute__((visibility("default")))
    void remove_varnode_mysql(void* session_ptr, const int64_t primaryKey, const char* dbName, const char* tableName)
    {
        using Derived = tmp::mysql::r3::VarNodeRepository<int64_t>;
        try {
            auto* sess = static_cast<mysqlx::Session*>(session_ptr);
            if(sess) {
                Derived repo{sess, dbName, tableName, primaryKeyName};
                sess->startTransaction();
                repo.remove(primaryKey);
                sess->commit();
            } else {
                throw std::runtime_error("session_ptr is null.");
            }
        } catch(std::exception& e) {
            tmp::ptr_print_error<decltype(e)&>(e);
        } catch(...) {
            std::cerr << "Unknown Exception Occurred." << std::endl;
        }
    }

    // 9. メモリ解放
    __attribute__((visibility("default")))
    void destroy_varnode(void* node_ptr) {
        if (node_ptr) {
            delete static_cast<tmp::VarNode*>(node_ptr);
        }
    }

    // 9.a メモリ解放 RDBMS(MySQL X Dev API) のコネクション
    __attribute__((visibility("default")))
    void destroy_session_mysql(void* session)
    {
        if(session) {
            delete static_cast<mysqlx::Session*>(session);
        }
    }

    // 9.a メモリ解放 char* ... @see find_varnode_xxx()
    __attribute__((visibility("default")))
    void destroy_string(char* str_ptr)
    {
        if(str_ptr) {
            delete static_cast<char*>(str_ptr);
        }
    }
}
