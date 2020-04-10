/*
 * Copyright 2020 Associated Universities, Inc. Washington DC, USA.
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
#include <hyperion/hyperion.h>
#include <hyperion/utility.h>
#include <hyperion/Table.h>
#include <hyperion/MSMainTable.h>
#include <hyperion/TableReadTask.h>
#include <hyperion/DefaultMapper.h>

#include <hyperion/testing/TestSuiteDriver.h>
#include <hyperion/testing/TestRecorder.h>
#include <hyperion/testing/TestExpression.h>

#include <casacore/ms/MeasurementSets.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

using namespace hyperion;
using namespace Legion;

enum {
  MS_TEST_TASK,
  VERIFY_MAIN_TABLE_TASK
};

#define TE(f) testing::TestEval([&](){ return f; }, #f)

struct VerifyTableArgs {
  char table_path[1024];
};

void
verify_main_table(
  const Task* task,
  const std::vector<PhysicalRegion>& regions,
  Context ctx,
  Runtime* rt) {

  testing::TestLog<READ_WRITE> log(
    task->regions[0].region,
    regions[0],
    task->regions[1].region,
    regions[1],
    ctx,
    rt);
  testing::TestRecorder<READ_WRITE> recorder(log);

  auto [pt, rit, pit] =
    PhysicalTable::create(
      rt,
      task->regions.begin() + 2,
      task->regions.end(),
      regions.begin() + 2,
      regions.end())
    .value();
  assert(rit == task->regions.end());
  assert(pit == regions.end());

  MSMainTable table(pt);

  const VerifyTableArgs *args =
    static_cast<const VerifyTableArgs*>(task->args);
  CXX_FILESYSTEM_NAMESPACE::path main_path(args->table_path);
  casacore::MeasurementSet ms(
    main_path.string(),
    casacore::TableLock::PermanentLockingWait);
  casacore::ROMSMainColumns ms_main(ms);

  recorder.expect_true(
    "Table has TIME column",
    TE(table.has_time()));
  // TODO: For now, we don't check values, as utMS already has lots of such
  // tests; however, it might be useful to have some tests that confirm the
  // conversion Table->RegionRequiremnts->PhysicalTable->PhysicalColumn is
  // correct.
  recorder.expect_true(
    "Table has TIME measures",
    TE(table.has_time_meas()));
  recorder.expect_true(
    "Table TIME measures are correct",
    testing::TestEval(
      [&]() {
        auto col = ms_main.timeMeas();
        auto time_col = table.time_meas<AffineAccessor>();
        auto time_meas =
          time_col.meas_accessor<READ_ONLY>(
            rt,
            MSTableColumns<MS_MAIN>::units.at(
              MSTableColumns<MS_MAIN>::col_t::MS_MAIN_COL_TIME));
        bool result = true;
        for (PointInRectIterator<1> pir(time_col.rect());
             result && pir();
             pir++) {
          auto time = time_meas.read(*pir);
          decltype(time) cctime;
          col.get(pir[0], cctime);
          result = (time.getValue() == cctime.getValue());
        }
        return result;
      }));
  recorder.expect_true(
    "Table does not have TIME_EXTRA_PREC column",
    TE(!table.has_time_extra_prec()));
  recorder.expect_true(
    "Table has ANTENNA1 column",
    TE(table.has_antenna1()));
  recorder.expect_true(
    "Table has ANTENNA2 column",
    TE(table.has_antenna2()));
  recorder.expect_true(
    "Table does not have ANTENNA3 column",
    TE(!table.has_antenna3()));
  recorder.expect_true(
    "Table has FEED1 column",
    TE(table.has_feed1()));
  recorder.expect_true(
    "Table has FEED2 column",
    TE(table.has_feed2()));
  recorder.expect_true(
    "Table does not have FEED3 column",
    TE(!table.has_feed3()));
  recorder.expect_true(
    "Table has DATA_DESC_ID column",
    TE(table.has_data_desc_id()));
  recorder.expect_true(
    "Table has PROCESSOR_ID column",
    TE(table.has_processor_id()));
  recorder.expect_true(
    "Table does not have PHASE_ID column",
    TE(!table.has_phase_id()));
  recorder.expect_true(
    "Table has FIELD_ID column",
    TE(table.has_field_id()));
  recorder.expect_true(
    "Table has INTERVAL column",
    TE(table.has_interval()));
  recorder.expect_true(
    "Table has EXPOSURE column",
    TE(table.has_exposure()));
  recorder.expect_true(
    "Table has TIME_CENTROID column",
    TE(table.has_time_centroid()));
  recorder.expect_true(
    "Table has TIME_CENTROID measures",
    TE(table.has_time_centroid_meas()));
  recorder.expect_true(
    "Table TIME_CENTROID measures are correct",
    testing::TestEval(
      [&]() {
        auto col = ms_main.timeCentroidMeas();
        auto time_centroid_col =
          table.time_centroid_meas<AffineAccessor>();
        auto time_centroid_meas =
          time_centroid_col.meas_accessor<READ_ONLY>(
            rt,
            MSTableColumns<MS_MAIN>::units.at(
              MSTableColumns<MS_MAIN>::col_t::MS_MAIN_COL_TIME_CENTROID));
        bool result = true;
        for (PointInRectIterator<1> pir(time_centroid_col.rect());
             result && pir();
             pir++) {
          auto tc = time_centroid_meas.read(*pir);
          decltype(tc) cctc;
          col.get(pir[0], cctc);
          result = (tc.getValue() == cctc.getValue());
        }
        return result;
      }));
  recorder.expect_true(
    "Table does not have PULSAR_BIN column",
    TE(!table.has_pulsar_bin()));
  recorder.expect_true(
    "Table does not have PULSAR_GATE_ID column",
    TE(!table.has_pulsar_gate_id()));
  recorder.expect_true(
    "Table has SCAN_NUMBER column",
    TE(table.has_scan_number()));
  recorder.expect_true(
    "Table has OBSERVATION_ID column",
    TE(table.has_observation_id()));
  recorder.expect_true(
    "Table has ARRAY_ID column",
    TE(table.has_array_id()));
  recorder.expect_true(
    "Table has STATE_ID column",
    TE(table.has_state_id()));
  recorder.expect_true(
    "Table does not have BASELINE_REF column",
    TE(!table.has_baseline_ref()));
  recorder.expect_true(
    "Table has UVW column",
    TE(table.has_uvw()));
  recorder.expect_true(
    "Table has UVW measures",
    TE(table.has_uvw_meas()));
  recorder.expect_true(
    "Table UVW measures are correct",
    testing::TestEval(
      [&]() {
        auto col = ms_main.uvwMeas();
        auto uvw_col = table.uvw_meas<AffineAccessor>();
        auto uvw_meas =
          uvw_col.meas_accessor<READ_ONLY>(
            rt,
            MSTableColumns<MS_MAIN>::units.at(
              MSTableColumns<MS_MAIN>::col_t::MS_MAIN_COL_UVW));
        std::optional<coord_t> prev_row;
        bool result = true;
        for (PointInRectIterator<2> pir(uvw_col.rect(), false);
             result && pir();
             pir++) {
          if (pir[0] != prev_row.value_or(pir[0] + 1)) {
            auto uvw = uvw_meas.read(pir[0]);
            decltype(uvw) ccuvw;
            col.get(pir[0], ccuvw);
            result = (uvw.getValue() == ccuvw.getValue());
          }
        }
        return result;
      }));
  recorder.expect_true(
    "Table does not have UVW2 column",
    TE(!table.has_uvw2()));
  recorder.expect_true(
    "Table has DATA column",
    TE(table.has_data()));
  recorder.expect_true(
    "Table does not have FLOAT_DATA column",
    TE(!table.has_float_data()));
  recorder.expect_true(
    "Table does not have VIDEO_POINT column",
    TE(!table.has_video_point()));
  recorder.expect_true(
    "Table does not have LAG_DATA column",
    TE(!table.has_lag_data()));
  recorder.expect_true(
    "Table has SIGMA column",
    TE(table.has_sigma()));
  recorder.expect_true(
    "Table does not have SIGMA_SPECTRUM column",
    TE(!table.has_sigma_spectrum()));
  recorder.expect_true(
    "Table has WEIGHT column",
    TE(table.has_weight()));
  recorder.expect_true(
    "Table does not have WEIGHT_SPECTRUM column",
    TE(!table.has_weight_spectrum()));
  recorder.expect_true(
    "Table has FLAG column",
    TE(table.has_flag()));
  recorder.expect_true(
    "Table does not have FLAG_CATEGORY column",
    TE(!table.has_flag_category()));
  recorder.expect_true(
    "Table has FLAG_ROW column",
    TE(table.has_flag_row()));
}

void
ms_test(
  const Task* task,
  const std::vector<PhysicalRegion>& regions,
  Context ctx,
  Runtime* rt) {

  register_tasks(ctx, rt);

  const CXX_FILESYSTEM_NAMESPACE::path tpath = "data/t0.ms";

  // create the table
  auto table =
    Table::create(
      ctx,
      rt,
      std::get<1>(from_ms(ctx, rt, tpath, {"*"})));

  // read values from MS
  {
    auto reqs =
      std::get<0>(
        TableReadTask::requirements(
          ctx,
          rt,
          table,
          ColumnSpacePartition(),
          WRITE_ONLY));
    TableReadTask::Args args;
    fstrcpy(args.table_path, tpath);
    TaskLauncher read(
      TableReadTask::TASK_ID,
      TaskArgument(&args, sizeof(args)),
      Predicate::TRUE_PRED,
      mapper);
    for (auto& rq : reqs)
      read.add_region_requirement(rq);
    rt->execute_task(ctx, read);
  }

  // run tests
  {
    VerifyTableArgs args;
    fstrcpy(args.table_path, tpath);
    auto reqs = std::get<0>(table.requirements(ctx, rt));
    TaskLauncher verify(
      VERIFY_MAIN_TABLE_TASK,
      TaskArgument(&args, sizeof(args)),
      Predicate::TRUE_PRED,
      mapper);
    verify.add_region_requirement(task->regions[0]);
    verify.add_region_requirement(task->regions[1]);
    for (auto& rq : reqs)
      verify.add_region_requirement(rq);
    rt->execute_task(ctx, verify);
  }

  // clean up
  table.destroy(ctx, rt);
}

int
main(int argc, char** argv) {

  testing::TestSuiteDriver driver =
    testing::TestSuiteDriver::make<ms_test>(MS_TEST_TASK, "ms_test");
  {
    // verify_main_table
    TaskVariantRegistrar
      registrar(VERIFY_MAIN_TABLE_TASK, "verify_main_table");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    DefaultMapper::add_layouts(registrar);
    Runtime::preregister_task_variant<verify_main_table>(
      registrar,
      "verify_main_table");
  }

  return driver.start(argc, argv);
}

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End: