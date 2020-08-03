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
#include <hyperion/synthesis/PSTermTable.h>
#include <hyperion/synthesis/WTermTable.h>
#include <hyperion/synthesis/ATermTable.h>
#include <hyperion/synthesis/CFPhysicalTable.h>

using namespace hyperion::synthesis;
using namespace hyperion;
using namespace Legion;

namespace cc = casacore;

enum {
  CFCOMPUTE_TASK_ID,
};

void
cfcompute_task(
  const Task*,
  const std::vector<PhysicalRegion>&,
  Context ctx,
  Runtime* rt) {

  const size_t grid_size = 4;

  LinearCoordinateTable lc(ctx, rt, grid_size, {0.0});
  lc.compute_world_coordinates(ctx, rt, cc::LinearCoordinate(2), 1.0);

  PSTermTable ps(ctx, rt, grid_size, {0.16, 0.08});
  ps.compute_cfs(ctx, rt, lc);

  auto colreqs = Column::default_requirements;
  colreqs.values.mapped = true;
  colreqs.values.privilege = LEGION_READ_ONLY;

  auto pps =
    CFPhysicalTable<CF_PS_SCALE>(
      ps.map_inline(
        ctx,
        rt,
        {{cf_table_axis<CF_PS_SCALE>::name, colreqs},
         {CFTableBase::CF_VALUE_COLUMN_NAME, colreqs},
         {CFTableBase::CF_WEIGHT_COLUMN_NAME, colreqs}},
        CXX_OPTIONAL_NAMESPACE::nullopt));
  {
    auto ps_scales =
      pps.ps_scale<AffineAccessor>().accessor<LEGION_READ_ONLY>();
    auto values = pps.value<AffineAccessor>().accessor<LEGION_READ_ONLY>();
    for (size_t ps = 0; ps < 2; ++ps) {
      std::cout << "ps_scale " << ps_scales[ps] << std::endl;
      for (size_t x = 0; x < grid_size; ++x) {
        for (size_t y = 0; y < grid_size; ++y)
          std::cout << values[{ps, x, y}] << " ";
        std::cout << std::endl;
      }
    }
  }

  // WTermTable w(ctx, rt, cf_size, {2.2, 22.2, 222.2});
  // w.compute_cfs(ctx, rt, FIXME_CELL_SIZE);

  // auto pw =
  //   CFPhysicalTable<CF_W>(
  //     w.map_inline(
  //       ctx,
  //       rt,
  //       {{cf_table_axis<CF_W>::name, colreqs},
  //        {CFTableBase::CF_VALUE_COLUMN_NAME, colreqs},
  //        {CFTableBase::CF_WEIGHT_COLUMN_NAME, colreqs}},
  //       CXX_OPTIONAL_NAMESPACE::nullopt));
  // {
  //   auto ws = pw.w<AffineAccessor>().accessor<LEGION_READ_ONLY>();
  //   auto values = pw.value<AffineAccessor>().accessor<LEGION_READ_ONLY>();
  //   for (coord_t w = 0; w < 3; ++w) {
  //     std::cout << "w " << ws[w] << std::endl;
  //     for (coord_t x = 0; x < cf_size[0]; ++x) {
  //       for (coord_t y = 0; y < cf_size[1]; ++y)
  //         std::cout << values[{w, x, y}] << " ";
  //       std::cout << std::endl;
  //     }
  //   }
  // }

  // auto pa = index_axis<CF_PS_SCALE>(pps, pw);
  // for (auto& x : pa.values)
  //   std::cout << x << " ";
  // std::cout << std::endl;
  // auto wa = index_axis<CF_W>(pps, pw);
  // for (auto& x : wa.values)
  //   std::cout << x << " ";
  // std::cout << std::endl;
  pps.unmap_regions(ctx, rt);
  ps.destroy(ctx, rt);
  // pw.unmap_regions(ctx, rt);
  // w.destroy(ctx, rt);
}

int
main(int argc, char* argv[]) {

  hyperion::preregister_all();
  {
    TaskVariantRegistrar registrar(CFCOMPUTE_TASK_ID, "cfcompute_task");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<cfcompute_task>(
      registrar,
      "cfcompute_task");
    Runtime::set_top_level_task_id(CFCOMPUTE_TASK_ID);
  }
  synthesis::CFTableBase::preregister_all();
  return Runtime::start(argc, argv);
}

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End:
