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
public:
    VarNodeRepository(sw::redis::Redis& _redis,
        const std::string& _tableName,
        const std::string& _primaryValue,
        const std::string& _uniqueKey,
        const std::string& _uniqueValue
    ) : redis{_redis}, tableName{_tableName}, primaryValue{_primaryValue}, uniqueKey{_uniqueKey}, uniqueValue{_uniqueValue}
    {}

    ID insert(tmp::VarNode&& data) override
    {
        // TODO 実装
        int status;
        tmp::print_debug("insert ... ", abi::__cxa_demangle(typeid(*this).name(),0,0,&status));
        tmp::debug_print_varnode(&data);
        // ダミーデータの返却
        return std::string{"contractor:1"};
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
