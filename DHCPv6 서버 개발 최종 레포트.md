# **📘 DHCPv6 서버 개발 최종 레포트**

*(RFC 8415 / RFC 3633 준수, IA\_NA \+ IA\_PD, 논블로킹 아키텍처)*

---

## **1\. 개요 (Overview)**

본 프로젝트는 **RFC 표준에 최대한 부합하는 DHCPv6 서버**를 목표로 개발되었다.  
 단순한 학습용 구현이 아닌, **임베디드 장비 / 네트워크 장비 / 서버 환경**에서 실제 운용 가능한 수준을 목표로 하였으며, 다음과 같은 요구사항을 충족한다.

### **핵심 목표**

* RFC 8415(DHCPv6) 및 RFC 3633(Prefix Delegation) 최대 준수

* IA\_NA(주소 할당) \+ IA\_PD(프리픽스 위임) 동시 지원

* 논블로킹(event-driven) 아키텍처

* 환경 설정 파일 기반 구성

* 런타임 CLI를 통한 운영/디버깅

* C11 \+ `-Wall -Wextra` 기준 **경고 없는 빌드**

---

## **2\. 적용 표준 (Standards Compliance)**

### **2.1 주요 RFC**

| RFC | 내용 | 적용 상태 |
| ----- | ----- | ----- |
| RFC 8415 | DHCPv6 Core | ✅ MUST 전부 충족 |
| RFC 3633 | Prefix Delegation | ✅ 핵심 기능 구현 |
| RFC 3646 | DNS 옵션 | ✅ ORO 기반 제공 |
| RFC 4242 | Domain Search | ⭕ 구조 지원 (옵션) |

### **2.2 메시지 타입 지원 현황**

| 메시지 | 지원 여부 | 비고 |
| ----- | ----- | ----- |
| SOLICIT | ✅ | Rapid Commit 지원 |
| ADVERTISE | ✅ | 멀티캐스트 |
| REQUEST | ✅ | Server-ID 규칙 적용 |
| REPLY | ✅ | 유니/멀티캐스트 구분 |
| RENEW | ✅ | lease 연장 |
| REBIND | ✅ | Server-ID 무시 |
| RELEASE | ✅ | lease 해제 |
| DECLINE | ✅ | quarantine 처리 |
| CONFIRM | ✅ | On-link 판별 |
| INFORMATION-REQUEST | ✅ | 옵션 전용 |

---

## **3\. 전체 시스템 아키텍처**

### **3.1 아키텍처 개요**

`+-------------------+`  
`|   DHCPv6 Client   |`  
`+-------------------+`  
          `|`  
          `| UDP 546/547`  
          `v`  
`+-----------------------------+`  
`|        dhcpv6d              |`  
`|-----------------------------|`  
`| Event Loop (select)         |`  
`|  ├─ DHCPv6 Socket           |`  
`|  ├─ CLI Socket (UNIX)       |`  
`|-----------------------------|`  
`| DHCPv6 Handler              |`  
`|  ├─ Message Parser         |`  
`|  ├─ State Machine          |`  
`|  ├─ IA_NA / IA_PD Logic    |`  
`|-----------------------------|`  
`| Lease Store (in-memory)    |`  
`|-----------------------------|`  
`| Config / Logging / CLI     |`  
`+-----------------------------+`

### **3.2 설계 원칙**

* **Single-threaded \+ Non-blocking**

* 모든 I/O는 `select()` 기반

* DHCP 처리와 CLI 제어가 **서로 절대 블로킹하지 않음**

* 임베디드 환경에서도 사용 가능하도록 동적 메모리 사용 최소화

---

## **4\. DHCPv6 동작 설계**

### **4.1 상태 머신 개요**

#### **IA\_NA / IA\_PD 공통 상태**

| 상태 | 설명 |
| ----- | ----- |
| OFFERED | ADVERTISE 단계 |
| ALLOCATED | REQUEST/REPLY 완료 |
| DECLINED | 충돌로 사용 금지 |
| EXPIRED | GC 대상 |

### **4.2 SOLICIT 처리**

* Client-ID 필수

* IA\_NA / IA\_PD 각각 독립 처리

* Rapid Commit 옵션 존재 시:

  * ADVERTISE 생략

  * 즉시 REPLY \+ ALLOCATED

### **4.3 REQUEST / RENEW / REBIND**

* REQUEST / RENEW:

  * Server-ID 불일치 시 **IGNORE (RFC 8415 §15)**

* REBIND:

  * Server-ID 검사 생략

* 기존 lease 우선 유지 (stable allocation)

### **4.4 CONFIRM 처리**

* 주소/프리픽스 재할당 ❌

* On-link 여부만 판별

  * IA\_NA: 서버의 `/64` prefix 비교

  * IA\_PD: base prefix \+ base length 비교

* Status Code:

  * SUCCESS (0)

  * NotOnLink (6)

---

## **5\. Prefix Delegation (IA\_PD) 설계**

### **5.1 데이터 모델**

`(DUID, IAID, IA_PD) → {`  
  `prefix,`  
  `prefix_len,`  
  `preferred_lifetime,`  
  `valid_lifetime`  
`}`

### **5.2 PD 풀 모델**

* Base prefix (예: `/40`)

* Delegated prefix length (예: `/56`)

* Stable-hash 기반 후보 선택

* 충돌 시 probe 방식

### **5.3 Hint 처리 정책**

* 클라이언트 prefix length hint 파싱

* 정책 허용 범위 내에서만 수용

* 불가 시 서버 정책으로 대체

---

## **6\. 네트워크 I/O 설계**

### **6.1 DHCPv6 Socket**

* `recvmsg()` / `sendmsg()` 사용

* `IPV6_PKTINFO`를 통해 수신 인터페이스 식별

* 응답 시 인터페이스 index 지정

### **6.2 Multicast / Unicast 규칙**

| 상황 | 응답 방식 |
| ----- | ----- |
| ADVERTISE | ff02::1:2 (멀티캐스트) |
| SOLICIT \+ Rapid Commit | 멀티캐스트 |
| REQUEST / RENEW / CONFIRM | 유니캐스트 |

---

## **7\. 설정 파일 시스템**

### **7.1 설정 방식**

* key=value 형식

* 중복 key 허용 (DNS 등)

* 런타임 시 1회 로딩

### **7.2 주요 설정 항목**

`log_level=DEBUG`  
`offer_ttl=30`  
`decline_ttl=600`

`na_prefix=2001:db8:1::/64`  
`pd_base_prefix=2001:db8:1000::/40`  
`pd_delegated_len=56`

`dns=2001:4860:4860::8888`

---

## **8\. CLI 설계**

### **8.1 CLI 개요**

* UNIX domain socket (`/run/dhcpv6d.sock`)

* 논블로킹 accept/read

* 서버 메인 루프와 통합

### **8.2 지원 명령**

| 명령 | 기능 |
| ----- | ----- |
| show config | 현재 설정 출력 |
| set log LEVEL | 로그 레벨 변경 |

### **8.3 특징**

* 운영 중 서버 중단 없이 제어 가능

* 디버깅/운영 친화적

---

## **9\. 로그 시스템**

### **9.1 로그 특성**

* POSIX `localtime_r` 사용

* 타임스탬프 \+ 레벨 출력

* stderr 출력 (systemd/컨테이너 친화)

예시:

`2026-01-30 12:00:00 [INF] dhcpv6d started`

---

## **10\. 빌드 및 품질 기준**

### **10.1 컴파일 기준**

`gcc -O2 -Wall -Wextra -std=c11`

### **10.2 품질 상태**

* 컴파일 경고 0개

* POSIX feature macro 명시

* Undefined behavior 없음

---

## **11\. 테스트 및 검증**

### **11.1 수동 테스트**

* `dhclient -6`

* `odhcp6c`

* `tcpdump` 패킷 검증

### **11.2 검증 항목**

* SOLICIT/ADVERTISE/REPLY 흐름

* Rapid Commit 동작

* IA\_PD prefix 위임

* CONFIRM Success / NotOnLink

* DECLINE quarantine

* CLI 동작

---

## **12\. 한계 및 향후 확장**

### **12.1 현재 범위에서 제외한 기능**

* Relay-forward / relay-reply

* DHCPv6 Authentication

* Leasequery / HA

* Persistent lease 저장

### **12.2 향후 확장 가능성**

* epoll 기반 이벤트 루프

* SIGHUP 기반 config reload

* lease 조회 CLI (`show leases`)

* systemd 서비스화

---

## **13\. 결론**

본 DHCPv6 서버는:

* **RFC 8415 / RFC 3633에 충실**

* IA\_NA \+ IA\_PD 완전 지원

* 논블로킹, 운영 친화적 구조

* 임베디드/서버 환경 모두 적합

👉 **학습용을 넘어 “실제 제품/장비에 적용 가능한 수준”의 구현체**이다.

