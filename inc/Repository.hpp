#ifndef REPOSITORY_H_
#define REPOSITORY_H_

#include <optional>
#include <variant>
#include <iostream>
#include <memory>
#include <vector>
#include <nlohmann/json.hpp>
#include <base64pp.h>

namespace tmp {

template <class M, class D>
inline void (*ptr_print_debug)(M, D) = [](const auto message, const auto debug) -> void
{
#ifdef DEBUG
    std::cout << "DEBUG: " << message << '\t' << debug << std::endl;
#endif
};

template <class Error>
concept ErrReasonable = requires(Error& e) {
    e.what();
};
template <class Error>
requires ErrReasonable<Error>
inline void (*ptr_print_error)(Error) = [](const auto e) -> void {
    std::cerr << "ERROR: " << e.what() << std::endl;
};

// ポインタ安全な出力のためのヘルパー関数オーバーロード
inline void safe_print(const char* p)
{
    if (p == nullptr) {
        std::cout << "(nullptr)";
    } else {
        std::cout << p;
    }
}
// 他の型はそのまま出力する汎用テンプレート
template <typename T>
inline void safe_print(const T& element)
{
    std::cout << element;
}
template <class... Args>
inline void print_debug([[maybe_unused]] Args&&... args)
{
#ifdef DEBUG
    std::cout << "Debug: ";
    auto print_element = [](const auto& element) {
        safe_print(element);
        std::cout << '\t';
    };
    // C++17以降の pack expansion で要素を順に処理
    (print_element(std::forward<Args>(args)), ...);
    std::cout << std::endl;
#endif
}

class Transaction
{
public:
    virtual ~Transaction() = default;
    virtual void begin() = 0;
    virtual void commit() = 0;
    virtual void rollback() = 0;
};

// サービス基底クラス（インターフェース）
class ServiceExecutor {
public:
    virtual ~ServiceExecutor() = default;
    virtual void execute() = 0;
};

template <class ID, class Data>
class Repository {
public:
    virtual ~Repository() = default;
    virtual ID insert(Data&&) = 0;
    virtual void update(const ID&, Data&&) = 0;
    virtual void remove(const ID&) = 0;
    virtual std::optional<Data> findById(const ID&) const = 0;
};

// VarNode が保持できる型のリストを定義する
using ValueType = std::variant<
    std::monostate, // 値がない状態を表す
    int64_t,
    uint64_t,
    float,
    double,
    bool,
    std::string,
    std::vector<uint8_t>  // バイナリデータ保持用に追加
>;
// ValueType (std::variant) が表現する型であれば何でも保持可能な構造体
// ディクトリ構造も表現可能なもの
struct VarNode
{
    std::string key;
    ValueType data;
    VarNode* parent = nullptr;
    std::vector<std::unique_ptr<VarNode>> children;

    // デストラクタのみ「宣言」に留める
    ~VarNode();
    // コピーコンストラクタの削除
    VarNode(const VarNode&) = delete;
    VarNode& operator=(const VarNode&) = delete;
    // ムーブは明示的に default 指定（ただし実装ファイルが必要）
    VarNode(VarNode&&) noexcept;
    VarNode& operator=(VarNode&&) noexcept;

    // コンストラクタを ValueType を受け取るように変更
    VarNode(const std::string& _key, ValueType _data, VarNode* _parent = nullptr)
        : key(_key), data(_data), parent(_parent)
    {}

    // 値の取得は std::get<T> を使う（型が違えば std::bad_variant_access を投げる）
    template <class T>
    T get() const
    {
        return std::get<T>(data);
        // std::get_if を使って安全なポインタ取得を試みる
        // return std::get_if<T>(data);
    }
    template <class T>
    bool is_type() const
    {
        return std::holds_alternative<T>(data);
    }
    VarNode* addChild(const std::string& _key, ValueType _data)
    {
        if (auto* existing = getChild(_key)) {
            existing->data = std::move(_data);
            return existing;
        }
        children.push_back(std::make_unique<VarNode>(_key, _data, this));
        return children.back().get();
    }
    // const 修飾による関数のオーバーロードあり。本メンバ関数はconst 修飾なし。
    VarNode* getChild(const std::string& _key)
    {
        auto it = std::ranges::find_if(children, 
                            [&](const auto& child){
                                return child->key == _key;
                            });
        if (it != children.end()) {
            return it->get();
        }
        return nullptr; // 見つからなかった場合は nullptr を返す
    }
    // const 修飾による関数のオーバーロードあり。本メンバ関数はconst 修飾あり。
    const VarNode* getChild(const std::string& _key) const
    {
        auto it = std::ranges::find_if(children, 
                            [&](const auto& child){
                                return child->key == _key;
                            });
        return (it != children.end()) ? it->get() : nullptr;
    }

};  // VarNode

template <typename T>
inline std::optional<T> get_value_safely_optional(const VarNode* const node)
{
    if (node == nullptr) {
        throw std::runtime_error("VarNode* node is null.");
    }
    if (const T* value_ptr = std::get_if<T>(&node->data)) {
        return *value_ptr; // 値そのものを optional でラップして返す
    } else {
        return std::nullopt; // 値がないことを示す
    }
}

inline void debug_print_varnode(const VarNode* const _node, std::ostream& os = std::cout, int indent = 0)
{
    if (_node == nullptr) {
        os << "node is nil." << std::endl;
        return;
    }
    // インデント表示
    os << std::string(indent * 2, ' ');
    os << "key: " << _node->key << "\tdata: ";

    // std::visit のラムダ式内で、全ての型を明示的に処理する
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            os << "(null)";
        // この分岐は冗長に思うかもしれないが、現状はキープする
        } else if constexpr (std::is_same_v<T, bool>) {
            os << (arg ? "true" : "false");
        // この分岐は冗長に思うかもしれないが、現状はキープする
        } else if constexpr (std::is_same_v<T, int64_t> || 
                            std::is_same_v<T, uint64_t> ||
                            std::is_same_v<T, float> ||
                            std::is_same_v<T, double> ||
                            std::is_same_v<T, std::string>) {
            // std::cout はこれらのプリミティブ型やstd::stringを安全に出力できる
            os << arg;
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            // バイナリ型の出力処理を追加
            os << "[Binary Data: " << arg.size() << " bytes]";
            /* 
            // 詳細を表示する場合の例
            os << " (hex: ";
            for(auto b : arg) os << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
            os << std::dec << ")";
            */
        } else {
            // 将来ValueTypeに新しい型（例えばカスタムオブジェクトやポインタ）が追加された場合
            // コンパイルエラーにならずに、ここで処理を止めるか、汎用的な出力を行う
            os << "(Unknown/Unhandled Type)";
        }
    }, _node->data);
    os << std::endl;

    // 子要素を再帰的に呼び出す
    for (const auto& child : _node->children) {
        debug_print_varnode(child.get(), os, indent + 1);
    }
}

inline void to_json(nlohmann::json& _json, const VarNode& _node) {
	nlohmann::json tmp = nlohmann::json::object(); // 明示的にオブジェクト化
    for (const auto& child : _node.children) {
        if (!child->children.empty()) {
            // 子がさらに孫を持っている場合：再帰的にオブジェクトを作成
            to_json(tmp[child->key], *child);
        } else {
            // std::visit のラムダ式内で、全ての型を明示的に処理する
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    // ignore
                } else if constexpr (std::is_same_v<T, bool>) {
                    tmp[child.get()->key] = (arg ? "true" : "false");
                // この分岐は冗長に思うかもしれないが、現状はキープする
                } else if constexpr (std::is_same_v<T, int64_t> || 
                                    std::is_same_v<T, uint64_t> ||
                                    std::is_same_v<T, float> ||
                                    std::is_same_v<T, double> ||
                                    std::is_same_v<T, std::string>) {
                    tmp[child.get()->key] = arg;
                } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                    // base64pp を使用したエンコード
                    tmp[child.get()->key] = base64pp::encode(arg);
                } else {
                    // ignore
                }
            }, child.get()->data);
        }
    }
    _json = std::move(tmp);
}

// Type Erasure パターンを利用したリポジトリの設計。
class CudConcept
{
public:
    virtual ~CudConcept() = default;
    virtual uint64_t insert() = 0;
    virtual void update() = 0;
    virtual void remove() = 0;
    virtual std::unique_ptr<CudConcept> clone() const = 0;
};


template <class Data, class Model>
concept CudNeeds = requires(Data& d, Model& m) {
    m.insert(d);
    m.update(d);
    m.remove(d);
};
template<class Data, class Model>
requires CudNeeds<Data, Model>
class CudModel final : public CudConcept
{
private:
    Data data;  // ここにデータを持ちたくない。不変にもできない場合は困る。
    Model model;
public:
    CudModel(Data _data, Model _model): data{std::move(_data)}, model{std::move(_model)}
    {}
    virtual uint64_t insert() override
    {
        return model.insert(data);
    }
    virtual void update() override
    {
        model.update(data);
    }
    virtual void remove() override
    {
        model.remove(data);
    }
    std::unique_ptr<CudConcept> clone() const override {
        return std::make_unique<CudModel>(*this);
    }
};

}   // namespace tmp



namespace tmp::r1
{


// Type Erasure としては不完全なのか？
// 使い勝手とこのコンセプトをどのように考えるかによる。
// このコンセプトを独立したインタフェースとしたい場合は
// 必ず、Data を必要とするため不便かもしれないが、結局
// 具象化クラスではData を必要としているし。
// CRUD をひとつのコンセプトとしたい場合は、結局この形に
// 落ち着く。判断が難しい。

template <class Data>
class CrudConcept
{
public:
    virtual ~CrudConcept() = default;
    virtual uint64_t insert() = 0;
    virtual void update() = 0;
    virtual void remove() = 0;
    virtual std::unique_ptr<CrudConcept> clone() const = 0;
    virtual std::optional<Data> findById() const = 0;
};


template <class Data, class Model>
concept CrudNeeds = requires(Data& d, Model& m) {
    m.insert(d);
    m.update(d);
    m.remove(d);
    m.findById(d);
};
template<class Data, class Model>
requires CrudNeeds<Data, Model>
class CrudModel final : public CrudConcept<Data>
{
private:
    Data data;  // ここにデータを持ちたくない。不変にもできない場合は困る。
    Model model;
public:
    CrudModel(Data _data, Model _model): data{std::move(_data)}, model{std::move(_model)}
    {}
    virtual uint64_t insert() override
    {
        return model.insert(data);
    }
    virtual void update() override
    {
        model.update(data);
    }
    virtual void remove() override
    {
        model.remove(data);
    }
    std::unique_ptr<CrudConcept<Data>> clone() const override {
        return std::make_unique<CrudModel>(*this);
    }
    virtual std::optional<Data> findById() const override
    {
        return model.findById(data);
    }
};

}   // namespace tmp::r1
#endif
