/**
 * redis_template.hpp
 * 
 * 環境構築手順等は、src/study_redis.cpp を参照すること。
 * Repository のRedis 用サブクラスの宣言・定義を行う。
 */
#ifndef REDIS_TEMPLATE_H_
#define REDIS_TEMPLATE_H_

#include <iostream>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <cxxabi.h>
#include <format>
#include <sw/redis++/redis++.h>

#include <Repository.hpp>

namespace tmp::redis::r1
{

/**
 * Redis VarNodeRepository r1
 * 
 * 他のRDBMS, NoSQL のストレージとの決定的な違いは、Redis を従属的な役割にしている点である。
 * つまり MySQL + Redis or PostgreSQL + Redis は想定しているが、Redis 単体やRDBMS と主従が
 * 逆転した関係の運用は考えていなということ。そのようなものが必要になった際はRepository の
 * 別サブクラスとして設計し直す。
 */
template <class ID = std::string>
class VarNodeRepository final : public tmp::Repository<ID, tmp::VarNode>
{
private:
    std::shared_ptr<sw::redis::Redis> redis;
    std::string tableName;
    std::string primaryValue;
    std::string uniqueKey;
    std::string uniqueValue;
    int64_t timeout;
public:
    // For insert, update, remove
    VarNodeRepository(std::shared_ptr<sw::redis::Redis> _redis,
        const std::string& _tableName,
        const std::string& _primaryValue,
        const std::string& _uniqueKey,
        const std::string& _uniqueValue,
        const int64_t _timeout = 3600   // 3600秒（1時間）の有効期限
    ) : redis{std::move(_redis)}, tableName{_tableName}, primaryValue{_primaryValue}, uniqueKey{_uniqueKey}, uniqueValue{_uniqueValue}, timeout{_timeout}
    {
        if (!redis) {
            throw std::invalid_argument("Redis connection pointer is null.");
        }
    }
    // For find
    VarNodeRepository(std::shared_ptr<sw::redis::Redis> _redis,
        const int64_t& _timeout = 3600
    ) : redis{std::move(_redis)}, timeout{_timeout}
    {
        if (!redis) {
            throw std::invalid_argument("Redis connection pointer is null.");
        }
    }
    // デストラクタ
    ~VarNodeRepository() override = default;

    ID insert(tmp::VarNode&& data) override
    {
    #ifdef DEBUG
        int status;
        char* demangled = abi::__cxa_demangle(typeid(*this).name(), 0, 0, &status);
        tmp::print_debug("insert ... ", demangled ? demangled : typeid(*this).name());
        free(demangled); // abi::__cxa_demangle は malloc されたメモリを返すため free が必須
    #endif
        // 1. Luaスクリプト：EXPIRE設定を内部に統合
        // KEYS[1]: primary_key, KEYS[2]: unique_key
        // ARGV[1]: uniqueValue, ARGV[2]: primaryValue, ARGV[3]: timeout_seconds, ARGV[4...]: field/value pairs
        const std::string script = R"(
            if redis.call('HEXISTS', KEYS[2], ARGV[1]) == 1 then
                return 0
            end
            -- 本体データの保存
            redis.call('HSET', KEYS[1], unpack(ARGV, 4))
            -- ユニークインデックスの保存
            redis.call('HSET', KEYS[2], ARGV[1], ARGV[2])
            -- 両方のキーに有効期限を設定
            redis.call('EXPIRE', KEYS[1], ARGV[3])
            redis.call('EXPIRE', KEYS[2], ARGV[3])
            return 1
        )";

        const std::string primary_key = std::format("{}:{}", tableName, primaryValue);
        const std::string unique_key  = std::format("{}:{}", tableName, uniqueKey);
        // 2. 引数の構築（中間Mapを介さず直接構築して効率化）
        std::vector<std::string> args;
        args.reserve(4 + data.children.size() * 2); // メモリ再確保を抑制
        args.push_back(uniqueValue);
        args.push_back(primaryValue);
        args.push_back(std::to_string(timeout)); // 有効期限を引数に追加
        for(const auto& child : data.children) {
            args.push_back(child->key);
            args.push_back(child->get<std::string>());
        }
        // 3. スクリプトの実行
        std::vector<std::string> keys = {primary_key, unique_key};
        auto result = redis->eval<long long>(
            script,
            keys.begin(), keys.end(),
            args.begin(), args.end()
        );
        if (result == 1) {
            std::cout << "Successfully inserted with expiration." << std::endl;
        } else {
            std::cout << "Duplicate unique value found. Insertion aborted." << std::endl;
        }
        return primary_key;
    }

    std::optional<tmp::VarNode> findById(const ID& id) const override
    {
    #ifdef DEBUG
        // 改善点1: ヘルパー関数化（推奨）または安全な free を伴うデマングル
        int status;
        char* demangled = abi::__cxa_demangle(typeid(*this).name(), nullptr, nullptr, &status);
        tmp::print_debug("findById ... ", demangled ? demangled : typeid(*this).name());
        if (demangled) free(demangled); 
    #endif

        // 改善点2: std::pair の vector を使用し、ハッシュ計算（map）を完全に回避
        // フィールド数が予測できる場合は reserve することで再確保コストも抑えられます
        std::vector<std::pair<std::string, std::string>> result;
        // Redis++ の hgetall は vector<pair<...>> を直接受け取れます
        redis->hgetall(id, std::back_inserter(result));
        if (!result.empty()) {
            // 改善点3: 有効期限の延長（Touch）
            // 読み取り成功時にキャッシュ寿命をリセットし、RDBMSへのフォールバックを防ぐ
            redis->expire(id, std::chrono::seconds(timeout));
            tmp::VarNode node("result_row_redis", std::monostate{});
            // 改善点4: map を介さない直接的な addChild
            // vector の要素をそのまま利用するため、一時オブジェクトの生成が最小限になります
            for (const auto& kv : result) {
                node.addChild(kv.first, kv.second);
            }
            return node;
        }
        return std::nullopt;
    }

    void update(const ID& id, tmp::VarNode&& data) override
    {
    #ifdef DEBUG
        int status;
        char* demangled = abi::__cxa_demangle(typeid(*this).name(), 0, 0, &status);
        tmp::print_debug("update ... ", demangled ? demangled : typeid(*this).name());
        free(demangled); 
    #endif
        const std::string primary_key = std::format("{}:{}", tableName, primaryValue);
        const std::string unique_key  = std::format("{}:{}", tableName, uniqueKey);
        // 管理対象外のIDに対するガード
        if (id != primary_key) {
            std::cerr << "Update skipped: ID mismatch with repository setting." << std::endl;
            return;
        }
        const std::string update_script = R"(
            local current_id = redis.call('HGET', KEYS[2], ARGV[1])
            if current_id and current_id ~= ARGV[2] then
                return 0
            end
            -- unpackの開始位置を修正 (ARGV[4]からが本体データ)
            redis.call('HSET', KEYS[1], unpack(ARGV, 4))
            -- ユニークインデックスの更新
            redis.call('HSET', KEYS[2], ARGV[1], ARGV[2])
            -- 有効期限の再設定
            redis.call('EXPIRE', KEYS[1], ARGV[3])
            redis.call('EXPIRE', KEYS[2], ARGV[3])
            return 1
        )";
        // 引数構築 (uniqueValue, primaryValue, timeout, data...)
        std::vector<std::string> args;
        args.reserve(4 + data.children.size() * 2);
        args.push_back(uniqueValue);
        args.push_back(primaryValue);
        args.push_back(std::to_string(timeout));
        // 重複除去が必要な場合は map を介す
        std::unordered_map<std::string, std::string> map;
        for(const auto& child : data.children) {
            map.insert({child->key, child->get<std::string>()});
        }
        for (const auto& [f, v] : map) {
            args.push_back(f);
            args.push_back(v);
        }
        std::vector<std::string> keys = {primary_key, unique_key};
        auto result = redis->eval<long long>(
            update_script,
            keys.begin(), keys.end(),
            args.begin(), args.end()
        );
        if (result == 1) {
            std::cout << "Successfully updated." << std::endl;
        } else {
            std::cout << "Update aborted: Unique constraint violation." << std::endl;
        }
    }
    void remove(const ID& id) override
    {
    #ifdef DEBUG
        int status;
        char* demangled = abi::__cxa_demangle(typeid(*this).name(), 0, 0, &status);
        tmp::print_debug("remove ... ", demangled ? demangled : typeid(*this).name());
        free(demangled); // abi::__cxa_demangle は malloc されたメモリを返すため free が必須
    #endif
        const std::string del_script = R"(
            -- 本体とインデックスの両方を削除
            local d1 = redis.call('DEL', KEYS[1])
            local d2 = redis.call('HDEL', KEYS[2], ARGV[1])
            -- 少なくとも一方が削除されれば成功とみなす
            if d1 > 0 or d2 > 0 then return 1 else return 0 end
        )";
        const std::string primary_key = std::format("{}:{}", tableName, primaryValue);
        const std::string unique_key  = std::format("{}:{}", tableName, uniqueKey);
        // 管理対象外のIDに対するガード
        if (id != primary_key) {
            std::cerr << "Remove skipped: ID mismatch with repository setting." << std::endl;
            return;
        }
        auto removed = redis->eval<long long>(
            del_script,
            {primary_key, unique_key},      // KEYS
            {uniqueValue}                   // ARGV
        );
        if(removed == 1) {
            std::cout << "Successfully removed." << std::endl;
        } else {
            std::cout << "Removing aborted." << std::endl;
        }
    }
};  // VarNodeRepository

}   // namespace tmp::redis::r1

#endif  // REDIS_TEMPLATE_H_
