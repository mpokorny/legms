/*
 * Copyright 2019 Associated Universities, Inc. Washington DC, USA.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <hyperion/testing/TestSuiteDriver.h>
#include <hyperion/testing/TestRecorder.h>

#include <algorithm>
#include <memory>
#include <ostream>
#include <vector>

#include <hyperion/utility.h>
#include <hyperion/Table.h>
#include <hyperion/Column.h>

#ifndef NO_REINDEX

using namespace hyperion;
using namespace Legion;

enum {
  REINDEX_COLUMN_TASK_TEST_SUITE,
};

enum struct Table0Axes {
  ROW = 0,
  X,
  Y
};

template <>
struct hyperion::Axes<Table0Axes> {
  static const constexpr char* uid = "Table0Axes";
  static const std::vector<std::string> names;
  static const unsigned num_axes = 3;
#ifdef HYPERION_USE_HDF5
  static const hid_t h5_datatype;
#endif
};

const std::vector<std::string>
hyperion::Axes<Table0Axes>::names{"ROW", "X", "Y"};

#ifdef HYPERION_USE_HDF5
hid_t
h5_dt() {
  hid_t result = H5Tenum_create(H5T_NATIVE_UCHAR);
  Table0Axes a = Table0Axes::ROW;
  herr_t err = H5Tenum_insert(result, "ROW", &a);
  assert(err >= 0);
  a = Table0Axes::X;
  err = H5Tenum_insert(result, "X", &a);
  a = Table0Axes::Y;
  err = H5Tenum_insert(result, "Y", &a);
  return result;
}

const hid_t
hyperion::Axes<Table0Axes>::h5_datatype = h5_dt();
#endif

std::ostream&
operator<<(std::ostream& stream, const Table0Axes& ax) {
  switch (ax) {
  case Table0Axes::ROW:
    stream << "Table0Axes::ROW";
    break;
  case Table0Axes::X:
    stream << "Table0Axes::X";
    break;
  case Table0Axes::Y:
    stream << "Table0Axes::Y";
    break;
  }
  return stream;
}

std::ostream&
operator<<(std::ostream& stream, const std::vector<Table0Axes>& axs) {
  stream << "[";
  const char* sep = "";
  std::for_each(
    axs.begin(),
    axs.end(),
    [&stream, &sep](auto& ax) {
      stream << sep << ax;
      sep = ",";
    });
  stream << "]";
  return stream;
}

#define TABLE0_NUM_X 4
#define TABLE0_NUM_Y 3
#define TABLE0_NUM_ROWS (TABLE0_NUM_X * TABLE0_NUM_Y)
unsigned table0_x[TABLE0_NUM_ROWS] {
                   0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3};
unsigned table0_y[TABLE0_NUM_ROWS] {
                   0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2};
unsigned table0_z[TABLE0_NUM_ROWS] {
                   0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

Column::Generator
table0_col(const std::string& name) {
  return
    [=](Context context, Runtime* runtime) {
      return
        std::make_unique<Column>(
          context,
          runtime,
          name,
          std::vector<Table0Axes>{Table0Axes::ROW},
          ValueType<unsigned>::DataType,
          IndexTreeL(TABLE0_NUM_ROWS));
    };
}

PhysicalRegion
attach_table0_col(
  const Column* col,
  unsigned *base,
  Context context,
  Runtime* runtime) {

  const Memory local_sysmem =
    Machine::MemoryQuery(Machine::get_machine())
    .has_affinity_to(runtime->get_executing_processor(context))
    .only_kind(Memory::SYSTEM_MEM)
    .first();

  AttachLauncher
    task(EXTERNAL_INSTANCE, col->logical_region(), col->logical_region());
  task.attach_array_soa(
    base,
    true,
    {Column::value_fid},
    local_sysmem);
  return runtime->attach_external_resource(context, task);
}

#define TE(f) testing::TestEval([&](){ return f; }, #f)

void
reindex_column_task_test_suite(
  const Task* task,
  const std::vector<PhysicalRegion>& regions,
  Context context,
  Runtime* runtime) {

  register_tasks(context, runtime);

  testing::TestRecorder<READ_WRITE> recorder(
    testing::TestLog<READ_WRITE>(
      task->regions[0].region,
      regions[0],
      task->regions[1].region,
      regions[1],
      context,
      runtime));

  Table
    table0(
      context,
      runtime,
      "table0",
      std::vector<Table0Axes>{Table0Axes::ROW},
      {table0_col("X"),
       table0_col("Y"),
       table0_col("Z")});
  auto col_x =
    attach_table0_col(table0.column("X").get(), table0_x, context, runtime);
  auto col_y =
    attach_table0_col(table0.column("Y").get(), table0_y, context, runtime);
  auto col_z =
    attach_table0_col(table0.column("Z").get(), table0_z, context, runtime);

  IndexColumnTask icx(table0.column("X"), static_cast<int>(Table0Axes::X));
  IndexColumnTask icy(table0.column("Y"), static_cast<int>(Table0Axes::Y));
  std::vector<Future> icfs {
    icx.dispatch(context, runtime),
    icy.dispatch(context, runtime)};
  std::vector<std::shared_ptr<Column>> ics;
  std::for_each(
    icfs.begin(),
    icfs.end(),
    [&context, &runtime, &ics](Future& f) {
      auto col =
        f.get_result<ColumnGenArgs>().operator()(context, runtime);
      ics.push_back(std::move(col));
    });

  ReindexColumnTask rcz(table0.column("Z"), 0, ics, false);
  Future fz = rcz.dispatch(context, runtime);
  auto cz =
    fz.get_result<ColumnGenArgs>().operator()(context, runtime);
  recorder.assert_true(
    "Reindexed column index space rank is 2",
    TE(cz->rank()) == 2);
  recorder.expect_true(
    "Reindexed column index space dimensions are X and Y",
    TE(cz->axes()) ==
    map_to_int(std::vector<Table0Axes>{ Table0Axes::X, Table0Axes::Y }));
  {
    RegionRequirement
      req(cz->logical_region(), READ_ONLY, EXCLUSIVE, cz->logical_region());
    req.add_field(Column::value_fid);
    PhysicalRegion pr = runtime->map_region(context, req);
    DomainT<2,coord_t> d(pr);
    const FieldAccessor<
      READ_ONLY, unsigned, 2, coord_t,
      AffineAccessor<unsigned, 2, coord_t>, false> z(pr, Column::value_fid);
    recorder.expect_true(
      "Reindexed column values are correct",
      testing::TestEval(
        [&d, &z]() {
          bool all_eq = true;
          for (PointInDomainIterator<2> pid(d); all_eq && pid(); pid++)
            all_eq = z[*pid] == pid[0] * TABLE0_NUM_Y + pid[1];
          return all_eq;
        },
        "all(z[x,y] == x * TABLE0_NUM_Y + y)"));
  }

  runtime->detach_external_resource(context, col_x);
  runtime->detach_external_resource(context, col_y);
  runtime->detach_external_resource(context, col_z);
}

int
main(int argc, char* argv[]) {

  AxesRegistrar::register_axes<Table0Axes>();

  testing::TestSuiteDriver driver =
    testing::TestSuiteDriver::make<reindex_column_task_test_suite>(
      REINDEX_COLUMN_TASK_TEST_SUITE,
      "reindex_column_task_test_suite");

  return driver.start(argc, argv);
}

#else

int main(int argc, char* argv[]) {
  return -1;
}

#endif // NO_REINDEX

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End: