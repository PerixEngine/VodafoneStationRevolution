/* ANSI-C code produced by gperf version 3.0.3 */
/* Command-line: gperf -m 10 lib/aliases.gperf  */
/* Computed positions: -k'1,3-11,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 1 "lib/aliases.gperf"
struct alias { int name; unsigned int encoding_index; };

#define TOTAL_KEYWORDS 340
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 45
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 1047
/* maximum key range = 1044, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
aliases_hash (register const char *str, register unsigned int len)
{
  static const unsigned short asso_values[] =
    {
      1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048,
      1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048,
      1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048,
      1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048,
      1048, 1048, 1048, 1048, 1048,    0,  191, 1048,   44,    2,
        16,   28,    9,   13,    5,   47,   20,    0,  129, 1048,
      1048, 1048, 1048, 1048, 1048,    1,  169,    1,    6,   86,
        53,  101,   58,   88,  365,  230,   43,  208,    2,    0,
        36, 1048,    0,   15,  109,  212,  212,  201,  149,   19,
         0, 1048, 1048, 1048, 1048,    1, 1048, 1048, 1048, 1048,
      1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048,
      1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048,
      1048, 1048, 1048, 1048, 1048, 1048, 1048, 1048
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[10]];
      /*FALLTHROUGH*/
      case 10:
        hval += asso_values[(unsigned char)str[9]];
      /*FALLTHROUGH*/
      case 9:
        hval += asso_values[(unsigned char)str[8]];
      /*FALLTHROUGH*/
      case 8:
        hval += asso_values[(unsigned char)str[7]];
      /*FALLTHROUGH*/
      case 7:
        hval += asso_values[(unsigned char)str[6]];
      /*FALLTHROUGH*/
      case 6:
        hval += asso_values[(unsigned char)str[5]];
      /*FALLTHROUGH*/
      case 5:
        hval += asso_values[(unsigned char)str[4]];
      /*FALLTHROUGH*/
      case 4:
        hval += asso_values[(unsigned char)str[3]];
      /*FALLTHROUGH*/
      case 3:
        hval += asso_values[(unsigned char)str[2]];
      /*FALLTHROUGH*/
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

struct stringpool_t
  {
    char stringpool_str4[sizeof("C99")];
    char stringpool_str5[sizeof("CN")];
    char stringpool_str6[sizeof("CHAR")];
    char stringpool_str15[sizeof("CP949")];
    char stringpool_str22[sizeof("R8")];
    char stringpool_str28[sizeof("CP819")];
    char stringpool_str33[sizeof("866")];
    char stringpool_str39[sizeof("CP154")];
    char stringpool_str41[sizeof("CP866")];
    char stringpool_str42[sizeof("CP1251")];
    char stringpool_str44[sizeof("CP936")];
    char stringpool_str46[sizeof("CP1361")];
    char stringpool_str47[sizeof("L1")];
    char stringpool_str48[sizeof("CP1256")];
    char stringpool_str50[sizeof("L6")];
    char stringpool_str54[sizeof("L4")];
    char stringpool_str55[sizeof("862")];
    char stringpool_str56[sizeof("CP1254")];
    char stringpool_str58[sizeof("L5")];
    char stringpool_str60[sizeof("HZ")];
    char stringpool_str61[sizeof("L2")];
    char stringpool_str63[sizeof("CP862")];
    char stringpool_str64[sizeof("CP1255")];
    char stringpool_str65[sizeof("L8")];
    char stringpool_str66[sizeof("CP932")];
    char stringpool_str70[sizeof("CP1252")];
    char stringpool_str73[sizeof("L3")];
    char stringpool_str74[sizeof("PT154")];
    char stringpool_str78[sizeof("CP1258")];
    char stringpool_str91[sizeof("CP874")];
    char stringpool_str92[sizeof("L7")];
    char stringpool_str94[sizeof("CP1253")];
    char stringpool_str95[sizeof("CP1133")];
    char stringpool_str97[sizeof("EUCCN")];
    char stringpool_str98[sizeof("EUC-CN")];
    char stringpool_str107[sizeof("CP950")];
    char stringpool_str111[sizeof("850")];
    char stringpool_str113[sizeof("PTCP154")];
    char stringpool_str121[sizeof("ISO646-CN")];
    char stringpool_str126[sizeof("CP1250")];
    char stringpool_str127[sizeof("CP850")];
    char stringpool_str132[sizeof("CP1257")];
    char stringpool_str133[sizeof("CP367")];
    char stringpool_str134[sizeof("L10")];
    char stringpool_str137[sizeof("SJIS")];
    char stringpool_str150[sizeof("ISO8859-9")];
    char stringpool_str151[sizeof("ISO-8859-9")];
    char stringpool_str152[sizeof("ISO_8859-9")];
    char stringpool_str154[sizeof("ISO8859-1")];
    char stringpool_str155[sizeof("ISO-8859-1")];
    char stringpool_str156[sizeof("ISO_8859-1")];
    char stringpool_str157[sizeof("ISO8859-11")];
    char stringpool_str158[sizeof("ISO-8859-11")];
    char stringpool_str159[sizeof("ISO_8859-11")];
    char stringpool_str160[sizeof("ISO8859-6")];
    char stringpool_str161[sizeof("ISO-8859-6")];
    char stringpool_str162[sizeof("ISO_8859-6")];
    char stringpool_str163[sizeof("ISO8859-16")];
    char stringpool_str164[sizeof("ISO-8859-16")];
    char stringpool_str165[sizeof("ISO_8859-16")];
    char stringpool_str167[sizeof("ISO_8859-16:2001")];
    char stringpool_str168[sizeof("ISO8859-4")];
    char stringpool_str169[sizeof("ISO-8859-4")];
    char stringpool_str170[sizeof("ISO_8859-4")];
    char stringpool_str171[sizeof("ISO8859-14")];
    char stringpool_str172[sizeof("ISO-8859-14")];
    char stringpool_str173[sizeof("ISO_8859-14")];
    char stringpool_str176[sizeof("ISO8859-5")];
    char stringpool_str177[sizeof("ISO-8859-5")];
    char stringpool_str178[sizeof("ISO_8859-5")];
    char stringpool_str179[sizeof("ISO8859-15")];
    char stringpool_str180[sizeof("ISO-8859-15")];
    char stringpool_str181[sizeof("ISO_8859-15")];
    char stringpool_str182[sizeof("ISO8859-2")];
    char stringpool_str183[sizeof("ISO-8859-2")];
    char stringpool_str184[sizeof("ISO_8859-2")];
    char stringpool_str185[sizeof("GB2312")];
    char stringpool_str188[sizeof("ISO-IR-199")];
    char stringpool_str189[sizeof("ISO_8859-14:1998")];
    char stringpool_str190[sizeof("ISO8859-8")];
    char stringpool_str191[sizeof("ISO-8859-8")];
    char stringpool_str192[sizeof("ISO_8859-8")];
    char stringpool_str193[sizeof("ISO_8859-15:1998")];
    char stringpool_str194[sizeof("ISO-IR-6")];
    char stringpool_str196[sizeof("ISO-2022-CN")];
    char stringpool_str197[sizeof("ISO-IR-149")];
    char stringpool_str201[sizeof("ISO-IR-159")];
    char stringpool_str203[sizeof("ISO-IR-166")];
    char stringpool_str204[sizeof("X0212")];
    char stringpool_str205[sizeof("ISO-IR-14")];
    char stringpool_str206[sizeof("ISO8859-3")];
    char stringpool_str207[sizeof("ISO-8859-3")];
    char stringpool_str208[sizeof("ISO_8859-3")];
    char stringpool_str209[sizeof("ISO8859-13")];
    char stringpool_str210[sizeof("ISO-8859-13")];
    char stringpool_str211[sizeof("ISO_8859-13")];
    char stringpool_str212[sizeof("CSISO2022CN")];
    char stringpool_str213[sizeof("MAC")];
    char stringpool_str214[sizeof("ISO-IR-126")];
    char stringpool_str215[sizeof("ISO-IR-144")];
    char stringpool_str217[sizeof("UHC")];
    char stringpool_str218[sizeof("X0201")];
    char stringpool_str219[sizeof("ISO-IR-165")];
    char stringpool_str220[sizeof("ISO_8859-10:1992")];
    char stringpool_str225[sizeof("CSPTCP154")];
    char stringpool_str228[sizeof("ISO-IR-226")];
    char stringpool_str229[sizeof("US")];
    char stringpool_str232[sizeof("ISO-IR-109")];
    char stringpool_str235[sizeof("ISO-IR-179")];
    char stringpool_str236[sizeof("ISO-IR-101")];
    char stringpool_str237[sizeof("ISO-IR-148")];
    char stringpool_str238[sizeof("ISO-IR-58")];
    char stringpool_str239[sizeof("TIS620")];
    char stringpool_str240[sizeof("TIS-620")];
    char stringpool_str241[sizeof("ISO8859-10")];
    char stringpool_str242[sizeof("ISO-8859-10")];
    char stringpool_str243[sizeof("ISO_8859-10")];
    char stringpool_str244[sizeof("ISO8859-7")];
    char stringpool_str245[sizeof("ISO-8859-7")];
    char stringpool_str246[sizeof("ISO_8859-7")];
    char stringpool_str249[sizeof("LATIN-9")];
    char stringpool_str250[sizeof("UCS-4")];
    char stringpool_str251[sizeof("MS936")];
    char stringpool_str252[sizeof("LATIN1")];
    char stringpool_str253[sizeof("CSUCS4")];
    char stringpool_str254[sizeof("X0208")];
    char stringpool_str256[sizeof("ISO-IR-138")];
    char stringpool_str257[sizeof("ROMAN8")];
    char stringpool_str258[sizeof("LATIN6")];
    char stringpool_str260[sizeof("ELOT_928")];
    char stringpool_str262[sizeof("GB_1988-80")];
    char stringpool_str264[sizeof("UCS-2")];
    char stringpool_str266[sizeof("LATIN4")];
    char stringpool_str267[sizeof("ARABIC")];
    char stringpool_str271[sizeof("ASCII")];
    char stringpool_str273[sizeof("CYRILLIC")];
    char stringpool_str274[sizeof("LATIN5")];
    char stringpool_str278[sizeof("ISO-IR-110")];
    char stringpool_str280[sizeof("LATIN2")];
    char stringpool_str282[sizeof("GB_2312-80")];
    char stringpool_str283[sizeof("UTF-16")];
    char stringpool_str285[sizeof("TIS620-0")];
    char stringpool_str286[sizeof("ISO_8859-9:1989")];
    char stringpool_str288[sizeof("LATIN8")];
    char stringpool_str289[sizeof("CSASCII")];
    char stringpool_str290[sizeof("GB18030")];
    char stringpool_str292[sizeof("ISO-IR-57")];
    char stringpool_str295[sizeof("ISO-IR-157")];
    char stringpool_str296[sizeof("CYRILLIC-ASIAN")];
    char stringpool_str298[sizeof("ISO-IR-127")];
    char stringpool_str299[sizeof("ISO-IR-87")];
    char stringpool_str300[sizeof("BIG5")];
    char stringpool_str301[sizeof("BIG-5")];
    char stringpool_str302[sizeof("ISO-IR-203")];
    char stringpool_str304[sizeof("LATIN3")];
    char stringpool_str306[sizeof("KSC_5601")];
    char stringpool_str307[sizeof("ISO-2022-CN-EXT")];
    char stringpool_str310[sizeof("UTF-8")];
    char stringpool_str313[sizeof("KS_C_5601-1989")];
    char stringpool_str315[sizeof("ISO_8859-4:1988")];
    char stringpool_str318[sizeof("HP-ROMAN8")];
    char stringpool_str319[sizeof("ISO_8859-5:1988")];
    char stringpool_str320[sizeof("ISO-IR-100")];
    char stringpool_str321[sizeof("MS-CYRL")];
    char stringpool_str322[sizeof("EUCKR")];
    char stringpool_str323[sizeof("EUC-KR")];
    char stringpool_str324[sizeof("IBM819")];
    char stringpool_str325[sizeof("ECMA-114")];
    char stringpool_str326[sizeof("ISO_8859-8:1988")];
    char stringpool_str327[sizeof("KOREAN")];
    char stringpool_str329[sizeof("TCVN")];
    char stringpool_str330[sizeof("GEORGIAN-ACADEMY")];
    char stringpool_str331[sizeof("UTF-32")];
    char stringpool_str334[sizeof("ISO_8859-3:1988")];
    char stringpool_str335[sizeof("ISO_8859-1:1987")];
    char stringpool_str337[sizeof("IBM866")];
    char stringpool_str338[sizeof("ISO_8859-6:1987")];
    char stringpool_str339[sizeof("LATIN10")];
    char stringpool_str342[sizeof("LATIN7")];
    char stringpool_str344[sizeof("KOI8-R")];
    char stringpool_str346[sizeof("CSKOI8R")];
    char stringpool_str347[sizeof("ECMA-118")];
    char stringpool_str348[sizeof("ASMO-708")];
    char stringpool_str349[sizeof("ISO_8859-2:1987")];
    char stringpool_str356[sizeof("CSHPROMAN8")];
    char stringpool_str357[sizeof("CSGB2312")];
    char stringpool_str358[sizeof("ISO646-US")];
    char stringpool_str359[sizeof("IBM862")];
    char stringpool_str360[sizeof("KS_C_5601-1987")];
    char stringpool_str361[sizeof("ISO_8859-7:2003")];
    char stringpool_str362[sizeof("CSISOLATIN1")];
    char stringpool_str364[sizeof("UTF-7")];
    char stringpool_str365[sizeof("CSISOLATINARABIC")];
    char stringpool_str367[sizeof("CSISOLATINCYRILLIC")];
    char stringpool_str368[sizeof("CSISOLATIN6")];
    char stringpool_str370[sizeof("GEORGIAN-PS")];
    char stringpool_str371[sizeof("CHINESE")];
    char stringpool_str373[sizeof("CSKSC56011987")];
    char stringpool_str376[sizeof("CSISOLATIN4")];
    char stringpool_str380[sizeof("ISO_8859-7:1987")];
    char stringpool_str384[sizeof("CSISOLATIN5")];
    char stringpool_str389[sizeof("ISO-10646-UCS-4")];
    char stringpool_str390[sizeof("CSISOLATIN2")];
    char stringpool_str391[sizeof("CSBIG5")];
    char stringpool_str392[sizeof("CN-BIG5")];
    char stringpool_str394[sizeof("MULELAO-1")];
    char stringpool_str396[sizeof("ISO-10646-UCS-2")];
    char stringpool_str403[sizeof("JP")];
    char stringpool_str407[sizeof("GSM7")];
    char stringpool_str408[sizeof("GSM-7")];
    char stringpool_str409[sizeof("MS-ANSI")];
    char stringpool_str410[sizeof("UNICODE-1-1")];
    char stringpool_str413[sizeof("CSUNICODE11")];
    char stringpool_str414[sizeof("CSISOLATIN3")];
    char stringpool_str415[sizeof("TCVN5712-1")];
    char stringpool_str416[sizeof("HZ-GB-2312")];
    char stringpool_str421[sizeof("ISO-2022-KR")];
    char stringpool_str423[sizeof("IBM850")];
    char stringpool_str424[sizeof("MACCROATIAN")];
    char stringpool_str426[sizeof("TCVN-5712")];
    char stringpool_str427[sizeof("ISO-CELTIC")];
    char stringpool_str429[sizeof("IBM367")];
    char stringpool_str430[sizeof("MACROMAN")];
    char stringpool_str431[sizeof("IBM-CP1133")];
    char stringpool_str437[sizeof("CSISO2022KR")];
    char stringpool_str440[sizeof("TIS620.2529-1")];
    char stringpool_str445[sizeof("CN-GB")];
    char stringpool_str450[sizeof("ARMSCII-8")];
    char stringpool_str452[sizeof("MACICELAND")];
    char stringpool_str458[sizeof("UCS-4LE")];
    char stringpool_str461[sizeof("UNICODE-1-1-UTF-7")];
    char stringpool_str462[sizeof("CSUNICODE11UTF7")];
    char stringpool_str465[sizeof("UCS-2LE")];
    char stringpool_str468[sizeof("JIS_C6226-1983")];
    char stringpool_str469[sizeof("CSISO57GB1988")];
    char stringpool_str470[sizeof("WINDOWS-1251")];
    char stringpool_str471[sizeof("MS-EE")];
    char stringpool_str473[sizeof("WINDOWS-1256")];
    char stringpool_str474[sizeof("WINDOWS-936")];
    char stringpool_str477[sizeof("WINDOWS-1254")];
    char stringpool_str479[sizeof("MACARABIC")];
    char stringpool_str480[sizeof("TIS620.2533-1")];
    char stringpool_str481[sizeof("WINDOWS-1255")];
    char stringpool_str482[sizeof("JIS_C6220-1969-RO")];
    char stringpool_str484[sizeof("WINDOWS-1252")];
    char stringpool_str486[sizeof("WCHAR_T")];
    char stringpool_str488[sizeof("WINDOWS-1258")];
    char stringpool_str489[sizeof("CN-GB-ISOIR165")];
    char stringpool_str491[sizeof("CSUNICODE")];
    char stringpool_str495[sizeof("UTF-16LE")];
    char stringpool_str496[sizeof("WINDOWS-1253")];
    char stringpool_str498[sizeof("VISCII")];
    char stringpool_str501[sizeof("US-ASCII")];
    char stringpool_str503[sizeof("ANSI_X3.4-1986")];
    char stringpool_str504[sizeof("MACCYRILLIC")];
    char stringpool_str509[sizeof("CSIBM866")];
    char stringpool_str510[sizeof("CSISO58GB231280")];
    char stringpool_str512[sizeof("WINDOWS-1250")];
    char stringpool_str515[sizeof("WINDOWS-1257")];
    char stringpool_str518[sizeof("ANSI_X3.4-1968")];
    char stringpool_str520[sizeof("MACROMANIA")];
    char stringpool_str521[sizeof("WINDOWS-874")];
    char stringpool_str522[sizeof("TIS620.2533-0")];
    char stringpool_str528[sizeof("MS-HEBR")];
    char stringpool_str529[sizeof("EUCJP")];
    char stringpool_str530[sizeof("EUC-JP")];
    char stringpool_str531[sizeof("JIS0208")];
    char stringpool_str532[sizeof("UTF-32LE")];
    char stringpool_str537[sizeof("CSEUCKR")];
    char stringpool_str539[sizeof("CSPC862LATINHEBREW")];
    char stringpool_str544[sizeof("UCS-4-SWAPPED")];
    char stringpool_str548[sizeof("MACINTOSH")];
    char stringpool_str549[sizeof("GREEK8")];
    char stringpool_str550[sizeof("NEXTSTEP")];
    char stringpool_str551[sizeof("UCS-2-SWAPPED")];
    char stringpool_str552[sizeof("CSMACINTOSH")];
    char stringpool_str553[sizeof("ISO646-JP")];
    char stringpool_str555[sizeof("MS-ARAB")];
    char stringpool_str560[sizeof("MACTHAI")];
    char stringpool_str562[sizeof("KOI8-T")];
    char stringpool_str564[sizeof("GBK")];
    char stringpool_str575[sizeof("TCVN5712-1:1993")];
    char stringpool_str578[sizeof("UCS-4-INTERNAL")];
    char stringpool_str583[sizeof("JAVA")];
    char stringpool_str584[sizeof("UCS-4BE")];
    char stringpool_str585[sizeof("UCS-2-INTERNAL")];
    char stringpool_str589[sizeof("CSVISCII")];
    char stringpool_str591[sizeof("UCS-2BE")];
    char stringpool_str596[sizeof("ISO-2022-JP-1")];
    char stringpool_str601[sizeof("CSISO14JISC6220RO")];
    char stringpool_str603[sizeof("EUCTW")];
    char stringpool_str604[sizeof("EUC-TW")];
    char stringpool_str610[sizeof("ISO-2022-JP-2")];
    char stringpool_str614[sizeof("VISCII1.1-1")];
    char stringpool_str617[sizeof("ISO_646.IRV:1991")];
    char stringpool_str621[sizeof("UTF-16BE")];
    char stringpool_str622[sizeof("CSISOLATINHEBREW")];
    char stringpool_str625[sizeof("CSISO2022JP2")];
    char stringpool_str626[sizeof("BIG5HKSCS")];
    char stringpool_str627[sizeof("BIG5-HKSCS")];
    char stringpool_str628[sizeof("ISO-2022-JP")];
    char stringpool_str633[sizeof("JIS_X0212")];
    char stringpool_str638[sizeof("CSHALFWIDTHKATAKANA")];
    char stringpool_str639[sizeof("MACCENTRALEUROPE")];
    char stringpool_str644[sizeof("CSISO2022JP")];
    char stringpool_str647[sizeof("JIS_X0201")];
    char stringpool_str651[sizeof("CSISO159JISX02121990")];
    char stringpool_str655[sizeof("JISX0201-1976")];
    char stringpool_str658[sizeof("UTF-32BE")];
    char stringpool_str668[sizeof("JIS_X0212-1990")];
    char stringpool_str683[sizeof("JIS_X0208")];
    char stringpool_str693[sizeof("CSISOLATINGREEK")];
    char stringpool_str698[sizeof("JIS_X0208-1983")];
    char stringpool_str714[sizeof("JIS_X0208-1990")];
    char stringpool_str721[sizeof("HEBREW")];
    char stringpool_str727[sizeof("EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE")];
    char stringpool_str738[sizeof("GREEK")];
    char stringpool_str749[sizeof("CSPC850MULTILINGUAL")];
    char stringpool_str757[sizeof("SHIFT-JIS")];
    char stringpool_str758[sizeof("SHIFT_JIS")];
    char stringpool_str767[sizeof("JOHAB")];
    char stringpool_str768[sizeof("KOI8-U")];
    char stringpool_str769[sizeof("KOI8-RU")];
    char stringpool_str802[sizeof("BIGFIVE")];
    char stringpool_str803[sizeof("BIG-FIVE")];
    char stringpool_str817[sizeof("CSSHIFTJIS")];
    char stringpool_str818[sizeof("CSEUCTW")];
    char stringpool_str823[sizeof("CSISO87JISX0208")];
    char stringpool_str841[sizeof("UNICODELITTLE")];
    char stringpool_str861[sizeof("JIS_X0212.1990-0")];
    char stringpool_str862[sizeof("UNICODEBIG")];
    char stringpool_str924[sizeof("MACUKRAINE")];
    char stringpool_str949[sizeof("MS-GREEK")];
    char stringpool_str950[sizeof("MACGREEK")];
    char stringpool_str989[sizeof("MACTURKISH")];
    char stringpool_str991[sizeof("MS_KANJI")];
    char stringpool_str996[sizeof("MS-TURK")];
    char stringpool_str1019[sizeof("MACHEBREW")];
    char stringpool_str1039[sizeof("WINBALTRIM")];
    char stringpool_str1047[sizeof("CSEUCPKDFMTJAPANESE")];
  };
static const struct stringpool_t stringpool_contents =
  {
    "C99",
    "CN",
    "CHAR",
    "CP949",
    "R8",
    "CP819",
    "866",
    "CP154",
    "CP866",
    "CP1251",
    "CP936",
    "CP1361",
    "L1",
    "CP1256",
    "L6",
    "L4",
    "862",
    "CP1254",
    "L5",
    "HZ",
    "L2",
    "CP862",
    "CP1255",
    "L8",
    "CP932",
    "CP1252",
    "L3",
    "PT154",
    "CP1258",
    "CP874",
    "L7",
    "CP1253",
    "CP1133",
    "EUCCN",
    "EUC-CN",
    "CP950",
    "850",
    "PTCP154",
    "ISO646-CN",
    "CP1250",
    "CP850",
    "CP1257",
    "CP367",
    "L10",
    "SJIS",
    "ISO8859-9",
    "ISO-8859-9",
    "ISO_8859-9",
    "ISO8859-1",
    "ISO-8859-1",
    "ISO_8859-1",
    "ISO8859-11",
    "ISO-8859-11",
    "ISO_8859-11",
    "ISO8859-6",
    "ISO-8859-6",
    "ISO_8859-6",
    "ISO8859-16",
    "ISO-8859-16",
    "ISO_8859-16",
    "ISO_8859-16:2001",
    "ISO8859-4",
    "ISO-8859-4",
    "ISO_8859-4",
    "ISO8859-14",
    "ISO-8859-14",
    "ISO_8859-14",
    "ISO8859-5",
    "ISO-8859-5",
    "ISO_8859-5",
    "ISO8859-15",
    "ISO-8859-15",
    "ISO_8859-15",
    "ISO8859-2",
    "ISO-8859-2",
    "ISO_8859-2",
    "GB2312",
    "ISO-IR-199",
    "ISO_8859-14:1998",
    "ISO8859-8",
    "ISO-8859-8",
    "ISO_8859-8",
    "ISO_8859-15:1998",
    "ISO-IR-6",
    "ISO-2022-CN",
    "ISO-IR-149",
    "ISO-IR-159",
    "ISO-IR-166",
    "X0212",
    "ISO-IR-14",
    "ISO8859-3",
    "ISO-8859-3",
    "ISO_8859-3",
    "ISO8859-13",
    "ISO-8859-13",
    "ISO_8859-13",
    "CSISO2022CN",
    "MAC",
    "ISO-IR-126",
    "ISO-IR-144",
    "UHC",
    "X0201",
    "ISO-IR-165",
    "ISO_8859-10:1992",
    "CSPTCP154",
    "ISO-IR-226",
    "US",
    "ISO-IR-109",
    "ISO-IR-179",
    "ISO-IR-101",
    "ISO-IR-148",
    "ISO-IR-58",
    "TIS620",
    "TIS-620",
    "ISO8859-10",
    "ISO-8859-10",
    "ISO_8859-10",
    "ISO8859-7",
    "ISO-8859-7",
    "ISO_8859-7",
    "LATIN-9",
    "UCS-4",
    "MS936",
    "LATIN1",
    "CSUCS4",
    "X0208",
    "ISO-IR-138",
    "ROMAN8",
    "LATIN6",
    "ELOT_928",
    "GB_1988-80",
    "UCS-2",
    "LATIN4",
    "ARABIC",
    "ASCII",
    "CYRILLIC",
    "LATIN5",
    "ISO-IR-110",
    "LATIN2",
    "GB_2312-80",
    "UTF-16",
    "TIS620-0",
    "ISO_8859-9:1989",
    "LATIN8",
    "CSASCII",
    "GB18030",
    "ISO-IR-57",
    "ISO-IR-157",
    "CYRILLIC-ASIAN",
    "ISO-IR-127",
    "ISO-IR-87",
    "BIG5",
    "BIG-5",
    "ISO-IR-203",
    "LATIN3",
    "KSC_5601",
    "ISO-2022-CN-EXT",
    "UTF-8",
    "KS_C_5601-1989",
    "ISO_8859-4:1988",
    "HP-ROMAN8",
    "ISO_8859-5:1988",
    "ISO-IR-100",
    "MS-CYRL",
    "EUCKR",
    "EUC-KR",
    "IBM819",
    "ECMA-114",
    "ISO_8859-8:1988",
    "KOREAN",
    "TCVN",
    "GEORGIAN-ACADEMY",
    "UTF-32",
    "ISO_8859-3:1988",
    "ISO_8859-1:1987",
    "IBM866",
    "ISO_8859-6:1987",
    "LATIN10",
    "LATIN7",
    "KOI8-R",
    "CSKOI8R",
    "ECMA-118",
    "ASMO-708",
    "ISO_8859-2:1987",
    "CSHPROMAN8",
    "CSGB2312",
    "ISO646-US",
    "IBM862",
    "KS_C_5601-1987",
    "ISO_8859-7:2003",
    "CSISOLATIN1",
    "UTF-7",
    "CSISOLATINARABIC",
    "CSISOLATINCYRILLIC",
    "CSISOLATIN6",
    "GEORGIAN-PS",
    "CHINESE",
    "CSKSC56011987",
    "CSISOLATIN4",
    "ISO_8859-7:1987",
    "CSISOLATIN5",
    "ISO-10646-UCS-4",
    "CSISOLATIN2",
    "CSBIG5",
    "CN-BIG5",
    "MULELAO-1",
    "ISO-10646-UCS-2",
    "JP",
    "GSM7",
    "GSM-7",
    "MS-ANSI",
    "UNICODE-1-1",
    "CSUNICODE11",
    "CSISOLATIN3",
    "TCVN5712-1",
    "HZ-GB-2312",
    "ISO-2022-KR",
    "IBM850",
    "MACCROATIAN",
    "TCVN-5712",
    "ISO-CELTIC",
    "IBM367",
    "MACROMAN",
    "IBM-CP1133",
    "CSISO2022KR",
    "TIS620.2529-1",
    "CN-GB",
    "ARMSCII-8",
    "MACICELAND",
    "UCS-4LE",
    "UNICODE-1-1-UTF-7",
    "CSUNICODE11UTF7",
    "UCS-2LE",
    "JIS_C6226-1983",
    "CSISO57GB1988",
    "WINDOWS-1251",
    "MS-EE",
    "WINDOWS-1256",
    "WINDOWS-936",
    "WINDOWS-1254",
    "MACARABIC",
    "TIS620.2533-1",
    "WINDOWS-1255",
    "JIS_C6220-1969-RO",
    "WINDOWS-1252",
    "WCHAR_T",
    "WINDOWS-1258",
    "CN-GB-ISOIR165",
    "CSUNICODE",
    "UTF-16LE",
    "WINDOWS-1253",
    "VISCII",
    "US-ASCII",
    "ANSI_X3.4-1986",
    "MACCYRILLIC",
    "CSIBM866",
    "CSISO58GB231280",
    "WINDOWS-1250",
    "WINDOWS-1257",
    "ANSI_X3.4-1968",
    "MACROMANIA",
    "WINDOWS-874",
    "TIS620.2533-0",
    "MS-HEBR",
    "EUCJP",
    "EUC-JP",
    "JIS0208",
    "UTF-32LE",
    "CSEUCKR",
    "CSPC862LATINHEBREW",
    "UCS-4-SWAPPED",
    "MACINTOSH",
    "GREEK8",
    "NEXTSTEP",
    "UCS-2-SWAPPED",
    "CSMACINTOSH",
    "ISO646-JP",
    "MS-ARAB",
    "MACTHAI",
    "KOI8-T",
    "GBK",
    "TCVN5712-1:1993",
    "UCS-4-INTERNAL",
    "JAVA",
    "UCS-4BE",
    "UCS-2-INTERNAL",
    "CSVISCII",
    "UCS-2BE",
    "ISO-2022-JP-1",
    "CSISO14JISC6220RO",
    "EUCTW",
    "EUC-TW",
    "ISO-2022-JP-2",
    "VISCII1.1-1",
    "ISO_646.IRV:1991",
    "UTF-16BE",
    "CSISOLATINHEBREW",
    "CSISO2022JP2",
    "BIG5HKSCS",
    "BIG5-HKSCS",
    "ISO-2022-JP",
    "JIS_X0212",
    "CSHALFWIDTHKATAKANA",
    "MACCENTRALEUROPE",
    "CSISO2022JP",
    "JIS_X0201",
    "CSISO159JISX02121990",
    "JISX0201-1976",
    "UTF-32BE",
    "JIS_X0212-1990",
    "JIS_X0208",
    "CSISOLATINGREEK",
    "JIS_X0208-1983",
    "JIS_X0208-1990",
    "HEBREW",
    "EXTENDED_UNIX_CODE_PACKED_FORMAT_FOR_JAPANESE",
    "GREEK",
    "CSPC850MULTILINGUAL",
    "SHIFT-JIS",
    "SHIFT_JIS",
    "JOHAB",
    "KOI8-U",
    "KOI8-RU",
    "BIGFIVE",
    "BIG-FIVE",
    "CSSHIFTJIS",
    "CSEUCTW",
    "CSISO87JISX0208",
    "UNICODELITTLE",
    "JIS_X0212.1990-0",
    "UNICODEBIG",
    "MACUKRAINE",
    "MS-GREEK",
    "MACGREEK",
    "MACTURKISH",
    "MS_KANJI",
    "MS-TURK",
    "MACHEBREW",
    "WINBALTRIM",
    "CSEUCPKDFMTJAPANESE"
  };
#define stringpool ((const char *) &stringpool_contents)

static const struct alias aliases[] =
  {
    {-1}, {-1}, {-1}, {-1},
#line 51 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str4, ei_c99},
#line 283 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str5, ei_iso646_cn},
#line 350 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str6, ei_local_char},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 342 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str15, ei_cp949},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 226 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str22, ei_hp_roman8},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 57 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str28, ei_iso8859_1},
    {-1}, {-1}, {-1}, {-1},
#line 207 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str33, ei_cp866},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 235 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str39, ei_pt154},
    {-1},
#line 205 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str41, ei_cp866},
#line 174 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str42, ei_cp1251},
    {-1},
#line 318 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str44, ei_ces_gbk},
    {-1},
#line 345 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str46, ei_johab},
#line 60 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str47, ei_iso8859_1},
#line 189 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str48, ei_cp1256},
    {-1},
#line 134 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str50, ei_iso8859_10},
    {-1}, {-1}, {-1},
#line 84 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str54, ei_iso8859_4},
#line 203 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str55, ei_cp862},
#line 183 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str56, ei_cp1254},
    {-1},
#line 126 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str58, ei_iso8859_9},
    {-1},
#line 325 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str60, ei_hz},
#line 68 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str61, ei_iso8859_2},
    {-1},
#line 201 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str63, ei_cp862},
#line 186 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str64, ei_cp1255},
#line 151 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str65, ei_iso8859_14},
#line 306 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str66, ei_cp932},
    {-1}, {-1}, {-1},
#line 177 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str70, ei_cp1252},
    {-1}, {-1},
#line 76 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str73, ei_iso8859_3},
#line 233 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str74, ei_pt154},
    {-1}, {-1}, {-1},
#line 195 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str78, ei_cp1258},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1},
#line 248 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str91, ei_cp874},
#line 144 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str92, ei_iso8859_13},
    {-1},
#line 180 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str94, ei_cp1253},
#line 239 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str95, ei_cp1133},
    {-1},
#line 313 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str97, ei_euc_cn},
#line 312 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str98, ei_euc_cn},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 336 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str107, ei_cp950},
    {-1}, {-1}, {-1},
#line 199 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str111, ei_cp850},
    {-1},
#line 234 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str113, ei_pt154},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 281 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str121, ei_iso646_cn},
    {-1}, {-1}, {-1}, {-1},
#line 171 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str126, ei_cp1250},
#line 197 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str127, ei_cp850},
    {-1}, {-1}, {-1}, {-1},
#line 192 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str132, ei_cp1257},
#line 19 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str133, ei_ascii},
#line 165 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str134, ei_iso8859_16},
    {-1}, {-1},
#line 303 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str137, ei_sjis},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1},
#line 128 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str150, ei_iso8859_9},
#line 121 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str151, ei_iso8859_9},
#line 122 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str152, ei_iso8859_9},
    {-1},
#line 62 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str154, ei_iso8859_1},
#line 53 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str155, ei_iso8859_1},
#line 54 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str156, ei_iso8859_1},
#line 139 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str157, ei_iso8859_11},
#line 137 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str158, ei_iso8859_11},
#line 138 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str159, ei_iso8859_11},
#line 102 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str160, ei_iso8859_6},
#line 94 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str161, ei_iso8859_6},
#line 95 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str162, ei_iso8859_6},
#line 166 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str163, ei_iso8859_16},
#line 160 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str164, ei_iso8859_16},
#line 161 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str165, ei_iso8859_16},
    {-1},
#line 162 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str167, ei_iso8859_16},
#line 86 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str168, ei_iso8859_4},
#line 79 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str169, ei_iso8859_4},
#line 80 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str170, ei_iso8859_4},
#line 153 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str171, ei_iso8859_14},
#line 146 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str172, ei_iso8859_14},
#line 147 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str173, ei_iso8859_14},
    {-1}, {-1},
#line 93 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str176, ei_iso8859_5},
#line 87 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str177, ei_iso8859_5},
#line 88 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str178, ei_iso8859_5},
#line 159 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str179, ei_iso8859_15},
#line 154 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str180, ei_iso8859_15},
#line 155 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str181, ei_iso8859_15},
#line 70 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str182, ei_iso8859_2},
#line 63 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str183, ei_iso8859_2},
#line 64 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str184, ei_iso8859_2},
#line 314 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str185, ei_euc_cn},
    {-1}, {-1},
#line 149 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str188, ei_iso8859_14},
#line 148 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str189, ei_iso8859_14},
#line 120 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str190, ei_iso8859_8},
#line 114 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str191, ei_iso8859_8},
#line 115 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str192, ei_iso8859_8},
#line 156 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str193, ei_iso8859_15},
#line 16 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str194, ei_ascii},
    {-1},
#line 322 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str196, ei_iso2022_cn},
#line 294 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str197, ei_ksc5601},
    {-1}, {-1}, {-1},
#line 278 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str201, ei_jisx0212},
    {-1},
#line 247 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str203, ei_tis620},
#line 277 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str204, ei_jisx0212},
#line 259 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str205, ei_iso646_jp},
#line 78 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str206, ei_iso8859_3},
#line 71 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str207, ei_iso8859_3},
#line 72 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str208, ei_iso8859_3},
#line 145 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str209, ei_iso8859_13},
#line 140 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str210, ei_iso8859_13},
#line 141 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str211, ei_iso8859_13},
#line 323 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str212, ei_iso2022_cn},
#line 211 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str213, ei_mac_roman},
#line 107 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str214, ei_iso8859_7},
#line 90 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str215, ei_iso8859_5},
    {-1},
#line 343 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str217, ei_cp949},
#line 264 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str218, ei_jisx0201},
#line 289 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str219, ei_isoir165},
#line 131 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str220, ei_iso8859_10},
    {-1}, {-1}, {-1}, {-1},
#line 237 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str225, ei_pt154},
    {-1}, {-1},
#line 163 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str228, ei_iso8859_16},
#line 21 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str229, ei_ascii},
    {-1}, {-1},
#line 74 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str232, ei_iso8859_3},
    {-1}, {-1},
#line 142 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str235, ei_iso8859_13},
#line 66 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str236, ei_iso8859_2},
#line 124 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str237, ei_iso8859_9},
#line 286 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str238, ei_gb2312},
#line 242 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str239, ei_tis620},
#line 241 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str240, ei_tis620},
#line 136 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str241, ei_iso8859_10},
#line 129 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str242, ei_iso8859_10},
#line 130 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str243, ei_iso8859_10},
#line 113 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str244, ei_iso8859_7},
#line 103 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str245, ei_iso8859_7},
#line 104 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str246, ei_iso8859_7},
    {-1}, {-1},
#line 158 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str249, ei_iso8859_15},
#line 33 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str250, ei_ucs4},
#line 319 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str251, ei_ces_gbk},
#line 59 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str252, ei_iso8859_1},
#line 35 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str253, ei_ucs4},
#line 270 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str254, ei_jisx0208},
    {-1},
#line 117 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str256, ei_iso8859_8},
#line 225 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str257, ei_hp_roman8},
#line 133 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str258, ei_iso8859_10},
    {-1},
#line 109 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str260, ei_iso8859_7},
    {-1},
#line 280 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str262, ei_iso646_cn},
    {-1},
#line 24 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str264, ei_ucs2},
    {-1},
#line 83 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str266, ei_iso8859_4},
#line 100 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str267, ei_iso8859_6},
    {-1}, {-1}, {-1},
#line 13 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str271, ei_ascii},
    {-1},
#line 91 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str273, ei_iso8859_5},
#line 125 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str274, ei_iso8859_9},
    {-1}, {-1}, {-1},
#line 82 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str278, ei_iso8859_4},
    {-1},
#line 67 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str280, ei_iso8859_2},
    {-1},
#line 285 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str282, ei_gb2312},
#line 38 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str283, ei_utf16},
    {-1},
#line 243 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str285, ei_tis620},
#line 123 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str286, ei_iso8859_9},
    {-1},
#line 150 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str288, ei_iso8859_14},
#line 22 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str289, ei_ascii},
#line 321 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str290, ei_gb18030},
    {-1},
#line 282 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str292, ei_iso646_cn},
    {-1}, {-1},
#line 132 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str295, ei_iso8859_10},
#line 236 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str296, ei_pt154},
    {-1},
#line 97 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str298, ei_iso8859_6},
#line 271 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str299, ei_jisx0208},
#line 330 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str300, ei_ces_big5},
#line 331 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str301, ei_ces_big5},
#line 157 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str302, ei_iso8859_15},
    {-1},
#line 75 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str304, ei_iso8859_3},
    {-1},
#line 291 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str306, ei_ksc5601},
#line 324 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str307, ei_iso2022_cn_ext},
    {-1}, {-1},
#line 23 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str310, ei_utf8},
    {-1}, {-1},
#line 293 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str313, ei_ksc5601},
    {-1},
#line 81 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str315, ei_iso8859_4},
    {-1}, {-1},
#line 224 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str318, ei_hp_roman8},
#line 89 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str319, ei_iso8859_5},
#line 56 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str320, ei_iso8859_1},
#line 176 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str321, ei_cp1251},
#line 340 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str322, ei_euc_kr},
#line 339 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str323, ei_euc_kr},
#line 58 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str324, ei_iso8859_1},
#line 98 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str325, ei_iso8859_6},
#line 116 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str326, ei_iso8859_8},
#line 296 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str327, ei_ksc5601},
    {-1},
#line 253 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str329, ei_tcvn},
#line 230 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str330, ei_georgian_academy},
#line 41 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str331, ei_utf32},
    {-1}, {-1},
#line 73 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str334, ei_iso8859_3},
#line 55 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str335, ei_iso8859_1},
    {-1},
#line 206 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str337, ei_cp866},
#line 96 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str338, ei_iso8859_6},
#line 164 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str339, ei_iso8859_16},
    {-1}, {-1},
#line 143 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str342, ei_iso8859_13},
    {-1},
#line 167 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str344, ei_koi8_r},
    {-1},
#line 168 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str346, ei_koi8_r},
#line 108 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str347, ei_iso8859_7},
#line 99 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str348, ei_iso8859_6},
#line 65 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str349, ei_iso8859_2},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 227 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str356, ei_hp_roman8},
#line 316 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str357, ei_euc_cn},
#line 14 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str358, ei_ascii},
#line 202 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str359, ei_cp862},
#line 292 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str360, ei_ksc5601},
#line 106 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str361, ei_iso8859_7},
#line 61 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str362, ei_iso8859_1},
    {-1},
#line 44 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str364, ei_utf7},
#line 101 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str365, ei_iso8859_6},
    {-1},
#line 92 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str367, ei_iso8859_5},
#line 135 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str368, ei_iso8859_10},
    {-1},
#line 231 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str370, ei_georgian_ps},
#line 288 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str371, ei_gb2312},
    {-1},
#line 295 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str373, ei_ksc5601},
    {-1}, {-1},
#line 85 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str376, ei_iso8859_4},
    {-1}, {-1}, {-1},
#line 105 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str380, ei_iso8859_7},
    {-1}, {-1}, {-1},
#line 127 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str384, ei_iso8859_9},
    {-1}, {-1}, {-1}, {-1},
#line 34 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str389, ei_ucs4},
#line 69 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str390, ei_iso8859_2},
#line 335 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str391, ei_ces_big5},
#line 334 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str392, ei_ces_big5},
    {-1},
#line 238 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str394, ei_mulelao},
    {-1},
#line 25 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str396, ei_ucs2},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 260 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str403, ei_iso646_jp},
    {-1}, {-1}, {-1},
#line 349 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str407, ei_gsm7},
#line 348 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str408, ei_gsm7},
#line 179 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str409, ei_cp1252},
#line 29 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str410, ei_ucs2be},
    {-1}, {-1},
#line 30 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str413, ei_ucs2be},
#line 77 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str414, ei_iso8859_3},
#line 255 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str415, ei_tcvn},
#line 326 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str416, ei_hz},
    {-1}, {-1}, {-1}, {-1},
#line 346 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str421, ei_iso2022_kr},
    {-1},
#line 198 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str423, ei_cp850},
#line 215 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str424, ei_mac_croatian},
    {-1},
#line 254 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str426, ei_tcvn},
#line 152 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str427, ei_iso8859_14},
    {-1},
#line 20 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str429, ei_ascii},
#line 209 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str430, ei_mac_roman},
#line 240 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str431, ei_cp1133},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 347 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str437, ei_iso2022_kr},
    {-1}, {-1},
#line 244 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str440, ei_tis620},
    {-1}, {-1}, {-1}, {-1},
#line 315 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str445, ei_euc_cn},
    {-1}, {-1}, {-1}, {-1},
#line 229 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str450, ei_armscii_8},
    {-1},
#line 214 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str452, ei_mac_iceland},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 37 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str458, ei_ucs4le},
    {-1}, {-1},
#line 45 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str461, ei_utf7},
#line 46 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str462, ei_utf7},
    {-1}, {-1},
#line 31 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str465, ei_ucs2le},
    {-1}, {-1},
#line 272 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str468, ei_jisx0208},
#line 284 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str469, ei_iso646_cn},
#line 175 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str470, ei_cp1251},
#line 173 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str471, ei_cp1250},
    {-1},
#line 190 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str473, ei_cp1256},
#line 320 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str474, ei_ces_gbk},
    {-1}, {-1},
#line 184 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str477, ei_cp1254},
    {-1},
#line 222 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str479, ei_mac_arabic},
#line 246 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str480, ei_tis620},
#line 187 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str481, ei_cp1255},
#line 257 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str482, ei_iso646_jp},
    {-1},
#line 178 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str484, ei_cp1252},
    {-1},
#line 351 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str486, ei_local_wchar_t},
    {-1},
#line 196 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str488, ei_cp1258},
#line 290 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str489, ei_isoir165},
    {-1},
#line 26 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str491, ei_ucs2},
    {-1}, {-1}, {-1},
#line 40 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str495, ei_utf16le},
#line 181 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str496, ei_cp1253},
    {-1},
#line 250 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str498, ei_viscii},
    {-1}, {-1},
#line 12 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str501, ei_ascii},
    {-1},
#line 18 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str503, ei_ascii},
#line 217 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str504, ei_mac_cyrillic},
    {-1}, {-1}, {-1}, {-1},
#line 208 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str509, ei_cp866},
#line 287 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str510, ei_gb2312},
    {-1},
#line 172 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str512, ei_cp1250},
    {-1}, {-1},
#line 193 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str515, ei_cp1257},
    {-1}, {-1},
#line 17 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str518, ei_ascii},
    {-1},
#line 216 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str520, ei_mac_romania},
#line 249 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str521, ei_cp874},
#line 245 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str522, ei_tis620},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 188 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str528, ei_cp1255},
#line 298 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str529, ei_euc_jp},
#line 297 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str530, ei_euc_jp},
#line 269 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str531, ei_jisx0208},
#line 43 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str532, ei_utf32le},
    {-1}, {-1}, {-1}, {-1},
#line 341 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str537, ei_euc_kr},
    {-1},
#line 204 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str539, ei_cp862},
    {-1}, {-1}, {-1}, {-1},
#line 50 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str544, ei_ucs4swapped},
    {-1}, {-1}, {-1},
#line 210 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str548, ei_mac_roman},
#line 110 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str549, ei_iso8859_7},
#line 228 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str550, ei_nextstep},
#line 48 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str551, ei_ucs2swapped},
#line 212 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str552, ei_mac_roman},
#line 258 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str553, ei_iso646_jp},
    {-1},
#line 191 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str555, ei_cp1256},
    {-1}, {-1}, {-1}, {-1},
#line 223 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str560, ei_mac_thai},
    {-1},
#line 232 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str562, ei_koi8_t},
    {-1},
#line 317 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str564, ei_ces_gbk},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 256 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str575, ei_tcvn},
    {-1}, {-1},
#line 49 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str578, ei_ucs4internal},
    {-1}, {-1}, {-1}, {-1},
#line 52 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str583, ei_java},
#line 36 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str584, ei_ucs4be},
#line 47 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str585, ei_ucs2internal},
    {-1}, {-1}, {-1},
#line 252 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str589, ei_viscii},
    {-1},
#line 27 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str591, ei_ucs2be},
    {-1}, {-1}, {-1}, {-1},
#line 309 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str596, ei_iso2022_jp1},
    {-1}, {-1}, {-1}, {-1},
#line 261 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str601, ei_iso646_jp},
    {-1},
#line 328 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str603, ei_euc_tw},
#line 327 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str604, ei_euc_tw},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 310 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str610, ei_iso2022_jp2},
    {-1}, {-1}, {-1},
#line 251 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str614, ei_viscii},
    {-1}, {-1},
#line 15 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str617, ei_ascii},
    {-1}, {-1}, {-1},
#line 39 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str621, ei_utf16be},
#line 119 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str622, ei_iso8859_8},
    {-1}, {-1},
#line 311 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str625, ei_iso2022_jp2},
#line 338 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str626, ei_big5hkscs},
#line 337 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str627, ei_big5hkscs},
#line 307 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str628, ei_iso2022_jp},
    {-1}, {-1}, {-1}, {-1},
#line 274 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str633, ei_jisx0212},
    {-1}, {-1}, {-1}, {-1},
#line 265 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str638, ei_jisx0201},
#line 213 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str639, ei_mac_centraleurope},
    {-1}, {-1}, {-1}, {-1},
#line 308 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str644, ei_iso2022_jp},
    {-1}, {-1},
#line 262 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str647, ei_jisx0201},
    {-1}, {-1}, {-1},
#line 279 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str651, ei_jisx0212},
    {-1}, {-1}, {-1},
#line 263 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str655, ei_jisx0201},
    {-1}, {-1},
#line 42 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str658, ei_utf32be},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 276 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str668, ei_jisx0212},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 266 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str683, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 112 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str693, ei_iso8859_7},
    {-1}, {-1}, {-1}, {-1},
#line 267 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str698, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 268 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str714, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 118 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str721, ei_iso8859_8},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 299 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str727, ei_euc_jp},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 111 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str738, ei_iso8859_7},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 200 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str749, ei_cp850},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 302 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str757, ei_sjis},
#line 301 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str758, ei_sjis},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 344 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str767, ei_johab},
#line 169 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str768, ei_koi8_u},
#line 170 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str769, ei_koi8_ru},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1},
#line 333 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str802, ei_ces_big5},
#line 332 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str803, ei_ces_big5},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1},
#line 305 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str817, ei_sjis},
#line 329 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str818, ei_euc_tw},
    {-1}, {-1}, {-1}, {-1},
#line 273 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str823, ei_jisx0208},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 32 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str841, ei_ucs2le},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 275 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str861, ei_jisx0212},
#line 28 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str862, ei_ucs2be},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 218 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str924, ei_mac_ukraine},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 182 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str949, ei_cp1253},
#line 219 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str950, ei_mac_greek},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1},
#line 220 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str989, ei_mac_turkish},
    {-1},
#line 304 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str991, ei_sjis},
    {-1}, {-1}, {-1}, {-1},
#line 185 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str996, ei_cp1254},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1},
#line 221 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str1019, ei_mac_hebrew},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
    {-1},
#line 194 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str1039, ei_cp1257},
    {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 300 "lib/aliases.gperf"
    {(int)(long)&((struct stringpool_t *)0)->stringpool_str1047, ei_euc_jp}
  };

#ifdef __GNUC__
__inline
#ifdef __GNUC_STDC_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
const struct alias *
aliases_lookup (register const char *str, register unsigned int len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = aliases_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register int o = aliases[key].name;
          if (o >= 0)
            {
              register const char *s = o + stringpool;

              if (*str == *s && !strcmp (str + 1, s + 1))
                return &aliases[key];
            }
        }
    }
  return 0;
}
