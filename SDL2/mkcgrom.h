/*
X68000 cgromをOSのTrueTypeで描画して作り出す。
buf:font[$c0000]
pri:16x16で使うTrueTypeのURL
sec:24x24で使うTrueTypeのURL
x68030用フォント？(未使用)
*/

int32_t make_cgromdat(uint8_t *buf, char *pri, char *sec, uint32_t x68030);

/* utf8 Mapping
 for X68000 utf8 font data. (全角ANK、半角ANK、JIS第一水準、JIS第２水準)
  2022/11/3  by kameya 
*/

/* UTF8          字(full width)*/
static const uint32_t x68_ank[] = {
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/

  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE28692,    /*→*/
  0xE28690,    /*←*/
  0xE28691,    /*↑*/
  0xE28693,    /*↓*/

  0xE38080,    /*　*/
  0xEFBC81,    /*！*/
  0xE2809D,    /*”*/
  0xEFBC83,    /*＃*/
  0xEFBC84,    /*＄*/
  0xEFBC85,    /*％*/
  0xEFBC86,    /*＆*/
  0xE28099,    /*’*/
  0xEFBC88,    /*（*/
  0xEFBC89,    /*）*/
  0xEFBC8A,    /*＊*/
  0xEFBC8B,    /*＋*/
  0xEFBC8C,    /*，*/
  0xE383BC,    /*－*/
  0xEFBC8E,    /*．*/
  0xEFBC8F,    /*／*/

  0xEFBC90,    /*０*/
  0xEFBC91,    /*１*/
  0xEFBC92,    /*２*/
  0xEFBC93,    /*３*/
  0xEFBC94,    /*４*/
  0xEFBC95,    /*５*/
  0xEFBC96,    /*６*/
  0xEFBC97,    /*７*/
  0xEFBC98,    /*８*/
  0xEFBC99,    /*９*/
  0xEFBC9A,    /*：*/
  0xEFBC9B,    /*；*/
  0xEFBC9C,    /*＜*/
  0xEFBC9D,    /*＝*/
  0xEFBC9E,    /*＞*/
  0xEFBC9F,    /*？*/

  0xEFBCA0,    /*＠*/
  0xEFBCA1,    /*Ａ*/
  0xEFBCA2,    /*Ｂ*/
  0xEFBCA3,    /*Ｃ*/
  0xEFBCA4,    /*Ｄ*/
  0xEFBCA5,    /*Ｅ*/
  0xEFBCA6,    /*Ｆ*/
  0xEFBCA7,    /*Ｇ*/
  0xEFBCA8,    /*Ｈ*/
  0xEFBCA9,    /*Ｉ*/
  0xEFBCAA,    /*Ｊ*/
  0xEFBCAB,    /*Ｋ*/
  0xEFBCAC,    /*Ｌ*/
  0xEFBCAD,    /*Ｍ*/
  0xEFBCAE,    /*Ｎ*/
  0xEFBCAF,    /*Ｏ*/

  0xEFBCB0,    /*Ｐ*/
  0xEFBCB1,    /*Ｑ*/
  0xEFBCB2,    /*Ｒ*/
  0xEFBCB3,    /*Ｓ*/
  0xEFBCB4,    /*Ｔ*/
  0xEFBCB5,    /*Ｕ*/
  0xEFBCB6,    /*Ｖ*/
  0xEFBCB7,    /*Ｗ*/
  0xEFBCB8,    /*Ｘ*/
  0xEFBCB9,    /*Ｙ*/
  0xEFBCBA,    /*Ｚ*/
  0xEFBCBB,    /*［*/
  0xEFBFA5,    /*￥*/
  0xEFBCBD,    /*］*/
  0xEFBCBE,    /*＾*/
  0xEFBCBF,    /*＿*/

  0xEFBD80,    /*｀*/
  0xEFBD81,    /*ａ*/
  0xEFBD82,    /*ｂ*/
  0xEFBD83,    /*ｃ*/
  0xEFBD84,    /*ｄ*/
  0xEFBD85,    /*ｅ*/
  0xEFBD86,    /*ｆ*/
  0xEFBD87,    /*ｇ*/
  0xEFBD88,    /*ｈ*/
  0xEFBD89,    /*ｉ*/
  0xEFBD8A,    /*ｊ*/
  0xEFBD8B,    /*ｋ*/
  0xEFBD8C,    /*ｌ*/
  0xEFBD8D,    /*ｍ*/
  0xEFBD8E,    /*ｎ*/
  0xEFBD8F,    /*ｏ*/

  0xEFBD90,    /*ｐ*/
  0xEFBD91,    /*ｑ*/
  0xEFBD92,    /*ｒ*/
  0xEFBD93,    /*ｓ*/
  0xEFBD94,    /*ｔ*/
  0xEFBD95,    /*ｕ*/
  0xEFBD96,    /*ｖ*/
  0xEFBD97,    /*ｗ*/
  0xEFBD98,    /*ｘ*/
  0xEFBD99,    /*ｙ*/
  0xEFBD9A,    /*ｚ*/
  0xEFBD9B,    /*｛*/
  0xEFBD9C,    /*｜*/
  0xEFBD9D,    /*｝*/
  0xEFBFA3,    /*￣*/
  0xE38080,    /*　*/

  0xEFBCBC,    /*＼*/
  0xE3809C,    /*～*/
  0xEFBD9C,    /*｜*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0xE38292,    /*を*/
  0xE38181,    /*ぁ*/
  0xE38183,    /*ぃ*/
  0xE38185,    /*ぅ*/
  0xE38187,    /*ぇ*/
  0xE38189,    /*ぉ*/
  0xE38283,    /*ゃ*/
  0xE38285,    /*ゅ*/
  0xE38287,    /*ょ*/
  0xE381A3,    /*っ*/

  0xE38080,    /*　*/
  0xE38182,    /*あ*/
  0xE38184,    /*い*/
  0xE38186,    /*う*/
  0xE38188,    /*え*/
  0xE3818A,    /*お*/
  0xE3818B,    /*か*/
  0xE3818D,    /*き*/
  0xE3818F,    /*く*/
  0xE38191,    /*け*/
  0xE38193,    /*こ*/
  0xE38195,    /*さ*/
  0xE38197,    /*し*/
  0xE38199,    /*す*/
  0xE3819B,    /*せ*/
  0xE3819D,    /*そ*/

  0xE38080,    /*　*/
  0xE38082,    /*。*/
  0xE3808C,    /*「*/
  0xE3808D,    /*」*/
  0xE38081,    /*、*/
  0xE383BB,    /*・*/
  0xE383B2,    /*ヲ*/
  0xE382A1,    /*ァ*/
  0xE382A3,    /*ィ*/
  0xE382A5,    /*ゥ*/
  0xE382A7,    /*ェ*/
  0xE382A9,    /*ォ*/
  0xE383A3,    /*ャ*/
  0xE383A5,    /*ュ*/
  0xE383A7,    /*ョ*/
  0xE38383,    /*ッ*/

  0xE383BC,    /*ー*/
  0xE382A2,    /*ア*/
  0xE382A4,    /*イ*/
  0xE382A6,    /*ウ*/
  0xE382A8,    /*エ*/
  0xE382AA,    /*オ*/
  0xE382AB,    /*カ*/
  0xE382AD,    /*キ*/
  0xE382AF,    /*ク*/
  0xE382B1,    /*ケ*/
  0xE382B3,    /*コ*/
  0xE382B5,    /*サ*/
  0xE382B7,    /*シ*/
  0xE382B9,    /*ス*/
  0xE382BB,    /*セ*/
  0xE382BD,    /*ソ*/

  0xE382BF,    /*タ*/
  0xE38381,    /*チ*/
  0xE38384,    /*ツ*/
  0xE38386,    /*テ*/
  0xE38388,    /*ト*/
  0xE3838A,    /*ナ*/
  0xE3838B,    /*ニ*/
  0xE3838C,    /*ヌ*/
  0xE3838D,    /*ネ*/
  0xE3838E,    /*ノ*/
  0xE3838F,    /*ハ*/
  0xE38392,    /*ヒ*/
  0xE38395,    /*フ*/
  0xE38398,    /*ヘ*/
  0xE3839B,    /*ホ*/
  0xE3839E,    /*マ*/

  0xE3839F,    /*ミ*/
  0xE383A0,    /*ム*/
  0xE383A1,    /*メ*/
  0xE383A2,    /*モ*/
  0xE383A4,    /*ヤ*/
  0xE383A6,    /*ユ*/
  0xE383A8,    /*ヨ*/
  0xE383A9,    /*ラ*/
  0xE383AA,    /*リ*/
  0xE383AB,    /*ル*/
  0xE383AC,    /*レ*/
  0xE383AD,    /*ロ*/
  0xE383AF,    /*ワ*/
  0xE383B3,    /*ン*/
  0xE3829B,    /*゛*/
  0xE3829C,    /*゜*/

  0xE3819F,    /*た*/
  0xE381A1,    /*ち*/
  0xE381A4,    /*つ*/
  0xE381A6,    /*て*/
  0xE381A8,    /*と*/
  0xE381AA,    /*な*/
  0xE381AB,    /*に*/
  0xE381AC,    /*ぬ*/
  0xE381AD,    /*ね*/
  0xE381AE,    /*の*/
  0xE381AF,    /*は*/
  0xE381B2,    /*ひ*/
  0xE381B5,    /*ふ*/
  0xE381B8,    /*へ*/
  0xE381BB,    /*ほ*/
  0xE381BE,    /*ま*/

  0xE381BF,    /*み*/
  0xE38280,    /*む*/
  0xE38281,    /*め*/
  0xE38282,    /*も*/
  0xE38284,    /*や*/
  0xE38286,    /*ゆ*/
  0xE38288,    /*よ*/
  0xE38289,    /*ら*/
  0xE3828A,    /*り*/
  0xE3828B,    /*る*/
  0xE3828C,    /*れ*/
  0xE3828D,    /*ろ*/
  0xE3828F,    /*わ*/
  0xE38293,    /*ん*/
  0xE38080,    /*　*/
  0xE38080,    /*　*/
  0x000000,    /*END*/
};

/*半角で8x16や12x24をきれいに描ける場合がある？*/
/* UTF8      字(half width)*/
static const uint8_t x68_ank_h[] = {
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */

  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */
  0x20,    /* */

  0x20,    /* */
  0x21,    /*!*/
  0x22,    /*"*/
  0x23,    /*#*/
  0x24,    /*$*/
  0x25,    /*%*/
  0x26,    /*&*/
  0x27,    /*'*/
  0x28,    /*(*/
  0x29,    /*)*/
  0x2a,    /***/
  0x2b,    /*+*/
  0x2c,    /*,*/
  0x2d,    /*-*/
  0x2e,    /*.*/
  0x2f,    /*／*/
  0x30,    /*0*/
  0x31,    /*1*/
  0x32,    /*2*/
  0x33,    /*3*/
  0x34,    /*4*/
  0x35,    /*5*/
  0x36,    /*6*/
  0x37,    /*7*/
  0x38,    /*8*/
  0x39,    /*9*/
  0x3a,    /*:*/
  0x3b,    /*;*/
  0x3c,    /*<*/
  0x3d,    /*=*/
  0x3e,    /*>*/
  0x3f,    /*?*/
  0x40,    /*@*/
  0x41,    /*A*/
  0x42,    /*B*/
  0x43,    /*C*/
  0x44,    /*D*/
  0x45,    /*E*/
  0x46,    /*F*/
  0x47,    /*G*/
  0x48,    /*H*/
  0x49,    /*I*/
  0x4a,    /*J*/
  0x4b,    /*K*/
  0x4c,    /*L*/
  0x4d,    /*M*/
  0x4e,    /*N*/
  0x4f,    /*O*/
  0x50,    /*P*/
  0x51,    /*Q*/
  0x52,    /*R*/
  0x53,    /*S*/
  0x54,    /*T*/
  0x55,    /*U*/
  0x56,    /*V*/
  0x57,    /*W*/
  0x58,    /*X*/
  0x59,    /*Y*/
  0x5a,    /*Z*/
  0x5b,    /*[*/
  0x5c,    /*¥*/
  0x5d,    /*]*/
  0x5e,    /*^*/
  0x5f,    /*_*/
  0x60,    /*`*/
  0x61,    /*a*/
  0x62,    /*b*/
  0x63,    /*c*/
  0x64,    /*d*/
  0x65,    /*e*/
  0x66,    /*f*/
  0x67,    /*g*/
  0x68,    /*h*/
  0x69,    /*i*/
  0x6a,    /*j*/
  0x6b,    /*k*/
  0x6c,    /*l*/
  0x6d,    /*m*/
  0x6e,    /*n*/
  0x6f,    /*o*/
  0x70,    /*p*/
  0x71,    /*q*/
  0x72,    /*r*/
  0x73,    /*s*/
  0x74,    /*t*/
  0x75,    /*u*/
  0x76,    /*v*/
  0x77,    /*w*/
  0x78,    /*x*/
  0x79,    /*y*/
  0x7a,    /*z*/
  0x7b,    /*{*/
  0x7c,    /*|*/
  0x7d,    /*}*/
  0x7e,    /*￣*/
  0x20,    /* */
  0x00,    /*END*/
};

/* UTF8        字(未サポート領域？)*/
static const uint32_t x68_ank_h2[] = {
  0x000020,  /* */
  0xEFBDA1,  /*｡*/
  0xEFBDA2,  /*｢*/
  0xEFBDA3,  /*｣*/
  0xEFBDA4,  /*､*/
  0xEFBDA5,  /*･*/
  0xEFBDA6,  /*ｦ*/
  0xEFBDA7,  /*ｧ*/
  0xEFBDA8,  /*ｨ*/
  0xEFBDA9,  /*ｩ*/
  0xEFBDAA,  /*ｪ*/
  0xEFBDAB,  /*ｫ*/
  0xEFBDAC,  /*ｬ*/
  0xEFBDAD,  /*ｭ*/
  0xEFBDAE,  /*ｮ*/
  0xEFBDAF,  /*ｯ*/
  0xEFBDB0,  /*ｰ*/
  0xEFBDB1,  /*ｱ*/
  0xEFBDB2,  /*ｲ*/
  0xEFBDB3,  /*ｳ*/
  0xEFBDB4,  /*ｴ*/
  0xEFBDB5,  /*ｵ*/
  0xEFBDB6,  /*ｶ*/
  0xEFBDB7,  /*ｷ*/
  0xEFBDB8,  /*ｸ*/
  0xEFBDB9,  /*ｹ*/
  0xEFBDBA,  /*ｺ*/
  0xEFBDBB,  /*ｻ*/
  0xEFBDBC,  /*ｼ*/
  0xEFBDBD,  /*ｽ*/
  0xEFBDBE,  /*ｾ*/
  0xEFBDBF,  /*ｿ*/
  0xEFBE80,  /*ﾀ*/
  0xEFBE81,  /*ﾁ*/
  0xEFBE82,  /*ﾂ*/
  0xEFBE83,  /*ﾃ*/
  0xEFBE84,  /*ﾄ*/
  0xEFBE85,  /*ﾅ*/
  0xEFBE86,  /*ﾆ*/
  0xEFBE87,  /*ﾇ*/
  0xEFBE88,  /*ﾈ*/
  0xEFBE89,  /*ﾉ*/
  0xEFBE8A,  /*ﾊ*/
  0xEFBE8B,  /*ﾋ*/
  0xEFBE8C,  /*ﾌ*/
  0xEFBE8D,  /*ﾍ*/
  0xEFBE8E,  /*ﾎ*/
  0xEFBE8F,  /*ﾏ*/
  0xEFBE90,  /*ﾐ*/
  0xEFBE91,  /*ﾑ*/
  0xEFBE92,  /*ﾒ*/
  0xEFBE93,  /*ﾓ*/
  0xEFBE94,  /*ﾔ*/
  0xEFBE95,  /*ﾕ*/
  0xEFBE96,  /*ﾖ*/
  0xEFBE97,  /*ﾗ*/
  0xEFBE98,  /*ﾘ*/
  0xEFBE99,  /*ﾙ*/
  0xEFBE9A,  /*ﾚ*/
  0xEFBE9B,  /*ﾛ*/
  0xEFBE9C,  /*ﾜ*/
  0xEFBE9D,  /*ﾝ*/
  0xEFBE9E,  /*ﾞ*/
  0xEFBE9F,  /*ﾟ*/
  0x000000,  /*END*/
};