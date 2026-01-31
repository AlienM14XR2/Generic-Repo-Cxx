/**
 * Redis 習作
 * 
 * 
 * - Redis client written in C++ 
 * https://github.com/sewenew/redis-plus-plus
 * 
 * - redis Docker Official Image
 * https://hub.docker.com/_/redis
 * 
 * 
 * Docker イメージによる環境構築
```
sudo docker pull redis

docker run --name redis -p 6379:6379 \
-v $(pwd)/redis-data:/data \
-d redis redis-server --appendonly yes \
--requirepass "pass"
```
 * 
 * 
 * 動作確認
```
# 起動中のコンテナ内で redis-cli を実行
docker exec -it redis redis-cli

127.0.0.1:6379> AUTH pass
OK

# テスト入力 (cliの中で)
> ping
PONG

SELECT 1
127.0.0.1:6379[1]> MONITOR
127.0.0.1:6379[1]> KEYS *
127.0.0.1:6379[1]> HGETALL contractor:1
127.0.0.1:6379[1]> HGET contractor:email_to_id derek@example.com
```
 * 
 * 
 * Redis CLIで日本語を正しく表示する方法
```
docker exec -it redis redis-cli -a pass --raw
```
 * 
 * C++ クライアントによる接続準備
 * 依存ライブラリ (hiredis) のインストール
```
sudo apt update
sudo apt install libhiredis-dev
```
 * 
 * redis-plus-plus のビルドとインストール
```
# ビルドツールのインストール... 既にインストール済みならカット。
sudo apt install cmake g++ git

# ソースの取得
cd ~/tmp
git clone https://github.com/sewenew/redis-plus-plus.git
cd redis-plus-plus

# ビルド・インストール
mkdir build
cd build
cmake ..
make
sudo make install

# ライブラリキャッシュの更新
sudo ldconfig
```
 * 
 * 
 * e.g. compile
 * ```
 * g++ -O3 -DDEBUG -std=c++20 -pedantic-errors -Wall -Wextra -Werror -I../inc/ -I/usr/local/include -L/usr/local/lib study_redis.cpp VarNode.cpp -lredis++ -lhiredis -pthread -o ../bin/study_redis
 * ```
 */
#include <iostream>
#include <string>
#include <memory>
#include <cassert>
#include <cxxabi.h>
#include <format>
#include <sw/redis++/redis++.h>
#include <Repository.hpp>

sw::redis::Redis redis_factory();
// 本テスト環境で利用すべき唯一のRedis オブジェクトと考えてください（意味合いとしては Singleton）。
sw::redis::Redis redis_ = redis_factory();

sw::redis::Redis redis_factory()
{
    try {
        sw::redis::ConnectionOptions opts;
        opts.host = "127.0.0.1";
        opts.port = 6379;
        opts.password = "pass";
        opts.db = 1; // ここで DB 1番を選択
        opts.socket_timeout = std::chrono::milliseconds(200);
        sw::redis::ConnectionPoolOptions pool_opts;
        pool_opts.size = 3;
        pool_opts.wait_timeout = std::chrono::milliseconds(100);
        pool_opts.connection_lifetime = std::chrono::minutes(10);
        // redis オブジェクトの作成
        auto redis = sw::redis::Redis(opts, pool_opts);
        // 疎通確認。
        redis.ping();
        return redis;
    } catch(const sw::redis::Error &e) {
        tmp::ptr_print_error<decltype(e)&>(e);
        throw e;
    }
}

int test_connect()
{
    puts("------ test_connect");
    try {
        sw::redis::ConnectionOptions opts;
        opts.host = "127.0.0.1";
        opts.port = 6379;
        opts.password = "pass";
        opts.db = 1; // ここで DB 1番を選択
        opts.socket_timeout = std::chrono::milliseconds(200);
        sw::redis::ConnectionPoolOptions pool_opts;
        pool_opts.size = 3;
        pool_opts.wait_timeout = std::chrono::milliseconds(100);
        pool_opts.connection_lifetime = std::chrono::minutes(10);
        // redis オブジェクトの作成, Key Value の確認
        auto redis = sw::redis::Redis(opts, pool_opts);
        redis.set("key", "val");
        auto val = redis.get("key");
        if (val) {
            std::cout << *val << std::endl;
        }
        // データの登録
        std::unordered_map<std::string, std::string> data = {
            {"company_id", "E1_3000"},
            {"email", "derek@example.com"},
            {"password", "derek1111"},
            {"name", "デレク"},
            {"roles", "Operator"}
        };
        // hset ... db の upsert
        const std::string uk_contractor = "contractor:1";
        const std::string uk_contractor_email_id = "contractor:email_to_id";
        const std::string email = "derek@example.com";
        redis.hset(uk_contractor, data.begin(), data.end());
        // Unique 制約用のインデックス更新 ... hsetnx は一意性を厳格に守り、新規作成時のみ成功させたい場合に利用できる（複数行は不可）。
        bool is_new_insert = redis.hsetnx(uk_contractor_email_id, email, "1");
        tmp::print_debug("is_new_insert: ", is_new_insert);
        // 取得先のコンテナを用意
        std::unordered_map<std::string, std::string> result;
        // hgetall(key, output_iterator)
        // 取得した全フィールドが result に格納される
        redis.hgetall(uk_contractor, std::inserter(result, result.begin()));
        // 内容の確認
        for (const auto& [field, value] : result) {
            std::cout << field << ": " << value << std::endl;
        }
        // キー（行）を丸ごと削除する
        // 戻り値: 削除されたキーの数 (1: 成功, 0: キーが存在しない)
        long long removed_count = redis.del(uk_contractor);
        if (removed_count > 0) {
            tmp::print_debug("Successfully deleted. ", uk_contractor);
        } else {
            tmp::print_debug("Key not found. ", uk_contractor);
        }
        long long removed_fields = redis.hdel(uk_contractor_email_id, email);
        if (removed_fields > 0) {
            tmp::print_debug("Successfully deleted. ", uk_contractor_email_id, email);
        } else {
            tmp::print_debug("Not removed Key and Field . ", uk_contractor, email);
        }
        return EXIT_SUCCESS;
    } catch(std::exception& e) {
        tmp::ptr_print_error<decltype(e)&>(e);
        return EXIT_FAILURE;
    }
}

int test_remove(const uint64_t& id, const std::string& email)
{
    puts("------ test_remove");
    try {
        const std::string uk_contractor = "contractor" + std::format(":{}", id);
        // このように記述することで、見えてくるものがあるだろう。RDBMS とは異なるフィールの持ち方等（ユニークキ制約）。
        const std::string uk_contractor_email_id = "contractor" + std::format(":{}", "email_to_id");
        // トランザクションオブジェクトの作成
        auto pipe = redis_.transaction();
        pipe.del(uk_contractor);
        pipe.hdel(uk_contractor_email_id, email);
        auto replies = pipe.exec();
        for(size_t i = 0; i < replies.size(); i++) {
            long long result = replies.get<long long>(i);
            tmp::print_debug("index: ", i, "result: ", result);
        }
        return EXIT_SUCCESS;
    } catch(std::exception& e) {
        tmp::ptr_print_error<decltype(e)&>(e);
        return EXIT_FAILURE;
    }
}

int test_insert(const uint64_t& id, const std::string& email)
{
    puts("------ test_insert");
    try {
        // トランザクションオブジェクトの作成
        auto pipe = redis_.transaction();
        // 1. 本体データの登録 (Hash)
        // std::string email = "derek@example.com";
        std::unordered_map<std::string, std::string> data = {
            {"company_id", "E1_3000"},
            {"email", email},
            {"password", "derek1111"},
            {"name", "デレク"},
            {"roles", "Operator"}
        };
        // key を純粋なテーブル名に一致させた方が都合がよさそうだが。
        std::string contractor_key = "contractor" + std::format(":{}", id);
        std::string contractor_email_id = "contractor:email_to_id";
        pipe.hset(contractor_key, data.begin(), data.end());
        // 2. ユニーク制約用インデックスの登録
        // pipe.hsetnx(contractor_email_id, email, std::format("{}", id));
        pipe.hset(contractor_email_id, email, std::format("{}", id));
        // 3. 実行（EXECコマンドの発行）
        // 実行結果は各コマンドの戻り値が sw::redis::QueuedReply として返る
        auto replies = pipe.exec();
        for(size_t i = 0; i < replies.size(); i++) {
            long long result = replies.get<long long>(i);
            tmp::print_debug("index: ", i, "result: ", result);
        }
        return EXIT_SUCCESS;
    // } catch(const sw::redis::Error &e) {     // sw::redis::Error ... std::exception を継承していた。
    //     tmp::ptr_print_error<decltype(e)&>(e);
    //     return EXIT_FAILURE;
    } catch(std::exception& e) {
        tmp::ptr_print_error<decltype(e)&>(e);
        return EXIT_FAILURE;
    }
}

int main()
{
    puts("START main ===");
    int ret = -1;
    if(0) {
        tmp::print_debug("Play and Result ... ", ret = test_connect());
        assert(ret == 0);
    }
    if(1) {
        uint64_t id = 1ul;
        // RDBMS におけるユニークキ制約カラム
        std::string email = "derek@example.com";
        tmp::print_debug("Play and Result ... ", ret = test_insert(id, email));
        tmp::print_debug("Play and Result ... ", ret = test_remove(id, email));
        assert(ret == 0);
    }
    puts("=== END   main");
    return EXIT_SUCCESS;
}
