#pragma once
// SymbolBatch: 여러 종목을 차례로 조회하는 공급자(KIS 현재가, FSC 종가)의 결과 집계 규칙.
// 두 공급자가 같은 규칙을 쓰도록 한 곳에 모은다(ADR 0026, 감사 ISSUE-009·G-6).
//
// 규칙
//  - 종목 하나의 실패(NoData, ProviderError)는 그 종목만 실패로 표시하고 다음 종목을 계속 조회한다.
//    (잘못 입력했거나 상장폐지된 코드 하나가 나머지 시세를 막지 않게.)
//  - 호출 보호 오류(AuthError, RateLimited, QuotaExceeded)나 전송 실패(DNS/TLS/시간 초과/예산 소진)는
//    즉시 멈춘다. 공급자 결과는 그 오류다(백오프·차단 정책이 그대로 적용된다).
//  - 멈추지 않았고 한 종목이라도 성공하면 공급자 결과는 Ok(일부 실패는 fail_mask로 따로 보여 준다).
//    모두 실패하면 첫 실패 결과.
#include <stddef.h>
#include <stdint.h>

#include "../domain/Types.h"

namespace kdash {
namespace app {

// 이 결과가 남은 종목 조회를 멈춰야 하는가.
bool StopsBatch(Outcome o, bool transport_error);

struct SymbolBatch {
  int ok = 0;
  int failed = 0;
  uint8_t fail_mask = 0;  // bit i: i번째 종목 실패(멈춤 원인은 포함하지 않음)
  Outcome first_failure = Outcome::Ok;
  bool stopped = false;
  Outcome stop_outcome = Outcome::Ok;

  // 종목 index의 결과를 넣는다. true를 돌려주면 더 요청하지 않는다.
  bool Add(size_t index, Outcome o, bool transport_error);
  // 종목과 무관한 이유(토큰 발급 실패 등)로 멈춘다.
  void Stop(Outcome o);
  // 공급자 결과(MarkResult에 넘길 값).
  Outcome Result() const;
};

}  // namespace app
}  // namespace kdash
