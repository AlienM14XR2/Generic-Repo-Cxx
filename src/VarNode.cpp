#include <Repository.hpp>

// 不完全型: std::unique_ptr を用いた再帰定義は、VarNode が
// 不完全型の状態でインスタンス化される可能性があるため、
// デストラクタを実装ファイル（.cpp）に明示的に定義することが
// 推奨されます（インラインデストラクタではコンパイルエラーの原因となります）。

tmp::VarNode::~VarNode() = default;

// ムーブコンストラクタの実装
tmp::VarNode::VarNode(tmp::VarNode&&) noexcept = default;

// ムーブ代入演算子の実装
tmp::VarNode& tmp::VarNode::operator=(tmp::VarNode&&) noexcept = default;
