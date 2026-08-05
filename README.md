# -px68k by TurtleBazooka-

## From(thanx!)
* fork from kenyahiro(https://github.com/kenyahiro/px68k)
* Original fork from hissorii(https://github.com/hissorii/px68k)

## License
* 大元のオリジナルソースの著作は、けんじょさんのWinX68k(けろぴー)です。(http://retropc.net/kenjo)
* c68k(MC68000 Emu.)はStephane Dallongeville氏がyabauseの一部としてGPLv2で公開されているものです。
* MusashiはKarl Stenerud氏によるemulation engineです。(https://github.com/kstenerud/Musashi)
* FM音源合成のfmgenは、cisc氏の著作物です。(詳しくはfmgen/readme.txt参照)
* YMFMはBSD-licensedで公開されています。(https://github.com/aaronsgiles/ymfm)

## About
* 単独アプリ形式のエミュレータの改良を目指します。
* (DWORD)-1 で頻繁に異常終了する問題に対処
* Support Full-Screen Mode.(`F11`)
* ソフトキーボード復活(Right click on `F12`Menu mode)
* SCSI DiskImageサポート(`Can boot from *.HDS`)
* 最終画面出力を24bitに変更(RGB565→RGBX8888)
* Fileを画面へDrag&Dropで自動的にFDD/HDDにマウント
* `Support MIDI-Play` (Internal/Munt/fluidsynth/USB-MIDI)
* Support US-KeyBoard. `keyconf.dat` put into .keropi folder
* Printer出力はFileに保存
* Can use XBOX like USB-GamePad (hot-pluggable)
* Add support CyberStick! (DIGITAL/ANALOG mode)
* `FileNameのutf8/sjis` 自動判別日本語表示(専用table変換)
* 日本語TrueTypeから cgrom.tmp 生成可能
* YM2151(FM音源)のエミュレートにfmgen/YMFMが使用可能
* macOS/Linux/MinGW64(win) で動作確認

## for macOS
* Mac用単独アプリ化ラッパーとアイコン追加
* SDL3/2.frameworkでのbuildにも対応
* pngからiconリソース生成機能搭載(% make icon)

## Need
* SDL3 (or still support SDL2)(https://www.libsdl.org/)
* `iplrom/iplromxv/iplromco/iplrom30.dat` `cgrom.dat` and SCSI-IPL `scsiexrom.dat` into .keropi folder
* On SDL3,`gamepaddb.txt` into .keropi folder.(if needed)(change platform `Mac OS X` to `macOS`)
* On SDL2,`gamecontrollerdb.txt` into .keropi folder (if needed)(https://github.com/gabomdq/SDL_GameControllerDB)
* cmake and pkg-config build system (if needed)(https://cmake.org)
* Munt/MT-32 emu (if needed)(https://sourceforge.net/projects/munt/)
* fluidsynth and SoundFont2 (if needed)(https://www.fluidsynth.org/)

## Build (with make)

```sh
[for macOS]
 $ make YMFM=ON mac
 
[for Linux]
 $ make YMFM=ON
 
[for Win(MinGW)]
 $ make YMFM=ON win

[option]
 $ make YMFM=ON FLUID=ON (use fluidsynth for MIDI)
or
 $ make (use fmgen for YM2151)
or
 $ make SDL2=ON NO_MIDI=ON (SDL2 with No MIDI Support)

 $ make clean (お掃除)
 $ make cgrom (app for Generate cgrom.tmp)
```
## Build (need cmake)

```sh
[for macOS/Linux/MinGW]
$ cmake -S . -B build -D YMFM=ON
$ cmake --build build --config Release
 
[on SDL2 and use fmgen for YM2151]
$ cmake -S . -B build -D SDL2=ON
$ cmake --build build --config (RelWithDebInfo/Debug/MinSizeRel)
```

## Support image file (mount and boot)

      FDD-image         D88,88D,HDM,DUP,2HD,DIM,XDF,IMG
      HDD-image(SASI)   HDF(10/20/40MB)
      HDD-image(SCSI)   HDS(max.900MB)
      MO -image(SCSI)   MOS(128/230/540/640MB) at ID=5

## Chg Menu Language
* edit `config` file in .keropi folder.

      MenuLanguage=0  default(Japanese Menu)
      MenuLanguage=1  option (English  Menu)

## Limitation
 * SCSI対応は現状IOCSレベル。よってNetBSD/X68kはboot不可
 * SCSI-IPLの機能もEmu側で実装予定 (未)
 * WinのMIDIはデバイスリストから選択可
 * macのMIDIはCoreMIDIデバイスリストから選択可
 * LinuxのMIDIは現状fluidsynthのみ
 * MIDI-INは暫定対応(WinMM,and CoreMIDI)
 * CyberStickのアナログモードは確認継続中(認識不完全アリ)
 * SDL1のサポートは削除しました。
 * SDL3に正式対応。(SDL2も絶賛サポート中)
 * BigEndianのCPUでは動きません。(例:Wii/U,Mac/G4)

## How to make `cgrom.tmp`
 * cgrom.datがない場合用に代替ファイルをTrueTypeから生成できます。
 * generate from Japanese-TrueType Font.(need `SDL3_ttf`)
 * maked `cgrom.tmp` put into .keropi folder.

```sh
  $ make cgrom
  $ ./mkcgrom
  or
  $ ./mkcgrom ./pri.ttf ./sec.ttf
```
