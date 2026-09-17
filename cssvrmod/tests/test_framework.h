#pragma once
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

struct TestCase {
  std::string name;
  std::function<void()> func;
};

inline std::vector<TestCase>& GetTests() {
  static std::vector<TestCase> tests;
  return tests;
}
inline int& TestsPassed() {
  static int v = 0;
  return v;
}
inline int& TestsFailed() {
  static int v = 0;
  return v;
}
inline int& AssertsPassed() {
  static int v = 0;
  return v;
}
inline int& AssertsFailed() {
  static int v = 0;
  return v;
}
inline bool& CurrentTestFailed() {
  static bool v = false;
  return v;
}

#define TEST(name)                                                                                 \
  void test_##name();                                                                              \
  static bool _reg_##name = []() {                                                                 \
    GetTests().push_back({#name, test_##name});                                                    \
    return true;                                                                                   \
  }();                                                                                             \
  void test_##name()

#define ASSERT_TRUE(expr)                                                                          \
  do {                                                                                             \
    if (!(expr)) {                                                                                 \
      fprintf(stderr, "  FAIL: %s:%d: ASSERT_TRUE(%s)\n", __FILE__, __LINE__, #expr);             \
      AssertsFailed()++;                                                                           \
      CurrentTestFailed() = true;                                                                  \
    } else {                                                                                       \
      AssertsPassed()++;                                                                           \
    }                                                                                              \
  } while (0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_EQ(a, b)                                                                            \
  do {                                                                                             \
    if ((a) != (b)) {                                                                              \
      fprintf(stderr, "  FAIL: %s:%d: ASSERT_EQ(%s, %s)\n", __FILE__, __LINE__, #a, #b);          \
      AssertsFailed()++;                                                                           \
      CurrentTestFailed() = true;                                                                  \
    } else {                                                                                       \
      AssertsPassed()++;                                                                           \
    }                                                                                              \
  } while (0)

#define ASSERT_NEAR(a, b, eps)                                                                     \
  do {                                                                                             \
    if (fabs((double)(a) - (double)(b)) > (eps)) {                                                 \
      fprintf(stderr, "  FAIL: %s:%d: ASSERT_NEAR got %f vs %f\n", __FILE__, __LINE__,            \
              (double)(a), (double)(b));                                                           \
      AssertsFailed()++;                                                                           \
      CurrentTestFailed() = true;                                                                  \
    } else {                                                                                       \
      AssertsPassed()++;                                                                           \
    }                                                                                              \
  } while (0)

#define ASSERT_STREQ(a, b)                                                                         \
  do {                                                                                             \
    if (strcmp((a), (b)) != 0) {                                                                   \
      fprintf(stderr, "  FAIL: %s:%d: ASSERT_STREQ got \"%s\" vs \"%s\"\n", __FILE__, __LINE__,   \
              (a), (b));                                                                           \
      AssertsFailed()++;                                                                           \
      CurrentTestFailed() = true;                                                                  \
    } else {                                                                                       \
      AssertsPassed()++;                                                                           \
    }                                                                                              \
  } while (0)

inline int RunAllTests() {
  for (auto& t : GetTests()) {
    CurrentTestFailed() = false;
    t.func();
    if (CurrentTestFailed()) {
      fprintf(stderr, "FAIL %s\n", t.name.c_str());
      TestsFailed()++;
    } else {
      printf("PASS %s\n", t.name.c_str());
      TestsPassed()++;
    }
  }
  printf("cssvrmod tests: %d passed, %d failed (%d/%d asserts)\n", TestsPassed(), TestsFailed(),
         AssertsPassed(), AssertsPassed() + AssertsFailed());
  return TestsFailed() == 0 ? 0 : 1;
}
