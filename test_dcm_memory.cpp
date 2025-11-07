/**
 * @file test_dcm_memory.cpp
 * @brief DICOM save 메모리 관리 문제 테스트 프로그램
 *
 * 이 테스트는 mqi::io::save_to_dcm 함수의 메모리 관리 문제를 감지하기 위해 설계되었습니다.
 *
 * 테스트 목적:
 * 1. save_to_dcm 함수 호출 시 메모리 누수/corruption 확인
 * 2. 반복 호출 시 메모리 문제 재현
 * 3. 다양한 크기의 데이터로 테스트
 *
 * 실행 방법:
 * - 일반 실행: ./test_dcm_memory
 * - Valgrind: valgrind --leak-check=full --show-leak-kinds=all ./test_dcm_memory
 * - AddressSanitizer: ASAN_OPTIONS=detect_leaks=1 ./test_dcm_memory
 *
 * 컴파일 방법:
 * g++ -std=c++11 -I./moqui -I/usr/include/gdcm-3.0 \
 *     test_dcm_memory.cpp -o test_dcm_memory \
 *     -lgdcmCommon -lgdcmDICT -lgdcmDSED -lgdcmIOD -lgdcmMSFF \
 *     -lz -fsanitize=address -g
 */

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

#include <moqui/base/mqi_io.hpp>
#include <moqui/base/mqi_scorer.hpp>
#include <moqui/base/mqi_hash_table.hpp>
#include <moqui/base/mqi_vec.hpp>
#include <moqui/base/mqi_track.hpp>
#include <moqui/base/mqi_grid3d.hpp>

// 테스트용 더미 함수
template<typename R>
double dummy_compute_hit(const mqi::track_t<R>&, const mqi::cnb_t&, mqi::grid3d<mqi::density_t, R>&) {
    return 0.0;
}

/**
 * @brief 테스트용 scorer 객체 생성
 * @param dim 3D 차원
 * @param num_entries 채울 데이터 엔트리 수
 * @return scorer 객체 포인터 (사용 후 반드시 delete 필요)
 */
template<typename R>
mqi::scorer<R>* create_test_scorer(const mqi::vec3<mqi::ijk_t>& dim, uint32_t num_entries) {
    uint32_t capacity = num_entries * 2;  // 해시 테이블 효율을 위해 2배 크기 할당

    // Scorer 생성
    auto* scorer = new mqi::scorer<R>("test_dose", capacity, dummy_compute_hit<R>);
    scorer->type_ = mqi::DOSE;
    scorer->roi_ = nullptr;

    // 데이터 할당
    scorer->data_ = new mqi::key_value[capacity];

    // 초기화
    for (uint32_t i = 0; i < capacity; i++) {
        scorer->data_[i].key1 = mqi::empty_pair;
        scorer->data_[i].key2 = mqi::empty_pair;
        scorer->data_[i].value = 0.0;
    }

    // 테스트 데이터 채우기
    uint32_t vol_size = dim.x * dim.y * dim.z;
    for (uint32_t i = 0; i < num_entries && i < capacity; i++) {
        scorer->data_[i].key1 = i % vol_size;  // voxel index
        scorer->data_[i].key2 = 0;              // spot index (단일 spot)
        scorer->data_[i].value = 1.0 + (i % 100) / 100.0;  // 1.0 ~ 1.99 범위 dose
    }

    return scorer;
}

/**
 * @brief 단일 DICOM save 테스트
 * @param test_name 테스트 이름
 * @param dim 3D 차원
 * @param num_entries 데이터 엔트리 수
 * @return 성공 여부
 */
bool test_single_dcm_save(const char* test_name,
                          const mqi::vec3<mqi::ijk_t>& dim,
                          uint32_t num_entries) {
    std::cout << "\n=== " << test_name << " ===" << std::endl;
    std::cout << "Dimension: (" << dim.x << ", " << dim.y << ", " << dim.z << ")" << std::endl;
    std::cout << "Entries: " << num_entries << std::endl;

    // Scorer 생성
    auto* scorer = create_test_scorer<float>(dim, num_entries);

    // 출력 디렉토리 생성
    std::string output_dir = "./test_dcm_output";
    mkdir(output_dir.c_str(), 0755);

    // DICOM 저장
    std::string filename = std::string(test_name);
    uint32_t length = dim.x * dim.y * dim.z;

    try {
        mqi::io::save_to_dcm(scorer, 1.0f, output_dir, filename, length, dim, false);
        std::cout << "✓ DICOM save completed" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ Exception: " << e.what() << std::endl;
        delete scorer;
        return false;
    }

    // Scorer 삭제 (메모리 해제)
    delete scorer;

    std::cout << "✓ Test passed" << std::endl;
    return true;
}

/**
 * @brief 반복 DICOM save 테스트 (메모리 누수 감지용)
 * @param iterations 반복 횟수
 * @return 성공 여부
 */
bool test_repeated_dcm_save(int iterations) {
    std::cout << "\n=== Repeated DICOM Save Test (Memory Leak Detection) ===" << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;

    mqi::vec3<mqi::ijk_t> dim = {50, 50, 50};
    uint32_t num_entries = 1000;

    for (int i = 0; i < iterations; i++) {
        if (i % 10 == 0) {
            std::cout << "Progress: " << i << "/" << iterations << std::endl;
        }

        auto* scorer = create_test_scorer<float>(dim, num_entries);

        std::string output_dir = "./test_dcm_output";
        std::string filename = "repeated_test_" + std::to_string(i);
        uint32_t length = dim.x * dim.y * dim.z;

        try {
            mqi::io::save_to_dcm(scorer, 1.0f, output_dir, filename, length, dim, false);
        } catch (const std::exception& e) {
            std::cerr << "✗ Exception at iteration " << i << ": " << e.what() << std::endl;
            delete scorer;
            return false;
        }

        delete scorer;

        // 생성된 파일 삭제 (디스크 공간 절약)
        std::string dcm_file = output_dir + "/" + filename + ".dcm";
        unlink(dcm_file.c_str());
    }

    std::cout << "✓ All " << iterations << " iterations passed" << std::endl;
    return true;
}

/**
 * @brief 다양한 크기로 DICOM save 테스트
 * @return 성공 여부
 */
bool test_various_sizes() {
    std::cout << "\n=== Various Size Test ===" << std::endl;

    struct TestCase {
        const char* name;
        mqi::vec3<mqi::ijk_t> dim;
        uint32_t entries;
    };

    TestCase test_cases[] = {
        {"small_10x10x10",   {10, 10, 10},     100},
        {"medium_50x50x50",  {50, 50, 50},     1000},
        {"large_100x100x50", {100, 100, 50},   5000},
        {"sparse_100x100x100", {100, 100, 100}, 1000},  // sparse data
    };

    for (const auto& tc : test_cases) {
        if (!test_single_dcm_save(tc.name, tc.dim, tc.entries)) {
            return false;
        }
    }

    return true;
}

/**
 * @brief 메모리 스트레스 테스트 (큰 데이터)
 * @return 성공 여부
 */
bool test_large_data() {
    std::cout << "\n=== Large Data Stress Test ===" << std::endl;

    mqi::vec3<mqi::ijk_t> dim = {256, 256, 128};  // 8M voxels
    uint32_t num_entries = 50000;  // 50K non-zero entries

    std::cout << "Total voxels: " << (dim.x * dim.y * dim.z) << std::endl;
    std::cout << "Memory size: ~" << (dim.x * dim.y * dim.z * sizeof(uint16_t) / 1024 / 1024) << " MB" << std::endl;

    return test_single_dcm_save("large_256x256x128", dim, num_entries);
}

/**
 * @brief 메모리 상태 출력 (Linux procfs 이용)
 */
void print_memory_info() {
    std::cout << "\n=== Memory Information ===" << std::endl;

    // /proc/self/status에서 메모리 정보 읽기
    FILE* fp = fopen("/proc/self/status", "r");
    if (!fp) return;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmSize:", 7) == 0 ||
            strncmp(line, "VmRSS:", 6) == 0 ||
            strncmp(line, "VmPeak:", 7) == 0) {
            std::cout << line;
        }
    }
    fclose(fp);
}

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "DICOM Save Memory Test Program" << std::endl;
    std::cout << "========================================" << std::endl;

    print_memory_info();

    bool all_passed = true;

    // 테스트 1: 다양한 크기
    if (!test_various_sizes()) {
        all_passed = false;
        std::cerr << "✗ Various sizes test failed" << std::endl;
    }

    print_memory_info();

    // 테스트 2: 반복 테스트 (메모리 누수 감지)
    int iterations = 50;
    if (argc > 1) {
        iterations = atoi(argv[1]);
    }
    if (!test_repeated_dcm_save(iterations)) {
        all_passed = false;
        std::cerr << "✗ Repeated test failed" << std::endl;
    }

    print_memory_info();

    // 테스트 3: 큰 데이터 스트레스 테스트
    if (!test_large_data()) {
        all_passed = false;
        std::cerr << "✗ Large data test failed" << std::endl;
    }

    print_memory_info();

    std::cout << "\n========================================" << std::endl;
    if (all_passed) {
        std::cout << "✓ All tests passed!" << std::endl;
        std::cout << "Note: Run with Valgrind or AddressSanitizer for memory leak detection" << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some tests failed!" << std::endl;
        return 1;
    }
}
