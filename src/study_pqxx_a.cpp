/**
 * 習作　PostgreSQL に関するもの
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

// PostgreSQL
DROP TABLE IF EXISTS fulltype;
CREATE TABLE fulltype (
id              SERIAL          PRIMARY KEY,
email           VARCHAR(100)    NOT NULL,
memo            TEXT            ,
age             INTEGER         ,
on_busy         BOOLEAN         DEFAULT FALSE,
col_float       FLOAT           ,
col_double      DOUBLE PRECISION,
created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);
ALTER TABLE fulltype ADD CONSTRAINT fulltype_email_uk unique (email);
INSERT INTO fulltype(email, memo, age, on_busy, col_float, col_double)
VALUES
('admin@loki.org', 'football player', 33, TRUE, 3.141592, 35.6809591);

// バイナリ型の確認用
ALTER TABLE fulltype ADD COLUMN raw_data BYTEA;


 * 
 * e.g. compile
 * g++ -O3 -DDEBUG -std=c++20 -pedantic-errors -Wall -Wextra -Werror -I../inc -I/usr/local/include -I/usr/include/postgresql -L/usr/local/lib study_pqxx_a.cpp VarNode.cpp -lpqxx -lpq -o ../bin/study_pqxx_a
 */
#include <iostream>
#include <cassert>
#include <pqxx_template.hpp>

constexpr std::string table = "fulltype";
constexpr std::string idseq = "fulltype_id_seq";
constexpr std::string pkey  = "id";

int test_FindById(const uint64_t& _id)
{
    puts("------ test_FindById");
    using Data = tmp::VarNode;
    try {
        auto dataMapper = [](Data& root, const pqxx::row& row) -> void {
            // テーブルで、null を許可している場合は const char* を使わざるを得ない。
            auto [id, email, memo, age, on_busy, col_float, col_double, created_at, updated_at, raw_data]
            = row.as<uint64_t, std::string, std::string, int64_t, bool, float, double, std::string, std::string, const char*>();
            tmp::print_debug(id, email, memo, age, on_busy, col_float, col_double, created_at, updated_at, raw_data);
            root.addChild("id", id);
            root.addChild("email", email);
            root.addChild("memo", memo);
            root.addChild("age", age);
            root.addChild("on_busy", on_busy);
            root.addChild("col_float", col_float);
            root.addChild("col_double", col_double);
            root.addChild("created_at", created_at);
            root.addChild("updated_at", updated_at);
            if(raw_data) {
                // ポインタとサイズから新しいvectorを作成（コピー）
                std::vector<uint8_t> buffer(raw_data, raw_data + strlen(raw_data));
                root.addChild("raw_data", buffer);
            }
            else root.addChild("raw_data", std::monostate{});
            // if(roles) root.addChild("roles", std::string(roles));
            // else root.addChild("roles", std::monostate{});
        };
        pqxx::connection conn{"hostaddr=127.0.0.1 port=5432 dbname=derek user=derek password=derek1234"};
        pqxx::work tx(conn);
        std::unique_ptr<tmp::Repository<uint64_t, Data>>
            repo = std::make_unique<tmp::postgres::r3::VarNodeRepository<uint64_t>>(&tx, table, idseq, dataMapper, pkey);

        std::optional<Data> result = repo->findById(_id);
        tx.commit();
        conn.close();
        if(result) tmp::debug_print_varnode(&(result.value()));
        else throw std::runtime_error("UnExpects result.");
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
    using Derived = tmp::postgres::r3::VarNodeRepository<uint64_t>;
    try {
        pqxx::connection conn{"hostaddr=127.0.0.1 port=5432 dbname=derek user=derek password=derek1234"};
        pqxx::work tx(conn);
        Derived repo{&tx, table, idseq, pkey};

        Data root{"_ROOT_KEY_", std::monostate{}};
        uint64_t nid{0};
        root.addChild(pkey, nid);
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

        *_id = repo.insert(std::move(root));
        tmp::print_debug("_id: ", *_id);
        tx.commit();
        conn.close();
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
    using Derived = tmp::postgres::r3::VarNodeRepository<uint64_t>;
    try {
        pqxx::connection conn{"hostaddr=127.0.0.1 port=5432 dbname=derek user=derek password=derek1234"};
        pqxx::work tx(conn);
        Derived repo{&tx, table, idseq, pkey};

        Data root{"_ROOT_KEY_", std::monostate{}};
        uint64_t nid{0};
        root.addChild(pkey, nid);
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

        repo.update(*_id, std::move(root));
        tmp::print_debug("_id: ", *_id);
        tx.commit();
        conn.close();
        return EXIT_SUCCESS;
    } catch(std::exception& e) {
        tmp::ptr_print_error<decltype(e)&>(e);
        return EXIT_FAILURE;
    }
}

int test_remove(uint64_t* _id)
{
    puts("------ test_remove");
    using Derived = tmp::postgres::r3::VarNodeRepository<uint64_t>;
    try {
        pqxx::connection conn{"hostaddr=127.0.0.1 port=5432 dbname=derek user=derek password=derek1234"};
        pqxx::work tx(conn);
        Derived repo{&tx, table, idseq, pkey};

        repo.remove(*_id);
        tmp::print_debug("_id: ", *_id);
        tx.commit();
        conn.close();
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
        // tmp::print_debug("Play and Result ... ", ret = test_FindById(1UL));
        // assert(ret == 0);
        uint64_t id = 0ul;
        tmp::print_debug("Play and Result ... ", ret = test_insert(&id));
        assert(ret == 0);
        tmp::print_debug("Play and Result ... ", ret = test_update(&id));
        assert(ret == 0);
        tmp::print_debug("Play and Result ... ", ret = test_FindById(id));
        assert(ret == 0);
        tmp::print_debug("Play and Result ... ", ret = test_remove(&id));
        assert(ret == 0);
    }
    puts("=== main END");
    return EXIT_SUCCESS;
}