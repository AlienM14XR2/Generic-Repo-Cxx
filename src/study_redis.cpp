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

sw::redis::Redis redis_factory(const int&);
// 本テスト環境で利用すべき唯一のRedis オブジェクトと考えてください（意味合いとしては Singleton）。
sw::redis::Redis redis_ = redis_factory(1);  // ここで DB 1番を選択

sw::redis::Redis redis_factory(const int& _db)
{
    try {
        sw::redis::ConnectionOptions opts;
        opts.host = "127.0.0.1";
        opts.port = 6379;
        opts.password = "pass";
        opts.db = _db;
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

int test_insert_lua(const uint64_t& id, const std::string& email, const int64_t& timeout=3600/** 3600秒（1時間）の有効期限 */) {
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
            redis_.expire(contractor_key, std::chrono::seconds(timeout));
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

int test_update_lua(const uint64_t& id, const std::string& email) {
    puts("------ test_update_lua");
    try {
        const std::string update_script = R"(
            local current_id = redis.call('HGET', KEYS[2], ARGV[1])
            if current_id and current_id ~= ARGV[2] then
                return 0
            end
            -- 本体更新 (unpackはARGVの3番目から)
            redis.call('HSET', KEYS[1], unpack(ARGV, 3))
            -- インデックス更新
            redis.call('HSET', KEYS[2], ARGV[1], ARGV[2])
            return 1
        )";
        const std::string contractor_key = std::format("contractor:{}", id);
        const std::string index_key = "contractor:email_to_id";
        // 引数パッキング
        std::vector<std::string> args = {email, std::to_string(id)};
        std::unordered_map<std::string, std::string> data = {
            {"company_id", "E1_3000_UPDATED"},
            {"email", email},
            {"name", "デレク・改"}
        };
        for (const auto& [f, v] : data) {
            args.push_back(f);
            args.push_back(v);
        }
        std::vector<std::string> keys = {contractor_key, index_key};
        auto result = redis_.eval<long long>(
            update_script,
            keys.begin(),  // Key の開始
            keys.end(),    // Key の終了
            args.begin(),
            args.end()
        );
        if (result == 1) {
            std::cout << "Successfully updated." << std::endl;
        } else {
            std::cout << "Update aborted: Email already in use by another ID." << std::endl;
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        tmp::ptr_print_error<decltype(e)&>(e);
        return EXIT_FAILURE;
    }
}

int test_read(const uint64_t& id)
{
    puts("------ test_read");
    try {
        const std::string contractor_key = std::format("contractor:{}", id);
        // 取得先のコンテナを用意
        std::unordered_map<std::string, std::string> result;
        // hgetall(key, output_iterator)
        // 取得した全フィールドが result に格納される
        redis_.hgetall(contractor_key, std::inserter(result, result.begin()));
        // 内容の確認
        for (const auto& [field, value] : result) {
            std::cout << field << ": " << value << std::endl;
        }
        return EXIT_SUCCESS;
    } catch(std::exception& e) {
        tmp::ptr_print_error<decltype(e)&>(e);
        return EXIT_FAILURE;
    }
}

/**
 * Redis リポジトリに関する考察
 * 
 * 他のRDBMS, NoSQL のストレージとの決定的な違いは、Redis を従属的な役割にしている点である。
 * つまり MySQL + Redis or PostgreSQL + Redis は想定しているが、Redis 単体やRDBMS と主従が
 * 逆転した関係の運用は考えていなということ。そのようなものが必要になった際はRepository の
 * 別サブクラスとして設計し直す（現状はこんなところか）。
 * VarNode はComposite パターンを採用している。
 * 
 * - VarNode におけるRedis 特有のデータの保持構造（CUD の更新系で必要）を決定する。
 * - VarNode を引数、戻値とした簡易テスト関数の実装。
 */

/**
 * テストデータの作成
 * 
 * table: テーブル名
 * ukey: redis 用のユニークキ名に使用するワード
 * id: RDBMS 等で発行されたID
 * email: RDBMS 等で管理されるユニークな値
 * name: ID で表現されるデータの別名
 */
tmp::VarNode createTestData(const std::string& table, const std::string& ukey, const uint64_t& id, const std::string& email, const std::string& name)
{
    tmp::VarNode root("__root__", std::monostate{});    // Root ノードのKey とValue はなんでもよい（RDBMS NoSQL でも同じ）。
    root.addChild("pkey", std::format("{}:{}", table, id));
    root.addChild("pvalue", std::format("{}", id));
    root.addChild("ukey", std::format("{}:{}", table, ukey));
    root.addChild("uvalue", std::format("{}", email));
    tmp::VarNode* data = root.addChild("data_redis", std::monostate{});
    // Redis に保存する値は、std::string に限定する。
    data->addChild("id", std::to_string(id));
    data->addChild("email", email);
    data->addChild("name", name);
    return root;
}

int test_redis_VarNode()
{
    puts("------ test_redis_VarNode ... Redis 特有のデータの持ち方");
    const std::string TABLE = "contractor";
    const std::string UKEY  = "email_to_id";
    const uint64_t    ID    = 1;
    const std::string EMAIL = "derek@example.com";
    const std::string NAME  = "デレク";
    try {
        tmp::VarNode root = createTestData(TABLE, UKEY, ID, EMAIL, NAME);
        // 出力して目視確認：）
        tmp::debug_print_varnode(&root);
        // 真面目に比較検証 ... redis 特有の構造
        tmp::VarNode* pkey = root.getChild("pkey");
        std::string redis_pkey = pkey->get<std::string>();
        assert(redis_pkey.compare(std::format("{}:{}", TABLE, ID)) == 0);
        tmp::VarNode* pvalue = root.getChild("pvalue");
        std::string primary = pvalue->get<std::string>();
        assert(primary.compare(std::to_string(ID)) == 0);
        tmp::VarNode* ukey = root.getChild("ukey");
        std::string unique = ukey->get<std::string>();
        assert(unique.compare(std::format("{}:{}", TABLE, UKEY)) == 0);
        tmp::VarNode* uvalue = root.getChild("uvalue");
        std::string unique_value = uvalue->get<std::string>();
        assert(unique_value.compare(EMAIL) == 0);
        // data_redis の確認
        tmp::VarNode* dataRedis = root.getChild("data_redis");
        for(const auto& child: dataRedis->children ) {
            if(child->key == "id") {
                std::string actualId = child->get<std::string>();
                assert(actualId.compare(std::to_string(ID)) == 0);
            }
            else if(child->key == "email") {
                std::string actualEmail = child->get<std::string>();
                assert(actualEmail.compare(EMAIL) == 0);
            }
            else if(child->key == "name") {
                std::string actualName = child->get<std::string>();
                assert(actualName.compare(NAME) == 0);
            }
        }
        return EXIT_SUCCESS;
    } catch(std::exception& e) {
        tmp::ptr_print_error<decltype(e)>(e);
        return EXIT_FAILURE;
    }
}

int test_insert_lua(tmp::VarNode&& node, const int64_t& timeout=3600/** 3600秒（1時間）の有効期限 */)
{
    puts("------ test_insert_lua ... VarNode");
    try {
        tmp::VarNode* pkey   = node.getChild("pkey");
        tmp::VarNode* pvalue = node.getChild("pvalue");
        tmp::VarNode* ukey   = node.getChild("ukey");
        tmp::VarNode* uvalue = node.getChild("uvalue");
        tmp::VarNode* data_redis = node.getChild("data_redis");
        if(pkey && pvalue && ukey && uvalue && data_redis) {
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
            const std::string primary_key = pkey->get<std::string>();
            const std::string unique_key  = ukey->get<std::string>();
            // ARGVの構築（uvalue, pvalue, その後に本体データ）
            std::vector<std::string> args = {uvalue->get<std::string>(), pvalue->get<std::string>()};
            std::unordered_map<std::string, std::string> data = {};
            for(const auto& child: data_redis->children) {
                tmp::print_debug(child->get<std::string>());
                data.insert({child->key, child->get<std::string>()});
            }
            for (const auto& [f, v] : data) {
                args.push_back(f);
                args.push_back(v);
            }
            // スクリプトの実行
            // eval(script, keys_iterator, args_iterator)
            // 第2引数の initializer_list を明示的に記述し、
            // 第3, 4引数に args のイテレータを渡す
            std::vector<std::string> keys = {primary_key, unique_key};
            auto result = redis_.eval<long long>(
                script,
                keys.begin(),  // Key の開始
                keys.end(),    // Key の終了
                args.begin(),  // Arg の開始
                args.end()     // Arg の終了
            );
            if (result == 1) {
                std::cout << "Successfully inserted." << std::endl;
                redis_.expire(primary_key, std::chrono::seconds(timeout));
            } else {
                std::cout << "Duplicate email found. Insertion aborted." << std::endl;
            }
        } else {
            throw std::runtime_error("NullPointer Exception. The redis data is incomplete.");
        }
        return EXIT_SUCCESS;
    } catch(std::exception& e) {
        tmp::ptr_print_error<decltype(e)>(e);
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
    if(0) {
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
        tmp::print_debug("Play and Result (after insert check) ... ", ret = test_read(id));
        assert(ret == 0);
        tmp::print_debug("Play and Result ... ", ret = test_update_lua(id, email));
        assert(ret == 0);
        tmp::print_debug("Play and Result (after update check) ... ", ret = test_read(id));
        assert(ret == 0);
        // tmp::print_debug("Play and Result ... ", ret = test_remove(id, email));
        tmp::print_debug("Play and Result ... ", ret = test_remove_lua(id, email));
        assert(ret == 0);
        tmp::print_debug("Play and Result (2nd)... ", ret = test_remove_lua(id, email));
        assert(ret == 0);
    }
    if(1) {
        tmp::print_debug("Play and Result ... ", ret = test_redis_VarNode());
        assert(ret == 0);
        const std::string TABLE = "contractor";
        const std::string UKEY  = "email_to_id";
        const uint64_t    ID    = 1;
        const std::string EMAIL = "derek@example.com";
        const std::string NAME  = "デレク";
        tmp::print_debug("Play and Result ... ", ret = test_insert_lua(std::move(createTestData(TABLE,UKEY,ID,EMAIL,NAME))));
        assert(ret == 0);
    }
    puts("=== END   main");
    return EXIT_SUCCESS;
}
