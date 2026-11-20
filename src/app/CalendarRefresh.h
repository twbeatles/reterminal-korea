#pragma once
// CalendarRefresh (SRP: 공휴일 연 테이블 갱신만 담당).
// SOLID:
//  - DIP: IHttpClient/IStorage/IFetchControl 추상에만 의존.
//  - SRP: 재시도 백오프 상태는 이 모듈이 단일 소유(RTC 보존).
//  - OCP: 스케줄 정책 변경은 호출자(CycleRunner) 수정 없이 이 모듈 확장.
#include "../../include/IHttpClient.h"
#include "../../include/IStorage.h"
#include "../app/Fetcher.h"
#include "../domain/Types.h"

namespace kdash {
namespace app {

// 올해·내년 테이블 중 갱신이 필요한 해를 최대 2회 시도.
// budget.CanRequest()가 false면 즉시 중단(다음 사이클로 연기, 실패 아님).
void RefreshCalendar(AppState& state, IHttpClient& http, IStorage& storage,
                     const FetchContext& ctx, IFetchControl& budget);
// 설정 변경·연도 정렬 시 재시도 상태 초기화(RtcPersistence에서 호출).
void ResetCalendarRetry();
// 포털 재저장 후 인증 오류로 대기 중인 공휴일 재시도만 즉시 해제(ClearAuthBlocks에서 호출, ISSUE-012).
void ClearCalendarAuthBlocks();

}  // namespace app
}  // namespace kdash
