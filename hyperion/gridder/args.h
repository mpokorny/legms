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
#include <hyperion/hyperion.h>

#include <hyperion/gridder/gridder.h>

#include CXX_FILESYSTEM_HEADER
#include CXX_OPTIONAL_HEADER
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace hyperion {
namespace gridder {

typedef enum {
  VALUE_ARGS,
  STRING_ARGS,
  OPT_VALUE_ARGS,
  OPT_STRING_ARGS} args_t;

template <args_t G>
struct ArgsCompletion {
  static const args_t val;
};
template <>
struct ArgsCompletion<VALUE_ARGS> {
  static const constexpr args_t val = VALUE_ARGS;
};
template <>
struct ArgsCompletion<STRING_ARGS> {
  static const constexpr args_t val = STRING_ARGS;
};
template <>
struct ArgsCompletion<OPT_VALUE_ARGS> {
  static const constexpr args_t val = VALUE_ARGS;
};
template <>
struct ArgsCompletion<OPT_STRING_ARGS> {
  static const constexpr args_t val = STRING_ARGS;
};

template <typename T, bool OPT, args_t G>
struct ArgType {
  //static const char* tag;
};
template <typename T>
struct ArgType<T, false, VALUE_ARGS> {
  ArgType(const char* _tag, const char* _desc)
    : tag(_tag)
    , desc(_desc) {}

  typedef T type;

  T val;

  const char* tag;

  const char* desc;

  T
  value() const {
    return val;
  }

  void
  operator=(const T& t) {
    val = t;
  }

  operator bool() const {
    return true;
  }
};
template <typename T>
struct ArgType<T, true, VALUE_ARGS> {
  ArgType(const char* _tag, const char* _desc)
    : tag(_tag)
    , desc(_desc) {}

  typedef CXX_OPTIONAL_NAMESPACE::optional<T> type;

  CXX_OPTIONAL_NAMESPACE::optional<T> val;

  const char* tag;

  const char* desc;

  T
  value() const {
    return val.value();
  }

  void
  operator=(const T& t) {
    val = t;
  }

  void
  operator=(const CXX_OPTIONAL_NAMESPACE::optional<T>& ot) {
    val = ot;
  }

  operator bool() const {
    return (bool)val;
  }
};
template <typename T>
struct ArgType<T, false, STRING_ARGS> {
  ArgType(const char* _tag, const char* _desc)
    : tag(_tag)
    , desc(_desc) {}

  typedef std::string type;

  std::string val;

  const char* tag;

  const char* desc;

  std::string
  value() const {
    return val;
  }

  void
  operator=(const std::string& str) {
    val = str;
  }

  operator bool() const {
    return true;
  }
};
template <typename T>
struct ArgType<T, true, STRING_ARGS> {
  ArgType(const char* _tag, const char* _desc)
    : tag(_tag)
    , desc(_desc) {}

  typedef CXX_OPTIONAL_NAMESPACE::optional<std::string> type;

  CXX_OPTIONAL_NAMESPACE::optional<std::string> val;

  const char* tag;

  const char* desc;

  std::string
  value() const {
    return val.value();
  }

  void
  operator=(const std::string& str) {
    val = str;
  }

  void
  operator=(const CXX_OPTIONAL_NAMESPACE::optional<std::string>& ostr) {
    val = ostr;
  }

  operator bool() const {
    return (bool)val;
  }
};
template <typename T>
struct ArgType<T, false, OPT_VALUE_ARGS> {
  ArgType(const char* _tag, const char* _desc)
    : tag(_tag)
    , desc(_desc) {}

  typedef CXX_OPTIONAL_NAMESPACE::optional<T> type;

  CXX_OPTIONAL_NAMESPACE::optional<T> val;

  const char* tag;

  const char* desc;

  T
  value() const {
    return val;
  }

  void
  operator=(const T& t) {
    val = t;
  }

  void
  operator=(const CXX_OPTIONAL_NAMESPACE::optional<T>& ot) {
    val = ot;
  }

  operator bool() const {
    return (bool)val;
  }
};
template <typename T>
struct ArgType<T, true, OPT_VALUE_ARGS> {
  ArgType(const char* _tag, const char* _desc)
    : tag(_tag)
    , desc(_desc) {}

  typedef CXX_OPTIONAL_NAMESPACE::optional<T> type;

  CXX_OPTIONAL_NAMESPACE::optional<T> val;

  const char* tag;

  const char* desc;

  T
  value() const {
    return val.value();
  }

  void
  operator=(const T& t) {
    val = t;
  }

  void
  operator=(const CXX_OPTIONAL_NAMESPACE::optional<T>& ot) {
    val = ot;
  }

  operator bool() const {
    return (bool)val;
  }
};
template <typename T>
struct ArgType<T, false, OPT_STRING_ARGS> {
  ArgType(const char* _tag, const char* _desc)
    : tag(_tag)
    , desc(_desc) {}

  typedef CXX_OPTIONAL_NAMESPACE::optional<std::string> type;

  CXX_OPTIONAL_NAMESPACE::optional<std::string> val;

  const char* tag;

  const char* desc;

  std::string
  value() const {
    return val.value();
  }

  void
  operator=(const std::string& str) {
    val = str;
  }

  void
  operator=(const CXX_OPTIONAL_NAMESPACE::optional<std::string>& ostr) {
    val = ostr;
  }

  operator bool() const {
    return (bool)val;
  }
};
template <typename T>
struct ArgType<T, true, OPT_STRING_ARGS> {
  ArgType(const char* _tag, const char* _desc)
    : tag(_tag)
    , desc(_desc) {}

  typedef CXX_OPTIONAL_NAMESPACE::optional<std::string> type;

  CXX_OPTIONAL_NAMESPACE::optional<std::string> val;

  const char* tag;

  const char* desc;

  std::string
  value() const {
    return val.value();
  }

  void
  operator=(const std::string& str) {
    val = str;
  }

  void
  operator=(const CXX_OPTIONAL_NAMESPACE::optional<std::string>& ostr) {
    val = ostr;
  }

  operator bool() const {
    return (bool)val;
  }
};

struct ArgsBase {
  static const constexpr char* h5_path_tag = "h5";
  static const constexpr char* h5_path_desc =
    "path to MS-derived HDF5 file [REQUIRED]";

  static const constexpr char* config_path_tag = "configuration";
  static const constexpr char* config_path_desc =
    "path to gridder configuration file";

  static const constexpr char* echo_tag = "echo";
  static const constexpr char* echo_desc =
    "echo configuration parameters to stdout (true/false)";

  static const constexpr char* min_block_tag = "min_block";
  static const constexpr char* min_block_desc =
    "gridding block size (number of rows)";

  static const constexpr char* pa_step_tag = "pa_step";
  static const constexpr char* pa_step_desc =
    "parallactic angle bin size (degrees)";

  static const constexpr char* pa_block_tag = "pa_block";
  static const constexpr char* pa_block_desc =
    "parallactic angle computation block size (number of rows)";

  static const constexpr char* w_planes_tag = "w_proj_planes";
  static const constexpr char* w_planes_desc =
    "number of W-projection planes";

  static const std::vector<std::string>&
  tags() {
    static const std::vector<std::string> result{
      h5_path_tag,
      config_path_tag,
      echo_tag,
      min_block_tag,
      pa_step_tag,
      pa_block_tag,
      w_planes_tag
    };
    return result;
  }
};

template <args_t G>
struct Args
  : public ArgsBase {
  ArgType<CXX_FILESYSTEM_NAMESPACE::path, false, G> h5_path;
  ArgType<CXX_FILESYSTEM_NAMESPACE::path, true, G> config_path;
  ArgType<bool, false, G> echo;
  ArgType<size_t, false, G> min_block;
  ArgType<PARALLACTIC_ANGLE_TYPE, false, G> pa_step;
  ArgType<size_t, false, G> pa_block;
  ArgType<int, false, G> w_planes;

  Args()
    : h5_path(h5_path_tag, h5_path_desc)
    , config_path(config_path_tag, config_path_desc)
    , echo(echo_tag, echo_desc)
    , min_block(min_block_tag, min_block_desc)
    , pa_step(pa_step_tag, pa_step_desc)
    , pa_block(pa_block_tag, pa_block_desc)
    , w_planes(w_planes_tag, w_planes_desc) {}

  Args(
    const typename decltype(h5_path)::type& h5_path_,
    const typename decltype(config_path)::type& config_path_,
    const typename decltype(echo)::type& echo_,
    const typename decltype(min_block)::type& min_block_,
    const typename decltype(pa_step)::type& pa_step_,
    const typename decltype(pa_block)::type& pa_block_,
    const typename decltype(w_planes)::type& w_planes_)
    : h5_path(h5_path_tag, h5_path_desc)
    , config_path(config_path_tag, config_path_desc)
    , echo(echo_tag, echo_desc)
    , min_block(min_block_tag, min_block_desc)
    , pa_step(pa_step_tag, pa_step_desc)
    , pa_block(pa_block_tag, pa_block_desc)
    , w_planes(w_planes_tag, w_planes_desc) {

    h5_path = h5_path_;
    config_path = config_path_;
    echo = echo_;
    min_block = min_block_;
    pa_step = pa_step_;
    pa_block = pa_block_;
    w_planes = w_planes_;
  }

  bool
  is_complete() const {
    return
      h5_path
      && echo
      && min_block
      && pa_step
      && pa_block
      && w_planes;
  }

  CXX_OPTIONAL_NAMESPACE::optional<Args<ArgsCompletion<G>::val>>
  as_complete() const {
    CXX_OPTIONAL_NAMESPACE::optional<Args<ArgsCompletion<G>::val>> result;
    if (is_complete())
      result =
        CXX_OPTIONAL_NAMESPACE::optional<Args<ArgsCompletion<G>::val>>(
          Args<ArgsCompletion<G>::val>(
            h5_path.value(),
            (config_path
             ? config_path.value()
             : CXX_OPTIONAL_NAMESPACE::optional<std::string>()),
            echo.value(),
            min_block.value(),
            pa_step.value(),
            pa_block.value(),
            w_planes.value()));
    return result;
  }

  YAML::Node
  as_node() const {
    YAML::Node result;
    if (h5_path)
      result[h5_path.tag] = h5_path.value().c_str();
    if (config_path)
      result[config_path.tag] = config_path.value().c_str();
    if (echo)
      result[echo.tag] = echo.value();
    if (min_block)
      result[min_block.tag] = min_block.value();
    if (pa_step)
      result[pa_step.tag] = pa_step.value();
    if (pa_block)
      result[pa_block.tag] = pa_block.value();
    if (w_planes)
      result[w_planes.tag] = w_planes.value();
    return result;
  }

  std::map<std::string, std::string>
  help() const {
    return
      { {h5_path_tag, h5_path_desc}
      , {config_path_tag, config_path_desc}
      , {echo_tag, echo_desc}
      , {min_block_tag, min_block_desc}
      , {pa_step_tag, pa_step_desc}
      , {pa_block_tag, pa_block_desc}
      , {w_planes_tag, w_planes_desc}
      };
  }
};

Args<VALUE_ARGS>
as_args(YAML::Node&& node);

bool
has_help_flag(const Legion::InputArgs& args);

bool
get_args(const Legion::InputArgs& args, Args<OPT_STRING_ARGS>& gridder_args);

CXX_OPTIONAL_NAMESPACE::optional<std::string>
validate_args(const Args<VALUE_ARGS>& args);

} // end namespace gridder
} // end namespace hyperion


// Local Variables:
// mode: c++
// c-basic-offset: 2
// fill-column: 80
// indent-tabs-mode: nil
// End:
