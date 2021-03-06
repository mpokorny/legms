find_package(Filesystem COMPONENTS Experimental)

enable_language(CXX)

set(hyperion_CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "hyperion build type")
set(KOKKOS_CXX_COMPILER "" CACHE STRING "")
option(hyperion_USE_CASACORE "Use casacore in hyperion" ${USE_CASACORE})
option(hyperion_USE_HDF5 "Use HDF5 in hyperion" ${USE_HDF5})
option(hyperion_USE_CUDA "Use CUDA in hyperion" ${USE_CUDA})
option(hyperion_USE_OPENMP "Use OpenMP in hyperion" ${USE_OPENMP})
option(hyperion_USE_KOKKOS "Use Kokkos in hyperion" ${USE_KOKKOS})
option(hyperion_USE_KOKKOS_KERNELS "Use Kokkos kernels in hyperion" ${hyperion_USE_KOKKOS})
option(hyperion_USE_YAML "Use YAML in hyperion" ON)

set(hyperion_MAX_STRING_SIZE 64 CACHE STRING "Maximum string size")
set(hyperion_MAX_NUM_TABLE_COLUMNS 100 CACHE STRING "Maximum number of table columns (> 50)")
set(hyperion_CXX_STANDARD 14 CACHE STRING "Compile with required C++ standard")
set_property(CACHE hyperion_CXX_STANDARD PROPERTY STRINGS 14 17)

if(hyperion_USE_OPENMP)
  set(HYPERION_FFTW_COMPONENTS
    FLOAT_OPENMP_LIB FLOAT_LIB DOUBLE_OPENMP_LIB DOUBLE_LIB)
else()
  set(HYPERION_FFTW_COMPONENTS FLOAT_LIB DOUBLE_LIB)
endif()
find_package(FFTW REQUIRED COMPONENTS ${HYPERION_FFTW_COMPONENTS})

if(hyperion_USE_KOKKOS)
  set(CMAKE_CXX_COMPILER ${KOKKOS_CXX_COMPILER})
  if(hyperion_USE_KOKKOS_KERNELS)
    find_package(KokkosKernels REQUIRED)
  endif()
endif()

if(hyperion_USE_YAML)
  find_package(yaml-cpp REQUIRED)
endif()

if(NOT USE_KOKKOS AND hyperion_USE_CUDA)
  enable_language(CUDA)
endif()

include(CheckCXXCompilerFlag)

set(CMAKE_BUILD_TYPE ${hyperion_CMAKE_BUILD_TYPE})

if(NOT USE_KOKKOS)
  check_cxx_compiler_flag("-march=${BUILD_ARCH}" COMPILER_SUPPORTS_MARCH)
  if(COMPILER_SUPPORTS_MARCH)
    add_compile_options("-march=${BUILD_ARCH}")
  else()
    message(STATUS "The flag -march=${BUILD_ARCH} is not supported by the compiler")
  endif()

  check_cxx_compiler_flag("-mtune=${BUILD_ARCH}" COMPILER_SUPPORTS_MTUNE)
  if(COMPILER_SUPPORTS_MTUNE)
    add_compile_options("-mtune=${BUILD_ARCH}")
  else()
    message(STATUS "The flag -mtune=${BUILD_ARCH} is not supported by the compiler")
  endif()
endif()

# Legion can't handle certain optimizations. See:
# https://github.com/StanfordLegion/legion/issues/168#issuecomment-244582958
check_cxx_compiler_flag("-fno-strict-aliasing" COMPILER_SUPPORTS_NO_STRICT_ALIASING)
if(COMPILER_SUPPORTS_NO_STRICT_ALIASING)
  add_compile_options("-fno-strict-aliasing")
endif()

#------------------------------------------------------------------------------#
# hyperion library
#------------------------------------------------------------------------------#

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)
add_compile_options(-Wall)
add_compile_definitions($<$<CONFIG:Debug>:HYPERION_DEBUG>)

add_compile_options($<$<CONFIG:Debug>:-O0>)
# check_cxx_compiler_flag("-Og" COMPILER_SUPPORTS_OPTIMIZE_DEBUG)
# if(COMPILER_SUPPORTS_OPTIMIZE_DEBUG)
#   add_compile_options($<$<CONFIG:Debug>:-Og>)
# else()
#   check_cxx_compiler_flag("-O0" COMPILER_SUPPORTS_OPTIMIZE_ZERO)
#   if(COMPILER_SUPPORTS_OPTIMIZE_ZERO)
#     add_compile_options($<$<CONFIG:Debug>:-O0>)
#   endif()
# endif()

set(MAX_STRING_SIZE ${hyperion_MAX_STRING_SIZE})
set(MAX_NUM_TABLE_COLUMNS ${hyperion_MAX_NUM_TABLE_COLUMNS})

# FIXME: this doesn't work after setting CMAKE_CXX_COMPILER to
# KOKKOS_CXX_COMPILER (gets a false positive on the c++17 test when
# CMAKE_CXX_COMPILER supports it, but KOKKOS_CXX_COMPILER does not)
#
# if(hyperion_CXX_STANDARD STREQUAL 17)
#   set(HAVE_CXX17 $<COMPILE_FEATURES:cxx_std_17>)
#   if(NOT HAVE_CXX17)
#     message(FATAL_ERROR "Compiler does not support c++17")
#   endif()
#   set(CXX_STANDARD 17)
# else()
#   set(HAVE_CXX14 $<COMPILE_FEATURES:cxx_std_14>)
#   if(NOT HAVE_CXX14)
#     message(FATAL_ERROR "Compiler does not support c++14")
#   endif()
#   set(CXX_STANDARD 14)
# endif()
#
# FIXME: after fixing the above, remove the following
set(CXX_STANDARD ${hyperion_CXX_STANDARD})

if (CXX_STANDARD STREQUAL 17)
  set(HAVE_CXX17 True)
else()
  set(HAVE_CXX14 True)
endif()

if(CXX_STANDARD STREQUAL 17)
  set(CXX_ANY_HEADER any)
  set(CXX_ANY_NAMESPACE std)

  set(CXX_OPTIONAL_HEADER optional)
  set(CXX_OPTIONAL_NAMESPACE std)
else()
  set(CXX_ANY_HEADER experimental/any)
  set(CXX_ANY_NAMESPACE std::experimental)

  set(CXX_OPTIONAL_HEADER experimental/optional)
  set(CXX_OPTIONAL_NAMESPACE std::experimental)
endif()

set(HYPERION_USE_CASACORE ${hyperion_USE_CASACORE})
set(HYPERION_USE_OPENMP ${hyperion_USE_OPENMP})
set(HYPERION_USE_CUDA ${hyperion_USE_CUDA})
set(HYPERION_USE_HDF5 ${hyperion_USE_HDF5})
set(HYPERION_USE_KOKKOS ${hyperion_USE_KOKKOS})
set(HYPERION_USE_KOKKOS_KERNELS ${hyperion_USE_KOKKOS_KERNELS})
configure_file(hyperion_config.h.in hyperion_config.h)

if(hyperion_USE_HDF5)
  add_compile_definitions(H5_USE_110_API)
endif()

function(set_host_target_properties target)
  set_target_properties(${target} PROPERTIES
    CXX_STANDARD ${CXX_STANDARD}
    CXX_EXTENSIONS ON
    CXX_STANDARD_REQUIRED ON)
endfunction()

add_library(hyperion)
target_sources(hyperion PRIVATE
  hyperion_c.h
  # Grids_c.cc
  # Grids_c.h
  Grids.h
  IndexTree.h
  tree_index_space.cc
  tree_index_space.h
  utility.cc
  utility.h
  utility_c.cc
  utility_c.h
  Column.h
  Column.cc
  # Column_c.cc
  # Column_c.h
  ColumnSpace.h
  ColumnSpace.cc
  ColumnSpacePartition.h
  ColumnSpacePartition.cc
  c_util.h
  Keywords.h
  Keywords.cc
  MSTable.h
  MSTable.cc
  MSTable_c.cc
  MSTable_c.h
  MSTableColumns_c.h
  MSTableColumns.h
  MSTableColumns.cc
  MSAntennaTable.h
  MSDataDescriptionTable.h
  MSFieldTable.h
  MSMainTable.h
  MSSpWindowTable.h
  PhysicalColumn.h
  PhysicalColumn.cc
  PhysicalTable.h
  PhysicalTable.cc
  PhysicalTableGuard.h
  Table.h
  Table.cc
  # Table_c.cc
  # Table_c.h
  TableField.h
  TableField.cc
  TableMapper.h
  TableMapper.cc
  KeywordsBuilder.h)

if(hyperion_USE_CASACORE)
  target_sources(hyperion PRIVATE
    ColumnBuilder.h
    TableBuilder.h
    TableBuilder.cc
    TableReadTask.cc
    TableReadTask.h
    # TableReadTask_c.cc
    # TableReadTask_c.h
    Measures.h
    Measures.cc
    MeasRef.h
    MeasRef.cc
    MSFieldTable.cc
    NullIO.cc)
endif()
if(hyperion_USE_HDF5)
  target_sources(hyperion PRIVATE
    hdf5.h
    hdf5.cc)
endif()

set_target_properties(hyperion PROPERTIES
  VERSION 0
  SOVERSION 0.1.0)

set_host_target_properties(hyperion)
target_include_directories(hyperion PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
if (hyperion_USE_CASACORE)
  if (BUILD_CASACORE)
    add_dependencies(hyperion casacore_build)
    target_include_directories(hyperion
      PUBLIC $<BUILD_INTERFACE:${CASACORE_INCLUDE_DIR}>)
  else()
    target_include_directories(hyperion
      PUBLIC ${CASACORE_INCLUDE_DIR})
  endif ()
  target_link_libraries(hyperion PUBLIC ${CASACORE_LIBRARIES})
endif()
if(hyperion_USE_HDF5)
  target_include_directories(hyperion PUBLIC ${HDF5_INCLUDE_DIRS})
  target_link_libraries(hyperion PUBLIC ${HDF5_LIBRARIES})
endif()
target_link_libraries(hyperion PUBLIC std::filesystem)
target_link_libraries(hyperion PUBLIC std::mdspan)
if(hyperion_USE_OPENMP)
  target_link_libraries(hyperion PUBLIC FFTW::FloatOpenMP)
  target_link_libraries(hyperion PUBLIC FFTW::DoubleOpenMP)
endif()
target_link_libraries(hyperion PUBLIC FFTW::Float)
target_link_libraries(hyperion PUBLIC FFTW::Double)
if(hyperion_USE_CUDA)
  target_link_libraries(hyperion PUBLIC cufft)
endif()
if(hyperion_USE_KOKKOS)
  if(hyperion_USE_CUDA)
    target_compile_options(hyperion PUBLIC
      --expt-relaxed-constexpr)
  endif()
  target_link_libraries(hyperion PUBLIC Kokkos::kokkoscore)
  if(hyperion_USE_KOKKOS_KERNELS)
    target_link_libraries(hyperion PUBLIC Kokkos::kokkoskernels)
  endif()
endif()
if(HAVE_CXX17)
  target_compile_definitions(hyperion PUBLIC HAVE_CXX17)
endif()
target_link_libraries(hyperion PUBLIC LegionRuntime)

include(GenerateExportHeader)
generate_export_header(hyperion)

install(TARGETS hyperion EXPORT HyperionConfig)
install(FILES ${HYPERION_INCLUDE}
  DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}/hyperion)
install(EXPORT HyperionConfig DESTINATION share/hyperion/cmake)
export(TARGETS hyperion FILE HyperionConfig.cmake)

if (hyperion_USE_HDF5 AND hyperion_USE_CASACORE)
  add_executable(ms2h5 ms2h5.cc)
  set_host_target_properties(ms2h5)
  target_link_libraries(ms2h5 hyperion)
  install(TARGETS ms2h5)
endif()

if (hyperion_USE_CASACORE)
  add_subdirectory(synthesis)
endif()
add_subdirectory(cfcompute)
# add_subdirectory(gridder)

add_subdirectory(testing)
add_subdirectory(tests)
