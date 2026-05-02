#pragma once

#include "Game/RPGBattle/battle_data.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace lve::game::battle {

  enum class BattleResult {
    Running,
    Victory,
    Defeat,
    Escaped
  };

  struct BattleActorState {
    BattleActorData data;
    int hp{0};

    bool isAlive() const { return hp > 0; }
  };

  class BattleSystem {
  public:
    void start(const BattleDefinition &definition);
    void reset();

    BattleResult getResult() const { return result; }
    const std::vector<BattleActorState> &party() const { return partyActors; }
    const std::vector<BattleActorState> &enemies() const { return enemyActors; }
    const std::string &lastLog() const { return logLine; }

    bool playerAttack(std::size_t partyIndex, std::size_t enemyIndex);
    bool enemyTurn();

  private:
    static int computeDamage(const BattleActorState &attacker, const BattleActorState &target);
    void updateResult();

    std::vector<BattleActorState> partyActors;
    std::vector<BattleActorState> enemyActors;
    BattleResult result{BattleResult::Running};
    std::string logLine;
  };

} // namespace lve::game::battle
