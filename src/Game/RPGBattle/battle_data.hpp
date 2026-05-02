#pragma once

#include <string>
#include <vector>

namespace lve::game::battle {

  struct BattleActorData {
    std::string id;
    std::string displayName;
    int maxHp{100};
    int attack{10};
    int defense{0};
  };

  struct BattleAction {
    std::string id;
    std::string displayName;
    int power{10};
  };

  struct BattleDefinition {
    std::string id;
    std::vector<BattleActorData> party;
    std::vector<BattleActorData> enemies;
  };

} // namespace lve::game::battle
