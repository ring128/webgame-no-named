# ローカルで遊ぶ

このWSL環境ではリポジトリのルートから次を実行する。

```sh
bash play.sh
```

Windowsのブラウザで http://localhost:8848/ を開く。WSLのターミナルで Ctrl+C を押すと終了する。起動時にTypeScriptとC++をビルドし、Drogonが画面とAPIを同じポートで配信する。

新しいUbuntu 24.04環境ではNode.js 22.12以降とnpmを用意し、次のビルド依存を導入する。DBサービスの起動は不要。

```sh
sudo apt update
sudo apt install build-essential cmake libdrogon-dev libjsoncpp-dev uuid-dev zlib1g-dev libpq-dev libsqlite3-dev libmariadb-dev-compat libbrotli-dev libhiredis-dev libyaml-cpp-dev
bash play.sh
```

「ゲーム開始」を押し、猫がゴールに入るまでの進路を妨害する。初期能力は壁。中央の石垣にある1マスの通路をふさぎ、必要なら消える前に置き直す。24秒守ればクリア。初勝利で次の能力が解放され、「再挑戦」から選べる。「猫の見た目」で選んだ表情と色はゲーム開始時に保存される。進捗はリポジトリ内の `data/progress.json` に保存される。

ステージの検証は `ctest --test-dir backend/build/local --output-on-failure`。能力ごとの単独クリア時のシード・時刻・クリック座標は `backend/build/local/stage_test` の出力で確認できる。
