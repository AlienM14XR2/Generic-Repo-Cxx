#include <Repository.hpp>

// 不完全型: std::unique_ptr を用いた再帰定義は、VarNode が
// 不完全型の状態でインスタンス化される可能性があるため、
// デストラクタを実装ファイル（.cpp）に明示的に定義することが
// 推奨されます（インラインデストラクタではコンパイルエラーの原因となります）。

namespace tmp {

VarNode::~VarNode() = default;

// ムーブコンストラクタの実装
// VarNode::VarNode(VarNode&&) noexcept = default;
// 1. ムーブコンストラクタ
VarNode::VarNode(VarNode&& other) noexcept
    : key(std::move(other.key)),
      data(std::move(other.data)),
      parent(other.parent),               // 親ポインタを引き継ぐ
      children(std::move(other.children))  // 子ノードの所有権を移動
{
    // 移動してきたすべての子供たちの親ポインタを、自分の新しいアドレス（this）に更新
    for (auto& child : children) {
        if (child) {
            child->parent = this;
        }
    }
    // ムーブ元のオブジェクトのポインタを安全な状態にする
    other.parent = nullptr;
}

// ムーブ代入演算子の実装
// VarNode& VarNode::operator=(VarNode&&) noexcept = default;
// 2. ムーブ代入演算子
VarNode& VarNode::operator=(VarNode&& other) noexcept
{
    if (this != &other) {
        // 既存のコンテンツ（childrenを含む）は、この代入によって自動的に解体される
        key = std::move(other.key);
        data = std::move(other.data);
        parent = other.parent;
        children = std::move(other.children);

        // 移動してきたすべての子供たちの親ポインタを、自分の新しいアドレス（this）に更新
        for (auto& child : children) {
            if (child) {
                child->parent = this;
            }
        }

        // ムーブ元のオブジェクトのポインタを安全な状態にする
        other.parent = nullptr;
    }
    return *this;
}

} // namespace tmp