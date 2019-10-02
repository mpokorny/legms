#include "testing/TestSuiteDriver.h"
#include "testing/TestRecorder.h"

#include "MeasRefDict.h"

#ifdef LEGMS_USE_CASACORE
#include <casacore/measures/Measures/MeasData.h>
#include <casacore/casa/System/AppState.h>

using namespace legms;
using namespace Legion;

enum {
  MEAS_REF_DICT_TEST_SUITE,
};

class CasacoreState
  : public casacore::AppState {
public:

  CasacoreState() {}

  std::list<std::string>
  dataPath() const override {
    static std::list<std::string>
      result{"/users/mpokorny/projects/casa.git/data"};
    return result;
  }

  bool
  initialized() const override {
    return true;
  }
};

#define TE(f) testing::TestEval([&](){ return f; }, #f)

bool
check_dict_value_type(
  Context ctx,
  Runtime* rt,
  const MeasRefDict::Ref& value,
  const MeasRef& ref) {

  bool result = false;
  switch (ref.mclass(ctx, rt)) {
#define CHECK(M)                                                        \
    case M:                                                             \
      result =                                                          \
        std::holds_alternative<casacore::MeasRef<MClassT<M>::type>>(value); \
    break;
    LEGMS_FOREACH_MCLASS(CHECK)
  default:
    assert(false);
    break;
  }
  return result;
}

void
meas_ref_dict_test_suite(
  const Task *task,
  const std::vector<PhysicalRegion>& regions,
  Context ctx,
  Runtime* rt) {

  casacore::AppStateSource::initialize(new CasacoreState);

  register_tasks(ctx, rt);

  testing::TestRecorder<READ_WRITE> recorder(
    testing::TestLog<READ_WRITE>(
      task->regions[0].region,
      regions[0],
      task->regions[1].region,
      regions[1],
      ctx,
      rt));

  {
    casacore::MEpoch::Ref ref_tai(casacore::MEpoch::TAI);
    MeasRef mr_tai = MeasRef::create(ctx, rt, "EPOCH", ref_tai);
    casacore::MDirection::Ref ref_j2000(casacore::MDirection::J2000);
    MeasRef mr_j2000 = MeasRef::create(ctx, rt, "DIRECTION", ref_j2000);
    casacore::MPosition::Ref ref_wgs84(casacore::MPosition::WGS84);
    MeasRef mr_wgs84 = MeasRef::create(ctx, rt, "POSITION", ref_wgs84);
    casacore::MFrequency::Ref ref_geo(casacore::MFrequency::GEO);
    MeasRef mr_geo = MeasRef::create(ctx, rt, "FREQUENCY", ref_geo);
    casacore::MRadialVelocity::Ref ref_topo(casacore::MRadialVelocity::TOPO);
    MeasRef mr_topo = MeasRef::create(ctx, rt, "RADIAL_VELOCITY", ref_topo);
    casacore::MDoppler::Ref ref_z(casacore::MDoppler::Z);
    MeasRef mr_z = MeasRef::create(ctx, rt, "DOPPLER", ref_z);
    std::vector<const MeasRef*>
      mrs{&mr_tai, &mr_j2000, &mr_wgs84, &mr_geo, &mr_topo, &mr_z};

    {
      MeasRefDict dict(ctx, rt, mrs);
      recorder.expect_false(
        "Empty optional value returned for non-existent MeasRef name",
        TE(dict.get("FOOBAR").has_value()));
      for (auto& mr : mrs) {
        auto name = mr->name(ctx, rt);
        auto ref = dict.get(name);
        recorder.assert_true(
          std::string("Non-empty optional value returned for MeasRef ") + name,
          TE(ref.has_value()));
        recorder.expect_true(
          std::string("Contained value for MeasRef ")
          + name + " has expected type",
          TE(check_dict_value_type(ctx, rt, *ref.value(), *mr)));
      }
    }

    for (auto& mr : mrs)
      const_cast<MeasRef*>(mr)->destroy(ctx, rt);
  }
}

int
main(int argc, char* argv[]) {

  testing::TestSuiteDriver driver =
    testing::TestSuiteDriver::make<meas_ref_dict_test_suite>(
      MEAS_REF_DICT_TEST_SUITE,
      "meas_ref_dict_test_suite");

  return driver.start(argc, argv);
}

#endif // LEGMS_USE_CASACORE

// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End:
