#include <gtest/gtest.h>
#include <sw/redis++/redis++.h>
#include <Repository.hpp> // プロジェクトの共通ヘッダ
#include <redis_template.hpp>
#include <optional>
#include <string>

using namespace sw::redis;

// Redis テスト用のフィクスチャクラス
class RedisVarNodeRepositoryTest : public ::testing::Test {
protected:
    // 各テストケースの実行前に呼ばれる
    void SetUp() override {
        try {
            // デフォルトの接続先。環境に合わせて変更してください。
            auto opts = ConnectionOptions();
            opts.host = "127.0.0.1";
            opts.port = 6379;
            opts.socket_timeout = std::chrono::milliseconds(200);
            opts.password = "pass";
            redis = std::make_shared<Redis>(opts);            
            // 接続確認
            redis->ping();
        } catch (const Error &e) {
            GTEST_SKIP() << "Redis server is not available: " << e.what();
        }
    }

    // 各テストケースの実行後に呼ばれる
    void TearDown() override {
        if (redis) {
            // 必要に応じてテストデータのクリーンアップ
            redis->del("test_key");
        }
    }

    std::shared_ptr<Redis> redis;
    const std::string table = "contractor";
    const uint64_t primary = 1;
    const std::string ukey = "email_to_id";
    const std::string uvalue = "derek@example.com";
};

// 基本的な SET / GET のテスト
TEST_F(RedisVarNodeRepositoryTest, BasicSetGet) {
    std::string key = "test_key";
    std::string val = "hello_redis";
    ASSERT_NO_THROW(redis->set(key, val));
    auto result = redis->get(key);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, val);
}

TEST_F(RedisVarNodeRepositoryTest, InsertAndFindIntegrity) {
    using VarNodeRepository = tmp::redis::r1::VarNodeRepository<std::string>;
    VarNodeRepository repo(redis, table, std::to_string(primary), ukey, uvalue);

    // --- 1. 期待値 (Expectation) の定義 ---
    const std::string expect_company_id = "E1_3000";
    const std::string expect_name = "デレク";

    tmp::VarNode root("__root__", std::monostate{});
    root.addChild("id", std::to_string(primary));
    root.addChild("company_id", expect_company_id);
    root.addChild("email", uvalue);
    root.addChild("name", expect_name);

    // --- 2. 実行 (Action) ---
    std::string generated_id = repo.insert(std::move(root));
    VarNodeRepository find_repo{redis};
    std::optional<tmp::VarNode> result = find_repo.findById(generated_id);

    // --- 3. 検証 (Verification) ---
    ASSERT_TRUE(result.has_value()) << "findById failed to return data for ID: " << generated_id;
    // Actual データの取得と突合
    // ※ VarNode の子要素取得 API (getChild 等) を想定しています
    auto actual_company_id = result->getChild("company_id");
    auto actual_name       = result->getChild("name");
    ASSERT_NE(actual_company_id, nullptr) << "company_id node is missing";
    ASSERT_NE(actual_name, nullptr)       << "name node is missing";

    // 値の比較
    EXPECT_EQ(actual_company_id->get<std::string>(), expect_company_id);
    EXPECT_EQ(actual_name->get<std::string>(),       expect_name);
    EXPECT_EQ(result->getChild("email")->get<std::string>(), uvalue);
    // --- 4. 後片付け (Teardown) ---
    repo.remove(generated_id);
}
TEST_F(RedisVarNodeRepositoryTest, UpdateIntegrity) {
    using VarNodeRepository = tmp::redis::r1::VarNodeRepository<std::string>;
    VarNodeRepository repo(redis, table, std::to_string(primary), ukey, uvalue);

    // --- 1. 初期データの Insert ---
    const std::string initial_name = "デレク";
    tmp::VarNode root("__root__", std::monostate{});
    root.addChild("id", std::to_string(primary));
    root.addChild("company_id", std::string{"E1_3000"});
    root.addChild("email", uvalue);
    root.addChild("name", initial_name);
    std::string generated_id = repo.insert(std::move(root));

    // --- 2. 更新データ (Update) の準備と実行 ---
    const std::string updated_company_id = "E1_3333"; // 変更点
    const std::string updated_name = "デレク改";      // 変更点

    tmp::VarNode update_root("__root__", std::monostate{});
    update_root.addChild("id", std::to_string(primary));
    update_root.addChild("company_id", updated_company_id);
    update_root.addChild("email", uvalue);
    update_root.addChild("name", updated_name);

    ASSERT_NO_THROW({
        repo.update(generated_id, std::move(update_root));
    });

    // --- 3. 再取得と検証 (Verification) ---
    VarNodeRepository find_repo{redis};
    std::optional<tmp::VarNode> result = find_repo.findById(generated_id);
    ASSERT_TRUE(result.has_value()) << "findById failed after update for ID: " << generated_id;

    // 更新後の値を検証
    EXPECT_EQ(result->getChild("company_id")->get<std::string>(), updated_company_id);
    EXPECT_EQ(result->getChild("name")->get<std::string>(),       updated_name);
    
    // 更新していない値（email）が維持されているか
    EXPECT_EQ(result->getChild("email")->get<std::string>(), uvalue);

    // --- 4. 後片付け ---
    repo.remove(generated_id);
}
TEST_F(RedisVarNodeRepositoryTest, RemoveIntegrity) {
    using VarNodeRepository = tmp::redis::r1::VarNodeRepository<std::string>;
    VarNodeRepository repo(redis, table, std::to_string(primary), ukey, uvalue);

    // --- 1. データの準備と Insert ---
    tmp::VarNode root("__root__", std::monostate{});
    root.addChild("id", std::to_string(primary));
    root.addChild("company_id", std::string{"E1_3000"});
    root.addChild("email", uvalue);
    root.addChild("name", std::string{"デレク"});
    std::string generated_id = repo.insert(std::move(root));
    // 存在する状態であることを念のため確認
    VarNodeRepository find_repo{redis};
    ASSERT_TRUE(find_repo.findById(generated_id).has_value());

    // --- 2. 実行 (Remove) ---
    ASSERT_NO_THROW({
        repo.remove(generated_id);
    });

    // --- 3. 検証 (Verification) ---
    // 削除後は findById が空を返すことを確認
    std::optional<tmp::VarNode> result = find_repo.findById(generated_id);
    EXPECT_FALSE(result.has_value()) << "Data still exists after removal for ID: " << generated_id;
    // 補足: 存在しないデータに対する Remove が例外を投げないか（冪等性の確認）
    EXPECT_NO_THROW({
        repo.remove(generated_id);
    });
}
