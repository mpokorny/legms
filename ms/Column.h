#ifndef LEGMS_MS_COLUMN_H_
#define LEGMS_MS_COLUMN_H_

#include <unordered_map>

#include <casacore/casa/aipstype.h>
#include <casacore/casa/Utilities/DataType.h>
#include "legion.h"

#include "utility.h"
#include "tree_index_space.h"
#include "WithKeywords.h"
#include "IndexTree.h"
#include "ColumnBuilder.h"

namespace legms {
namespace ms {

class Column
  : public WithKeywords {
public:

  Column(
    Legion::Context ctx,
    Legion::Runtime* runtime,
    const ColumnBuilder& builder)
    : WithKeywords(builder.keywords())
    , m_name(builder.name())
    , m_datatype(builder.datatype())
    , m_num_rows(builder.num_rows())
    , m_row_index_shape(builder.row_index_shape())
    , m_index_tree(builder.index_tree())
    , m_context(ctx)
    , m_runtime(runtime) {
  }

  Column(
    Legion::Context ctx,
    Legion::Runtime* runtime,
    const std::string& name,
    casacore::DataType datatype,
    const IndexTreeL& row_index_shape,
    const IndexTreeL& index_tree,
    const std::unordered_map<std::string, casacore::DataType>& kws =
      std::unordered_map<std::string, casacore::DataType>())
    : WithKeywords(kws)
    , m_name(name)
    , m_datatype(datatype)
    , m_num_rows(nr(row_index_shape, index_tree).value())
    , m_row_index_shape(row_index_shape)
    , m_index_tree(index_tree)
    , m_context(ctx)
    , m_runtime(runtime) {
  }

  Column(
    Legion::Context ctx,
    Legion::Runtime* runtime,
    const std::string& name,
    casacore::DataType datatype,
    const IndexTreeL& row_index_shape,
    unsigned num_rows,
    const std::unordered_map<std::string, casacore::DataType>& kws =
    std::unordered_map<std::string, casacore::DataType>())
    : WithKeywords(kws)
    , m_name(name)
    , m_datatype(datatype)
    , m_num_rows(num_rows)
    , m_row_index_shape(row_index_shape)
    , m_index_tree(ixt(row_index_shape, num_rows))
    , m_context(ctx)
    , m_runtime(runtime) {
  }

  virtual ~Column() {
    if (m_index_space)
      m_runtime->destroy_index_space(m_context, m_index_space.value());
  }

  const std::string&
  name() const {
    return m_name;
  }

  casacore::DataType
  datatype() const {
    return m_datatype;
  }

  const IndexTreeL&
  index_tree() const {
    return m_index_tree;
  }

  const IndexTreeL&
  row_index_shape() const {
    return m_row_index_shape;
  }

  unsigned
  row_rank() const {
    return m_row_index_shape.rank().value();
  }

  unsigned
  rank() const {
    return m_index_tree.rank().value();
  }

  size_t
  num_rows() const {
    return m_num_rows;
  }

  std::optional<Legion::IndexSpace>
  index_space() const {
    if (!m_index_space)
      m_index_space =
        legms::tree_index_space(m_index_tree, m_context, m_runtime);
    return m_index_space;
  }

  Legion::FieldID
  add_field(Legion::FieldSpace fs, Legion::FieldAllocator fa) const {

    auto result = legms::add_field(m_datatype, fa);
    m_runtime->attach_name(fs, result, name().c_str());
    return result;
  }

private:

  static std::optional<size_t>
  nr(
    const IndexTreeL& row_shape,
    const IndexTreeL& full_shape,
    bool cycle = true) {

    if (row_shape.rank().value() > full_shape.rank().value())
      return std::nullopt;
    auto pruned_shape = full_shape.pruned(row_shape.rank().value() - 1);
    auto p_iter = pruned_shape.children().begin();
    auto p_end = pruned_shape.children().end();
    Legion::coord_t i0 = std::get<0>(row_shape.index_range());
    size_t result = 0;
    Legion::coord_t pi, pn;
    IndexTreeL pt;
    std::tie(pi, pn, pt) = *p_iter;
    while (p_iter != p_end) {
      auto r_iter = row_shape.children().begin();
      auto r_end = row_shape.children().end();
      Legion::coord_t i, n;
      IndexTreeL t;
      while (p_iter != p_end && r_iter != r_end) {
        std::tie(i, n, t) = *r_iter;
        if (i + i0 != pi) {
          return std::nullopt;
        } else if (t == pt) {
          auto m = std::min(n, pn);
          result += m * t.size();
          pi += m;
          pn -= m;
          if (pn == 0) {
            ++p_iter;
            if (p_iter != p_end)
              std::tie(pi, pn, pt) = *p_iter;
          }
        } else {
          ++p_iter;
          if (p_iter != p_end)
            return std::nullopt;
          auto chnr = nr(t, pt, false);
          if (chnr)
            result += chnr.value();
          else
            return std::nullopt;
        }
        ++r_iter;
      }
      i0 = i + n;
      if (!cycle && p_iter != p_end && r_iter == r_end)
        return std::nullopt;
    }
    return result;
  }

  static IndexTreeL
  ixt(const IndexTreeL& row_shape, size_t num) {
    std::vector<std::tuple<Legion::coord_t, Legion::coord_t, IndexTreeL>> ch;
    auto shape_n = row_shape.size();
    auto shape_rep = num / shape_n;
    auto shape_rem = num % shape_n;
    assert(std::get<0>(row_shape.index_range()) == 0);
    auto stride = std::get<1>(row_shape.index_range()) + 1;
    Legion::coord_t offset = 0;
    if (row_shape.children().size() == 1) {
      Legion::coord_t i;
      IndexTreeL t;
      std::tie(i, std::ignore, t) = row_shape.children()[0];
      offset += shape_rep * stride;
      ch.emplace_back(i, offset, t);
    } else {
      for (size_t r = 0; r < shape_rep; ++r) {
        std::transform(
          row_shape.children().begin(),
          row_shape.children().end(),
          std::back_inserter(ch),
          [&offset](auto& c) {
            auto& [i, n, t] = c;
            return std::make_tuple(i + offset, n, t);
          });
        offset += stride;
      }
    }
    auto rch = row_shape.children().begin();
    auto rch_end = row_shape.children().end();
    while (shape_rem > 0 && rch != rch_end) {
      auto& [i, n, t] = *rch;
      auto tsz = t.size();
      if (shape_rem >= tsz) {
        auto nt = std::min(shape_rem / tsz, static_cast<size_t>(n));
        ch.emplace_back(i + offset, nt, t);
        shape_rem -= nt * tsz;
        if (nt == static_cast<size_t>(n))
          ++rch;
      } else /* shape_rem < tsz */ {
        auto pt = ixt(t, shape_rem);
        shape_rem = 0;
        ch.emplace_back(i + offset, 1, pt);
      }
    }
    auto result = IndexTreeL(ch);
    assert(result.size() == num);
    return result;
  }

  std::string m_name;

  casacore::DataType m_datatype;

  size_t m_num_rows;

  IndexTreeL m_row_index_shape;

  IndexTreeL m_index_tree;

  Legion::Context m_context;

  Legion::Runtime* m_runtime;

  mutable std::optional<Legion::IndexSpace> m_index_space;
};

} // end namespace ms
} // end namespace legms

#endif // LEGMS_MS_COLUMN_H_

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End:
