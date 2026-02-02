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
template <class ID>
class VarNodeRepository final : public tmp::Repository<ID, tmp::VarNode>
{
private:
    sw::redis::Redis& redis;
    std::string tableName;
    std::string primaryValue;
    std::string uniqueKey;
    std::string uniqueValue;
    const int64_t timeout;
public:
    VarNodeRepository(sw::redis::Redis& _redis,
        const std::string& _tableName,
        const std::string& _primaryValue,
        const std::string& _uniqueKey,
        const std::string& _uniqueValue,
        const int64_t _timeout = 3600   // 3600秒（1時間）の有効期限
    ) : redis{_redis}, tableName{_tableName}, primaryValue{_primaryValue}, uniqueKey{_uniqueKey}, uniqueValue{_uniqueValue}, timeout{_timeout}
    {}

    ID insert(tmp::VarNode&& data) override
    {
        // TODO 実装
        int status;
        tmp::print_debug("insert ... ", abi::__cxa_demangle(typeid(*this).name(),0,0,&status));
        // Luaスクリプトの定義
        // KEYS[1]: contractor_key, KEYS[2]: index_key
        // ARGV[1]: email, ARGV[2]: id, ARGV[3...]: data (field, value, ...)
        const std::string script = R"(
            if redis.call('HEXISTS', KEYS[2], ARGV[1]) == 1 then
                return 0
            end
            redis.call('HSET', KEYS[1], unpack(ARGV, 3))
            redis.call('HSET', KEYS[2], ARGV[1], ARGV[2])
            return 1
        )";
        const std::string primary_key = std::format("{}:{}", tableName, primaryValue);
        const std::string unique_key  = std::format("{}:{}", tableName, uniqueKey);
        // ARGVの構築（uvalue, pvalue, その後に本体データ）
        std::vector<std::string> args = {uniqueValue, primaryValue};
        std::unordered_map<std::string, std::string> map = {};
        for(const auto& child: data.children) {
            // tmp::print_debug(child->get<std::string>());
            map.insert({child->key, child->get<std::string>()});
        }
        for (const auto& [f, v] : map) {
            args.push_back(f);
            args.push_back(v);
        }
        // スクリプトの実行
        // eval(script, keys_iterator, args_iterator)
        // 第2引数の initializer_list を明示的に記述し、
        // 第3, 4引数に args のイテレータを渡す
        std::vector<std::string> keys = {primary_key, unique_key};
        auto result = redis.eval<long long>(
            script,
            keys.begin(),  // Key の開始
            keys.end(),    // Key の終了
            args.begin(),  // Arg の開始
            args.end()     // Arg の終了
        );
        if (result == 1) {
            std::cout << "Successfully inserted." << std::endl;
            redis.expire(unique_key, std::chrono::seconds(timeout));
            redis.expire(primary_key, std::chrono::seconds(timeout));
        } else {
            std::cout << "Duplicate unique value found. Insertion aborted." << std::endl;
        }
        return primary_key;
    }
    std::optional<tmp::VarNode> findById(const ID& id) const override
    {
        return std::nullopt;
    }
    void update(const ID& id, tmp::VarNode&& data) override
    {
        // TODO 実装
    }
    void remove(const ID& id) override
    {
        // TODO 実装
    }
};  // VarNodeRepository

}   // namespace tmp::redis::r1

#endif  // REDIS_TEMPLATE_H_
