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
#ifndef HYPERION_TABLE_BUILDER_H_
#define HYPERION_TABLE_BUILDER_H_

#include <hyperion/hyperion.h>
#include <hyperion/KeywordsBuilder.h>
#include <hyperion/ColumnBuilder.h>
#include <hyperion/Column.h>
#include <hyperion/Table.h>
#include <hyperion/IndexTree.h>
#include <hyperion/MSTable.h>
#include <hyperion/MSTableColumns.h>
#include <hyperion/MeasRefContainer.h>

#pragma GCC visibility push(default)
# include <algorithm>
# include <cassert>
# include <functional>
# include <memory>
# include <unordered_map>
# include <unordered_set>
# include <vector>

# include <casacore/casa/aipstype.h>
# include <casacore/casa/BasicSL/String.h>
# include <casacore/tables/Tables.h>
#pragma GCC visibility pop

namespace hyperion {

template <MSTables D>
class TableBuilderT
  : public KeywordsBuilder {

  friend class Table;

public:

  typedef typename MSTable<D>::Axes Axes;

  TableBuilderT(const std::string& name)
    : KeywordsBuilder()
    , m_name(name)
    , m_num_rows(0) {
  }

  const std::string&
  name() const {
    return m_name;
  }

  template <typename T>
  void
  add_scalar_column(
    const casacore::Table& table,
    const std::string& name,
    const std::optional<std::string>& measure_name) {

    add_column(
      table,
      ScalarColumnBuilder<D>::template generator<T>(name, measure_name)());
  }

  template <typename T, int DIM>
  void
  add_array_column(
    const casacore::Table& table,
    const std::string& name,
    const std::vector<Axes>& element_axes,
    const std::optional<std::string>& measure_name,
    std::function<std::array<size_t, DIM>(const std::any&)> element_shape) {

    std::vector<Axes> axes {MSTable<D>::ROW_AXIS};
    std::copy(
      element_axes.begin(),
      element_axes.end(),
      std::back_inserter(axes));
    add_column(
      table,
      ArrayColumnBuilder<D, DIM>::template generator<T>(
        name,
        measure_name,
        element_shape)(
          axes));
  }

  void
  add_row(const std::unordered_map<std::string, std::any>& args) {
    std::for_each(
      m_columns.begin(),
      m_columns.end(),
      [&args](auto& nm_col) {
        const std::string& nm = std::get<0>(nm_col);
        auto nm_arg = args.find(nm);
        std::any arg = ((nm_arg != args.end()) ? nm_arg->second : std::any());
        std::get<1>(nm_col)->add_row(arg);
      });
    ++m_num_rows;
  }

  void
  add_row() {
    add_row(std::unordered_map<std::string, std::any>());
  }

  void
  add_meas_record(const casacore::Record& rec) {
    m_meas_records.push_back(rec);
  }

  const std::vector<casacore::Record>&
  meas_records() const {
    return m_meas_records;
  }

  std::unordered_set<std::string>
  column_names() const {
    std::unordered_set<std::string> result;
    for (auto& nm_cb : m_columns)
      result.insert(nm_cb.first);
    return result;
  }

  std::vector<Column::Generator>
  column_generators() const {
    std::vector<Column::Generator> result;
    for (auto& nm_cb : m_columns) {
      result.push_back(
        [cb=nm_cb.second]
        (Legion::Context ctx,
         Legion::Runtime* rt,
         const std::string& name_prefix,
         const MeasRefContainer& table_meas_ref) {
         return cb->column(ctx, rt, name_prefix, table_meas_ref);
        });
    }
    return result;
  }

protected:

  struct SizeArgs {
    std::shared_ptr<casacore::TableColumn> tcol;
    unsigned *row;
    casacore::IPosition shape;
  };

  template <int DIM>
  static std::array<size_t, DIM>
  size(const std::any& args) {
    std::array<size_t, DIM> result;
    auto sa = std::any_cast<SizeArgs>(args);
    std::optional<casacore::IPosition> shape;
    if (sa.tcol) {
      if (sa.tcol->hasContent(*sa.row)){
        auto rsh = sa.tcol->shape(*sa.row);
        if (std::count(rsh.begin(), rsh.end(), 0) == 0)
          shape = rsh;
      }
    } else {
      shape = sa.shape;
    }
    if (shape) {
      auto shp = shape.value();
      assert(shp.size() == DIM);
      for (size_t i = 0; i < DIM; ++i)
        result[i] = shp[DIM - 1 - i];
    } else {
      for (size_t i = 0; i < DIM; ++i)
        result[i] = 0;
    }
    return result;
  }

  template <TypeTag DT>
  void
  add_from_table_column(
    const casacore::Table& table,
    const std::string& nm,
    const std::vector<Axes>& element_axes,
    std::unordered_set<std::string>& array_names) {

    typedef typename hyperion::DataType<DT>::ValueType VT;

    auto col = MSTableColumns<D>::lookup_col(nm).value();
    std::optional<std::string> measure_name;
    {
      auto m = MSTableColumns<D>::measure_names.find(col);
      if (m != MSTableColumns<D>::measure_names.end()) {
        measure_name = m->second;
        std::string msr = nm + "_MEASURE_";
        auto pos = measure_name.value().find(msr);
        assert(pos == 0);
        pos += msr.size();
        measure_name = measure_name.value().substr(pos);
      }
    }
    switch (element_axes.size()) {
    case 0:
      add_scalar_column<VT>(table, nm, measure_name);
      break;

    case 1:
      add_array_column<VT, 1>(table, nm, element_axes, measure_name, size<1>);
      array_names.insert(nm);
      break;

    case 2:
      add_array_column<VT, 2>(table, nm, element_axes, measure_name, size<2>);
      array_names.insert(nm);
      break;

    case 3:
      add_array_column<VT, 3>(table, nm, element_axes, measure_name, size<3>);
      array_names.insert(nm);
      break;

    default:
      assert(false);
      break;
    }
  }

  void
  add_column(
    const casacore::Table& table,
    std::unique_ptr<ColumnBuilder<D>>&& col) {

    std::shared_ptr<ColumnBuilder<D>> scol = std::move(col);
    assert(scol->num_rows() == m_num_rows);
    assert(m_columns.count(scol->name()) == 0);
    m_columns[scol->name()] = scol;
    auto tcol = casacore::TableColumn(table, scol->name());
    auto kws = tcol.keywordSet();
    auto nf = kws.nfields();
    for (unsigned f = 0; f < nf; ++f) {
      std::string name = kws.name(f);
      auto dt = kws.dataType(f);
      if (name == "MEASINFO") {
        if (dt == casacore::DataType::TpRecord)
          scol->add_meas_record(kws.asRecord(f));
      } else if (name != "QuantumUnits") {
        switch (dt) {
#define ADD_KW(DT)                              \
          case DataType<DT>::CasacoreTypeTag:   \
            scol->add_keyword(name, DT);        \
            break;
          HYPERION_FOREACH_DATATYPE(ADD_KW);
#undef ADD_KW
        default:
          // ignore other kw types, like Table

          // TODO: support for Array<String> could be useful (e.g, in
          // FLAG_CATEGORY)
          break;
        }
      }
    }
  }

  std::string m_name;

  std::unordered_map<std::string, std::shared_ptr<ColumnBuilder<D>>> m_columns;

  size_t m_num_rows;

  std::vector<casacore::Record> m_meas_records;

public:

  static TableBuilderT
  from_casacore_table(
    const CXX_FILESYSTEM_NAMESPACE::path& path,
    const std::unordered_set<std::string>& column_selections,
    const std::unordered_map<std::string, std::vector<Axes>>& element_axes) {

    casacore::Table table(
      casacore::String((path.filename() == "MAIN") ? path.parent_path() : path),
      casacore::TableLock::PermanentLockingWait);

    std::string table_name = path.filename();
    if (table_name == ".")
      table_name = "MAIN";
    TableBuilderT result(table_name);
    std::unordered_set<std::string> array_names;

    // expand wildcard column selection, get selected columns that exist in the
    // table
    auto tdesc = table.tableDesc();
    auto column_names = tdesc.columnNames();
    std::unordered_set<std::string> actual_column_selections;
    bool select_all = column_selections.count("*") > 0;
    for (auto& nm : column_names) {
      if (tdesc.isColumn(nm)
          && element_axes.count(nm) > 0
          && (select_all || (column_selections.count(nm) > 0))) {
        auto col = tdesc[nm];
        if (col.isScalar() || (col.isArray() && col.ndim() >= 0))
          actual_column_selections.insert(nm);
      }
    }
    // add a column to TableBuilderT for each of the selected columns
    std::for_each(
      actual_column_selections.begin(),
      actual_column_selections.end(),
      [&table, &result, &tdesc, &element_axes, &array_names](auto& nm) {
        auto axes = element_axes.at(nm);
        auto cdesc = tdesc[nm];
        switch (cdesc.dataType()) {
#define ADD_FROM_TCOL(DT)                                               \
          case DataType<DT>::CasacoreTypeTag:                           \
            result.template add_from_table_column<DT>(                  \
              table, nm, axes, array_names);                          \
            break;
          HYPERION_FOREACH_DATATYPE(ADD_FROM_TCOL);
#undef ADD_FROM_TCOL
        default:
          assert(false);
          break;
        }
      });

    // get table keyword names and types
    {
      auto kws = table.keywordSet();
      auto nf = kws.nfields();
      for (unsigned f = 0; f < nf; ++f) {
        std::string name = kws.name(f);
        auto dt = kws.dataType(f);
        if (name == "MEASINFO") {
          if (dt == casacore::DataType::TpRecord)
            result.add_meas_record(kws.asRecord(f));
        } else if (name != "QuantumUnits") {
          switch (dt) {
#define ADD_KW(DT)                              \
            case DataType<DT>::CasacoreTypeTag: \
              result.add_keyword(name, DT);     \
              break;
            HYPERION_FOREACH_DATATYPE(ADD_KW);
#undef ADD_KW
          default:
            // ignore other kw types, like Table
            break;
          }
        }
      }
    }
    // scan rows to get shapes for all selected array columns
    //
    std::unordered_map<std::string, std::any> args;
    // local variable to hold row number, args values contain
    // pointer to this variable
    unsigned row;
    std::transform(
      array_names.begin(),
      array_names.end(),
      std::inserter(args, args.end()),
      [&table, &tdesc, &row](auto& nm) {
        SizeArgs sa;
        sa.row = &row;
        const casacore::IPosition& shp = tdesc[nm].shape();
        if (shp.empty())
          sa.tcol = std::make_shared<casacore::TableColumn>(table, nm);
        else
          sa.shape = shp;
        return std::make_pair(nm, sa);
      });

    auto nrow = table.nrow();
    for (row = 0; row < nrow; ++row)
      result.add_row(args);

    return result;
  }
};

struct HYPERION_API TableBuilder {

  template <MSTables T>
  static TableBuilderT<T>
  from_ms(
    const CXX_FILESYSTEM_NAMESPACE::path& path,
    const std::unordered_set<std::string>& column_selections) {

    return
      TableBuilderT<T>::from_casacore_table(
        ((path.filename() == MSTable<T>::name)
         ? path
         : (path / MSTable<T>::name)),
        column_selections,
        MSTable<T>::element_axes);
  }
};

void
initialize_keywords_from_ms(
  Legion::Context ctx,
  Legion::Runtime* rt,
  const CXX_FILESYSTEM_NAMESPACE::path& path,
  Table& table);

template <MSTables T>
Table
from_ms(
  Legion::Context ctx,
  Legion::Runtime* rt,
  const CXX_FILESYSTEM_NAMESPACE::path& path,
  const std::unordered_set<std::string>& column_selections) {

  typedef typename MSTable<T>::Axes Axes;

  const CXX_FILESYSTEM_NAMESPACE::path& table_path =
    ((path.filename() == MSTable<T>::name)
     ? path
     : (path / MSTable<T>::name));

  auto builder = TableBuilder::from_ms<T>(table_path, column_selections);

  std::unordered_map<std::string, MeasRef> meas_refs;
  std::for_each(
    builder.meas_records().begin(),
    builder.meas_records().end(),
    [&meas_refs, &ctx, rt](const casacore::RecordInterface& rec) {
      casacore::MeasureHolder mh;
      casacore::String err;
      auto converted = mh.fromType(err, rec);
      if (converted) {
        if (false) {}
#define MK_MR(MC)                               \
        else if (MClassT<MC>::holds(mh)) {      \
          auto m = MClassT<MC>::get(mh);        \
          meas_refs.emplace(                    \
            toupper(MClassT<MC>::name),         \
            MeasRef::create<MClassT<MC>::type>( \
              ctx,                              \
              rt,                               \
              m.getRef()));                     \
        }
        HYPERION_FOREACH_MCLASS(MK_MR)
#undef MK_MR
        else { assert(false); }
      }
    }
    );
  auto result =
    Table::create(
      ctx,
      rt,
      builder.name(),
      std::vector<Axes>{MSTable<T>::ROW_AXIS},
      builder.column_generators(),
      meas_refs,
      MeasRefContainer(),
      builder.keywords(),
      "/");

  initialize_keywords_from_ms(ctx, rt, table_path, result);
  
  return result;
}

} // end namespace hyperion

#endif // HYPERION_TABLE_BUILDER_H_

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End:
