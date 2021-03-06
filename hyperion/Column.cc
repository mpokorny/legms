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
#include <hyperion/Column.h>

#include <mappers/default_mapper.h>

using namespace hyperion;

using namespace Legion;

// FIXME: use GenericAccessor rather than AffineAccessor, or at least leave it
// as a parameter
template <typename T, int DIM, bool CHECK_BOUNDS=false>
using ROAccessor =
  FieldAccessor<
    READ_ONLY,
    T,
    DIM,
    coord_t,
    AffineAccessor<T, DIM, coord_t>,
    CHECK_BOUNDS>;

template <typename T, int DIM, bool CHECK_BOUNDS=false>
using WOAccessor =
  FieldAccessor<
    WRITE_ONLY,
    T,
    DIM,
    coord_t,
    AffineAccessor<T, DIM, coord_t>,
    CHECK_BOUNDS>;

const Column::Requirements Column::default_requirements{
  Column::Req{READ_ONLY, EXCLUSIVE, false},
  Column::Req{READ_ONLY, EXCLUSIVE, true},
  Column::Req{READ_ONLY, EXCLUSIVE, true},
  Column::Req{READ_ONLY, EXCLUSIVE, true},
  TableMapper::to_mapping_tag(TableMapper::default_column_layout_tag),
  0,
  LogicalPartition::NO_PART
};

const Column::Requirements Column::default_requirements_mapped{
  Column::Req{READ_ONLY, EXCLUSIVE, true},
  Column::Req{READ_ONLY, EXCLUSIVE, true},
  Column::Req{READ_ONLY, EXCLUSIVE, true},
  Column::Req{READ_ONLY, EXCLUSIVE, true},
  TableMapper::to_mapping_tag(TableMapper::default_column_layout_tag),
  0,
  LogicalPartition::NO_PART
};

template <hyperion::TypeTag DT>
static LogicalRegion
index_column(
  Context ctx,
  Runtime *rt,
  TaskID task_id,
  const RegionRequirement& col_req) {

  typedef typename DataType<DT>::ValueType T;
  static const constexpr size_t min_block_size = 10000;

  // launch index space task on input region to compute accumulator value
  std::vector<std::tuple<T, Column::COLUMN_INDEX_ROWS_TYPE>> acc;
  {
    IndexPartition ip =
      partition_over_default_tunable(
        ctx,
        rt,
        col_req.region.get_index_space(),
        min_block_size,
        Mapping::DefaultMapper::DefaultTunables::DEFAULT_TUNABLE_GLOBAL_CPUS);
    IndexSpace cs = rt->get_index_partition_color_space_name(ctx, ip);
    LogicalPartition col_lp =
      rt->get_logical_partition(ctx, col_req.region, ip);

    IndexTaskLauncher task(task_id, cs, TaskArgument(NULL, 0), ArgumentMap());
    task.add_region_requirement(
      RegionRequirement(col_lp, 0, READ_ONLY, EXCLUSIVE, col_req.region));
    task.add_field(0, *col_req.privilege_fields.begin());
    Future f =
      rt->execute_index_space(
        ctx,
        task,
        OpsManager::reduction_id(DataType<DT>::af_redop_id));
    rt->destroy_index_space(ctx, cs);
    rt->destroy_index_partition(ctx, ip);
    acc = f.get_result<acc_field_redop_rhs<T>>().v;
  }

  LogicalRegionT<1> result_lr;
  if (acc.size() > 0) {
    auto result_fs = rt->create_field_space(ctx);
    {
      auto fa = rt->create_field_allocator(ctx, result_fs);
      add_field(DT, fa, Column::COLUMN_INDEX_VALUE_FID);
      rt->attach_name(
        result_fs,
        Column::COLUMN_INDEX_VALUE_FID,
        "Column::index_value");
      fa.allocate_field(
        sizeof(Column::COLUMN_INDEX_ROWS_TYPE),
        Column::COLUMN_INDEX_ROWS_FID,
        OpsManager::serdez_id(OpsManager::V_DOMAIN_POINT_SID));
      rt->attach_name(
        result_fs,
        Column::COLUMN_INDEX_ROWS_FID,
        "Column::index_rows");
    }
    IndexSpaceT<1> result_is =
      rt->create_index_space(ctx, Rect<1>(0, acc.size() - 1));
    result_lr = rt->create_logical_region(ctx, result_is, result_fs);

    // transfer values and row numbers from acc_lr to result_lr
    RegionRequirement result_req(result_lr, WRITE_ONLY, EXCLUSIVE, result_lr);
    result_req.add_field(Column::COLUMN_INDEX_VALUE_FID);
    result_req.add_field(Column::COLUMN_INDEX_ROWS_FID);
    PhysicalRegion result_pr = rt->map_region(ctx, result_req);
    const WOAccessor<T, 1> values(result_pr, Column::COLUMN_INDEX_VALUE_FID);
    const WOAccessor<Column::COLUMN_INDEX_ROWS_TYPE, 1>
      rns(result_pr, Column::COLUMN_INDEX_ROWS_FID);
    for (size_t i = 0; i < acc.size(); ++i) {
      ::new (rns.ptr(i)) Column::COLUMN_INDEX_ROWS_TYPE;
      std::tie(values[i], rns[i]) = acc[i];
    }
    rt->unmap_region(ctx, result_pr);
  }
  return result_lr;
}

LogicalRegion
Column::create_index(Context ctx, Runtime* rt) const {
  LogicalRegion result;
  RegionRequirement req(region, READ_ONLY, EXCLUSIVE, region);
  req.add_field(fid);
  switch (dt) {
#define ICR(DT)                                 \
    case DT:                                    \
      result = index_column<DT>(                \
        ctx,                                    \
        rt,                                     \
        index_accumulate_task_id[(unsigned)DT], \
        req);                                   \
      break;
    HYPERION_FOREACH_DATATYPE(ICR);
#undef ICR
  default:
    assert(false);
    break;
  }
  return result;
}

TaskID Column::index_accumulate_task_id[HYPERION_NUM_TYPE_TAGS];

std::string Column::index_accumulate_task_name[HYPERION_NUM_TYPE_TAGS];

template <typename T, int DIM>
std::map<T, std::vector<DomainPoint>>
acc_d_pts(FieldID fid, const DomainT<DIM>& dom, const PhysicalRegion& pr) {
  std::map<T, std::vector<Legion::DomainPoint>> result;
  const ROAccessor<T, DIM> vals(pr, fid);
  for (PointInDomainIterator<DIM> pid(dom); pid();pid++) {
    if (result.count(vals[*pid]) == 0)
      result[vals[*pid]] = std::vector<DomainPoint>();
    result[vals[*pid]].push_back(*pid);
  }
  return result;
}

template <hyperion::TypeTag DT>
std::map<typename DataType<DT>::ValueType, std::vector<DomainPoint>>
acc_pts(Runtime* rt, const RegionRequirement& req, const PhysicalRegion& pr) {
  typedef typename DataType<DT>::ValueType T;
  assert(req.privilege_fields.size() == 1);
  Legion::FieldID fid = *(req.privilege_fields.begin());
  IndexSpace is = req.region.get_index_space();
  std::map<T, std::vector<DomainPoint>> result;
  switch (is.get_dim()) {
#define ACC_D_PTS(D)                                                  \
  case D: {                                                           \
    result = acc_d_pts<T,D>(fid, rt->get_index_space_domain(is), pr); \
    break;                                                            \
  }
  HYPERION_FOREACH_N(ACC_D_PTS)
#undef ACC_D_PTS
  default:
    assert(false);
    break;
  }
  return result;
}

#define INDEX_ACCUMULATE_TASK(DT)                                       \
  template <>                                                           \
  acc_field_redop_rhs<typename DataType<DT>::ValueType> \
  Column::index_accumulate_task<DT>(                                    \
    const Legion::Task* task,                                           \
    const std::vector<Legion::PhysicalRegion>& regions,                 \
    Legion::Context,                                                    \
    Legion::Runtime* rt) {                                              \
    std::map<                                                           \
      typename DataType<DT>::ValueType, \
      std::vector<Legion::DomainPoint>> pts = \
      acc_pts<DT>(rt, task->regions[0], regions[0]);                    \
    acc_field_redop_rhs<DataType<DT>::ValueType> result;                \
    result.v.reserve(pts.size());                                       \
    std::copy(pts.begin(), pts.end(), std::back_inserter(result.v));    \
    return result;                                                      \
  }
HYPERION_FOREACH_DATATYPE(INDEX_ACCUMULATE_TASK);
#undef INDEX_ACCUMULATE_TASK

template <hyperion::TypeTag DT>
void
Column::preregister_index_accumulate_task() {
  index_accumulate_task_id[(unsigned)DT] = Runtime::generate_static_task_id();
  index_accumulate_task_name[(unsigned)DT] =
    std::string("x::Column::index_accumulate_task<") + DataType<DT>::s
    + std::string(">");
  TaskVariantRegistrar registrar(
    index_accumulate_task_id[(unsigned)DT],
    index_accumulate_task_name[(unsigned)DT].c_str());
  registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
  registrar.set_leaf();
  registrar.set_idempotent();
  // registrar.set_replicable();
  Runtime::preregister_task_variant<
    acc_field_redop_rhs<typename DataType<DT>::ValueType>,
    index_accumulate_task<DT>>(
    registrar,
    index_accumulate_task_name[(unsigned)DT].c_str());
}

CXX_OPTIONAL_NAMESPACE::optional<ColumnSpacePartition>
Column::narrow_partition(
  Context ctx,
  Runtime* rt,
  const ColumnSpacePartition& part,
  const std::set<int>& block,
  bool nondegenerate) const {

  CXX_OPTIONAL_NAMESPACE::optional<ColumnSpacePartition> result;
  if (!nondegenerate || part.is_valid()) {
    if (part.is_valid() && part.column_space == cs) {
      result = part;
    } else if (!nondegenerate
               || part.column_space.axes_uid(ctx, rt) == cs.axes_uid(ctx, rt)) {
      std::vector<AxisPartition> ap;
      if (part.is_valid()) {
        auto axes = cs.axes(ctx, rt);
        std::set<int> permit(axes.begin(), axes.end());
        for (auto& b : block)
          permit.erase(b);
        for (int i = 0; i < part.color_dim(rt); ++i) {
          int d = part.partition[i].dim;
          if (permit.count(d) > 0)
            ap.push_back(part.partition[i]);
        }
      }
      if (!nondegenerate || ap.size() > 0)
        result =
          ColumnSpacePartition::create(ctx, rt, cs, ap)
          .get_result<ColumnSpacePartition>();
    }
  }
  return result;
}

void
Column::preregister_tasks() {
  {
    // index_accumulate_task
#define PREREG_TASK(DT)                         \
    preregister_index_accumulate_task<DT>();
    HYPERION_FOREACH_DATATYPE(PREREG_TASK);
#undef PREREG_TASK
  }
}

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End:
