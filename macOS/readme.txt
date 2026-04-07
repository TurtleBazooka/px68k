Part of SDL3/2 for px68k.
written by kameya  2021/11/23

folder [macOS]
build for double clickable aplication resource.

sdl2.pc,sdl3.pc script put-in /usr/local/lib/pkgconfig folder.
it's build with SDL2/3.Framework use with pkgconfig build system.

this script for --lib, --cflags LINKER option, for LINK with FrameWork SDL2/3.

How to build for mac.

$ make
and...
$ make mac

build for GNU/Linux/MinGW(win)
$ make

joy for your Mac life!


[日本語解説] by kameya
Winにはxm6というスバらしいエミュがありますよね。
しかし、Macにはここ最近、単独アプリのX68Kのエミュはありません。(PowerPCの頃にはあった)
RetroArchのPluginで、PX68Kが出ているのが唯一の救いです。

単独アプリでMacで動くX68Kエミュレータがあったらいいな、と思い
kenyahiroさんが64bit対応されてましたGitから、Forkさせていただきました。(感謝)
Framework版のSDL3をメインに、MacBookAir(2014)/Montereyで開発しています。
主な特徴は以下です。
・SDL3/2のGPU活用(Texture、Renderer活用)
・フルスクリーンモード追加(F11)
・画面表示の実機同等の再現性を追求
・24bitカラーで最終画面出力(RGB565→RGBX8888に変更)
・輝度変更はCRTみたいに徐々に残光しながら変化します。
・SDL1は削除(Discon)、SDL3推奨（SDL2も絶賛サポート中）
・SCSIのDiskImageサポート(*.HDSからBootできます。)
・MOイメージサポート(128/230/540/640MBの*.MOSをマウント可)
・MIDI演奏に対応(Win/Mac)　※Linuxは現状Fluidsynthのみ
・MIDI-INも一応対応(Win/Mac)
・SoundはStreamingになります。(SDL3)
・PrintOutをFileに出力
・USキーボードのサポート(keyconf.datを.keropiに入れてください)
・cgrom.tmpを自前生成可能(cgrom.datは公式からは配布されてない)
・ソフトキーボードサポート復活(Menu上で右クリック)
・画面上にImageFileをDrag&Dropすると自動的にマウント
・File選択時のリスト表示をutf8対応に(X68搭載Fontの範囲で)
・グラフィックモードの再現性向上(横スクロール、部分表示)
・XBOXコンパチのGamePadをサポート。HotPlugで認識します。
・サイバースティックのアナログ入力にも対応（ただし非対応アプリもあります）
・LinuxやMinGW(win)でもコンパイル可能
・Macの単独アプリとしての生成リソースを追加

[実行]
SDL3/2のFrameWorkを/Library/Frameworksに入れておいてください。
cgrom.tmpを自前生成する場合はSDL3_ttf.frameworkも必要です。

[コンパイル]
% make
and..
% make mac

または、
% mkdir build
% cd build
% cmake ..
% cmake --build .

[cgrom.tmpの生成機能]
配布されていないcgromをOSのTrueTypeから生成する場合は、
% make cgrom
% ./mkcgrom で実行、生成。
できあがったcgrom.tmpを.keropiに入れておいてください。

[必要なIPL]
iplrom.dat/iplromxv.dat　、　cgrom.dat/cgrom.tmp 、　scsiexrom.dat
を.keropiに入れておいてください。

[GameController対応]
SDL2のGameController対応は、対応したGamePad記述のあるgamecontrollerdb.txt
を.keropiに入れておいてください。

[GamePad対応]
SDL3のGamePad対応はSDL2のgamecontrollerdb.txt　を　gamepaddb.txt にCOPYし、
Mac OS X を　macOS に全置換して、同じく.keropiに入れておいてください。

欠点もたくさんあります。
動作実績のあったAndroid/iPhoneでの動作確認はしていません。
SCSIサポートはIOCSレベルなのでNetBSD/X68KやSCSIコントローラを
直接操作するドライバ系は動きません。
インジケータ類(HDDのアクセスLEDなど)は皆無。でもシンプルでいいよね？(笑)

utf8対応は最初iconvを使ってみましたがX68Kの搭載fontとの相性が
イマイチと感じたので、結局Table変換してます。utf8-macでも濁点表示可能です。

ネット上でX68000の解析や改造を精力的に行われてる方々が公開
されている情報にホントに助けられました。 このユーザパワーは
素晴らしいですね。　あらためてお礼申し上げます。
