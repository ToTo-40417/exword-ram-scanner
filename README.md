# EX-word RAM Scanner

日本語 | [English](#english)

EX-word RAM Scannerは、CASIO EX-wordのアプリケーションから安全に利用できるRAM領域を実機上で調査するための研究用ツールです。

> [!CAUTION]
> このツールはRAMへ試験パターンを書き込みます。アドレス範囲を誤ると、フリーズ、未保存データの消失、再起動不能などを招く可能性があります。現行設定を実機確認したのはXD-N6500（DATAPLUS 7 / CY460）のみです。他機種でも利用できる可能性はありますが、アドレス配置や実行環境を確認し、復旧手段を用意したうえで自己責任で実行してください。

## 動作確認

- XD-N6500（DATAPLUS 7 / CY460）
- 対象論理範囲：`0x8c800000`–`0x8cffffff`（8 MiB）
- 単位：256 KiB × 32 blocks
- block 00–31のバックアップ、2種類のpattern検証、復元、復元後hash照合：完走

XD-B4800を含む他機種では、このアドレス構成を検証していません。DATAPLUS世代だけを根拠に同じ範囲を安全と判断せず、対象機の調査結果に応じて設定を見直してください。今後のDATAPLUS 8以降やDATAPLUS 5以前を対象外とはしていません。

## 動作

各blockについて次の処理を行います。

1. `BEGIN`を`RAMSCAN.TSV`へ記録し、ファイルを閉じる
2. 対象blockを別領域へ退避する
3. 2種類の決定的な試験patternを書き込み、read-back検証する
4. 元データを復元する
5. 復元後の内容とFNV-1a hashを検証する
6. `PASS`または失敗内容を記録する

起動時に既存ログを読み、最初の未完了blockから再開します。block間の待機中は「戻る」で安全に停止できます。試験中に入力不能となった場合のみ、電池を抜いて再起動してください。

この結果は「この実機・この起動条件で対象範囲のread/write/restoreが完走した」ことを示します。物理RAM総容量、恒久的な安全性、他機種との互換性を保証するものではありません。

## ビルド

devkitSH4と[`libdataplus`](https://github.com/brijohn/libdataplus)が必要です。依存物は同梱しないため、各配布元から取得してください。

```bash
export DEVKITPRO="$HOME/toolchains/devkitPro"
export DEVKITSH4="$DEVKITPRO/devkitSH4"
export PATH="$DEVKITPRO/tools/bin:$DEVKITSH4/bin:$PATH"
make
```

ビルド済みD01は配布せず、ソースのみを公開します。

## ライセンス

アプリ本体はGPL-2.0です。最小libcにはLGPL-2.1-or-later、BSD、public-domainのコードも含まれ、各ソースの著作権表示とライセンス表記を保持しています。詳細は[`COPYING`](COPYING)、[`COPYING.LIB`](COPYING.LIB)、[`THIRD_PARTY.md`](THIRD_PARTY.md)を参照してください。`libdataplus`は同梱せず、上流repoから取得します。

## English

EX-word RAM Scanner is a research tool for qualifying RAM ranges that can be used by an application on CASIO EX-word hardware.

> [!CAUTION]
> This tool writes test patterns directly to RAM. An incorrect address range may freeze the device, destroy unsaved state, or prevent a normal restart. The current configuration has been validated only on the XD-N6500 (DATAPLUS 7 / CY460). Other models may be usable, but only at the operator's own risk after reviewing their address layout and preparing a recovery path.

The validated configuration scans `0x8c800000`–`0x8cffffff` as 32 blocks of 256 KiB. For each block it closes a persistent `BEGIN` log record, backs up the original contents, verifies two deterministic patterns, restores the block, verifies the restored bytes and hash, and records the result. Blocks 00–31 completed on the tested XD-N6500.

This result qualifies one physical device under the tested boot conditions. It does not prove total physical RAM capacity, permanent safety, or compatibility with another EX-word model. Earlier and later DATAPLUS generations are not categorically excluded; their address ranges must be established independently before testing.

Build from source with devkitSH4 and [`libdataplus`](https://github.com/brijohn/libdataplus). Dependencies and prebuilt D01 files are not distributed here. The application is licensed under GPL-2.0. The bundled minimal libc also contains LGPL-2.1-or-later, BSD, and public-domain components as identified by their source headers; see [`COPYING`](COPYING), [`COPYING.LIB`](COPYING.LIB), and [`THIRD_PARTY.md`](THIRD_PARTY.md).

## 関連プロジェクト / Related projects

- [`EXLLM`](https://github.com/ToTo-40417/exllm) — model and training source
- [`exllm-exword`](https://github.com/ToTo-40417/exllm-exword) — EX-word inference runtime
- [`exllm-model-check`](https://github.com/ToTo-40417/exllm-model-check) — EXQ12 structural validator
- [`exword-hardware-dump`](https://github.com/ToTo-40417/exword-hardware-dump) — non-destructive hardware probe
- [`exword-gnuboy-save-importer`](https://github.com/ToTo-40417/exword-gnuboy-save-importer) — Gnuboy save-transfer utility
