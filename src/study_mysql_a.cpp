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
    std::string
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

 * 
 * 
 * e.g. compile
 * 実験、サンプリングでは以下を推奨（-Wextra があると簡単に関数内をすべてコメントとかできないから。
 * g++ -O3 -std=c++20 -DDEBUG -pedantic-errors -Wall -Werror -I../inc/ -I/usr/include/mysql-cppconn/ -L/usr/lib/x86_64-linux-gnu/ study_mysql_a.cpp -lmysqlcppconn -lmysqlcppconnx -o ../bin/study_mysql_a
 */
#include <iostream>
#include <mysql_template.hpp>

int test_FindById(const uint64_t& _id)
{
    puts("------ test_FindById");
    using Data = tmp::VarNode;
    using derived = tmp::mysql::r3::VarNodeRepository<uint64_t>;
    const std::string dbName = "test";
    const std::string tableName = "fulltype";
    const std::string primaryKeyName = "id";
    try {
        mysqlx::Session sess{"localhost", 33060, "root", "root1234"};
        derived repo{&sess, dbName, tableName, primaryKeyName};
        std::optional<Data> node = repo.findById(_id);
        if(!node) throw std::runtime_error("Unexpected case.");
        else tmp::debug_print_varnode(&(node.value()));
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
    if(1) {
        tmp::print_debug("Play and Result ... ", ret = test_FindById(1));
        assert(ret == 0);
    }
    puts("=== main END");
    return EXIT_SUCCESS;
}
