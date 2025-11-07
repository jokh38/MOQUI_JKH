# 🚀 DCM 메모리 테스트 빠른 시작 가이드

## 📦 1단계: GDCM 라이브러리 설치

### Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install -y libgdcm-dev libgdcm-tools
```

### macOS:
```bash
brew install gdcm
```

### 설치 확인:
```bash
# GDCM 헤더 파일 확인
ls /usr/include/gdcm*/gdcmDataElement.h

# GDCM 라이브러리 확인
ldconfig -p | grep gdcm
```

## 🔨 2단계: 테스트 빌드

```bash
cd /home/user/MOQUI_JKH

# 기본 빌드
make -f Makefile.test_dcm

# 또는 AddressSanitizer 포함 빌드 (권장)
make -f Makefile.test_dcm asan
```

## ▶️ 3단계: 테스트 실행

### 방법 1: 자동화 스크립트 (가장 쉬움)
```bash
# 모든 테스트 자동 실행
./run_dcm_memory_test.sh

# 특정 테스트만 실행
./run_dcm_memory_test.sh basic      # 기본 테스트
./run_dcm_memory_test.sh valgrind   # Valgrind 메모리 체크
./run_dcm_memory_test.sh asan       # AddressSanitizer
./run_dcm_memory_test.sh stress     # 스트레스 테스트 (1000회 반복)
```

### 방법 2: 직접 실행
```bash
# 기본 실행 (50회 반복)
./test_dcm_memory

# 반복 횟수 지정
./test_dcm_memory 100

# Valgrind로 메모리 누수 검사
valgrind --leak-check=full --show-leak-kinds=all ./test_dcm_memory 30

# AddressSanitizer로 실행
make -f Makefile.test_dcm asan
ASAN_OPTIONS=detect_leaks=1 ./test_dcm_memory 30
```

## 📊 4단계: 결과 확인

### 성공 케이스:
```
✓ All tests passed!

Valgrind 결과:
  ERROR SUMMARY: 0 errors from 0 contexts
  All heap blocks were freed -- no leaks are possible
```

### 메모리 누수 발견:
```
Valgrind:
  definitely lost: 1,024 bytes in 1 blocks
  indirectly lost: 512 bytes in 2 blocks

→ 리포트: test_reports/valgrind_report.txt 확인
```

### 메모리 Corruption 발견:
```
free(): invalid size
*** stack smashing detected ***

AddressSanitizer:
  heap-use-after-free on address 0x...

→ 리포트: test_reports/asan_report.* 확인
```

## 🐛 현재 알려진 문제

### mqi_io.hpp의 save_to_dcm 함수 (라인 619-908)

**문제점:**
```cpp
// 라인 765-767: SmartPointer 사용
gdcm::SmartPointer<gdcm::DataElement> pixel_element =
    new gdcm::DataElement(gdcm::Tag(0x7FE0, 0x0010));
pixel_element->SetByteValue(
    reinterpret_cast<const char*>(pixel_data.data()),
    pixel_data_size);  // ← 포인터만 저장, 데이터 복사 안 함!

ds.Insert(*pixel_element);
```

**문제 원인:**
1. `SetByteValue()`는 `pixel_data.data()` 포인터만 저장
2. `pixel_data` vector는 함수 끝에서 소멸
3. GDCM이 소멸 시 이미 해제된 메모리를 접근 → **Dangling Pointer**
4. "free(): invalid size" 또는 "heap-use-after-free" 발생

**해결 방법:**

#### 옵션 1: ByteValue 복사 생성 (권장)
```cpp
// SetByteValue 대신 복사본을 만들어서 사용
gdcm::DataElement pixel_element(gdcm::Tag(0x7FE0, 0x0010));
pixel_element.SetVR(gdcm::VR::OB);

// 데이터 복사본 생성
std::vector<char> pixel_copy(
    reinterpret_cast<const char*>(pixel_data.data()),
    reinterpret_cast<const char*>(pixel_data.data()) + pixel_data_size
);
pixel_element.SetByteValue(&pixel_copy[0], pixel_copy.size());

// 또는 GDCM의 ByteValue를 직접 생성
gdcm::ByteValue* bv = new gdcm::ByteValue(
    reinterpret_cast<const char*>(pixel_data.data()),
    pixel_data_size
);
pixel_element.SetValue(*bv);

ds.Insert(pixel_element);
```

#### 옵션 2: pixel_data를 함수 스코프 밖으로 이동
```cpp
// pixel_data를 함수 스코프 끝까지 유지
void save_to_dcm(...) {
    std::vector<uint16_t> pixel_data;
    // ... 데이터 준비 ...

    gdcm::File file;
    gdcm::DataSet& ds = file.GetDataSet();

    // DataElement 생성 및 삽입
    gdcm::DataElement pixel_element(gdcm::Tag(0x7FE0, 0x0010));
    pixel_element.SetByteValue(...);
    ds.Insert(pixel_element);

    // Writer 생성 및 저장
    gdcm::Writer writer;
    writer.SetFile(file);
    writer.Write();

    // pixel_data는 여기서 소멸 (Writer.Write() 이후이므로 안전)
}
```

## 📁 생성된 파일 구조

```
MOQUI_JKH/
├── test_dcm_memory.cpp           ← 메인 테스트 코드
├── Makefile.test_dcm             ← 빌드 설정
├── run_dcm_memory_test.sh        ← 자동화 스크립트
├── DCM_MEMORY_TEST_README.md     ← 상세 문서
├── TEST_QUICK_START.md           ← 이 파일
│
├── test_dcm_output/              ← 생성된 DICOM 파일 (자동 생성)
└── test_reports/                 ← 테스트 리포트 (자동 생성)
    ├── valgrind_report.txt
    └── asan_report.*
```

## 💡 유용한 명령어

### 빌드 관련
```bash
make -f Makefile.test_dcm          # 일반 빌드
make -f Makefile.test_dcm asan     # AddressSanitizer 빌드
make -f Makefile.test_dcm clean    # 클린
```

### 테스트 실행
```bash
./test_dcm_memory                  # 기본 (50회)
./test_dcm_memory 200              # 200회 반복
```

### 메모리 분석
```bash
# Valgrind 상세 분석
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file=valgrind_full.txt \
         ./test_dcm_memory 50

# AddressSanitizer 상세 옵션
ASAN_OPTIONS="detect_leaks=1:print_stats=1:atexit=1" \
    ./test_dcm_memory 50

# 메모리 사용량 모니터링
watch -n 1 'ps aux | grep test_dcm_memory | grep -v grep'
```

### GDB 디버깅
```bash
gdb ./test_dcm_memory
(gdb) run 10
# 크래시 발생 시
(gdb) bt              # 백트레이스
(gdb) frame 0         # 크래시 프레임 확인
(gdb) print pixel_data.size()
(gdb) info locals    # 로컬 변수 확인
```

## 🎯 다음 단계

### 문제가 발견되면:
1. **리포트 확인**: `test_reports/` 디렉토리의 상세 리포트 분석
2. **코드 수정**: `moqui/base/mqi_io.hpp`의 `save_to_dcm` 함수 수정
3. **재테스트**: 수정 후 다시 테스트 실행
4. **커밋**: 문제 해결 후 git commit

### 문제가 없으면:
1. 더 많은 반복으로 재테스트 (1000회 이상)
2. 더 큰 데이터로 스트레스 테스트
3. 다양한 입력 케이스 추가

## 📞 도움말

### 더 자세한 정보:
```bash
cat DCM_MEMORY_TEST_README.md    # 전체 문서
./run_dcm_memory_test.sh help    # 스크립트 도움말
```

### 문제 발생 시:
1. GDCM 설치 확인: `dpkg -l | grep gdcm`
2. 헤더 파일 확인: `find /usr -name "gdcmDataElement.h"`
3. 라이브러리 확인: `ldconfig -p | grep gdcm`

---

**생성 날짜**: 2025-11-07
**목적**: mqi_io.hpp save_to_dcm 메모리 문제 진단
