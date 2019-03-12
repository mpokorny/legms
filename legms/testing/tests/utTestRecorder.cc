#include <iostream>

#include "TestRecorder.h"

using namespace legms;
using namespace Legion;

enum {
  TEST_SUITE_DRIVER_TASK_ID,
  TEST_RECORDER_TEST_SUITE_ID,
};

#define LOG_LENGTH 100

std::string
verify_result(
  const testing::TestResult<READ_WRITE>& tr,
  const testing::TestResult<READ_ONLY>& expected) {

  const char* sep = "";
  std::ostringstream oss;
  if (tr.name != expected.name) {
    oss << "'name' expected '" << expected.name
        << "', got'" << tr.name << "'";
    sep = "; ";
  }
  if (tr.state != expected.state) {
    oss << sep
        << "'state' expected " << expected.state
        << ", got " << tr.state;
    sep = "; ";
  }
  if (tr.abort != expected.abort) {
    oss << sep
        << "'abort' expected " << expected.abort
        << ", got " << tr.abort;
    sep = "; ";
  }
  if (tr.fail_info != expected.fail_info) {
    oss << sep
        << "'fail_info' expected '" << expected.fail_info
        << "', got '" << tr.fail_info << "'";
    sep = "; ";
  }
  return oss.str();
}

void
test_recorder_test_suite(
  const Task* task,
  const std::vector<PhysicalRegion>& regions,
  Context ctx,
  Runtime *runtime) {

  testing::TestLog<READ_WRITE> log(regions[0], regions[1], ctx, runtime);
  testing::TestRecorder recorder(log);

  testing::TestResult<READ_ONLY> dummy_success{
    testing::TestState::SUCCESS,
    false,
    "Dummy success",
    ""};
  recorder.append(dummy_success.name, dummy_success.state);

  testing::TestResult<READ_ONLY> dummy_success_testresult{
    testing::TestState::SUCCESS,
    false,
    "Dummy success TestResult",
    ""};
  recorder.append(dummy_success_testresult);

  testing::TestResult<READ_ONLY> dummy_failure{
    testing::TestState::FAILURE,
    false,
    "Dummy failure",
    "Expected FAILURE"};
  recorder.append(dummy_failure);

  auto log_readback = log.iterator();
  {
    const char* name = "Dummy success readback";
    testing::TestResult<READ_WRITE> test_result(*log_readback);
    std::string errors = verify_result(test_result, dummy_success);
    if (errors.size() == 0)
      recorder.append(name, testing::TestState::SUCCESS);
    else
      recorder.append(name, testing::TestState::FAILURE, false, errors);
  }
  ++log_readback;

  {
    const char *name = "Dummy success TestResult readback";
    testing::TestResult<READ_WRITE> test_result(*log_readback);
    std::string errors = verify_result(test_result, dummy_success_testresult);
    if (errors.size() == 0)
      recorder.append(name, testing::TestState::SUCCESS);
    else
      recorder.append(name, testing::TestState::FAILURE, false, errors);
  }
  ++log_readback;

  {
    const char *name = "Dummy failure readback";
    testing::TestResult<READ_WRITE> test_result(*log_readback);
    std::string errors = verify_result(test_result, dummy_failure);
    if (errors.size() == 0)
      recorder.append(name, testing::TestState::SUCCESS);
    else
      recorder.append(name, testing::TestState::FAILURE, false, errors);
  }
  ++log_readback;
}

void
test_suite_driver_task(
  const Task*,
  const std::vector<PhysicalRegion>&,
  Context context,
  Runtime* runtime) {

  // initialize the test log
  testing::TestLogReference logref(LOG_LENGTH, context, runtime);
  testing::TestLog<WRITE_DISCARD>(logref, context, runtime).initialize();

  TaskLauncher test(TEST_RECORDER_TEST_SUITE_ID, TaskArgument());
  auto reqs = logref.requirements<READ_WRITE>();
  test.add_region_requirement(reqs[0]);
  test.add_region_requirement(reqs[1]);
  runtime->execute_task(context, test);

  // print out the test log
  std::ostringstream oss;
  testing::TestLog<READ_ONLY>(logref, context, runtime).for_each(
    [&oss](auto& it) {
      auto test_result = *it;
      switch (test_result.state) {
      case testing::TestState::SUCCESS:
        oss << "PASS: "
            << test_result.name
            << std::endl;
        break;
      case testing::TestState::FAILURE:
        oss << "FAIL: "
            << test_result.name;
        if (test_result.fail_info.size() > 0)
          oss << ": "
              << test_result.fail_info;
        oss << std::endl;
        break;
      case testing::TestState::SKIPPED:
        oss << "SKIPPED: "
            << test_result.name
            << std::endl;
        break;
      case testing::TestState::UNKNOWN:
        break;
      }
    });
  std::cout << oss.str();
}

int
main(int argc, char* argv[]) {

  Runtime::set_top_level_task_id(TEST_SUITE_DRIVER_TASK_ID);
  SerdezManager::register_ops();

  {
    TaskVariantRegistrar registrar(TEST_SUITE_DRIVER_TASK_ID, "test_driver");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<test_suite_driver_task>(
      registrar,
      "test_driver");
  }
  {
    TaskVariantRegistrar registrar(TEST_RECORDER_TEST_SUITE_ID, "test_suite");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<test_recorder_test_suite>(
      registrar,
      "test_suite");
  }

  return Runtime::start(argc, argv);

}

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End: