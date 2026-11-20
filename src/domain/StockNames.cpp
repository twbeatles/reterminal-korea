#include "StockNames.h"

#include <string.h>

namespace kdash {
namespace stocks {
namespace {
// Sorted by code (binary search). Codes/names were cross-checked on 2026-11-11 against the
// FSC GetStockSecuritiesInfoService_V2 itmsNm field (a few long names shortened for the
// 190px name column). Display names are short common names.
constexpr NameEntry kNames[] = {
    {"000100", "유한양행"},
    {"000270", "기아"},
    {"000660", "SK하이닉스"},
    {"000720", "현대건설"},
    {"000810", "삼성화재"},
    {"000880", "한화"},
    {"003490", "대한항공"},
    {"003550", "LG"},
    {"003670", "포스코퓨처엠"},
    {"004020", "현대제철"},
    {"005380", "현대차"},
    {"005490", "POSCO홀딩스"},
    {"005930", "삼성전자"},
    {"005935", "삼성전자우"},
    {"006400", "삼성SDI"},
    {"009150", "삼성전기"},
    {"009540", "HD한국조선해양"},
    {"009830", "한화솔루션"},
    {"010130", "고려아연"},
    {"010140", "삼성중공업"},
    {"010950", "S-Oil"},
    {"011170", "롯데케미칼"},
    {"011200", "HMM"},
    {"011790", "SKC"},
    {"012330", "현대모비스"},
    {"012450", "한화에어로스페이스"},
    {"015760", "한국전력"},
    {"017670", "SK텔레콤"},
    {"018260", "삼성에스디에스"},
    {"018880", "한온시스템"},
    {"023530", "롯데쇼핑"},
    {"024110", "기업은행"},
    {"028050", "삼성E&A"},
    {"028260", "삼성물산"},
    {"028300", "HLB"},
    {"030200", "KT"},
    {"032830", "삼성생명"},
    {"033780", "KT&G"},
    {"034020", "두산에너빌리티"},
    {"034220", "LG디스플레이"},
    {"034730", "SK"},
    {"035420", "NAVER"},
    {"035720", "카카오"},
    {"035900", "JYP Ent."},
    {"036570", "NC"},
    {"041510", "에스엠"},
    {"042660", "한화오션"},
    {"047050", "포스코인터내셔널"},
    {"051900", "LG생활건강"},
    {"051910", "LG화학"},
    {"055550", "신한지주"},
    {"066570", "LG전자"},
    {"066970", "엘앤에프"},
    {"068270", "셀트리온"},
    {"086520", "에코프로"},
    {"086790", "하나금융지주"},
    {"090430", "아모레퍼시픽"},
    {"096770", "SK이노베이션"},
    {"097950", "CJ제일제당"},
    {"105560", "KB금융"},
    {"122870", "와이지엔터테인먼트"},
    {"139480", "이마트"},
    {"161390", "한국타이어"},
    {"196170", "알테오젠"},
    {"207940", "삼성바이오로직스"},
    {"247540", "에코프로비엠"},
    {"251270", "넷마블"},
    {"259960", "크래프톤"},
    {"267260", "HD현대일렉트릭"},
    {"271560", "오리온"},
    {"316140", "우리금융지주"},
    {"323410", "카카오뱅크"},
    {"329180", "HD현대중공업"},
    {"352820", "하이브"},
    {"373220", "LG에너지솔루션"},
    {"402340", "SK스퀘어"},
};
constexpr size_t kCount = sizeof(kNames) / sizeof(kNames[0]);
}  // namespace

const char* NameForCode(const char* code) {
  if (!code) return nullptr;
  size_t lo = 0, hi = kCount;
  while (lo < hi) {
    const size_t mid = lo + (hi - lo) / 2;
    const int c = strcmp(code, kNames[mid].code);
    if (c == 0) return kNames[mid].name;
    if (c < 0) hi = mid; else lo = mid + 1;
  }
  return nullptr;
}

size_t NameCount() { return kCount; }
const NameEntry& NameAt(size_t index) { return kNames[index < kCount ? index : 0]; }

}  // namespace stocks
}  // namespace kdash
