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
#include <hyperion/hdf5.h>
#include <hyperion/tree_index_space.h>
#include <hyperion/MSTable.h>
#include <hyperion/Table.h>
#include <hyperion/TableField.h>
#include <hyperion/Column.h>
#include <hyperion/ColumnSpace.h>
#include <hyperion/PhysicalTable.h>

#include <algorithm>
#include CXX_ANY_HEADER
#include <cstring>
#include <numeric>
#include CXX_OPTIONAL_HEADER
#include <sstream>

using namespace hyperion::hdf5;
using namespace hyperion;
using namespace Legion;

const char* table_index_axes_attr_name =
  HYPERION_NAMESPACE_PREFIX "index_axes";
const char* table_axes_dt_name =
  HYPERION_NAMESPACE_PREFIX "table_axes";
const char* column_axes_attr_name =
  HYPERION_NAMESPACE_PREFIX "axes";
const char* column_refcol_attr_name =
  HYPERION_NAMESPACE_PREFIX "refcol";
const char* column_space_link_name =
  HYPERION_NAMESPACE_PREFIX "colspace";
const char* index_column_space_link_name =
  HYPERION_NAMESPACE_PREFIX "indexcolspace";

static bool
starts_with(const char* str, const char* pref) {
  bool result = true;
  while (result && *str != '\0' && *pref != '\0') {
    result = *str == *pref;
    ++str; ++pref;
  }
  return result && *pref == '\0';
}

// static bool
// ends_with(const char* str, const char* suff) {
//   bool result = true;
//   const char* estr = strchr(str, '\0');
//   const char* esuff = strchr(suff, '\0');
//   do {
//     --estr; --esuff;
//     result = *estr == *esuff;
//   } while (result && estr != str && esuff != suff);
//   return result && esuff == suff;
// }


size_t
hyperion::hdf5::binary_index_tree_serdez::serialized_size(
  const IndexTreeL& tree) {
  return tree.serialized_size();
}

size_t
hyperion::hdf5::binary_index_tree_serdez::serialize(
  const IndexTreeL& tree, void *buffer) {
  return tree.serialize(static_cast<char*>(buffer));
}

size_t
hyperion::hdf5::binary_index_tree_serdez::deserialize(
  IndexTreeL& tree, const void* buffer) {
  tree = IndexTreeL::deserialize(static_cast<const char*>(buffer));
  return tree.serialized_size();
}

size_t
hyperion::hdf5::string_index_tree_serdez::serialized_size(
  const IndexTreeL& tree) {
  return tree.show().size() + 1;
}

size_t
hyperion::hdf5::string_index_tree_serdez::serialize(
  const IndexTreeL& tree, void *buffer) {
  auto tr = tree.show();
  std::memcpy(static_cast<char*>(buffer), tr.c_str(), tr.size() + 1);
  return tr.size() + 1;
}

size_t
hyperion::hdf5::string_index_tree_serdez::deserialize(
  IndexTreeL&, const void*) {
  // TODO
  assert(false);
  return 0;
}

CXX_OPTIONAL_NAMESPACE::optional<std::string>
hyperion::hdf5::read_index_tree_attr_metadata(
  hid_t grp_id,
  const std::string& attr_name) {

  CXX_OPTIONAL_NAMESPACE::optional<std::string> result;

  std::string md_id_name =
    std::string(HYPERION_ATTRIBUTE_SID_PREFIX) + attr_name;
  if (H5Aexists(grp_id, md_id_name.c_str())) {

    hid_t attr_id = H5Aopen(grp_id, md_id_name.c_str(), H5P_DEFAULT);

    if (attr_id >= 0) {
      hid_t attr_type = H5Aget_type(attr_id);
      hid_t attr_dt = H5DatatypeManager::datatype<HYPERION_TYPE_STRING>();
      if (H5Tequal(attr_type, attr_dt) > 0) {
        string attr;
        CHECK_H5(H5Aread(attr_id, attr_dt, attr.val));
        result = attr.val;
      }
      CHECK_H5(H5Aclose(attr_id));
      CHECK_H5(H5Tclose(attr_type));
    }
  }
  return result;
}

static CXX_OPTIONAL_NAMESPACE::optional<IndexTreeL>
read_index_tree_binary(hid_t grp_id, const char* attr_nm) {
  CXX_OPTIONAL_NAMESPACE::optional<IndexTreeL> result;
  CXX_OPTIONAL_NAMESPACE::optional<std::string> sid =
    read_index_tree_attr_metadata(grp_id, attr_nm);
  if (sid && (sid.value() == "hyperion::hdf5::binary_index_tree_serdez"))
    result =
      read_index_tree_from_attr<binary_index_tree_serdez>(grp_id, attr_nm);
  return result;
}

template <hyperion::TypeTag DT>
using KW =
  FieldAccessor<
  READ_ONLY,
  typename DataType<DT>::ValueType,
  1,
  coord_t,
  AffineAccessor<
    typename DataType<DT>::ValueType,
    1,
    coord_t>,
  false>;

static void
init_datatype_attr(
  hid_t loc_id,
  hyperion::TypeTag dt) {

  htri_t rc = H5Aexists(loc_id, HYPERION_ATTRIBUTE_DT);
  if (rc > 0)
    CHECK_H5(H5Adelete(loc_id, HYPERION_ATTRIBUTE_DT));

  hid_t ds = CHECK_H5(H5Screate(H5S_SCALAR));
  hid_t did = hyperion::H5DatatypeManager::datatypes()[
    hyperion::H5DatatypeManager::DATATYPE_H5T];
  hid_t attr_id =
    CHECK_H5(
      H5Acreate(
        loc_id,
        HYPERION_ATTRIBUTE_DT,
        did,
        ds,
        H5P_DEFAULT,
        H5P_DEFAULT));
  CHECK_H5(H5Awrite(attr_id, did, &dt));
  CHECK_H5(H5Sclose(ds));
  CHECK_H5(H5Aclose(attr_id));
}

static hid_t
init_kw(
  hid_t loc_id,
  const char *attr_name,
  hid_t type_id,
  hyperion::TypeTag dt);

static hid_t
init_kw(
  hid_t loc_id,
  const char *attr_name,
  hid_t type_id,
  hyperion::TypeTag dt) {

  {
    htri_t rc = CHECK_H5(H5Lexists(loc_id, attr_name, H5P_DEFAULT));
    if (rc > 0)
      CHECK_H5(H5Ldelete(loc_id, attr_name, H5P_DEFAULT));
  }
  hid_t result;
  {
    hid_t attr_ds = CHECK_H5(H5Screate(H5S_SCALAR));
    result =
      CHECK_H5(
        H5Dcreate(
          loc_id,
          attr_name,
          type_id,
          attr_ds,
          H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
    CHECK_H5(H5Sclose(attr_ds));
  }
  init_datatype_attr(result, dt);
  return result;
}

template <hyperion::TypeTag DT>
static void
write_kw(
  hid_t loc_id,
  const char *attr_name,
  const PhysicalRegion& region,
  FieldID fid);

template <hyperion::TypeTag DT>
static void
write_kw(
  hid_t loc_id,
  const char *attr_name,
  const PhysicalRegion& region,
  FieldID fid) {

  hid_t dt = hyperion::H5DatatypeManager::datatype<DT>();
  hid_t attr_id = init_kw(loc_id, attr_name, dt, DT);
  const KW<DT> kw(region, fid);
  CHECK_H5(H5Dwrite(attr_id, dt, H5S_ALL, H5S_ALL, H5P_DEFAULT, kw.ptr(0)));
  CHECK_H5(H5Dclose(attr_id));
}

template <>
void
write_kw<HYPERION_TYPE_STRING> (
  hid_t loc_id,
  const char *attr_name,
  const PhysicalRegion& region,
  FieldID fid) {

  hid_t dt = hyperion::H5DatatypeManager::datatype<HYPERION_TYPE_STRING>();
  hid_t attr_id =
    init_kw(loc_id, attr_name, dt, HYPERION_TYPE_STRING);
  const KW<HYPERION_TYPE_STRING> kw(region, fid);
  const hyperion::string& kwval = kw[0];
  hyperion::string buf;
  fstrcpy(buf.val, kwval.val);
  CHECK_H5(H5Dwrite(attr_id, dt, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf.val));
  CHECK_H5(H5Dclose(attr_id));
}

void
hyperion::hdf5::write_keywords(
  Runtime *rt,
  hid_t loc_id,
  const Keywords::pair<PhysicalRegion>& kw_prs) {

  for (auto& nm_dt_val : Keywords::to_map(rt, kw_prs)) {
    auto& nm = std::get<0>(nm_dt_val);
    auto& dt_val = std::get<1>(nm_dt_val);
    auto& dt = std::get<0>(dt_val);
    auto& val = std::get<1>(dt_val);
    switch (dt) {
#define WRITE_KW(DT)                                              \
      case DT: {                                                  \
        hid_t hdt = hyperion::H5DatatypeManager::datatype<DT>();  \
        hid_t attr_id = init_kw(loc_id, nm.c_str(), hdt, DT);     \
        CHECK_H5(                                                 \
          H5Dwrite(                                               \
            attr_id,                                              \
            hdt,                                                  \
            H5S_ALL,                                              \
            H5S_ALL,                                              \
            H5P_DEFAULT,                                          \
            CXX_ANY_NAMESPACE::any_cast<DataType<DT>::ValueType>(&val))); \
        CHECK_H5(H5Dclose(attr_id));                              \
        break;                                                    \
      }
      HYPERION_FOREACH_DATATYPE(WRITE_KW)
#undef WRITE_KW
      default:
        assert(false);
        break;
    }
  }
}

void
hyperion::hdf5::write_keywords(
  Context ctx,
  Runtime *rt,
  hid_t loc_id,
  const Keywords& keywords) {

  if (keywords.values_lr == LogicalRegion::NO_REGION)
    return;

  std::vector<std::string> keys = keywords.keys(rt);
  std::vector<FieldID> fids(keys.size());
  std::iota(fids.begin(), fids.end(), 0);
  RegionRequirement
    req(keywords.values_lr, READ_ONLY, EXCLUSIVE, keywords.values_lr);
  req.add_fields(fids);
  PhysicalRegion pr = rt->map_region(ctx, req);

  auto value_types = keywords.value_types(ctx, rt, fids);
  for (size_t i = 0; i < keys.size(); ++i) {
    assert(keys[i].substr(0, sizeof(HYPERION_NAMESPACE_PREFIX) - 1)
           != HYPERION_NAMESPACE_PREFIX);
    switch (value_types[i].value()) {
#define WRITE_KW(DT)                                  \
      case (DT):                                      \
        write_kw<DT>(loc_id, keys[i].c_str(), pr, i); \
        break;
      HYPERION_FOREACH_DATATYPE(WRITE_KW)
#undef WRITE_KW
    default:
        assert(false);
    }
  }
  rt->unmap_region(ctx, pr);
}

#ifdef HYPERION_USE_CASACORE
template <int D, typename A, typename T>
std::vector<T>
copy_mr_region(Runtime* rt, PhysicalRegion pr, FieldID fid) {

  // copy values into vector...pr index space may be sparse
  Domain domain =
    rt->get_index_space_domain(pr.get_logical_region().get_index_space());
  Rect<D,coord_t> rect = domain.bounds<D,coord_t>();
  size_t sz = 1;
  for (size_t i = 0; i < D; ++i)
    sz *= rect.hi[i] + 1;
  std::vector<T> result(sz);
  auto t = result.begin();
  const A acc(pr, fid);
  for (PointInRectIterator<D> pir(rect, false); pir(); pir++) {
    if (domain.contains(*pir))
      *t = acc[*pir];
    ++t;
  }
  return result;
}

template <int D, typename A, typename T>
std::vector<T>
copy_mr_region(
  Context ctx,
  Runtime *rt,
  LogicalRegion lr,
  FieldID fid) {

  RegionRequirement req(lr, READ_ONLY, EXCLUSIVE, lr);
  req.add_field(fid);
  auto pr = rt->map_region(ctx, req);
  auto result = copy_mr_region<D, A, T>(rt, pr, fid);
  rt->unmap_region(ctx, pr);
  return result;
}

template <int D, typename A, typename T>
static void
write_mr_region(
  Runtime* rt,
  hid_t ds,
  hid_t dt,
  PhysicalRegion pr,
  FieldID fid) {

  std::vector<T> buff = copy_mr_region<D, A, T>(rt, pr, fid);
  CHECK_H5(H5Dwrite(ds, dt, H5S_ALL, H5S_ALL, H5P_DEFAULT, buff.data()));
}

template <int D, typename A, typename T>
static void
write_mr_region(
  Context ctx,
  Runtime *rt,
  hid_t ds,
  hid_t dt,
  LogicalRegion lr,
  FieldID fid) {

  std::vector<T> buff = copy_mr_region<D, A, T>(ctx, rt, lr, fid);
  CHECK_H5(H5Dwrite(ds, dt, H5S_ALL, H5S_ALL, H5P_DEFAULT, buff.data()));
}

void
hyperion::hdf5::write_measure(
  Runtime* rt,
  hid_t mr_id,
  const MeasRef::DataRegions& mr_drs) {

  auto metadata_is = mr_drs.metadata.get_logical_region().get_index_space();
  auto values_is = mr_drs.values.get_logical_region().get_index_space();
  auto index_is =
    map(
      mr_drs.index,
      [](const auto& pr) { return pr.get_logical_region().get_index_space(); });
  std::vector<hsize_t> dims, dims1;
  hid_t sp, sp1 = -1;
  switch (metadata_is.get_dim()) {
#define SP(D)                                                       \
    case D:                                                         \
    {                                                               \
      Rect<D> bounds = rt->get_index_space_domain(metadata_is);     \
      dims.resize(D);                                               \
      for (size_t i = 0; i < D; ++i)                                \
        dims[i] = bounds.hi[i] + 1;                                 \
      sp = CHECK_H5(H5Screate_simple(D, dims.data(), NULL));        \
    }                                                               \
    {                                                               \
      Rect<D+1> bounds = rt->get_index_space_domain(values_is);     \
      dims1.resize(D + 1);                                          \
      for (size_t i = 0; i < D + 1; ++i)                            \
        dims1[i] = bounds.hi[i] + 1;                                \
      sp1 = CHECK_H5(H5Screate_simple(D + 1, dims1.data(), NULL));  \
    }                                                               \
    break;
    HYPERION_FOREACH_N_LESS_MAX(SP)
#undef SP
    default:
      assert(false);
      sp = -1;
      break;
  }
  {
    // Write the datasets for the MeasRef values directly, without going through
    // the Legion HDF5 interface, as the dataset sizes are small. Not worrying
    // too much about efficiency for this, in any case.
    {
      hid_t ds =
        CHECK_H5(
          H5Dcreate(
            mr_id,
            HYPERION_MEAS_REF_MCLASS_DS,
            H5DatatypeManager::datatypes()[
              H5DatatypeManager::MEASURE_CLASS_H5T],
            sp,
            H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));

      switch (dims.size()) {
#define W_MCLASS(D)                                                     \
        case D:                                                         \
          write_mr_region<                                              \
            D, \
            MeasRef::MeasureClassAccessor<READ_ONLY, D>, \
            MeasRef::MEASURE_CLASS_TYPE>( \
              rt,                                      \
              ds,                                      \
              H5DatatypeManager::datatypes()[          \
                H5DatatypeManager::MEASURE_CLASS_H5T], \
              mr_drs.metadata,                         \
              MeasRef::MEASURE_CLASS_FID);             \
          break;
        HYPERION_FOREACH_N_LESS_MAX(W_MCLASS);
#undef W_MCLASS
      default:
        assert(false);
        break;
      }
      CHECK_H5(H5Dclose(ds));
    }
    {
      hid_t ds =
        CHECK_H5(
          H5Dcreate(
            mr_id,
            HYPERION_MEAS_REF_RTYPE_DS,
            H5DatatypeManager::datatype<
            ValueType<MeasRef::REF_TYPE_TYPE>::DataType>(),
            sp,
            H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));

      switch (dims.size()) {
#define W_RTYPE(D)                                                      \
        case D:                                                         \
          write_mr_region<                                              \
            D, \
            MeasRef::RefTypeAccessor<READ_ONLY, D>, \
            MeasRef::REF_TYPE_TYPE>( \
              rt,                                                 \
              ds,                                                 \
              H5DatatypeManager::datatype<                        \
                ValueType<MeasRef::REF_TYPE_TYPE>::DataType>(),   \
              mr_drs.metadata,                                    \
              MeasRef::REF_TYPE_FID);                             \
          break;
        HYPERION_FOREACH_N_LESS_MAX(W_RTYPE);
#undef W_RTYPE
      default:
        assert(false);
        break;
      }
      CHECK_H5(H5Dclose(ds));
    }
    {
      hid_t ds =
        CHECK_H5(
          H5Dcreate(
            mr_id,
            HYPERION_MEAS_REF_NVAL_DS,
            H5DatatypeManager::datatype<
            ValueType<MeasRef::NUM_VALUES_TYPE>::DataType>(),
            sp,
            H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));

      switch (dims.size()) {
#define W_NVAL(D)                                                       \
        case D:                                                         \
          write_mr_region<                                              \
            D, \
            MeasRef::NumValuesAccessor<READ_ONLY, D>, \
            MeasRef::NUM_VALUES_TYPE>( \
              rt,                                                   \
              ds,                                                   \
              H5DatatypeManager::datatype<                          \
                ValueType<MeasRef::NUM_VALUES_TYPE>::DataType>(),   \
              mr_drs.metadata,                                      \
              MeasRef::NUM_VALUES_FID);                             \
          break;
        HYPERION_FOREACH_N_LESS_MAX(W_NVAL);
#undef W_NVAL
      default:
        assert(false);
        break;
      }
      CHECK_H5(H5Dclose(ds));
    }
  }
  if (dims1.size() > 0) {
    hid_t ds =
      CHECK_H5(
        H5Dcreate(
          mr_id,
          HYPERION_MEAS_REF_VALUES_DS,
          H5DatatypeManager::datatype<ValueType<MeasRef::VALUE_TYPE>::DataType>(),
          sp1,
          H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));

    switch (dims1.size()) {
#define W_VALUES(D)                                                     \
      case D + 1:                                                       \
        write_mr_region<                                                \
          D + 1, \
          MeasRef::ValueAccessor<READ_ONLY, D + 1>, \
          MeasRef::VALUE_TYPE>( \
            rt,                                             \
            ds,                                             \
            H5DatatypeManager::datatype<                    \
              ValueType<MeasRef::VALUE_TYPE>::DataType>(),  \
            mr_drs.values,                                  \
            0);                                             \
        break;
      HYPERION_FOREACH_N_LESS_MAX(W_VALUES);
#undef W_VALUES
    default:
      assert(false);
      break;
    }
    CHECK_H5(H5Dclose(ds));
  }
  // write the index array, if it exists
  if (index_is) {
    hid_t udt =
      H5DatatypeManager::datatype<
        ValueType<MeasRef::M_CODE_TYPE>::DataType>();
    hid_t ds =
      CHECK_H5(
        H5Dcreate(
          mr_id,
          HYPERION_MEAS_REF_INDEX_DS,
          udt,
          sp1,
          H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
    write_mr_region<
      1,
      MeasRef::MCodeAccessor<READ_ONLY>,
      MeasRef::M_CODE_TYPE>(
        rt,
        ds,
        udt,
        mr_drs.index.value(),
        MeasRef::M_CODE_FID);
    CHECK_H5(H5Dclose(ds));
  }
  IndexTreeL metadata_tree = index_space_as_tree(rt, metadata_is);
  write_index_tree_to_attr<binary_index_tree_serdez>(
    mr_id,
    "metadata_index_tree",
    metadata_tree);
  IndexTreeL value_tree = index_space_as_tree(rt, values_is);
  write_index_tree_to_attr<binary_index_tree_serdez>(
    mr_id,
    "value_index_tree",
    value_tree);
}

void
hyperion::hdf5::write_measure(
  Context ctx,
  Runtime* rt,
  hid_t mr_id,
  const MeasRef& mr) {

  if (!mr.is_empty()) {
#if HAVE_CXX17
    auto [mreq, vreq, oireq] = mr.requirements(READ_ONLY);
#else // !HAVE_CXX17
    auto rqs = mr.requirements(READ_ONLY);
    auto& mreq = std::get<0>(rqs);
    auto& vreq = std::get<1>(rqs);
    auto& oireq = std::get<2>(rqs);
#endif // HAVE_CXX17
    auto mpr = rt->map_region(ctx, mreq);
    auto vpr = rt->map_region(ctx, vreq);
    auto oipr =
      map(
        oireq,
        [&ctx, rt](const auto& rq) { return rt->map_region(ctx, rq); });
    write_measure(rt, mr_id, MeasRef::DataRegions{mpr, vpr, oipr});
    map(
      oipr,
      [&ctx, rt](const auto& pr) {
        rt->unmap_region(ctx, pr);
        return true;
      });
    rt->unmap_region(ctx, vpr);
    rt->unmap_region(ctx, mpr);
  }
}
#endif //HYPERION_USE_CASACORE

void
hyperion::hdf5::write_column(
  Runtime* rt,
  hid_t col_grp_id,
  const std::string& cs_name,
  const PhysicalColumn& column) {

  auto axes =
    ColumnSpace::from_axis_vector(ColumnSpace::axes(column.metadata()));

  // create column dataset
  {
    auto rank = axes.size();
    hsize_t dims[rank];
    switch (rank) {
#define DIMS(N)                                                 \
      case N: {                                                 \
        Rect<N> rect = column.domain().bounds<N, coord_t>();    \
        for (size_t i = 0; i < N; ++i)                          \
          dims[i] = rect.hi[i] + 1;                             \
        break;                                                  \
      }
      HYPERION_FOREACH_N(DIMS)
#undef DIMS
    default:
      assert(false);
      break;
    }
    hid_t ds = CHECK_H5(H5Screate_simple(rank, dims, NULL));

    hid_t dt;
    switch (column.dt()) {
#define DT(T) \
      case T: dt = H5DatatypeManager::datatype<T>(); break;
      HYPERION_FOREACH_DATATYPE(DT)
#undef DT
      default:
        assert(false);
        dt = -1;
        break;
    }

    hid_t col_id =
      CHECK_H5(
        H5Dcreate(
          col_grp_id,
          HYPERION_COLUMN_DS,
          dt,
          ds,
          H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
    CHECK_H5(H5Sclose(ds));
    CHECK_H5(H5Dclose(col_id));

    // write column value datatype
    {
      hid_t ds = CHECK_H5(H5Screate(H5S_SCALAR));
      hid_t did = hyperion::H5DatatypeManager::datatypes()[
        hyperion::H5DatatypeManager::DATATYPE_H5T];
      hid_t attr_id =
        CHECK_H5(
          H5Acreate(
            col_grp_id,
            HYPERION_ATTRIBUTE_DT,
            did,
            ds,
            H5P_DEFAULT, H5P_DEFAULT));
      auto col_dt = column.dt();
      CHECK_H5(H5Awrite(attr_id, did, &col_dt));
      CHECK_H5(H5Sclose(ds));
      CHECK_H5(H5Aclose(attr_id));
    }

    // write column fid
    {
      hid_t ds = CHECK_H5(H5Screate(H5S_SCALAR));
      hid_t fid_dt = hyperion::H5DatatypeManager::datatypes()[
        hyperion::H5DatatypeManager::FIELD_ID_H5T];
      hid_t attr_id =
        CHECK_H5(
          H5Acreate(
            col_grp_id,
            HYPERION_ATTRIBUTE_FID,
            fid_dt,
            ds,
            H5P_DEFAULT, H5P_DEFAULT));
      auto col_fid = column.fid();
      CHECK_H5(H5Awrite(attr_id, fid_dt, &col_fid));
      CHECK_H5(H5Sclose(ds));
      CHECK_H5(H5Aclose(attr_id));
    }
  }

  // write link to column space
  {
    auto target_path = std::string("../") + cs_name;
    CHECK_H5(
      H5Lcreate_soft(
        target_path.c_str(),
        col_grp_id,
        column_space_link_name,
        H5P_DEFAULT, H5P_DEFAULT));
  }

#ifdef HYPERION_USE_CASACORE
  // write measure reference column name to attribute
  if (column.refcol()) {
    hsize_t dims = 1;
    hid_t refcol_ds = CHECK_H5(H5Screate_simple(1, &dims, NULL));
    const hid_t sdt = H5DatatypeManager::datatype<HYPERION_TYPE_STRING>();
    hid_t refcol_id =
      CHECK_H5(
        H5Acreate(
          col_grp_id,
          column_refcol_attr_name,
          sdt,
          refcol_ds,
          H5P_DEFAULT, H5P_DEFAULT));
    CHECK_H5(
      H5Awrite(refcol_id, sdt, std::get<0>(column.refcol().value()).c_str()));
    CHECK_H5(H5Aclose(refcol_id));
    CHECK_H5(H5Sclose(refcol_ds));
  }

  if (column.mr_drs()) {
    hid_t measure_id =
      CHECK_H5(
        H5Gcreate(
          col_grp_id,
          HYPERION_MEASURE_GROUP,
          H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
    write_measure(rt, measure_id, column.mr_drs().value());
    CHECK_H5(H5Gclose(measure_id));
  }
#endif

  if (column.kws())
    write_keywords(rt, col_grp_id, column.kws().value());
}

void
hyperion::hdf5::write_column(
  Context ctx,
  Runtime* rt,
  hid_t col_grp_id,
  const std::string& column_space_name,
  const Column& column) {

  std::vector<int> axes;
  {
    RegionRequirement
      req(column.cs.metadata_lr, READ_ONLY, EXCLUSIVE, column.cs.metadata_lr);
    req.add_field(ColumnSpace::AXIS_VECTOR_FID);
    auto pr = rt->map_region(ctx, req);
    const ColumnSpace::AxisVectorAccessor<READ_ONLY>
      ax(pr, ColumnSpace::AXIS_VECTOR_FID);
    axes = ColumnSpace::from_axis_vector(ax[0]);
    rt->unmap_region(ctx, pr);
  }

  // create column dataset
  {
    auto rank = axes.size();
    hsize_t dims[rank];
    switch (rank) {
#define DIMS(N)                                                 \
      case N: {                                                 \
        Rect<N> rect =                                          \
          rt->get_index_space_domain(ctx, column.cs.column_is)  \
          .bounds<N, coord_t>();                                \
        for (size_t i = 0; i < N; ++i)                          \
          dims[i] = rect.hi[i] + 1;                             \
        break;                                                  \
      }
      HYPERION_FOREACH_N(DIMS)
#undef DIMS
    default:
      assert(false);
      break;
    }
    hid_t ds = CHECK_H5(H5Screate_simple(rank, dims, NULL));

    hid_t dt;
    switch (column.dt) {
#define DT(T) \
      case T: dt = H5DatatypeManager::datatype<T>(); break;
      HYPERION_FOREACH_DATATYPE(DT)
#undef DT
      default:
        assert(false);
        dt = -1;
        break;
    }

    hid_t col_id =
      CHECK_H5(
        H5Dcreate(
          col_grp_id,
          HYPERION_COLUMN_DS,
          dt,
          ds,
          H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
    CHECK_H5(H5Sclose(ds));
    CHECK_H5(H5Dclose(col_id));

    // write column value datatype
    {
      hid_t ds = CHECK_H5(H5Screate(H5S_SCALAR));
      hid_t did = hyperion::H5DatatypeManager::datatypes()[
        hyperion::H5DatatypeManager::DATATYPE_H5T];
      hid_t attr_id =
        CHECK_H5(
          H5Acreate(
            col_grp_id,
            HYPERION_ATTRIBUTE_DT,
            did,
            ds,
            H5P_DEFAULT, H5P_DEFAULT));
      CHECK_H5(H5Awrite(attr_id, did, &column.dt));
      CHECK_H5(H5Sclose(ds));
      CHECK_H5(H5Aclose(attr_id));
    }

    // write column fid
    {
      hid_t ds = CHECK_H5(H5Screate(H5S_SCALAR));
      hid_t fid_dt = hyperion::H5DatatypeManager::datatypes()[
        hyperion::H5DatatypeManager::FIELD_ID_H5T];
      hid_t attr_id =
        CHECK_H5(
          H5Acreate(
            col_grp_id,
            HYPERION_ATTRIBUTE_FID,
            fid_dt,
            ds,
            H5P_DEFAULT, H5P_DEFAULT));
      CHECK_H5(H5Awrite(attr_id, fid_dt, &column.fid));
      CHECK_H5(H5Sclose(ds));
      CHECK_H5(H5Aclose(attr_id));
    }
  }

  // write link to column space
  {
    auto target_path = std::string("../") + column_space_name;
    CHECK_H5(
      H5Lcreate_soft(
        target_path.c_str(),
        col_grp_id,
        column_space_link_name,
        H5P_DEFAULT, H5P_DEFAULT));
  }

  // write measure reference column name to attribute
  if (column.rc) {
    hsize_t dims = 1;
    hid_t refcol_ds = CHECK_H5(H5Screate_simple(1, &dims, NULL));
    const hid_t sdt = H5DatatypeManager::datatype<HYPERION_TYPE_STRING>();
    hid_t refcol_id =
      CHECK_H5(
        H5Acreate(
          col_grp_id,
          column_refcol_attr_name,
          sdt,
          refcol_ds,
          H5P_DEFAULT, H5P_DEFAULT));
    CHECK_H5(H5Awrite(refcol_id, sdt, column.rc.value().val));
    CHECK_H5(H5Aclose(refcol_id));
    CHECK_H5(H5Sclose(refcol_ds));
  }

#ifdef HYPERION_USE_CASACORE
  if (!column.mr.is_empty()) {
    hid_t measure_id =
      CHECK_H5(
        H5Gcreate(
          col_grp_id,
          HYPERION_MEASURE_GROUP,
          H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
    write_measure(ctx, rt, measure_id, column.mr);
    CHECK_H5(H5Gclose(measure_id));
  }
#endif

  write_keywords(ctx, rt, col_grp_id, column.kw);
}

void
hyperion::hdf5::write_columnspace(
  Runtime* rt,
  hid_t cs_grp_id,
  const PhysicalRegion& cs_md,
  const IndexSpace& cs_is,
  hid_t table_axes_dt) {

  {
    auto axes = ColumnSpace::axes(cs_md);
    hsize_t dims = ColumnSpace::size(axes);
    hid_t ds = CHECK_H5(H5Screate_simple(1, &dims, NULL));
    hid_t id =
      CHECK_H5(
        H5Acreate(
          cs_grp_id,
          HYPERION_COLUMN_SPACE_AXES,
          table_axes_dt,
          ds,
          H5P_DEFAULT, H5P_DEFAULT));
    std::vector<unsigned char> ax;
    ax.reserve(dims);
    std::copy(
      axes.begin(),
      axes.begin() + dims,
      std::back_inserter(ax));
    CHECK_H5(H5Awrite(id, table_axes_dt, ax.data()));
    CHECK_H5(H5Aclose(id));
    CHECK_H5(H5Sclose(ds));
  }
  {
    bool is_index = ColumnSpace::is_index(cs_md);
    hsize_t dims = 1;
    hid_t ds = CHECK_H5(H5Screate_simple(1, &dims, NULL));
    hid_t id =
      CHECK_H5(
        H5Acreate(
          cs_grp_id,
          HYPERION_COLUMN_SPACE_FLAG,
          H5T_NATIVE_HBOOL,
          ds,
          H5P_DEFAULT, H5P_DEFAULT));
    CHECK_H5(H5Awrite(id, H5T_NATIVE_HBOOL, &is_index));
    CHECK_H5(H5Aclose(id));
    CHECK_H5(H5Sclose(ds));
  }
  {
    auto uid = ColumnSpace::axes_uid(cs_md);
    hsize_t dims = 1;
    hid_t ds = CHECK_H5(H5Screate_simple(1, &dims, NULL));
    hid_t dt = H5DatatypeManager::datatype<HYPERION_TYPE_STRING>();
    hid_t id =
      CHECK_H5(
        H5Acreate(
          cs_grp_id,
          HYPERION_COLUMN_SPACE_AXES_UID,
          dt,
          ds,
          H5P_DEFAULT, H5P_DEFAULT));
    CHECK_H5(H5Awrite(id, dt, uid.val));
    CHECK_H5(H5Aclose(id));
    CHECK_H5(H5Sclose(ds));
  }
  auto itree = index_space_as_tree(rt, cs_is);
  // TODO: it would make more sense to simply write the index tree into
  // a dataset for the ColumnSpace (and replace the ColumnSpace group
  // with that dataset)
  write_index_tree_to_attr<binary_index_tree_serdez>(
    cs_grp_id,
    HYPERION_COLUMN_SPACE_INDEX_TREE,
    itree);
}

void
hyperion::hdf5::write_columnspace(
  Context ctx,
  Runtime* rt,
  hid_t cs_grp_id,
  const ColumnSpace& cs,
  hid_t table_axes_dt) {

  RegionRequirement req(cs.metadata_lr, READ_ONLY, EXCLUSIVE, cs.metadata_lr);
  req.add_field(ColumnSpace::AXIS_SET_UID_FID);
  req.add_field(ColumnSpace::AXIS_VECTOR_FID);
  req.add_field(ColumnSpace::INDEX_FLAG_FID);
  auto pr = rt->map_region(ctx, req);
  write_columnspace(rt, cs_grp_id, pr, cs.column_is, table_axes_dt);
  rt->unmap_region(ctx, pr);
}

static herr_t
remove_column_space_groups(
  hid_t group,
  const char* name,
  const H5L_info_t*,
  void*) {

  if (starts_with(name, HYPERION_COLUMN_SPACE_GROUP_PREFIX))
    CHECK_H5(H5Ldelete(group, name, H5P_DEFAULT));
  return 0;
}

static void
write_table_columns(
  Context ctx,
  Runtime* rt,
  hid_t table_grp_id,
  hid_t table_axes_dt,
  const std::unordered_map<std::string, Column>& columns) {

  if (columns.size() == 0)
    return;

  std::map<ColumnSpace, std::set<std::string>> column_groups;
  for (auto& nm_col : columns) {
#if HAVE_CXX17
    auto& [nm, col] = nm_col;
#else // !HAVE_CXX17
    auto& nm = std::get<0>(nm_col);
    auto& col = std::get<1>(nm_col);
#endif // HAVE_CXX17
    if (column_groups.count(col.cs) == 0)
      column_groups[col.cs] = {};
    column_groups[col.cs].insert(nm);
  }
  size_t i = 0;
  for (auto& cs_colnames : column_groups) {
    auto& cs = std::get<0>(cs_colnames);
    auto& colnames = std::get<1>(cs_colnames);
    auto cs_nm =
      std::string(HYPERION_COLUMN_SPACE_GROUP_PREFIX) + std::to_string(i++);
    // write the ColumnSpace group, and add its attributes to the group
    {
      hid_t cs_grp_id =
        CHECK_H5(
          H5Gcreate(
            table_grp_id,
            cs_nm.c_str(),
            H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
      write_columnspace(ctx, rt, cs_grp_id, cs, table_axes_dt);
      CHECK_H5(H5Gclose(cs_grp_id));
    }

    // write the (included) Columns in this ColumnSpace
    for (auto& colname : colnames) {
      htri_t grp_exists =
        CHECK_H5(H5Lexists(table_grp_id, colname.c_str(), H5P_DEFAULT));
      if (grp_exists > 0)
        CHECK_H5(H5Ldelete(table_grp_id, colname.c_str(), H5P_DEFAULT));
      hid_t col_grp_id =
        CHECK_H5(
          H5Gcreate(
            table_grp_id,
            colname.c_str(),
            H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
      write_column(ctx, rt, col_grp_id, cs_nm, columns.at(colname));
      CHECK_H5(H5Gclose(col_grp_id));
    }
  }
}

static void
write_table_columns(
  Runtime* rt,
  hid_t table_grp_id,
  hid_t table_axes_dt,
  const PhysicalTable& table) {

  auto columns = table.columns();
  std::map<ColumnSpace, std::set<std::string>> column_groups;
  for (auto& nm_col : columns) {
#if HAVE_CXX17
    auto& [nm, col] = nm_col;
#else // !HAVE_CXX17
    auto& nm = std::get<0>(nm_col);
    auto& col = std::get<1>(nm_col);
#endif // HAVE_CXX17
    auto cs = col->column_space();
    if (column_groups.count(cs) == 0)
      column_groups[cs] = {};
    column_groups[cs].insert(nm);
  }
  size_t i = 0;
  for (auto& cs_colnames : column_groups) {
    auto& cs = std::get<0>(cs_colnames);
    auto& colnames = std::get<1>(cs_colnames);
    auto cs_nm =
      std::string(HYPERION_COLUMN_SPACE_GROUP_PREFIX) + std::to_string(i++);
    // write the ColumnSpace group, and add its attributes to the group
    {
      hid_t cs_grp_id =
        CHECK_H5(
          H5Gcreate(
            table_grp_id,
            cs_nm.c_str(),
            H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
      write_columnspace(
        rt,
        cs_grp_id,
        columns.begin()->second->metadata(),
        cs.column_is,
        table_axes_dt);
      CHECK_H5(H5Gclose(cs_grp_id));
    }

    // write the (included) Columns in this ColumnSpace
    for (auto& colname : colnames) {
      htri_t grp_exists =
        CHECK_H5(H5Lexists(table_grp_id, colname.c_str(), H5P_DEFAULT));
      if (grp_exists > 0)
        CHECK_H5(H5Ldelete(table_grp_id, colname.c_str(), H5P_DEFAULT));
      hid_t col_grp_id =
        CHECK_H5(
          H5Gcreate(
            table_grp_id,
            colname.c_str(),
            H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
      write_column(rt, col_grp_id, cs_nm, *columns.at(colname));
      CHECK_H5(H5Gclose(col_grp_id));
    }
  }
}

void
hyperion::hdf5::write_table(
  Runtime* rt,
  hid_t table_grp_id,
  const PhysicalTable& table) {

  if (table.columns().size() == 0)
    return;

  hid_t table_axes_dt;
  {
    auto axes = AxesRegistrar::axes(table.axes_uid().value());
    assert(axes);
    table_axes_dt = axes.value().h5_datatype;
  }
  // write axes datatype to table
  CHECK_H5(
    H5Tcommit(
      table_grp_id,
      table_axes_dt_name,
      table_axes_dt,
      H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));

  // write index axes attribute to table
  htri_t rc = CHECK_H5(H5Aexists(table_grp_id, table_index_axes_attr_name));
  if (rc > 0)
    CHECK_H5(H5Adelete(table_grp_id, table_index_axes_attr_name));
  {
    auto index_axes = table.index_axes();
    hsize_t dims = index_axes.size();
    hid_t index_axes_ds = CHECK_H5(H5Screate_simple(1, &dims, NULL));
    hid_t index_axes_id =
      CHECK_H5(
        H5Acreate(
          table_grp_id,
          table_index_axes_attr_name,
          table_axes_dt,
          index_axes_ds,
          H5P_DEFAULT, H5P_DEFAULT));
    std::vector<unsigned char> ax;
    ax.reserve(dims);
    std::copy(
      index_axes.begin(),
      index_axes.begin() + dims,
      std::back_inserter(ax));
    CHECK_H5(H5Awrite(index_axes_id, table_axes_dt, ax.data()));
    CHECK_H5(H5Aclose(index_axes_id));
    CHECK_H5(H5Sclose(index_axes_ds));
  }

  // delete all column space groups
  CHECK_H5(
    H5Literate(
      table_grp_id,
      H5_INDEX_NAME,
      H5_ITER_NATIVE,
      NULL,
      remove_column_space_groups,
      NULL));

  // write the table index ColumnSpace
  {
    hid_t cs_grp_id =
      CHECK_H5(
        H5Gcreate(
          table_grp_id,
          HYPERION_INDEX_COLUMN_SPACE_GROUP,
          H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
    write_columnspace(
      rt,
      cs_grp_id,
      table.index_column_space_metadata(),
      table.index_column_space_index_space(),
      table_axes_dt);
    CHECK_H5(H5Gclose(cs_grp_id));
  }

  // FIXME: awaiting Table keywords support...
  // write_keywords(rt, table_grp_id, table.m_kws);

  write_table_columns(rt, table_grp_id, table_axes_dt, table);
}

void
hyperion::hdf5::write_table(
  Context ctx,
  Runtime* rt,
  hid_t table_grp_id,
  const Table& table,
  const std::unordered_set<std::string>& columns) {

  auto tbl_columns = table.columns();

  if (tbl_columns.size() == 0)
    return;

  hid_t table_axes_dt;
  {
    auto& cs = tbl_columns.begin()->second.cs;
    RegionRequirement req(cs.metadata_lr, READ_ONLY, EXCLUSIVE, cs.metadata_lr);
    req.add_field(ColumnSpace::AXIS_SET_UID_FID);
    auto pr = rt->map_region(ctx, req);
    const ColumnSpace::AxisSetUIDAccessor<READ_ONLY>
      au(pr, ColumnSpace::AXIS_SET_UID_FID);
    auto axes = AxesRegistrar::axes(au[0]);
    assert(axes);
    table_axes_dt = axes.value().h5_datatype;
    rt->unmap_region(ctx, pr);
  }
  // write axes datatype to table
  CHECK_H5(
    H5Tcommit(
      table_grp_id,
      table_axes_dt_name,
      table_axes_dt,
      H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));

  // write index axes attribute to table
  htri_t rc = CHECK_H5(H5Aexists(table_grp_id, table_index_axes_attr_name));
  if (rc > 0)
    CHECK_H5(H5Adelete(table_grp_id, table_index_axes_attr_name));
  {
    auto ics = table.index_column_space(ctx, rt);
    auto index_axes = ics.axes(ctx, rt);
    hsize_t dims = index_axes.size();
    hid_t index_axes_ds = CHECK_H5(H5Screate_simple(1, &dims, NULL));
    hid_t index_axes_id =
      CHECK_H5(
        H5Acreate(
          table_grp_id,
          table_index_axes_attr_name,
          table_axes_dt,
          index_axes_ds,
          H5P_DEFAULT, H5P_DEFAULT));
    std::vector<unsigned char> ax;
    ax.reserve(index_axes.size());
    std::copy(index_axes.begin(), index_axes.end(), std::back_inserter(ax));
    CHECK_H5(H5Awrite(index_axes_id, table_axes_dt, ax.data()));
    CHECK_H5(H5Aclose(index_axes_id));
    CHECK_H5(H5Sclose(index_axes_ds));
  }

  // delete all column space groups
  CHECK_H5(
    H5Literate(
      table_grp_id,
      H5_INDEX_NAME,
      H5_ITER_NATIVE,
      NULL,
      remove_column_space_groups,
      NULL));

  // write the table index ColumnSpace
  {
    hid_t cs_grp_id =
      CHECK_H5(
        H5Gcreate(
          table_grp_id,
          HYPERION_INDEX_COLUMN_SPACE_GROUP,
          H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT));
    auto ics = table.index_column_space(ctx, rt);
    write_columnspace(ctx, rt, cs_grp_id, ics, table_axes_dt);
    CHECK_H5(H5Gclose(cs_grp_id));
  }

  // FIXME: awaiting Table keywords support...
  // write_keywords(ctx, rt, table_grp_id, table.keywords);

  std::unordered_map<std::string, Column> selected_columns;
  for (auto& col : columns) {
    if (tbl_columns.count(col) > 0)
      selected_columns[col] = tbl_columns.at(col);
  }

  write_table_columns(ctx, rt, table_grp_id, table_axes_dt, selected_columns);
}

void
hyperion::hdf5::write_table(
  Context ctx,
  Runtime* rt,
  hid_t table_grp_id,
  const Table& table) {

  auto tbl_columns = table.columns();

  std::unordered_set<std::string> columns;
  for (auto& nm_col : tbl_columns)
    columns.insert(std::get<0>(nm_col));

  write_table(ctx, rt, table_grp_id, table, columns);
}

static herr_t
acc_kw_names(
  hid_t loc_id,
  const char* name,
  const H5L_info_t*,
  void* ctx) {

  std::vector<std::string>* acc = static_cast<std::vector<std::string>*>(ctx);
  if (!starts_with(name, HYPERION_NAMESPACE_PREFIX)) {
    H5O_info_t infobuf;
    CHECK_H5(H5Oget_info_by_name(loc_id, name, &infobuf, H5P_DEFAULT));
    if (infobuf.type == H5O_TYPE_DATASET)
      acc->push_back(name);
  }
  return 0;
}

static hyperion::TypeTag
read_dt_value(hid_t dt_id) {
  hyperion::TypeTag dt;
  // enumeration datatypes are converted by libhdf5 based on symbol names, which
  // ensures interoperability for hyperion HDF5 files written with one enumeration
  // definition and read with a different enumeration definition (for example,
  // in two hyperion codes built with and without HYPERION_USE_CASACORE)
  CHECK_H5(H5Aread(dt_id, H5T_NATIVE_INT, &dt));
  return dt;
}

hyperion::Keywords::kw_desc_t
hyperion::hdf5::init_keywords(hid_t loc_id) {

  std::vector<std::string> kw_names;
  CHECK_H5(
    H5Literate(
      loc_id,
      H5_INDEX_NAME,
      H5_ITER_INC,
      NULL,
      acc_kw_names,
      &kw_names));

  if (kw_names.size() == 0)
    return {};

  return
    hyperion::map(
      kw_names,
      [&](const auto& nm) {
        hid_t dt_id =
          CHECK_H5(
            H5Aopen_by_name(
              loc_id,
              nm.c_str(),
              HYPERION_ATTRIBUTE_DT,
              H5P_DEFAULT, H5P_DEFAULT));
        hyperion::TypeTag dt = read_dt_value(dt_id);
        CHECK_H5(H5Aclose(dt_id));
        return std::make_tuple(nm, dt);
      });
}

#ifdef HYPERION_USE_CASACORE
template <typename T>
std::vector<T>
copy_mr_ds(hid_t ds) {

  hid_t spc = CHECK_H5(H5Dget_space(ds));
  [[maybe_unused]] int rank = CHECK_H5(H5Sget_simple_extent_ndims(spc));
  assert(rank > 0);
  hssize_t npts = H5Sget_simple_extent_npoints(spc);
  std::vector<T> result(npts);
  CHECK_H5(
    H5Dread(
      ds,
      H5DatatypeManager::datatype<ValueType<T>::DataType>(),
      H5S_ALL,
      H5S_ALL,
      H5P_DEFAULT,
      result.data()));
  CHECK_H5(H5Sclose(spc));
  return result;
}

template <int D, typename A, typename T>
static void
read_mr_region(
  Context ctx,
  Runtime* rt,
  hid_t ds,
  LogicalRegion region,
  FieldID fid) {

  std::vector<T> buff = copy_mr_ds<T>(ds);
  RegionRequirement req(region, WRITE_ONLY, EXCLUSIVE, region);
  req.add_field(fid);
  auto pr = rt->map_region(ctx, req);
  const A acc(pr, fid);
  Domain dom = rt->get_index_space_domain(region.get_index_space());
  Rect<D,coord_t> rect = dom.bounds<D,coord_t>();
  auto t = buff.begin();
  for (PointInRectIterator<D> pir(rect, false); pir(); pir++) {
    if (dom.contains(*pir))
      acc[*pir] = *t;
    ++t;
  }
  rt->unmap_region(ctx, pr);
}

static MeasRef
init_meas_ref(
  Context ctx,
  Runtime* rt,
  hid_t loc_id,
  const CXX_OPTIONAL_NAMESPACE::optional<IndexTreeL>& metadata_tree,
  const CXX_OPTIONAL_NAMESPACE::optional<IndexTreeL>& value_tree,
  bool no_index) {

  if (!metadata_tree)
    return MeasRef();

  std::array<LogicalRegion, 3> regions =
    MeasRef::create_regions(
      ctx,
      rt,
      metadata_tree.value(),
      value_tree.value(),
      no_index);
  LogicalRegion metadata_lr = regions[0];
  LogicalRegion values_lr = regions[1];
  LogicalRegion index_lr = regions[2];
  {
    // Read the datasets for the MeasRef values directly.
    {
      hid_t ds =
        CHECK_H5(H5Dopen(loc_id, HYPERION_MEAS_REF_MCLASS_DS, H5P_DEFAULT));

      switch (metadata_lr.get_index_space().get_dim()) {
#define W_MCLASS(D)                                                     \
        case D:                                                         \
          read_mr_region<                                               \
            D, \
            MeasRef::MeasureClassAccessor<WRITE_ONLY, D>, \
            MeasRef::MEASURE_CLASS_TYPE>( \
              ctx,                                                      \
              rt,                                                       \
              ds,                                                       \
              metadata_lr,                                              \
              MeasRef::MEASURE_CLASS_FID);                              \
          break;
        HYPERION_FOREACH_N_LESS_MAX(W_MCLASS);
#undef W_MCLASS
      default:
        assert(false);
        break;
      }
      CHECK_H5(H5Dclose(ds));
    }
    {
      hid_t ds =
        CHECK_H5(H5Dopen(loc_id, HYPERION_MEAS_REF_RTYPE_DS, H5P_DEFAULT));

      switch (metadata_lr.get_index_space().get_dim()) {
#define W_RTYPE(D)                                                      \
        case D:                                                         \
          read_mr_region<                                               \
            D, \
            MeasRef::RefTypeAccessor<WRITE_ONLY, D>, \
            MeasRef::REF_TYPE_TYPE>( \
              ctx,                                                      \
              rt,                                                       \
              ds,                                                       \
              metadata_lr,                                              \
              MeasRef::REF_TYPE_FID);                                   \
          break;
        HYPERION_FOREACH_N_LESS_MAX(W_RTYPE);
#undef W_RTYPE
      default:
        assert(false);
        break;
      }
      CHECK_H5(H5Dclose(ds));
    }
    {
      hid_t ds =
        CHECK_H5(H5Dopen(loc_id, HYPERION_MEAS_REF_NVAL_DS, H5P_DEFAULT));

      switch (metadata_lr.get_index_space().get_dim()) {
#define W_NVAL(D)                                                       \
        case D:                                                         \
          read_mr_region<                                               \
            D, \
            MeasRef::NumValuesAccessor<WRITE_ONLY, D>, \
            MeasRef::NUM_VALUES_TYPE>( \
              ctx,                                                      \
              rt,                                                       \
              ds,                                                       \
              metadata_lr,                                              \
              MeasRef::NUM_VALUES_FID);                                 \
          break;
        HYPERION_FOREACH_N_LESS_MAX(W_NVAL);
#undef W_NVAL
      default:
        assert(false);
        break;
      }
      CHECK_H5(H5Dclose(ds));
    }
  }
  if (values_lr != LogicalRegion::NO_REGION) {
    hid_t ds =
      CHECK_H5(H5Dopen(loc_id, HYPERION_MEAS_REF_VALUES_DS, H5P_DEFAULT));

    switch (values_lr.get_index_space().get_dim()) {
#define W_VALUES(D)                                                     \
      case D:                                                           \
        read_mr_region<                                                 \
          D, \
          MeasRef::ValueAccessor<WRITE_ONLY, D>, \
          MeasRef::VALUE_TYPE>( \
            ctx,                                                        \
            rt,                                                         \
            ds,                                                         \
            values_lr,                                                  \
            0);                                                         \
        break;
      HYPERION_FOREACH_N(W_VALUES);
#undef W_VALUES
    default:
      assert(false);
      break;
    }
    CHECK_H5(H5Dclose(ds));
  }
  if (index_lr != LogicalRegion::NO_REGION) {
    hid_t ds =
      CHECK_H5(H5Dopen(loc_id, HYPERION_MEAS_REF_INDEX_DS, H5P_DEFAULT));
    read_mr_region<
      1,
      MeasRef::MCodeAccessor<WRITE_ONLY>,
      MeasRef::M_CODE_TYPE>(ctx, rt, ds, index_lr, MeasRef::M_CODE_FID);
    CHECK_H5(H5Dclose(ds));
  }
  return MeasRef(metadata_lr, values_lr, index_lr);
}
#endif // HYPERION_USE_CASACORE

struct acc_tflds_t {
  Context ctx;
  Runtime* rt;
  std::unordered_map<
    std::string,
    std::vector<std::pair<std::string, TableField>>> cs_fields;
};

herr_t
acc_tflds_fn(
  hid_t group,
  const char* name,
  const H5L_info_t* info,
  void* op_data) {

  acc_tflds_t* args = static_cast<acc_tflds_t*>(op_data);
  if (!starts_with(name, HYPERION_COLUMN_SPACE_GROUP_PREFIX)) {
    H5O_info_t infobuf;
    CHECK_H5(H5Oget_info_by_name(group, name, &infobuf, H5P_DEFAULT));
    if (infobuf.type == H5O_TYPE_GROUP) {
      hid_t col_grp_id = CHECK_H5(H5Gopen(group, name, H5P_DEFAULT));
      htri_t cs_link_exists =
        CHECK_H5(H5Lexists(col_grp_id, column_space_link_name, H5P_DEFAULT));
      if (cs_link_exists > 0) {
        H5L_info_t linfo;
        CHECK_H5(
          H5Lget_info(col_grp_id, column_space_link_name, &linfo, H5P_DEFAULT));
        assert(linfo.type == H5L_TYPE_SOFT);
        std::vector<char> target(linfo.u.val_size);
        CHECK_H5(
          H5Lget_val(
            col_grp_id,
            column_space_link_name,
            target.data(),
            target.size(),
            H5P_DEFAULT));
        assert(starts_with(target.data(), "../"));
        std::string cs_name = target.data() + 3;
        htri_t ds_exists =
          CHECK_H5(H5Lexists(col_grp_id, HYPERION_COLUMN_DS, H5P_DEFAULT));
        if (ds_exists > 0) {
          // from here we'll assume that col_grp_id is a Column group
          TableField tfld;
          {
            hid_t dt_attr_id =
              CHECK_H5(H5Aopen(col_grp_id, HYPERION_ATTRIBUTE_DT, H5P_DEFAULT));
            hid_t did = hyperion::H5DatatypeManager::datatypes()[
              hyperion::H5DatatypeManager::DATATYPE_H5T];
            CHECK_H5(H5Aread(dt_attr_id, did, &tfld.dt));
            CHECK_H5(H5Aclose(dt_attr_id));
          }
          {
            hid_t fid_attr_id =
              CHECK_H5(
                H5Aopen(col_grp_id, HYPERION_ATTRIBUTE_FID, H5P_DEFAULT));
            hid_t fid_dt = hyperion::H5DatatypeManager::datatypes()[
              hyperion::H5DatatypeManager::FIELD_ID_H5T];
            CHECK_H5(H5Aread(fid_attr_id, fid_dt, &tfld.fid));
            CHECK_H5(H5Aclose(fid_attr_id));
          }
          if (H5Aexists(col_grp_id, column_refcol_attr_name) > 0) {
            hid_t rc_attr_id =
              CHECK_H5(
                H5Aopen(col_grp_id, column_refcol_attr_name, H5P_DEFAULT));
            hid_t rc_dt = H5DatatypeManager::datatype<HYPERION_TYPE_STRING>();
            hyperion::string rc;
            CHECK_H5(H5Aread(rc_attr_id, rc_dt, &rc.val));
            tfld.rc = rc;
          }
#ifdef HYPERION_USE_CASACORE
          if (H5Lexists(col_grp_id, HYPERION_MEASURE_GROUP, H5P_DEFAULT)
              > 0) {
            hid_t measure_id =
              CHECK_H5(
                H5Gopen(col_grp_id, HYPERION_MEASURE_GROUP, H5P_DEFAULT));
            CXX_OPTIONAL_NAMESPACE::optional<IndexTreeL> metadata_tree =
              read_index_tree_binary(measure_id, "metadata_index_tree");
            CXX_OPTIONAL_NAMESPACE::optional<IndexTreeL> value_tree =
              read_index_tree_binary(measure_id, "value_index_tree");
            tfld.mr =
              init_meas_ref(
                args->ctx,
                args->rt,
                measure_id,
                metadata_tree,
                value_tree,
                !tfld.rc);
            CHECK_H5(H5Gclose(measure_id));
          }
#endif
          tfld.kw =
            Keywords::create(args->ctx, args->rt, init_keywords(col_grp_id));
          if (args->cs_fields.count(cs_name) == 0)
            args->cs_fields[cs_name] = {};
          args->cs_fields[cs_name].emplace_back(name, tfld);
        }
      }
      CHECK_H5(H5Gclose(col_grp_id));
    }
  }
  return 0;

}

hyperion::ColumnSpace
hyperion::hdf5::init_columnspace(
  Legion::Context ctx,
  Legion::Runtime* rt,
  hid_t table_grp_id,
  hid_t table_axes_dt,
  const std::string& cs_name) {

  htri_t cs_exists =
    CHECK_H5(H5Lexists(table_grp_id, cs_name.c_str(), H5P_DEFAULT));
  if (cs_exists == 0)
    return ColumnSpace();

  hid_t cs_grp_id =
    CHECK_H5(H5Gopen(table_grp_id, cs_name.c_str(), H5P_DEFAULT));
  std::vector<int> axes;
  {
    htri_t axes_exists =
      CHECK_H5(H5Aexists(cs_grp_id, HYPERION_COLUMN_SPACE_AXES));
    if (axes_exists == 0)
      return ColumnSpace();
    hid_t axes_id =
      CHECK_H5(H5Aopen(cs_grp_id, HYPERION_COLUMN_SPACE_AXES, H5P_DEFAULT));
    hid_t axes_id_ds = CHECK_H5(H5Aget_space(axes_id));
    int ndims = H5Sget_simple_extent_ndims(axes_id_ds);
    if (ndims != 1)
      return ColumnSpace();
    std::vector<unsigned char> ax(H5Sget_simple_extent_npoints(axes_id_ds));
    CHECK_H5(H5Aread(axes_id, table_axes_dt, ax.data()));
    axes.reserve(ax.size());
    std::copy(ax.begin(), ax.end(), std::back_inserter(axes));
    CHECK_H5(H5Aclose(axes_id));
  }
  bool is_index = false;
  {
    htri_t iflg_exists =
      CHECK_H5(H5Aexists(cs_grp_id, HYPERION_COLUMN_SPACE_FLAG));
    if (iflg_exists == 0)
      return ColumnSpace();
    hid_t iflg_id =
      CHECK_H5(H5Aopen(cs_grp_id, HYPERION_COLUMN_SPACE_FLAG, H5P_DEFAULT));
    CHECK_H5(H5Aread(iflg_id, H5T_NATIVE_HBOOL, &is_index));
    CHECK_H5(H5Aclose(iflg_id));
  }
  std::string axes_uid;
  {
    htri_t au_exists =
      CHECK_H5(H5Aexists(cs_grp_id, HYPERION_COLUMN_SPACE_AXES_UID));
    if (au_exists == 0)
      return ColumnSpace();
    hid_t au_id =
      CHECK_H5(
        H5Aopen(cs_grp_id, HYPERION_COLUMN_SPACE_AXES_UID, H5P_DEFAULT));
    hyperion::string au;
    CHECK_H5(
      H5Aread(
        au_id,
        H5DatatypeManager::datatype<HYPERION_TYPE_STRING>(),
        au.val));
    CHECK_H5(H5Aclose(au_id));
    axes_uid = au;
  }
  CXX_OPTIONAL_NAMESPACE::optional<IndexTreeL> ixtree =
    read_index_tree_binary(cs_grp_id, HYPERION_COLUMN_SPACE_INDEX_TREE);
  assert(ixtree);
  auto itrank = ixtree.value().rank();
  if (!itrank || itrank.value() != axes.size())
    return ColumnSpace();
  IndexTreeL it = ixtree.value();
  CHECK_H5(H5Gclose(cs_grp_id));

  return
    ColumnSpace::create(
      ctx,
      rt,
      axes,
      axes_uid,
      tree_index_space(it, ctx, rt),
      is_index);
}

struct acc_cs_t {
  Context ctx;
  Runtime *rt;
  hid_t table_axes_dt;
  std::unordered_map<std::string, hyperion::ColumnSpace> css;
};

static herr_t
acc_cs_fn(
  hid_t group,
  const char* name,
  const H5L_info_t* info,
  void* op_data) {

  if (starts_with(name, HYPERION_COLUMN_SPACE_GROUP_PREFIX)) {
    acc_cs_t* acc = static_cast<acc_cs_t*>(op_data);
    acc->css[name] =
      init_columnspace(acc->ctx, acc->rt, group, acc->table_axes_dt, name);
  }

  return 0;
}

CXX_OPTIONAL_NAMESPACE::optional<
  std::tuple<
    Table::fields_t,
    std::unordered_map<std::string, std::string>>>
hyperion::hdf5::table_fields(
  Context ctx,
  Runtime* rt,
  hid_t loc_id,
  const std::string& table_name) {

  CXX_OPTIONAL_NAMESPACE::optional<
    std::tuple<Table::fields_t, std::unordered_map<std::string, std::string>>>
    result;

  htri_t table_exists =
    CHECK_H5(H5Lexists(loc_id, table_name.c_str(), H5P_DEFAULT));
  if (table_exists > 0)
    using_resource(
      [&]() {
        return CHECK_H5(H5Gopen(loc_id, table_name.c_str(), H5P_DEFAULT));
      },
      [&](hid_t table_grp_id) {
        acc_cs_t acc_cs;
        acc_cs.ctx = ctx;
        acc_cs.rt = rt;
        acc_cs.table_axes_dt =
          CHECK_H5(H5Topen(table_grp_id, table_axes_dt_name, H5P_DEFAULT));
        CHECK_H5(
          H5Literate(
            table_grp_id,
            H5_INDEX_NAME,
            H5_ITER_NATIVE,
            NULL,
            acc_cs_fn,
            &acc_cs));
        CHECK_H5(H5Tclose(acc_cs.table_axes_dt));
        acc_tflds_t acc_tflds;
        acc_tflds.ctx = ctx;
        acc_tflds.rt = rt;
        CHECK_H5(
          H5Literate(
            table_grp_id,
            H5_INDEX_NAME,
            H5_ITER_NATIVE,
            NULL,
            acc_tflds_fn,
            &acc_tflds));
        decltype(result)::value_type fields_paths;
#if HAVE_CXX17
        auto& [fields, paths] = fields_paths;
#else // !HAVE_CXX17
        auto& fields = std::get<0>(fields_paths);
        auto& paths = std::get<1>(fields_paths);
#endif // HAVE_CXX17
        for (auto& nm_tflds : acc_tflds.cs_fields) {
          auto& nm = std::get<0>(nm_tflds);
          auto& tflds = std::get<1>(nm_tflds);
          assert(acc_cs.css.count(nm) > 0);
          fields.emplace_back(acc_cs.css[nm], tflds);
        }
        // FIXME: awaiting keywords support in Table: auto kws =
        // init_keywords(table_grp_id);
        for (auto& cs_nm_tflds : fields)
          for (auto& nm_tfld : std::get<1>(cs_nm_tflds)) {
            auto& nm = std::get<0>(nm_tfld);
            paths[nm] = table_name + "/" + nm + "/" + HYPERION_COLUMN_DS;
          }
        result = fields_paths;
      },
      [](hid_t table_grp_id) {
        CHECK_H5(H5Gclose(table_grp_id));
      });
  return result;
}

std::tuple<hyperion::Table, std::unordered_map<std::string, std::string>>
hyperion::hdf5::init_table(
  Context ctx,
  Runtime* rt,
  hid_t loc_id,
  const std::string& table_name) {

  std::tuple<
    hyperion::Table,
    std::unordered_map<std::string, std::string>> result;

  htri_t table_exists =
    CHECK_H5(H5Lexists(loc_id, table_name.c_str(), H5P_DEFAULT));
  if (table_exists > 0)
    using_resource(
      [&]() {
        return CHECK_H5(H5Gopen(loc_id, table_name.c_str(), H5P_DEFAULT));
      },
      [&](hid_t table_grp_id) {
        hid_t table_axes_dt =
          CHECK_H5(H5Topen(table_grp_id, table_axes_dt_name, H5P_DEFAULT));
        ColumnSpace index_col_cs =
          init_columnspace(
            ctx,
            rt,
            table_grp_id,
            table_axes_dt,
            HYPERION_INDEX_COLUMN_SPACE_GROUP);
        acc_cs_t acc_cs;
        acc_cs.ctx = ctx;
        acc_cs.rt = rt;
        acc_cs.table_axes_dt = table_axes_dt;
        CHECK_H5(
          H5Literate(
            table_grp_id,
            H5_INDEX_NAME,
            H5_ITER_NATIVE,
            NULL,
            acc_cs_fn,
            &acc_cs));
        CHECK_H5(H5Tclose(table_axes_dt));
        acc_tflds_t acc_tflds;
        acc_tflds.ctx = ctx;
        acc_tflds.rt = rt;
        CHECK_H5(
          H5Literate(
            table_grp_id,
            H5_INDEX_NAME,
            H5_ITER_NATIVE,
            NULL,
            acc_tflds_fn,
            &acc_tflds));
        std::vector<
          std::tuple<
            ColumnSpace,
            std::vector<std::pair<std::string, TableField>>>> cflds;
        for (auto& nm_tflds : acc_tflds.cs_fields) {
          auto& nm = std::get<0>(nm_tflds);
          auto& tflds = std::get<1>(nm_tflds);
          assert(acc_cs.css.count(nm) > 0);
          cflds.emplace_back(acc_cs.css[nm], tflds);
        }
        // FIXME: awaiting keywords support in Table: auto kws =
        // init_keywords(table_grp_id);
#if HAVE_CXX17
        auto& [tb, paths] = result;
#else // !HAVE_CXX17
        auto& tb = std::get<0>(result);
        auto& paths = std::get<1>(result);
#endif // HAVE_CXX17
        for (auto& cs_nm_tflds : cflds)
          for (auto& nm_tfld : std::get<1>(cs_nm_tflds)) {
            auto& nm = std::get<0>(nm_tfld);
            paths[nm] = table_name + "/" + nm + "/" + HYPERION_COLUMN_DS;
          }
        tb = Table::create(ctx, rt, std::move(index_col_cs), std::move(cflds));
      },
      [](hid_t table_grp_id) {
        CHECK_H5(H5Gclose(table_grp_id));
      });
  return result;
}

struct acc_all_tflds_t {
  Context ctx;
  Runtime *rt;
  std::unordered_map<
    std::string,
    std::tuple<
      Table::fields_t,
      std::unordered_map<std::string, std::string>>> acc;
};

static herr_t
acc_all_tflds_fn(
  hid_t group,
  const char* name,
  const H5L_info_t* info,
  void* op_data) {

  acc_all_tflds_t* args = static_cast<acc_all_tflds_t*>(op_data);

  H5O_info_t infobuf;
  CHECK_H5(H5Oget_info_by_name(group, name, &infobuf, H5P_DEFAULT));
  if (infobuf.type == H5O_TYPE_GROUP) {
    hid_t tbl_grp_id = CHECK_H5(H5Gopen(group, name, H5P_DEFAULT));
    auto tfp =
      hyperion::hdf5::table_fields(args->ctx, args->rt, tbl_grp_id, name);
    if (tfp)
      args->acc.emplace(name, tfp.value());
    CHECK_H5(H5Gclose(tbl_grp_id));
  }
  return 0;
}

std::unordered_map<
  std::string,
    std::tuple<
      Table::fields_t,
      std::unordered_map<std::string, std::string>>>
hyperion::hdf5::all_table_fields(Context ctx, Runtime* rt, hid_t loc_id) {

  acc_all_tflds_t acc_all_tflds;
  acc_all_tflds.ctx = ctx;
  acc_all_tflds.rt = rt;
  CHECK_H5(
    H5Literate(
      loc_id,
      H5_INDEX_NAME,
      H5_ITER_NATIVE,
      NULL,
      acc_all_tflds_fn,
      &acc_all_tflds));
  return acc_all_tflds.acc;
}

std::unordered_map<std::string, std::string>
hyperion::hdf5::get_table_column_paths(
  hid_t file_id,
  const std::string& table_path,
  const std::unordered_set<std::string>& columns) {

  std::unordered_map<std::string, std::string> result;
  htri_t table_exists =
    CHECK_H5(H5Lexists(file_id, table_path.c_str(), H5P_DEFAULT));
  if (table_exists > 0) {
    for (auto& col : columns) {
      auto col_path = table_path + "/" + col;
      htri_t col_exists =
        CHECK_H5(H5Lexists(file_id, col_path.c_str(), H5P_DEFAULT));
      if (col_exists > 0) {
        auto col_ds_path = col_path + "/" + HYPERION_COLUMN_DS;
        htri_t col_ds_exists =
          CHECK_H5(H5Lexists(file_id, col_ds_path.c_str(), H5P_DEFAULT));
        if (col_ds_exists > 0)
          result[col] = col_ds_path;
      }
    }
  }
  return result;
}

std::unordered_map<std::string, std::string>
hyperion::hdf5::get_table_column_paths(
  const CXX_FILESYSTEM_NAMESPACE::path& file_path,
  const std::string& table_path,
  const std::unordered_set<std::string>& columns) {

  std::unordered_map<std::string, std::string> result;
  hid_t file_id = H5Fopen(file_path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
  if (file_id >= 0) {
    result = get_table_column_paths(file_id, table_path, columns);
    CHECK_H5(H5Fclose(file_id));
  }
  return result;
}

PhysicalRegion
hyperion::hdf5::attach_keywords(
  Context ctx,
  Runtime* rt,
  const CXX_FILESYSTEM_NAMESPACE::path& file_path,
  const std::string& keywords_path,
  const Keywords& keywords,
  bool read_only) {

  assert(!keywords.is_empty());
  auto kws = keywords.values_lr;
  std::vector<std::string> keys = keywords.keys(rt);
  std::vector<std::string> field_paths(keys.size());
  std::map<FieldID, const char*> fields;
  for (size_t i = 0; i < keys.size(); ++i) {
    field_paths[i] = keywords_path + "/" + keys[i];
    fields[i] = field_paths[i].c_str();
  }
  AttachLauncher kws_attach(EXTERNAL_HDF5_FILE, kws, kws);
  kws_attach.attach_hdf5(
    file_path.c_str(),
    fields,
    read_only ? LEGION_FILE_READ_ONLY : LEGION_FILE_READ_WRITE);
  return rt->attach_external_resource(ctx, kws_attach);
}

CXX_OPTIONAL_NAMESPACE::optional<PhysicalRegion>
hyperion::hdf5::attach_table_columns(
  Context ctx,
  Runtime* rt,
  const CXX_FILESYSTEM_NAMESPACE::path& file_path,
  const std::string& root_path,
  const Table& table,
  const std::unordered_set<std::string>& columns,
  const std::unordered_map<std::string, std::string>& column_paths,
  bool read_only,
  bool mapped) {

  auto table_columns = table.columns();
  CXX_OPTIONAL_NAMESPACE::optional<ColumnSpace> cs;
  LogicalRegion lr;
  std::map<FieldID, std::string> paths;
  for (auto& nm : columns) {
    if (table_columns.count(nm) > 0) {
      if (column_paths.count(nm) > 0) {
        auto &c = table_columns.at(nm);
        if (!cs) {
          cs = c.cs;
          lr = c.region;
        } else if (cs.value() != c.cs) {
          // FIXME: warning message: multiple ColumnSpaces in column selection
          // of call to attach_table_columns()
          return CXX_OPTIONAL_NAMESPACE::nullopt;
        }
        paths[c.fid] = root_path + column_paths.at(nm);
      } else {
        // FIXME: warning: selected column without a provided path
        return CXX_OPTIONAL_NAMESPACE::nullopt;
      }
    }
  }
  if (paths.size() == 0)
    return CXX_OPTIONAL_NAMESPACE::nullopt;
  AttachLauncher attach(EXTERNAL_HDF5_FILE, lr, lr, true, mapped);
  std::map<FieldID, const char*> field_map;
  for (auto& fid_p : paths)
    field_map[std::get<0>(fid_p)] = std::get<1>(fid_p).c_str();
  attach.attach_hdf5(
    file_path.c_str(),
    field_map,
    read_only ? LEGION_FILE_READ_ONLY : LEGION_FILE_READ_WRITE);
  return rt->attach_external_resource(ctx, attach);
}

template <typename F>
std::map<PhysicalRegion, std::unordered_map<std::string, Column>>
attach_selected_table_columns(
  Context ctx,
  Runtime* rt,
  const CXX_FILESYSTEM_NAMESPACE::path& file_path,
  const std::string& root_path,
  const Table& table,
  F select,
  const std::unordered_map<std::string, std::string>& column_paths,
  bool read_only,
  bool mapped) {

  std::map<PhysicalRegion, std::unordered_map<std::string, Column>> result;
  auto tbl_columns = table.columns();
  for (auto& nm_c : tbl_columns) {
#if HAVE_CXX17
    auto& [nm, c] = nm_c;
#else // !HAVE_CXX17
    auto& nm = std::get<0>(nm_c);
    auto& c = std::get<1>(nm_c);
#endif // HAVE_CXX17
    std::unordered_set<std::string> colnames;
    std::unordered_map<std::string, Column> cols;
    if (select(nm)) {
      colnames.insert(nm);
      cols[nm] = c;
    }
    auto opr =
      attach_table_columns(
        ctx,
        rt,
        file_path,
        root_path,
        table,
        colnames,
        column_paths,
        mapped,
        read_only);
    if (opr)
      result[opr.value()] = cols;
  }
  return result;
}

std::map<PhysicalRegion, std::unordered_map<std::string, Column>>
hyperion::hdf5::attach_all_table_columns(
  Context ctx,
  Runtime* rt,
  const CXX_FILESYSTEM_NAMESPACE::path& file_path,
  const std::string& root_path,
  const Table& table,
  const std::unordered_set<std::string>& exclude,
  const std::unordered_map<std::string, std::string>& column_paths,
  bool read_only,
  bool mapped) {

  return
    attach_selected_table_columns(
      ctx,
      rt,
      file_path,
      root_path,
      table,
      [&exclude](const std::string& nm) { return exclude.count(nm) == 0; },
      column_paths,
      read_only,
      mapped);
}

std::map<PhysicalRegion, std::unordered_map<std::string, Column>>
hyperion::hdf5::attach_some_table_columns(
  Context ctx,
  Runtime* rt,
  const CXX_FILESYSTEM_NAMESPACE::path& file_path,
  const std::string& root_path,
  const Table& table,
  const std::unordered_set<std::string>& include,
  const std::unordered_map<std::string, std::string>& column_paths,
  bool read_only,
  bool mapped) {

  return
    attach_selected_table_columns(
      ctx,
      rt,
      file_path,
      root_path,
      table,
      [&include](const std::string& nm) { return include.count(nm) > 0; },
      column_paths,
      read_only,
      mapped);
}
// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End:
