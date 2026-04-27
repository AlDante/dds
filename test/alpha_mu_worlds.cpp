/*
  alpha_mu world-construction and information-state helpers

   Copyright © 2026 by David Jenkins
   All rights reserved.
*/

#include "alpha_mu_core.h"

namespace alpha_mu
{
  using namespace std;

#ifndef NDEBUG
  static void DebugCheckWorldMaskCapacity(
    const unsigned worldCount,
    const string& context)
  {
    ostringstream oss;
    oss << context
        << " requires at most 64 worlds because WorldMask is backed by one 64-bit word"
        << " (got " << worldCount << ")";
    Check(worldCount <= 64U, oss.str());
  }
#endif

  bool ConstructorLengthConstraintsPossible(
    const vector<int>& hiddenSeats,
    const vector<WorldConstraint>& constraints,
    const ParsedWorld& current,
    const vector<HiddenCardCandidate>& hiddenCards,
    const unsigned nextIndex)
  {
    for (unsigned i = 0; i < constraints.size(); i++)
    {
      const WorldConstraint& constraint = constraints[i];
      if (! ConstructorConstraintTouchesHiddenSeats(hiddenSeats, constraint))
        continue;

      if (constraint.kind != CONSTRAINT_VOID_SUIT &&
          constraint.kind != CONSTRAINT_MIN_LENGTH &&
          constraint.kind != CONSTRAINT_MAX_LENGTH &&
          constraint.kind != CONSTRAINT_PARTNERSHIP_MIN_LENGTH &&
          constraint.kind != CONSTRAINT_PARTNERSHIP_MAX_LENGTH)
      {
        continue;
      }

      if (constraint.kind == CONSTRAINT_PARTNERSHIP_MIN_LENGTH ||
          constraint.kind == CONSTRAINT_PARTNERSHIP_MAX_LENGTH)
      {
        const int partner = PartnerSeat(constraint.player);
        const int currentLength = WorldSuitLength(current, constraint.player,
          constraint.suit) + WorldSuitLength(current, partner, constraint.suit);
        if (constraint.kind == CONSTRAINT_PARTNERSHIP_MAX_LENGTH)
        {
          if (currentLength > constraint.count)
            return false;
          continue;
        }

        const int remaining = PotentialRemainingSuitCardsForSeat(hiddenCards,
          nextIndex, constraint.player, constraint.suit) +
          PotentialRemainingSuitCardsForSeat(hiddenCards, nextIndex, partner,
            constraint.suit);
        if (currentLength + remaining < constraint.count)
          return false;
        continue;
      }

      const int currentLength = WorldSuitLength(current, constraint.player,
        constraint.suit);
      if (constraint.kind == CONSTRAINT_VOID_SUIT)
      {
        if (currentLength > 0)
          return false;
        continue;
      }

      if (constraint.kind == CONSTRAINT_MAX_LENGTH)
      {
        if (currentLength > constraint.count)
          return false;
        continue;
      }

      const int remaining = PotentialRemainingSuitCardsForSeat(hiddenCards,
        nextIndex, constraint.player, constraint.suit);
      if (currentLength + remaining < constraint.count)
        return false;
    }

    return true;
  }

  bool ConstructorHCPConstraintsPossible(
    const vector<int>& hiddenSeats,
    const vector<WorldConstraint>& constraints,
    const ParsedWorld& current,
    const vector<HiddenCardCandidate>& hiddenCards,
    const unsigned nextIndex)
  {
    for (unsigned i = 0; i < constraints.size(); i++)
    {
      const WorldConstraint& constraint = constraints[i];
      if (! ConstructorConstraintTouchesHiddenSeats(hiddenSeats, constraint))
        continue;

      if (constraint.kind != CONSTRAINT_MIN_HCP &&
          constraint.kind != CONSTRAINT_MAX_HCP &&
          constraint.kind != CONSTRAINT_PARTNERSHIP_MIN_HCP &&
          constraint.kind != CONSTRAINT_PARTNERSHIP_MAX_HCP)
      {
        continue;
      }

      if (constraint.kind == CONSTRAINT_PARTNERSHIP_MIN_HCP ||
          constraint.kind == CONSTRAINT_PARTNERSHIP_MAX_HCP)
      {
        const int partner = PartnerSeat(constraint.player);
        const int currentHCP = WorldHighCardPoints(current, constraint.player) +
          WorldHighCardPoints(current, partner);
        if (constraint.kind == CONSTRAINT_PARTNERSHIP_MAX_HCP)
        {
          if (currentHCP > constraint.count)
            return false;
          continue;
        }

        const int remainingHCP =
          PotentialRemainingHighCardPointsForSeat(hiddenCards, nextIndex,
            constraint.player) +
          PotentialRemainingHighCardPointsForSeat(hiddenCards, nextIndex,
            partner);
        if (currentHCP + remainingHCP < constraint.count)
          return false;
        continue;
      }

      const int currentHCP = WorldHighCardPoints(current, constraint.player);
      if (constraint.kind == CONSTRAINT_MAX_HCP)
      {
        if (currentHCP > constraint.count)
          return false;
        continue;
      }

      const int remainingHCP = PotentialRemainingHighCardPointsForSeat(hiddenCards,
        nextIndex, constraint.player);
      if (currentHCP + remainingHCP < constraint.count)
        return false;
    }

    return true;
  }

  bool PartialSeatCanStillReachBalancedShape(
    const ParsedWorld& current,
    const vector<HiddenCardCandidate>& hiddenCards,
    const unsigned nextIndex,
    const int seat,
    const int targetCount,
    const int handType)
  {
    if (targetCount != 13)
      return true;

    int currentLengths[4];
    int maxLengths[4];
    for (int suit = 0; suit < 4; suit++)
    {
      currentLengths[suit] = WorldSuitLength(current, seat, suit);
      maxLengths[suit] = currentLengths[suit] +
        PotentialRemainingSuitCardsForSeat(hiddenCards, nextIndex, seat, suit);
    }

    for (int suit0 = currentLengths[0]; suit0 <= maxLengths[0]; suit0++)
    {
      for (int suit1 = currentLengths[1]; suit1 <= maxLengths[1]; suit1++)
      {
        for (int suit2 = currentLengths[2]; suit2 <= maxLengths[2]; suit2++)
        {
          const int suit3 = 13 - suit0 - suit1 - suit2;
          if (suit3 < currentLengths[3] || suit3 > maxLengths[3])
            continue;

          const int lengths[4] = { suit0, suit1, suit2, suit3 };
          if (LengthsMatchHandType(lengths, 13, handType))
            return true;
        }
      }
    }

    return false;
  }

  bool ConstructorBalancedConstraintsPossible(
    const vector<int>& hiddenSeats,
    const vector<WorldConstraint>& constraints,
    const ParsedWorld& current,
    const vector<HiddenCardCandidate>& hiddenCards,
    const int finalSeatCounts[4],
    const unsigned nextIndex)
  {
    for (unsigned i = 0; i < constraints.size(); i++)
    {
      const WorldConstraint& constraint = constraints[i];
      if (! VectorContainsSeat(hiddenSeats, constraint.player))
        continue;

      if (constraint.kind != CONSTRAINT_BALANCED &&
          constraint.kind != CONSTRAINT_HAND_TYPE)
        continue;

      if (! PartialSeatCanStillReachBalancedShape(current, hiddenCards,
            nextIndex, constraint.player, finalSeatCounts[constraint.player],
            ConstraintHandType(constraint)))
      {
        return false;
      }
    }

    return true;
  }

  bool ConstructorBiddingConstraintsPossible(
    const vector<int>& hiddenSeats,
    const vector<WorldConstraint>& constraints,
    const ParsedWorld& current,
    const vector<HiddenCardCandidate>& hiddenCards,
    const int finalSeatCounts[4],
    const unsigned nextIndex)
  {
    return ConstructorLengthConstraintsPossible(hiddenSeats, constraints, current,
      hiddenCards, nextIndex) &&
      ConstructorHCPConstraintsPossible(hiddenSeats, constraints, current,
        hiddenCards, nextIndex) &&
      ConstructorBalancedConstraintsPossible(hiddenSeats, constraints, current,
        hiddenCards, finalSeatCounts, nextIndex);
  }

  bool CanAssignHiddenCard(
    const HiddenCardCandidate& candidate,
    const int seat,
    const int targetCounts[4],
    const int assignedCounts[4])
  {
    if ((candidate.allowedSeatsMask & SeatBit(seat)) == 0U)
      return false;

    if (assignedCounts[seat] >= targetCounts[seat])
      return false;

    return true;
  }

  void ConstructHistoryDerivedWorldsRec(
    const vector<int>& hiddenSeats,
    const vector<HiddenCardCandidate>& hiddenCards,
    const vector<WorldConstraint>& constructorConstraints,
    const int targetCounts[4],
    const int finalSeatCounts[4],
    int assignedCounts[4],
    ParsedWorld& current,
    const unsigned index,
    vector<ParsedWorld>& worlds)
  {
    if (index == hiddenCards.size())
    {
      for (unsigned i = 0; i < hiddenSeats.size(); i++)
      {
        if (assignedCounts[hiddenSeats[i]] != targetCounts[hiddenSeats[i]])
          return;
      }

      ParsedWorld world(current);
      CanonicalizeWorld(world);
      worlds.push_back(world);
      return;
    }

    const HiddenCardCandidate& candidate = hiddenCards[index];
    for (unsigned i = 0; i < hiddenSeats.size(); i++)
    {
      const int seat = hiddenSeats[i];
      if (! CanAssignHiddenCard(candidate, seat, targetCounts, assignedCounts))
        continue;

      current.suits[seat][candidate.card.suit].push_back(candidate.card.rank);
      assignedCounts[seat]++;

      if (ConstructorBiddingConstraintsPossible(hiddenSeats, constructorConstraints,
            current, hiddenCards, finalSeatCounts, index + 1))
      {
        ConstructHistoryDerivedWorldsRec(hiddenSeats, hiddenCards,
          constructorConstraints, targetCounts, finalSeatCounts, assignedCounts,
          current, index + 1, worlds);
      }

      assignedCounts[seat]--;
      current.suits[seat][candidate.card.suit].erase(
        current.suits[seat][candidate.card.suit].size() - 1U, 1U);
    }
  }

  void EnumerateHistoryDerivedWorldsRec(
    const vector<int>& hiddenSeats,
    const vector<HiddenCardCandidate>& hiddenCards,
    const int targetCounts[4],
    int assignedCounts[4],
    ParsedWorld& current,
    const unsigned index,
    vector<ParsedWorld>& worlds,
    const unsigned worldCap)
  {
    if (worldCap > 0U && worlds.size() >= worldCap)
      return;

    if (index == hiddenCards.size())
    {
      for (unsigned i = 0; i < hiddenSeats.size(); i++)
      {
        if (assignedCounts[hiddenSeats[i]] != targetCounts[hiddenSeats[i]])
          return;
      }

      ParsedWorld world(current);
      CanonicalizeWorld(world);
      worlds.push_back(world);
      return;
    }

    const HiddenCardCandidate& candidate = hiddenCards[index];
    for (unsigned i = 0; i < hiddenSeats.size(); i++)
    {
      const int seat = hiddenSeats[i];
      if (! CanAssignHiddenCard(candidate, seat, targetCounts, assignedCounts))
        continue;

      current.suits[seat][candidate.card.suit].push_back(candidate.card.rank);
      assignedCounts[seat]++;
      EnumerateHistoryDerivedWorldsRec(hiddenSeats, hiddenCards, targetCounts,
        assignedCounts, current, index + 1U, worlds, worldCap);
      assignedCounts[seat]--;
      current.suits[seat][candidate.card.suit].erase(
        current.suits[seat][candidate.card.suit].size() - 1U, 1U);
    }
  }

  vector<ParsedWorld> EnumerateHistoryDerivedWorlds(
    const vector<int>& hiddenSeats,
    const vector<HiddenCardCandidate>& hiddenCards,
    const int targetCounts[4],
    const ParsedWorld& visibleSeedWorld,
    const unsigned worldCap)
  {
    vector<ParsedWorld> worlds;
    ParsedWorld current(visibleSeedWorld);
    int assignedCounts[4];
    memset(assignedCounts, 0, sizeof(assignedCounts));
    EnumerateHistoryDerivedWorldsRec(hiddenSeats, hiddenCards, targetCounts,
      assignedCounts, current, 0U, worlds, worldCap);
    SortConstructedWorlds(worlds);
    return worlds;
  }

  bool WorldMatchesConstructorLengthConstraints(
    const ParsedWorld& world,
    const vector<int>& hiddenSeats,
    const vector<WorldConstraint>& constraints)
  {
    for (unsigned i = 0; i < constraints.size(); i++)
    {
      const WorldConstraint& constraint = constraints[i];
      if (! ConstructorConstraintTouchesHiddenSeats(hiddenSeats, constraint))
        continue;

      if (constraint.kind != CONSTRAINT_VOID_SUIT &&
          constraint.kind != CONSTRAINT_MIN_LENGTH &&
          constraint.kind != CONSTRAINT_MAX_LENGTH &&
          constraint.kind != CONSTRAINT_PARTNERSHIP_MIN_LENGTH &&
          constraint.kind != CONSTRAINT_PARTNERSHIP_MAX_LENGTH)
      {
        continue;
      }

      if (! WorldMatchesConstraint(world, constraint))
        return false;
    }
    return true;
  }

  bool WorldMatchesConstructorHCPConstraints(
    const ParsedWorld& world,
    const vector<int>& hiddenSeats,
    const vector<WorldConstraint>& constraints)
  {
    for (unsigned i = 0; i < constraints.size(); i++)
    {
      const WorldConstraint& constraint = constraints[i];
      if (! ConstructorConstraintTouchesHiddenSeats(hiddenSeats, constraint))
        continue;

      if (constraint.kind != CONSTRAINT_MIN_HCP &&
          constraint.kind != CONSTRAINT_MAX_HCP &&
          constraint.kind != CONSTRAINT_PARTNERSHIP_MIN_HCP &&
          constraint.kind != CONSTRAINT_PARTNERSHIP_MAX_HCP)
      {
        continue;
      }

      if (! WorldMatchesConstraint(world, constraint))
        return false;
    }
    return true;
  }

  bool WorldMatchesConstructorBalancedConstraints(
    const ParsedWorld& world,
    const vector<int>& hiddenSeats,
    const vector<WorldConstraint>& constraints)
  {
    for (unsigned i = 0; i < constraints.size(); i++)
    {
      const WorldConstraint& constraint = constraints[i];
      if (! ConstructorConstraintTouchesHiddenSeats(hiddenSeats, constraint))
        continue;

      if (constraint.kind != CONSTRAINT_BALANCED &&
          constraint.kind != CONSTRAINT_HAND_TYPE)
        continue;

      if (! WorldMatchesConstraint(world, constraint))
        return false;
    }
    return true;
  }

  string FirstConstructorLengthFailureReason(
    const ParsedWorld& world,
    const vector<int>& hiddenSeats,
    const vector<WorldConstraint>& constraints)
  {
    for (unsigned i = 0; i < constraints.size(); i++)
    {
      const WorldConstraint& constraint = constraints[i];
      if (! ConstructorConstraintTouchesHiddenSeats(hiddenSeats, constraint))
        continue;

      if (constraint.kind != CONSTRAINT_VOID_SUIT &&
          constraint.kind != CONSTRAINT_MIN_LENGTH &&
          constraint.kind != CONSTRAINT_MAX_LENGTH &&
          constraint.kind != CONSTRAINT_PARTNERSHIP_MIN_LENGTH &&
          constraint.kind != CONSTRAINT_PARTNERSHIP_MAX_LENGTH)
      {
        continue;
      }

      if (! WorldMatchesConstraint(world, constraint))
        return ConstraintToString(constraint);
    }
    return "all constructor-local length constraints passed";
  }

  string FirstConstructorHCPFailureReason(
    const ParsedWorld& world,
    const vector<int>& hiddenSeats,
    const vector<WorldConstraint>& constraints)
  {
    for (unsigned i = 0; i < constraints.size(); i++)
    {
      const WorldConstraint& constraint = constraints[i];
      if (! ConstructorConstraintTouchesHiddenSeats(hiddenSeats, constraint))
        continue;

      if (constraint.kind != CONSTRAINT_MIN_HCP &&
          constraint.kind != CONSTRAINT_MAX_HCP &&
          constraint.kind != CONSTRAINT_PARTNERSHIP_MIN_HCP &&
          constraint.kind != CONSTRAINT_PARTNERSHIP_MAX_HCP)
      {
        continue;
      }

      if (! WorldMatchesConstraint(world, constraint))
        return ConstraintToString(constraint);
    }
    return "all constructor-local HCP constraints passed";
  }

  string FirstConstructorBalancedFailureReason(
    const ParsedWorld& world,
    const vector<int>& hiddenSeats,
    const vector<WorldConstraint>& constraints)
  {
    for (unsigned i = 0; i < constraints.size(); i++)
    {
      const WorldConstraint& constraint = constraints[i];
      if (! ConstructorConstraintTouchesHiddenSeats(hiddenSeats, constraint))
        continue;

      if (constraint.kind != CONSTRAINT_BALANCED &&
          constraint.kind != CONSTRAINT_HAND_TYPE)
        continue;

      if (! WorldMatchesConstraint(world, constraint))
        return ConstraintToString(constraint);
    }
    return "all constructor-local balanced-shape constraints passed";
  }

  HistoryDerivedConstructionResult ConstructCandidateWorldsFromHistory(
    const HistoryDerivedWorldSpec& spec,
    const BridgeInformationState& information)
  {
    HistoryDerivedConstructionResult result;
    vector<WorldConstraint> constructorConstraints;
    int targetCounts[4];
    int finalSeatCounts[4];
    PrepareHistoryDerivedConstruction(spec, information, result,
      constructorConstraints, targetCounts, finalSeatCounts);

    ApplyConstructionPlayedCardOwnership(spec.hiddenSeats, information,
      result.hiddenCards);
    ApplyConstructionCardLocationConstraints(spec.hiddenSeats,
      constructorConstraints, result.hiddenCards);
    SortHiddenCardCandidates(result.hiddenCards);

    ParsedWorld current(result.visibleSeedWorld);
    int assignedCounts[4];
    memset(assignedCounts, 0, sizeof(assignedCounts));
    if (ConstructorBiddingConstraintsPossible(spec.hiddenSeats,
          constructorConstraints, current, result.hiddenCards,
          finalSeatCounts, 0U))
    {
      ConstructHistoryDerivedWorldsRec(spec.hiddenSeats, result.hiddenCards,
        constructorConstraints, targetCounts, finalSeatCounts, assignedCounts,
        current, 0U, result.worlds);
    }
    SortConstructedWorlds(result.worlds);
    return result;
  }

  HistoryDerivedConstructionExplanation ExplainHistoryDerivedConstruction(
    const HistoryDerivedWorldSpec& spec,
    const BridgeInformationState& information)
  {
    HistoryDerivedConstructionExplanation explanation;
    HistoryDerivedConstructionResult prepared;
    int targetCounts[4];
    int finalSeatCounts[4];
    PrepareHistoryDerivedConstruction(spec, information, prepared,
      explanation.constructorConstraints, targetCounts, finalSeatCounts);

    SortHiddenCardCandidates(prepared.hiddenCards);
    const unsigned explanationCap = 10000U;
    const vector<ParsedWorld> rawWorlds = EnumerateHistoryDerivedWorlds(
      spec.hiddenSeats, prepared.hiddenCards, targetCounts,
      prepared.visibleSeedWorld, explanationCap);
    explanation.stats.rawAssignmentCount = static_cast<unsigned>(rawWorlds.size());

    vector<HiddenCardCandidate> ownershipHiddenCards = prepared.hiddenCards;
    ApplyConstructionPlayedCardOwnership(spec.hiddenSeats, information,
      ownershipHiddenCards);
    SortHiddenCardCandidates(ownershipHiddenCards);
    const vector<ParsedWorld> ownershipWorlds = EnumerateHistoryDerivedWorlds(
      spec.hiddenSeats, ownershipHiddenCards, targetCounts,
      prepared.visibleSeedWorld, explanationCap);
    explanation.stats.afterOwnershipCount = static_cast<unsigned>(ownershipWorlds.size());

    vector<HiddenCardCandidate> cardLocationHiddenCards = ownershipHiddenCards;
    ApplyConstructionCardLocationConstraints(spec.hiddenSeats,
      explanation.constructorConstraints, cardLocationHiddenCards);
    SortHiddenCardCandidates(cardLocationHiddenCards);
    const vector<ParsedWorld> cardLocationWorlds = EnumerateHistoryDerivedWorlds(
      spec.hiddenSeats, cardLocationHiddenCards, targetCounts,
      prepared.visibleSeedWorld, explanationCap);
    explanation.stats.afterCardLocationCount =
      static_cast<unsigned>(cardLocationWorlds.size());

    unsigned lengthAcceptedWorlds = 0U;
    unsigned hcpAcceptedWorlds = 0U;
    unsigned balancedAcceptedWorlds = 0U;
    unsigned acceptedWorlds = 0U;
    for (unsigned i = 0; i < cardLocationWorlds.size(); i++)
    {
      WorldExplanation world;
      world.worldIndex = i;
      world.serializedWorld = SerializePBNWorld(cardLocationWorlds[i]);

      if (! WorldMatchesConstructorLengthConstraints(cardLocationWorlds[i],
            spec.hiddenSeats, explanation.constructorConstraints))
      {
        AddWorldExplanationStep(world, "constructor_length", false,
          FirstConstructorLengthFailureReason(cardLocationWorlds[i],
            spec.hiddenSeats, explanation.constructorConstraints));
        explanation.worlds.push_back(world);
        continue;
      }
      lengthAcceptedWorlds++;
      AddWorldExplanationStep(world, "constructor_length", true,
        "passed constructor-local length constraints");

      if (! WorldMatchesConstructorHCPConstraints(cardLocationWorlds[i],
            spec.hiddenSeats, explanation.constructorConstraints))
      {
        AddWorldExplanationStep(world, "constructor_hcp", false,
          FirstConstructorHCPFailureReason(cardLocationWorlds[i],
            spec.hiddenSeats, explanation.constructorConstraints));
        explanation.worlds.push_back(world);
        continue;
      }
      hcpAcceptedWorlds++;
      AddWorldExplanationStep(world, "constructor_hcp", true,
        "passed constructor-local HCP constraints");

      if (! WorldMatchesConstructorBalancedConstraints(cardLocationWorlds[i],
            spec.hiddenSeats, explanation.constructorConstraints))
      {
        AddWorldExplanationStep(world, "constructor_balanced", false,
          FirstConstructorBalancedFailureReason(cardLocationWorlds[i],
            spec.hiddenSeats, explanation.constructorConstraints));
        explanation.worlds.push_back(world);
        continue;
      }
      balancedAcceptedWorlds++;
      AddWorldExplanationStep(world, "constructor_balanced", true,
        "passed constructor-local balanced-shape constraints");

      world.accepted = true;
      explanation.worlds.push_back(world);
      acceptedWorlds++;
    }

    explanation.stats.afterConstructorLengthCount = lengthAcceptedWorlds;
    explanation.stats.afterConstructorHCPCount = hcpAcceptedWorlds;
    explanation.stats.afterConstructorBalancedCount = balancedAcceptedWorlds;
    explanation.stats.afterConstructorConstraintCount = acceptedWorlds;
    explanation.stats.finalWorldCount = acceptedWorlds;
    return explanation;
  }

  bool WorldMatchesConstraint(
    const ParsedWorld& world,
    const WorldConstraint& constraint)
  {
    switch (constraint.kind)
    {
      case CONSTRAINT_HAS_CARD:
        return WorldHasCard(world, constraint.player, constraint.suit,
          constraint.rank);

      case CONSTRAINT_NOT_HAS_CARD:
        return ! WorldHasCard(world, constraint.player, constraint.suit,
          constraint.rank);

      case CONSTRAINT_VOID_SUIT:
        return WorldSuitLength(world, constraint.player, constraint.suit) == 0;

      case CONSTRAINT_MIN_LENGTH:
        return WorldSuitLength(world, constraint.player, constraint.suit) >=
          constraint.count;

      case CONSTRAINT_MAX_LENGTH:
        return WorldSuitLength(world, constraint.player, constraint.suit) <=
          constraint.count;

      case CONSTRAINT_MIN_HCP:
        return WorldHighCardPoints(world, constraint.player) >=
          constraint.count;

      case CONSTRAINT_MAX_HCP:
        return WorldHighCardPoints(world, constraint.player) <=
          constraint.count;

      case CONSTRAINT_BALANCED:
        return WorldHasHandType(world, constraint.player, HAND_TYPE_BALANCED);

      case CONSTRAINT_HAND_TYPE:
        return WorldHasHandType(world, constraint.player, constraint.count);

      case CONSTRAINT_PARTNERSHIP_MIN_LENGTH:
        return WorldSuitLength(world, constraint.player, constraint.suit) +
          WorldSuitLength(world, PartnerSeat(constraint.player),
            constraint.suit) >= constraint.count;

      case CONSTRAINT_PARTNERSHIP_MAX_LENGTH:
        return WorldSuitLength(world, constraint.player, constraint.suit) +
          WorldSuitLength(world, PartnerSeat(constraint.player),
            constraint.suit) <= constraint.count;

      case CONSTRAINT_PARTNERSHIP_MIN_HCP:
        return WorldHighCardPoints(world, constraint.player) +
          WorldHighCardPoints(world, PartnerSeat(constraint.player)) >=
          constraint.count;

      case CONSTRAINT_PARTNERSHIP_MAX_HCP:
        return WorldHighCardPoints(world, constraint.player) +
          WorldHighCardPoints(world, PartnerSeat(constraint.player)) <=
          constraint.count;

      default:
        throw runtime_error("Unknown world constraint kind");
    }
  }

  bool WorldMatchesAllConstraints(
    const ParsedWorld& world,
    const vector<WorldConstraint>& constraints)
  {
    for (unsigned i = 0; i < constraints.size(); i++)
    {
      if (! WorldMatchesConstraint(world, constraints[i]))
        return false;
    }
    return true;
  }

  WorldMask FilterWorldsByConstraints(
    const vector<ParsedWorld>& worlds,
    const WorldMask& candidates,
    const vector<WorldConstraint>& constraints)
  {
    if (constraints.empty())
      return candidates;

    WorldMask mask = WorldMask::None(candidates.count);
    for (unsigned i = 0; i < worlds.size(); i++)
    {
      if (! candidates.Has(i))
        continue;

      if (WorldMatchesAllConstraints(worlds[i], constraints))
        mask.bits |= (1ULL << i);
    }
    return mask;
  }

  bool WorldCanReplayHistory(
    const ParsedWorld& world,
    const vector<PlayHistoryEvent>& history)
  {
    return CheckWorldReplayHistory(world, history, "history").ok;
  }

  WorldMask FilterWorldsByHistory(
    const vector<ParsedWorld>& worlds,
    const WorldMask& candidates,
    const vector<PlayHistoryEvent>& history)
  {
    if (history.empty())
      return candidates;

    WorldMask mask = WorldMask::None(candidates.count);
    for (unsigned i = 0; i < worlds.size(); i++)
    {
      if (! candidates.Has(i))
        continue;

      if (WorldCanReplayHistory(worlds[i], history))
        mask.bits |= (1ULL << i);
    }
    return mask;
  }

  WorldMask FilterWorldsByHistoryAfterHistory(
    const vector<ParsedWorld>& worlds,
    const WorldMask& candidates,
    const vector<PlayHistoryEvent>& priorHistory,
    const vector<PlayHistoryEvent>& history)
  {
    if (history.empty())
      return candidates;

    WorldMask mask = WorldMask::None(candidates.count);
    for (unsigned i = 0; i < worlds.size(); i++)
    {
      if (! candidates.Has(i))
        continue;

      if (CheckWorldReplayHistoryAfterHistory(worlds[i], priorHistory, history,
            "history").ok)
      {
        mask.bits |= (1ULL << i);
      }
    }
    return mask;
  }

  WorldMask DeduplicateWorldMask(
    const vector<ParsedWorld>& worlds,
    const WorldMask& candidates,
    unsigned& duplicatesRemoved)
  {
    map<string, unsigned> firstSeen;
    WorldMask deduped = WorldMask::None(candidates.count);
    duplicatesRemoved = 0;

    for (unsigned i = 0; i < worlds.size(); i++)
    {
      if (! candidates.Has(i))
        continue;

      const string key = SerializePBNWorld(worlds[i]);
      if (firstSeen.find(key) != firstSeen.end())
      {
        duplicatesRemoved++;
        continue;
      }

      firstSeen[key] = i;
      deduped.bits |= (1ULL << i);
    }

    return deduped;
  }

  WorldMask SampleWorldMaskDeterministically(
    const vector<ParsedWorld>& worlds,
    const WorldMask& candidates,
    const unsigned sampleLimit,
    const unsigned samplingSeed,
    unsigned& sampledOutWorlds)
  {
    sampledOutWorlds = 0;
    if (sampleLimit == 0)
      return candidates;

    vector<unsigned> active;
    for (unsigned i = 0; i < worlds.size(); i++)
    {
      if (candidates.Has(i))
        active.push_back(i);
    }

    if (active.size() <= sampleLimit)
      return candidates;

    sort(active.begin(), active.end(),
      [&](const unsigned left, const unsigned right)
      {
        const string leftKey = SerializePBNWorld(worlds[left]);
        const string rightKey = SerializePBNWorld(worlds[right]);
        if (leftKey != rightKey)
          return leftKey < rightKey;
        return left < right;
      });

    WorldMask sampled = WorldMask::None(candidates.count);
    const unsigned offset = samplingSeed % static_cast<unsigned>(active.size());
    for (unsigned i = 0; i < sampleLimit; i++)
    {
      const unsigned picked = active[(offset + i) % active.size()];
      sampled.bits |= (1ULL << picked);
    }

    sampledOutWorlds = static_cast<unsigned>(active.size()) - sampleLimit;
    return sampled;
  }

  int EvaluateWorldPlausibility(
    const ParsedWorld& world,
    const BridgeInformationState& information,
    vector<string>* satisfiedLabels,
    vector<string>* unsatisfiedLabels)
  {
    int score = 0;
    for (unsigned i = 0; i < information.plausibilityHints.size(); i++)
    {
      const WorldPlausibilityHint& hint = information.plausibilityHints[i];
      const string label = PlausibilityHintLabel(hint);
      if (WorldMatchesConstraint(world, hint.constraint))
      {
        score += hint.weight;
        if (satisfiedLabels != NULL)
          satisfiedLabels->push_back(label);
      }
      else if (unsatisfiedLabels != NULL)
        unsatisfiedLabels->push_back(label);
    }
    return score;
  }

  namespace
  {
    struct WorldPipelineState
    {
      vector<char> knownCards;
      vector<char> bidding;
      vector<char> followSuit;
      vector<char> playHistory;
      vector<char> currentTrick;
      vector<char> deduplication;
      vector<char> finalSelection;
      map<string, unsigned> firstSeen;
      vector<WorldConstraint> appliedFollowSuitConstraints;
    };

    static unsigned CountAccepted(const vector<char>& accepted)
    {
      unsigned count = 0;
      for (unsigned i = 0; i < accepted.size(); i++)
      {
        if (accepted[i])
          count++;
      }
      return count;
    }

    static vector<unsigned> AcceptedIndices(const vector<char>& accepted)
    {
      vector<unsigned> indices;
      for (unsigned i = 0; i < accepted.size(); i++)
      {
        if (accepted[i])
          indices.push_back(i);
      }
      return indices;
    }

    static vector<unsigned> RankWorldIndicesByPlausibility(
      const vector<ParsedWorld>& worlds,
      const vector<unsigned>& indices,
      const BridgeInformationState& information)
    {
      vector<unsigned> ranked(indices);
      stable_sort(ranked.begin(), ranked.end(),
        [&](const unsigned left, const unsigned right)
        {
          const int leftScore = EvaluateWorldPlausibility(worlds[left], information,
            NULL, NULL);
          const int rightScore = EvaluateWorldPlausibility(worlds[right], information,
            NULL, NULL);
          if (leftScore != rightScore)
            return leftScore > rightScore;

          const string leftKey = SerializePBNWorld(worlds[left]);
          const string rightKey = SerializePBNWorld(worlds[right]);
          if (leftKey != rightKey)
            return leftKey < rightKey;
          return left < right;
        });
      return ranked;
    }

    static WorldMask MakeWorldMaskFromIndices(
      const unsigned worldCount,
      const vector<unsigned>& indices)
    {
      WorldMask mask = WorldMask::None(worldCount);
      for (unsigned i = 0; i < indices.size(); i++)
      {
        Check(indices[i] < worldCount,
          "world-mask conversion should not reference a world outside the candidate vector");
        mask.bits |= (1ULL << indices[i]);
      }
      return mask;
    }

    static WorldPipelineState EvaluateWorldPipeline(
      const vector<ParsedWorld>& worlds,
      const BridgeInformationState& information,
      WorldGenerationStats* stats)
    {
      WorldPipelineState pipeline;
      const unsigned worldCount = static_cast<unsigned>(worlds.size());
      pipeline.knownCards.assign(worldCount, 0);
      pipeline.bidding.assign(worldCount, 0);
      pipeline.followSuit.assign(worldCount, 0);
      pipeline.playHistory.assign(worldCount, 0);
      pipeline.currentTrick.assign(worldCount, 0);
      pipeline.deduplication.assign(worldCount, 0);
      pipeline.finalSelection.assign(worldCount, 0);
      pipeline.appliedFollowSuitConstraints =
        CollectFollowSuitConstraints(information);

      if (stats != NULL)
      {
        *stats = WorldGenerationStats();
        stats->candidateWorldCount = worldCount;
      }

      for (unsigned i = 0; i < worldCount; i++)
      {
        if (WorldMatchesAllConstraints(worlds[i], information.knownCardConstraints))
          pipeline.knownCards[i] = 1;
      }
      if (stats != NULL)
        stats->afterKnownCardCount = CountAccepted(pipeline.knownCards);

      for (unsigned i = 0; i < worldCount; i++)
      {
        if (pipeline.knownCards[i] &&
            WorldMatchesAllConstraints(worlds[i], information.biddingConstraints))
        {
          pipeline.bidding[i] = 1;
        }
      }
      if (stats != NULL)
        stats->afterBiddingCount = CountAccepted(pipeline.bidding);

      for (unsigned i = 0; i < worldCount; i++)
      {
        if (pipeline.bidding[i] &&
            WorldMatchesAllConstraints(worlds[i],
              pipeline.appliedFollowSuitConstraints))
        {
          pipeline.followSuit[i] = 1;
        }
      }
      if (stats != NULL)
        stats->afterFollowSuitCount = CountAccepted(pipeline.followSuit);

      for (unsigned i = 0; i < worldCount; i++)
      {
        if (pipeline.followSuit[i] &&
            CheckWorldReplayHistory(worlds[i], information.playHistory,
              "history").ok)
        {
          pipeline.playHistory[i] = 1;
        }
      }
      if (stats != NULL)
        stats->afterPlayHistoryCount = CountAccepted(pipeline.playHistory);

      for (unsigned i = 0; i < worldCount; i++)
      {
        if (pipeline.playHistory[i] &&
            CheckWorldReplayHistoryAfterHistory(worlds[i], information.playHistory,
              information.currentTrickHistory, "history").ok)
        {
          pipeline.currentTrick[i] = 1;
        }
      }
      if (stats != NULL)
        stats->afterCurrentTrickCount = CountAccepted(pipeline.currentTrick);

      if (information.deduplicateEquivalentWorlds)
      {
        for (unsigned i = 0; i < worldCount; i++)
        {
          if (! pipeline.currentTrick[i])
            continue;

          const string key = SerializePBNWorld(worlds[i]);
          if (pipeline.firstSeen.find(key) != pipeline.firstSeen.end())
            continue;

          pipeline.firstSeen[key] = i;
          pipeline.deduplication[i] = 1;
        }
      }
      else
      {
        for (unsigned i = 0; i < worldCount; i++)
          pipeline.deduplication[i] = pipeline.currentTrick[i];
      }

      if (stats != NULL)
      {
        stats->duplicateWorldsRemoved = CountAccepted(pipeline.currentTrick) -
          CountAccepted(pipeline.deduplication);
      }

      vector<unsigned> selected = AcceptedIndices(pipeline.deduplication);
      const unsigned effectiveSampleLimit =
        (information.sampleLimit == 0 ? 64U : min(64U, information.sampleLimit));
      if (selected.size() > effectiveSampleLimit)
      {
        sort(selected.begin(), selected.end(),
          [&](const unsigned left, const unsigned right)
          {
            const string leftKey = SerializePBNWorld(worlds[left]);
            const string rightKey = SerializePBNWorld(worlds[right]);
            if (leftKey != rightKey)
              return leftKey < rightKey;
            return left < right;
          });

        vector<unsigned> sampled;
        const unsigned offset = information.samplingSeed %
          static_cast<unsigned>(selected.size());
        for (unsigned i = 0; i < effectiveSampleLimit; i++)
          sampled.push_back(selected[(offset + i) % selected.size()]);
        sort(sampled.begin(), sampled.end());
        selected.swap(sampled);
      }

      for (unsigned i = 0; i < selected.size(); i++)
        pipeline.finalSelection[selected[i]] = 1;

      if (stats != NULL)
      {
        stats->sampledOutWorlds = CountAccepted(pipeline.deduplication) -
          CountAccepted(pipeline.finalSelection);
        stats->afterSamplingCount = CountAccepted(pipeline.finalSelection);
        stats->finalWorldCount = CountAccepted(pipeline.finalSelection);
      }

      return pipeline;
    }
  }

  vector<unsigned> RankWorldsByPlausibility(
    const vector<ParsedWorld>& worlds,
    const WorldMask& candidates,
    const BridgeInformationState& information)
  {
    vector<unsigned> ranked;
    for (unsigned i = 0; i < worlds.size(); i++)
    {
      if (candidates.Has(i))
        ranked.push_back(i);
    }

    return RankWorldIndicesByPlausibility(worlds, ranked, information);
  }

  DecisionWorldPipelineResult BuildDecisionWorldPipeline(
    const vector<ParsedWorld>& candidateWorlds,
    const BridgeInformationState& information)
  {
    DecisionWorldPipelineResult result;
    result.candidateWorlds = candidateWorlds;

    const WorldPipelineState pipeline = EvaluateWorldPipeline(candidateWorlds,
      information, &result.stats);
    result.activeWorldIndices = AcceptedIndices(pipeline.finalSelection);
    result.activeWorlds.reserve(result.activeWorldIndices.size());
    for (unsigned i = 0; i < result.activeWorldIndices.size(); i++)
      result.activeWorlds.push_back(candidateWorlds[result.activeWorldIndices[i]]);

    result.explanation.appliedFollowSuitConstraints =
      pipeline.appliedFollowSuitConstraints;
    result.explanation.finalWorldIndices = result.activeWorldIndices;
    if (candidateWorlds.size() <= 64U)
    {
      result.explanation.finalWorldMask = MakeWorldMaskFromIndices(
        static_cast<unsigned>(candidateWorlds.size()), result.activeWorldIndices);
    }
    else
    {
      result.explanation.finalWorldMask = WorldMask::All(static_cast<unsigned>(
        result.activeWorldIndices.size()));
    }
    result.explanation.plausibilityRankedWorldIndices =
      RankWorldIndicesByPlausibility(candidateWorlds,
        result.activeWorldIndices, information);

    for (unsigned i = 0; i < candidateWorlds.size(); i++)
    {
      WorldExplanation world;
      world.worldIndex = i;
      world.serializedWorld = SerializePBNWorld(candidateWorlds[i]);
      for (unsigned h = 0; h < information.plausibilityHints.size(); h++)
        world.plausibilityMaxScore += information.plausibilityHints[h].weight;
      world.plausibilityScore = EvaluateWorldPlausibility(candidateWorlds[i],
        information, &world.satisfiedPlausibilityHints,
        &world.unsatisfiedPlausibilityHints);

      if (! pipeline.knownCards[i])
      {
        world.accepted = false;
        AddWorldExplanationStep(world, "known_cards", false,
          FirstConstraintFailureReason(candidateWorlds[i],
            information.knownCardConstraints));
        result.explanation.worlds.push_back(world);
        continue;
      }
      AddWorldExplanationStep(world, "known_cards", true,
        "passed known-card constraints");

      if (! pipeline.bidding[i])
      {
        world.accepted = false;
        AddWorldExplanationStep(world, "bidding", false,
          FirstConstraintFailureReason(candidateWorlds[i],
            information.biddingConstraints));
        result.explanation.worlds.push_back(world);
        continue;
      }
      AddWorldExplanationStep(world, "bidding", true,
        "passed bidding constraints");

      if (! pipeline.followSuit[i])
      {
        world.accepted = false;
        AddWorldExplanationStep(world, "follow_suit", false,
          FirstConstraintFailureReason(candidateWorlds[i],
            result.explanation.appliedFollowSuitConstraints));
        result.explanation.worlds.push_back(world);
        continue;
      }
      AddWorldExplanationStep(world, "follow_suit", true,
        (result.explanation.appliedFollowSuitConstraints.empty() ?
          "no explicit follow-suit implications" :
          "passed explicit follow-suit implications"));

      if (! pipeline.playHistory[i])
      {
        world.accepted = false;
        AddWorldExplanationStep(world, "play_history", false,
          CheckWorldReplayHistory(candidateWorlds[i], information.playHistory,
            "play-history").reason);
        result.explanation.worlds.push_back(world);
        continue;
      }
      AddWorldExplanationStep(world, "play_history", true,
        "replayed prior tricks legally");

      if (! pipeline.currentTrick[i])
      {
        world.accepted = false;
        AddWorldExplanationStep(world, "current_trick", false,
          CheckWorldReplayHistoryAfterHistory(candidateWorlds[i],
            information.playHistory, information.currentTrickHistory,
            "current-trick").reason);
        result.explanation.worlds.push_back(world);
        continue;
      }
      AddWorldExplanationStep(world, "current_trick", true,
        "replayed the current partial trick legally");

      if (information.deduplicateEquivalentWorlds && ! pipeline.deduplication[i])
      {
        world.accepted = false;
        const string key = SerializePBNWorld(candidateWorlds[i]);
        const unsigned first = pipeline.firstSeen.find(key)->second;
        ostringstream oss;
        oss << "duplicate of surviving world " << first;
        AddWorldExplanationStep(world, "deduplication", false, oss.str());
        result.explanation.worlds.push_back(world);
        continue;
      }
      if (information.deduplicateEquivalentWorlds)
        AddWorldExplanationStep(world, "deduplication", true,
          "kept as the canonical surviving world");

      if (! pipeline.finalSelection[i])
      {
        world.accepted = false;
        ostringstream oss;
        oss << "deterministic sampling seed " << information.samplingSeed
            << " skipped this world after canonical ordering";
        AddWorldExplanationStep(world, "sampling", false, oss.str());
        result.explanation.worlds.push_back(world);
        continue;
      }

      if (information.sampleLimit != 0)
        AddWorldExplanationStep(world, "sampling", true,
          "retained by deterministic sampling");

      world.accepted = true;
      result.explanation.worlds.push_back(world);
    }

    return result;
  }

  WorldMask GeneratePossibleWorlds(
    const vector<ParsedWorld>& worlds,
    const BridgeInformationState& information,
    WorldGenerationStats* stats)
  {
#ifndef NDEBUG
    DebugCheckWorldMaskCapacity(static_cast<unsigned>(worlds.size()),
      "GeneratePossibleWorlds");
#endif
    const DecisionWorldPipelineResult pipeline = BuildDecisionWorldPipeline(
      worlds, information);
    if (stats != NULL)
      *stats = pipeline.stats;
    return MakeWorldMaskFromIndices(static_cast<unsigned>(worlds.size()),
      pipeline.activeWorldIndices);
  }

  WorldGenerationExplanation ExplainPossibleWorldGeneration(
    const vector<ParsedWorld>& worlds,
    const BridgeInformationState& information)
  {
    return BuildDecisionWorldPipeline(worlds, information).explanation;
  }

  WorldMask GeneratePossibleWorlds(
    const vector<ParsedWorld>& worlds,
    const vector<WorldConstraint>& constraints)
  {
    BridgeInformationState information;
    information.knownCardConstraints = constraints;
    return GeneratePossibleWorlds(worlds, information, NULL);
  }

  HistoryDerivedWorldSpec BuildWorldSpecFromDeal(
    const dealPBN& fullDeal,
    const int declarerSeat,
    const vector<PlayHistoryEvent>& playedCards)
  {
    HistoryDerivedWorldSpec spec;
    const ParsedWorld fullWorld = ParsePBNWorld(fullDeal.remainCards);

    const int dummySeat = (declarerSeat + 2) % 4;
    const int lho = (declarerSeat + 1) % 4;
    const int rho = (declarerSeat + 3) % 4;

    set<pair<int,char> > allPlayed;
    for (unsigned i = 0; i < playedCards.size(); i++)
      allPlayed.insert(make_pair(playedCards[i].move.suit,
                                 playedCards[i].move.rank));

    ParsedWorld seedWorld;
    for (int s = 0; s < 4; s++)
    {
      for (int visSeat = 0; visSeat < 2; visSeat++)
      {
        const int seat = (visSeat == 0) ? declarerSeat : dummySeat;
        seedWorld.suits[seat][s] = "";
        const string& orig = fullWorld.suits[seat][s];
        for (unsigned c = 0; c < orig.size(); c++)
        {
          if (allPlayed.find(make_pair(s, orig[c])) == allPlayed.end())
            seedWorld.suits[seat][s] += orig[c];
        }
      }
      seedWorld.suits[lho][s] = "";
      seedWorld.suits[rho][s] = "";
    }

    spec.hiddenSeats.push_back(lho);
    spec.hiddenSeats.push_back(rho);
    spec.inferHiddenCardsFromVisibleHands = false;

    vector<BridgeMove> hiddenPool;
    const string ranks = "AKQJT98765432";
    for (int suit = 0; suit < 4; suit++)
    {
      for (unsigned r = 0; r < ranks.size(); r++)
      {
        const char rank = ranks[r];
        if (allPlayed.find(make_pair(suit, rank)) != allPlayed.end())
          continue;
        if (FindCardSeatInWorld(seedWorld, suit, rank) >= 0)
          continue;
        hiddenPool.push_back(BridgeMove(suit, rank));
      }
    }

    int cardsPlayedBySeat[4] = {0, 0, 0, 0};
    for (unsigned i = 0; i < playedCards.size(); i++)
      cardsPlayedBySeat[playedCards[i].player]++;

    unsigned idx = 0;
    for (unsigned h = 0; h < spec.hiddenSeats.size(); h++)
    {
      const int seat = spec.hiddenSeats[h];
      const unsigned remaining = static_cast<unsigned>(
        13 - cardsPlayedBySeat[seat]);
      for (unsigned c = 0; c < remaining && idx < hiddenPool.size(); c++, idx++)
        seedWorld.suits[seat][hiddenPool[idx].suit] += hiddenPool[idx].rank;
    }
    spec.seedWorld = seedWorld;

    return spec;
  }


  BridgeInformationState BuildInformationStateFromPlay(
    const dealPBN& fullDeal,
    const int declarerSeat,
    const vector<PlayHistoryEvent>& playedCards,
    const unsigned maxWorlds,
    const unsigned samplingSeed)
  {
    BridgeInformationState info;
    const ParsedWorld fullWorld = ParsePBNWorld(fullDeal.remainCards);
    const int dummySeat = (declarerSeat + 2) % 4;

    set<pair<int,char> > playedByVisible;
    for (unsigned i = 0; i < playedCards.size(); i++)
    {
      const int player = playedCards[i].player;
      if (player == declarerSeat || player == dummySeat)
        playedByVisible.insert(make_pair(
          playedCards[i].move.suit, playedCards[i].move.rank));
    }

    for (int seat = 0; seat < 4; seat++)
    {
      if (seat != declarerSeat && seat != dummySeat)
        continue;
      for (int s = 0; s < 4; s++)
      {
        const string& hand = fullWorld.suits[seat][s];
        for (unsigned c = 0; c < hand.size(); c++)
        {
          if (playedByVisible.find(make_pair(s, hand[c])) ==
              playedByVisible.end())
          {
            info.knownCardConstraints.push_back(
              WorldConstraint::HasCard(seat, s, hand[c]));
          }
        }
      }
    }

    unsigned cardsInCurrentTrick = playedCards.size() % 4;
    unsigned completedCards = playedCards.size() - cardsInCurrentTrick;

    for (unsigned i = 0; i < completedCards; i++)
      info.playHistory.push_back(playedCards[i]);

    for (unsigned i = completedCards; i < playedCards.size(); i++)
      info.currentTrickHistory.push_back(playedCards[i]);

    info.deriveFollowSuitConstraints = true;
    info.deduplicateEquivalentWorlds = true;
    info.sampleLimit = maxWorlds;
    info.samplingSeed = samplingSeed;
    return info;
  }

  BridgeInformationState ApplyInformationOverrides(
    const BridgeInformationState& base,
    const BridgeInformationState& overrides,
    const unsigned maxWorlds,
    const unsigned samplingSeed)
  {
    BridgeInformationState merged(base);
    merged.knownCardConstraints.insert(merged.knownCardConstraints.end(),
      overrides.knownCardConstraints.begin(),
      overrides.knownCardConstraints.end());
    merged.biddingConstraints.insert(merged.biddingConstraints.end(),
      overrides.biddingConstraints.begin(),
      overrides.biddingConstraints.end());
    merged.followSuitConstraints.insert(merged.followSuitConstraints.end(),
      overrides.followSuitConstraints.begin(),
      overrides.followSuitConstraints.end());
    merged.plausibilityHints.insert(merged.plausibilityHints.end(),
      overrides.plausibilityHints.begin(),
      overrides.plausibilityHints.end());
    merged.deriveFollowSuitConstraints =
      overrides.deriveFollowSuitConstraints;
    merged.deduplicateEquivalentWorlds =
      overrides.deduplicateEquivalentWorlds;
    merged.sampleLimit = maxWorlds;
    merged.samplingSeed = samplingSeed;
    return merged;
  }
}

