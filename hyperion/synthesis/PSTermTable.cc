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
#include <hyperion/synthesis/PSTermTable.h>

#include <cmath>

using namespace hyperion::synthesis;
using namespace hyperion;
using namespace Legion;

#define USE_KOKKOS_SERIAL_COMPUTE_CFS_TASK // undef to disable
#define USE_KOKKOS_OPENMP_COMPUTE_CFS_TASK //undef to disable
#undef USE_KOKKOS_CUDA_COMPUTE_CFS_TASK //undef to disable

#define USE_KOKKOS_VARIANT(V, T)                \
  (defined(USE_KOKKOS_##V##_COMPUTE_##T) &&     \
   defined(HYPERION_USE_KOKKOS) &&              \
   defined(KOKKOS_ENABLE_##V))

#define USE_PLAIN_SERIAL_VARIANT(T)             \
  (!USE_KOKKOS_VARIANT(SERIAL, T) &&            \
   !USE_KOKKOS_VARIANT(OPENMP, T) &&            \
   !USE_KOKKOS_VARIANT(CUDA, T))

#if !HAVE_CXX17
const constexpr unsigned PSTermTable::d_ps;
const constexpr char* PSTermTable::compute_cfs_task_name;
#endif
TaskID PSTermTable::compute_cfs_task_id;

PSTermTable::PSTermTable(
  Context ctx,
  Runtime* rt,
  const size_t& grid_size,
  const std::vector<typename cf_table_axis<CF_PS_SCALE>::type>& ps_scales)
  : CFTable(ctx, rt, grid_size, Axis<CF_PS_SCALE>(ps_scales)) {}

#if USE_PLAIN_SERIAL_VARIANT(CFS_TASK)
void
PSTermTable::compute_cfs_task(
  const Task* task,
  const std::vector<PhysicalRegion>& regions,
  Context ctx,
  Runtime* rt) {

  const ComputeCFsTaskArgs& args =
    *static_cast<ComputeCFsTaskArgs*>(task->args);
  std::vector<Table::Desc> tdescs{args.ps, args.gc};

  auto ptcrs =
    PhysicalTable::create_many(
      rt,
      tdescs,
      task->regions.begin(),
      task->regions.end(),
      regions.begin(),
      regions.end())
    .value();
#if HAVE_CXX17
  auto& [pts, rit, pit] = ptcrs;
#else // !HAVE_CXX17
  auto& pts = std::get<0>(ptcrs);
  auto& rit = std::get<1>(ptcrs);
  auto& pit = std::get<2>(ptcrs);
#endif // HAVE_CXX17
  assert(rit == task->regions.end());
  assert(pit == regions.end());

  auto ps_tbl = CFPhysicalTable<CF_PS_SCALE>(pts[0]);
  auto gc_tbl = CFPhysicalTable<CF_PARALLACTIC_ANGLE>(pts[1]);

  auto ps_scales = ps_tbl.ps_scale<AffineAccessor>().accessor<READ_ONLY>();
  auto value_col = ps_tbl.value<AffineAccessor>();
  auto values = value_col.accessor<WRITE_ONLY>();
  auto weight_col = ps_tbl.value<AffineAccessor>();
  auto weights = weight_col.accessor<WRITE_ONLY>();

  auto cs_x_col =
    GridCoordinateTable::CoordColumn<Legion::AffineAccessor>(
      *gc_tbl.column(GridCoordinateTable::COORD_X_NAME).value());
  auto i_pa = cs_x_col.rect().lo[GridCoordinateTable::d_pa];
  auto cs_x = cs_x_col.accessor<LEGION_READ_ONLY>();
  auto cs_y =
    GridCoordinateTable::CoordColumn<Legion::AffineAccessor>(
      *gc_tbl.column(GridCoordinateTable::COORD_Y_NAME).value())
    .accessor<LEGION_READ_ONLY>();

  for (PointInRectIterator<3> pir(value_col.rect()); pir(); pir++) {
    const cf_fp_t x = cs_x(i_pa, pir[d_x], pir[d_y]);
    const cf_fp_t y = cs_y(i_pa, pir[d_x], pir[d_y]);
    const cf_fp_t rs = std::sqrt(x * x + y * y) * ps_scales[pir[d_ps]];
    if (rs <= (cf_fp_t)1.0) {
      const cf_fp_t v = spheroidal(rs) * ((cf_fp_t)1.0 - rs * rs);
      values[*pir] = v;
      weights[*pir] = v * v;
    } else {
      values[*pir] = (cf_fp_t)0.0;
      weights[*pir] = std::numeric_limits<cf_fp_t>::quiet_NaN();
    }
  }
}
#endif

void
PSTermTable::compute_cfs(
  Context ctx,
  Runtime* rt,
  const GridCoordinateTable& gc,
  const ColumnSpacePartition& partition) const {

  auto ro_colreqs = Column::default_requirements;
  ro_colreqs.values.mapped = true;

  std::vector<RegionRequirement> all_reqs;
  std::vector<ColumnSpacePartition> all_parts;
  ComputeCFsTaskArgs args;
  {
    Column::Requirements wo_colreqs = Column::default_requirements;
    wo_colreqs.values.privilege = LEGION_WRITE_ONLY;
    wo_colreqs.values.mapped = true;

    auto reqs =
      requirements(
        ctx,
        rt,
        partition,
        {{CF_VALUE_COLUMN_NAME, wo_colreqs},
         {CF_WEIGHT_COLUMN_NAME, wo_colreqs}},
        ro_colreqs);
#if HAVE_CXX17
    auto& [treqs, tparts, tdesc] = reqs;
#else // !HAVE_CXX17
    auto& treqs = std::get<0>(reqs);
    auto& tparts = std::get<1>(reqs);
    auto& tdesc = std::get<2>(reqs);
#endif // HAVE_CXX17
    std::copy(treqs.begin(), treqs.end(), std::back_inserter(all_reqs));
    std::copy(tparts.begin(), tparts.end(), std::back_inserter(all_parts));
    args.ps = tdesc;
  }
  {
    auto reqs =
      gc.requirements(
        ctx,
        rt,
        partition,
        {{GridCoordinateTable::COORD_X_NAME, ro_colreqs},
         {GridCoordinateTable::COORD_Y_NAME, ro_colreqs}},
        CXX_OPTIONAL_NAMESPACE::nullopt);
#if HAVE_CXX17
    auto& [treqs, tparts, tdesc] = reqs;
#else // !HAVE_CXX17
    auto& treqs = std::get<0>(reqs);
    auto& tparts = std::get<1>(reqs);
    auto& tdesc = std::get<2>(reqs);
#endif // HAVE_CXX17
    std::copy(treqs.begin(), treqs.end(), std::back_inserter(all_reqs));
    std::copy(tparts.begin(), tparts.end(), std::back_inserter(all_parts));
    args.gc = tdesc;
  }
  if (!partition.is_valid()) {
    TaskLauncher task(
      compute_cfs_task_id,
      TaskArgument(&args, sizeof(args)),
      Predicate::TRUE_PRED,
      table_mapper);
    for (auto& r : all_reqs)
      task.add_region_requirement(r);
    rt->execute_task(ctx, task);
  } else {
    IndexTaskLauncher task(
      compute_cfs_task_id,
      rt->get_index_partition_color_space(ctx, partition.column_ip),
      TaskArgument(&args, sizeof(args)),
      ArgumentMap(),
      Predicate::TRUE_PRED,
      table_mapper);
    for (auto& r : all_reqs)
      task.add_region_requirement(r);
    rt->execute_index_space(ctx, task);
  }
  for (auto& p : all_parts)
    p.destroy(ctx, rt);
}

void
PSTermTable::preregister_tasks() {
  //
  // compute_cfs_task
  //
  {
#if USE_KOKKOS_VARIANT(SERIAL, CFS_TASK) ||     \
  USE_KOKKOS_VARIANT(OPENMP, CFS_TASK) ||       \
  USE_PLAIN_SERIAL_VARIANT(CFS_TASK)
    LayoutConstraintRegistrar
      cpu_constraints(FieldSpace::NO_SPACE, "PSTermTable::compute_cfs");
    add_aos_right_ordering_constraint(cpu_constraints);
    cpu_constraints.add_constraint(
      SpecializedConstraint(LEGION_AFFINE_SPECIALIZE));
    auto cpu_layout_id = Runtime::preregister_layout(cpu_constraints);
#endif

#if USE_KOKKOS_VARIANT(CUDA, CFS_TASK)
    LayoutConstraintRegistrar
      gpu_constraints(FieldSpace::NO_SPACE, "PSTermTable::compute_cfs");
    add_soa_left_ordering_constraint(gpu_constraints);
    gpu_constraints.add_constraint(
      SpecializedConstraint(LEGION_AFFINE_SPECIALIZE));
    auto gpu_layout_id = Runtime::preregister_layout(gpu_constraints);
#endif

    compute_cfs_task_id = Runtime::generate_static_task_id();

#if USE_KOKKOS_VARIANT(SERIAL, CFS_TASK)
    // register a serial version on the CPU
    {
      TaskVariantRegistrar
        registrar(compute_cfs_task_id, compute_cfs_task_name);
      registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
      registrar.set_leaf();
      registrar.set_idempotent();

      registrar.add_layout_constraint_set(
        TableMapper::to_mapping_tag(TableMapper::default_column_layout_tag),
        cpu_layout_id);

      Runtime::preregister_task_variant<compute_cfs_task<Kokkos::Serial>>(
        registrar,
        compute_cfs_task_name);
    }
#endif

#if USE_KOKKOS_VARIANT(OPENMP, CFS_TASK)
    // register an OpenMP version
    {
      TaskVariantRegistrar
        registrar(compute_cfs_task_id, compute_cfs_task_name);
      registrar.add_constraint(ProcessorConstraint(Processor::OMP_PROC));
      registrar.set_leaf();
      registrar.set_idempotent();

      registrar.add_layout_constraint_set(
        TableMapper::to_mapping_tag(TableMapper::default_column_layout_tag),
        cpu_layout_id);

      Runtime::preregister_task_variant<compute_cfs_task<Kokkos::OpenMP>>(
        registrar,
        compute_cfs_task_name);
    }
#endif

#if USE_KOKKOS_VARIANT(CUDA, CFS_TASK)
    // register a version on the GPU
    {
      TaskVariantRegistrar
        registrar(compute_cfs_task_id, compute_cfs_task_name);
      registrar.add_constraint(ProcessorConstraint(Processor::TOC_PROC));
      registrar.set_leaf();
      registrar.set_idempotent();

      registrar.add_layout_constraint_set(
        TableMapper::to_mapping_tag(TableMapper::default_column_layout_tag),
        gpu_layout_id);

      Runtime::preregister_task_variant<compute_cfs_task<Kokkos::Cuda>>(
        registrar,
        compute_cfs_task_name);
    }
#endif

#if USE_PLAIN_SERIAL_VARIANT(CFS_TASK)
    // register a non-Kokkos, serial version
    {
      TaskVariantRegistrar
        registrar(compute_cfs_task_id, compute_cfs_task_name);
      registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
      registrar.set_leaf();
      registrar.set_idempotent();
      registrar.add_layout_constraint_set(
        TableMapper::to_mapping_tag(TableMapper::default_column_layout_tag),
        cpu_layout_id);

      Runtime::preregister_task_variant<compute_cfs_task>(
        registrar,
        compute_cfs_task_name);
    }
#endif
  }
}

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End:
