/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2014, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#ifndef PHP_HTTP_UTF8_H
#define PHP_HTTP_UTF8_H

typedef struct utf8_range {
	unsigned int start;
	unsigned int end;
	unsigned char step;
} utf8_range_t;

static const unsigned char utf8_mblen[256] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    4,4,4,4,4,4,4,4,5,5,5,5,6,6,6,6
};

static const unsigned char utf8_mask[] = {
		0, 0x7f, 0x1f, 0x0f, 0x07, 0x03, 0x01
};

static const utf8_range_t utf8_ranges[] = {
/* BEGIN::UTF8TABLE */
/* BASIC LATIN */
	{    0x0041,     0x005A, 1},
	{    0x0061,     0x007A, 1},
/* LATIN-1 SUPPLEMENT */
	{    0x00AA,          0, 0},
	{    0x00B5,          0, 0},
	{    0x00BA,          0, 0},
	{    0x00C0,     0x00D6, 1},
	{    0x00D8,     0x00F6, 1},
	{    0x00F8,     0x00FF, 1},
/* LATIN EXTENDED-A */
	{    0x0100,     0x017F, 1},
/* LATIN EXTENDED-B */
	{    0x0180,     0x024F, 1},
/* IPA EXTENSIONS */
	{    0x0250,     0x02AF, 1},
/* SPACING MODIFIER LETTERS */
	{    0x02B0,     0x02C1, 1},
	{    0x02C6,     0x02D1, 1},
	{    0x02E0,     0x02E4, 1},
	{    0x02EE,          0, 0},
/* COMBINING DIACRITICAL MARKS */
	{    0x0345,          0, 0},
/* BASIC GREEK */
	{    0x0370,     0x0373, 1},
	{    0x0376,     0x0377, 1},
	{    0x037A,     0x037D, 1},
	{    0x0386,          0, 0},
	{    0x0388,     0x038A, 1},
	{    0x038C,          0, 0},
	{    0x038E,     0x03A1, 1},
	{    0x03A3,     0x03CE, 1},
/* GREEK SYMBOLS AND COPTIC */
	{    0x03D0,     0x03F5, 1},
	{    0x03F7,     0x03FF, 1},
/* CYRILLIC */
	{    0x0400,     0x0481, 1},
	{    0x048A,     0x04FF, 1},
/* CYRILLIC SUPPLEMENT */
	{    0x0500,     0x0523, 1},
/* ARMENIAN */
	{    0x0531,     0x0556, 1},
	{    0x0559,          0, 0},
	{    0x0561,     0x0587, 1},
/* HEBREW */
	{    0x05D0,     0x05EA, 1},
	{    0x05F0,     0x05F2, 1},
/* ARABIC */
	{    0x0621,     0x064A, 1},
	{    0x066E,     0x066F, 1},
	{    0x0671,     0x06D3, 1},
	{    0x06D5,          0, 0},
	{    0x06E5,     0x06E6, 1},
	{    0x06EE,     0x06EF, 1},
	{    0x06FA,     0x06FC, 1},
	{    0x06FF,          0, 0},
/* SYRIAC */
	{    0x0710,          0, 0},
	{    0x0712,     0x072F, 1},
	{    0x074D,     0x074F, 1},
/* ARABIC SUPPLEMENT */
	{    0x0750,     0x077F, 1},
/* THAANA */
	{    0x0780,     0x07A5, 1},
	{    0x07B1,          0, 0},
/* NKO */
	{    0x07C0,     0x07EA, 1},
	{    0x07F4,     0x07F5, 1},
	{    0x07FA,          0, 0},
/* - All Matras of Indic and Sinhala are moved from punct to alpha class */
/* - Added Unicode 5.1 charctares of Indic scripts */
/* DEVANAGARI */
	{    0x0901,     0x0939, 1},
	{    0x093C,     0x094D, 1},
	{    0x0950,     0x0954, 1},
	{    0x0958,     0x0961, 1},
	{    0x0962,          0, 0},
	{    0x0963,          0, 0},
	{    0x0972,          0, 0},
	{    0x097B,     0x097F, 1},
/* TABLE 18 BENGALI */
	{    0x0981,     0x0983, 1},
	{    0x0985,     0x098C, 1},
	{    0x098F,          0, 0},
	{    0x0990,          0, 0},
	{    0x0993,     0x09A8, 1},
	{    0x09AA,     0x09B0, 1},
	{    0x09B2,          0, 0},
	{    0x09B6,     0x09B9, 1},
	{    0x09BC,     0x09C4, 1},
	{    0x09C7,          0, 0},
	{    0x09C8,          0, 0},
	{    0x09CB,     0x09CE, 1},
	{    0x09D7,          0, 0},
	{    0x09DC,          0, 0},
	{    0x09DD,          0, 0},
	{    0x09DF,     0x09E3, 1},
	{    0x09F0,     0x09FA, 1},
/* GURMUKHI */
	{    0x0A01,     0x0A03, 1},
	{    0x0A05,     0x0A0A, 1},
	{    0x0A0F,          0, 0},
	{    0x0A10,          0, 0},
	{    0x0A13,     0x0A28, 1},
	{    0x0A2A,     0x0A30, 1},
	{    0x0A32,          0, 0},
	{    0x0A33,          0, 0},
	{    0x0A35,          0, 0},
	{    0x0A36,          0, 0},
	{    0x0A38,          0, 0},
	{    0x0A39,          0, 0},
	{    0x0A3C,          0, 0},
	{    0x0A3E,     0x0A42, 1},
	{    0x0A47,          0, 0},
	{    0x0A48,          0, 0},
	{    0x0A4B,     0x0A4D, 1},
	{    0x0A51,          0, 0},
	{    0x0A59,     0x0A5C, 1},
	{    0x0A5E,          0, 0},
	{    0x0A70,     0x0A75, 1},
/* GUJARATI */
	{    0x0A81,     0x0A83, 1},
	{    0x0A85,     0x0A8D, 1},
	{    0x0A8F,     0x0A91, 1},
	{    0x0A93,     0x0AA8, 1},
	{    0x0AAA,     0x0AB0, 1},
	{    0x0AB2,          0, 0},
	{    0x0AB3,          0, 0},
	{    0x0AB5,     0x0AB9, 1},
	{    0x0ABC,     0x0AC5, 1},
	{    0x0AC7,     0x0AC9, 1},
	{    0x0ACB,     0x0ACD, 1},
	{    0x0AD0,          0, 0},
	{    0x0AE0,     0x0AE3, 1},
	{    0x0AF1,          0, 0},
/* ORIYA */
	{    0x0B01,     0x0B03, 1},
	{    0x0B05,     0x0B0C, 1},
	{    0x0B0F,          0, 0},
	{    0x0B10,          0, 0},
	{    0x0B13,     0x0B28, 1},
	{    0x0B2A,     0x0B30, 1},
	{    0x0B32,          0, 0},
	{    0x0B33,          0, 0},
	{    0x0B35,     0x0B39, 1},
	{    0x0B3C,     0x0B44, 1},
	{    0x0B47,     0x0B48, 1},
	{    0x0B4B,     0x0B4D, 1},
	{    0x0B56,     0x0B57, 1},
	{    0x0B5C,          0, 0},
	{    0x0B5D,          0, 0},
	{    0x0B5F,     0x0B63, 1},
	{    0x0B70,          0, 0},
	{    0x0B71,          0, 0},
/* TAMIL */
	{    0x0B82,          0, 0},
	{    0x0B83,          0, 0},
	{    0x0B85,     0x0B8A, 1},
	{    0x0B8E,     0x0B90, 1},
	{    0x0B92,     0x0B95, 1},
	{    0x0B99,          0, 0},
	{    0x0B9A,          0, 0},
	{    0x0B9C,          0, 0},
	{    0x0B9E,          0, 0},
	{    0x0B9F,          0, 0},
	{    0x0BA3,          0, 0},
	{    0x0BA4,          0, 0},
	{    0x0BA8,     0x0BAA, 1},
	{    0x0BAE,     0x0BB9, 1},
	{    0x0BBE,     0x0BC2, 1},
	{    0x0BC6,     0x0BC8, 1},
	{    0x0BCA,     0x0BCD, 1},
	{    0x0BD0,          0, 0},
	{    0x0BD7,          0, 0},
	{    0x0BF0,     0x0BFA, 1},
/* TELUGU */
	{    0x0C01,     0x0C03, 1},
	{    0x0C05,     0x0C0C, 1},
	{    0x0C0E,     0x0C10, 1},
	{    0x0C12,     0x0C28, 1},
	{    0x0C2A,     0x0C33, 1},
	{    0x0C35,     0x0C39, 1},
	{    0x0C3D,     0x0C44, 1},
	{    0x0C46,     0x0C48, 1},
	{    0x0C4A,     0x0C4D, 1},
	{    0x0C55,     0x0C56, 1},
	{    0x0C58,     0x0C59, 1},
	{    0x0C60,     0x0C63, 1},
/* KANNADA */
	{    0x0C82,     0x0C83, 1},
	{    0x0C85,     0x0C8C, 1},
	{    0x0C8E,     0x0C90, 1},
	{    0x0C92,     0x0CA8, 1},
	{    0x0CAA,     0x0CB3, 1},
	{    0x0CB5,     0x0CB9, 1},
	{    0x0CBC,     0x0CC4, 1},
	{    0x0CC6,     0x0CC8, 1},
	{    0x0CCA,     0x0CCD, 1},
	{    0x0CD5,     0x0CD6, 1},
	{    0x0CDE,          0, 0},
	{    0x0CE0,     0x0CE3, 1},
	{    0x0CF1,          0, 0},
	{    0x0CF2,          0, 0},
/* MALAYALAM */
	{    0x0D02,     0x0D03, 1},
	{    0x0D05,     0x0D0C, 1},
	{    0x0D0E,     0x0D10, 1},
	{    0x0D12,     0x0D28, 1},
	{    0x0D2A,     0x0D39, 1},
	{    0x0D3D,     0x0D44, 1},
	{    0x0D46,     0x0D48, 1},
	{    0x0D4A,     0x0D4D, 1},
	{    0x0D57,          0, 0},
	{    0x0D60,     0x0D63, 1},
	{    0x0D79,     0x0D7F, 1},
/* SINHALA */
	{    0x0D82,     0x0D83, 1},
	{    0x0D85,     0x0D96, 1},
	{    0x0D9A,     0x0DB1, 1},
	{    0x0DB3,     0x0DBB, 1},
	{    0x0DBD,          0, 0},
	{    0x0DC0,     0x0DC6, 1},
	{    0x0DCA,          0, 0},
	{    0x0DCF,     0x0DD4, 1},
	{    0x0DD6,          0, 0},
	{    0x0DD8,     0x0DDF, 1},
	{    0x0DF2,     0x0DF4, 1},
/* THAI */
	{    0x0E01,     0x0E2E, 1},
	{    0x0E30,     0x0E3A, 1},
	{    0x0E40,     0x0E45, 1},
	{    0x0E47,     0x0E4E, 1},
/* LAO */
	{    0x0E81,     0x0E82, 1},
	{    0x0E84,          0, 0},
	{    0x0E87,     0x0E88, 1},
	{    0x0E8A,          0, 0},
	{    0x0E8D,          0, 0},
	{    0x0E94,     0x0E97, 1},
	{    0x0E99,     0x0E9F, 1},
	{    0x0EA1,     0x0EA3, 1},
	{    0x0EA5,          0, 0},
	{    0x0EA7,          0, 0},
	{    0x0EAA,     0x0EAB, 1},
	{    0x0EAD,     0x0EB0, 1},
	{    0x0EB2,     0x0EB3, 1},
	{    0x0EBD,          0, 0},
	{    0x0EC0,     0x0EC4, 1},
	{    0x0EC6,          0, 0},
	{    0x0EDC,     0x0EDD, 1},
/* TIBETAN */
	{    0x0F00,          0, 0},
	{    0x0F40,     0x0F47, 1},
	{    0x0F49,     0x0F6C, 1},
	{    0x0F88,     0x0F8B, 1},
/* MYANMAR */
	{    0x1000,     0x102A, 1},
	{    0x1050,     0x1055, 1},
	{    0x105A,     0x105D, 1},
	{    0x1061,          0, 0},
	{    0x0165,          0, 0},
	{    0x1066,          0, 0},
	{    0x106E,     0x1070, 1},
	{    0x1075,     0x1081, 1},
	{    0x108E,          0, 0},
/* GEORGIAN */
	{    0x10A0,     0x10C5, 1},
	{    0x10D0,     0x10FA, 1},
	{    0x10FC,          0, 0},
/* HANGUL JAMO */
	{    0x1100,     0x1159, 1},
	{    0x115F,     0x11A2, 1},
	{    0x11A8,     0x11F9, 1},
/* ETHIOPIC */
	{    0x1200,     0x1248, 1},
	{    0x124A,     0x124D, 1},
	{    0x1250,     0x1256, 1},
	{    0x1258,          0, 0},
	{    0x125A,     0x125D, 1},
	{    0x1260,     0x1288, 1},
	{    0x128A,     0x128D, 1},
	{    0x1290,     0x12B0, 1},
	{    0x12B2,     0x12B5, 1},
	{    0x12B8,     0x12BE, 1},
	{    0x12C0,          0, 0},
	{    0x12C2,     0x12C5, 1},
	{    0x12C8,     0x12D6, 1},
	{    0x12D8,     0x1310, 1},
	{    0x1312,     0x1315, 1},
	{    0x1318,     0x135A, 1},
/* ETHIOPIC EXTENDED */
	{    0x1380,     0x138F, 1},
/* CHEROKEE */
	{    0x13A0,     0x13F4, 1},
/* UNIFIED CANADIAN ABORIGINAL SYLLABICS */
	{    0x1401,     0x166C, 1},
	{    0x166F,     0x1676, 1},
/* OGHAM */
	{    0x1681,     0x169A, 1},
/* RUNIC */
	{    0x16A0,     0x16EA, 1},
	{    0x16EE,     0x16F0, 1},
/* TAGALOG */
	{    0x1700,     0x170C, 1},
	{    0x170E,     0x1711, 1},
/* HANUNOO */
	{    0x1720,     0x1731, 1},
/* BUHID */
	{    0x1740,     0x1751, 1},
/* TAGBANWA */
	{    0x1760,     0x176C, 1},
	{    0x176E,     0x1770, 1},
/* KHMER */
	{    0x1780,     0x17B3, 1},
	{    0x17D7,          0, 0},
	{    0x17DC,          0, 0},
/* MONGOLIAN */
	{    0x1820,     0x1877, 1},
	{    0x1880,     0x18A8, 1},
	{    0x18AA,          0, 0},
/* LIMBU */
	{    0x1900,     0x191C, 1},
	{    0x1946,     0x194F, 1},
/* TAI LE */
	{    0x1950,     0x196D, 1},
	{    0x1970,     0x1974, 1},
/* NEW TAI LUE */
	{    0x1980,     0x19A9, 1},
	{    0x19C1,     0x19C7, 1},
	{    0x19D0,     0x19D9, 1},
/* BUGINESE */
	{    0x1A00,     0x1A16, 1},
/* BALINESE */
	{    0x1B05,     0x1B33, 1},
	{    0x1B45,     0x1B4B, 1},
	{    0x1B50,     0x1B59, 1},
/* SUNDANESE */
	{    0x1B83,     0x1BA0, 1},
	{    0x1BAE,     0x1BAF, 1},
/* LEPCHA */
	{    0x1C00,     0x1C23, 1},
	{    0x1C4D,     0x1C4F, 1},
/* OL CHIKI */
	{    0x1C5A,     0x1C7D, 1},
/* PHONETIC EXTENSIONS */
	{    0x1D00,     0x1DBF, 1},
/* LATIN EXTENDED ADDITIONAL */
	{    0x1E00,     0x1E9F, 1},
	{    0x1EA0,     0x1EFF, 1},
/* GREEK EXTENDED */
	{    0x1F00,     0x1F15, 1},
	{    0x1F18,     0x1F1D, 1},
	{    0x1F20,     0x1F45, 1},
	{    0x1F48,     0x1F4D, 1},
	{    0x1F50,     0x1F57, 1},
	{    0x1F59,          0, 0},
	{    0x1F5B,          0, 0},
	{    0x1F5D,          0, 0},
	{    0x1F5F,     0x1F7D, 1},
	{    0x1F80,     0x1FB4, 1},
	{    0x1FB6,     0x1FBC, 1},
	{    0x1FBE,          0, 0},
	{    0x1FC2,     0x1FC4, 1},
	{    0x1FC6,     0x1FCC, 1},
	{    0x1FD0,     0x1FD3, 1},
	{    0x1FD6,     0x1FDB, 1},
	{    0x1FE0,     0x1FEC, 1},
	{    0x1FF2,     0x1FF4, 1},
	{    0x1FF6,     0x1FFC, 1},
/* SUPERSCRIPTS AND SUBSCRIPTS */
	{    0x2071,          0, 0},
	{    0x207F,          0, 0},
	{    0x2090,     0x2094, 1},
/* LETTERLIKE SYMBOLS */
	{    0x2102,          0, 0},
	{    0x2107,          0, 0},
	{    0x210A,     0x2113, 1},
	{    0x2115,          0, 0},
	{    0x2119,     0x211D, 1},
	{    0x2124,          0, 0},
	{    0x2126,          0, 0},
	{    0x2128,     0x212D, 1},
	{    0x212F,     0x2139, 1},
	{    0x213C,     0x213F, 1},
	{    0x2145,     0x2149, 1},
	{    0x214E,          0, 0},
/* NUMBER FORMS */
	{    0x2160,     0x2188, 1},
/* ENCLOSED ALPHANUMERICS */
	{    0x249C,     0x24E9, 1},
/* GLAGOLITIC */
	{    0x2C00,     0x2C2E, 1},
	{    0x2C30,     0x2C5E, 1},
/* LATIN EXTENDED-C */
	{    0x2C60,     0x2C6F, 1},
	{    0x2C71,     0x2C7D, 1},
/* COPTIC */
	{    0x2C80,     0x2CE4, 1},
/* GEORGIAN SUPPLEMENT */
	{    0x2D00,     0x2D25, 1},
/* TIFINAGH */
	{    0x2D30,     0x2D65, 1},
	{    0x2D6F,          0, 0},
/* ETHIOPIC EXTENDED */
	{    0x2D80,     0x2D96, 1},
	{    0x2DA0,     0x2DA6, 1},
	{    0x2DA8,     0x2DAE, 1},
	{    0x2DB0,     0x2DB6, 1},
	{    0x2DB8,     0x2DBE, 1},
	{    0x2DC0,     0x2DC6, 1},
	{    0x2DC8,     0x2DCE, 1},
	{    0x2DD0,     0x2DD6, 1},
	{    0x2DD8,     0x2DDE, 1},
/* CJK SYMBOLS AND PUNCTUATION */
	{    0x3005,     0x3007, 1},
	{    0x3021,     0x3029, 1},
	{    0x3031,     0x3035, 1},
	{    0x3038,     0x303C, 1},
/* HIRAGANA */
	{    0x3041,     0x3096, 1},
	{    0x309D,     0x309F, 1},
/* KATAKANA */
	{    0x30A1,     0x30FA, 1},
	{    0x30FC,     0x30FF, 1},
/* BOPOMOFO */
	{    0x3105,     0x312D, 1},
/* HANGUL COMPATIBILITY JAMO */
	{    0x3131,     0x318E, 1},
/* BOPOMOFO EXTENDED */
	{    0x31A0,     0x31B7, 1},
/* KATAKANA PHONETIC EXTENSIONS */
	{    0x31F0,     0x31FF, 1},
/* CJK UNIFIED IDEOGRAPHS EXTENSION */
	{    0x3400,     0x4DB5, 1},
/* CJK UNIFIED IDEOGRAPHS */
	{    0x4E00,     0x9FBB, 1},
/* YI SYLLABLES */
	{    0xA000,     0xA48C, 1},
/* VAI SYLLABLES */
	{    0xA500,     0xA60B, 1},
	{    0xA610,     0xA61F, 1},
	{    0xA62A,     0xA62B, 1},
/* CYRILLIC SUPPLEMENT 2 */
	{    0xA640,     0xA65F, 1},
	{    0xA662,     0xA66E, 1},
	{    0xA680,     0xA697, 1},
/* LATIN EXTENDED-D */
	{    0xA717,     0xA71F, 1},
	{    0xA722,     0xA78C, 1},
	{    0xA7FB,     0xA7FF, 1},
/* SYLOTI NEGRI */
	{    0xA800,          0, 0},
	{    0xA801,          0, 0},
	{    0xA803,     0xA805, 1},
	{    0xA807,     0xA80A, 1},
	{    0xA80C,     0xA822, 1},
/* PHAGS PA */
	{    0xA840,     0xA873, 1},
/* SAURASHTRA */
	{    0xA882,     0xA8B3, 1},
/* KAYAH LI */
	{    0xA90A,     0xA92D, 1},
/* REJANG */
	{    0xA930,     0xA946, 1},
/* CHAM */
	{    0xAA00,     0xAA28, 1},
	{    0xAA40,     0xAA42, 1},
	{    0xAA44,     0xAA4B, 1},
/* HANGUL SYLLABLES */
	{    0xAC00,     0xD7A3, 1},
/* CJK COMPATIBILITY IDEOGRAPHS */
	{    0xF900,     0xFA2D, 1},
	{    0xFA30,     0xFA6A, 1},
	{    0xFA70,     0xFAD9, 1},
/* ALPHABETIC PRESENTATION FORMS */
	{    0xFB00,     0xFB06, 1},
	{    0xFB13,     0xFB17, 1},
	{    0xFB1D,          0, 0},
	{    0xFB1F,     0xFB28, 1},
	{    0xFB2A,     0xFB36, 1},
	{    0xFB38,     0xFB3C, 1},
	{    0xFB3E,          0, 0},
	{    0xFB40,          0, 0},
	{    0xFB41,          0, 0},
	{    0xFB43,          0, 0},
	{    0xFB44,          0, 0},
	{    0xFB46,     0xFB4F, 1},
/* ARABIC PRESENTATION FORMS-A */
	{    0xFB50,     0xFBB1, 1},
	{    0xFBD3,     0xFD3D, 1},
	{    0xFD50,     0xFD8F, 1},
	{    0xFD92,     0xFDC7, 1},
	{    0xFDF0,     0xFDFB, 1},
/* ARABIC PRESENTATION FORMS-B */
	{    0xFE70,     0xFE74, 1},
	{    0xFE76,     0xFEFC, 1},
/* HALFWIDTH AND FULLWIDTH FORMS */
	{    0xFF21,     0xFF3A, 1},
	{    0xFF41,     0xFF5A, 1},
	{    0xFF66,     0xFFBE, 1},
	{    0xFFC2,     0xFFC7, 1},
	{    0xFFCA,     0xFFCF, 1},
	{    0xFFD2,     0xFFD7, 1},
	{    0xFFDA,     0xFFDC, 1},
/* LINEAR B SYLLABARY */
	{0x00010000, 0x0001000B, 1},
	{0x0001000D, 0x00010026, 1},
	{0x00010028, 0x0001003A, 1},
	{0x0001003C, 0x0001003D, 1},
	{0x0001003F, 0x0001004D, 1},
	{0x00010050, 0x0001005D, 1},
/* LINEAR B IDEOGRAMS */
	{0x00010080, 0x000100FA, 1},
/* ANCIENT GREEK NUMBERS */
	{0x00010140, 0x00010174, 1},
/* LYCIAN */
	{0x00010280, 0x0001029C, 1},
/* CARIAN */
	{0x000102A0, 0x000102D0, 1},
/* OLD ITALIC */
	{0x00010300, 0x0001031E, 1},
/* GOTHIC */
	{0x00010330, 0x0001034A, 1},
/* UGARITIC */
	{0x00010380, 0x0001039D, 1},
/* OLD PERSIAN */
	{0x000103A0, 0x000103C3, 1},
	{0x000103C8, 0x000103CF, 1},
	{0x000103D1, 0x000103D5, 1},
/* DESERET */
	{0x00010400, 0x0001044F, 1},
/* SHAVIAN */
	{0x00010450, 0x0001047F, 1},
/* OSMANYA */
	{0x00010480, 0x0001049D, 1},
	{0x000104A0, 0x000104A9, 1},
/* CYPRIOT SYLLABARY */
	{0x00010800, 0x00010805, 1},
	{0x00010808,          0, 0},
	{0x0001080A, 0x00010835, 1},
	{0x00010837, 0x00010838, 1},
	{0x0001083C,          0, 0},
	{0x0001083F,          0, 0},
/* PHOENICIAN */
	{0x00010900, 0x00010915, 1},
	{0x00010A00,          0, 0},
	{0x00010A10, 0x00010A13, 1},
/* KHAROSHTI */
	{0x00010A15, 0x00010A17, 1},
	{0x00010A19, 0x00010A33, 1},
/* CUNEIFORM */
	{0x00012000, 0x0001236E, 1},
/* CUNEIFORM NUMBERS AND PONCTUATION */
	{0x00012400, 0x00012462, 1},
/* BYZANTINE MUSICAL SYMBOLS */
/* MATHEMATICAL ALPHANUMERIC SYMBOLS */
	{0x0001D400, 0x0001D454, 1},
	{0x0001D456, 0x0001D49C, 1},
	{0x0001D49E, 0x0001D49F, 1},
	{0x0001D4A2,          0, 0},
	{0x0001D4A5, 0x0001D4A6, 1},
	{0x0001D4A9, 0x0001D4AC, 1},
	{0x0001D4AE, 0x0001D4B9, 1},
	{0x0001D4BB,          0, 0},
	{0x0001D4BD, 0x0001D4C3, 1},
	{0x0001D4C5, 0x0001D505, 1},
	{0x0001D507, 0x0001D50A, 1},
	{0x0001D50D, 0x0001D514, 1},
	{0x0001D516, 0x0001D51C, 1},
	{0x0001D51E, 0x0001D539, 1},
	{0x0001D53B, 0x0001D53E, 1},
	{0x0001D540, 0x0001D544, 1},
	{0x0001D546,          0, 0},
	{0x0001D54A, 0x0001D550, 1},
	{0x0001D552, 0x0001D6A5, 1},
	{0x0001D6A8, 0x0001D6C0, 1},
	{0x0001D6C2, 0x0001D6DA, 1},
	{0x0001D6DC, 0x0001D6FA, 1},
	{0x0001D6FC, 0x0001D714, 1},
	{0x0001D716, 0x0001D734, 1},
	{0x0001D736, 0x0001D74E, 1},
	{0x0001D750, 0x0001D76E, 1},
	{0x0001D770, 0x0001D788, 1},
	{0x0001D78A, 0x0001D7A8, 1},
	{0x0001D7AA, 0x0001D7C2, 1},
	{0x0001D7C4, 0x0001D7CB, 1},
	{0x0001D7CE, 0x0001D7FF, 1},
/* CJK UNIFIED IDEOGRAPHS EXTENSION */
	{0x00020000, 0x0002A6D6, 1},
/* CJK COMPATIBILITY IDEOGRAPHS SUPPLEMENT */
	{0x0002F800, 0x0002FA1D, 1},
/* The non-ASCII number characters are included here because ISO C 99 */
/* forbids us to classify them as digits; however, they behave more like */
/* alphanumeric than like punctuation. */
/* ARABIC */
	{    0x0660,     0x0669, 1},
	{    0x06F0,     0x06F9, 1},
/* DEVANAGARI */
	{    0x0966,     0x096F, 1},
/* BENGALI */
	{    0x09E6,     0x09EF, 1},
/* GURMUKHI */
	{    0x0A66,     0x0A6F, 1},
/* GUJARATI */
	{    0x0AE6,     0x0AEF, 1},
/* ORIYA */
	{    0x0B66,     0x0B6F, 1},
/* TAMIL */
	{    0x0BE6,     0x0BEF, 1},
/* TELUGU */
	{    0x0C66,     0x0C6F, 1},
	{    0x0C78,     0x0C7F, 1},
/* KANNADA */
	{    0x0CE6,     0x0CEF, 1},
/* MALAYALAM */
	{    0x0D66,     0x0D75, 1},
	{    0x0D70,     0x0D75, 1},
/* THAI */
	{    0x0E50,     0x0E59, 1},
/* LAO */
	{    0x0ED0,     0x0ED9, 1},
/* TIBETAN */
	{    0x0F20,     0x0F29, 1},
/* MYANMAR */
	{    0x1040,     0x1049, 1},
/* KHMER */
	{    0x17E0,     0x17E9, 1},
/* MONGOLIAN */
	{    0x1810,     0x1819, 1},
/* SUNDANESE */
	{    0x1BB0,     0x1BB9, 1},
/* LEPCHA */
	{    0x1C40,     0x1C49, 1},
/* OL CHIKI */
	{    0x1C50,     0x1C59, 1},
/* VAI */
	{    0xA620,     0xA629, 1},
/* SAURASHTRA */
	{    0xA8D0,     0xA8D9, 1},
/* KAYAH LI */
	{    0xA900,     0xA909, 1},
/* CHAM */
	{    0xAA50,     0xAA59, 1},
/* HALFWIDTH AND FULLWIDTH FORMS */
	{    0xFF10,     0xFF19, 1},

/* END::UTF8TABLE */
};

static inline size_t utf8towc(unsigned *wc, const unsigned char *uc, size_t len)
{
	unsigned char ub = utf8_mblen[*uc];

	if (!ub || ub > len || ub > 3) {
		return 0;
	}

	*wc = *uc & utf8_mask[ub];

	switch (ub) {
	case 4:
		if ((uc[1] & 0xc0) != 0x80) {
			return 0;
		}
		*wc <<= 6;
		*wc += *++uc & 0x3f;
		/* no break */
	case 3:
		if ((uc[1] & 0xc0) != 0x80) {
			return 0;
		}
		*wc <<= 6;
		*wc += *++uc & 0x3f;
		/* no break */
	case 2:
		if ((uc[1] & 0xc0) != 0x80) {
			return 0;
		}
		*wc <<= 6;
		*wc += *++uc & 0x3f;
		/* no break */
	case 1:
		break;

	default:
		return 0;
	}

	return ub;
}

static inline zend_bool isualpha(unsigned ch)
{
	unsigned i, j;

	for (i = 0; i < sizeof(utf8_ranges)/sizeof(utf8_range_t); ++i) {
		if (utf8_ranges[i].start == ch) {
			return 1;
		} else if (utf8_ranges[i].start <= ch && utf8_ranges[i].end >= ch) {
			if (utf8_ranges[i].step == 1) {
				return 1;
			}
			for (j = utf8_ranges[i].start; j <= utf8_ranges[i].end; j+= utf8_ranges[i].step) {
				if (ch == j) {
					return 1;
				}
			}
			return 0;
		}
	}
	return 0;
}

static inline zend_bool isualnum(unsigned ch)
{
	/* digits */
	if (ch >= 0x30 && ch <= 0x39) {
		return 1;
	}
	return isualpha(ch);
}

#endif	/* PHP_HTTP_UTF8_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
