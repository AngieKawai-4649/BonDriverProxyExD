# Windows用TV-TUNER制御アプリケーション BonDriverProxyExD  

ベース：誰かが対応したB25デコード版  

## 【改造箇所】  
１．64bitビルド対応  
２．b25デコーダーの解放をしておらずBonDriverを切り替える度にメモリリークするので以下の修正を実施

- BonDriverを解放するタイミングでb25デコーダーをリリースする
- b25デコーダーソースコードをプロジェクトに抱え込んでビルドしていたのをdecoderDLLを動的リンクするように修正
- decoderDLLを複数設定できるように修正

 ３．通知アイコンに表示されないことがあるので表示するように修正  
 ４．VS2022 にてソリューション作り直し、ワーニング除去、細かいバグ修正 ソースコード整理  
 ５．iniファイルフォーマット変更

 - フォーマット詳細はBonDriverProxyExD.iniファイルを参照  

## 【セットアップ】  
- BonDriverProxyExD：TV-TUNERが接続されたPC上でセットアップするTV-TUNER制御アプリケーション  
- BonDriverProxy.dll：TV-TUNERをネットワーク越しに使用するクライアントPCでセットアップするdll

Docフォルダにセットアップガイドがあるので参照  
