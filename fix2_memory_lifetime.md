# Fix 2: DICOM Memory Error - String Lifetime Issues

## 날짜
2025-11-06

## 문제 분석

### 이전 세션에서 해결된 문제
- ✅ 파일 크기 문제 (68MB → 320KB)
- ✅ DICOM 태그 누락 (SamplesPerPixel, PhotometricInterpretation)
- ✅ Dimension 불일치

### 여전히 남아있던 문제
```
free(): invalid pointer
```
- DCM 파일은 정상적으로 저장됨
- 저장 후 cleanup 단계에서 크래시 발생
- `pixel_buffer` 수정으로 일부 문제 해결했지만 다른 메모리 문제 존재

## 근본 원인

### String Lifetime 문제 (Dangling Pointer)

`save_to_dcm` 함수에서 여러 `std::string` 객체들이 **임시로 생성**되고 `.c_str()`를 통해 GDCM DataElement에 전달되었습니다.

문제가 되는 패턴:
```cpp
// ❌ 잘못된 방법 - 임시 string이 scope를 벗어나면 포인터가 무효화됨
std::string temp_str = some_function();
element.SetByteValue(temp_str.c_str(), temp_str.length());
// temp_str이 소멸되면 GDCM이 보유한 포인터가 dangling pointer가 됨
```

### 문제가 발생한 위치들

1. **Line 686**: `std::string sop_instance_uid = uid_generator.Generate();`
   - 함수 중간에서 생성되어 scope 문제 가능성

2. **Line 733**: `std::string pixel_spacing_str = pixel_spacing_stream.str();`
   - 임시 string이 즉시 SetByteValue에 전달

3. **Line 742**: `std::string image_pos_str = image_pos_stream.str();`
   - 임시 string 생성

4. **Line 774**: `std::string dose_grid_str = dose_grid_stream.str();`
   - 중간에 생성된 string

5. **Line 836**: `std::string frames_str = std::to_string(dim.z);`
   - for loop 내부에서 생성

6. **Line 842**: `char* pixel_buffer = new char[pixel_data_size];`
   - 불필요한 수동 메모리 관리

## 해결 방법

### 핵심 아이디어
모든 string 객체를 **함수 시작 부분에서 생성**하여 `writer.Write()`가 완료될 때까지 살아있도록 보장

### 변경사항

#### 1. 모든 String을 함수 시작에서 선언
```cpp
template<typename R>
void
mqi::io::save_to_dcm(...) {
    // ✅ 모든 string을 먼저 생성 (함수 전체 lifetime 보장)
    gdcm::UIDGenerator uid_generator;
    std::string sop_instance_uid = uid_generator.Generate();
    std::string study_instance_uid = uid_generator.Generate();
    std::string series_instance_uid = uid_generator.Generate();

    std::ostringstream pixel_spacing_stream;
    pixel_spacing_stream << std::fixed << std::setprecision(6) << "1.0\\1.0";
    std::string pixel_spacing_str = pixel_spacing_stream.str();

    std::ostringstream image_pos_stream;
    image_pos_stream << std::fixed << std::setprecision(6) << "0.0\\0.0\\0.0";
    std::string image_pos_str = image_pos_stream.str();

    std::string frames_str = std::to_string(dim.z);

    // dose_grid_str도 미리 계산 (max_dose 계산 후)
    double dose_grid_scaling = (max_dose > 0) ? 1.0 / scale_factor : 1.0;
    std::ostringstream dose_grid_stream;
    dose_grid_stream << std::fixed << std::setprecision(10) << dose_grid_scaling;
    std::string dose_grid_str = dose_grid_stream.str();

    // ... 나머지 DICOM 생성 로직 ...

    // 모든 string이 여기까지 살아있음
    bool write_success = writer.Write();

    // 함수 종료시 모든 string이 자동으로 정리됨
}
```

#### 2. 불필요한 pixel_buffer 제거
```cpp
// ❌ 이전 코드
char* pixel_buffer = new char[pixel_data_size];
std::memcpy(pixel_buffer, pixel_data.data(), pixel_data_size);
pixel_data_elem.SetByteValue(pixel_buffer, pixel_data_size);
// ...
delete[] pixel_buffer; // 메모리 관리 복잡

// ✅ 개선된 코드
// SetByteValue가 데이터를 복사하므로 vector를 직접 사용 가능
pixel_data_elem.SetByteValue(
    reinterpret_cast<const char*>(pixel_data.data()),
    pixel_data_size
);
// pixel_data vector는 함수 끝까지 자동으로 유지됨
```

## 수정된 파일

- `/home/user/MOQUI_JKH/moqui/base/mqi_io.hpp`
  - `save_to_dcm` 함수 (lines 623-878)

## 메모리 안정성 보장

### Before (문제 있는 코드)
```
함수 시작
  ↓
GDCM 객체 생성
  ↓
string 1 생성 → SetByteValue(string1.c_str()) → string1 소멸 ❌
  ↓
string 2 생성 → SetByteValue(string2.c_str()) → string2 소멸 ❌
  ↓
writer.Write() ← dangling pointers! 💥
```

### After (안전한 코드)
```
함수 시작
  ↓
모든 string 생성 (string1, string2, ...)
  ↓
GDCM 객체 생성
  ↓
SetByteValue(string1.c_str()) ← string1 여전히 살아있음 ✅
SetByteValue(string2.c_str()) ← string2 여전히 살아있음 ✅
  ↓
writer.Write() ← 모든 string이 유효함 ✅
  ↓
함수 종료 (자동으로 모든 string 정리)
```

## 테스트 방법

### 1. 빌드
```bash
cd /home/user/MOQUI_JKH/tps_env
rm -rf build CMakeCache.txt CMakeFiles
mkdir build && cd build
cmake ..
make
```

### 2. 실행 및 메모리 체크
```bash
# 정상 실행
./tps_env <config_file>

# Valgrind로 메모리 에러 체크 (권장)
valgrind --leak-check=full --show-leak-kinds=all ./tps_env <config_file>
```

### 3. 예상 결과
- ✅ `free(): invalid pointer` 에러 사라짐
- ✅ DCM 파일 정상 생성 (320KB)
- ✅ Valgrind에서 메모리 에러 없음
- ✅ 프로그램이 정상 종료

## 추가 개선 사항

### GDCM의 SetByteValue 동작
GDCM의 `SetByteValue`는 데이터를 **복사**하므로:
- String lifetime이 보장되면 안전
- 하지만 일부 GDCM 버전에서는 참조만 저장할 수 있으므로 주의 필요
- 이번 수정으로 모든 경우를 커버

### C++ Best Practice
1. **RAII (Resource Acquisition Is Initialization)** 활용
   - 모든 리소스를 자동 관리되는 객체로 처리
   - `std::vector`, `std::string` 사용 (manual new/delete 제거)

2. **Lifetime 명확화**
   - 모든 데이터가 사용되는 동안 살아있도록 보장
   - 함수 시작에서 필요한 모든 string 생성

3. **메모리 안전성**
   - Dangling pointer 방지
   - Use-after-free 방지

## 관련 커밋

- 이전: `e89bf48` - Add missing DICOM tags and fix memory corruption in DCM save
- 이전: `5e7fc49` - Fix DCM memory allocation issues and 2cm mode file size
- 현재: String lifetime issues 수정

## References

- GDCM Documentation: https://gdcm.sourceforge.net/
- C++ String Lifetime: https://en.cppreference.com/w/cpp/string/basic_string
- DICOM RT Dose: http://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_A.18.html
