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
#ifndef HYPERION_X_TABLE_H_
#define HYPERION_X_TABLE_H_

#include <hyperion/hyperion.h>
#include <hyperion/x/Column.h>
#include <hyperion/x/ColumnSpace.h>
#include <hyperion/x/TableField.h>
#include <hyperion/Table.h>

#pragma GCC visibility push(default)
# include <array>
# include <string>
# include <type_traits>
# include <unordered_map>
# include <utility>
# include <vector>
#pragma GCC visibility pop

namespace hyperion {
namespace x {

enum class TableFieldsFid {
  NM,
  DT,
  KW,
  MR,
  MD,
  VS
};

template <TableFieldsFid F>
struct TableFieldsType {
  typedef void type;
};
template<>
struct TableFieldsType<TableFieldsFid::NM> {
  typedef hyperion::string type;
};
template<>
struct TableFieldsType<TableFieldsFid::DT> {
  typedef hyperion::TypeTag type;
};
template<>
struct TableFieldsType<TableFieldsFid::KW> {
  typedef hyperion::Keywords type;
};
template<>
struct TableFieldsType<TableFieldsFid::MR> {
  typedef hyperion::MeasRef type;
};
template<>
struct TableFieldsType<TableFieldsFid::MD> {
  typedef Legion::LogicalRegion type;
};
template<>
struct TableFieldsType<TableFieldsFid::VS> {
  typedef Legion::LogicalRegion type;
};

class HYPERION_API Table {

public:

  // FIXME: value of MAX_COLUMNS has been reduced due to size of
  // columns_result_t
  static const constexpr size_t MAX_COLUMNS = HYPERION_MAX_NUM_TABLE_COLUMNS / 2;

  struct columns_result_tt {
    typedef std::tuple<hyperion::string, TableField> tbl_fld_t;

    std::vector<
      std::tuple<
        ColumnSpace,
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
    legion_privilege_mode_t MODE,
    TableFieldsFid F,
    bool CHECK_BOUNDS=false>
  using Accessor =
    Legion::FieldAccessor<
      MODE,
      typename TableFieldsType<F>::type,
      1,
      Legion::coord_t,
      Legion::AffineAccessor<
        typename TableFieldsType<F>::type,
        1,
        Legion::coord_t>,
      CHECK_BOUNDS>;

public:

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  using NameAccessor =
    Accessor<MODE, TableFieldsFid::NM, CHECK_BOUNDS>;

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  using DatatypeAccessor =
    Accessor<MODE, TableFieldsFid::DT, CHECK_BOUNDS>;

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  using KeywordsAccessor =
    Accessor<MODE, TableFieldsFid::KW, CHECK_BOUNDS>;

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  using MeasRefAccessor =
    Accessor<MODE, TableFieldsFid::MR, CHECK_BOUNDS>;

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  using MetadataAccessor =
    Accessor<MODE, TableFieldsFid::MD, CHECK_BOUNDS>;

  template <legion_privilege_mode_t MODE, bool CHECK_BOUNDS=false>
  using ValuesAccessor =
    Accessor<MODE, TableFieldsFid::VS, CHECK_BOUNDS>;

  Table() {}

  Table(const Legion::LogicalRegion& fields_lr_)
    : fields_lr(fields_lr_) {}

  static Table
  create(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const std::vector<
      std::tuple<
        ColumnSpace,
        std::vector<std::pair<std::string, TableField>>>>& columns);

  typedef Table convert_result_t;

  static Legion::Future /* convert_result_t */
  convert(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const hyperion::Table& table,
    const std::unordered_map<std::string, Legion::FieldID> fids);

  static convert_result_t
  convert(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const std::unordered_map<std::string, Legion::FieldID>& fids,
    const std::vector<Legion::IndexSpace>& col_values_iss,
    const std::vector<
      std::tuple<
        Legion::PhysicalRegion,
        Legion::PhysicalRegion,
        std::optional<hyperion::MeasRef::DataRegions>,
        std::optional<hyperion::Keywords::pair<Legion::PhysicalRegion>>>>&
    col_prs);

  bool
  is_valid() const {
    return fields_lr != Legion::LogicalRegion::NO_REGION;
  }

  void
  copy_values_from(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const hyperion::Table& table) const;

  static void
  copy_values_from(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const Legion::PhysicalRegion& columns_pr,
    const std::vector<
      std::tuple<Legion::PhysicalRegion, Legion::PhysicalRegion>>& src_col_prs);

  void
  destroy(
    Legion::Context ctx,
    Legion::Runtime* rt,
    bool destroy_column_space_components=false);

  typedef std::array<std::tuple<hyperion::string, Column>, MAX_COLUMNS>
  columns_result_t;

  Legion::Future /* columns_result_t */
  columns(Legion::Context ctx, Legion::Runtime *rt) const;

  static columns_result_t
  columns(Legion::Runtime *rt, const Legion::PhysicalRegion& fields_pr);

  static void
  preregister_tasks();

  Legion::LogicalRegion fields_lr;

private:

  static std::unordered_map<std::string, Column>
  column_map(const columns_result_t& columns_result);

  static std::unordered_map<std::string, Column>
  column_map(const columns_result_tt& columns_result);

  static void
  create_columns(
    Legion::Context ctx,
    Legion::Runtime* rt,
    const ColumnSpace& column_space,
    const std::vector<std::pair<std::string, TableField>>& tbl_fields,
    Legion::LogicalRegion& fields_lr,
    size_t tbl_field_offset);

  static Legion::TaskID init_task_id;

  static const char* init_task_name;

  static void
  init_task(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& regions,
    Legion::Context ctx,
    Legion::Runtime *rt);

  static Legion::TaskID columns_task_id;

  static const char* columns_task_name;

  static columns_result_t
  columns_task(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& regions,
    Legion::Context ctx,
    Legion::Runtime *rt);

  static Legion::TaskID convert_task_id;

  static const char* convert_task_name;

  static convert_result_t
  convert_task(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& regions,
    Legion::Context ctx,
    Legion::Runtime *rt);

  static Legion::TaskID copy_values_from_task_id;

  static const char* copy_values_from_task_name;

  static void
  copy_values_from_task(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& regions,
    Legion::Context ctx,
    Legion::Runtime *rt);
};

} // end namespace x
} // end namespace hyperion

#endif // HYPERION_X_COLUMN_H_

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End: