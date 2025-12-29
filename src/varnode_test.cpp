#include <gtest/gtest.h>
#include <Repository.hpp>

namespace {
    using namespace tmp;

    class VarNodeTest : public ::testing::Test {
    protected:
        // ルートノードを準備
        VarNode root{"root", std::monostate{}};
    };

    // 1. 新規追加の検証
    TEST_F(VarNodeTest, AddChild_NewKey) {
        auto* child = root.addChild("key1", int64_t{100});
        
        ASSERT_NE(child, nullptr);
        EXPECT_EQ(child->key, "key1");
        EXPECT_EQ(std::get<int64_t>(child->data), 100);
        EXPECT_EQ(root.children.size(), 1);
        EXPECT_EQ(child->parent, &root);
    }

    // 2. 同じキーによる上書きの検証
    TEST_F(VarNodeTest, AddChild_OverwriteExistingKey) {
        root.addChild("key1", int64_t{100});
        size_t initial_size = root.children.size();

        // 同じキーで異なる値を追加（上書き）
        auto* updated_child = root.addChild("key1", int64_t{200});

        EXPECT_EQ(root.children.size(), initial_size); // サイズが増えていないこと
        EXPECT_EQ(std::get<int64_t>(updated_child->data), 200); // 値が更新されていること
        EXPECT_EQ(root.getChild("key1"), updated_child); // ポインタが一致すること
    }

    // 3. 型が異なる上書きの検証 (variant の動的な型入れ替え)
    TEST_F(VarNodeTest, AddChild_OverwriteWithDifferentType) {
        root.addChild("key1", int64_t{100});

        // int64_t 型のノードを std::string で上書き
        root.addChild("key1", std::string("overwritten"));

        auto* child = root.getChild("key1");
        ASSERT_NE(child, nullptr);
        EXPECT_TRUE(child->is_type<std::string>());
        EXPECT_EQ(std::get<std::string>(child->data), "overwritten");
        EXPECT_FALSE(child->is_type<int64_t>()); // 古い型は保持されていないこと
    }

    // 4. 親子関係の維持検証
    TEST_F(VarNodeTest, AddChild_MaintainParentPointerAfterOverwrite) {
        root.addChild("key1", int64_t{100});
        auto* child = root.addChild("key1", int64_t{200});

        EXPECT_EQ(child->parent, &root); // 上書き後も親へのポインタが正しいこと
    }
}
