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
#ifndef HYPERION_SYNTHESIS_A_TERM_ILLUMINATION_FUNCTION_H_
#define HYPERION_SYNTHESIS_A_TERM_ILLUMINATION_FUNCTION_H_

#include <hyperion/synthesis/CFTable.h>
#include <cmath>

#define HYPERION_A_TERM_ILLUMINATION_FUNCTION_AXES                  \
  CF_BASELINE_CLASS, CF_PARALLACTIC_ANGLE, CF_FREQUENCY, CF_STOKES

#include <hyperion/synthesis/ATermZernikeModel.h>
#include <hyperion/synthesis/GridCoordinateTable.h>

#include <fftw3.h>

namespace hyperion {
namespace synthesis {

/**
 * Helper table for ATermTable. For aperture illumination function values on a
 * grid derived from a polynomial function representation of a Zernike
 * expansion, with dependence on baseline class, parallactic angle, frequency,
 * and Stokes parameter value.
 */
class HYPERION_EXPORT ATermIlluminationFunction
  : public CFTable<HYPERION_A_TERM_ILLUMINATION_FUNCTION_AXES> {
public:

  /**
   * ATermIlluminationFunction constructor
   *
   * @param ctx Legion Context
   * @param rt Legion Runtime pointer
   * @param grid_size size of CF grid in either dimension
   * @param zernike_order order of Zernike expansion
   * @param baseline_classes baseline class axis values
   * @param parallactic_angles parallactic angle axis values
   * @param frequencies frequency axis values
   * @param stokes_values Stokes axis values
   */
  ATermIlluminationFunction(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const size_t& grid_size,
    unsigned zernike_order,
    const std::vector<typename cf_table_axis<CF_BASELINE_CLASS>::type>&
      baseline_classes,
    const std::vector<typename cf_table_axis<CF_PARALLACTIC_ANGLE>::type>&
      parallactic_angles,
    const std::vector<typename cf_table_axis<CF_FREQUENCY>::type>&
      frequencies,
    const std::vector<typename cf_table_axis<CF_STOKES>::type>&
      stokes_values);

  /** baseline class axis dimension index */
  static const constexpr unsigned d_blc =
    cf_indexing::index_of<
      CF_BASELINE_CLASS,
      HYPERION_A_TERM_ILLUMINATION_FUNCTION_AXES>
    ::type::value;
  /** parallactic angle axis dimension index */
  static const constexpr unsigned d_pa =
    cf_indexing::index_of<
      CF_PARALLACTIC_ANGLE,
      HYPERION_A_TERM_ILLUMINATION_FUNCTION_AXES>
    ::type::value;
  /** frequency axis dimension index */
  static const constexpr unsigned d_frq =
    cf_indexing::index_of<
      CF_FREQUENCY,
      HYPERION_A_TERM_ILLUMINATION_FUNCTION_AXES>
    ::type::value;
  /** Stokes axis dimension index */
  static const constexpr unsigned d_sto =
    cf_indexing::index_of<
      CF_STOKES,
      HYPERION_A_TERM_ILLUMINATION_FUNCTION_AXES>
    ::type::value;

  /** grid X-axis dimension index */
  static const constexpr unsigned d_x = index_rank;
  /** grid Y-axis dimension index */
  static const constexpr unsigned d_y = d_x + 1;

  // We use a GridCoordinateTable that is augmented with a column designed
  // for a branch-free evaluation of polynomial functions that are zero outside
  // the unit disk
  /**
   * Value exponent dimension index
   *
   * Domain points are stored as two values: if the point p is within the unit
   * disk, the values are p_i^0, p_i^1; outside, the values are 0, 0.
   */
  static const constexpr unsigned d_power =
    GridCoordinateTable::coord_rank;
  static const constexpr unsigned ept_rank = d_power + 1;
  static const constexpr Legion::FieldID EPT_X_FID =
    2 * GridCoordinateTable::COORD_X_FID;
  static const constexpr Legion::FieldID EPT_Y_FID =
    2 * GridCoordinateTable::COORD_Y_FID;
  static const constexpr char* EPT_X_NAME = "EPT_X";
  static const constexpr char* EPT_Y_NAME = "EPT_Y";
  typedef CFTableBase::cf_fp_t ept_t;
  template <Legion::PrivilegeMode MODE, bool CHECK_BOUNDS=HYPERION_CHECK_BOUNDS>
  using ept_accessor_t =
    Legion::FieldAccessor<
      MODE,
      ept_t,
      ept_rank,
      Legion::coord_t,
      Legion::AffineAccessor<ept_t, ept_rank, Legion::coord_t>,
      CHECK_BOUNDS>;
  template <
    template <typename, int, typename> typename A = Legion::GenericAccessor,
    typename COORD_T = Legion::coord_t>
  using EPtColumn =
    PhysicalColumnTD<
      ValueType<ept_t>::DataType,
      GridCoordinateTable::index_rank,
      ept_rank,
      A,
      COORD_T>;

protected:

  static void
  add_epts_columns(
    Legion::Context ctx,
    Legion::Runtime* rt,
    GridCoordinateTable& gc);

  void
  compute_epts(
    Legion::Context ctx,
    Legion::Runtime* rt,
    GridCoordinateTable& gc,
    const ColumnSpacePartition& partition = ColumnSpacePartition()) const;

public:

  static const constexpr char* compute_epts_task_name =
    "ATermIlluminationFunction::compute_epts_task";

  static Legion::TaskID compute_epts_task_id;

  template <typename execution_space>
  static void
  compute_epts_task(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& regions,
    Legion::Context ctx,
    Legion::Runtime* rt) {

    const Table::Desc& tdesc = *static_cast<const Table::Desc*>(task->args);

    auto pt =
      PhysicalTable::create_all_unsafe(rt, {tdesc}, task->regions, regions)[0];

    auto kokkos_work_space =
      rt->get_executing_processor(ctx).kokkos_work_space();

    CFPhysicalTable<CF_PARALLACTIC_ANGLE> gc(pt);

    // coordinates columns
    auto cx_col =
      GridCoordinateTable::CoordColumn<Legion::AffineAccessor>(
        *gc.column(GridCoordinateTable::COORD_X_NAME).value());
    auto cx_rect = cx_col.rect();
    auto cxs = cx_col.view<execution_space, LEGION_READ_ONLY>();
    auto cys =
      GridCoordinateTable::CoordColumn<Legion::AffineAccessor>(
        *gc.column(GridCoordinateTable::COORD_Y_NAME).value())
      .view<execution_space, LEGION_READ_ONLY>();

    // polynomial function evaluation points columns
    auto xpts =
      EPtColumn<Legion::AffineAccessor>(*gc.column(EPT_X_NAME).value())
      .view<execution_space, LEGION_WRITE_DISCARD>();
    auto ypts =
      EPtColumn<Legion::AffineAccessor>(*gc.column(EPT_Y_NAME).value())
      .view<execution_space, LEGION_WRITE_DISCARD>();

    Kokkos::parallel_for(
      Kokkos::MDRangePolicy<
        Kokkos::Rank<GridCoordinateTable::coord_rank>,
        execution_space>(
          kokkos_work_space,
          rect_zero(cx_rect),
          rect_size(cx_rect)),
      KOKKOS_LAMBDA(long pa_l, long x_l, long y_l) {
        // Outside of the unit disk, the function should evaluate to zero, which
        // is achieved by setting the X and Y vectors to zero.
        auto& cx = cxs(pa_l, x_l, y_l);
        auto& cy = cys(pa_l, x_l, y_l);
        ept_t ept0 = ((cx * cx + cy * cy <= 1.0) ? 1.0 : 0.0);
        xpts(pa_l, x_l, y_l, 0) = ypts(pa_l, x_l, y_l, 0) = ept0;
        xpts(pa_l, x_l, y_l, 1) = cx * ept0;
        ypts(pa_l, x_l, y_l, 1) = cy * ept0;
      });
  }

  /**
   * Compute the values of the aperture illumination function column
   *
   * This is the main computational task for this table; it launches a sequence
   * of sub-tasks to compute the values of the aperture illumination function.
   *
   * @param ctx Legion Context
   * @param rt Legion Runtime
   * @param gc grid coordinate system
   * @param zmodel Zernike expansion of aperture voltage pattern
   * @param coords image coordinate system
   * @param partition table partition
   * @param fftw_flags FFTW planner flags, ignored by CUDA implementation
   * @param fftw_timelimit FFTW planner time limit (seconds),
   *                       ignored by CUDA implementation
   */
  void
  compute_jones(
    Legion::Context ctx,
    Legion::Runtime* rt,
    GridCoordinateTable& gc,
    const ATermZernikeModel& zmodel,
    const ColumnSpacePartition& partition = ColumnSpacePartition(),
    unsigned fftw_flags = FFTW_MEASURE,
    double fftw_timelimit = 5.0) const;

protected:

  void
  compute_aifs(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const ATermZernikeModel& zmodel,
    const GridCoordinateTable& gc,
    const ColumnSpacePartition& partition) const;


  void
  compute_fft(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const ColumnSpacePartition& partition,
    unsigned fftw_flags,
    double fftw_timelimit) const;

public:

  static const constexpr char* compute_aifs_task_name =
    "ATermIlluminationFunction::compute_aifs_task";

  static Legion::TaskID compute_aifs_task_id;

  struct ComputeAIFsTaskArgs {
    Table::Desc zmodel;
    Table::Desc gc;
    Table::Desc aif;
  };

  template <typename execution_space>
  static void
  compute_aifs_task(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& regions,
    Legion::Context ctx,
    Legion::Runtime* rt) {

    const ComputeAIFsTaskArgs& args =
      *static_cast<ComputeAIFsTaskArgs*>(task->args);
    std::vector<Table::Desc> descs{args.zmodel, args.gc, args.aif};

    auto pts =
      PhysicalTable::create_all_unsafe(rt, descs, task->regions, regions);

    auto kokkos_work_space =
      rt->get_executing_processor(ctx).kokkos_work_space();

    CFPhysicalTable<HYPERION_A_TERM_ZERNIKE_MODEL_AXES> zmodel(pts[0]);
    CFPhysicalTable<CF_PARALLACTIC_ANGLE> gc(pts[1]);
    CFPhysicalTable<HYPERION_A_TERM_ILLUMINATION_FUNCTION_AXES> aif(pts[2]);

    // polynomial function coefficients column
    auto pc_col =
      ATermZernikeModel::PCColumn<Legion::AffineAccessor>(
        *zmodel.column(ATermZernikeModel::PC_NAME).value());
    auto pcs = pc_col.view<execution_space, LEGION_READ_ONLY>();

    // polynomial function evaluation points columns
    auto xpt_col =
      EPtColumn<Legion::AffineAccessor>(*gc.column(EPT_X_NAME).value());
    auto xpts = xpt_col.view<execution_space, LEGION_READ_ONLY>();
    auto ypt_col =
      EPtColumn<Legion::AffineAccessor>(*gc.column(EPT_Y_NAME).value());
    auto ypts = ypt_col.view<execution_space, LEGION_READ_ONLY>();

    // polynomial function values column
    auto value_col = aif.template value<Legion::AffineAccessor>();
    auto value_rect = value_col.rect();
    auto values =
      value_col.template view<execution_space, LEGION_WRITE_DISCARD>();

    // CUDA compilation fails without the following redundant definitions. Note
    // that similar usage in compute_epts_task works. TODO: remove these
    unsigned dd_blc = d_blc;
    unsigned dd_pa = d_pa;
    unsigned dd_frq = d_frq;
    unsigned dd_sto = d_sto;
    unsigned dd_x = d_x;
    unsigned dd_y = d_y;
    typedef typename Kokkos::TeamPolicy<execution_space>::member_type
      member_type;
    typedef Kokkos::View<
      ATermZernikeModel::pc_t*,
      typename execution_space::scratch_memory_space,
      Kokkos::MemoryTraits<Kokkos::Unmanaged>> shared_pc_1d;
    Kokkos::parallel_for(
      Kokkos::TeamPolicy<execution_space>(
        kokkos_work_space,
        linearized_index_range(value_rect),
        Kokkos::AUTO())
      .set_scratch_size(
        0,
        Kokkos::PerTeam(
          (zernike_max_order::value + 1) * sizeof(ATermZernikeModel::pc_t))),
      KOKKOS_LAMBDA(const member_type& team_member) {
        auto pt =
          multidimensional_index_l(
            static_cast<Legion::coord_t>(team_member.league_rank()),
            value_rect);
        auto& blc_l = pt[dd_blc];
        auto& pa_l = pt[dd_pa];
        auto& frq_l = pt[dd_frq];
        auto& sto_l = pt[dd_sto];
        auto& x_l = pt[dd_x];
        auto& y_l = pt[dd_y];
        auto xpt = Kokkos::subview(xpts, pa_l, x_l, y_l, Kokkos::ALL);
        auto ypt = Kokkos::subview(ypts, pa_l, x_l, y_l, Kokkos::ALL);
        auto pc =
          Kokkos::subview(pcs, blc_l, frq_l, sto_l, Kokkos::ALL, Kokkos::ALL);
        auto tmp = shared_pc_1d(team_member.team_scratch(0), pc.extent(0));
        Kokkos::parallel_for(
          Kokkos::TeamThreadRange(team_member, pc.extent(0)),
          [=](const int& i) {
            tmp(i) = (ATermZernikeModel::pc_t)0.0;
            for (int j = pc.extent(1) - 1; j > 0; --j)
              tmp(i) = (tmp(i) + pc(i, j)) * ypt(1);
            tmp(i) = (tmp(i) + pc(i, 0)) * ypt(0);
          });
        team_member.team_barrier();
        auto& v = values(blc_l, pa_l, frq_l, sto_l, x_l, y_l);
        v = (ATermZernikeModel::pc_t)0.0;
        for (int i = pc.extent(0) - 1; i > 0; --i)
          v = (v + tmp(i)) * xpt(1);
        v = (v + tmp(0)) * xpt(0);
      });
  }

  static void
  preregister_tasks();

};

} // end namespace synthesis
} // end namespace hyperion

#endif // HYPERION_SYNTHESIS_A_TERM_ILLUMINATION_FUNCTION_H_

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End:
