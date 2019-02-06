#include "utility.h"
#include "ms/Column.h"
#include "ms/TableReadTask.h"
#include "tree_index_space.h"

using namespace legms;
using namespace legms::ms;
using namespace Legion;

std::once_flag SerdezManager::initialized;

FieldID
legms::add_field(
  casacore::DataType datatype,
  FieldAllocator fa,
  FieldID field_id) {

  FieldID result;

#define ALLOC_FLD(tp)                           \
  tp:                                           \
    result = fa.allocate_field(                 \
      sizeof(DataType<tp>::ValueType),          \
      field_id,                                 \
      DataType<tp>::serdez_id);

  switch (datatype) {

  case ALLOC_FLD(casacore::DataType::TpBool)
    break;

  case ALLOC_FLD(casacore::DataType::TpChar)
    break;

  case ALLOC_FLD(casacore::DataType::TpUChar)
    break;

  case ALLOC_FLD(casacore::DataType::TpShort)
    break;

  case ALLOC_FLD(casacore::DataType::TpUShort)
    break;

  case ALLOC_FLD(casacore::DataType::TpInt)
    break;

  case ALLOC_FLD(casacore::DataType::TpUInt)
    break;

  // case ALLOC_FLD(casacore::DataType::TpInt64)
  //   break;

  case ALLOC_FLD(casacore::DataType::TpFloat)
    break;

  case ALLOC_FLD(casacore::DataType::TpDouble)
    break;

  case ALLOC_FLD(casacore::DataType::TpComplex)
    break;

  case ALLOC_FLD(casacore::DataType::TpDComplex)
    break;

  case ALLOC_FLD(casacore::DataType::TpString)
    break;

  case casacore::DataType::TpQuantity:
    assert(false); // TODO: implement quantity-valued columns
    break;

  case casacore::DataType::TpRecord:
    assert(false); // TODO: implement record-valued columns
    break;

  case casacore::DataType::TpTable:
    assert(false); // TODO: implement table-valued columns
    break;

  default:
    assert(false);
    break;
  }

#undef ALLOC_FLD

  return result;
}

ProjectedIndexPartitionTask::ProjectedIndexPartitionTask(
  IndexSpace launch_space,
  LogicalPartition lp,
  LogicalRegion lr,
  args* global_arg)
  : IndexTaskLauncher(
    TASK_ID,
    launch_space,
    TaskArgument(
      global_arg,
      sizeof(ProjectedIndexPartitionTask::args)
      + global_arg->prjdim * sizeof(global_arg->dmap[0])),
    ArgumentMap()){

  add_region_requirement(
    RegionRequirement(lp, 0, WRITE_DISCARD, EXCLUSIVE, lr));
  add_field(0, IMAGE_RANGES_FID);
}

void
ProjectedIndexPartitionTask::dispatch(Context ctx, Runtime* runtime) {
  runtime->execute_index_space(ctx, *this);
}

template <int IPDIM, int PRJDIM>
void
pipt_impl(
  const Task* task,
  const std::vector<PhysicalRegion>& regions,
  Context,
  Runtime *runtime) {

  const ProjectedIndexPartitionTask::args* targs =
    static_cast<const ProjectedIndexPartitionTask::args*>(task->args);
  Legion::Rect<PRJDIM> bounds = targs->bounds;

  const FieldAccessor<
    WRITE_DISCARD,
    Rect<PRJDIM>,
    IPDIM,
    coord_t,
    Realm::AffineAccessor<Rect<PRJDIM>, IPDIM, coord_t>,
    false>
    image_ranges(regions[0], ProjectedIndexPartitionTask::IMAGE_RANGES_FID);

  DomainT<IPDIM> domain =
    runtime->get_index_space_domain(
      task->regions[0].region.get_index_space());
  for (PointInDomainIterator<IPDIM> pid(domain);
       pid();
       pid++) {

    Rect<PRJDIM> r;
    for (size_t i = 0; i < PRJDIM; ++i)
      if (0 <= targs->dmap[i]) {
        r.lo[i] = r.hi[i] = pid[targs->dmap[i]];
      } else {
        r.lo[i] = bounds.lo[i];
        r.hi[i] = bounds.hi[i];
      }
    image_ranges[*pid] = r;
  }
}

void
ProjectedIndexPartitionTask::base_impl(
  const Task* task,
  const std::vector<PhysicalRegion>& regions,
  Context ctx,
  Runtime *runtime) {

  const args* targs = static_cast<const args*>(task->args);
  IndexSpace is = task->regions[0].region.get_index_space();
  switch (is.get_dim()) {
#if MAX_DIM >= 1
  case 1:
    switch (targs->prjdim) {
#if MAX_DIM >= 1
    case 1:
      pipt_impl<1, 1>(task, regions, ctx, runtime);
      break;
#endif
#if MAX_DIM >= 2
    case 2:
      pipt_impl<1, 2>(task, regions, ctx, runtime);
      break;
#endif
#if MAX_DIM >= 3
    case 3:
      pipt_impl<1, 3>(task, regions, ctx, runtime);
      break;
#endif
#if MAX_DIM >= 4
    case 4:
      pipt_impl<1, 4>(task, regions, ctx, runtime);
      break;
#endif
    default:
      assert(false);
      break;
    }
    break;
#endif
#if MAX_DIM >= 2
  case 2:
    switch (targs->prjdim) {
#if MAX_DIM >= 1
    case 1:
      pipt_impl<2, 1>(task, regions, ctx, runtime);
      break;
#endif
#if MAX_DIM >= 2
    case 2:
      pipt_impl<2, 2>(task, regions, ctx, runtime);
      break;
#endif
#if MAX_DIM >= 3
    case 3:
      pipt_impl<2, 3>(task, regions, ctx, runtime);
      break;
#endif
#if MAX_DIM >= 4
    case 4:
      pipt_impl<2, 4>(task, regions, ctx, runtime);
      break;
#endif
    default:
      assert(false);
      break;
    }
    break;
#endif
#if MAX_DIM >= 3
  case 3:
    switch (targs->prjdim) {
#if MAX_DIM >= 1
    case 1:
      pipt_impl<3, 1>(task, regions, ctx, runtime);
      break;
#endif
#if MAX_DIM >= 2
    case 2:
      pipt_impl<3, 2>(task, regions, ctx, runtime);
      break;
#endif
#if MAX_DIM >= 3
    case 3:
      pipt_impl<3, 3>(task, regions, ctx, runtime);
      break;
#endif
#if MAX_DIM >= 4
    case 4:
      pipt_impl<3, 4>(task, regions, ctx, runtime);
      break;
#endif
    default:
      assert(false);
      break;
    }
    break;
#endif
#if MAX_DIM >= 4
  case 4:
    switch (targs->prjdim) {
#if MAX_DIM >= 1
    case 1:
      pipt_impl<4, 1>(task, regions, ctx, runtime);
      break;
#endif
#if MAX_DIM >= 2
    case 2:
      pipt_impl<4, 2>(task, regions, ctx, runtime);
      break;
#endif
#if MAX_DIM >= 3
    case 3:
      pipt_impl<4, 3>(task, regions, ctx, runtime);
      break;
#endif
#if MAX_DIM >= 4
    case 4:
      pipt_impl<4, 4>(task, regions, ctx, runtime);
      break;
#endif
    default:
      assert(false);
      break;
    }
    break;
#endif
  default:
    assert(false);
    break;
  }
}

void
ProjectedIndexPartitionTask::register_task(Runtime* runtime) {
  TASK_ID =
    runtime->generate_library_task_ids("legms::ProjectedIndexPartitionTask", 1);
  TaskVariantRegistrar registrar(TASK_ID, TASK_NAME);
  registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
  registrar.set_leaf();
  registrar.set_idempotent();
  runtime->register_task_variant<base_impl>(registrar);
}

TaskID ProjectedIndexPartitionTask::TASK_ID;

IndexPartition
projected_index_partition(
  Context ctx,
  Runtime* runtime,
  IndexPartition ip,
  IndexSpace prj_is,
  const std::vector<int>& dmap) {

  if (prj_is == IndexSpace::NO_SPACE)
    return IndexPartition::NO_PART;

  switch (ip.get_dim()) {
  case 1:
    switch (prj_is.get_dim()) {
    case 1:
      return
        projected_index_partition(
          ctx,
          runtime,
          IndexPartitionT<1>(ip),
          IndexSpaceT<1>(prj_is),
          {dmap[0]});
      break;
    case 2:
      return
        projected_index_partition(
          ctx,
          runtime,
          IndexPartitionT<1>(ip),
          IndexSpaceT<2>(prj_is),
          {dmap[0], dmap[1]});
      break;
    case 3:
      return
        projected_index_partition(
          ctx,
          runtime,
          IndexPartitionT<1>(ip),
          IndexSpaceT<3>(prj_is),
          {dmap[0], dmap[1], dmap[2]});
      break;
    default:
      assert(false);
      break;
    }
    break;

  case 2:
    switch (prj_is.get_dim()) {
    case 1:
      return
        projected_index_partition(
          ctx,
          runtime,
          IndexPartitionT<2>(ip),
          IndexSpaceT<1>(prj_is),
          {dmap[0]});
      break;
    case 2:
      return
        projected_index_partition(
          ctx,
          runtime,
          IndexPartitionT<2>(ip),
          IndexSpaceT<2>(prj_is),
          {dmap[0], dmap[1]});
      break;
    case 3:
      return
        projected_index_partition(
          ctx,
          runtime,
          IndexPartitionT<2>(ip),
          IndexSpaceT<3>(prj_is),
          {dmap[0], dmap[1], dmap[2]});
      break;
    default:
      assert(false);
      break;
    }
    break;

  case 3:
    switch (prj_is.get_dim()) {
    case 1:
      return
        projected_index_partition(
          ctx,
          runtime,
          IndexPartitionT<3>(ip),
          IndexSpaceT<1>(prj_is),
          {dmap[0]});
      break;
    case 2:
      return
        projected_index_partition(
          ctx,
          runtime,
          IndexPartitionT<3>(ip),
          IndexSpaceT<2>(prj_is),
          {dmap[0], dmap[1]});
      break;
    case 3:
      return
        projected_index_partition(
          ctx,
          runtime,
          IndexPartitionT<3>(ip),
          IndexSpaceT<3>(prj_is),
          {dmap[0], dmap[1], dmap[2]});
      break;
    default:
      assert(false);
      break;
    }
    break;
  default:
    assert(false);
    break;
  }
}

void
register_tasks(Runtime* runtime) {
  TableReadTask::register_task(runtime);
  TreeIndexSpace::register_tasks(runtime);
  Column::register_tasks(runtime);
  ProjectedIndexPartitionTask::register_task(runtime);
}

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End:
