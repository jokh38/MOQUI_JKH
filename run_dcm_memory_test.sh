#!/bin/bash

# DICOM Memory Test Runner Script
# 이 스크립트는 다양한 메모리 분석 도구로 테스트를 실행합니다.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "========================================="
echo "DICOM Memory Test Runner"
echo "========================================="
echo ""

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_section() {
    echo ""
    echo -e "${BLUE}=========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}=========================================${NC}"
    echo ""
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# 빌드 확인
if [ ! -f "./test_dcm_memory" ]; then
    print_warning "Test binary not found. Building..."
    make -f Makefile.test_dcm
fi

# 출력 디렉토리 생성
mkdir -p test_dcm_output
mkdir -p test_reports

# 테스트 모드 선택
MODE=${1:-"all"}
ITERATIONS=${2:-"30"}

case $MODE in
    "basic"|"1")
        print_section "Test 1: Basic Execution"
        ./test_dcm_memory $ITERATIONS
        print_success "Basic test completed"
        ;;

    "valgrind"|"2")
        print_section "Test 2: Valgrind Memory Check"
        if ! command -v valgrind &> /dev/null; then
            print_error "Valgrind not found. Install with: sudo apt-get install valgrind"
            exit 1
        fi

        print_warning "Running Valgrind (this may take a while)..."
        valgrind --leak-check=full \
                 --show-leak-kinds=all \
                 --track-origins=yes \
                 --verbose \
                 --log-file=test_reports/valgrind_report.txt \
                 ./test_dcm_memory $ITERATIONS

        print_success "Valgrind test completed"
        echo "Report saved to: test_reports/valgrind_report.txt"
        echo ""
        echo "Summary:"
        grep -E "ERROR SUMMARY|definitely lost|indirectly lost|possibly lost" test_reports/valgrind_report.txt || true
        ;;

    "asan"|"3")
        print_section "Test 3: AddressSanitizer Check"
        print_warning "Rebuilding with AddressSanitizer..."
        make -f Makefile.test_dcm asan

        print_warning "Running with AddressSanitizer..."
        ASAN_OPTIONS="detect_leaks=1:log_path=test_reports/asan_report:print_stats=1" \
            ./test_dcm_memory $ITERATIONS

        print_success "AddressSanitizer test completed"
        if ls test_reports/asan_report.* 1> /dev/null 2>&1; then
            echo "Report saved to: test_reports/asan_report.*"
            echo ""
            echo "Summary:"
            cat test_reports/asan_report.* 2>/dev/null || true
        else
            print_success "No memory issues detected by AddressSanitizer!"
        fi
        ;;

    "stress"|"4")
        print_section "Test 4: Stress Test (Many Iterations)"
        STRESS_ITERATIONS=${2:-"1000"}
        print_warning "Running $STRESS_ITERATIONS iterations..."
        ./test_dcm_memory $STRESS_ITERATIONS
        print_success "Stress test completed"
        ;;

    "all"|"5")
        print_section "Running All Tests"

        # Test 1: Basic
        print_section "Test 1/4: Basic Execution"
        ./test_dcm_memory 20
        print_success "Basic test passed"

        # Test 2: Valgrind (if available)
        if command -v valgrind &> /dev/null; then
            print_section "Test 2/4: Valgrind Check"
            print_warning "Running Valgrind with reduced iterations..."
            valgrind --leak-check=full \
                     --show-leak-kinds=all \
                     --log-file=test_reports/valgrind_report.txt \
                     ./test_dcm_memory 10

            print_success "Valgrind test completed"
            echo "Report: test_reports/valgrind_report.txt"
            grep -E "ERROR SUMMARY|definitely lost|indirectly lost" test_reports/valgrind_report.txt || true
        else
            print_warning "Skipping Valgrind (not installed)"
        fi

        # Test 3: AddressSanitizer
        print_section "Test 3/4: AddressSanitizer Check"
        make -f Makefile.test_dcm asan
        ASAN_OPTIONS="detect_leaks=1:log_path=test_reports/asan_report" \
            ./test_dcm_memory 10
        print_success "AddressSanitizer test completed"

        # Test 4: Stress test
        print_section "Test 4/4: Stress Test"
        make -f Makefile.test_dcm  # Rebuild without ASAN for speed
        ./test_dcm_memory 100
        print_success "Stress test completed"

        print_section "All Tests Completed!"
        print_success "Check test_reports/ directory for detailed reports"
        ;;

    "clean")
        print_section "Cleaning Test Artifacts"
        make -f Makefile.test_dcm clean
        rm -rf test_reports
        print_success "Cleaned"
        ;;

    "help"|"-h"|"--help")
        echo "Usage: $0 [mode] [iterations]"
        echo ""
        echo "Modes:"
        echo "  basic, 1        - Basic execution test (default: 30 iterations)"
        echo "  valgrind, 2     - Run with Valgrind memory checker"
        echo "  asan, 3         - Run with AddressSanitizer"
        echo "  stress, 4       - Stress test with many iterations (default: 1000)"
        echo "  all, 5          - Run all tests (default)"
        echo "  clean           - Clean build and test artifacts"
        echo "  help            - Show this help"
        echo ""
        echo "Examples:"
        echo "  $0                      # Run all tests"
        echo "  $0 basic 50             # Run basic test with 50 iterations"
        echo "  $0 valgrind 20          # Run Valgrind with 20 iterations"
        echo "  $0 stress 2000          # Stress test with 2000 iterations"
        ;;

    *)
        print_error "Unknown mode: $MODE"
        echo "Run '$0 help' for usage information"
        exit 1
        ;;
esac

echo ""
print_section "Test Run Complete"
