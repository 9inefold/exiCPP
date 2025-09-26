#include <cstdio>
#include "gtest/gtest.h"

using ::testing::EmptyTestEventListener;
using ::testing::InitGoogleTest;
using ::testing::Test;
using ::testing::TestEventListeners;
using ::testing::TestInfo;
using ::testing::TestPartResult;
using ::testing::UnitTest;

namespace {
class CustomListener : public EmptyTestEventListener {
private:
  // Called before any test activity starts.
  void OnTestProgramStart(const UnitTest& /* unit_test */) override {}

  // Called after all test activities have ended.
  void OnTestProgramEnd(const UnitTest& unit_test) override {}

  // Called before a test starts.
  void OnTestStart(const TestInfo& test_info) override {}

  // Called after a failed assertion or a SUCCEED() invocation.
  void OnTestPartResult(const TestPartResult& test_part_result) override {}

  // Called after a test ends.
  void OnTestEnd(const TestInfo& test_info) override {}
};
} // namespace `anonymous`

GTEST_API_ int main(int argc, char** argv) {
  //std::printf("Running main() from %s\n", __FILE__);
  InitGoogleTest(&argc, argv);

  auto& Instance = *UnitTest::GetInstance();
  // Gets hold of the event listener list.
  TestEventListeners& listeners = Instance.listeners();
  
  // Removes the default console output listener from the list so it will
  // not receive events from Google Test and won't print any output.
  delete listeners.Release(listeners.default_result_printer());
  // Adds the custom output listener to the list.
  listeners.Append(new CustomListener);

  return RUN_ALL_TESTS();
}
