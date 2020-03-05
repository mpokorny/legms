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
#ifndef HYPERION_MS_ANTENNA_COLUMNS_H_
#define HYPERION_MS_ANTENNA_COLUMNS_H_

#include <hyperion/hyperion.h>
#include <hyperion/Column.h>
#include <hyperion/MSTableColumns.h>

#pragma GCC visibility push(default)
# include <casacore/measures/Measures/MPosition.h>
# include <casacore/measures/Measures/MCPosition.h>

# include <any>
# include <memory>
# include <optional>
# include <unordered_map>
# include <variant>
# include <vector>
#pragma GCC visibility pop

namespace hyperion {

class HYPERION_API MSAntennaColumns
  : public MSTableColumnsBase {
public:

  typedef MSTableColumns<MS_ANTENNA> C;

  MSAntennaColumns(
    Legion::Runtime* rt,
    const Legion::RegionRequirement& rows_requirement,
    const std::unordered_map<std::string, Regions>& regions);

  static const constexpr unsigned row_rank = 1;

  Legion::DomainT<row_rank>
  rows() const {
    return m_rows;
  }

  //
  // NAME
  //
  static const constexpr unsigned name_rank =
    row_rank + C::element_ranks[C::col_t::MS_ANTENNA_COL_NAME];

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS>
  using NameAccessor =
    FieldAccessor<HYPERION_TYPE_STRING, name_rank, MODE, CHECK_BOUNDS>;

  bool
  has_name() const {
    return m_regions.count(C::col_t::MS_ANTENNA_COL_NAME) > 0;
  }

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  NameAccessor<MODE, CHECK_BOUNDS>
  name() const {
    return
      NameAccessor<MODE, CHECK_BOUNDS>(
        m_regions.at(C::col_t::MS_ANTENNA_COL_NAME),
        C::fid(C::col_t::MS_ANTENNA_COL_NAME));
  }

  //
  // STATION
  //
  static const constexpr unsigned station_rank =
    row_rank + C::element_ranks[C::col_t::MS_ANTENNA_COL_STATION];

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS>
  using StationAccessor =
    FieldAccessor<HYPERION_TYPE_STRING, station_rank, MODE, CHECK_BOUNDS>;

  bool
  has_station() const {
    return m_regions.count(C::col_t::MS_ANTENNA_COL_STATION) > 0;
  }

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  StationAccessor<MODE, CHECK_BOUNDS>
  station() const {
    return
      StationAccessor<MODE, CHECK_BOUNDS>(
        m_regions.at(C::col_t::MS_ANTENNA_COL_STATION),
        C::fid(C::col_t::MS_ANTENNA_COL_STATION));
  }

  //
  // TYPE
  //
  static const constexpr unsigned type_rank =
    row_rank + C::element_ranks[C::col_t::MS_ANTENNA_COL_TYPE];

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS>
  using TypeAccessor =
    FieldAccessor<HYPERION_TYPE_STRING, type_rank, MODE, CHECK_BOUNDS>;

  bool
  has_type() const {
    return m_regions.count(C::col_t::MS_ANTENNA_COL_TYPE) > 0;
  }

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  TypeAccessor<MODE, CHECK_BOUNDS>
  type() const {
    return
      TypeAccessor<MODE, CHECK_BOUNDS>(
        m_regions.at(C::col_t::MS_ANTENNA_COL_TYPE),
        C::fid(C::col_t::MS_ANTENNA_COL_TYPE));
  }

  //
  // MOUNT
  //
  static const constexpr unsigned mount_rank =
    row_rank + C::element_ranks[C::col_t::MS_ANTENNA_COL_MOUNT];

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS>
  using MountAccessor =
    FieldAccessor<HYPERION_TYPE_STRING, mount_rank, MODE, CHECK_BOUNDS>;

  bool
  has_mount() const {
    return m_regions.count(C::col_t::MS_ANTENNA_COL_MOUNT) > 0;
  }

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  MountAccessor<MODE, CHECK_BOUNDS>
  mount() const {
    return
      MountAccessor<MODE, CHECK_BOUNDS>(
        m_regions.at(C::col_t::MS_ANTENNA_COL_MOUNT),
        C::fid(C::col_t::MS_ANTENNA_COL_MOUNT));
  }

  //
  // POSITION
  //
  static const constexpr unsigned position_rank =
    row_rank + C::element_ranks[C::col_t::MS_ANTENNA_COL_POSITION];

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS>
  using PositionAccessor =
    FieldAccessor<HYPERION_TYPE_DOUBLE, position_rank, MODE, CHECK_BOUNDS>;

  bool
  has_position() const {
    return m_regions.count(C::col_t::MS_ANTENNA_COL_POSITION) > 0;
  }

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  PositionAccessor<MODE, CHECK_BOUNDS>
  position() const {
    return
      PositionAccessor<MODE, CHECK_BOUNDS>(
        m_regions.at(C::col_t::MS_ANTENNA_COL_POSITION),
        C::fid(C::col_t::MS_ANTENNA_COL_POSITION));
  }

#ifdef HYPERION_USE_CASACORE

  template <typename T>
  class PositionMeasWriterMixin
    : public T {
  public:
    using T::T;

    void
    write(
      const Legion::Point<row_rank, Legion::coord_t>& pt,
      const casacore::MPosition& val) {

      static_assert(row_rank == 1);
      static_assert(position_rank == 2);

      auto cvt = T::m_cm.convert_at(pt);
      auto p = cvt(val);
      auto vs = p.get(*T::m_units).getValue();
      T::m_position[Legion::Point<position_rank>(pt[0], 0)] = vs[0];
      T::m_position[Legion::Point<position_rank>(pt[0], 1)] = vs[1];
      T::m_position[Legion::Point<position_rank>(pt[0], 2)] = vs[2];
    }
  };

  template <typename T>
  class PositionMeasReaderMixin
    : public T {
  public:
    using T::T;

    casacore::MPosition
    read(const Legion::Point<row_rank, Legion::coord_t>& pt) const {

      static_assert(row_rank == 1);
      static_assert(position_rank == 2);

      auto mr = T::m_cm.meas_ref_at(pt);
      const DataType<HYPERION_TYPE_DOUBLE>::ValueType* mp =
        T::m_position.ptr(Legion::Point<position_rank>(pt[0], 0));
      return
        casacore::MPosition(
          casacore::Quantity(mp[0], *T::m_units),
          casacore::Quantity(mp[1], *T::m_units),
          casacore::Quantity(mp[2], *T::m_units),
          mr);
    }
  };

  template <
    Legion::FieldID FID,
    legion_privilege_mode_t MODE,
    bool CHECK_BOUNDS>
  class PositionMeasAccessorBase {
  public:
    PositionMeasAccessorBase(
      const char* units,
      const Legion::PhysicalRegion& region,
      const mr_t<casacore::MPosition>* mr)
      : m_units(units)
      , m_position(region, FID)
      , m_cm(mr) {
    }

  protected:

    const char* m_units;

    PositionAccessor<MODE, CHECK_BOUNDS> m_position;

    ColumnMeasure<
      casacore::MPosition,
      row_rank,
      row_rank,
      READ_ONLY,
      CHECK_BOUNDS> m_cm;
  };

  template <
    Legion::FieldID FID,
    legion_privilege_mode_t MODE,
    bool CHECK_BOUNDS>
  class PositionMeasAccessor:
    public PositionMeasWriterMixin<
  PositionMeasAccessorBase<FID, MODE, CHECK_BOUNDS>> {
    typedef PositionMeasWriterMixin<
      PositionMeasAccessorBase<FID, MODE, CHECK_BOUNDS>> T;
  public:
    using T::T;
  };

  template <Legion::FieldID FID, bool CHECK_BOUNDS>
  class PositionMeasAccessor<FID, READ_ONLY, CHECK_BOUNDS>
    : public PositionMeasReaderMixin<
        PositionMeasAccessorBase<FID, READ_ONLY, CHECK_BOUNDS>> {
    typedef PositionMeasReaderMixin<
      PositionMeasAccessorBase<FID, READ_ONLY, CHECK_BOUNDS>> T;
  public:
    using T::T;
  };

  template <Legion::FieldID FID, bool CHECK_BOUNDS>
  class PositionMeasAccessor<FID, READ_WRITE, CHECK_BOUNDS>
    : public PositionMeasReaderMixin<
        PositionMeasWriterMixin<
          PositionMeasAccessorBase<FID, READ_WRITE, CHECK_BOUNDS>>> {
    typedef PositionMeasReaderMixin<
      PositionMeasWriterMixin<
        PositionMeasAccessorBase<FID, READ_WRITE, CHECK_BOUNDS>>> T;
  public:
    using T::T;
  };

  bool
  has_position_meas() const {
    return has_position() && m_mrs.count(C::col_t::MS_ANTENNA_COL_POSITION) > 0;
  }

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  PositionMeasAccessor<
    C::fid(C::col_t::MS_ANTENNA_COL_POSITION),
    MODE,
    CHECK_BOUNDS>
  position_meas() const {
    return
      PositionMeasAccessor<
        C::fid(C::col_t::MS_ANTENNA_COL_POSITION),
        MODE,
        CHECK_BOUNDS>(
        C::units.at(C::col_t::MS_ANTENNA_COL_POSITION),
        m_regions.at(C::col_t::MS_ANTENNA_COL_POSITION),
        std::any_cast<mr_t<casacore::MPosition>>(
          &m_mrs.at(C::col_t::MS_ANTENNA_COL_POSITION)));
  }
#endif // HYPERION_USE_CASACORE

  //
  // OFFSET
  //
  static const constexpr unsigned offset_rank =
    row_rank + C::element_ranks[C::col_t::MS_ANTENNA_COL_OFFSET];

  static_assert(offset_rank == position_rank);

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS>
  using OffsetAccessor = PositionAccessor<MODE, CHECK_BOUNDS>;

  bool
  has_offset() const {
    return m_regions.count(C::col_t::MS_ANTENNA_COL_OFFSET) > 0;
  }

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  OffsetAccessor<MODE, CHECK_BOUNDS>
  offset() const {
    return
      OffsetAccessor<MODE, CHECK_BOUNDS>(
        m_regions.at(C::col_t::MS_ANTENNA_COL_OFFSET),
        C::fid(C::col_t::MS_ANTENNA_COL_OFFSET));
  }

#ifdef HYPERION_USE_CASACORE
  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS>
  using OffsetMeasAccessor =
    PositionMeasAccessor<
      C::fid(C::col_t::MS_ANTENNA_COL_OFFSET),
      MODE,
      CHECK_BOUNDS>;

  bool
  has_offset_meas() const {
    return has_offset() && m_mrs.count(C::col_t::MS_ANTENNA_COL_OFFSET) > 0;
  }

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  OffsetMeasAccessor<MODE, CHECK_BOUNDS>
  offset_meas() const {
    return
      OffsetMeasAccessor<MODE, CHECK_BOUNDS>(
        C::units.at(C::col_t::MS_ANTENNA_COL_OFFSET),
        m_regions.at(C::col_t::MS_ANTENNA_COL_OFFSET),
        std::any_cast<casacore::MPosition>(
          &m_mrs.at(C::col_t::MS_ANTENNA_COL_OFFSET)));
  }
#endif // HYPERION_USE_CASACORE

  //
  // DISH_DIAMETER
  //
  static const constexpr unsigned dish_diameter_rank =
    row_rank + C::element_ranks[C::col_t::MS_ANTENNA_COL_DISH_DIAMETER];

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS>
  using DishDiameterAccessor =
    FieldAccessor<HYPERION_TYPE_DOUBLE, dish_diameter_rank, MODE, CHECK_BOUNDS>;

  bool
  has_dish_diameter() const {
    return m_regions.count(C::col_t::MS_ANTENNA_COL_DISH_DIAMETER) > 0;
  }

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  DishDiameterAccessor<MODE, CHECK_BOUNDS>
  dish_diameter() const {
    return
      DishDiameterAccessor<MODE, CHECK_BOUNDS>(
        m_regions.at(C::col_t::MS_ANTENNA_COL_DISH_DIAMETER),
        C::fid(C::col_t::MS_ANTENNA_COL_DISH_DIAMETER));
  }

  //
  // ORBIT_ID
  //
  static const constexpr unsigned orbit_id_rank =
    row_rank + C::element_ranks[C::col_t::MS_ANTENNA_COL_ORBIT_ID];

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS>
  using OrbitIdAccessor =
    FieldAccessor<HYPERION_TYPE_INT, orbit_id_rank, MODE, CHECK_BOUNDS>;

  bool
  has_orbit_id() const {
    return m_regions.count(C::col_t::MS_ANTENNA_COL_ORBIT_ID) > 0;
  }

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  OrbitIdAccessor<MODE, CHECK_BOUNDS>
  orbit_id() const {
    return
      OrbitIdAccessor<MODE, CHECK_BOUNDS>(
        m_regions.at(C::col_t::MS_ANTENNA_COL_ORBIT_ID),
        C::fid(C::col_t::MS_ANTENNA_COL_ORBIT_ID));
  }

  //
  // MEAN_ORBIT
  //
  static const constexpr unsigned mean_orbit_rank =
    row_rank + C::element_ranks[C::col_t::MS_ANTENNA_COL_MEAN_ORBIT];

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS>
  using MeanOrbitAccessor =
    FieldAccessor<HYPERION_TYPE_DOUBLE, mean_orbit_rank, MODE, CHECK_BOUNDS>;

  bool
  has_mean_orbit() const {
    return m_regions.count(C::col_t::MS_ANTENNA_COL_MEAN_ORBIT) > 0;
  }

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  MeanOrbitAccessor<MODE, CHECK_BOUNDS>
  mean_orbit() const {
    return
      MeanOrbitAccessor<MODE, CHECK_BOUNDS>(
        m_regions.at(C::col_t::MS_ANTENNA_COL_MEAN_ORBIT),
        C::fid(C::col_t::MS_ANTENNA_COL_MEAN_ORBIT));
  }

  //
  // PHASED_ARRAY_ID
  //
  static const constexpr unsigned phased_array_id_rank =
    row_rank + C::element_ranks[C::col_t::MS_ANTENNA_COL_PHASED_ARRAY_ID];

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS>
  using PhasedArrayIdAccessor =
    FieldAccessor<HYPERION_TYPE_INT, phased_array_id_rank, MODE, CHECK_BOUNDS>;

  bool
  has_phased_array_id() const {
    return m_regions.count(C::col_t::MS_ANTENNA_COL_PHASED_ARRAY_ID) > 0;
  }

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  PhasedArrayIdAccessor<MODE, CHECK_BOUNDS>
  phased_array_id() const {
    return
      PhasedArrayIdAccessor<MODE, CHECK_BOUNDS>(
        m_regions.at(C::col_t::MS_ANTENNA_COL_PHASED_ARRAY_ID),
        C::fid(C::col_t::MS_ANTENNA_COL_PHASED_ARRAY_ID));
  }

  //
  // FLAG_ROW
  //
  static const constexpr unsigned flag_row_rank =
    row_rank + C::element_ranks[C::col_t::MS_ANTENNA_COL_FLAG_ROW];

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS>
  using FlagRowAccessor =
    FieldAccessor<HYPERION_TYPE_BOOL, flag_row_rank, MODE, CHECK_BOUNDS>;

  bool
  has_flag_row() const {
    return m_regions.count(C::col_t::MS_ANTENNA_COL_FLAG_ROW) > 0;
  }

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  FlagRowAccessor<MODE, CHECK_BOUNDS>
  flag_row() const {
    return
      FlagRowAccessor<MODE, CHECK_BOUNDS>(
        m_regions.at(C::col_t::MS_ANTENNA_COL_FLAG_ROW),
        C::fid(C::col_t::MS_ANTENNA_COL_FLAG_ROW));
  }

private:

  Legion::DomainT<row_rank> m_rows;

  std::unordered_map<C::col_t, Legion::PhysicalRegion> m_regions;

#ifdef HYPERION_USE_CASACORE
  std::unordered_map<C::col_t, std::any> m_mrs;
#endif
};

} // end namespace hyperion

#endif // HYPERION_ANTENNA_COLUMNS_H_

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End:
