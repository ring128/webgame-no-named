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

ホームで「スタート」→「石垣の庭」→所持している能力→「作戦開始」の順に選ぶと全画面で始まる。最初に使える能力は16種類からランダムで1種。渦を引いたら、猫の少し後ろをクリックすると引き戻せる。24秒守ればクリア。猫がゴールに入ると敗北。初勝利で次の能力が解放され、「再挑戦」から選べる。

「スキン」の上段で猫の種類、下段で平常時の表情を選ぶと、次のプレイにも引き継がれる。

## 意見の管理

ホームの「意見」から投稿すると、本文・ブラウザの匿名ID・投稿日時を `data/feedback/posts.sqlite3` に保存する。旧版の `data/feedback.jsonl` がある場合は初回に取り込む。旧版の投稿は日時と投稿者が不明なので、本文の検索と閲覧ができる。

管理画面は http://localhost:8848/admin.html 。以下をWSLのリポジトリ直下で実行し、表示された管理キーを画面に入力する。キーは初回の `bash play.sh` で生成され、次回の起動でも使える。

```sh
cat data/feedback-admin.key
```

本文または匿名IDの一部で検索できる。「この投稿者をブロック」を選ぶと、既存の意見を残したまま同じ匿名IDの以後の投稿を拒否する。匿名IDはブラウザのCookieで識別し、別のブラウザやCookieを消した後の投稿には別のIDが付く。

## ランキングとGoogleログイン

クリア時の猫の位置から、妨害をすべて解除して通常速度で最短経路を歩き、ゴールに入るまでの秒数をサーバーが計算する。同じゲームサーバーでの自己ベストを長い順に掲載し、同タイムは同順位とする。未ログインで出した自己ベストも、同じブラウザでログインしてランキングを開くと登録される。能力の解放、スキン、記録の保存先は `data/progress.json`。

Google Cloudでウェブアプリケーション用のOAuthクライアントを作成し、承認済みのリダイレクトURIに `http://localhost:8848/auth/google/callback` を登録する。テスト公開中は利用するGoogleアカウントをテストユーザーに登録する。WSLのターミナルで次を実行し、発行された値を入力する。シークレットの入力中は文字が表示されない。[Googleの設定手順](https://developers.google.com/identity/openid-connect/openid-connect)

```sh
read -rp 'Client ID: ' GOOGLE_CLIENT_ID
read -rsp 'Client secret: ' GOOGLE_CLIENT_SECRET
echo
export GOOGLE_CLIENT_ID GOOGLE_CLIENT_SECRET
bash play.sh
```

ブラウザは登録したURIと同じホスト名の `http://localhost:8848/` で開き、「ランキング」→「Googleでログイン」を選ぶ。Googleを設定する前でもステージを遊べる。

ステージの検証は `ctest --test-dir backend/build/local --output-on-failure`。能力ごとの単独クリア時のシード・時刻・クリック座標は `backend/build/local/stage_test` の出力で確認できる。
