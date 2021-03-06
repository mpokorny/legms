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
#include <hyperion/synthesis/ATermIlluminationFunction.h>
#include <hyperion/synthesis/FFT.h>

using namespace hyperion;
using namespace hyperion::synthesis;
using namespace Legion;

namespace cc = casacore;

TaskID ATermIlluminationFunction::compute_epts_task_id;
TaskID ATermIlluminationFunction::compute_aifs_task_id;

#if !HAVE_CXX17
const constexpr unsigned ATermIlluminationFunction::d_blc;
const constexpr unsigned ATermIlluminationFunction::d_pa;
const constexpr unsigned ATermIlluminationFunction::d_frq;
const constexpr unsigned ATermIlluminationFunction::d_sto;
const constexpr unsigned ATermIlluminationFunction::d_power;
const constexpr unsigned ATermIlluminationFunction::ept_rank;
const constexpr FieldID ATermIlluminationFunction::EPT_X_FID;
const constexpr FieldID ATermIlluminationFunction::EPT_Y_FID;
const constexpr char* ATermIlluminationFunction::EPT_X_NAME;
const constexpr char* ATermIlluminationFunction::EPT_Y_NAME;
#endif // !HAVE_CXX17

#define USE_KOKKOS_SERIAL_COMPUTE_EPTS_TASK // undef to disable
#define USE_KOKKOS_OPENMP_COMPUTE_EPTS_TASK // undef to disable
#define USE_KOKKOS_CUDA_COMPUTE_EPTS_TASK // undef to disable

#define USE_KOKKOS_SERIAL_COMPUTE_AIFS_TASK // undef to disable
#define USE_KOKKOS_OPENMP_COMPUTE_AIFS_TASK // undef to disable
#define USE_KOKKOS_CUDA_COMPUTE_AIFS_TASK // undef to disable

void
ATermIlluminationFunction::add_epts_columns(
  Context ctx,
  Runtime* rt,
  GridCoordinateTable& gc) {

  Rect<GridCoordinateTable::coord_rank> w_rect(
    rt->get_index_space_domain(
      gc.columns().at(GridCoordinateTable::COORD_X_NAME).cs.column_is));
  Rect<ept_rank> ept_rect;
  for (size_t i = 0; i < GridCoordinateTable::coord_rank; ++i) {
    ept_rect.lo[i] = w_rect.lo[i];
    ept_rect.hi[i] = w_rect.hi[i];
  }
  ept_rect.lo[d_power] = 0;
  ept_rect.hi[d_power] = 1;
  IndexSpace is = rt->create_index_space(ctx, ept_rect);
  ColumnSpace ept_cs =
    ColumnSpace::create<cf_table_axes_t>(
      ctx,
      rt,
      {CF_PARALLACTIC_ANGLE, CF_X, CF_Y, CF_ORDER0},
      is,
      false);
  Table::fields_t tflds =
    {{ept_cs,
      {{EPT_X_NAME, TableField(ValueType<ept_t>::DataType, EPT_X_FID)},
       {EPT_Y_NAME, TableField(ValueType<ept_t>::DataType, EPT_Y_FID)}}}};
  gc.add_columns(ctx, rt, std::move(tflds));
  rt->destroy_index_space(ctx, is);
}

void
ATermIlluminationFunction::compute_epts(
  Context ctx,
  Runtime* rt,
  GridCoordinateTable& gc,
  const ColumnSpacePartition& partition) const {

  add_epts_columns(ctx, rt, gc);

  // compute grid coordinates via augmented GridCoordinateTable
  {
    auto ro_colreqs = Column::default_requirements;
    ro_colreqs.values.privilege = LEGION_READ_ONLY;
    ro_colreqs.values.mapped = true;
    auto wd_colreqs = Column::default_requirements;
    wd_colreqs.values.privilege = LEGION_WRITE_DISCARD;
    wd_colreqs.values.mapped = true;
    auto part =
      gc.columns().at(GridCoordinateTable::COORD_X_NAME)
      .narrow_partition(ctx, rt, partition)
      .value_or(ColumnSpacePartition());
    auto reqs =
      gc.requirements(
        ctx,
        rt,
        part,
        {{GridCoordinateTable::COORD_X_NAME, ro_colreqs},
         {GridCoordinateTable::COORD_Y_NAME, ro_colreqs},
         {EPT_X_NAME, wd_colreqs},
         {EPT_Y_NAME, wd_colreqs}},
        CXX_OPTIONAL_NAMESPACE::nullopt);
#if HAVE_CXX17
    auto& [treqs, tparts, tdesc] = reqs;
#else // !HAVE_CXX17
    auto& treqs = std::get<0>(reqs);
    auto& tparts = std::get<1>(reqs);
    auto& tdesc = std::get<2>(reqs);
#endif // HAVE_CXX17
    if (!part.is_valid()) {
      TaskLauncher task(
        compute_epts_task_id,
        TaskArgument(&tdesc, sizeof(tdesc)),
        Predicate::TRUE_PRED,
        table_mapper);
      for (auto& r : treqs)
        task.add_region_requirement(r);
      rt->execute_task(ctx, task);
    } else {
      IndexTaskLauncher task(
        compute_epts_task_id,
        rt->get_index_partition_color_space(ctx, part.column_ip),
        TaskArgument(&tdesc, sizeof(tdesc)),
        ArgumentMap(),
        Predicate::TRUE_PRED,
        false,
        table_mapper);
      for (auto& r : treqs)
        task.add_region_requirement(r);
      rt->execute_index_space(ctx, task);
    }
    for (auto& p : tparts)
      p.destroy(ctx, rt);
    if (part.is_valid() && part != partition)
      part.destroy(ctx, rt);
  }
}

void
ATermIlluminationFunction::compute_aifs(
  Context ctx,
  Runtime* rt,
  const ATermZernikeModel& zmodel,
  const GridCoordinateTable& gc,
  const ColumnSpacePartition& partition) const {

  // execute compute_aifs_task
  ComputeAIFsTaskArgs args;

  std::vector<RegionRequirement> all_reqs;
  std::vector<ColumnSpacePartition> all_parts;

  // zmodel table, READ_ONLY privileges on polynomial coefficients region
  {
    auto ro_colreqs = Column::default_requirements;
    ro_colreqs.values.privilege = READ_ONLY;
    ro_colreqs.values.mapped = true;

    auto reqs =
      zmodel.requirements(
        ctx,
        rt,
        partition,
        {{ATermZernikeModel::PC_NAME, ro_colreqs}},
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
    args.zmodel = tdesc;
  }
  // gc table, READ_ONLY privileges on epts columns
  {
    auto ro_colreqs = Column::default_requirements;
    ro_colreqs.values.privilege = READ_ONLY;
    ro_colreqs.values.mapped = true;

    auto reqs =
      gc.requirements(
        ctx,
        rt,
        partition,
        {{ATermIlluminationFunction::EPT_X_NAME, ro_colreqs},
         {ATermIlluminationFunction::EPT_Y_NAME, ro_colreqs}},
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
  // this table, WRITE_DISCARD privileges on values and weights
  {
    auto wd_colreqs = Column::default_requirements;
    wd_colreqs.values.privilege = WRITE_DISCARD;
    wd_colreqs.values.mapped = true;

    auto reqs =
      requirements(
        ctx,
        rt,
        partition,
        {{CFTableBase::CF_VALUE_COLUMN_NAME, wd_colreqs},
         {CFTableBase::CF_WEIGHT_COLUMN_NAME, wd_colreqs}},
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
    args.aif = tdesc;
  }
  TaskArgument ta(&args, sizeof(args));
  if (!partition.is_valid()) {
    TaskLauncher task(
      compute_aifs_task_id,
      ta,
      Predicate::TRUE_PRED,
      table_mapper);
    for (auto& r : all_reqs)
      task.add_region_requirement(r);
    rt->execute_task(ctx, task);
  } else {
    IndexTaskLauncher task(
      compute_aifs_task_id,
      rt->get_index_partition_color_space(ctx, partition.column_ip),
      ta,
      ArgumentMap(),
      Predicate::TRUE_PRED,
      false,
      table_mapper);
    for (auto& r : all_reqs)
      task.add_region_requirement(r);
    rt->execute_index_space(ctx, task);
  }
  for (auto& p : all_parts)
    p.destroy(ctx, rt);
}

void
ATermIlluminationFunction::compute_fft(
  Context ctx,
  Runtime* rt,
  const ColumnSpacePartition& partition,
  unsigned fftw_flags,
  double fftw_timelimit) const {

  // READ_WRITE privileges on values and weights
  auto rw_colreqs = Column::default_requirements;
  rw_colreqs.values.privilege = LEGION_READ_WRITE;
  rw_colreqs.values.mapped = true;
  auto reqs =
    requirements(
      ctx,
      rt,
      partition,
      {{CFTableBase::CF_VALUE_COLUMN_NAME, rw_colreqs},
       {CFTableBase::CF_WEIGHT_COLUMN_NAME, rw_colreqs}},
      CXX_OPTIONAL_NAMESPACE::nullopt);
#if HAVE_CXX17
  auto& [treqs, tparts, tdesc] = reqs;
#else // !HAVE_CXX17
  auto& treqs = std::get<0>(reqs);
  auto& tparts = std::get<1>(reqs);
  auto& tdesc = std::get<2>(reqs);
#endif // HAVE_CXX17
  FFT::Args args;
  args.desc.rank = 2;
  args.desc.precision =
    ((typeid(CFTableBase::cf_fp_t) == typeid(float))
     ? FFT::Precision::SINGLE
     : FFT::Precision::DOUBLE);
  args.desc.transform = FFT::Type::C2C;
  args.desc.sign = -1;
  args.rotate_in = false;
  args.rotate_out = true;
  args.seconds = fftw_timelimit;
  args.flags = fftw_flags;
  for (auto& fid : {CFTableBase::CF_VALUE_FID/*, CFTableBase::CF_WEIGHT_FID*/}) {
    // FFT::in_place needs a simple RegionRequirement: find the requirement for
    // the column, copy it, and ensure the copy includes just the desired field
    RegionRequirement req;
    for (auto& r : treqs) {
      if (r.privilege_fields.count(fid) > 0) {
        req = r;
        req.privilege_fields.clear();
        req.privilege_fields.insert(fid);
        req.instance_fields.clear();
        if (std::find(r.instance_fields.begin(), r.instance_fields.end(), fid)
            != r.instance_fields.end())
          req.instance_fields.push_back(fid);
        break;
      }
    }
    assert(req.privilege_fields.size() == 1);
    args.fid = fid;
    if (!partition.is_valid()) {
      TaskLauncher
        task(FFT::in_place_task_id, TaskArgument(&args, sizeof(args)));
      task.add_region_requirement(req);
      rt->execute_task(ctx, task);
    } else {
      IndexTaskLauncher task(
        FFT::in_place_task_id,
        rt->get_index_partition_color_space(ctx, partition.column_ip),
        TaskArgument(&args, sizeof(args)),
        ArgumentMap());
      task.add_region_requirement(req);
      rt->execute_index_space(ctx, task);
    }
  }
  for (auto& p : tparts)
    p.destroy(ctx, rt);
}

ATermIlluminationFunction::ATermIlluminationFunction(
  Context ctx,
  Runtime* rt,
  const size_t& grid_size,
  unsigned zernike_order,
  const std::vector<typename cf_table_axis<CF_BASELINE_CLASS>::type>&
    baseline_classes,
  const std::vector<typename cf_table_axis<CF_PARALLACTIC_ANGLE>::type>&
    parallactic_angles,
  const std::vector<typename cf_table_axis<CF_FREQUENCY>::type>&
    frequencies,
  const std::vector<typename cf_table_axis<CF_STOKES>::type>&
    stokes_values)
  : CFTable(
    ctx,
    rt,
    grid_size,
    Axis<CF_BASELINE_CLASS>(baseline_classes),
    Axis<CF_PARALLACTIC_ANGLE>(parallactic_angles),
    Axis<CF_FREQUENCY>(frequencies),
    Axis<CF_STOKES>(stokes_values)) {
}

void
ATermIlluminationFunction::compute_jones(
  Context ctx,
  Runtime* rt,
  GridCoordinateTable& gc,
  const ATermZernikeModel& zmodel,
  const ColumnSpacePartition& partition,
  unsigned fftw_flags,
  double fftw_timelimit) const {

  // add "ept" columns to gc table
  compute_epts(ctx, rt, gc, partition);

  // execute compute_aifs_task
  compute_aifs(ctx, rt, zmodel, gc, partition);

  // FFT on the values region
  compute_fft(ctx, rt, partition, fftw_flags, fftw_timelimit);
}

#define USE_KOKKOS_VARIANT(V, T)                \
  (defined(USE_KOKKOS_##V##_COMPUTE_##T) &&     \
   defined(KOKKOS_ENABLE_##V))

void
ATermIlluminationFunction::preregister_tasks() {
  //
  // compute_epts_task
  //
  {
    // in the augmented GridCoordinateTable the two EPT columns share an
    // index space; use an AOS layout for CPUs as default for that reason
#if USE_KOKKOS_VARIANT(SERIAL, EPTS_TASK) || \
  USE_KOKKOS_VARIANT(OPENMP, EPTS_TASK)
    LayoutConstraintRegistrar cpu_constraints(
      FieldSpace::NO_SPACE,
      "ATermIlluminationFunction::compute_epts");
    add_aos_right_ordering_constraint(cpu_constraints);
    cpu_constraints.add_constraint(
      SpecializedConstraint(LEGION_AFFINE_SPECIALIZE));
    auto cpu_layout_id = Runtime::preregister_layout(cpu_constraints);
#endif

#if USE_KOKKOS_VARIANT(CUDA, EPTS_TASK)
    LayoutConstraintRegistrar gpu_constraints(
      FieldSpace::NO_SPACE,
      "ATermIlluminationFunction::compute_epts");
    add_soa_left_ordering_constraint(gpu_constraints);
    gpu_constraints.add_constraint(
      SpecializedConstraint(LEGION_AFFINE_SPECIALIZE));
    auto gpu_layout_id = Runtime::preregister_layout(gpu_constraints);
#endif

    compute_epts_task_id = Runtime::generate_static_task_id();

#if USE_KOKKOS_VARIANT(OPENMP, EPTS_TASK)
    {
      TaskVariantRegistrar
        registrar(compute_epts_task_id, compute_epts_task_name);
      registrar.add_constraint(ProcessorConstraint(Processor::OMP_PROC));
      registrar.set_leaf();
      registrar.set_idempotent();

      registrar.add_layout_constraint_set(
        TableMapper::to_mapping_tag(TableMapper::default_column_layout_tag),
        cpu_layout_id);

      Runtime::preregister_task_variant<compute_epts_task<Kokkos::OpenMP>>(
        registrar,
        compute_epts_task_name);
    }
#endif

#if USE_KOKKOS_VARIANT(CUDA, EPTS_TASK)
    {
      TaskVariantRegistrar
        registrar(compute_epts_task_id, compute_epts_task_name);
      registrar.add_constraint(ProcessorConstraint(Processor::TOC_PROC));
      registrar.set_leaf();
      registrar.set_idempotent();

      registrar.add_layout_constraint_set(
        TableMapper::to_mapping_tag(TableMapper::default_column_layout_tag),
        gpu_layout_id);

      Runtime::preregister_task_variant<compute_epts_task<Kokkos::Cuda>>(
        registrar,
        compute_epts_task_name);
    }
#endif

#if USE_KOKKOS_VARIANT(SERIAL, EPTS_TASK)
    {
      TaskVariantRegistrar
        registrar(compute_epts_task_id, compute_epts_task_name);
      registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
      registrar.set_leaf();
      registrar.set_idempotent();

      registrar.add_layout_constraint_set(
        TableMapper::to_mapping_tag(TableMapper::default_column_layout_tag),
        cpu_layout_id);

      Runtime::preregister_task_variant<compute_epts_task<Kokkos::Serial>>(
        registrar,
        compute_epts_task_name);
    }
#endif
  }

  //
  // compute_aifs_task
  //
  {
    // the only table with two columns sharing an index space is
    // ATermIlluminationFunction, using EPT_X and EPT_Y; use an AOS layout for
    // CPUs as default for that reason
#if USE_KOKKOS_VARIANT(SERIAL, AIFS_TASK) ||    \
  USE_KOKKOS_VARIANT(OPENMP, AIFS_TASK)
    LayoutConstraintRegistrar cpu_constraints(
      FieldSpace::NO_SPACE,
      "ATermIlluminationFunction::compute_aifs");
    add_aos_right_ordering_constraint(cpu_constraints);
    cpu_constraints.add_constraint(
      SpecializedConstraint(LEGION_AFFINE_SPECIALIZE));
    auto cpu_layout_id = Runtime::preregister_layout(cpu_constraints);
#endif

#if USE_KOKKOS_VARIANT(CUDA, AIFS_TASK)
    LayoutConstraintRegistrar
      gpu_constraints(
        FieldSpace::NO_SPACE,
        "ATermIlluminationFunction::compute_aifs");
    add_soa_left_ordering_constraint(gpu_constraints);
    gpu_constraints.add_constraint(
      SpecializedConstraint(LEGION_AFFINE_SPECIALIZE));
    auto gpu_layout_id = Runtime::preregister_layout(gpu_constraints);
#endif
    compute_aifs_task_id = Runtime::generate_static_task_id();

#if USE_KOKKOS_VARIANT(SERIAL, AIFS_TASK)
    {
      TaskVariantRegistrar
        registrar(compute_aifs_task_id, compute_aifs_task_name);
      registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
      registrar.set_leaf();
      registrar.set_idempotent();

      registrar.add_layout_constraint_set(
        TableMapper::to_mapping_tag(TableMapper::default_column_layout_tag),
        cpu_layout_id);

      Runtime::preregister_task_variant<compute_aifs_task<Kokkos::Serial>>(
        registrar,
        compute_aifs_task_name);
    }
#endif

#if USE_KOKKOS_VARIANT(OPENMP, AIFS_TASK)
    {
      TaskVariantRegistrar
        registrar(compute_aifs_task_id, compute_aifs_task_name);
      registrar.add_constraint(ProcessorConstraint(Processor::OMP_PROC));
      registrar.set_leaf();
      registrar.set_idempotent();

      registrar.add_layout_constraint_set(
        TableMapper::to_mapping_tag(TableMapper::default_column_layout_tag),
        cpu_layout_id);

      Runtime::preregister_task_variant<compute_aifs_task<Kokkos::OpenMP>>(
        registrar,
        compute_aifs_task_name);
    }
#endif

#if USE_KOKKOS_VARIANT(CUDA, AIFS_TASK)
    {
      TaskVariantRegistrar
        registrar(compute_aifs_task_id, compute_aifs_task_name);
      registrar.add_constraint(ProcessorConstraint(Processor::TOC_PROC));
      registrar.set_leaf();
      registrar.set_idempotent();

      registrar.add_layout_constraint_set(
        TableMapper::to_mapping_tag(TableMapper::default_column_layout_tag),
        gpu_layout_id);

      Runtime::preregister_task_variant<compute_aifs_task<Kokkos::Cuda>>(
        registrar,
        compute_aifs_task_name);
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
