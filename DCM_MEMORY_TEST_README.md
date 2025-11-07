# DICOM Memory Test - README

이 테스트 코드는 `mqi::io::save_to_dcm` 함수의 메모리 관리 문제를 진단하기 위해 설계되었습니다.

## 📋 파일 구성

```
MOQUI_JKH/
├── test_dcm_memory.cpp           # 메인 테스트 코드
├── Makefile.test_dcm             # 빌드 설정
├── run_dcm_memory_test.sh        # 자동화된 테스트 스크립트
└── DCM_MEMORY_TEST_README.md     # 이 문서
```

## 🎯 테스트 목적

1. **메모리 누수 감지**: save_to_dcm 함수 호출 후 메모리가 제대로 해제되는지 확인
2. **메모리 corruption 감지**: "free(): invalid size" 등의 메모리 오류 재현
3. **반복 호출 안정성**: 여러 번 호출 시에도 문제없이 동작하는지 확인
4. **다양한 데이터 크기**: 작은 데이터부터 큰 데이터까지 테스트

## 🚀 빠른 시작

### 방법 1: 자동화 스크립트 사용 (권장)

```bash
# 모든 테스트 실행
./run_dcm_memory_test.sh

# 특정 테스트만 실행
./run_dcm_memory_test.sh basic      # 기본 실행
./run_dcm_memory_test.sh valgrind   # Valgrind 메모리 체크
./run_dcm_memory_test.sh asan       # AddressSanitizer
./run_dcm_memory_test.sh stress     # 스트레스 테스트
```

### 방법 2: 수동 빌드 및 실행

```bash
# 1. 빌드
make -f Makefile.test_dcm

# 2. 실행
./test_dcm_memory              # 기본 실행 (50회 반복)
./test_dcm_memory 100          # 100회 반복

# 3. Valgrind로 메모리 체크
valgrind --leak-check=full --show-leak-kinds=all ./test_dcm_memory

# 4. AddressSanitizer로 빌드 및 실행
make -f Makefile.test_dcm asan
ASAN_OPTIONS=detect_leaks=1 ./test_dcm_memory
```

## 🔍 테스트 모드 상세

### 1. Basic Test (기본 테스트)
```bash
./run_dcm_memory_test.sh basic 50
```
- 다양한 크기의 데이터로 DICOM 파일 저장
- 반복 호출을 통한 기본적인 안정성 확인
- 메모리 사용량 모니터링

### 2. Valgrind Test (메모리 누수 탐지)
```bash
./run_dcm_memory_test.sh valgrind 30
```
- 메모리 누수 및 잘못된 메모리 접근 탐지
- 상세한 메모리 오류 리포트 생성
- 결과: `test_reports/valgrind_report.txt`

**확인 항목:**
- `definitely lost`: 확실한 메모리 누수
- `indirectly lost`: 간접적 메모리 누수
- `invalid read/write`: 잘못된 메모리 접근
- `ERROR SUMMARY`: 전체 오류 요약

### 3. AddressSanitizer Test (메모리 오류 탐지)
```bash
./run_dcm_memory_test.sh asan 30
```
- 런타임 메모리 오류 탐지 (버퍼 오버플로우, use-after-free 등)
- Valgrind보다 빠른 실행 속도
- 결과: `test_reports/asan_report.*`

**탐지 가능한 오류:**
- Heap buffer overflow
- Use after free
- Double free
- Memory leak
- Invalid pointer dereference

### 4. Stress Test (스트레스 테스트)
```bash
./run_dcm_memory_test.sh stress 1000
```
- 많은 반복 호출로 간헐적 메모리 문제 재현
- 장시간 실행 안정성 확인

## 📊 테스트 결과 해석

### 성공적인 실행
```
✓ All tests passed!
VmSize: ~100 MB (안정적 유지)
VmRSS: ~50 MB (안정적 유지)
```

### 메모리 누수 발견
```
Valgrind:
  definitely lost: 1,024 bytes in 1 blocks

ASAN:
  Direct leak of 1024 byte(s) in 1 object(s)
```

### 메모리 corruption 발견
```
free(): invalid size
*** stack smashing detected ***
AddressSanitizer: heap-buffer-overflow
```

## 🐛 알려진 문제 및 해결 방법

### 문제 1: "free(): invalid size" 오류

**원인:**
- GDCM의 DataElement::SetByteValue()가 포인터만 저장
- std::vector 소멸 시 GDCM이 해제 시도 → allocator mismatch

**해결 방법:**
- gdcm::Image::SetBuffer() 사용 (데이터 복사)
- 또는 GDCM이 관리할 수 있는 메모리 할당 사용

**현재 코드 상태:**
- mqi_io.hpp의 save_to_dcm 함수는 Image API를 사용하도록 수정됨 (라인 745-905)
- 하지만 GDCM 3.0에서는 SetBuffer() API가 제거되어 직접 DataElement 설정 필요

### 문제 2: 메모리 누수

**확인 방법:**
```bash
# 반복 실행하면서 메모리 증가 확인
watch -n 1 'ps aux | grep test_dcm_memory | grep -v grep'

# 또는 테스트 내부 출력 확인
./test_dcm_memory 100
# VmRSS가 계속 증가하면 메모리 누수
```

### 문제 3: GDCM SmartPointer 사용

**현재 코드 (라인 765-767):**
```cpp
gdcm::SmartPointer<gdcm::DataElement> pixel_element = new gdcm::DataElement(...);
pixel_element->SetByteValue(...);  // 포인터 저장 - 위험!
```

**문제점:**
- SmartPointer가 DataElement를 관리하지만, SetByteValue()는 raw pointer 저장
- pixel_data vector가 먼저 소멸하면 dangling pointer 발생 가능

## 🔧 테스트 커스터마이징

### test_dcm_memory.cpp 수정

**테스트 케이스 추가:**
```cpp
// test_various_sizes() 함수에 추가
TestCase test_cases[] = {
    // ... 기존 케이스들
    {"custom_200x200x100", {200, 200, 100}, 10000},  // 커스텀 케이스
};
```

**반복 횟수 조정:**
```cpp
// main() 함수에서
int iterations = 100;  // 기본값 변경
```

### Makefile 옵션 조정

**최적화 레벨 변경:**
```makefile
CXXFLAGS = -std=c++11 -Wall -Wextra -g -O2  # -O2 추가
```

**추가 sanitizer 옵션:**
```makefile
asan: CXXFLAGS += -fsanitize=address,undefined  # undefined behavior 탐지 추가
```

## 📈 성능 측정

### 실행 시간 비교
```bash
# Normal
time ./test_dcm_memory 100

# Valgrind (매우 느림, ~50-100배)
time valgrind ./test_dcm_memory 100

# ASAN (약간 느림, ~2-3배)
time ./test_dcm_memory 100  # ASAN 빌드로
```

### 메모리 프로파일링
```bash
# Massif (힙 메모리 프로파일링)
valgrind --tool=massif ./test_dcm_memory 50
ms_print massif.out.*
```

## 🔬 디버깅 팁

### 1. GDB로 크래시 위치 확인
```bash
gdb ./test_dcm_memory
(gdb) run 10
# 크래시 발생 시
(gdb) bt        # backtrace
(gdb) info locals
(gdb) print pixel_data.size()
```

### 2. 특정 반복에서만 실패하는 경우
```cpp
// test_repeated_dcm_save에 추가
if (i == 42) {  // 문제가 되는 반복 번호
    std::cout << "Debug: About to call save_to_dcm" << std::endl;
    // breakpoint 또는 추가 로깅
}
```

### 3. GDCM 디버그 출력 활성화
```cpp
// main() 함수 시작 부분에 추가
gdcm::Trace::SetDebug(true);
gdcm::Trace::SetWarning(true);
```

## 📝 테스트 리포트 예시

### 성공 케이스
```
=== Memory Information ===
VmSize:    45678 kB
VmRSS:     23456 kB
VmPeak:    45678 kB

=== Various Size Test ===
✓ small_10x10x10 passed
✓ medium_50x50x50 passed
✓ large_100x100x50 passed
✓ sparse_100x100x100 passed

=== Repeated DICOM Save Test ===
Progress: 0/50
Progress: 10/50
Progress: 20/50
Progress: 30/50
Progress: 40/50
✓ All 50 iterations passed

Valgrind:
  ERROR SUMMARY: 0 errors from 0 contexts
  All heap blocks were freed -- no leaks are possible
```

### 실패 케이스
```
=== Repeated DICOM Save Test ===
Progress: 0/50
Progress: 10/50
free(): invalid size
Aborted (core dumped)

Valgrind:
  Invalid free() / delete / delete[] / realloc()
  Address 0x... is 0 bytes inside a block of size 1,000 alloc'd

AddressSanitizer:
  heap-use-after-free on address 0x...
  freed by thread T0 here:
    #0 in operator delete
    #1 in std::vector<>::~vector()
  previously allocated by thread T0 here:
    #0 in operator new[]
    #1 in std::vector<>::resize()
```

## 🆘 문제 해결 가이드

### 빌드 오류

**GDCM 헤더를 찾을 수 없음:**
```bash
# GDCM 설치 확인
dpkg -l | grep gdcm
locate gdcmImage.h

# Makefile 수정
INCLUDES = -I./moqui -I/usr/include/gdcm-3.0  # 경로 조정
```

**링크 오류:**
```bash
# GDCM 라이브러리 확인
ldconfig -p | grep gdcm

# 필요 시 LD_LIBRARY_PATH 설정
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

### 런타임 오류

**Segmentation fault:**
```bash
# Core dump 활성화
ulimit -c unlimited

# 크래시 후 분석
gdb ./test_dcm_memory core
```

**Permission denied (출력 디렉토리):**
```bash
mkdir -p test_dcm_output
chmod 755 test_dcm_output
```

## 📚 참고 자료

### GDCM 문서
- GDCM Wiki: http://gdcm.sourceforge.net/
- API Documentation: http://gdcm.sourceforge.net/html/

### 메모리 디버깅 도구
- Valgrind Manual: https://valgrind.org/docs/manual/
- AddressSanitizer: https://github.com/google/sanitizers/wiki/AddressSanitizer
- GDB Manual: https://sourceware.org/gdb/documentation/

### DICOM 표준
- RT Dose Storage (1.2.840.10008.5.1.4.1.1.481.2)
- DICOM Part 3: Information Object Definitions

## 🤝 기여

문제를 발견하거나 개선 사항이 있으면:
1. 이슈 생성
2. 테스트 케이스 추가
3. Pull Request 제출

---

**작성일:** 2025-11-07
**버전:** 1.0
**테스트 대상:** moqui/base/mqi_io.hpp - save_to_dcm 함수
