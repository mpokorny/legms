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
#include <hyperion/synthesis/ProductCFTable.h>

using namespace hyperion::synthesis;
using namespace hyperion;
using namespace Legion;

namespace cc = casacore;

using namespace std::complex_literals;

enum {
  CFCOMPUTE_TASK_ID,
  SHOW_GRID_TASK_ID,
};

#define ZC(F, S, N, C) ZCoeff{\
    0, F, cc::Stokes::S, \
    zernike_inverse_index(N).first, zernike_inverse_index(N).second, C}

std::vector<ZCoeff> zc{
  ZC(2.052e9, RR, 0, 0.37819f + 0.00002if),
  ZC(2.052e9, RR, 1, 0.01628f + 0.00047if),
  ZC(2.052e9, RR, 2, -0.09923f + -0.10694if),
  ZC(2.052e9, RR, 3, -0.39187f + 0.14695if),
  ZC(2.052e9, RR, 4, -0.13131f + -0.00091if),
  ZC(2.052e9, RR, 5, 0.01133f + -0.00092if),
  ZC(2.052e9, RR, 6, 0.00102f + 0.00192if),
  ZC(2.052e9, RR, 7, 0.01859f + -0.00012if),
  ZC(2.052e9, RR, 8, -0.11248f + -0.05952if),
  ZC(2.052e9, RR, 9, 0.26306f + 0.17708if),
  ZC(2.052e9, RR, 10, 0.69350f + -0.26642if),
  ZC(2.052e9, RR, 11, -0.03680f + 0.01169if),
  ZC(2.052e9, RR, 12, -0.29097f + 0.00009if),
  ZC(2.052e9, RR, 13, 0.00339f + 0.00033if),
  ZC(2.052e9, RR, 14, -0.06400f + 0.00003if),
  ZC(2.052e9, RR, 15, -0.00677f + 0.00264if),
  ZC(2.052e9, RR, 16, 0.00589f + -0.00239if),
  ZC(2.052e9, RR, 17, -0.01686f + -0.00083if),
  ZC(2.052e9, RR, 18, -0.08941f + -0.03308if),
  ZC(2.052e9, RR, 19, 0.14236f + 0.09008if),
  ZC(2.052e9, RR, 20, -0.00497f + 0.00497if),
  ZC(2.052e9, RR, 21, 0.15580f + -0.06187if),
  ZC(2.052e9, RR, 22, 0.03711f + -0.00733if),
  ZC(2.052e9, RR, 23, 0.05036f + -0.01948if),
  ZC(2.052e9, RR, 24, 0.10280f + 0.00141if),
  ZC(2.052e9, RR, 25, -0.01427f + 0.00114if),
  ZC(2.052e9, RR, 26, 0.13969f + 0.00059if),
  ZC(2.052e9, RR, 27, 0.00058f + 0.00019if),
  ZC(2.052e9, RR, 28, -0.00119f + -0.00111if),
  ZC(2.052e9, RR, 29, 0.00694f + -0.00259if),
  ZC(2.052e9, RR, 30, -0.00683f + -0.00186if),
  ZC(2.052e9, RR, 31, -0.00070f + -0.00161if),
  ZC(2.052e9, RR, 32, -0.02364f + -0.04063if),
  ZC(2.052e9, RR, 33, 0.05053f + 0.02714if),
  ZC(2.052e9, RR, 34, 0.00652f + -0.00628if),
  ZC(2.052e9, RR, 35, -0.15033f + -0.09639if),
  ZC(2.052e9, RR, 36, -0.03384f + 0.00533if),
  ZC(2.052e9, RR, 37, 0.00513f + -0.00445if),
  ZC(2.052e9, RR, 38, -0.00770f + 0.00211if),
  ZC(2.052e9, RR, 39, 0.00647f + -0.00278if),
  ZC(2.052e9, RR, 40, -0.02438f + -0.00152if),
  ZC(2.052e9, RR, 41, 0.00422f + -0.00167if),
  ZC(2.052e9, RR, 42, -0.04437f + -0.00022if),
  ZC(2.052e9, RR, 43, -0.00595f + 0.00014if),
  ZC(2.052e9, RR, 44, -0.08273f + 0.00019if),
  ZC(2.052e9, RR, 45, -0.00992f + 0.00086if),
  ZC(2.052e9, RR, 46, 0.00541f + -0.00039if),
  ZC(2.052e9, RR, 47, 0.00341f + 0.00008if),
  ZC(2.052e9, RR, 48, -0.00214f + 0.00619if),
  ZC(2.052e9, RR, 49, -0.00764f + 0.00319if),
  ZC(2.052e9, RR, 50, -0.01077f + 0.01481if),
  ZC(2.052e9, RR, 51, 0.00811f + 0.00655if),
  ZC(2.052e9, RR, 52, 0.00167f + 0.00230if),
  ZC(2.052e9, RR, 53, -0.02934f + -0.01891if),
  ZC(2.052e9, RR, 54, 0.05106f + 0.03961if),
  ZC(2.052e9, RR, 55, 0.00461f + 0.00036if),
  ZC(2.052e9, RR, 56, 0.02158f + -0.00070if),
  ZC(2.052e9, RR, 57, 0.00122f + 0.00022if),
  ZC(2.052e9, RR, 58, -0.00541f + 0.00050if),
  ZC(2.052e9, RR, 59, -0.00383f + 0.00122if),
  ZC(2.052e9, RR, 60, 0.06256f + 0.00081if),
  ZC(2.052e9, RR, 61, 0.00303f + 0.00109if),
  ZC(2.052e9, RR, 62, -0.04991f + -0.00042if),
  ZC(2.052e9, RR, 63, 0.00538f + -0.00042if),
  ZC(2.052e9, RR, 64, 0.09183f + -0.00002if),
  ZC(2.052e9, RR, 65, 0.00234f + 0.00002if),
  ZC(2.052e9, RL, 0, -25.80661f + 0.06602if),
  ZC(2.052e9, RL, 1, -0.65152f + -1.00155if),
  ZC(2.052e9, RL, 2, -15.04070f + 41.52323if),
  ZC(2.052e9, RL, 3, -186.20722f + 24.34289if),
  ZC(2.052e9, RL, 4, 18.62878f + 0.07767if),
  ZC(2.052e9, RL, 5, 1.03500f + 0.04380if),
  ZC(2.052e9, RL, 6, 1.38037f + 2.91762if),
  ZC(2.052e9, RL, 7, 0.52933f + -2.25038if),
  ZC(2.052e9, RL, 8, -15.05019f + 48.04883if),
  ZC(2.052e9, RL, 9, 35.20039f + -96.21997if),
  ZC(2.052e9, RL, 10, 333.31978f + -42.80111if),
  ZC(2.052e9, RL, 11, -23.80762f + 2.54675if),
  ZC(2.052e9, RL, 12, -13.08137f + -0.13430if),
  ZC(2.052e9, RL, 13, -34.23562f + 0.51758if),
  ZC(2.052e9, RL, 14, 3.75519f + -0.35850if),
  ZC(2.052e9, RL, 15, -0.35673f + 1.18708if),
  ZC(2.052e9, RL, 16, -0.74078f + -6.97377if),
  ZC(2.052e9, RL, 17, 1.32906f + 2.61703if),
  ZC(2.052e9, RL, 18, -10.89611f + 31.58441if),
  ZC(2.052e9, RL, 19, 17.94895f + -57.77273if),
  ZC(2.052e9, RL, 20, -0.89739f + 3.52072if),
  ZC(2.052e9, RL, 21, 64.83839f + -8.97041if),
  ZC(2.052e9, RL, 22, 16.67755f + -1.95098if),
  ZC(2.052e9, RL, 23, 22.84547f + -3.04019if),
  ZC(2.052e9, RL, 24, -3.13330f + 0.34980if),
  ZC(2.052e9, RL, 25, 20.27727f + 0.05112if),
  ZC(2.052e9, RL, 26, 7.86434f + 0.39691if),
  ZC(2.052e9, RL, 27, 7.57648f + 0.15748if),
  ZC(2.052e9, RL, 28, -0.13053f + -0.42633if),
  ZC(2.052e9, RL, 29, -0.10469f + 2.24020if),
  ZC(2.052e9, RL, 30, 0.62816f + 6.13542if),
  ZC(2.052e9, RL, 31, -0.77741f + -0.19489if),
  ZC(2.052e9, RL, 32, -2.81552f + 0.69070if),
  ZC(2.052e9, RL, 33, 6.44841f + -17.54858if),
  ZC(2.052e9, RL, 34, 0.22423f + -0.59755if),
  ZC(2.052e9, RL, 35, -19.50908f + 54.87752if),
  ZC(2.052e9, RL, 36, -3.58408f + 0.84919if),
  ZC(2.052e9, RL, 37, 2.63395f + 0.01330if),
  ZC(2.052e9, RL, 38, -4.51402f + 0.87025if),
  ZC(2.052e9, RL, 39, 2.29331f + -0.35031if),
  ZC(2.052e9, RL, 40, -2.21066f + -0.45484if),
  ZC(2.052e9, RL, 41, -6.12727f + -0.16934if),
  ZC(2.052e9, RL, 42, 8.00712f + -0.09759if),
  ZC(2.052e9, RL, 43, -6.65897f + -0.57116if),
  ZC(2.052e9, RL, 44, -12.73984f + -0.49130if),
  ZC(2.052e9, RL, 45, -0.57588f + -1.81337if),
  ZC(2.052e9, RL, 46, 0.62120f + -2.30787if),
  ZC(2.052e9, RL, 47, 1.41000f + -4.25086if),
  ZC(2.052e9, RL, 48, -1.19747f + -1.83575if),
  ZC(2.052e9, RL, 49, -0.60233f + 1.97311if),
  ZC(2.052e9, RL, 50, -1.50148f + 4.89920if),
  ZC(2.052e9, RL, 51, 1.23064f + -2.45920if),
  ZC(2.052e9, RL, 52, 0.69755f + 4.16094if),
  ZC(2.052e9, RL, 53, -3.88263f + 11.09111if),
  ZC(2.052e9, RL, 54, 6.24459f + -20.45131if),
  ZC(2.052e9, RL, 55, -1.95306f + -0.04489if),
  ZC(2.052e9, RL, 56, -1.37097f + 0.04812if),
  ZC(2.052e9, RL, 57, 2.74444f + -0.27308if),
  ZC(2.052e9, RL, 58, 0.58533f + -0.76869if),
  ZC(2.052e9, RL, 59, -6.01217f + 0.34919if),
  ZC(2.052e9, RL, 60, 7.50058f + 0.24948if),
  ZC(2.052e9, RL, 61, 3.57798f + -0.23103if),
  ZC(2.052e9, RL, 62, -13.12083f + 0.07727if),
  ZC(2.052e9, RL, 63, 4.11820f + 0.67011if),
  ZC(2.052e9, RL, 64, 10.69056f + 0.17448if),
  ZC(2.052e9, RL, 65, 4.66236f + -0.07345if),
  ZC(2.052e9, LR, 0, -24.99897f + 0.06202if),
  ZC(2.052e9, LR, 1, -0.67741f + -2.89692if),
  ZC(2.052e9, LR, 2, -22.03341f + 18.41248if),
  ZC(2.052e9, LR, 3, -268.71116f + 19.09900if),
  ZC(2.052e9, LR, 4, 16.87455f + -0.03516if),
  ZC(2.052e9, LR, 5, 0.76369f + -0.04144if),
  ZC(2.052e9, LR, 6, 1.39703f + 2.57244if),
  ZC(2.052e9, LR, 7, 0.30889f + -0.74684if),
  ZC(2.052e9, LR, 8, -23.23877f + 27.58344if),
  ZC(2.052e9, LR, 9, 51.42289f + -49.69930if),
  ZC(2.052e9, LR, 10, 480.64330f + -34.43130if),
  ZC(2.052e9, LR, 11, -29.90342f + 1.91919if),
  ZC(2.052e9, LR, 12, -10.76355f + 0.09642if),
  ZC(2.052e9, LR, 13, -33.66236f + 0.29802if),
  ZC(2.052e9, LR, 14, 2.16650f + -0.16708if),
  ZC(2.052e9, LR, 15, -0.51364f + -1.22897if),
  ZC(2.052e9, LR, 16, -0.52481f + -2.90719if),
  ZC(2.052e9, LR, 17, 1.40361f + -0.11373if),
  ZC(2.052e9, LR, 18, -15.19495f + 15.62002if),
  ZC(2.052e9, LR, 19, 27.06347f + -26.61686if),
  ZC(2.052e9, LR, 20, -1.15298f + -1.85528if),
  ZC(2.052e9, LR, 21, 98.50320f + -8.07025if),
  ZC(2.052e9, LR, 22, 19.77717f + -1.00834if),
  ZC(2.052e9, LR, 23, 33.53663f + -2.48772if),
  ZC(2.052e9, LR, 24, -3.29014f + 0.29252if),
  ZC(2.052e9, LR, 25, 19.26223f + 0.43511if),
  ZC(2.052e9, LR, 26, 9.93516f + -0.17523if),
  ZC(2.052e9, LR, 27, 5.72509f + -0.08472if),
  ZC(2.052e9, LR, 28, -0.43423f + 2.04602if),
  ZC(2.052e9, LR, 29, -0.02363f + 3.98447if),
  ZC(2.052e9, LR, 30, 0.42752f + 3.40931if),
  ZC(2.052e9, LR, 31, -0.64922f + 0.74964if),
  ZC(2.052e9, LR, 32, -4.05905f + -3.84284if),
  ZC(2.052e9, LR, 33, 9.29950f + -9.25395if),
  ZC(2.052e9, LR, 34, 0.06991f + 4.47719if),
  ZC(2.052e9, LR, 35, -28.90019f + 27.68897if),
  ZC(2.052e9, LR, 36, -6.74281f + 1.22119if),
  ZC(2.052e9, LR, 37, 9.45561f + 0.46380if),
  ZC(2.052e9, LR, 38, -6.15714f + 0.56670if),
  ZC(2.052e9, LR, 39, 3.74609f + -0.33597if),
  ZC(2.052e9, LR, 40, -3.46545f + -0.44572if),
  ZC(2.052e9, LR, 41, -5.84630f + -0.13484if),
  ZC(2.052e9, LR, 42, 5.32861f + 0.23153if),
  ZC(2.052e9, LR, 43, -5.89859f + 0.11522if),
  ZC(2.052e9, LR, 44, -11.97309f + -0.93312if),
  ZC(2.052e9, LR, 45, -0.91883f + 0.19888if),
  ZC(2.052e9, LR, 46, 0.78500f + -6.59305if),
  ZC(2.052e9, LR, 47, 1.65358f + -3.35719if),
  ZC(2.052e9, LR, 48, -1.00453f + -2.80342if),
  ZC(2.052e9, LR, 49, -0.68997f + 2.00094if),
  ZC(2.052e9, LR, 50, -1.72370f + 2.59812if),
  ZC(2.052e9, LR, 51, 1.34669f + -0.42930if),
  ZC(2.052e9, LR, 52, 0.93263f + 0.77580if),
  ZC(2.052e9, LR, 53, -5.65750f + 5.42056if),
  ZC(2.052e9, LR, 54, 9.94611f + -7.90033if),
  ZC(2.052e9, LR, 55, -2.80800f + 0.23331if),
  ZC(2.052e9, LR, 56, 1.90653f + -0.24550if),
  ZC(2.052e9, LR, 57, -1.10131f + -0.41898if),
  ZC(2.052e9, LR, 58, 0.47834f + -0.53619if),
  ZC(2.052e9, LR, 59, -7.26214f + -0.13598if),
  ZC(2.052e9, LR, 60, 7.84777f + 0.02842if),
  ZC(2.052e9, LR, 61, 3.39186f + -0.41548if),
  ZC(2.052e9, LR, 62, -10.96003f + 0.39200if),
  ZC(2.052e9, LR, 63, 5.03114f + 0.08369if),
  ZC(2.052e9, LR, 64, 10.56528f + 0.43006if),
  ZC(2.052e9, LR, 65, 3.14053f + -0.01114if),
  ZC(2.052e9, LL, 0, 0.37761f + 0.00000if),
  ZC(2.052e9, LL, 1, 0.01628f + -0.02645if),
  ZC(2.052e9, LL, 2, -0.07972f + 0.07925if),
  ZC(2.052e9, LL, 3, -0.75420f + 0.26384if),
  ZC(2.052e9, LL, 4, -0.13261f + 0.00091if),
  ZC(2.052e9, LL, 5, 0.01219f + 0.00238if),
  ZC(2.052e9, LL, 6, 0.00105f + -0.00164if),
  ZC(2.052e9, LL, 7, 0.01858f + 0.02544if),
  ZC(2.052e9, LL, 8, -0.09017f + 0.01234if),
  ZC(2.052e9, LL, 9, 0.21847f + -0.07088if),
  ZC(2.052e9, LL, 10, 1.36273f + -0.47233if),
  ZC(2.052e9, LL, 11, -0.07123f + 0.02620if),
  ZC(2.052e9, LL, 12, -0.28977f + 0.00105if),
  ZC(2.052e9, LL, 13, 0.00334f + 0.00095if),
  ZC(2.052e9, LL, 14, -0.06581f + -0.00006if),
  ZC(2.052e9, LL, 15, -0.00588f + 0.00327if),
  ZC(2.052e9, LL, 16, 0.00673f + 0.00178if),
  ZC(2.052e9, LL, 17, -0.01683f + 0.00897if),
  ZC(2.052e9, LL, 18, -0.07708f + 0.00169if),
  ZC(2.052e9, LL, 19, 0.11816f + -0.03748if),
  ZC(2.052e9, LL, 20, -0.00594f + -0.00045if),
  ZC(2.052e9, LL, 21, 0.30369f + -0.10381if),
  ZC(2.052e9, LL, 22, 0.04042f + -0.01481if),
  ZC(2.052e9, LL, 23, 0.09780f + -0.03381if),
  ZC(2.052e9, LL, 24, 0.10355f + 0.00091if),
  ZC(2.052e9, LL, 25, -0.01356f + -0.00086if),
  ZC(2.052e9, LL, 26, 0.14030f + 0.00000if),
  ZC(2.052e9, LL, 27, 0.00178f + 0.00002if),
  ZC(2.052e9, LL, 28, -0.00255f + -0.00361if),
  ZC(2.052e9, LL, 29, 0.00628f + -0.00372if),
  ZC(2.052e9, LL, 30, -0.00736f + -0.00050if),
  ZC(2.052e9, LL, 31, -0.00084f + -0.01341if),
  ZC(2.052e9, LL, 32, -0.02006f + -0.00953if),
  ZC(2.052e9, LL, 33, 0.04297f + -0.01177if),
  ZC(2.052e9, LL, 34, 0.00686f + 0.00580if),
  ZC(2.052e9, LL, 35, -0.12508f + 0.03523if),
  ZC(2.052e9, LL, 36, -0.01539f + 0.00664if),
  ZC(2.052e9, LL, 37, 0.01723f + -0.00545if),
  ZC(2.052e9, LL, 38, -0.01439f + 0.00342if),
  ZC(2.052e9, LL, 39, 0.01303f + -0.00433if),
  ZC(2.052e9, LL, 40, -0.02536f + -0.00228if),
  ZC(2.052e9, LL, 41, 0.00448f + -0.00145if),
  ZC(2.052e9, LL, 42, -0.04408f + 0.00041if),
  ZC(2.052e9, LL, 43, -0.00730f + -0.00011if),
  ZC(2.052e9, LL, 44, -0.08711f + -0.00008if),
  ZC(2.052e9, LL, 45, -0.00767f + 0.00250if),
  ZC(2.052e9, LL, 46, 0.00847f + 0.00397if),
  ZC(2.052e9, LL, 47, 0.00302f + 0.00128if),
  ZC(2.052e9, LL, 48, -0.00269f + 0.00080if),
  ZC(2.052e9, LL, 49, -0.00770f + 0.00089if),
  ZC(2.052e9, LL, 50, -0.01020f + 0.02487if),
  ZC(2.052e9, LL, 51, 0.00636f + -0.00119if),
  ZC(2.052e9, LL, 52, 0.00263f + -0.00103if),
  ZC(2.052e9, LL, 53, -0.02428f + 0.00800if),
  ZC(2.052e9, LL, 54, 0.03852f + -0.02041if),
  ZC(2.052e9, LL, 55, 0.00256f + -0.00003if),
  ZC(2.052e9, LL, 56, -0.00686f + 0.00080if),
  ZC(2.052e9, LL, 57, -0.00230f + 0.00056if),
  ZC(2.052e9, LL, 58, 0.00411f + 0.00005if),
  ZC(2.052e9, LL, 59, -0.00397f + 0.00059if),
  ZC(2.052e9, LL, 60, 0.06297f + 0.00070if),
  ZC(2.052e9, LL, 61, 0.00234f + 0.00153if),
  ZC(2.052e9, LL, 62, -0.04909f + 0.00045if),
  ZC(2.052e9, LL, 63, 0.00602f + 0.00025if),
  ZC(2.052e9, LL, 64, 0.09658f + -0.00066if),
  ZC(2.052e9, LL, 65, 0.00484f + -0.00030if),
};

template <unsigned INDEX_RANK>
using cf_col_t =
  PhysicalColumnTD<
  ValueType<complex<float>>::DataType,
  INDEX_RANK,
  INDEX_RANK + 2,
  AffineAccessor>;

#define CF_TABLE_AXES \
  CF_BASELINE_CLASS, CF_PARALLACTIC_ANGLE, CF_FREQUENCY, CF_W, CF_STOKES_OUT, CF_STOKES_IN

void
show_grid_task(
  const Task* task,
  const std::vector<PhysicalRegion>& regions,
  Context ctx,
  Runtime* rt) {

  const CFTableBase::ShowValuesTaskArgs& args =
    *static_cast<const CFTableBase::ShowValuesTaskArgs*>(task->args);
  std::cout << "!!!!" << args.title << "!!!!" << std::endl;

  auto pt =
    PhysicalTable::create_all_unsafe(
      rt,
      {args.tdesc},
      task->regions,
      regions)[0];
  auto columns = pt.columns();
  std::vector<cf_table_axes_t> index_axes;
  for (auto& ia : pt.index_axes())
    index_axes.push_back(static_cast<cf_table_axes_t>(ia));
  std::vector<std::shared_ptr<PhysicalColumn>> index_columns;
  for (auto& ia : index_axes)
    index_columns.push_back(columns.at(cf_table_axis_name(ia)));
  auto cs_x_col =
    GridCoordinateTable::CoordColumn<Legion::AffineAccessor>(
      *pt.column(GridCoordinateTable::COORD_X_NAME).value());
  auto cs_y_col =
    GridCoordinateTable::CoordColumn<Legion::AffineAccessor>(
      *pt.column(GridCoordinateTable::COORD_Y_NAME).value());
  auto cs_x_rect = cs_x_col.rect();
  auto grid_size = cs_x_rect.hi[2] - cs_x_rect.lo[2] + 1;
  auto cs_x = cs_x_col.template accessor<LEGION_READ_ONLY>();
  auto cs_y = cs_y_col.template accessor<LEGION_READ_ONLY>();
  Point<3> index;
  for (size_t i = 0; i < 1; ++i)
    index[i] = -1;
  PointInRectIterator<3> pir(cs_x_rect, false);
  while (pir()) {
    for (size_t i = 0; i < 1; ++i)
      index[i] = pir[i];
    std::cout << "*** " << cf_table_axis_name(index_axes[0])
              << ": ";
    CFTableBase::show_index_value(*index_columns[0], pir[0]);
    std::cout << std::endl;
    for (coord_t i = 0; i < grid_size; ++i) {
      for (coord_t j = 0; j < grid_size; ++j) {
        auto x = cs_x[*pir];
        auto y = cs_y[*pir++];
        std::cout << "(" << x << "," << y << ") ";
      }
      std::cout << std::endl;
    }
  }
}

[[maybe_unused]] static void
show_grid(
  Context ctx,
  Runtime* rt,
  const std::string& title,
  const GridCoordinateTable& table) {

  auto colreqs = Column::default_requirements_mapped;

  auto reqs =
    table.requirements(
      ctx,
      rt,
      ColumnSpacePartition(),
      {{GridCoordinateTable::COORD_X_NAME, colreqs},
       {GridCoordinateTable::COORD_Y_NAME, colreqs},
       {cf_table_axis<CF_PARALLACTIC_ANGLE>::name, colreqs}},
      CXX_OPTIONAL_NAMESPACE::nullopt);
  CFTableBase::ShowValuesTaskArgs args;
  args.tdesc = std::get<2>(reqs);
  args.title = title;
  TaskLauncher task(SHOW_GRID_TASK_ID, TaskArgument(&args, sizeof(args)));
  for (auto& r : std::get<0>(reqs))
    task.add_region_requirement(r);
  rt->execute_task(ctx, task);
}

void
cfcompute_task(
  const Task*,
  const std::vector<PhysicalRegion>&,
  Context ctx,
  Runtime* rt) {

  const size_t grid_size = 5;
  const double cf_radius = static_cast<double>(grid_size) / 2;

  GridCoordinateTable ps_coords(ctx, rt, grid_size, {0.0});
  ps_coords.compute_coordinates(ctx, rt, cc::LinearCoordinate(2), cf_radius);
  PSTermTable ps_tbl(ctx, rt, grid_size, {0.08, 0.16});
  ps_tbl.compute_cfs(ctx, rt, ps_coords);
  ps_tbl.show_cf_values(ctx, rt, "PSTerm");
  ps_coords.destroy(ctx, rt);

  GridCoordinateTable w_coords(ctx, rt, grid_size, {0.0});
  w_coords.compute_coordinates(ctx, rt, cc::LinearCoordinate(2), 2.0);
  WTermTable w_tbl(ctx, rt, grid_size, {2.2, 22.2, 222.2});
  w_tbl.compute_cfs(ctx, rt, w_coords);
  w_tbl.show_cf_values(ctx, rt, "WTerm");
  w_coords.destroy(ctx, rt);

  std::vector<typename cf_table_axis<CF_PARALLACTIC_ANGLE>::type>
    parallactic_angles{0.0, 3.1415926 / 4.0};
  GridCoordinateTable a_coords(ctx, rt, grid_size, parallactic_angles);
  a_coords.compute_coordinates(ctx, rt, cc::LinearCoordinate(2), 1.0);
  ATermTable a_tbl(
    ctx,
    rt,
    grid_size,
    {0},
    parallactic_angles,
    {2.052e9},
    {cc::Stokes::RR},
    {cc::Stokes::RR});
  a_tbl.compute_cfs(ctx, rt, a_coords, zc);
  a_tbl.show_cf_values(ctx, rt, "ATerm");
  a_coords.destroy(ctx, rt);

  auto cf_tbl =
    ProductCFTable<CF_TABLE_AXES>::create_and_fill(
      ctx,
      rt,
      ColumnSpacePartition(),
      a_tbl,
      w_tbl,
      ps_tbl);
  cf_tbl.show_cf_values(ctx, rt, "Pre-FFT CF");
  cf_tbl.apply_fft(ctx, rt, 1, true, true, FFTW_MEASURE, 5.0);
  cf_tbl.show_cf_values(ctx, rt, "Post-FFT CF");

  std::cout << "+++++ DONE +++++" << std::endl;
  ps_tbl.destroy(ctx, rt);
  w_tbl.destroy(ctx, rt);
  a_tbl.destroy(ctx, rt);
  cf_tbl.destroy(ctx, rt);
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
  {
    TaskVariantRegistrar registrar(SHOW_GRID_TASK_ID, "show_grid_task");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<show_grid_task>(
      registrar,
      "show_grid_task");
  }
  synthesis::CFTableBase::preregister_all();
  synthesis::ProductCFTable<CF_TABLE_AXES>::preregister_tasks();
  return Runtime::start(argc, argv);
}

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End:
