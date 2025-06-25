#pragma once

#include "util_json.h"

namespace dxvk {
  void to_json(json& j, const Vector3& p) {
    j = json { {"x", p.x},{"y", p.y},{"z", p.z} };
  }

  void from_json(const json& j, Vector3& p) {
    j.at("x").get_to(p.x);
    j.at("y").get_to(p.y);
    j.at("z").get_to(p.z);
  }

  void to_json(json& j, const Vector2& p) {
    j = json { {"x", p.x},{"y", p.y} };
  }

  void from_json(const json& j, Vector2& p) {
    j.at("x").get_to(p.x);
    j.at("y").get_to(p.y);
  }
}