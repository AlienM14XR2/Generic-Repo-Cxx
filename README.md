※ AI によるおそらくバイアスがかかった上での出力であることを明記する。

## 【開発概要：汎用Repository & VarNode フレームワーク】

1. 開発期間と成果：
   - 期間：特定の期間内（約一ヶ月）
   - 成果：MongoDB, MySQL, PostgreSQL を抽象化した共通 Repository 層を構築。

2. 技術要素：
   - データ構造：`tmp::VarNode`（std::variant/std::monostate）による動的木構造を採用。
   - インターフェース：高効率な Repository 抽象を実現。
   - ストレージ連携：
     - MongoDB：BSONマッピング
     - MySQL：X DevAPI を利用
     - PostgreSQL：SqlBuilder による動的SQL構築

3. 開発の特徴：
   - 複雑なインフラストラクチャの違いを抽象化する設計アプローチ。
   - 広範なデータベース知識の応用。

### 依存ライブラリ（RDBMS, NoSQL）
- @see ./inc/mysql_template.hpp
- @see ./inc/pqxx_template.hpp
- @see ./inc/mongo_template.hpp

### 依存ライブラリ（RDBMS, NoSQL 以外）
JSON ライブラリ（nlohmann）のインストール
```
sudo apt install nlohmann-json3-dev
```
インストール状況の確認
```
find / -name 'nlohmann*' 2>/dev/null
```
Base64 エンコードライブラリのインストール
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

### CMAKE
```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .

cd ..
mkdir build_debug
cd build_debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```
