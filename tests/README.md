# Unit Tests

This directory contains unit tests for jm-raid-status components.

## Running Tests

```bash
# Run all tests
make tests

# Build main program and tests
make all tests

# Clean and rebuild everything including tests
make clean && make && make tests
```

## Test Framework

We use a lightweight custom test framework (`test_framework.h`) that provides:
- `ASSERT_TRUE(expr, msg)` - Assert expression is true
- `ASSERT_FALSE(expr, msg)` - Assert expression is false  
- `ASSERT_EQ(a, b, msg)` - Assert values are equal
- `ASSERT_NEQ(a, b, msg)` - Assert values are not equal
- `ASSERT_STR_EQ(a, b, msg)` - Assert strings are equal
- `ASSERT_MEM_EQ(a, b, len, msg)` - Assert memory regions are equal

### Writing Tests

```c
#include "test_framework.h"
#include "../src/module_to_test.h"

void test_something(void) {
    TEST_CASE("Description of what you're testing");
    
    // Your test code
    int result = function_to_test();
    
    ASSERT_EQ(result, expected_value, "Should return expected value");
}

int main(void) {
    TEST_SUITE("Module Name Tests");
    
    test_something();
    // ... more tests
    
    TEST_SUMMARY();  // Returns exit code 0 or 1
}
```

## Current Test Coverage

- **test_crc.c** - JMicron CRC calculation tests
  - Empty buffer handling
  - Known values
  - Consistency/determinism
  - Different lengths

- **test_xor.c** - SATA XOR scrambling tests
  - Reversibility (double XOR = identity)
  - Zero buffer handling
  - Pattern preservation

## Adding New Tests

1. Create `test_<module>.c` in this directory
2. Include `test_framework.h`
3. Write test functions
4. Add to `main()` with `TEST_SUITE` and `TEST_SUMMARY`
5. Run `make tests` - it will auto-discover and build new tests

## Test Data

The `fixtures/` directory contains test data files:
- Sample SMART responses
- Protocol captures
- Known-good/known-bad data sets

## Integration Tests

Integration tests and ad-hoc testing scripts are in `scripts/`:
- `test_signal_handling.sh` - Verifies sector restoration on interruption
- `analyze_captures.sh` - Analysis of protocol captures

## CI/CD

Tests should be run before commits:
```bash
make clean && make && make tests && echo "Ready to commit"
```

Future: Add GitHub Actions workflow to run tests automatically.
