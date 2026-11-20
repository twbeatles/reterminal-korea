# T02 — 영속 저장: NVS 설정, 원자 쓰기, 스냅샷·상태 보존

- 상태: DONE_VERIFIED_HOST + BUILD (실기 전원차단만 BLOCKED_HW)
- 선행: 없음
- 필요 자원: 없음 (실기는 최종 확인용)
- 관련: 감사 G-2, docs/02 "캐시 정책", docs/04 "Settings 구조", docs/11, CACHE-01

## 1. 목표

1. 설정(`Settings`)을 NVS에 버전이 붙은 형식으로 저장하고, 부팅할 때 검증·마이그레이션해 불러옵니다.
2. 쓰기는 **원자적**이어야 합니다. 쓰는 도중 전원이 끊겨도 마지막 정상본이 남아야 합니다.
3. 마지막 정상 스냅샷(`app::Snapshot`)과 제공자 상태(`ProviderStatus[]`)를 다음 경로로 보존합니다.
   - deep sleep 동안: RTC 메모리(T05)
   - 전원 차단 후: NVS(빈도 제한)

## 2. 현재 상태

- `include/IStorage.h`: `read / write_atomic / erase` 인터페이스만 있고 구현체가 없습니다.
- `src/storage/CacheStore.*`: 헤더(magic, schema, kind, observed, fetched, len, crc32)+페이로드 프레이밍만 있습니다(검증 완료).
- `src/main.cpp`: `dev_credentials.h`에서만 설정을 읽습니다.
- 측정된 크기(ESP32): `Settings` 1,560B, `Snapshot` 1,880B, `ProviderStatus[7]` 336B, `KisSession` 1,040B.

## 3. 설계

### 3-1. 저장 키 구조 (NVS namespace `kdash`)

| NVS 키 | 내용 | 쓰기 빈도 |
|---|---|---|
| `set.a` / `set.b` | 설정 슬롯 A/B (CacheStore 프레이밍, kind=1) | 포털 저장 시에만 |
| `snap.a` / `snap.b` | 스냅샷 슬롯 A/B (kind=2) | 값이 바뀌었을 때, **최소 30분 간격** |
| `meta` | 설정·스냅샷 각각의 활성 슬롯 번호 + 세대 카운터 | 위와 동시 |

### 3-2. 원자 쓰기 (dual-slot)

```text
write(kind, payload):
  gen = active.gen + 1
  target = (active.slot == A) ? B : A
  blob = CacheEncode(payload, kind, observed, now) + gen(헤더 확장 또는 payload 앞 4B)
  nvs_set_blob(target, blob); nvs_commit()
  # 커밋이 끝난 뒤에야 meta를 바꾼다. 둘 사이에 전원이 끊기면 이전 슬롯이 그대로 유효하다.
  meta.active = target, meta.gen = gen; nvs_commit()

read(kind):
  두 슬롯을 모두 CacheDecode. 유효한 것 중 gen이 큰 것을 선택.
  둘 다 무효 → 기본값 + "설정 필요" (CACHE-01)
```

- meta 자체가 깨져도 슬롯의 gen 비교만으로 복구할 수 있게 합니다(meta는 힌트일 뿐).
- `CacheHeader`에 `gen` 필드를 넣으면 `kCacheSchemaVersion`을 2로 올리고, v1 blob은 읽기 거부 → 기본값으로 처리합니다(마이그레이션 대상 실데이터가 아직 없음).

### 3-3. 직렬화 형식

- **포인터를 저장하지 않습니다.** `DataPoint::source`는 `const char*`이므로 저장 시 `uint8_t source_id`(enum: KMA=1, AIR=2, EXIM=3, FSC=4, KIS=5)로 바꾸고, 읽을 때 정적 문자열로 복원합니다.
- 구조체를 `memcpy`로 덤프하지 말고 필드 단위의 명시적 직렬화를 씁니다(`src/storage/Codec.{h,cpp}`).
  - 리틀엔디언 고정 폭 정수, 길이 접두 문자열
  - 이유: 패딩·컴파일러 차이 때문에 host와 device의 덤프가 같다는 보장이 없습니다.
- 설정 페이로드: `schema_version(u16)`, location, wifi, providers(키는 길이+바이트), watchlist(count + 항목), flags.
- 마이그레이션 함수 `MigrateSettings(int from, Reader&, Settings&) -> bool`. 모르는 버전이면 false → 기본값 + 포털 플래그.

### 3-4. 파일 배치

| 파일 | 역할 | 대상 |
|---|---|---|
| `src/storage/Codec.{h,cpp}` | 바이트 Reader/Writer (경계 검사) | 이식 |
| `src/storage/SettingsCodec.{h,cpp}` | Settings ↔ bytes, 마이그레이션 | 이식 |
| `src/storage/SnapshotCodec.{h,cpp}` | Snapshot + ProviderStatus ↔ bytes | 이식 |
| `src/storage/SlotStore.{h,cpp}` | dual-slot 알고리즘 (IStorage 위에서) | 이식 |
| `src/storage/NvsStorage.{h,cpp}` | `IStorage` 구현 (`Preferences` 또는 `nvs_*` API) | 기기 전용 |
| `test_native/` | `FakeStorage`(쓰기 중단 주입 가능) | 테스트 |

`FakeStorage`는 "N번째 write에서 앞 k바이트만 기록하고 실패"를 주입할 수 있게 만듭니다(정전 시뮬레이션).

### 3-5. 쓰기 빈도 제한 (flash wear)

- 스냅샷 NVS 쓰기 조건: 값이 바뀜 && 마지막 쓰기 후 30분 이상 경과 && 배터리 모드이면 deep sleep 진입 직전에만.
- 하루 쓰기 횟수 상한을 상수로 둡니다(예: 48회). 이를 넘으면 RTC 메모리에만 유지합니다.

## 4. 구현 단계

1. `Codec` + 왕복 테스트 (경계·잘린 입력·과대 길이 거부)
2. `SettingsCodec` + 마이그레이션 테스트 (v2 왕복, 모르는 버전 → false, 잘린 blob → false)
3. `SlotStore` + `FakeStorage` 정전 주입 테스트 (아래 5절)
4. `SnapshotCodec` (source_id 복원 포함) + 왕복 후 `RefreshFreshness` 결과 동일성 테스트
5. `NvsStorage` 기기 어댑터 + `main.cpp` 연결
   - 부팅 순서: `LoadSettings`(NVS) → 없으면 `dev_credentials.h`(개발용) → 없으면 기본값
   - 설정 출처를 UART에 라벨로만 표시합니다(`config: source=nvs|dev|default`).
6. `pio run` 빌드, `docs/12` P2-03 갱신

## 5. 테스트 (최소)

| 케이스 | 입력 | 기대 |
|---|---|---|
| 왕복 | 모든 필드를 채운 Settings (키 256B, 한글 64B) | 저장→로드 결과가 바이트 단위로 동일 |
| 정전 1 | 슬롯 B에 쓰는 중 50%만 기록 | 로드 결과 = 이전 A 내용 |
| 정전 2 | B 커밋 후 meta 갱신 전 중단 | 로드 결과 = 새 B (gen 비교) |
| 비트 반전 | 활성 슬롯 1비트 변조 | 다른 슬롯 사용, 둘 다 깨지면 기본값 + `needs_portal=true` |
| 스키마 | schema 99 | 기본값, 크래시 없음 |
| 포인터 | 스냅샷 왕복 후 `source` | 정적 문자열과 같은 내용 (`strcmp`) |
| 빈도 | 10분 간격 6회 저장 요청 | 실제 NVS 쓰기 ≤ 1회 |
| 비밀 | Settings blob의 UART 출력 경로 | 없음 (SEC-01) |

## 6. 완료 기준

- [ ] 위 테스트가 네이티브 테스트로 통과, 펌웨어 빌드 성공
- [ ] 정전 주입 100회 반복(무작위 중단 지점)에서 손상·크래시 0
- [ ] 실기: 저장 직후 전원 차단 10회 → 부팅 시 설정 유지 (BLOCKED_HW 전까지 미검증 표기)

## 7. 주의

- NVS 전체 erase(`nvs_flash_erase`)는 공장초기화 2단계 확인 경로(T07)에서만 호출합니다. 에이전트가 직접 실행하지 않습니다.
- NVS 암호화는 별도 승인 사항입니다(docs/11). 이 과제에서 켜지 않습니다.
