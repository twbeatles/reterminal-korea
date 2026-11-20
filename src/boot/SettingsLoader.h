#pragma once
// SettingsLoader (SRP: 설정 소스 결정만 담당).
// 우선순위: NVS 슬롯 -> dev_credentials.h -> 기본값.
// SOLID:
//  - DIP: 구체 NvsStorage가 아닌 IStorage 추상에만 의존.
//  - OCP: 새 설정 소스 추가는 이 모듈 안에서 확장, 호출 측은 닫힘.
//  - ISP: 필요한 최소 인터페이스(read via SlotStore)만 사용.
#include "../../include/Config.h"
#include "../../include/IStorage.h"

namespace kdash {
namespace boot {

// "nvs" | "dev" | "default" — 마지막 LoadSettings 호출의 출처.
const char* ConfigSource();
// NVS 슬롯에서 설정 복원. false => 슬롯 없음/손상.
bool LoadSettingsFromNvs(IStorage& storage, Settings& out);
// dev_credentials.h -> Settings. 헤더가 없으면 out을 건드리지 않음.
void LoadDevSettings(Settings& out);
// Boot 순서 전체: NVS -> dev -> defaults.
void LoadSettings(IStorage& storage, Settings& out);
// 라벨만 출력("등록됨/미등록"), 값은 절대 출력 금지(SEC-01).
void LogConfigRedacted(const Settings& s);

}  // namespace boot
}  // namespace kdash
