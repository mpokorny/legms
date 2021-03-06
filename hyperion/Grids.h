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
#ifndef HYPERION_GRIDS_H_
#define HYPERION_GRIDS_H_

#include <tuple>

#include <hyperion/hyperion.h>

namespace hyperion {

constexpr void
bb(
  long long* blks,
  long long rect_lo,
  long long rect_hi,
  long long num_blocks) {
  // ceil(rect_hi - rect_lo + 1, num_blocks)
  *blks = (rect_hi - rect_lo + num_blocks) / num_blocks;
}

template <typename... Args>
constexpr void
bb(
  long long* blks,
  long long rect_lo,
  long long rect_hi,
  long long num_blocks,
  Args... args) {

  bb(blks, rect_lo, rect_hi, num_blocks);
  bb(blks + 1, args...);
}

template <int D>
Legion::Point<D>
blockify(
  const Legion::Rect<D>& grid,
  const Legion::Point<D>& num_blocks) {
}

template <>
Legion::Point<2>
blockify<2>(
  const Legion::Rect<2>& grid,
  const Legion::Point<2>& num_blocks) {

  long long blks[2]{0, 0};
  bb(
    blks,
    grid.lo.x, grid.hi.x, num_blocks.x,
    grid.lo.y, grid.hi.y, num_blocks.y);
  return Legion::Point<2>(blks);
}

template <>
Legion::Point<3>
blockify<3>(
  const Legion::Rect<3>& grid,
  const Legion::Point<3>& num_blocks) {

  long long blks[3]{0, 0, 0};
  bb(
    blks,
    grid.lo.x, grid.hi.x, num_blocks.x,
    grid.lo.y, grid.hi.y, num_blocks.y,
    grid.lo.z, grid.hi.z, num_blocks.z);
  return Legion::Point<3>(blks);
}

// block_partition_and_overlap
//
// partition a grid twice, once into disjoint blocks, and once by extending the
// disjoint blocks with borders
//
// the caller is required to clean up the implicit color space defined by the
// disjoint block partition (runtime->destroy_index_space(ctx,
// runtime->get_index_partition_color_space_name(ctx, disjoint_block_partition))
// TODO: is this recipe correct?)
template <int D>
std::tuple<Legion::IndexPartition, Legion::IndexPartition>
block_partition_and_extend(
  const Legion::IndexSpaceT<D>& grid_is,
  const Legion::Point<D>& num_blocks,
  const Legion::Point<D>& border,
  Legion::Context ctx,
  Legion::Runtime* runtime) {

  Legion::Rect<D> grid = runtime->get_index_space_domain(ctx, grid_is);
  const Legion::Point<D> block(blockify(grid, num_blocks));
  Legion::IndexPartition disjoint_ip =
    runtime->create_partition_by_blockify(ctx, grid_is, block);
  Legion::IndexSpace color_space =
    runtime->get_index_partition_color_space_name(ctx, disjoint_ip);
  Legion::Transform<D,D> transform;
  for (int i = 0; i < D; ++i)
    for (int j = 0; j < D; ++j)
      if (i == j)
        transform[i][j] = block[i];
      else
        transform[i][j] = 0;
  Legion::Point<D> zeros;
  for (int i = 0; i < D; i++)
    zeros[i] = 0;
  Legion::Point<D> ones;
  for (int i = 0; i < D; i++)
    ones[i] = 1;
  const Legion::Rect<D> extent(zeros - border, block + border - ones);
  Legion::IndexPartition extended_ip =
    runtime->create_partition_by_restriction(
      ctx,
      grid_is,
      color_space,
      transform,
      extent);

  return std::make_tuple(disjoint_ip, extended_ip);
};

template <int D>
std::tuple<Legion::IndexPartition, Legion::IndexPartition>
block_and_halo_partitions(
  Legion::Context ctx,
  Legion::Runtime* runtime,
  const Legion::IndexSpaceT<D>& grid,
  const Legion::Point<D>& block_size,
  const Legion::Point<D>& border) {

  Legion::IndexPartition disjoint_ip =
    runtime->create_partition_by_blockify(ctx, grid, block_size);
  Legion::IndexSpace color_space =
    runtime->get_index_partition_color_space_name(ctx, disjoint_ip);
  Legion::Transform<D,D> transform;
  for (int i = 0; i < D; ++i)
    for (int j = 0; j < D; ++j)
      if (i == j)
        transform[i][j] = block_size[i];
      else
        transform[i][j] = 0;
  const Legion::Rect<D> extent(
    Legion::Point<D>::ZEROES() - border,
    block_size + border - Legion::Point<D>::ONES());
  Legion::IndexPartition halo_ip =
    runtime->create_partition_by_restriction(
      ctx,
      grid,
      color_space,
      transform,
      extent);

  return std::make_tuple(disjoint_ip, halo_ip);
};

} // end namespace hyperion

#endif // HYPERION_GRIDS_H_

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End:
