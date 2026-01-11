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
#include <iostream>
#include <string>
#include <vector>

using namespace tmp;

// 名前修飾を防ぎ、外部から見えるように定義
extern "C" {

    // 1. VarNodeの生成 (Java側にはポインタを返す)
    __attribute__((visibility("default")))
    void* create_varnode(const char* key) {
        try {
            // key, data (std::monostate), parent (nullptr) で初期化
            return new VarNode(std::string(key), std::monostate{}, nullptr);
        } catch (...) {
            return nullptr;
        }
    }

    // 2. 子要素を追加 (String限定版)
    __attribute__((visibility("default")))
    void add_child_str(void* node_ptr, const char* key, const char* value) {
        auto* node = static_cast<VarNode*>(node_ptr);
        if (node) {
            // C++側の addChild を呼び出し、ValueTypeとしてstd::stringを渡す
            node->addChild(std::string(key), std::string(value));
        }
    }

    // 3. デバッグ表示の実行
    __attribute__((visibility("default")))
    void debug_varnode(void* node_ptr) {
        auto* node = static_cast<VarNode*>(node_ptr);
        if (node) {
            // 共有されたグローバルなインライン関数を呼び出し
            debug_print_varnode(node, std::cout, 0);
        }
    }

    // 4. メモリ解放
    __attribute__((visibility("default")))
    void destroy_varnode(void* node_ptr) {
        if (node_ptr) {
            delete static_cast<VarNode*>(node_ptr);
        }
    }
}
