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
#ifndef HYPERION_TABLE_H_
#define HYPERION_TABLE_H_

#include <hyperion/hyperion.h>
#include <hyperion/utility.h>
#include <hyperion/Column.h>
#include <hyperion/ColumnSpace.h>
#include <hyperion/ColumnSpacePartition.h>
#include <hyperion/TableField.h>

#pragma GCC visibility push(default)
# include <array>
# include <map>
# include <string>
# include <type_traits>
# include <unordered_map>
# include <utility>
# include <vector>
#pragma GCC visibility pop

namespace hyperion {

enum class TableFieldsFid {
  NM,
  DT,
  KW,
#ifdef HYPERION_USE_CASACORE
  MR,
  RC,
#endif
  CS,
  VF,
  VS
};

template <TableFieldsFid F>
struct TableFieldsType {
  typedef void type;
};
template<>
struct TableFieldsType<TableFieldsFid::NM> {
  typedef string type;
};
template<>
struct TableFieldsType<TableFieldsFid::DT> {
  typedef hyperion::TypeTag type;
};
template<>
struct TableFieldsType<TableFieldsFid::KW> {
  typedef Keywords type;
};
#ifdef HYPERION_USE_CASACORE
template<>
struct TableFieldsType<TableFieldsFid::MR> {
  typedef MeasRef type;
};
template<>
struct TableFieldsType<TableFieldsFid::RC> {
  typedef string type;
};
#endif
template<>
struct TableFieldsType<TableFieldsFid::CS> {
  typedef ColumnSpace type;
};
template<>
struct TableFieldsType<TableFieldsFid::VF> {
  typedef Legion::FieldID type;
};
template<>
struct TableFieldsType<TableFieldsFid::VS> {
  typedef Legion::LogicalRegion type;
};

class PhysicalTable;

class HYPERION_API Table {

  // FIXME: add support for table keywords

public:

  static const constexpr size_t MAX_COLUMNS = HYPERION_MAX_NUM_TABLE_COLUMNS;

  struct partition_rows_result_t {
    std::vector<ColumnSpacePartition> partitions;

    size_t
    legion_buffer_size(void) const;

    size_t
    legion_serialize(void* buffer) const;

    size_t
    legion_deserialize(const void* buffer);

    std::optional<ColumnSpacePartition>
    find(const ColumnSpace& cs) const;
  };

  struct columns_result_t {
    typedef std::tuple<hyperion::string, TableField> tbl_fld_t;

    std::vector<
      std::tuple<
        ColumnSpace,
        bool,
        Legion::LogicalRegion,
        std::vector<tbl_fld_t>>> fields;

    size_t
    legion_buffer_size(void) const;

    size_t
    legion_serialize(void* buffer) const;

    size_t
    legion_deserialize(const void* buffer);
  };

private:

  template <
    Legion::PrivilegeMode MODE,
    TableFieldsFid F,
    bool CHECK_BOUNDS=false>
  using Accessor =
    Legion::FieldAccessor<
      MODE,
      typename TableFieldsType<F>::type,
      1,
      Legion::coord_t,
      Legion::GenericAccessor<
        typename TableFieldsType<F>::type,
        1,
        Legion::coord_t>,
      CHECK_BOUNDS>;

public:

  template <Legion::PrivilegeMode MODE, bool CHECK_BOUNDS=false>
  using NameAccessor =
    Accessor<MODE, TableFieldsFid::NM, CHECK_BOUNDS>;

  template <Legion::PrivilegeMode MODE, bool CHECK_BOUNDS=false>
  using DatatypeAccessor =
    Accessor<MODE, TableFieldsFid::DT, CHECK_BOUNDS>;

  template <Legion::PrivilegeMode MODE, bool CHECK_BOUNDS=false>
  using KeywordsAccessor =
    Accessor<MODE, TableFieldsFid::KW, CHECK_BOUNDS>;

#ifdef HYPERION_USE_CASACORE
  template <Legion::PrivilegeMode MODE, bool CHECK_BOUNDS=false>
  using MeasRefAccessor =
    Accessor<MODE, TableFieldsFid::MR, CHECK_BOUNDS>;

  template <Legion::PrivilegeMode MODE, bool CHECK_BOUNDS=false>
  using RefColumnAccessor =
    Accessor<MODE, TableFieldsFid::RC, CHECK_BOUNDS>;
#endif

  template <Legion::PrivilegeMode MODE, bool CHECK_BOUNDS=false>
  using ColumnSpaceAccessor =
    Accessor<MODE, TableFieldsFid::CS, CHECK_BOUNDS>;

  template <Legion::PrivilegeMode MODE, bool CHECK_BOUNDS=false>
  using ValueFidAccessor =
    Accessor<MODE, TableFieldsFid::VF, CHECK_BOUNDS>;

  template <Legion::PrivilegeMode MODE, bool CHECK_BOUNDS=false>
  using ValuesAccessor =
    Accessor<MODE, TableFieldsFid::VS, CHECK_BOUNDS>;

  Table() {}

  Table(const Legion::LogicalRegion& fields_lr_)
  : fields_lr(fields_lr_) {}

  Table(
    const Legion::LogicalRegion& fields_lr_,
    const std::optional<Legion::LogicalRegion>& fields_parent_,
    const std::unordered_map<std::string, Legion::LogicalRegion>&
    column_parents_)
    : fields_lr(fields_lr_)
    , fields_parent(fields_parent_)
    , column_parents(column_parents_) {}

  typedef std::vector<
    std::tuple<
      ColumnSpace,
      bool,
      std::vector<std::pair<std::string, TableField>>>> fields_t;

  static Table
  create(Legion::Context ctx, Legion::Runtime* rt, const fields_t& fields);

  typedef std::optional<ColumnSpace> index_column_space_result_t;

  Legion::Future /* index_column_space_result_t */
  index_column_space(Legion::Context ctx, Legion::Runtime* rt) const;

  static index_column_space_result_t
  index_column_space(
    Legion::Runtime* rt,
    const Legion::LogicalRegion& fields_parent,
    const Legion::PhysicalRegion& fields_pr);

  static bool
  is_empty(const index_column_space_result_t& index_cs);

  // Any LogicalPartitions returned from requirements() should eventually be
  // destroyed by the caller. Such LogicalPartitions should only appear when
  // "table_partition" is not empty or column_modes deselects some columns.
  std::tuple<
    std::vector<Legion::RegionRequirement>,
    std::vector<Legion::LogicalPartition>>
  requirements(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const ColumnSpacePartition& table_partition = ColumnSpacePartition(),
    Legion::PrivilegeMode table_privilege = READ_ONLY,
    const std::map<
      std::string,
      std::optional<
        std::tuple<bool, Legion::PrivilegeMode, Legion::CoherenceProperty>>>&
    column_modes = {},
    bool columns_mapped = true,
    Legion::PrivilegeMode columns_privilege = READ_ONLY,
    Legion::CoherenceProperty columns_coherence = EXCLUSIVE) const;

  static std::tuple<
    std::vector<Legion::RegionRequirement>,
    std::vector<Legion::LogicalPartition>>
  requirements(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const Legion::LogicalRegion& fields_parent,
    const Legion::PhysicalRegion& fields_pr,
    const std::unordered_map<std::string, Legion::LogicalRegion>&
    column_parents,
    const ColumnSpacePartition& table_partition = ColumnSpacePartition(),
    Legion::PrivilegeMode table_privilege = READ_ONLY,
    const std::map<
      std::string,
      std::optional<
        std::tuple<bool, Legion::PrivilegeMode, Legion::CoherenceProperty>>>&
    column_modes = {},
    bool columns_mapped = true,
    Legion::PrivilegeMode columns_privilege = READ_ONLY,
    Legion::CoherenceProperty columns_coherence = EXCLUSIVE);

  Legion::Future /* bool */
  is_conformant(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const ColumnSpace& cs) const;

  static bool
  is_conformant(
    Legion::Runtime* rt,
    const Legion::LogicalRegion& fields_parent,
    const Legion::PhysicalRegion& fields_pr,
    const std::optional<std::tuple<Legion::IndexSpace, Legion::PhysicalRegion>>&
    index_cs,
    const Legion::IndexSpace& cs_is,
    const Legion::PhysicalRegion& cs_md_pr);

  Legion::Future /* bool */
  add_columns(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const fields_t& fields) const;

  static bool
  add_columns(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const std::vector<
      std::tuple<
        ColumnSpace,
        bool,
        size_t,
        std::vector<std::pair<hyperion::string, TableField>>>>& columns,
    const std::vector<Legion::LogicalRegion>& val_lrs,
    const Legion::LogicalRegion& fields_parent,
    const Legion::PhysicalRegion& fields_pr,
    const std::optional<
      std::tuple<Legion::IndexSpace, Legion::PhysicalRegion>>& index_cs,
    const std::vector<Legion::PhysicalRegion>& csp_md_prs);

  bool
  remove_columns(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const std::unordered_set<std::string>& columns,
    bool destroy_orphan_column_spaces=true,
    bool destroy_field_data=true);

  static bool
  remove_columns(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const std::set<hyperion::string>& columns,
    bool destroy_orphan_column_spaces,
    bool destroy_field_data,
    const Legion::LogicalRegion& fields_parent,
    const Legion::PhysicalRegion& fields_pr,
    const std::vector<ColumnSpace>& cs,
    const std::vector<Legion::PhysicalRegion>& cs_md_prs);

  // each element of the block_sizes vector is the block size on the
  // corresponding axis of the index axes vector, with an empty value indicating
  // that there is no partitioning on that axis; if the length of block_sizes is
  // less than the length of the index axes vector the "missing" axes will not
  // be partitioned
  Legion::Future /* partition_rows_result_t */
  partition_rows(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const std::vector<std::optional<size_t>>& block_sizes) const;

  static partition_rows_result_t
  partition_rows(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const std::vector<std::optional<size_t>>& block_sizes,
    const Legion::PhysicalRegion& index_cs_md_pr,
    const std::vector<Legion::IndexSpace>& cs_iss,
    const std::vector<Legion::PhysicalRegion>& cs_md_prs);

  bool
  is_valid() const {
    return fields_lr != Legion::LogicalRegion::NO_REGION;
  }

  // 'allow_rows' is intended to support the case where reindexing may not
  // result in a single value in a column per aggregate index, necessitating the
  // maintenance of a row index. A value of 'true' for this argument is always
  // safe, but may result in a degenerate axis when an aggregate index always
  // identifies a single value in a column. If the value is 'false' and a
  // non-degenerate axis is required by the reindexing, this method will return
  // an empty value. TODO: remove degenerate axes after the fact, and do that
  // automatically in this method, which would allow us to remove the
  // 'allow_rows' argument.
  Legion::Future /* LogicalRegion */
  reindexed(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const std::vector<std::pair<int, std::string>>& index_axes,
    bool allow_rows) const;

  template <typename D>
  Legion::Future /* LogicalRegion */
  reindexed(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const std::vector<D>& index_axes,
    bool allow_rows) const {

    std::vector<std::pair<int, std::string>> iax;
    iax.reserve(index_axes.size());
    for (auto& d : index_axes) {
      int i = static_cast<int>(d);
      iax.emplace_back(i, Axes<D>::names[i]);
    }
    return reindexed(ctx, rt, iax, allow_rows);
  }

  struct ColumnRegions {
    std::tuple<Legion::LogicalRegion, Legion::PhysicalRegion> values;
    Legion::PhysicalRegion metadata;
    std::optional<Legion::PhysicalRegion> mr_metadata;
    std::optional<Legion::PhysicalRegion> mr_values;
    std::optional<Legion::PhysicalRegion> mr_index;
    std::optional<Legion::PhysicalRegion> kw_type_tags;
    std::optional<Legion::PhysicalRegion> kw_values;
  };

  static Legion::LogicalRegion
  reindexed(
    Legion::Context ctx,
    Legion::Runtime *rt,
    const std::vector<std::pair<int, std::string>>& index_axes,
    bool allow_rows,
    const Legion::LogicalRegion& fields_parent,
    const Legion::PhysicalRegion& fields_pr,
    const Legion::PhysicalRegion& index_cs_md_pr,
    const std::vector<std::tuple<Legion::coord_t, ColumnRegions>>&
    column_regions);

  void
  destroy(
    Legion::Context ctx,
    Legion::Runtime* rt,
    bool destroy_column_space_components=false,
    bool destroy_field_data=false);

  Legion::Future /* columns_result_t */
  columns(Legion::Context ctx, Legion::Runtime *rt) const;

  static columns_result_t
  columns(
    Legion::Runtime *rt,
    const Legion::LogicalRegion& fields_parent,
    const Legion::PhysicalRegion& fields_pr);

  static std::unordered_map<std::string, Column>
  column_map(const columns_result_t& columns_result);

  static void
  preregister_tasks();

  Legion::LogicalRegion fields_lr;

  std::optional<Legion::LogicalRegion> fields_parent;

  std::unordered_map<std::string, Legion::LogicalRegion> column_parents;

protected:

  friend class Legion::LegionTaskWrapper;

  static index_column_space_result_t
  index_column_space_task(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& regions,
    Legion::Context ctx,
    Legion::Runtime *rt);

  static bool
  is_conformant_task(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& regions,
    Legion::Context ctx,
    Legion::Runtime *rt);

  static partition_rows_result_t
  partition_rows_task(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& regions,
    Legion::Context ctx,
    Legion::Runtime *rt);

  static bool
  add_columns_task(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& regions,
    Legion::Context ctx,
    Legion::Runtime *rt);

  static columns_result_t
  columns_task(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& regions,
    Legion::Context ctx,
    Legion::Runtime *rt);

  static Legion::LogicalRegion
  reindexed_task(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& regions,
    Legion::Context ctx,
    Legion::Runtime *rt);

  static Legion::IndexSpace
  reindex_column_space_task(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& regions,
    Legion::Context ctx,
    Legion::Runtime *rt);

  static void
  reindex_copy_values_task(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& regions,
    Legion::Context ctx,
    Legion::Runtime *rt);

protected:

  friend class PhysicalTable;

  static const constexpr Legion::FieldID no_column = AUTO_GENERATE_ID;

private:

  static Legion::TaskID index_column_space_task_id;

  static const char* index_column_space_task_name;

  static Legion::TaskID is_conformant_task_id;

  static const char* is_conformant_task_name;

  static Legion::TaskID partition_rows_task_id;

  static const char* partition_rows_task_name;

  static Legion::TaskID add_columns_task_id;

  static const char* add_columns_task_name;

  static Legion::TaskID columns_task_id;

  static const char* columns_task_name;

  static Legion::TaskID reindexed_task_id;

  static const char* reindexed_task_name;

  static Legion::TaskID reindex_column_space_task_id;

  static const char* reindex_column_space_task_name;

  static Legion::TaskID reindex_copy_values_task_id;

  static const char* reindex_copy_values_task_name;
};

} // end namespace hyperion

#endif // HYPERION_COLUMN_H_

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End:
