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
#ifndef HYPERION_SYNTHESIS_DIRECTION_COORDINATE_TABLE_H_
#define HYPERION_SYNTHESIS_DIRECTION_COORDINATE_TABLE_H_

#include <hyperion/hyperion.h>
#include <hyperion/synthesis/CFTable.h>

#include <casacore/coordinates/Coordinates/DirectionCoordinate.h>

#include <array>
#include <vector>

namespace hyperion {
namespace synthesis {

class HYPERION_EXPORT DirectionCoordinateTable
  : public CFTable<CF_PARALLACTIC_ANGLE> {
public:

  DirectionCoordinateTable(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const std::array<size_t, 2>& cf_size,
    const std::vector<typename cf_table_axis<CF_PARALLACTIC_ANGLE>::type>&
      parallactic_angles);

  static const constexpr unsigned d_pa = 0;

  /**
   * columns of world coordinates
   */
  static const constexpr unsigned worldc_rank = d_y + 1;
  static const constexpr Legion::FieldID WORLD_X_FID = 88;
  static const constexpr Legion::FieldID WORLD_Y_FID = 89;
  static const constexpr char* WORLD_X_NAME = "WORLD_X";
  static const constexpr char* WORLD_Y_NAME = "WORLD_Y";
  typedef double worldc_t; // type used by casacore::DirectionCoordinate
  template <Legion::PrivilegeMode MODE, bool CHECK_BOUNDS=HYPERION_CHECK_BOUNDS>
  using worldc_accessor_t =
    Legion::FieldAccessor<
      MODE,
      worldc_t,
      worldc_rank,
      Legion::coord_t,
      Legion::AffineAccessor<worldc_t, worldc_rank, Legion::coord_t>,
      CHECK_BOUNDS>;
  template <
    template <typename, int, typename> typename A = Legion::GenericAccessor,
    typename COORD_T = Legion::coord_t>
  using WorldCColumn =
    PhysicalColumnTD<
      ValueType<worldc_t>::DataType,
      index_rank,
      worldc_rank,
      A,
      COORD_T>;

  void
  compute_world_coordinates(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const casacore::DirectionCoordinate& direction,
    const ColumnSpacePartition& partition = ColumnSpacePartition()) const;

  static void
  compute_world_coordinates(
    const CFPhysicalTable<CF_PARALLACTIC_ANGLE>& dc,
    const casacore::DirectionCoordinate& dc0,
    const std::array<double, 2>& pixel_offset);

  static const constexpr char* compute_world_coordinates_task_name =
    "DirectionCoordinateTable::compute_world_coordinates";

  static Legion::TaskID compute_world_coordinates_task_id;

  struct ComputeWorldCoordinatesTaskArgs {
    Table::Desc desc;
    std::array<double, 2> pixel_offset;
    std::array<char, direction_coordinate_serdez::MAX_SERIALIZED_SIZE> dc;
  };

  static void
  compute_world_coordinates_task(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& regions,
    Legion::Context ctx,
    Legion::Runtime* rt);

  static void
  preregister_tasks();
};

} // end namespace synthesis
} // end namespace hyperion

#endif // HYPERION_SYNTHESIS_DIRECTION_COORDINATE_TABLE_H_

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End:
