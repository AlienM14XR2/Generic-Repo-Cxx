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
# 1. 本体データ（Hash）の削除
127.0.0.1:6379[1]> DEL contractor:1
# 2. インデックス（Hash内のフィールド）の削除
127.0.0.1:6379[1]> HDEL contractor:email_to_id derek@example.com
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

int test_insert_lua(const uint64_t& id, const std::string& email) {
    puts("------ test_insert_lua");
    try {
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
        const std::string contractor_key = std::format("contractor:{}", id);
        const std::string index_key = "contractor:email_to_id";
        // ARGVの構築（email, id, その後に本体データ）
        std::vector<std::string> args = {email, std::to_string(id)};
        std::unordered_map<std::string, std::string> data = {
            {"company_id", "E1_3000"}, {"email", email}, {"name", "デレク"}
        };
        for (const auto& [f, v] : data) {
            args.push_back(f);
            args.push_back(v);
        }
        // スクリプトの実行
        // eval(script, keys_iterator, args_iterator)
        // 第2引数の initializer_list を明示的に記述し、
        // 第3, 4引数に args のイテレータを渡す
        std::vector<std::string> keys = {contractor_key, index_key};
        auto result = redis_.eval<long long>(
            script, 
            keys.begin(),  // Key の開始
            keys.end(),    // Key の終了
            args.begin(),  // Arg の開始
            args.end()     // Arg の終了
        );
        if (result == 1) {
            std::cout << "Successfully inserted." << std::endl;
        } else {
            std::cout << "Duplicate email found. Insertion aborted." << std::endl;
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        tmp::ptr_print_error<decltype(e)&>(e);
        return EXIT_FAILURE;
    }
}

int test_remove_lua(const uint64_t& id, const std::string& email)
{
    puts("------ test_remove_lua");
    try {
        const std::string del_script = R"(
            -- KEYS[1]: contractor:id
            -- KEYS[2]: contractor:email_to_id
            -- ARGV[1]: email
            redis.call('DEL', KEYS[1])
            return redis.call('HDEL', KEYS[2], ARGV[1])
        )";
        const std::string contractor_key = std::format("contractor:{}", id);
        const std::string index_key = "contractor:email_to_id";
        // 実行例
        // std::vector<std::string> keys = {contractor_key, index_key};
        auto removed = redis_.eval<long long>(
            del_script,
            {contractor_key, index_key},    // KEYS
            {email}                         // ARGV
        );
        if(removed == 1) {
            std::cout << "Successfully removed." << std::endl;
        } else {
            std::cout << "Removing aborted." << std::endl;
        }
        return EXIT_SUCCESS;
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
        // tmp::print_debug("Play and Result ... ", ret = test_insert(id, email));
        tmp::print_debug("Play and Result ... ", ret = test_insert_lua(id, email));
        assert(ret == 0);
        // 2回目：重複登録（Lua側で拒否され、result: 0 が返るはず... exception にはならない）
        uint64_t id2 = 200ul;
        tmp::print_debug("Second attempt (ID:200): ", ret = test_insert_lua(id2, email));
        assert(ret == 0);
        // tmp::print_debug("Play and Result ... ", ret = test_remove(id, email));
        tmp::print_debug("Play and Result ... ", ret = test_remove_lua(id, email));
        assert(ret == 0);
        tmp::print_debug("Play and Result (2nd)... ", ret = test_remove_lua(id, email));
        assert(ret == 0);
    }
    puts("=== END   main");
    return EXIT_SUCCESS;
}
