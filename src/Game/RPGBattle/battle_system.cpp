#include "Game/RPGBattle/battle_system.hpp"

#include <algorithm>

namespace lve::game::battle {
  namespace {
    BattleActorState makeState(const BattleActorData &data) {
      BattleActorState state{};
      state.data = data;
      state.hp = data.maxHp;
      return state;
    }
  } // namespace

  void BattleSystem::start(const BattleDefinition &definition) {
    partyActors.clear();
    enemyActors.clear();
    partyActors.reserve(definition.party.size());
    enemyActors.reserve(definition.enemies.size());

    for (const auto &actor : definition.party) {
      partyActors.push_back(makeState(actor));
    }
    for (const auto &actor : definition.enemies) {
      enemyActors.push_back(makeState(actor));
    }

    result = BattleResult::Running;
    logLine = "Battle started.";
    updateResult();
  }

  void BattleSystem::reset() {
    partyActors.clear();
    enemyActors.clear();
    result = BattleResult::Running;
    logLine.clear();
  }

  int BattleSystem::computeDamage(const BattleActorState &attacker, const BattleActorState &target) {
    return std::max(1, attacker.data.attack - target.data.defense);
  }

  bool BattleSystem::playerAttack(std::size_t partyIndex, std::size_t enemyIndex) {
    if (result != BattleResult::Running ||
        partyIndex >= partyActors.size() ||
        enemyIndex >= enemyActors.size() ||
        !partyActors[partyIndex].isAlive() ||
        !enemyActors[enemyIndex].isAlive()) {
      return false;
    }

    const int damage = computeDamage(partyActors[partyIndex], enemyActors[enemyIndex]);
    enemyActors[enemyIndex].hp = std::max(0, enemyActors[enemyIndex].hp - damage);
    logLine = partyActors[partyIndex].data.displayName + " attacked " +
      enemyActors[enemyIndex].data.displayName + ".";
    updateResult();
    return true;
  }

  bool BattleSystem::enemyTurn() {
    if (result != BattleResult::Running) {
      return false;
    }

    auto enemyIt = std::find_if(enemyActors.begin(), enemyActors.end(), [](const BattleActorState &actor) {
      return actor.isAlive();
    });
    auto partyIt = std::find_if(partyActors.begin(), partyActors.end(), [](const BattleActorState &actor) {
      return actor.isAlive();
    });
    if (enemyIt == enemyActors.end() || partyIt == partyActors.end()) {
      updateResult();
      return false;
    }

    const int damage = computeDamage(*enemyIt, *partyIt);
    partyIt->hp = std::max(0, partyIt->hp - damage);
    logLine = enemyIt->data.displayName + " attacked " + partyIt->data.displayName + ".";
    updateResult();
    return true;
  }

  void BattleSystem::updateResult() {
    const bool anyPartyAlive = std::any_of(partyActors.begin(), partyActors.end(), [](const BattleActorState &actor) {
      return actor.isAlive();
    });
    const bool anyEnemyAlive = std::any_of(enemyActors.begin(), enemyActors.end(), [](const BattleActorState &actor) {
      return actor.isAlive();
    });

    if (!anyPartyAlive) {
      result = BattleResult::Defeat;
    } else if (!anyEnemyAlive) {
      result = BattleResult::Victory;
    } else {
      result = BattleResult::Running;
    }
  }

} // namespace lve::game::battle
