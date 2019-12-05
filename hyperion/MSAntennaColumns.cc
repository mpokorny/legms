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
#include <hyperion/MSAntennaColumns.h>
#include <hyperion/MSTableColumns.h>
#include <hyperion/MeasRefContainer.h>
#include <hyperion/MeasRefDict.h>

using namespace hyperion;
using namespace Legion;

MSAntennaColumns::MSAntennaColumns(
  Context ctx,
  Runtime* rt,
  const RegionRequirement& rows_requirement,
  const std::unordered_map<std::string, std::vector<Legion::PhysicalRegion>>&
  regions)
  : m_rows_requirement(rows_requirement) {

  for (auto& [nm, prs] : regions) {
    auto col = C::lookup_col(nm);
    if (col) {
      m_regions[col.value()] = prs[0];
    }
#ifdef HYPERION_USE_CASACORE
    else if (nm == "POSITION_MEAS_REF") {
      if (prs.size() > 0)
        m_position_mr =
          MeasRefDict::get<M_POSITION>(
            MeasRefContainer::make_dict(ctx, rt, prs.begin(), prs.end())
            .get("Position").value());
    } else if (nm == "OFFSET_MEAS_REF") {
      if (prs.size() > 0)
        m_offset_mr =
          MeasRefDict::get<M_POSITION>(
            MeasRefContainer::make_dict(ctx, rt, prs.begin(), prs.end())
            .get("Position").value());
    }
#endif // HYPERION_USE_CASACORE
  }
}

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End:
