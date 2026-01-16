/**
 * 習作　MySQLに関するもの
 * 
 * 学習、実験、簡易サンプリング等、なんでもありの場所。
 * 
 * 今さらだが、ValueType が現在対応している型のマッチングを行っていなかった。
 * 
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

// 次のテーブルを用意する。
DROP TABLE IF EXISTS fulltype;
CREATE TABLE fulltype (
id              SERIAL          PRIMARY KEY,
email           VARCHAR(100)    NOT NULL,
memo            TEXT            ,
age             INTEGER         ,
on_busy         BOOLEAN         DEFAULT 0,
col_float       FLOAT           ,
col_double      DOUBLE PRECISION,
created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);
ALTER TABLE fulltype ADD CONSTRAINT email_uk unique (email);
INSERT INTO fulltype(email, memo, age, on_busy, col_float, col_double)
VALUES
('admin@loki.org', 'football player', 33, 1, 3.141592, 35.6809591);

// バイナリ型の確認用
ALTER TABLE fulltype ADD COLUMN raw_data VARBINARY(256) AFTER col_double;

// JSON ライブラリ（nlohmann）のインストール
e.g. 
```
sudo apt install nlohmann-json3-dev
```
// インストール状況の確認
```
find / -name 'nlohmann*' 2>/dev/null
```

// Base64 エンコードライブラリのインストール
```
cd ~/tmp/
git clone https://github.com/matheusgomes28/base64pp.git
cd base64pp
mkdir build
cd build
cmake ..
make
sudo make install
```
 * 
 * 
 * e.g. compile
 * 実験、サンプリングでは以下を推奨（-Wextra があると簡単に関数内をすべてコメントとかできないから。
 * g++ -O3 -std=c++20 -DDEBUG -pedantic-errors -Wall -Werror -I../inc/ -I/usr/include/mysql-cppconn/ -L/usr/lib/x86_64-linux-gnu/ study_mysql_a.cpp VarNode.cpp -lmysqlcppconn -lmysqlcppconnx -lbase64pp -o ../bin/study_mysql_a
 */
#include <iostream>
#include <mysql_template.hpp>

constexpr std::string dbName = "test";
constexpr std::string tableName = "fulltype";
constexpr std::string primaryKeyName = "id";


int test_VarNode_Deep_Copy()
{
    puts("------ test_VarNode_Deep_Copy");
    using Data = tmp::VarNode;
    Data* cpy = nullptr;
    try {
        Data root{"_ROOT_KEY_", std::monostate{}};
        std::string email = "derek@loki.org";
        Data* nodeEmail = root.addChild("email", email);
        // nodeEmail 用のブロック
        {
            std::string companyId{"B3_1000"};
            nodeEmail->addChild("company_id", companyId);
            std::string password{"derek1111"};
            nodeEmail->addChild("password", password);
            std::string name{"Derek"};
            nodeEmail->addChild("name", name);
        }
        std::string memo = "Baseball player, PG";
        root.addChild("memo", memo);
        uint64_t age = 30ul;
        root.addChild("age", age);
        bool on_busy = false;
        root.addChild("on_busy", on_busy);
        float col_float = 3.14;
        root.addChild("col_float", col_float);
        float col_double = 35.6809591;
        root.addChild("col_double", col_double);
        std::vector<uint8_t> raw_data;
        raw_data.push_back('a');raw_data.push_back('1');raw_data.push_back('b');raw_data.push_back('2');
        root.addChild("raw_data", raw_data);
        // ディープコピ前の確認
        puts("original ...");
        tmp::debug_print_varnode(&root);
        cpy = tmp::new_deep_copy_varnode(&root);
        puts("deep copy ...");
        tmp::debug_print_varnode(cpy);
        if(cpy) {
            delete cpy;
            cpy=nullptr;
        }
        return EXIT_SUCCESS;
    } catch(std::exception& e) {
        if(cpy) delete cpy;
        tmp::ptr_print_error<decltype(e)&>(e);
        return EXIT_FAILURE;
    }
}

int test_FindById(const uint64_t& _id)
{
    puts("------ test_FindById");
    using Data = tmp::VarNode;
    using Derived = tmp::mysql::r3::VarNodeRepository<uint64_t>;
    try {
        mysqlx::Session sess{"localhost", 33060, "root", "root1234"};
        Derived repo{&sess, dbName, tableName, primaryKeyName};
        std::optional<Data> node = repo.findById(_id);
        if(!node) throw std::runtime_error("Unexpected case.");
        else tmp::debug_print_varnode(&(node.value()));
        if(node) {
            nlohmann::json j;
            tmp::to_json(j, node.value());
            // std::cout << j.dump(4) << std::endl;
            tmp::print_debug("json: ", j.dump(4));
            // tmp::from_json() の確認
            Data node2 = Data{"__ROOT__NODE2__", std::monostate{}};
            tmp::from_json(j, node2);
            tmp::debug_print_varnode(&node2);
        }
        return EXIT_SUCCESS;
    } catch(std::exception& e) {
        tmp::ptr_print_error<decltype(e)&>(e);
        return EXIT_FAILURE;
    }
}

int test_insert(uint64_t* _id)
{
    puts("------ test_insert");
    using Data = tmp::VarNode;
    using Derived = tmp::mysql::r3::VarNodeRepository<uint64_t>;
    try {
        std::string rval = "fulltype";
        Data root{"_ROOT_KEY_", rval};
        std::string email = "derek@loki.org";
        root.addChild("email", email);
        std::string memo = "Baseball player, PG";
        root.addChild("memo", memo);
        uint64_t age = 30ul;
        root.addChild("age", age);
        bool on_busy = false;
        root.addChild("on_busy", on_busy);
        float col_float = 3.14;
        root.addChild("col_float", col_float);
        float col_double = 35.6809591;
        root.addChild("col_double", col_double);
        std::vector<uint8_t> raw_data;
        raw_data.push_back('a');raw_data.push_back('1');raw_data.push_back('b');raw_data.push_back('2');
        root.addChild("raw_data", raw_data);
        mysqlx::Session sess{"localhost", 33060, "root", "root1234"};
        Derived repo{&sess, dbName, tableName, primaryKeyName};
        sess.startTransaction();
        *_id = repo.insert(std::move(root));
        tmp::print_debug("_id: ", *_id);
        sess.commit();
        return EXIT_SUCCESS;
    } catch(std::exception& e) {
        tmp::ptr_print_error<decltype(e)&>(e);
        return EXIT_FAILURE;
    }
}

int test_update(uint64_t* _id)
{
    puts("------ test_update");
    using Data = tmp::VarNode;
    using Derived = tmp::mysql::r3::VarNodeRepository<uint64_t>;
    try {
        std::string rval = "fulltype";
        Data root{"_ROOT_KEY_", rval};
        std::string email = "derek_u@loki.org";
        root.addChild("email", email);
        std::string memo = "Baseball player, PG, DBA";
        root.addChild("memo", memo);
        uint64_t age = 33ul;
        root.addChild("age", age);
        bool on_busy = true;
        root.addChild("on_busy", on_busy);
        float col_float = 3.141592;
        root.addChild("col_float", col_float);
        float col_double = 35.6809591;
        root.addChild("col_double", col_double);
        std::vector<uint8_t> raw_data;
        raw_data.push_back('a');raw_data.push_back('1');
        root.addChild("raw_data", raw_data);
        mysqlx::Session sess{"localhost", 33060, "root", "root1234"};
        Derived repo{&sess, dbName, tableName, primaryKeyName};
        sess.startTransaction();
        repo.update(*_id, std::move(root));
        tmp::print_debug("_id: ", *_id);
        sess.commit();
        return EXIT_SUCCESS;
    } catch(std::exception& e) {
        tmp::ptr_print_error<decltype(e)&>(e);
        return EXIT_FAILURE;
    }
}

int test_remove(uint64_t* _id)
{
    puts("------ test_remove");
    using Derived = tmp::mysql::r3::VarNodeRepository<uint64_t>;
    try {
        mysqlx::Session sess{"localhost", 33060, "root", "root1234"};
        Derived repo{&sess, dbName, tableName, primaryKeyName};
        sess.startTransaction();
        repo.remove(*_id);
        tmp::print_debug("_id: ", *_id);
        sess.commit();
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
        // tmp::print_debug("Play and Result ... ", ret = test_FindById(1));
        // assert(ret == 0);
        uint64_t id = 0;
        tmp::print_debug("Play and Result ... ", ret = test_insert(&id));
        assert(ret == 0);
        tmp::print_debug("Play and Result ... ", ret = test_update(&id));
        assert(ret == 0);
        // tmp::print_debug("Play and Result ... ", ret = test_FindById(1L));
        tmp::print_debug("Play and Result ... ", ret = test_FindById(id));
        assert(ret == 0);
        tmp::print_debug("Play and Result ... ", ret = test_remove(&id));
        assert(ret == 0);
    }
    if(1) {
        tmp::print_debug("Play and Result ... ", ret = test_VarNode_Deep_Copy());
        assert(ret == 0);
    }
    puts("=== main END");
    return EXIT_SUCCESS;
}
