#define GENERATORS(t)                                                   \
  template <> inline                                                    \
  auto                                                                  \
  ScalarColumnBuilder::generator<casacore::t>(                          \
    const std::string& name,                                            \
    std::optional<Legion::FieldID> fid) {                               \
                                                                        \
    return                                                              \
      [=](const IndexTreeL& row_index_shape) {                          \
        return std::make_unique<ScalarColumnBuilder>(                   \
          name,                                                         \
          ValueType<casacore::t>::DataType,                             \
          row_index_shape,                                              \
          fid);                                                         \
      };                                                                \
  }                                                                     \
  template <> inline                                                    \
  auto                                                                  \
  ScalarColumnBuilder::generator<std::vector<casacore::t>>(             \
    const std::string& name,                                            \
    std::optional<Legion::FieldID> fid) {                               \
                                                                        \
    return                                                              \
      [=](const IndexTreeL& row_index_shape) {                          \
        return std::make_unique<ScalarColumnBuilder>(                   \
          name,                                                         \
          ValueType<std::vector<casacore::t>>::DataType,                \
          row_index_shape,                                              \
          fid);                                                         \
      };                                                                \
  }                                                                     \
  template <> template<> inline                                         \
  auto                                                                  \
  ArrayColumnBuilder<1>::generator<casacore::t>(                        \
    const std::string& name,                                            \
    std::function<std::array<size_t, 1>(const std::any&)> row_dimensions, \
    std::optional<Legion::FieldID> fid) {                               \
                                                                        \
    return                                                              \
      [=](const IndexTreeL& row_index_shape) {                          \
        return std::make_unique<ArrayColumnBuilder<1>>(                 \
          name,                                                         \
          ValueType<casacore::t>::DataType,                             \
          row_index_shape,                                              \
          row_dimensions,                                               \
          fid);                                                         \
      };                                                                \
  }\
  template <> template<> inline                                         \
  auto                                                                  \
  ArrayColumnBuilder<2>::generator<casacore::t>(                        \
    const std::string& name,                                            \
    std::function<std::array<size_t, 2>(const std::any&)> row_dimensions, \
    std::optional<Legion::FieldID> fid) {                               \
                                                                        \
    return                                                              \
      [=](const IndexTreeL& row_index_shape) {                          \
        return std::make_unique<ArrayColumnBuilder<2>>(                 \
          name,                                                         \
          ValueType<casacore::t>::DataType,                             \
          row_index_shape,                                              \
          row_dimensions,                                               \
          fid);                                                         \
      };                                                                \
  }\
  template <> template<> inline                                         \
  auto                                                                  \
  ArrayColumnBuilder<3>::generator<casacore::t>(                        \
    const std::string& name,                                            \
    std::function<std::array<size_t, 3>(const std::any&)> row_dimensions, \
    std::optional<Legion::FieldID> fid) {                               \
                                                                        \
    return                                                              \
      [=](const IndexTreeL& row_index_shape) {                          \
        return std::make_unique<ArrayColumnBuilder<3>>(                 \
          name,                                                         \
          ValueType<casacore::t>::DataType,                             \
          row_index_shape,                                              \
          row_dimensions,                                               \
          fid);                                                         \
      };                                                                \
  }


GENERATORS(Bool)
GENERATORS(Char)
GENERATORS(uChar)
GENERATORS(Short)
GENERATORS(uShort)
GENERATORS(Int)
GENERATORS(uInt)
GENERATORS(Float)
GENERATORS(Double)
GENERATORS(Complex)
GENERATORS(DComplex)
GENERATORS(String)

#undef GENERATORS

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End:
