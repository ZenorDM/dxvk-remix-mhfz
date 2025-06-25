#pragma once

#include <json/json.hpp>
#include "util_vector.h"

using json = nlohmann::json;

namespace dxvk {
  void to_json(json& j, const Vector3& p);
  void to_json(json& j, const Vector2& p);

  void from_json(const json& j, Vector3& p);
  void from_json(const json& j, Vector2& p);
}