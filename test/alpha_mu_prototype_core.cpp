/*
  alpha_mu_prototype, an alpha-mu bridge solver

   Copyright © 2026 by David Jenkins
   All rights reserved.
*/

#include "alpha_mu_prototype_core.h"

#include <atomic>
#include <mutex>
#include <thread>

namespace alpha_mu_prototype
{
  using namespace std;

  const char kPrototypeMessagePrefix[] = "alpha_mu_prototype: ";
  const char kAlphaMuPlayHandFile[] = "hands/alpha_mu_play.txt";

  // ========================================================================
  // Zobrist hashing and bridge transposition table implementation
  // ========================================================================

  ZobristTable gZobrist;
  static bool gZobristInitialized = false;

  void InitZobrist()
  {
    if (gZobristInitialized)
      return;
    gZobrist.Init();
    gZobristInitialized = true;
  }

  unsigned long long HashBridgeState(const BridgeState& state)
  {
    InitZobrist();
    unsigned long long h = 0;

    // Hash player to move
    h ^= gZobrist.playerToMove[state.playerToMove & 3];

    // Hash max tricks won
    if (state.maxTricksWon >= 0 &&
        state.maxTricksWon < static_cast<int>(ZobristTable::MAX_TRICKS))
      h ^= gZobrist.maxTricksWon[state.maxTricksWon];

    // Hash trump suit (+1 so notrump=-1 maps to index 0)
    const int trumpIdx = state.trumpSuit + 1;
    if (trumpIdx >= 0 && trumpIdx < 5)
      h ^= gZobrist.trumpSuit[trumpIdx];

    // Hash current trick cards
    for (unsigned t = 0; t < state.currentTrick.size(); t++)
    {
      const int ri = RankCharToIndex(state.currentTrick[t].rank);
      if (ri >= 0)
        h ^= gZobrist.currentTrickCard[t][state.currentTrick[t].suit][ri];
    }

    // Hash world mask
    for (unsigned w = 0; w < state.possibleWorlds.count && w < 64; w++)
    {
      if (state.possibleWorlds.Has(w))
        h ^= gZobrist.worldMaskBit[w];
    }

    // Hash card holdings per world
    for (unsigned w = 0; w < state.worlds.size(); w++)
    {
      if (! state.possibleWorlds.Has(w))
        continue;
      if (w >= ZobristTable::MAX_WORLDS)
        break;

      for (unsigned seat = 0; seat < 4; seat++)
      {
        for (unsigned suit = 0; suit < 4; suit++)
        {
          const string& cards = state.worlds[w].suits[seat][suit];
          for (unsigned c = 0; c < cards.size(); c++)
          {
            const int ri = RankCharToIndex(cards[c]);
            if (ri >= 0)
              h ^= gZobrist.card[w][seat][suit][ri];
          }
        }
      }
    }

    return h;
  }

  BridgeTranspositionTable::BridgeTranspositionTable(unsigned capacityHint)
  {
    // Round up to power of 2
    unsigned cap = 1;
    while (cap < capacityHint)
      cap <<= 1;
    capacity = cap;
    mask = cap - 1;
    stored = 0;
    table.resize(cap);
  }

  void BridgeTranspositionTable::Clear()
  {
    for (unsigned i = 0; i < capacity; i++)
      table[i].occupied = false;
    stored = 0;
  }

  const ParetoFront* BridgeTranspositionTable::Probe(
      unsigned long long hash,
      unsigned long long worldMaskBits) const
  {
    unsigned idx = static_cast<unsigned>(hash) & mask;
    // Linear probing with limited search
    for (unsigned i = 0; i < 4; i++)
    {
      const unsigned slot = (idx + i) & mask;
      const BridgeTTEntry& e = table[slot];
      if (! e.occupied)
        return NULL;
      if (e.hash == hash && e.worldMaskBits == worldMaskBits)
        return &e.front;
    }
    return NULL;
  }

  void BridgeTranspositionTable::Store(
      unsigned long long hash,
      unsigned long long worldMaskBits,
      const ParetoFront& front)
  {
    unsigned idx = static_cast<unsigned>(hash) & mask;
    // Linear probing: find empty or matching slot within 4 steps
    for (unsigned i = 0; i < 4; i++)
    {
      const unsigned slot = (idx + i) & mask;
      BridgeTTEntry& e = table[slot];
      if (! e.occupied)
      {
        e.hash = hash;
        e.worldMaskBits = worldMaskBits;
        e.front = front;
        e.occupied = true;
        stored++;
        return;
      }
      if (e.hash == hash && e.worldMaskBits == worldMaskBits)
      {
        e.front = front;
        return;
      }
    }
    // All 4 slots occupied — replace the first one
    const unsigned slot = idx;
    table[slot].hash = hash;
    table[slot].worldMaskBits = worldMaskBits;
    table[slot].front = front;
  }


void Fail(const string& msg)
  {
    cerr << kPrototypeMessagePrefix << msg << "\n";
    exit(1);
  }
void Check(const bool condition, const string& msg)
  {
    if (! condition)
      Fail(msg);
  }
string AlphaMuParallelModeName(const AlphaMuParallelMode mode)
  {
    switch (mode)
    {
      case ALPHA_MU_PARALLEL_SERIAL:
        return "serial";

      case ALPHA_MU_PARALLEL_BOARD:
        return "board";

      case ALPHA_MU_PARALLEL_ROOT:
        return "root";

      default:
        throw runtime_error("Unknown alpha-mu parallel mode");
    }
  }
AlphaMuParallelMode ParseAlphaMuParallelModeName(const string& text)
  {
    if (text == "serial")
      return ALPHA_MU_PARALLEL_SERIAL;
    else if (text == "board")
      return ALPHA_MU_PARALLEL_BOARD;
    else if (text == "root")
      return ALPHA_MU_PARALLEL_ROOT;

    throw runtime_error("Unknown alpha-mu parallel mode");
  }
SearchExecutionContext MakeSearchExecutionContext(
    const int ddsThreadId,
    BenchmarkBoardProgressContext * benchmarkProgress,
    const AlphaMuParallelMode parallelMode,
    const int boardWorkers,
    const int rootWorkers)
  {
    SearchExecutionContext context;
    context.ddsThreadId = ddsThreadId;
    context.parallelMode = parallelMode;
    context.boardWorkers = boardWorkers;
    context.rootWorkers = rootWorkers;
    context.benchmarkProgress = benchmarkProgress;
    return context;
  }
AlphaMuBenchmarkOptions NormalizeAlphaMuBenchmarkOptions(
    const AlphaMuBenchmarkOptions& options)
  {
    AlphaMuBenchmarkOptions normalized(options);
    if (normalized.boardWorkers < 1)
      normalized.boardWorkers = 1;
    if (normalized.rootWorkers < 1)
      normalized.rootWorkers = 1;
    if (normalized.ddsThreadId < 0)
      normalized.ddsThreadId = 0;
    return normalized;
  }
bool SameOutcome(
    const OutcomeVector& left,
    const OutcomeVector& right)
  {
    return left.valid == right.valid && left.values == right.values;
  }
bool FrontContains(
    const ParetoFront& front,
    const OutcomeVector& target)
  {
    for (unsigned i = 0; i < front.vectors.size(); i++)
    {
      if (SameOutcome(front.vectors[i], target))
        return true;
    }
    return false;
  }
int SeatIndex(const char seat)
  {
    switch (seat)
    {
      case 'N': return SEAT_NORTH;
      case 'E': return SEAT_EAST;
      case 'S': return SEAT_SOUTH;
      case 'W': return SEAT_WEST;
      default:
        throw runtime_error("Unknown seat in PBN world");
    }
  }
vector<string> SplitString(
    const string& text,
    const char delimiter,
    const bool keepEmpty)
  {
    vector<string> parts;
    string current;
    for (unsigned i = 0; i < text.size(); i++)
    {
      if (text[i] == delimiter)
      {
        if (keepEmpty || ! current.empty())
          parts.push_back(current);
        current.clear();
      }
      else
        current.push_back(text[i]);
    }

    if (keepEmpty || ! current.empty())
      parts.push_back(current);
    return parts;
  }
ParsedWorld ParsePBNWorld(const string& pbn)
  {
    const size_t colon = pbn.find(':');
    if (colon == string::npos || colon == 0)
      throw runtime_error("Bad PBN world string");

    const int startSeat = SeatIndex(pbn[0]);
    const vector<string> hands = SplitString(pbn.substr(colon + 1), ' ', false);
    if (hands.size() != 4)
      throw runtime_error("PBN world should contain four hands");

    ParsedWorld world;
    for (unsigned h = 0; h < 4; h++)
    {
      const vector<string> suits = SplitString(hands[h], '.', true);
      if (suits.size() != 4)
        throw runtime_error("PBN hand should contain four suits");

      const int seat = (startSeat + static_cast<int>(h)) % 4;
      for (unsigned s = 0; s < 4; s++)
        world.suits[seat][s] = suits[s];
    }

    return world;
  }
bool WorldHasCard(
    const ParsedWorld& world,
    const int player,
    const int suit,
    const char rank)
  {
    return world.suits[player][suit].find(rank) != string::npos;
  }
int WorldSuitLength(
    const ParsedWorld& world,
    const int player,
    const int suit)
  {
    return static_cast<int>(world.suits[player][suit].size());
  }
int HonorPointValue(const char rank)
  {
    switch (rank)
    {
      case 'A': return 4;
      case 'K': return 3;
      case 'Q': return 2;
      case 'J': return 1;
      default: return 0;
    }
  }
int WorldHighCardPoints(
    const ParsedWorld& world,
    const int player)
  {
    int points = 0;
    for (int suit = 0; suit < 4; suit++)
    {
      const string& cards = world.suits[player][suit];
      for (unsigned i = 0; i < cards.size(); i++)
        points += HonorPointValue(cards[i]);
    }
    return points;
  }
string HandTypeName(const int handType)
  {
    switch (handType)
    {
      case HAND_TYPE_BALANCED: return "balanced";
      case HAND_TYPE_ONE_SUITER: return "one-suiter";
      case HAND_TYPE_TWO_SUITER: return "two-suiter";
      case HAND_TYPE_THREE_SUITER: return "three-suiter";
      default: return "unknown hand type";
    }
  }
bool LengthsMatchHandType(
    const int lengths[4],
    const int totalCards,
    const int handType)
  {
    if (totalCards != 13)
      return false;

    int sortedLengths[4];
    int suitsAtLeast4 = 0;
    int suitsAtLeast5 = 0;
    for (int suit = 0; suit < 4; suit++)
    {
      sortedLengths[suit] = lengths[suit];
      if (lengths[suit] >= 4)
        suitsAtLeast4++;
      if (lengths[suit] >= 5)
        suitsAtLeast5++;
    }
    sort(sortedLengths, sortedLengths + 4);

    const bool balanced =
      ((sortedLengths[0] == 2 && sortedLengths[1] == 3 &&
        sortedLengths[2] == 3 && sortedLengths[3] == 5) ||
       (sortedLengths[0] == 2 && sortedLengths[1] == 3 &&
        sortedLengths[2] == 4 && sortedLengths[3] == 4) ||
       (sortedLengths[0] == 3 && sortedLengths[1] == 3 &&
        sortedLengths[2] == 3 && sortedLengths[3] == 4));

    switch (handType)
    {
      case HAND_TYPE_BALANCED:
        return balanced;

      case HAND_TYPE_THREE_SUITER:
        return ! balanced && suitsAtLeast4 >= 3;

      case HAND_TYPE_TWO_SUITER:
        return ! balanced && suitsAtLeast4 < 3 && suitsAtLeast5 >= 2;

      case HAND_TYPE_ONE_SUITER:
        return ! balanced && suitsAtLeast4 == 1 && suitsAtLeast5 == 1;

      default:
        throw runtime_error("Unknown hand type");
    }
  }
bool WorldHasHandType(
    const ParsedWorld& world,
    const int player,
    const int handType)
  {
    int lengths[4];
    int totalCards = 0;
    for (int suit = 0; suit < 4; suit++)
    {
      lengths[suit] = WorldSuitLength(world, player, suit);
      totalCards += lengths[suit];
    }
    return LengthsMatchHandType(lengths, totalCards, handType);
  }
bool WorldHasBalancedShape(
    const ParsedWorld& world,
    const int player)
  {
    return WorldHasHandType(world, player, HAND_TYPE_BALANCED);
  }
int ConstraintHandType(const WorldConstraint& constraint)
  {
    if (constraint.kind == CONSTRAINT_BALANCED)
      return HAND_TYPE_BALANCED;
    return constraint.count;
  }
int RankOrder(const char rank)
  {
    const string order = "23456789TJQKA";
    const size_t pos = order.find(rank);
    if (pos == string::npos)
      throw runtime_error("Unknown card rank");
    return static_cast<int>(pos);
  }
int RankValue(const char rank)
  {
    return RankOrder(rank) + 2;
  }
char RankFromDDSValue(const int value)
  {
    const string order = "23456789TJQKA";
    if (value < 2 || value > 14)
      throw runtime_error("Unknown DDS rank value");
    return order[static_cast<unsigned>(value - 2)];
  }
unsigned WorldCardCount(const ParsedWorld& world)
  {
    unsigned count = 0;
    for (int player = 0; player < 4; player++)
    {
      for (int suit = 0; suit < 4; suit++)
        count += static_cast<unsigned>(world.suits[player][suit].size());
    }
    return count;
  }
unsigned WorldSeatCardCount(
    const ParsedWorld& world,
    const int player)
  {
    unsigned count = 0;
    for (int suit = 0; suit < 4; suit++)
      count += static_cast<unsigned>(world.suits[player][suit].size());
    return count;
  }
string SerializePBNWorld(const ParsedWorld& world)
  {
    ostringstream oss;
    oss << "N:";

    for (int seat = 0; seat < 4; seat++)
    {
      if (seat != 0)
        oss << " ";

      for (int suit = 0; suit < 4; suit++)
      {
        if (suit != 0)
          oss << ".";
        oss << world.suits[seat][suit];
      }
    }

    return oss.str();
  }
string SeatName(const int seat)
  {
    switch (seat)
    {
      case SEAT_NORTH: return "North";
      case SEAT_EAST: return "East";
      case SEAT_SOUTH: return "South";
      case SEAT_WEST: return "West";
      default: return "Unknown";
    }
  }
string SuitName(const int suit)
  {
    switch (suit)
    {
      case SUIT_SPADES: return "spades";
      case SUIT_HEARTS: return "hearts";
      case SUIT_DIAMONDS: return "diamonds";
      case SUIT_CLUBS: return "clubs";
      default: return "unknown suit";
    }
  }
int PartnerSeat(const int seat)
  {
    switch (seat)
    {
      case SEAT_NORTH: return SEAT_SOUTH;
      case SEAT_EAST: return SEAT_WEST;
      case SEAT_SOUTH: return SEAT_NORTH;
      case SEAT_WEST: return SEAT_EAST;
      default:
        throw runtime_error("Unknown seat for partnership");
    }
  }
string PartnershipName(const int seat)
  {
    const int partner = PartnerSeat(seat);
    if ((seat == SEAT_NORTH && partner == SEAT_SOUTH) ||
        (seat == SEAT_SOUTH && partner == SEAT_NORTH))
    {
      return "North/South";
    }
    return "East/West";
  }
string CardName(const BridgeMove& move)
  {
    ostringstream oss;
    oss << move.rank << " of " << SuitName(move.suit);
    return oss.str();
  }
bool WorldMatchesConstraint(
    const ParsedWorld& world,
    const WorldConstraint& constraint);
void RemoveCardFromWorld(
    ParsedWorld& world,
    const int player,
    const BridgeMove& move);
string ConstraintToString(const WorldConstraint& constraint)
  {
    ostringstream oss;
    oss << SeatName(constraint.player) << " ";
    switch (constraint.kind)
    {
      case CONSTRAINT_HAS_CARD:
        oss << "must hold " << constraint.rank << " of " << SuitName(constraint.suit);
        break;

      case CONSTRAINT_NOT_HAS_CARD:
        oss << "must not hold " << constraint.rank << " of " << SuitName(constraint.suit);
        break;

      case CONSTRAINT_VOID_SUIT:
        oss << "must be void in " << SuitName(constraint.suit);
        break;

      case CONSTRAINT_MIN_LENGTH:
        oss << "must hold at least " << constraint.count << " cards in " <<
          SuitName(constraint.suit);
        break;

      case CONSTRAINT_MAX_LENGTH:
        oss << "must hold at most " << constraint.count << " cards in " <<
          SuitName(constraint.suit);
        break;

      case CONSTRAINT_MIN_HCP:
        oss << "must hold at least " << constraint.count << " HCP";
        break;

      case CONSTRAINT_MAX_HCP:
        oss << "must hold at most " << constraint.count << " HCP";
        break;

      case CONSTRAINT_BALANCED:
        oss << "must have a balanced shape";
        break;

      case CONSTRAINT_HAND_TYPE:
        oss << "must have hand type " << HandTypeName(constraint.count);
        break;

      case CONSTRAINT_PARTNERSHIP_MIN_LENGTH:
        oss << PartnershipName(constraint.player) << " partnership must hold at least "
            << constraint.count << " cards in " << SuitName(constraint.suit);
        break;

      case CONSTRAINT_PARTNERSHIP_MAX_LENGTH:
        oss << PartnershipName(constraint.player) << " partnership must hold at most "
            << constraint.count << " cards in " << SuitName(constraint.suit);
        break;

      case CONSTRAINT_PARTNERSHIP_MIN_HCP:
        oss << PartnershipName(constraint.player)
            << " partnership must hold at least " << constraint.count
            << " HCP";
        break;

      case CONSTRAINT_PARTNERSHIP_MAX_HCP:
        oss << PartnershipName(constraint.player)
            << " partnership must hold at most " << constraint.count
            << " HCP";
        break;

      default:
        oss << "must satisfy an unknown constraint";
        break;
    }
    return oss.str();
  }
string FirstConstraintFailureReason(
    const ParsedWorld& world,
    const vector<WorldConstraint>& constraints)
  {
    for (unsigned i = 0; i < constraints.size(); i++)
    {
      if (! WorldMatchesConstraint(world, constraints[i]))
        return ConstraintToString(constraints[i]);
    }

    return "all constraints passed";
  }
HistoryCheckResult CheckWorldReplayHistory(
    const ParsedWorld& world,
    const vector<PlayHistoryEvent>& history,
    const string& stageName)
  {
    ParsedWorld replay(world);
    HistoryCheckResult result;

    for (unsigned i = 0; i < history.size(); i++)
    {
      const PlayHistoryEvent& event = history[i];
      if (! WorldHasCard(replay, event.player, event.move.suit, event.move.rank))
      {
        ostringstream oss;
        oss << stageName << " event " << (i + 1) << " requires "
            << SeatName(event.player) << " to hold " << CardName(event.move);
        result.ok = false;
        result.reason = oss.str();
        return result;
      }

      if (event.leadSuit >= 0 &&
          event.move.suit != event.leadSuit &&
          WorldSuitLength(replay, event.player, event.leadSuit) > 0)
      {
        ostringstream oss;
        oss << stageName << " event " << (i + 1) << " violates follow-suit because "
            << SeatName(event.player) << " still holds " << SuitName(event.leadSuit);
        result.ok = false;
        result.reason = oss.str();
        return result;
      }

      RemoveCardFromWorld(replay, event.player, event.move);
    }

    return result;
  }
HistoryCheckResult CheckWorldReplayHistoryAfterHistory(
    const ParsedWorld& world,
    const vector<PlayHistoryEvent>& priorHistory,
    const vector<PlayHistoryEvent>& history,
    const string& stageName)
  {
    ParsedWorld replay(world);
    HistoryCheckResult result;

    for (unsigned i = 0; i < priorHistory.size(); i++)
    {
      const PlayHistoryEvent& event = priorHistory[i];
      if (! WorldHasCard(replay, event.player, event.move.suit, event.move.rank))
      {
        result.ok = false;
        result.reason = "prior history did not replay";
        return result;
      }

      if (event.leadSuit >= 0 &&
          event.move.suit != event.leadSuit &&
          WorldSuitLength(replay, event.player, event.leadSuit) > 0)
      {
        result.ok = false;
        result.reason = "prior history did not replay";
        return result;
      }

      RemoveCardFromWorld(replay, event.player, event.move);
    }

    for (unsigned i = 0; i < history.size(); i++)
    {
      const PlayHistoryEvent& event = history[i];
      if (! WorldHasCard(replay, event.player, event.move.suit, event.move.rank))
      {
        ostringstream oss;
        oss << stageName << " event " << (i + 1) << " requires "
            << SeatName(event.player) << " to hold " << CardName(event.move);
        result.ok = false;
        result.reason = oss.str();
        return result;
      }

      if (event.leadSuit >= 0 &&
          event.move.suit != event.leadSuit &&
          WorldSuitLength(replay, event.player, event.leadSuit) > 0)
      {
        ostringstream oss;
        oss << stageName << " event " << (i + 1) << " violates follow-suit because "
            << SeatName(event.player) << " still holds " << SuitName(event.leadSuit);
        result.ok = false;
        result.reason = oss.str();
        return result;
      }

      RemoveCardFromWorld(replay, event.player, event.move);
    }

    return result;
  }
void AppendDerivedFollowSuitConstraints(
    const vector<PlayHistoryEvent>& history,
    int playedCount[4][4],
    vector<WorldConstraint>& constraints)
  {
    for (unsigned i = 0; i < history.size(); i++)
    {
      const PlayHistoryEvent& event = history[i];
      if (event.leadSuit >= 0 && event.move.suit != event.leadSuit)
      {
        constraints.push_back(WorldConstraint::MaxLength(
          event.player,
          event.leadSuit,
          playedCount[event.player][event.leadSuit]));
      }

      if (event.move.suit >= 0 && event.move.suit < 4)
        playedCount[event.player][event.move.suit]++;
    }
  }
vector<WorldConstraint> CollectFollowSuitConstraints(
    const BridgeInformationState& information)
  {
    vector<WorldConstraint> constraints;
    if (information.deriveFollowSuitConstraints)
    {
      int playedCount[4][4];
      memset(playedCount, 0, sizeof(playedCount));
      AppendDerivedFollowSuitConstraints(information.playHistory, playedCount,
        constraints);
      AppendDerivedFollowSuitConstraints(information.currentTrickHistory,
        playedCount, constraints);
    }

    constraints.insert(constraints.end(), information.followSuitConstraints.begin(),
      information.followSuitConstraints.end());
    return constraints;
  }
vector<WorldConstraint> CollectConstructorConstraints(
    const BridgeInformationState& information)
  {
    vector<WorldConstraint> constraints = information.knownCardConstraints;
    constraints.insert(constraints.end(), information.biddingConstraints.begin(),
      information.biddingConstraints.end());
    const vector<WorldConstraint> followSuitConstraints =
      CollectFollowSuitConstraints(information);
    constraints.insert(constraints.end(), followSuitConstraints.begin(),
      followSuitConstraints.end());
    return constraints;
  }
void AddWorldExplanationStep(
    WorldExplanation& explanation,
    const string& stage,
    const bool passed,
    const string& detail)
  {
    WorldExplanationStep step;
    step.stage = stage;
    step.passed = passed;
    step.detail = detail;
    explanation.steps.push_back(step);

    if (! passed && explanation.rejectionStage.empty())
    {
      explanation.accepted = false;
      explanation.rejectionStage = stage;
      explanation.rejectionReason = detail;
    }
  }
unsigned SeatBit(const int seat)
  {
    return static_cast<unsigned>(1U << seat);
  }
unsigned SeatMask(const vector<int>& seats)
  {
    unsigned mask = 0U;
    for (unsigned i = 0; i < seats.size(); i++)
      mask |= SeatBit(seats[i]);
    return mask;
  }
bool VectorContainsSeat(
    const vector<int>& seats,
    const int seat)
  {
    return find(seats.begin(), seats.end(), seat) != seats.end();
  }
bool ConstructorConstraintTouchesHiddenSeats(
    const vector<int>& hiddenSeats,
    const WorldConstraint& constraint)
  {
    if (constraint.kind == CONSTRAINT_PARTNERSHIP_MIN_LENGTH ||
        constraint.kind == CONSTRAINT_PARTNERSHIP_MAX_LENGTH ||
        constraint.kind == CONSTRAINT_PARTNERSHIP_MIN_HCP ||
        constraint.kind == CONSTRAINT_PARTNERSHIP_MAX_HCP)
    {
      return VectorContainsSeat(hiddenSeats, constraint.player) ||
        VectorContainsSeat(hiddenSeats, PartnerSeat(constraint.player));
    }

    return VectorContainsSeat(hiddenSeats, constraint.player);
  }
string CardKey(const BridgeMove& move)
  {
    ostringstream oss;
    oss << move.suit << ":" << move.rank;
    return oss.str();
  }
void SortSuitCards(string& cards)
  {
    sort(cards.begin(), cards.end(),
      [](const char left, const char right)
      {
        return RankOrder(left) > RankOrder(right);
      });
  }
void CanonicalizeWorld(ParsedWorld& world)
  {
    for (int seat = 0; seat < 4; seat++)
    {
      for (int suit = 0; suit < 4; suit++)
        SortSuitCards(world.suits[seat][suit]);
    }
  }
void CollectPlayedCards(
    const BridgeInformationState& information,
    map<string, int>& playedBySeat)
  {
    playedBySeat.clear();
    for (unsigned i = 0; i < information.playHistory.size(); i++)
      playedBySeat[CardKey(information.playHistory[i].move)] =
        information.playHistory[i].player;

    for (unsigned i = 0; i < information.currentTrickHistory.size(); i++)
      playedBySeat[CardKey(information.currentTrickHistory[i].move)] =
        information.currentTrickHistory[i].player;
  }
unsigned CountSeatChoices(const unsigned allowedSeatsMask)
  {
    unsigned count = 0U;
    for (int seat = 0; seat < 4; seat++)
    {
      if ((allowedSeatsMask & SeatBit(seat)) != 0U)
        count++;
    }
    return count;
  }
int FindCardSeatInWorld(
    const ParsedWorld& world,
    const int suit,
    const char rank)
  {
    int foundSeat = -1;
    for (int seat = 0; seat < 4; seat++)
    {
      if (! WorldHasCard(world, seat, suit, rank))
        continue;

      Check(foundSeat < 0,
        "history-derived visible-seed construction should not contain duplicate cards");
      foundSeat = seat;
    }
    return foundSeat;
  }
void CollectSeedHiddenCards(
    const HistoryDerivedWorldSpec& spec,
    HistoryDerivedConstructionResult& result,
    const unsigned hiddenSeatMask,
    int targetCounts[4])
  {
    for (unsigned i = 0; i < spec.hiddenSeats.size(); i++)
    {
      const int seat = spec.hiddenSeats[i];
      for (int suit = 0; suit < 4; suit++)
      {
        const string cards = result.visibleSeedWorld.suits[seat][suit];
        targetCounts[seat] += static_cast<int>(cards.size());
        for (unsigned j = 0; j < cards.size(); j++)
        {
          HiddenCardCandidate candidate;
          candidate.card = BridgeMove(suit, cards[j]);
          candidate.allowedSeatsMask = hiddenSeatMask;
          result.hiddenCards.push_back(candidate);
        }

        result.visibleSeedWorld.suits[seat][suit].clear();
      }
    }
  }
void CollectInferredHiddenCardsFromVisibleHands(
    const HistoryDerivedWorldSpec& spec,
    HistoryDerivedConstructionResult& result,
    const unsigned hiddenSeatMask,
    int targetCounts[4],
    int finalSeatCounts[4])
  {
    const string ranks = "AKQJT98765432";
    for (unsigned i = 0; i < spec.hiddenSeats.size(); i++)
    {
      const int seat = spec.hiddenSeats[i];
      const unsigned knownCount = WorldSeatCardCount(result.visibleSeedWorld, seat);
      Check(knownCount <= 13U,
        "history-derived visible-seed construction should not specify more than thirteen cards on a hidden seat");
      targetCounts[seat] = 13 - static_cast<int>(knownCount);
      finalSeatCounts[seat] = 13;
    }

    unsigned inferredCount = 0U;
    for (int suit = 0; suit < 4; suit++)
    {
      for (unsigned i = 0; i < ranks.size(); i++)
      {
        if (FindCardSeatInWorld(result.visibleSeedWorld, suit, ranks[i]) >= 0)
          continue;

        HiddenCardCandidate candidate;
        candidate.card = BridgeMove(suit, ranks[i]);
        candidate.allowedSeatsMask = hiddenSeatMask;
        result.hiddenCards.push_back(candidate);
        inferredCount++;
      }
    }

    unsigned expectedCount = 0U;
    for (unsigned i = 0; i < spec.hiddenSeats.size(); i++)
      expectedCount += static_cast<unsigned>(targetCounts[spec.hiddenSeats[i]]);

    Check(expectedCount == inferredCount,
      "history-derived visible-seed construction should infer exactly the hidden-seat complement of the visible cards");
  }
void ApplyConstructionPlayedCardOwnership(
    const vector<int>& hiddenSeats,
    const BridgeInformationState& information,
    vector<HiddenCardCandidate>& hiddenCards)
  {
    map<string, int> playedBySeat;
    CollectPlayedCards(information, playedBySeat);
    for (unsigned i = 0; i < hiddenCards.size(); i++)
    {
      const string key = CardKey(hiddenCards[i].card);
      map<string, int>::const_iterator it = playedBySeat.find(key);
      if (it != playedBySeat.end() && VectorContainsSeat(hiddenSeats, it->second))
        hiddenCards[i].allowedSeatsMask = SeatBit(it->second);
    }
  }
void SortHiddenCardCandidates(
    vector<HiddenCardCandidate>& hiddenCards)
  {
    sort(hiddenCards.begin(), hiddenCards.end(),
      [](const HiddenCardCandidate& left, const HiddenCardCandidate& right)
      {
        const unsigned leftChoices = CountSeatChoices(left.allowedSeatsMask);
        const unsigned rightChoices = CountSeatChoices(right.allowedSeatsMask);
        if (leftChoices != rightChoices)
          return leftChoices < rightChoices;
        if (left.card.suit != right.card.suit)
          return left.card.suit < right.card.suit;
        return RankOrder(left.card.rank) > RankOrder(right.card.rank);
      });
  }
void SortConstructedWorlds(
    vector<ParsedWorld>& worlds)
  {
    sort(worlds.begin(), worlds.end(),
      [](const ParsedWorld& left, const ParsedWorld& right)
      {
        return SerializePBNWorld(left) < SerializePBNWorld(right);
      });
  }
void PrepareHistoryDerivedConstruction(
    const HistoryDerivedWorldSpec& spec,
    const BridgeInformationState& information,
    HistoryDerivedConstructionResult& result,
    vector<WorldConstraint>& constructorConstraints,
    int targetCounts[4],
    int finalSeatCounts[4])
  {
    result = HistoryDerivedConstructionResult();
    result.visibleSeedWorld = spec.seedWorld;
    constructorConstraints = CollectConstructorConstraints(information);

    const unsigned hiddenSeatMask = SeatMask(spec.hiddenSeats);
    memset(targetCounts, 0, sizeof(int) * 4U);
    memset(finalSeatCounts, 0, sizeof(int) * 4U);

    if (spec.inferHiddenCardsFromVisibleHands)
    {
      CollectInferredHiddenCardsFromVisibleHands(spec, result, hiddenSeatMask,
        targetCounts, finalSeatCounts);
    }
    else
    {
      CollectSeedHiddenCards(spec, result, hiddenSeatMask, targetCounts);
      for (unsigned i = 0; i < spec.hiddenSeats.size(); i++)
        finalSeatCounts[spec.hiddenSeats[i]] = targetCounts[spec.hiddenSeats[i]];
    }
  }
void ApplyConstructionCardLocationConstraints(
    const vector<int>& hiddenSeats,
    const vector<WorldConstraint>& constraints,
    vector<HiddenCardCandidate>& hiddenCards)
  {
    for (unsigned i = 0; i < constraints.size(); i++)
    {
      const WorldConstraint& constraint = constraints[i];
      if (! ConstructorConstraintTouchesHiddenSeats(hiddenSeats, constraint))
        continue;

      if (constraint.kind != CONSTRAINT_HAS_CARD &&
          constraint.kind != CONSTRAINT_NOT_HAS_CARD)
      {
        continue;
      }

      for (unsigned j = 0; j < hiddenCards.size(); j++)
      {
        if (hiddenCards[j].card.suit != constraint.suit ||
            hiddenCards[j].card.rank != constraint.rank)
        {
          continue;
        }

        if (constraint.kind == CONSTRAINT_HAS_CARD)
          hiddenCards[j].allowedSeatsMask &= SeatBit(constraint.player);
        else
          hiddenCards[j].allowedSeatsMask &= ~SeatBit(constraint.player);
      }
    }
  }
int PotentialRemainingSuitCardsForSeat(
    const vector<HiddenCardCandidate>& hiddenCards,
    const unsigned nextIndex,
    const int seat,
    const int suit)
  {
    int count = 0;
    for (unsigned i = nextIndex; i < hiddenCards.size(); i++)
    {
      if (hiddenCards[i].card.suit == suit &&
          (hiddenCards[i].allowedSeatsMask & SeatBit(seat)) != 0U)
      {
        count++;
      }
    }
    return count;
  }
int PotentialRemainingHighCardPointsForSeat(
    const vector<HiddenCardCandidate>& hiddenCards,
    const unsigned nextIndex,
    const int seat)
  {
    int points = 0;
    for (unsigned i = nextIndex; i < hiddenCards.size(); i++)
    {
      if ((hiddenCards[i].allowedSeatsMask & SeatBit(seat)) != 0U)
        points += HonorPointValue(hiddenCards[i].card.rank);
    }
    return points;
  }
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
  /**
   * Build the candidate-world pool from a partially observed bridge position.
   *
   * This is not part of the original alpha-mu papers themselves; it is the
   * repository-specific front-end that supplies the possible worlds the paper
   * search runs over. The function first prepares visible/hidden card state,
   * then applies constructor-safe ownership and card-location pruning, and only
   * then enumerates hidden-card assignments with additional feasibility checks.
   */
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
  /**
   * Explain the constructor-local pruning stages used during world building.
   *
   * The explanation mirrors the actual construction order so tests can show how
   * much each pre-search stage removes before the later staged filtering pass.
   */
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
dealPBN MakeDDSDealPBN(
    const BridgeState& state,
    const ParsedWorld& world)
  {
    dealPBN deal;
    memset(&deal, 0, sizeof(deal));

    deal.trump = (state.trumpSuit >= 0 ? state.trumpSuit : 4);
    deal.first = (state.currentTrick.empty() ? state.playerToMove : state.trickLeader);

    for (unsigned i = 0; i < state.currentTrick.size() && i < 3; i++)
    {
      deal.currentTrickSuit[i] = state.currentTrick[i].suit;
      deal.currentTrickRank[i] = RankValue(state.currentTrick[i].rank);
    }

    const string remain = SerializePBNWorld(world);
    Check(remain.size() < sizeof(deal.remainCards),
      "bridge DDS leaf serialization should fit into dealPBN.remainCards");
    strcpy(deal.remainCards, remain.c_str());
    return deal;
  }
int RemainingTricksInWorld(
    const BridgeState& state,
    const ParsedWorld& world)
  {
    return static_cast<int>((WorldCardCount(world) + state.currentTrick.size()) / 4U);
  }
bool BridgeMoveLess(
    const BridgeMove& left,
    const BridgeMove& right)
  {
    if (left.suit != right.suit)
      return left.suit < right.suit;
    return RankOrder(left.rank) < RankOrder(right.rank);
  }
int SeatSide(const int seat)
  {
    return (seat == SEAT_NORTH || seat == SEAT_SOUTH ? 0 : 1);
  }
unsigned WinningCardIndex(
    const vector<BridgeMove>& trick,
    const int leadSuit,
    const int trumpSuit)
  {
    if (trick.empty())
      throw runtime_error("WinningCardIndex called on empty trick");

    unsigned best = 0;
    for (unsigned i = 1; i < trick.size(); i++)
    {
      const bool bestTrump = (trumpSuit >= 0 && trick[best].suit == trumpSuit);
      const bool candTrump = (trumpSuit >= 0 && trick[i].suit == trumpSuit);

      if (candTrump && ! bestTrump)
      {
        best = i;
        continue;
      }
      if (bestTrump && ! candTrump)
        continue;

      const int winningSuit = (bestTrump || candTrump ? trumpSuit : leadSuit);
      if (trick[i].suit == winningSuit && trick[best].suit != winningSuit)
      {
        best = i;
        continue;
      }

      if (trick[i].suit == trick[best].suit &&
          RankOrder(trick[i].rank) > RankOrder(trick[best].rank))
      {
        best = i;
      }
    }

    return best;
  }
int TrickWinner(
    const BridgeState& state)
  {
    if (state.currentTrick.size() != 4 ||
        state.currentTrickPlayers.size() != 4)
    {
      throw runtime_error("TrickWinner requires a complete 4-card trick");
    }

    const unsigned best = WinningCardIndex(state.currentTrick, state.leadSuit,
      state.trumpSuit);
    return state.currentTrickPlayers[best];
  }
bool WorldHasLegalSuit(
    const ParsedWorld& world,
    const int player,
    const int leadSuit)
  {
    return leadSuit >= 0 && WorldSuitLength(world, player, leadSuit) > 0;
  }
vector<BridgeMove> LegalMovesInWorld(
    const ParsedWorld& world,
    const int player,
    const int leadSuit)
  {
    vector<BridgeMove> moves;

    if (WorldHasLegalSuit(world, player, leadSuit))
    {
      const string& cards = world.suits[player][leadSuit];
      for (unsigned i = 0; i < cards.size(); i++)
        moves.push_back(BridgeMove(leadSuit, cards[i]));
      return moves;
    }

    for (int suit = 0; suit < 4; suit++)
    {
      const string& cards = world.suits[player][suit];
      for (unsigned i = 0; i < cards.size(); i++)
        moves.push_back(BridgeMove(suit, cards[i]));
    }
    return moves;
  }
bool ContainsMove(
    const vector<BridgeMove>& moves,
    const BridgeMove& move)
  {
    for (unsigned i = 0; i < moves.size(); i++)
    {
      if (moves[i] == move)
        return true;
    }
    return false;
  }
bool WorldCanPlayMove(
    const ParsedWorld& world,
    const int player,
    const int leadSuit,
    const BridgeMove& move)
  {
    return ContainsMove(LegalMovesInWorld(world, player, leadSuit), move);
  }
void RemoveCardFromWorld(
    ParsedWorld& world,
    const int player,
    const BridgeMove& move)
  {
    string& cards = world.suits[player][move.suit];
    const size_t pos = cards.find(move.rank);
    if (pos == string::npos)
      throw runtime_error("Tried to remove a card not held in world");
    cards.erase(pos, 1);
  }
vector<BridgeMove> GenerateBridgeMoves(const BridgeState& state)
  {
    vector<BridgeMove> moves;
    for (unsigned i = 0; i < state.worlds.size(); i++)
    {
      if (! state.possibleWorlds.Has(i))
        continue;

      const vector<BridgeMove> localMoves = LegalMovesInWorld(
        state.worlds[i],
        state.playerToMove,
        state.leadSuit);
      for (unsigned j = 0; j < localMoves.size(); j++)
      {
        if (! ContainsMove(moves, localMoves[j]))
          moves.push_back(localMoves[j]);
      }
    }

    sort(moves.begin(), moves.end(), BridgeMoveLess);
    return moves;
  }
BridgeState PlayBridgeMove(
    const BridgeState& state,
    const BridgeMove& move)
  {
    BridgeState next(state);
    next.possibleWorlds = WorldMask::None(state.possibleWorlds.count);

    for (unsigned i = 0; i < state.worlds.size(); i++)
    {
      if (! state.possibleWorlds.Has(i))
        continue;

      if (! WorldCanPlayMove(state.worlds[i], state.playerToMove,
            state.leadSuit, move))
        continue;

      next.possibleWorlds.bits |= (1ULL << i);
      RemoveCardFromWorld(next.worlds[i], state.playerToMove, move);
    }

    if (state.currentTrick.empty())
      next.trickLeader = state.playerToMove;

    next.currentTrick.push_back(move);
    next.currentTrickPlayers.push_back(state.playerToMove);
    if (state.leadSuit < 0)
      next.leadSuit = move.suit;

    if (next.currentTrick.size() == 4)
    {
      const int winner = TrickWinner(next);
      if (SeatSide(winner) == next.maxSide)
        next.maxTricksWon++;
      next.currentTrick.clear();
      next.currentTrickPlayers.clear();
      next.leadSuit = -1;
      next.playerToMove = winner;
      next.trickLeader = winner;
    }
    else
      next.playerToMove = (state.playerToMove + 1) % 4;

    return next;
  }
vector<BridgeChild> ExpandBridgeChildren(const BridgeState& state)
  {
    vector<BridgeChild> children;
    const vector<BridgeMove> moves = GenerateBridgeMoves(state);
    for (unsigned i = 0; i < moves.size(); i++)
    {
      BridgeChild child;
      child.move = moves[i];
      child.state = PlayBridgeMove(state, moves[i]);
      children.push_back(child);
    }
    return children;
  }
ParetoFront MakeZeroFront(const unsigned worldCount);
  int BestScore(const futureTricks& fut);
  void CheckDDS(const int ret, const string& tag);
  void MaybeReportBenchmarkBoardProgress(
    const BridgeState& state,
    const int tricksRemaining,
    const SearchExecutionContext& context);
ParetoFront MakeBridgeTerminalFront(const BridgeState& state)
  {
    ParetoFront front(state.possibleWorlds.count);
    OutcomeVector vec(state.possibleWorlds.count);
    vec.valid = state.possibleWorlds;
    for (unsigned i = 0; i < vec.values.size(); i++)
    {
      if (vec.valid.Has(i))
        vec.values[i] = state.maxTricksWon;
    }
    front.Insert(vec);
    return front;
  }
  /**
   * Evaluate each surviving world exactly with DDS and wrap the result as one
   * sparse outcome vector.
   *
   * In alpha-mu terms, DDS is the perfect-information leaf oracle. The bridge
   * continuation search above it stays in imperfect-information space, while DDS
   * supplies the exact score once the prototype reaches its leaf horizon.
   */
ParetoFront MakeBridgeDDSLeafFront(
    const BridgeState& state,
    const SearchExecutionContext& context)
  {
    vector<unsigned> active;
    for (unsigned i = 0; i < state.worlds.size(); i++)
    {
      if (state.possibleWorlds.Has(i))
        active.push_back(i);
    }

    if (active.empty())
      return MakeZeroFront(state.possibleWorlds.count);

    bool allFinished = state.currentTrick.empty();
    for (unsigned i = 0; i < active.size(); i++)
    {
      if (WorldCardCount(state.worlds[active[i]]) != 0)
      {
        allFinished = false;
        break;
      }
    }

    if (allFinished)
      return MakeBridgeTerminalFront(state);

    // Validate equal card counts before calling DDS
    for (unsigned i = 0; i < active.size(); i++)
    {
      const unsigned wi = active[i];
      unsigned seatCounts[4];
      for (int s = 0; s < 4; s++)
        seatCounts[s] = WorldSeatCardCount(state.worlds[wi], s);
      const unsigned expected = seatCounts[0];
      for (int s = 1; s < 4; s++)
      {
        if (seatCounts[s] != expected)
        {
          fprintf(stderr,
            "DEBUG: unbalanced world %u at DDS leaf: N=%u E=%u S=%u W=%u "
            "trickSize=%u playerToMove=%d maxSide=%d maxTricksWon=%d\n",
            wi, seatCounts[0], seatCounts[1], seatCounts[2], seatCounts[3],
            static_cast<unsigned>(state.currentTrick.size()),
            state.playerToMove, state.maxSide, state.maxTricksWon);
          fprintf(stderr, "  PBN: %s\n",
            SerializePBNWorld(state.worlds[wi]).c_str());
        }
      }
    }

    ParetoFront front(state.possibleWorlds.count);
    OutcomeVector vec(state.possibleWorlds.count);
    vec.valid = state.possibleWorlds;

    for (unsigned i = 0; i < active.size(); i++)
    {
      const unsigned worldIndex = active[i];
      futureTricks fut;
      memset(&fut, 0, sizeof(fut));

      if (context.benchmarkProgress != NULL)
        context.benchmarkProgress->ddsLeafCalls++;

      const dealPBN deal = MakeDDSDealPBN(state, state.worlds[worldIndex]);
      const int ret = SolveBoardPBN(deal, -1, 1, 1, &fut,
        context.ddsThreadId);
      ostringstream ddsTag;
      ddsTag << "SolveBoardPBN bridge DDS leaf"
             << " world=" << worldIndex
             << " first=" << deal.first
             << " trickSize=" << state.currentTrick.size()
             << " remain=\"" << deal.remainCards << "\"";
      CheckDDS(ret, ddsTag.str());

      const int best = BestScore(fut);
      const int tricksRemaining = RemainingTricksInWorld(state,
        state.worlds[worldIndex]);
      const int maxAdditional =
        (SeatSide(state.playerToMove) == state.maxSide ?
          best : tricksRemaining - best);
      vec.values[worldIndex] = state.maxTricksWon + maxAdditional;
    }

    front.Insert(vec);
    return front;
  }
ParetoFront MakeBridgeDDSLeafFront(const BridgeState& state)
  {
    return MakeBridgeDDSLeafFront(state, SearchExecutionContext());
  }
int BridgeDepthCost(
    const BridgeState& state,
    const BridgeState& child)
  {
    return (state.currentTrick.size() == 3 && child.currentTrick.empty() ? 1 : 0);
  }
  /**
   * Bridge continuation search using the alpha-mu front semantics.
   *
   * Max-side turns merge child fronts by union plus Pareto reduction; defender
   * turns combine children by Min-node product/min. This is the same semantic
   * split as the original alpha-mu paper, but applied to concrete bridge moves
   * and with DDS used as the exact leaf evaluator.
   */
ParetoFront SearchBridgeStateInternal(
    const BridgeState& state,
    const int tricksRemaining,
    const SearchExecutionContext& context)
  {
    MaybeReportBenchmarkBoardProgress(state, tricksRemaining, context);

    if (state.possibleWorlds.Empty())
      return MakeZeroFront(state.possibleWorlds.count);

    if (tricksRemaining <= 0)
      return MakeBridgeDDSLeafFront(state, context);

    const vector<BridgeChild> children = ExpandBridgeChildren(state);
    if (children.empty())
      return MakeBridgeDDSLeafFront(state, context);

    if (SeatSide(state.playerToMove) == state.maxSide)
    {
      ParetoFront front(state.possibleWorlds.count);
      for (unsigned i = 0; i < children.size(); i++)
      {
        const int nextDepth = tricksRemaining - BridgeDepthCost(state,
          children[i].state);
        front = ParetoFront::MaxMerge(front,
          SearchBridgeStateInternal(children[i].state, nextDepth, context));
      }
      return front;
    }

    ParetoFront front(state.possibleWorlds.count);
    bool initialized = false;
    for (unsigned i = 0; i < children.size(); i++)
    {
      const int nextDepth = tricksRemaining - BridgeDepthCost(state,
        children[i].state);
      const ParetoFront childFront = SearchBridgeStateInternal(
        children[i].state,
        nextDepth,
        context);
      if (! initialized)
      {
        front = childFront;
        initialized = true;
      }
      else
        front = ParetoFront::MinProduct(front, childFront);
    }
    return front;
  }
ParetoFront SearchBridgeStateInternal(
    const BridgeState& state,
    const int tricksRemaining)
  {
    return SearchBridgeStateInternal(state, tricksRemaining,
      SearchExecutionContext());
  }
ParetoFront SearchBridgeState(
    const BridgeState& state,
    const int tricksRemaining,
    const SearchExecutionContext& context)
  {
    return SearchBridgeStateInternal(state, tricksRemaining, context);
  }
ParetoFront SearchBridgeState(
    const BridgeState& state,
    const int tricksRemaining)
  {
    return SearchBridgeStateInternal(state, tricksRemaining,
      SearchExecutionContext());
  }
ParetoFront SearchBridgeStateWithTT(
    const BridgeState& state,
    const int tricksRemaining,
    const SearchExecutionContext& context,
    BridgeTranspositionTable* tt,
    BridgeTTStats* ttStats)
  {
    MaybeReportBenchmarkBoardProgress(state, tricksRemaining, context);

    if (state.possibleWorlds.Empty())
      return MakeZeroFront(state.possibleWorlds.count);

    if (tricksRemaining <= 0)
      return MakeBridgeDDSLeafFront(state, context);

    // TT probe
    unsigned long long hash = 0;
    if (tt != NULL)
    {
      hash = HashBridgeState(state);
      if (ttStats != NULL)
        ttStats->probes++;

      const ParetoFront* cached = tt->Probe(hash, state.possibleWorlds.bits);
      if (cached != NULL)
      {
        if (ttStats != NULL)
          ttStats->hits++;
        return *cached;
      }
    }

    const vector<BridgeChild> children = ExpandBridgeChildren(state);
    if (children.empty())
      return MakeBridgeDDSLeafFront(state, context);

    ParetoFront front(state.possibleWorlds.count);

    if (SeatSide(state.playerToMove) == state.maxSide)
    {
      for (unsigned i = 0; i < children.size(); i++)
      {
        const int nextDepth = tricksRemaining - BridgeDepthCost(state,
          children[i].state);
        front = ParetoFront::MaxMerge(front,
          SearchBridgeStateWithTT(children[i].state, nextDepth, context,
            tt, ttStats));
      }
    }
    else
    {
      bool initialized = false;
      for (unsigned i = 0; i < children.size(); i++)
      {
        const int nextDepth = tricksRemaining - BridgeDepthCost(state,
          children[i].state);
        const ParetoFront childFront = SearchBridgeStateWithTT(
          children[i].state, nextDepth, context, tt, ttStats);
        if (! initialized)
        {
          front = childFront;
          initialized = true;
        }
        else
          front = ParetoFront::MinProduct(front, childFront);
      }
    }

    // TT store
    if (tt != NULL)
    {
      if (ttStats != NULL)
        ttStats->stores++;
      tt->Store(hash, state.possibleWorlds.bits, front);
    }

    return front;
  }
BridgeRootReport AnalyzeBridgeRoot(
    const BridgeState& state,
    const int tricksRemaining,
    const SearchExecutionContext& context)
  {
    BridgeRootReport report(state.possibleWorlds.count);
    if (state.possibleWorlds.Empty())
      return report;

    const vector<BridgeChild> children = ExpandBridgeChildren(state);
    for (unsigned i = 0; i < children.size(); i++)
    {
      BridgeRootChildReport childReport(state.possibleWorlds.count);
      childReport.move = children[i].move;
      const int nextDepth = tricksRemaining - BridgeDepthCost(state,
        children[i].state);
      childReport.front = SearchBridgeState(children[i].state, nextDepth,
        context);
      childReport.validWorlds = childReport.front.ValidWorlds();
      childReport.usefulWorlds = childReport.front.UsefulWorlds();
      report.children.push_back(childReport);
      report.rootFront = ParetoFront::MaxMerge(report.rootFront,
        childReport.front);
    }

    return report;
  }
BridgeRootReport AnalyzeBridgeRoot(
    const BridgeState& state,
    const int tricksRemaining)
  {
    return AnalyzeBridgeRoot(state, tricksRemaining, SearchExecutionContext());
  }

BridgeRootReport AnalyzeBridgeRootWithTT(
    const BridgeState& state,
    const int tricksRemaining,
    const SearchExecutionContext& context,
    BridgeTranspositionTable* tt,
    BridgeTTStats* ttStats,
    const BridgeRootReport* previousReport)
  {
    BridgeRootReport report(state.possibleWorlds.count);
    if (state.possibleWorlds.Empty())
      return report;

    vector<BridgeChild> children = ExpandBridgeChildren(state);
    if (children.empty())
      return report;

    // Move ordering: if we have a previous report, sort children by
    // descending mu from the previous iteration
    if (previousReport != NULL && ! previousReport->children.empty())
    {
      // Build a map from move -> mu
      map<string, double> moveMu;
      for (unsigned i = 0; i < previousReport->children.size(); i++)
      {
        const BridgeMove& m = previousReport->children[i].move;
        const string key = SuitName(m.suit) + string(1, m.rank);
        moveMu[key] = previousReport->children[i].front.Mu();
      }

      // Sort children by descending previous mu (stable sort to preserve
      // order for moves not in the previous report)
      for (unsigned i = 0; i < children.size(); i++)
      {
        for (unsigned j = i + 1; j < children.size(); j++)
        {
          const string ki = SuitName(children[i].move.suit) +
            string(1, children[i].move.rank);
          const string kj = SuitName(children[j].move.suit) +
            string(1, children[j].move.rank);
          const double mi = (moveMu.count(ki) ? moveMu[ki] : -1.0);
          const double mj = (moveMu.count(kj) ? moveMu[kj] : -1.0);
          if (mj > mi)
            swap(children[i], children[j]);
        }
      }
    }

    for (unsigned i = 0; i < children.size(); i++)
    {
      BridgeRootChildReport childReport(state.possibleWorlds.count);
      childReport.move = children[i].move;
      const int nextDepth = tricksRemaining - BridgeDepthCost(state,
        children[i].state);
      childReport.front = SearchBridgeStateWithTT(
        children[i].state, nextDepth, context, tt, ttStats);
      childReport.validWorlds = childReport.front.ValidWorlds();
      childReport.usefulWorlds = childReport.front.UsefulWorlds();
      report.children.push_back(childReport);
      report.rootFront = ParetoFront::MaxMerge(report.rootFront,
        childReport.front);
    }

    return report;
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
  /**
   * Run the staged possible-world pipeline used before alpha-mu search starts.
   *
   * The alpha-mu papers assume a set of possible worlds is available. In this
   * repository that set is built incrementally from bridge facts: known cards,
   * bidding-derived facts, follow-suit implications, full play history, current
   * trick legality, optional deduplication, and deterministic downselection.
   */
WorldMask GeneratePossibleWorlds(
    const vector<ParsedWorld>& worlds,
    const BridgeInformationState& information,
    WorldGenerationStats* stats)
  {
    const vector<WorldConstraint> followSuitConstraints =
      CollectFollowSuitConstraints(information);
    WorldMask mask = WorldMask::All(static_cast<unsigned>(worlds.size()));
    if (stats != NULL)
      stats->candidateWorldCount = mask.PopCount();

    mask = FilterWorldsByConstraints(worlds, mask,
      information.knownCardConstraints);
    if (stats != NULL)
      stats->afterKnownCardCount = mask.PopCount();

    mask = FilterWorldsByConstraints(worlds, mask,
      information.biddingConstraints);
    if (stats != NULL)
      stats->afterBiddingCount = mask.PopCount();

    mask = FilterWorldsByConstraints(worlds, mask, followSuitConstraints);
    if (stats != NULL)
      stats->afterFollowSuitCount = mask.PopCount();

    mask = FilterWorldsByHistory(worlds, mask, information.playHistory);
    if (stats != NULL)
      stats->afterPlayHistoryCount = mask.PopCount();

    mask = FilterWorldsByHistoryAfterHistory(worlds, mask, information.playHistory,
      information.currentTrickHistory);
    if (stats != NULL)
      stats->afterCurrentTrickCount = mask.PopCount();

    if (information.deduplicateEquivalentWorlds)
    {
      unsigned duplicatesRemoved = 0;
      mask = DeduplicateWorldMask(worlds, mask, duplicatesRemoved);
      if (stats != NULL)
        stats->duplicateWorldsRemoved = duplicatesRemoved;
    }

    unsigned sampledOutWorlds = 0;
    mask = SampleWorldMaskDeterministically(worlds, mask, information.sampleLimit,
      information.samplingSeed, sampledOutWorlds);
    if (stats != NULL)
    {
      stats->afterSamplingCount = mask.PopCount();
      stats->sampledOutWorlds = sampledOutWorlds;
    }

    if (stats != NULL)
      stats->finalWorldCount = mask.PopCount();
    return mask;
  }
  /**
   * Produce a per-world explanation for each filtering stage.
   *
   * This is intentionally verbose and test-facing: it makes the bridge-specific
   * world-supply machinery auditable before worlds are handed to alpha-mu.
   */
WorldGenerationExplanation ExplainPossibleWorldGeneration(
    const vector<ParsedWorld>& worlds,
    const BridgeInformationState& information)
  {
    WorldGenerationExplanation explanation;
    explanation.appliedFollowSuitConstraints =
      CollectFollowSuitConstraints(information);

    const WorldMask allMask = WorldMask::All(static_cast<unsigned>(worlds.size()));
    const WorldMask knownMask = FilterWorldsByConstraints(worlds, allMask,
      information.knownCardConstraints);
    const WorldMask biddingMask = FilterWorldsByConstraints(worlds, knownMask,
      information.biddingConstraints);
    const WorldMask followSuitMask = FilterWorldsByConstraints(worlds, biddingMask,
      explanation.appliedFollowSuitConstraints);
    const WorldMask playHistoryMask = FilterWorldsByHistory(worlds, followSuitMask,
      information.playHistory);
    const WorldMask currentTrickMask = FilterWorldsByHistoryAfterHistory(worlds,
      playHistoryMask, information.playHistory, information.currentTrickHistory);

    WorldMask dedupMask = currentTrickMask;
    map<string, unsigned> firstSeen;
    if (information.deduplicateEquivalentWorlds)
    {
      unsigned duplicatesRemoved = 0;
      dedupMask = DeduplicateWorldMask(worlds, currentTrickMask, duplicatesRemoved);
      for (unsigned i = 0; i < worlds.size(); i++)
      {
        if (! currentTrickMask.Has(i))
          continue;

        const string key = SerializePBNWorld(worlds[i]);
        if (firstSeen.find(key) == firstSeen.end())
          firstSeen[key] = i;
      }
    }

    unsigned sampledOutWorlds = 0;
    explanation.finalWorldMask = SampleWorldMaskDeterministically(worlds, dedupMask,
      information.sampleLimit, information.samplingSeed, sampledOutWorlds);

    for (unsigned i = 0; i < worlds.size(); i++)
    {
      WorldExplanation world;
      world.worldIndex = i;
      world.serializedWorld = SerializePBNWorld(worlds[i]);

      if (! knownMask.Has(i))
      {
        AddWorldExplanationStep(world, "known_cards", false,
          FirstConstraintFailureReason(worlds[i], information.knownCardConstraints));
        explanation.worlds.push_back(world);
        continue;
      }
      AddWorldExplanationStep(world, "known_cards", true,
        "passed known-card constraints");

      if (! biddingMask.Has(i))
      {
        AddWorldExplanationStep(world, "bidding", false,
          FirstConstraintFailureReason(worlds[i], information.biddingConstraints));
        explanation.worlds.push_back(world);
        continue;
      }
      AddWorldExplanationStep(world, "bidding", true,
        "passed bidding constraints");

      if (! followSuitMask.Has(i))
      {
        AddWorldExplanationStep(world, "follow_suit", false,
          FirstConstraintFailureReason(worlds[i], explanation.appliedFollowSuitConstraints));
        explanation.worlds.push_back(world);
        continue;
      }
      AddWorldExplanationStep(world, "follow_suit", true,
        (explanation.appliedFollowSuitConstraints.empty() ?
          "no explicit follow-suit implications" :
          "passed explicit follow-suit implications"));

      if (! playHistoryMask.Has(i))
      {
        AddWorldExplanationStep(world, "play_history", false,
          CheckWorldReplayHistory(worlds[i], information.playHistory,
            "play-history").reason);
        explanation.worlds.push_back(world);
        continue;
      }
      AddWorldExplanationStep(world, "play_history", true,
        "replayed prior tricks legally");

      if (! currentTrickMask.Has(i))
      {
        AddWorldExplanationStep(world, "current_trick", false,
          CheckWorldReplayHistoryAfterHistory(worlds[i], information.playHistory,
            information.currentTrickHistory, "current-trick").reason);
        explanation.worlds.push_back(world);
        continue;
      }
      AddWorldExplanationStep(world, "current_trick", true,
        "replayed the current partial trick legally");

      if (information.deduplicateEquivalentWorlds && ! dedupMask.Has(i))
      {
        const string key = SerializePBNWorld(worlds[i]);
        const unsigned first = firstSeen[key];
        ostringstream oss;
        oss << "duplicate of surviving world " << first;
        AddWorldExplanationStep(world, "deduplication", false, oss.str());
        explanation.worlds.push_back(world);
        continue;
      }
      if (information.deduplicateEquivalentWorlds)
        AddWorldExplanationStep(world, "deduplication", true,
          "kept as the canonical surviving world");

      if (! explanation.finalWorldMask.Has(i))
      {
        ostringstream oss;
        oss << "deterministic sampling seed " << information.samplingSeed
            << " skipped this world after canonical ordering";
        AddWorldExplanationStep(world, "sampling", false, oss.str());
        explanation.worlds.push_back(world);
        continue;
      }

      if (information.sampleLimit != 0)
        AddWorldExplanationStep(world, "sampling", true,
          "retained by deterministic sampling");

      world.accepted = true;
      explanation.worlds.push_back(world);
    }

    return explanation;
  }
WorldMask GeneratePossibleWorlds(
    const vector<ParsedWorld>& worlds,
    const vector<WorldConstraint>& constraints)
  {
    BridgeInformationState information;
    information.knownCardConstraints = constraints;
    return GeneratePossibleWorlds(worlds, information, NULL);
  }
OutcomeVector MakeBinaryOutcome(
    const string& text)
  {
    const unsigned n = static_cast<unsigned>(text.size());
    OutcomeVector vec(n);
    for (unsigned i = 0; i < n; i++)
    {
      if (text[i] == '0' || text[i] == '1')
      {
        vec.valid.bits |= (1ULL << i);
        vec.values[i] = text[i] - '0';
      }
      else if (text[i] == 'x' || text[i] == 'X' || text[i] == '?')
        continue;
      else
        throw runtime_error("MakeBinaryOutcome expects only 0/1/x text");
    }
    return vec;
  }
void AddChild(
    ToyNode& parent,
    const ToyNode& child,
    const WorldMask& worlds)
  {
    parent.children.push_back(&child);
    parent.childWorlds.push_back(worlds);
  }
void AddChild(
    ToyNode& parent,
    const ToyNode& child)
  {
    AddChild(parent, child, WorldMask::All(parent.leafFront.worldCount));
  }
string MakeTTKey(
    const ToyNode& node,
    const int maxMoves,
    const WorldMask& usefulWorlds)
  {
    ostringstream oss;
    oss << node.name << "|" << maxMoves << "|" << usefulWorlds.count << "|"
        << usefulWorlds.bits;
    return oss.str();
  }
ParetoFront MakeFront(
    const unsigned worldCount,
    const vector<string>& outcomes)
  {
    ParetoFront front(worldCount);
    for (unsigned i = 0; i < outcomes.size(); i++)
      front.Insert(MakeBinaryOutcome(outcomes[i]));
    return front;
  }
string ResolvePath(const string& candidate)
  {
    FILE * fp = fopen(candidate.c_str(), "r");
    if (fp != NULL)
    {
      fclose(fp);
      return candidate;
    }

    const string prefixed = "../" + candidate;
    fp = fopen(prefixed.c_str(), "r");
    if (fp != NULL)
    {
      fclose(fp);
      return prefixed;
    }

    return candidate;
  }
ParetoFront MakeZeroFront(const unsigned worldCount)
  {
    ParetoFront front(worldCount);
    OutcomeVector vec(worldCount);
    vec.valid = WorldMask::All(worldCount);
    front.Insert(vec);
    return front;
  }
ParetoFront MakeSingleWorldFront(
    const unsigned worldCount,
    const unsigned world,
    const int value)
  {
    ParetoFront front(worldCount);
    OutcomeVector vec(worldCount);
    vec.valid = WorldMask(worldCount, 1ULL << world);
    vec.values[world] = value;
    front.Insert(vec);
    return front;
  }
unsigned SoleWorldIndex(const WorldMask& mask)
  {
    for (unsigned i = 0; i < mask.count; i++)
    {
      if (mask.Has(i))
        return i;
    }

    throw runtime_error("SoleWorldIndex called on empty mask");
  }
int EvaluateLeafWorld(
    const ParetoFront& front,
    const unsigned world)
  {
    int best = 0;
    for (unsigned i = 0; i < front.vectors.size(); i++)
    {
      if (front.vectors[i].valid.Has(world))
        best = max(best, front.vectors[i].values[world]);
    }
    return best;
  }
int EvaluateSingleWorld(
    const ToyNode& node,
    const int maxMoves,
    const unsigned world,
    SearchStats& stats)
  {
    if (node.type == TOY_LEAF || maxMoves == 0)
    {
      stats.leafWorldEvaluations++;
      return EvaluateLeafWorld(node.leafFront, world);
    }

    if (node.type == TOY_MIN)
    {
      int best = 1;
      bool found = false;
      for (unsigned i = 0; i < node.children.size(); i++)
      {
        if (! node.childWorlds[i].Has(world))
          continue;

        const int value = EvaluateSingleWorld(
          * node.children[i],
          maxMoves,
          world,
          stats);
        found = true;
        best = min(best, value);
      }
      if (! found)
        return 0;
      return best;
    }

    int best = 0;
    for (unsigned i = 0; i < node.children.size(); i++)
    {
      if (! node.childWorlds[i].Has(world))
        continue;

      const int value = EvaluateSingleWorld(
        * node.children[i],
        maxMoves - 1,
        world,
        stats);
      best = max(best, value);
    }
    return best;
  }
  /**
   * Execute the paper-faithful alpha-mu recursion on the toy tree.
   *
   * This function is the clearest statement of the algorithm in the repository:
   *
   * - Max nodes use union/merge of child fronts.
   * - Min nodes use product/min combination of child fronts.
   * - Useful worlds are maintained after each Min backup.
   * - Optimistic completion is used when comparing sparse intermediate fronts.
   * - Early cut checks the nearest Max ancestor.
   * - Deep alpha cut checks earlier Max ancestors.
   * - Cut-on-win stops a Max node once a child wins in all useful worlds.
   * - Root cut stops iterative deepening once the root `mu` value stabilizes.
   * - The transposition table stores only exact fronts.
   *
   * Together, those items correspond to the original alpha-mu paper plus the
   * later optimization paper discussed in `docs/alpha-mu.md`.
   */
ParetoFront SearchToy(
    const ToyNode& node,
    const int maxMoves,
    const WorldMask& usefulWorlds,
    const vector<const ParetoFront *>& upperMaxFronts,
    const OutcomeVector& optimisticValues,
    TranspositionTable& tt,
    const bool isRoot,
    const double previousRootMu,
    SearchStats& stats,
    bool& rootCutTriggered,
    bool& exactComplete)
  {
    stats.nodesVisited++;
    stats.visitOrder.push_back(node.name);

    const string ttKey = MakeTTKey(node, maxMoves, usefulWorlds);
    ParetoFront ttFront(node.leafFront.worldCount);
    if (tt.Lookup(ttKey, ttFront))
    {
      stats.ttHits++;
      exactComplete = true;
      return ttFront;
    }

    if (usefulWorlds.Empty())
    {
      stats.worldCutsZero++;
      exactComplete = true;
      return MakeZeroFront(node.leafFront.worldCount);
    }

    if (usefulWorlds.PopCount() == 1)
    {
      const unsigned world = SoleWorldIndex(usefulWorlds);
      const int value = EvaluateSingleWorld(node, maxMoves, world, stats);
      stats.worldCutsSingle++;
      exactComplete = true;
      return MakeSingleWorldFront(node.leafFront.worldCount, world, value);
    }

    OutcomeVector nodeOptimistic(optimisticValues);
    for (unsigned i = 0; i < nodeOptimistic.values.size(); i++)
    {
      if (node.optimisticValues.valid.Has(i))
      {
        nodeOptimistic.valid.bits |= (1ULL << i);
        nodeOptimistic.values[i] = node.optimisticValues.values[i];
      }
    }

    if (node.type == TOY_LEAF || maxMoves == 0)
    {
      const WorldMask evaluated = node.leafFront.ValidWorlds().Intersection(
        usefulWorlds);
      stats.leafWorldEvaluations += static_cast<int>(evaluated.PopCount());
      const ParetoFront front = node.leafFront.RestrictToUseful(usefulWorlds);
      tt.Store(ttKey, front);
      stats.ttStores++;
      exactComplete = true;
      return front;
    }

    if (node.type == TOY_MIN)
    {
      /*
       * Optimization-paper path:
       *   1. combine children with MinProduct,
       *   2. shrink the useful-world set after each child,
       *   3. compare an optimistically completed sparse front against ancestor
       *      Max fronts for early/deep alpha cuts.
       */
      ParetoFront mini(node.leafFront.worldCount);
      bool initialized = false;
      bool complete = true;
      WorldMask currentUseful = usefulWorlds;

      for (unsigned i = 0; i < node.children.size(); i++)
      {
        bool childComplete = false;
        const ParetoFront f = SearchToy(
          * node.children[i],
          maxMoves,
          currentUseful.Intersection(node.childWorlds[i]),
          upperMaxFronts,
          nodeOptimistic,
          tt,
          false,
          previousRootMu,
          stats,
          rootCutTriggered,
          childComplete);
        complete = complete && childComplete;

        if (! initialized)
        {
          mini = f;
          initialized = true;
        }
        else
          mini = ParetoFront::MinProduct(mini, f);

        const WorldMask nextUseful = currentUseful.Intersection(
          mini.UsefulWorlds());
        if (! (nextUseful == currentUseful))
          stats.usefulWorldUpdates++;
        currentUseful = nextUseful;

        const ParetoFront optimisticMini = mini.CompleteOptimistically(
          currentUseful,
          nodeOptimistic);
        if (optimisticMini.ValidWorlds().PopCount() > mini.ValidWorlds().PopCount())
          stats.optimisticCompletions++;

        if (! upperMaxFronts.empty() &&
            upperMaxFronts.back()->DominatesFront(optimisticMini))
        {
          stats.earlyCuts++;
          complete = false;
          exactComplete = false;
          break;
        }

        for (unsigned j = 0; j + 1 < upperMaxFronts.size(); j++)
        {
          if (upperMaxFronts[j]->DominatesFront(optimisticMini))
          {
            stats.deepAlphaCuts++;
            complete = false;
            exactComplete = false;
            return mini;
          }
        }
      }

      exactComplete = complete;
      if (exactComplete)
      {
        tt.Store(ttKey, mini);
        stats.ttStores++;
      }
      return mini;
    }

    /*
     * Original-paper Max path plus optimization-paper cut-on-win/root-cut:
     * merge each child front into the running front, stop early if a child now
     * wins in every useful world, and at the root stop iterative deepening once
     * the reported `mu` value stops changing.
     */
    ParetoFront front(node.leafFront.worldCount);
    bool complete = true;
    vector<const ParetoFront *> childUpperMaxFronts(upperMaxFronts);
    childUpperMaxFronts.push_back(&front);
    for (unsigned i = 0; i < node.children.size(); i++)
    {
      const WorldMask childWorlds = usefulWorlds.Intersection(node.childWorlds[i]);
      bool childComplete = false;
      const ParetoFront f = SearchToy(
        * node.children[i],
        maxMoves - 1,
        childWorlds,
        childUpperMaxFronts,
        nodeOptimistic,
        tt,
        false,
        previousRootMu,
        stats,
        rootCutTriggered,
        childComplete);
      complete = complete && childComplete;

      front = ParetoFront::MaxMerge(front, f);

      if (f.WinsAll(usefulWorlds))
      {
        stats.cutOnWinCuts++;
        complete = false;
        exactComplete = false;
        break;
      }

      if (isRoot && previousRootMu >= 0.0 &&
          fabs(front.Mu() - previousRootMu) < 1e-9)
      {
        stats.rootCuts++;
        rootCutTriggered = true;
        complete = false;
        exactComplete = false;
        break;
      }
    }

    exactComplete = complete;
    if (exactComplete)
    {
      tt.Store(ttKey, front);
      stats.ttStores++;
    }
    return front;
  }
  /**
   * Iteratively deepen the toy alpha-mu search in number of Max moves.
   *
   * The root `mu` value from the previous iteration feeds the root-cut test in
   * the next iteration, matching the prototype interpretation of the later paper.
   */
IterativeResult RunIterativeDeepening(
    const ToyNode& root,
    const int maxDepth)
  {
    IterativeResult result(root.leafFront.worldCount);
    double previousMu = -1.0;

    for (int depth = 1; depth <= maxDepth; depth++)
    {
      SearchStats stats;
      bool rootCutTriggered = false;
      bool exactComplete = false;
      TranspositionTable tt;
      const ParetoFront front = SearchToy(
        root,
        depth,
        WorldMask::All(root.leafFront.worldCount),
        vector<const ParetoFront *>(),
        OutcomeVector(root.leafFront.worldCount),
        tt,
        true,
        previousMu,
        stats,
        rootCutTriggered,
        exactComplete);

      result.front = front;
      result.depthReached = depth;
      result.rootCutTriggered = rootCutTriggered;
      result.statsPerDepth.push_back(stats);
      previousMu = front.Mu();

      if (rootCutTriggered)
        break;
    }

    return result;
  }
int BestScore(const futureTricks& fut)
  {
    if (fut.cards <= 0)
      return 0;

    int best = fut.score[0];
    for (int i = 1; i < fut.cards; i++)
      best = max(best, fut.score[i]);
    return best;
  }
void CheckDDS(const int ret, const string& tag)
  {
    if (ret == RETURN_NO_FAULT)
      return;

    char line[80];
    ErrorMessage(ret, line);
    ostringstream oss;
    oss << tag << " failed: " << line;
    Fail(oss.str());
  }
int ExactBridgeDDSScoreForWorld(
    const BridgeState& state,
    const unsigned worldIndex,
    const SearchExecutionContext& context)
  {
    futureTricks fut;
    memset(&fut, 0, sizeof(fut));

    const dealPBN deal = MakeDDSDealPBN(state, state.worlds[worldIndex]);
    const int ret = SolveBoardPBN(deal, -1, 1, 1, &fut,
      context.ddsThreadId);
    CheckDDS(ret, "SolveBoardPBN exact bridge DDS score");

    const int best = BestScore(fut);
    const int tricksRemaining = RemainingTricksInWorld(state,
      state.worlds[worldIndex]);
    const int maxAdditional =
      (SeatSide(state.playerToMove) == state.maxSide ?
        best : tricksRemaining - best);
    return state.maxTricksWon + maxAdditional;
  }
int ExactBridgeDDSScoreForWorld(
    const BridgeState& state,
    const unsigned worldIndex)
  {
    return ExactBridgeDDSScoreForWorld(state, worldIndex,
      SearchExecutionContext());
  }
void LoadHandFile(
    const string& fname,
    HandFileData& data);
int SolveDDSLeafWorld(
    const HandFileData& data,
    const int index,
    const int thrId);
BridgeState MakeBridgeStateFromDDSDeal(
    const dealPBN& deal)
  {
    BridgeState state;
    state.worlds.push_back(ParsePBNWorld(deal.remainCards));
    state.possibleWorlds = WorldMask(1, 0x1ULL);
    state.trumpSuit = (deal.trump == 4 ? -1 : deal.trump);
    state.trickLeader = deal.first;

    for (int i = 0; i < 3; i++)
    {
      if (deal.currentTrickRank[i] == 0)
        break;

      state.currentTrick.push_back(BridgeMove(
        deal.currentTrickSuit[i],
        RankFromDDSValue(deal.currentTrickRank[i])));
      state.currentTrickPlayers.push_back((deal.first + i) % 4);
    }

    state.leadSuit = (state.currentTrick.empty() ? -1 :
      state.currentTrick[0].suit);
    state.playerToMove =
      (deal.first + static_cast<int>(state.currentTrick.size())) % 4;
    state.maxSide = SeatSide(state.playerToMove);
    state.maxTricksWon = 0;
    return state;
  }
int SuitFromPlayChar(const char c)
  {
    switch (c)
    {
      case 'S': return SUIT_SPADES;
      case 'H': return SUIT_HEARTS;
      case 'D': return SUIT_DIAMONDS;
      case 'C': return SUIT_CLUBS;
      default:
        throw runtime_error("Unknown suit character in play string");
    }
  }
vector<PlayHistoryEvent> ParsePBNPlayHistory(
    const playTracePBN& play,
    const int openingLeader,
    const int trumpSuit)
  {
    vector<PlayHistoryEvent> events;
    const string cards(play.cards);
    if (cards.empty() || play.number <= 0)
      return events;

    const unsigned requiredLength = static_cast<unsigned>(play.number) * 2U;
    if (cards.size() < requiredLength)
    {
      throw runtime_error(
        "play string too short for declared play.number ("
        + to_string(cards.size()) + " chars for "
        + to_string(play.number) + " cards, need "
        + to_string(requiredLength) + ")");
    }

    if (play.number > 52)
      throw runtime_error("play.number exceeds maximum of 52 cards");

    if (openingLeader < 0 || openingLeader > 3)
      throw runtime_error("opening leader must be a valid seat (0-3)");

    const string validRanks = "AKQJT98765432";

    int currentPlayer = openingLeader;
    int leadSuit = -1;
    unsigned cardsInTrick = 0;
    vector<BridgeMove> trickCards;
    vector<int> trickPlayers;

    for (int i = 0; i < play.number; i++)
    {
      const unsigned pos = static_cast<unsigned>(i) * 2U;

      const char suitChar = cards[pos];
      if (suitChar != 'S' && suitChar != 'H' &&
          suitChar != 'D' && suitChar != 'C')
      {
        throw runtime_error(
          "invalid suit character '" + string(1, suitChar)
          + "' at position " + to_string(pos) + " in play string");
      }

      const int suit = SuitFromPlayChar(suitChar);
      const char rank = cards[pos + 1];

      if (validRanks.find(rank) == string::npos)
      {
        throw runtime_error(
          "invalid rank character '" + string(1, rank)
          + "' at position " + to_string(pos + 1) + " in play string");
      }

      if (cardsInTrick == 0)
        leadSuit = suit;

      events.push_back(PlayHistoryEvent(currentPlayer, leadSuit,
        BridgeMove(suit, rank)));

      trickCards.push_back(BridgeMove(suit, rank));
      trickPlayers.push_back(currentPlayer);
      cardsInTrick++;

      if (cardsInTrick == 4)
      {
        const unsigned winIdx = WinningCardIndex(trickCards, leadSuit, trumpSuit);
        currentPlayer = trickPlayers[winIdx];
        trickCards.clear();
        trickPlayers.clear();
        cardsInTrick = 0;
        leadSuit = -1;
      }
      else
      {
        currentPlayer = (currentPlayer + 1) % 4;
      }
    }
    return events;
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

    // Collect all played cards as a set for quick lookup
    set<pair<int,char> > allPlayed;
    for (unsigned i = 0; i < playedCards.size(); i++)
      allPlayed.insert(make_pair(playedCards[i].move.suit,
                                 playedCards[i].move.rank));

    // Build seedWorld with remaining cards for all four seats.
    // Visible seats (declarer, dummy): original hand minus played cards.
    // Hidden seats (lho, rho): empty — their remaining cards form the
    // hidden pool that the constructor will distribute.
    ParsedWorld seedWorld;
    for (int s = 0; s < 4; s++)
    {
      // Visible seats: remaining cards only
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

    // Compute hidden card pool: all cards NOT in visible remaining hands
    // and NOT played by anyone.  These are the defenders' remaining cards.
    spec.hiddenSeats.push_back(lho);
    spec.hiddenSeats.push_back(rho);

    // Do NOT use inferHiddenCardsFromVisibleHands — that would include
    // already-played cards in the hidden pool.  Instead, place the
    // defenders' remaining cards into the seedWorld hidden seats so
    // CollectSeedHiddenCards picks them up and redistributes them.
    spec.inferHiddenCardsFromVisibleHands = false;

    // Collect hidden pool: cards not in visible remaining hands, not played.
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

    // Distribute hidden cards across hidden seats in seedWorld according to
    // each defender's actual remaining count (13 minus cards they played).
    // This matters when the play prefix ends mid-trick: the hidden seats may
    // legitimately have different remaining counts at construction time, and we
    // must preserve those exact counts before later history replay narrows the
    // candidate worlds.
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
    const unsigned maxWorlds)
  {
    BridgeInformationState info;
    const ParsedWorld fullWorld = ParsePBNWorld(fullDeal.remainCards);
    const int dummySeat = (declarerSeat + 2) % 4;

    // Known-card constraints: all remaining visible-hand cards
    // (cards not yet played by declarer or dummy)
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

    // Split play history into completed tricks and current trick
    unsigned cardsInCurrentTrick = playedCards.size() % 4;
    unsigned completedCards = playedCards.size() - cardsInCurrentTrick;

    for (unsigned i = 0; i < completedCards; i++)
      info.playHistory.push_back(playedCards[i]);

    for (unsigned i = completedCards; i < playedCards.size(); i++)
      info.currentTrickHistory.push_back(playedCards[i]);

    info.deriveFollowSuitConstraints = true;
    info.deduplicateEquivalentWorlds = true;
    info.sampleLimit = maxWorlds;
    info.samplingSeed = 42;
    return info;
  }
  // `WorldMask` is backed by a single 64-bit word, so oversized constructor
  // pools must be compacted before they become a BridgeState. Use the same
  // canonical sort-and-offset sampling rule as the world-generation pipeline so
  // test expectations and debugging remain reproducible.
  static vector<unsigned> SelectDeterministicWorldIndices(
    const vector<ParsedWorld>& worlds,
    const vector<unsigned>& candidates,
    const unsigned limit,
    const unsigned samplingSeed)
  {
    if (limit == 0 || candidates.size() <= limit)
      return candidates;

    vector<unsigned> ordered(candidates);
    sort(ordered.begin(), ordered.end(),
      [&](const unsigned left, const unsigned right)
      {
        const string leftKey = SerializePBNWorld(worlds[left]);
        const string rightKey = SerializePBNWorld(worlds[right]);
        if (leftKey != rightKey)
          return leftKey < rightKey;
        return left < right;
      });

    vector<unsigned> selected;
    const unsigned offset = samplingSeed % static_cast<unsigned>(ordered.size());
    for (unsigned i = 0; i < limit; i++)
      selected.push_back(ordered[(offset + i) % ordered.size()]);

    sort(selected.begin(), selected.end());
    return selected;
  }
BridgeState MakeBridgeStateFromPartialInformation(
    const dealPBN& fullDeal,
    const int declarerSeat,
    const vector<PlayHistoryEvent>& playedCards,
    const unsigned maxWorlds)
  {
    const HistoryDerivedWorldSpec spec =
      BuildWorldSpecFromDeal(fullDeal, declarerSeat, playedCards);
    const BridgeInformationState info =
      BuildInformationStateFromPlay(fullDeal, declarerSeat, playedCards, maxWorlds);

    const HistoryDerivedConstructionResult constructed =
      ConstructCandidateWorldsFromHistory(spec, info);

    vector<ParsedWorld> worlds = constructed.worlds;

    if (worlds.empty())
    {
      return MakeBridgeStateFromDDSDeal(fullDeal);
    }

    // Filter worlds by follow-suit evidence from the play history.
    // If a defender discarded (played off-suit) at some point, they were
    // void in the led suit from that point onward and cannot hold remaining
    // cards in that suit.
    //
    // Build a void set: voidSuits[seat][suit] = true if the defender showed
    // out of that suit at any point during play.
    bool voidSuits[4][4];
    memset(voidSuits, 0, sizeof(voidSuits));
    for (unsigned i = 0; i < playedCards.size(); i++)
    {
      const PlayHistoryEvent& ev = playedCards[i];
      if (ev.leadSuit >= 0 && ev.move.suit != ev.leadSuit)
        voidSuits[ev.player][ev.leadSuit] = true;
    }

    vector<unsigned> survivingIndices;
    for (unsigned w = 0; w < worlds.size(); w++)
    {
      bool valid = true;
      for (unsigned h = 0; h < spec.hiddenSeats.size() && valid; h++)
      {
        const int seat = spec.hiddenSeats[h];
        for (int s = 0; s < 4 && valid; s++)
        {
          if (voidSuits[seat][s] && ! worlds[w].suits[seat][s].empty())
            valid = false;
        }
      }
      if (valid)
        survivingIndices.push_back(w);
    }

    if (survivingIndices.empty())
    {
      return MakeBridgeStateFromDDSDeal(fullDeal);
    }

    const unsigned worldLimit =
      min(64U, (maxWorlds == 0 ? 64U : maxWorlds));

    // Compact oversized candidate sets before wrapping them in WorldMask. This
    // avoids aliasing multiple constructor indices onto the same 64-bit mask bit
    // and keeps later DDS leaves on legal equal-hand-count worlds.
    if (worlds.size() > 64 || survivingIndices.size() > worldLimit)
    {
      const vector<unsigned> selected = SelectDeterministicWorldIndices(
        worlds, survivingIndices, worldLimit, info.samplingSeed);
      vector<ParsedWorld> compacted;
      compacted.reserve(selected.size());
      for (unsigned i = 0; i < selected.size(); i++)
        compacted.push_back(worlds[selected[i]]);
      worlds.swap(compacted);
    }

    WorldMask possibleMask;
    possibleMask.count = static_cast<unsigned>(worlds.size());
    possibleMask.bits = 0ULL;
    for (unsigned w = 0; w < worlds.size(); w++)
      possibleMask.bits |= (1ULL << w);

    // Build the state
    BridgeState state;
    state.worlds = worlds;
    state.possibleWorlds = possibleMask;

    const int trumpSuit = (fullDeal.trump == 4 ? -1 : fullDeal.trump);
    state.trumpSuit = trumpSuit;

    // Determine current trick and player from the tail of the play
    unsigned cardsInCurrentTrick = playedCards.size() % 4;
    if (cardsInCurrentTrick > 0)
    {
      unsigned trickStart = playedCards.size() - cardsInCurrentTrick;
      state.trickLeader = playedCards[trickStart].player;
      state.leadSuit = playedCards[trickStart].move.suit;
      for (unsigned i = trickStart; i < playedCards.size(); i++)
      {
        state.currentTrick.push_back(playedCards[i].move);
        state.currentTrickPlayers.push_back(playedCards[i].player);
      }
      state.playerToMove =
        (state.trickLeader + static_cast<int>(cardsInCurrentTrick)) % 4;
    }
    else if (! playedCards.empty())
    {
      // Last trick just completed; find the winner to determine next leader
      unsigned lastTrickStart = playedCards.size() - 4;
      vector<BridgeMove> lastTrick;
      vector<int> lastPlayers;
      for (unsigned i = lastTrickStart; i < playedCards.size(); i++)
      {
        lastTrick.push_back(playedCards[i].move);
        lastPlayers.push_back(playedCards[i].player);
      }
      const unsigned winIdx = WinningCardIndex(lastTrick,
        playedCards[lastTrickStart].move.suit, trumpSuit);
      state.trickLeader = lastPlayers[winIdx];
      state.playerToMove = state.trickLeader;
      state.leadSuit = -1;
    }
    else
    {
      state.trickLeader = fullDeal.first;
      state.playerToMove = fullDeal.first;
      state.leadSuit = -1;
    }

    state.maxSide = SeatSide(state.playerToMove);
    state.maxTricksWon = 0;
    return state;
  }
int SingleWorldFrontScore(
    const ParetoFront& front,
    const int depth,
    const int boardIndex)
  {
    Check(front.worldCount == 1,
      "DDS comparison should only inspect single-world alpha-mu fronts");
    Check(front.vectors.size() == 1,
      "single-world alpha-mu search should collapse to one exact vector in DDS comparison mode");
    Check(front.vectors[0].valid == WorldMask(1, 0x1ULL),
      "single-world alpha-mu comparison front should remain valid in the only world");

    (void) depth;
    (void) boardIndex;
    return front.vectors[0].values[0];
  }
unsigned BoardsToBenchmark(
    const HandFileData& data,
    const int maxBoards)
  {
    return static_cast<unsigned>(
      (maxBoards > 0 ? min(data.number, maxBoards) : data.number));
  }
set<unsigned> ParseSkippedBoardNumbers(
    const string& skipSpec,
    const unsigned availableBoards)
  {
    set<unsigned> skipped;
    if (skipSpec.empty())
      return skipped;

    const vector<string> parts = SplitString(skipSpec, ',', false);
    for (unsigned i = 0; i < parts.size(); i++)
    {
      const string token = parts[i];
      const size_t dash = token.find('-');
      if (dash == string::npos)
      {
        const int boardNumber = atoi(token.c_str());
        Check(boardNumber > 0,
          "benchmark skip-board spec should use positive 1-based board numbers");
        Check(static_cast<unsigned>(boardNumber) <= availableBoards,
          "benchmark skip-board spec should not reference a board beyond the selected hand-file range");
        skipped.insert(static_cast<unsigned>(boardNumber));
        continue;
      }

      const int startBoard = atoi(token.substr(0, dash).c_str());
      const int endBoard = atoi(token.substr(dash + 1).c_str());
      Check(startBoard > 0 && endBoard > 0 && startBoard <= endBoard,
        "benchmark skip-board ranges should be positive increasing 1-based ranges");
      Check(static_cast<unsigned>(endBoard) <= availableBoards,
        "benchmark skip-board range should not extend beyond the selected hand-file range");
      for (int boardNumber = startBoard; boardNumber <= endBoard; boardNumber++)
        skipped.insert(static_cast<unsigned>(boardNumber));
    }

    return skipped;
  }
vector<unsigned> SelectBenchmarkBoardNumbers(
    const HandFileData& data,
    const int maxBoards,
    const string& skipSpec)
  {
    const unsigned candidateBoards = BoardsToBenchmark(data, maxBoards);
    const set<unsigned> skipped = ParseSkippedBoardNumbers(skipSpec, candidateBoards);
    vector<unsigned> selectedBoards;
    for (unsigned boardNumber = 1; boardNumber <= candidateBoards; boardNumber++)
    {
      if (skipped.find(boardNumber) == skipped.end())
        selectedBoards.push_back(boardNumber);
    }
    return selectedBoards;
  }

  double BenchmarkProgressIntervalSeconds();

  namespace
  {
    struct AlphaMuBenchmarkBoardResult
    {
      unsigned boardNumber;
      double boardElapsedSeconds;
      double completionElapsedSeconds;
      unsigned mismatches;

      AlphaMuBenchmarkBoardResult() :
        boardNumber(0),
        boardElapsedSeconds(0.0),
        completionElapsedSeconds(0.0),
        mismatches(0)
      {
      }
    };

    int ClampDDSBenchmarkThreadId(
      const int requestedThreadId,
      const int configuredThreads)
    {
      if (configuredThreads <= 1)
        return 0;

      return min(max(0, requestedThreadId), configuredThreads - 1);
    }

    unsigned DetermineAlphaMuBoardWorkerCount(
      const AlphaMuBenchmarkOptions& options,
      const unsigned boardsToTest,
      int& baseThreadId)
    {
      if (options.parallelMode != ALPHA_MU_PARALLEL_BOARD || boardsToTest <= 1U)
      {
        baseThreadId = max(0, options.ddsThreadId);
        return 1U;
      }

      SetMaxThreads(max(1, options.boardWorkers));

      DDSInfo info;
      memset(&info, 0, sizeof(info));
      GetDDSInfo(&info);

      const int configuredThreads = max(1, info.noOfThreads);
      baseThreadId = ClampDDSBenchmarkThreadId(options.ddsThreadId,
        configuredThreads);
      const unsigned availableWorkers = static_cast<unsigned>(
        max(1, configuredThreads - baseThreadId));
      const unsigned requestedWorkers = static_cast<unsigned>(
        max(1, options.boardWorkers));
      return min(boardsToTest, min(requestedWorkers, availableWorkers));
    }

    AlphaMuBenchmarkBoardResult RunAlphaMuBenchmarkBoard(
      const HandFileData& data,
      const string& resolvedHandFile,
      const unsigned totalBoards,
      const unsigned boardNumber,
      const AlphaMuBenchmarkOptions& options,
      const int configuredBoardWorkers,
      const int ddsThreadId,
      const chrono::steady_clock::time_point benchmarkStart,
      const bool enableProgress)
    {
      const unsigned boardIndex = boardNumber - 1U;
      const chrono::steady_clock::time_point boardStart =
        chrono::steady_clock::now();
      const BridgeState state = MakeBridgeStateFromDDSDeal(data.dealList[boardIndex]);

      BenchmarkBoardProgressContext progress;
      if (enableProgress)
      {
        progress.method = "alpha_mu";
        progress.handFile = resolvedHandFile;
        progress.boardNumber = boardNumber;
        progress.totalBoards = totalBoards;
        progress.depth = options.depth;
        progress.parallelMode = options.parallelMode;
        progress.boardWorkers = options.boardWorkers;
        progress.rootWorkers = options.rootWorkers;
        progress.ddsThreadId = ddsThreadId;
        progress.configuredBoardWorkers = configuredBoardWorkers;
        progress.reportIntervalSeconds = BenchmarkProgressIntervalSeconds();
        progress.totalStart = benchmarkStart;
        progress.boardStart = boardStart;
        progress.nextReportSeconds = progress.reportIntervalSeconds;
      }

      const SearchExecutionContext searchContext = MakeSearchExecutionContext(
        ddsThreadId,
        (enableProgress && progress.reportIntervalSeconds > 0.0 ? &progress : NULL),
        options.parallelMode,
        options.boardWorkers,
        options.rootWorkers);

      const ParetoFront front = SearchBridgeState(state, options.depth,
        searchContext);
      const int alphaScore = SingleWorldFrontScore(front, options.depth,
        static_cast<int>(boardIndex));

      AlphaMuBenchmarkBoardResult result;
      result.boardNumber = boardNumber;
      result.boardElapsedSeconds = chrono::duration<double>(
        chrono::steady_clock::now() - boardStart).count();
      result.completionElapsedSeconds = chrono::duration<double>(
        chrono::steady_clock::now() - benchmarkStart).count();
      result.mismatches = static_cast<unsigned>(
        alphaScore != BestScore(data.futList[boardIndex]));
      return result;
    }
  }

double BenchmarkCheckpointIntervalSeconds();
void ReportBenchmarkCheckpoint(
    const BenchmarkMethodSummary& summary,
    const unsigned completedBoards,
    const double elapsedSeconds);
double BenchmarkProgressIntervalSeconds();
void ReportBenchmarkBoardTiming(
    const BenchmarkMethodSummary& summary,
    const unsigned boardNumber,
    const double boardElapsedSeconds,
    const double totalElapsedSeconds);
BenchmarkMethodSummary BenchmarkDDSExactBoards(
    const string& handFile,
    const int maxBoards,
    const string& skipSpec)
  {
    HandFileData data;
    LoadHandFile(handFile, data);
    Check(data.number > 0,
      "DDS benchmark requires at least one board in the selected hand file");

    BenchmarkMethodSummary summary;
    summary.method = "dds";
    summary.handFile = ResolvePath(handFile);
    summary.parallelMode = ALPHA_MU_PARALLEL_SERIAL;
    summary.boardWorkers = 1;
    summary.rootWorkers = 1;
    summary.ddsThreadId = 0;
    summary.configuredBoardWorkers = 1;
    const vector<unsigned> boardNumbers = SelectBenchmarkBoardNumbers(data, maxBoards,
      skipSpec);
    summary.boardsTested = static_cast<unsigned>(boardNumbers.size());
    Check(summary.boardsTested > 0,
      "DDS benchmark selected zero boards to test");

    SetMaxThreads(0);
    const double checkpointSeconds = BenchmarkCheckpointIntervalSeconds();
    const chrono::steady_clock::time_point start = chrono::steady_clock::now();
    double lastCheckpoint = 0.0;
    for (unsigned i = 0; i < boardNumbers.size(); i++)
    {
      const unsigned boardNumber = boardNumbers[i];
      const chrono::steady_clock::time_point boardStart =
        chrono::steady_clock::now();
      const unsigned boardIndex = boardNumber - 1U;
      const int score = SolveDDSLeafWorld(data, static_cast<int>(boardIndex), 0);
      if (score != BestScore(data.futList[boardIndex]))
        summary.mismatches++;

      const double boardElapsed = chrono::duration<double>(
        chrono::steady_clock::now() - boardStart).count();
      summary.perBoardSeconds.push_back(boardElapsed);
      const double elapsed = chrono::duration<double>(
        chrono::steady_clock::now() - start).count();
      ReportBenchmarkBoardTiming(summary, boardNumber, boardElapsed, elapsed);

      if (checkpointSeconds > 0.0)
      {
        if ((elapsed - lastCheckpoint >= checkpointSeconds) ||
            (i + 1 == boardNumbers.size()))
        {
          ReportBenchmarkCheckpoint(summary, i + 1, elapsed);
          lastCheckpoint = elapsed;
        }
      }
    }
    const chrono::steady_clock::time_point end = chrono::steady_clock::now();
    summary.elapsedSeconds = chrono::duration<double>(end - start).count();
    return summary;
  }
BenchmarkMethodSummary BenchmarkAlphaMuExactBoards(
    const AlphaMuBenchmarkOptions& rawOptions)
  {
    const AlphaMuBenchmarkOptions options =
      NormalizeAlphaMuBenchmarkOptions(rawOptions);
    Check(options.depth >= 0,
      "alpha-mu benchmark depth should be non-negative");

    HandFileData data;
    LoadHandFile(options.handFile, data);
    Check(data.number > 0,
      "alpha-mu benchmark requires at least one board in the selected hand file");

    BenchmarkMethodSummary summary;
    summary.method = "alpha_mu";
    summary.handFile = ResolvePath(options.handFile);
    summary.parallelMode = options.parallelMode;
    summary.boardWorkers = options.boardWorkers;
    summary.rootWorkers = options.rootWorkers;
    const vector<unsigned> boardNumbers = SelectBenchmarkBoardNumbers(data,
      options.maxBoards, options.skipSpec);
    summary.boardsTested = static_cast<unsigned>(boardNumbers.size());
    summary.depth = options.depth;
    Check(summary.boardsTested > 0,
      "alpha-mu benchmark selected zero boards to test");

    int baseThreadId = 0;
    const unsigned boardWorkerCount = DetermineAlphaMuBoardWorkerCount(options,
      summary.boardsTested, baseThreadId);
    if (boardWorkerCount <= 1U)
      SetMaxThreads(0);

    DDSInfo info;
    memset(&info, 0, sizeof(info));
    GetDDSInfo(&info);
    baseThreadId = ClampDDSBenchmarkThreadId(baseThreadId,
      max(1, info.noOfThreads));
    summary.ddsThreadId = baseThreadId;
    summary.configuredBoardWorkers = static_cast<int>(boardWorkerCount);

    const double checkpointSeconds = BenchmarkCheckpointIntervalSeconds();
    const chrono::steady_clock::time_point start = chrono::steady_clock::now();
    double lastCheckpoint = 0.0;

    if (boardWorkerCount <= 1U)
    {
      for (unsigned i = 0; i < boardNumbers.size(); i++)
      {
        const AlphaMuBenchmarkBoardResult result = RunAlphaMuBenchmarkBoard(data,
          summary.handFile, summary.boardsTested, boardNumbers[i], options,
          summary.configuredBoardWorkers,
          baseThreadId, start, true);
        summary.mismatches += result.mismatches;
        summary.perBoardSeconds.push_back(result.boardElapsedSeconds);
        ReportBenchmarkBoardTiming(summary, result.boardNumber,
          result.boardElapsedSeconds, result.completionElapsedSeconds);

        if (checkpointSeconds > 0.0)
        {
          if ((result.completionElapsedSeconds - lastCheckpoint >= checkpointSeconds) ||
              (i + 1 == boardNumbers.size()))
          {
            ReportBenchmarkCheckpoint(summary, i + 1,
              result.completionElapsedSeconds);
            lastCheckpoint = result.completionElapsedSeconds;
          }
        }
      }
    }
    else
    {
      vector<AlphaMuBenchmarkBoardResult> results(boardNumbers.size());
      atomic<unsigned> nextBoardIndex(0U);
      exception_ptr workerFailure;
      mutex workerFailureMutex;

      const auto worker = [&](const unsigned workerIndex)
      {
        try
        {
          while (true)
          {
            const unsigned index = nextBoardIndex.fetch_add(1U);
            if (index >= boardNumbers.size())
              break;

            results[index] = RunAlphaMuBenchmarkBoard(data, summary.handFile,
              summary.boardsTested, boardNumbers[index], options,
              summary.configuredBoardWorkers,
              baseThreadId + static_cast<int>(workerIndex), start, false);
          }
        }
        catch (...)
        {
          lock_guard<mutex> lock(workerFailureMutex);
          if (workerFailure == NULL)
            workerFailure = current_exception();
        }
      };

      vector<thread> workers;
      workers.reserve(boardWorkerCount);
      for (unsigned workerIndex = 0; workerIndex < boardWorkerCount; workerIndex++)
        workers.push_back(thread(worker, workerIndex));

      for (unsigned i = 0; i < workers.size(); i++)
        workers[i].join();

      if (workerFailure != NULL)
        rethrow_exception(workerFailure);

      double reportedElapsedSeconds = 0.0;
      for (unsigned i = 0; i < results.size(); i++)
      {
        summary.mismatches += results[i].mismatches;
        summary.perBoardSeconds.push_back(results[i].boardElapsedSeconds);
        reportedElapsedSeconds = max(reportedElapsedSeconds,
          results[i].completionElapsedSeconds);
        ReportBenchmarkBoardTiming(summary, results[i].boardNumber,
          results[i].boardElapsedSeconds, reportedElapsedSeconds);

        if (checkpointSeconds > 0.0)
        {
          if ((reportedElapsedSeconds - lastCheckpoint >= checkpointSeconds) ||
              (i + 1 == results.size()))
          {
            ReportBenchmarkCheckpoint(summary, i + 1, reportedElapsedSeconds);
            lastCheckpoint = reportedElapsedSeconds;
          }
        }
      }
    }

    const chrono::steady_clock::time_point end = chrono::steady_clock::now();
    summary.elapsedSeconds = chrono::duration<double>(end - start).count();
    return summary;
  }
BenchmarkMethodSummary BenchmarkAlphaMuExactBoards(
    const string& handFile,
    const int depth,
    const int maxBoards,
    const string& skipSpec)
  {
    AlphaMuBenchmarkOptions options;
    options.handFile = handFile;
    options.depth = depth;
    options.maxBoards = maxBoards;
    options.skipSpec = skipSpec;
    return BenchmarkAlphaMuExactBoards(options);
  }
void ReportBenchmarkMethodSummary(
    const BenchmarkMethodSummary& summary)
  {
    double sumPerBoard = 0.0;
    for (unsigned i = 0; i < summary.perBoardSeconds.size(); i++)
      sumPerBoard += summary.perBoardSeconds[i];

    cout.setf(ios::fixed);
    cout << setprecision(6);
    cout << "ALPHA_MU_BENCHMARK method=" << summary.method
         << " file=" << summary.handFile
         << " boards=" << summary.boardsTested
         << " depth=" << summary.depth
         << " parallel=" << AlphaMuParallelModeName(summary.parallelMode)
         << " board_workers=" << summary.boardWorkers
         << " root_workers=" << summary.rootWorkers
         << " dds_thread_id=" << summary.ddsThreadId
         << " configured_board_workers=" << summary.configuredBoardWorkers
         << " total_seconds=" << summary.elapsedSeconds
         << " per_board_seconds="
         << (summary.perBoardSeconds.empty() ?
             (summary.boardsTested == 0 ? 0.0 :
               summary.elapsedSeconds / static_cast<double>(summary.boardsTested)) :
             sumPerBoard / static_cast<double>(summary.perBoardSeconds.size()))
         << " mismatches=" << summary.mismatches
         << "\n";
    Check(summary.mismatches == 0,
      "benchmark mode should preserve the exact golden FUT score on every tested board");
  }
double BenchmarkCheckpointIntervalSeconds()
  {
    const char * value = getenv("DDS_ALPHA_MU_BENCHMARK_CHECKPOINT_SECONDS");
    if (value == NULL || *value == '\0')
      return 0.0;

    const double parsed = atof(value);
    return (parsed > 0.0 ? parsed : 0.0);
  }
double BenchmarkProgressIntervalSeconds()
  {
    const char * value = getenv("DDS_ALPHA_MU_BENCHMARK_PROGRESS_SECONDS");
    if (value != NULL && *value != '\0')
    {
      const double parsed = atof(value);
      if (parsed > 0.0)
        return parsed;
    }

    return BenchmarkCheckpointIntervalSeconds();
  }
void MaybeReportBenchmarkBoardProgress(
    const BridgeState& state,
    const int tricksRemaining,
    const SearchExecutionContext& context)
  {
    BenchmarkBoardProgressContext * progress = context.benchmarkProgress;
    if (progress == NULL || progress->reportIntervalSeconds <= 0.0)
    {
      return;
    }

    progress->recursiveCalls++;
    if ((progress->recursiveCalls & 0x3FFULL) != 0ULL)
      return;

    const double boardElapsed = chrono::duration<double>(
      chrono::steady_clock::now() - progress->boardStart).count();
    if (boardElapsed < progress->nextReportSeconds)
      return;

    const double totalElapsed = chrono::duration<double>(
      chrono::steady_clock::now() - progress->totalStart).count();

    cout.setf(ios::fixed);
    cout << setprecision(6);
    cout << "ALPHA_MU_BENCHMARK_PROGRESS method="
         << progress->method
         << " file=" << progress->handFile
         << " board=" << progress->boardNumber
         << " total_boards=" << progress->totalBoards
         << " depth=" << progress->depth
         << " parallel=" << AlphaMuParallelModeName(progress->parallelMode)
         << " board_workers=" << progress->boardWorkers
         << " root_workers=" << progress->rootWorkers
         << " dds_thread_id=" << progress->ddsThreadId
         << " configured_board_workers=" << progress->configuredBoardWorkers
         << " board_elapsed_seconds=" << boardElapsed
         << " elapsed_seconds=" << totalElapsed
         << " recursive_calls=" << progress->recursiveCalls
         << " dds_leaf_calls=" << progress->ddsLeafCalls
         << " tricks_remaining=" << tricksRemaining
         << " active_worlds=" << state.possibleWorlds.PopCount()
         << " current_trick_size=" << state.currentTrick.size()
         << " player=" << state.playerToMove
         << endl;

    do
    {
      progress->nextReportSeconds += progress->reportIntervalSeconds;
    }
    while (boardElapsed >= progress->nextReportSeconds);
  }
void ReportBenchmarkCheckpoint(
    const BenchmarkMethodSummary& summary,
    const unsigned completedBoards,
    const double elapsedSeconds)
  {
    cout.setf(ios::fixed);
    cout << setprecision(6);
    cout << "ALPHA_MU_BENCHMARK_CHECKPOINT method=" << summary.method
         << " file=" << summary.handFile
         << " completed_boards=" << completedBoards
         << " total_boards=" << summary.boardsTested
         << " depth=" << summary.depth
         << " parallel=" << AlphaMuParallelModeName(summary.parallelMode)
         << " board_workers=" << summary.boardWorkers
         << " root_workers=" << summary.rootWorkers
         << " dds_thread_id=" << summary.ddsThreadId
         << " configured_board_workers=" << summary.configuredBoardWorkers
         << " elapsed_seconds=" << elapsedSeconds
         << " mismatches=" << summary.mismatches
         << endl;
  }
void ReportBenchmarkBoardTiming(
    const BenchmarkMethodSummary& summary,
    const unsigned boardNumber,
    const double boardElapsedSeconds,
    const double totalElapsedSeconds)
  {
    cout.setf(ios::fixed);
    cout << setprecision(6);
    cout << "ALPHA_MU_BENCHMARK_BOARD method=" << summary.method
         << " file=" << summary.handFile
         << " board=" << boardNumber
         << " total_boards=" << summary.boardsTested
         << " depth=" << summary.depth
         << " parallel=" << AlphaMuParallelModeName(summary.parallelMode)
         << " board_workers=" << summary.boardWorkers
         << " root_workers=" << summary.rootWorkers
         << " dds_thread_id=" << summary.ddsThreadId
         << " configured_board_workers=" << summary.configuredBoardWorkers
         << " board_seconds=" << boardElapsedSeconds
         << " elapsed_seconds=" << totalElapsedSeconds
         << " mismatches=" << summary.mismatches
         << endl;
  }
DDSVsAlphaMuComparison CompareDDSAndAlphaMu(
    const string& handFile,
    const int maxDepth,
    const int maxBoards)
  {
    Check(maxDepth >= 0,
      "DDS comparison max depth should be non-negative");

    HandFileData data;
    LoadHandFile(handFile, data);
    Check(data.number > 0,
      "DDS comparison requires at least one board in the selected hand file");

    const unsigned boardsToTest = BoardsToBenchmark(data, maxBoards);
    Check(boardsToTest > 0,
      "DDS comparison selected zero boards to test");

    DDSVsAlphaMuComparison summary;
    summary.handFile = ResolvePath(handFile);
    summary.boardsTested = boardsToTest;

    SetMaxThreads(0);
    const chrono::steady_clock::time_point ddsStart =
      chrono::steady_clock::now();
    vector<int> ddsScores(boardsToTest, 0);
    for (unsigned i = 0; i < boardsToTest; i++)
      ddsScores[i] = SolveDDSLeafWorld(data, static_cast<int>(i), 0);
    const chrono::steady_clock::time_point ddsEnd =
      chrono::steady_clock::now();

    summary.ddsElapsedSeconds = chrono::duration<double>(ddsEnd - ddsStart).
      count();

    for (int depth = 0; depth <= maxDepth; depth++)
    {
      DDSVsAlphaMuDepthTiming depthTiming;
      depthTiming.depth = depth;

      const chrono::steady_clock::time_point alphaStart =
        chrono::steady_clock::now();
      for (unsigned i = 0; i < boardsToTest; i++)
      {
        const BridgeState state = MakeBridgeStateFromDDSDeal(data.dealList[i]);
        const ParetoFront front = SearchBridgeState(state, depth);
        const int alphaScore = SingleWorldFrontScore(front, depth,
          static_cast<int>(i));
        if (alphaScore != ddsScores[i])
          depthTiming.mismatches++;
      }
      const chrono::steady_clock::time_point alphaEnd =
        chrono::steady_clock::now();

      depthTiming.elapsedSeconds = chrono::duration<double>(
        alphaEnd - alphaStart).count();
      summary.alphaMuDepths.push_back(depthTiming);
    }

    return summary;
  }
void ReportDDSVsAlphaMuComparison(
    const DDSVsAlphaMuComparison& summary)
  {
    cout.setf(ios::fixed);
    cout << setprecision(6);
    cout << "ALPHA_MU_COMPARE file=" << summary.handFile
         << " boards=" << summary.boardsTested
         << " max_depth=" <<
      (summary.alphaMuDepths.empty() ? 0 : summary.alphaMuDepths.back().depth)
         << "\n";
    cout << "ALPHA_MU_COMPARE dds total_seconds=" << summary.ddsElapsedSeconds
         << " per_board_seconds="
         << (summary.boardsTested == 0 ? 0.0 :
             summary.ddsElapsedSeconds / static_cast<double>(summary.boardsTested))
         << "\n";

    for (unsigned i = 0; i < summary.alphaMuDepths.size(); i++)
    {
      const DDSVsAlphaMuDepthTiming& depthTiming = summary.alphaMuDepths[i];
      const double ratio =
        (summary.ddsElapsedSeconds <= 0.0 ? 0.0 :
          depthTiming.elapsedSeconds / summary.ddsElapsedSeconds);
      cout << "ALPHA_MU_COMPARE depth=" << depthTiming.depth
           << " total_seconds=" << depthTiming.elapsedSeconds
           << " per_board_seconds="
           << (summary.boardsTested == 0 ? 0.0 :
               depthTiming.elapsedSeconds /
                 static_cast<double>(summary.boardsTested))
           << " ratio_vs_dds=" << ratio
           << " mismatches=" << depthTiming.mismatches
           << "\n";
      Check(depthTiming.mismatches == 0,
        "DDS comparison should preserve the exact single-world score for every alpha-mu depth tested");
    }
  }
void LoadHandFile(
    const string& fname,
    HandFileData& data)
  {
    const string path = ResolvePath(fname);
    if (! read_file(path,
        data.number,
        data.GIBmode,
        &data.dealerList,
        &data.vulList,
        &data.dealList,
        &data.futList,
        &data.tableList,
        &data.parList,
        &data.dealerParList,
        &data.playList,
        &data.traceList))
    {
      Fail("read_file failed for " + path);
    }
  }
int SolveDDSLeafWorld(
    const HandFileData& data,
    const int index,
    const int thrId)
  {
    futureTricks fut;
    memset(&fut, 0, sizeof(fut));

    const int ret = SolveBoardPBN(data.dealList[index], -1, 1, 1, &fut, thrId);
    CheckDDS(ret, "SolveBoardPBN DDS leaf demo");

    return BestScore(fut);
  }
void CheckDDSLeafBestScores(
    const HandFileData& data,
    const vector<int>& bestScores)
  {
    Check(static_cast<int>(bestScores.size()) == data.number,
      "DDS leaf evaluation should return one score per world");

    for (int i = 0; i < data.number; i++)
    {
      const int goldenBest = BestScore(data.futList[i]);
      Check(bestScores[static_cast<unsigned>(i)] == goldenBest,
        "DDS leaf demo optimum should match the golden FUT optimum");
    }
  }
DDSLeafEvalResult EvaluateDDSLeafThresholdSerial(
    const HandFileData& data,
    const int target)
  {
    DDSLeafEvalResult result(static_cast<unsigned>(data.number));
    result.leaf.valid = WorldMask::All(static_cast<unsigned>(data.number));
    result.workerCount = 1;

    for (int i = 0; i < data.number; i++)
    {
      const int best = SolveDDSLeafWorld(data, i, 0);
      result.bestScores[static_cast<unsigned>(i)] = best;
      result.leaf.values[static_cast<unsigned>(i)] = (best >= target ? 1 : 0);
    }

    CheckDDSLeafBestScores(data, result.bestScores);
    return result;
  }
DDSLeafEvalResult EvaluateDDSLeafThresholdParallel(
    const HandFileData& data,
    const int target,
    const int requestedThreads)
  {
    DDSLeafEvalResult result(static_cast<unsigned>(data.number));
    result.leaf.valid = WorldMask::All(static_cast<unsigned>(data.number));

    DDSInfo info;
    memset(&info, 0, sizeof(info));
    GetDDSInfo(&info);

    (void) requestedThreads;
    Check(info.noOfThreads >= 1,
      "DDS should expose at least one configured thread for leaf parallelization");
    result.workerCount = max(1, min(data.number, info.noOfThreads));

    boardsPBN boards;
    memset(&boards, 0, sizeof(boards));
    boards.noOfBoards = data.number;

    for (int i = 0; i < data.number; i++)
    {
      boards.deals[i] = data.dealList[i];
      boards.target[i] = -1;
      boards.solutions[i] = 1;
      boards.mode[i] = 1;
    }

    solvedBoards solved;
    memset(&solved, 0, sizeof(solved));

    const int ret = SolveAllBoards(&boards, &solved);
    CheckDDS(ret, "SolveAllBoards DDS leaf demo");
    Check(solved.noOfBoards == data.number,
      "parallel DDS leaf evaluation should solve every requested world");

    for (int i = 0; i < data.number; i++)
    {
      const int best = BestScore(solved.solvedBoard[i]);
      result.bestScores[static_cast<unsigned>(i)] = best;
      result.leaf.values[static_cast<unsigned>(i)] = (best >= target ? 1 : 0);
    }

    CheckDDSLeafBestScores(data, result.bestScores);
    return result;
  }


  void PrintPrototypeStatus(const string& msg)
  {
    cout << kPrototypeMessagePrefix << msg << "\n";
  }

AlphaMuSolveResult SolveAlphaMu(
    const dealPBN& deal,
    const int declarerSeat,
    const playTracePBN& play,
    const int depth,
    const unsigned maxWorlds,
    const double timeBudgetSeconds)
  {
    AlphaMuSolveResult result;
    const int trumpSuit = (deal.trump == 4 ? -1 : deal.trump);

    const chrono::steady_clock::time_point totalStart =
      chrono::steady_clock::now();

    // Phase 1: parse play history and build partial-information state
    const chrono::steady_clock::time_point worldGenStart =
      chrono::steady_clock::now();

    const vector<PlayHistoryEvent> history =
      ParsePBNPlayHistory(play, deal.first, trumpSuit);

    const BridgeState state = MakeBridgeStateFromPartialInformation(
      deal, declarerSeat, history, maxWorlds);

    const chrono::steady_clock::time_point worldGenEnd =
      chrono::steady_clock::now();
    result.worldGenerationSeconds =
      chrono::duration<double>(worldGenEnd - worldGenStart).count();

    result.worldCount = static_cast<unsigned>(state.worlds.size());
    result.survivingWorldCount = state.possibleWorlds.PopCount();

    if (result.survivingWorldCount == 0)
    {
      result.totalSeconds =
        chrono::duration<double>(chrono::steady_clock::now() - totalStart).count();
      return result;
    }

    // Phase 2: determine remaining tricks and search
    const chrono::steady_clock::time_point searchStart =
      chrono::steady_clock::now();

    unsigned maxCards = 0;
    for (unsigned i = 0; i < state.worlds.size(); i++)
    {
      if (state.possibleWorlds.Has(i))
      {
        const unsigned c = WorldCardCount(state.worlds[i]);
        if (c > maxCards)
          maxCards = c;
      }
    }
    const int tricksRemaining = static_cast<int>(
      (maxCards + state.currentTrick.size()) / 4U);
    const int maxSearchDepth = (depth <= 0 || depth > tricksRemaining)
      ? tricksRemaining : depth;

    SetMaxThreads(0);
    InitZobrist();
    BridgeTranspositionTable tt(1U << 20);
    BridgeTTStats ttStats;
    const SearchExecutionContext context;

    BridgeRootReport bestReport(state.possibleWorlds.count);
    int bestDepth = 0;

    if (timeBudgetSeconds > 0.0 && maxSearchDepth > 1)
    {
      // Iterative deepening with time budget
      const BridgeRootReport* prevReport = NULL;
      for (int d = 1; d <= maxSearchDepth; d++)
      {
        const double elapsed =
          chrono::duration<double>(chrono::steady_clock::now() - searchStart).count();
        if (d > 1 && elapsed > timeBudgetSeconds)
          break;

        tt.Clear();
        BridgeTTStats iterStats;
        const BridgeRootReport report = AnalyzeBridgeRootWithTT(
          state, d, context, &tt, &iterStats, prevReport);

        ttStats.probes += iterStats.probes;
        ttStats.hits += iterStats.hits;
        ttStats.stores += iterStats.stores;

        bestReport = report;
        bestDepth = d;
        prevReport = &bestReport;

        const double iterElapsed =
          chrono::duration<double>(chrono::steady_clock::now() - searchStart).count();
        cout << "  ID depth=" << d
             << " mu=" << fixed << setprecision(4) << report.rootFront.Mu()
             << " vectors=" << report.rootFront.vectors.size()
             << " tt_hits=" << iterStats.hits
             << " tt_stores=" << iterStats.stores
             << " elapsed=" << setprecision(3) << iterElapsed << "s"
             << endl;
      }
    }
    else
    {
      // Single-pass search at requested depth
      bestReport = AnalyzeBridgeRootWithTT(
        state, maxSearchDepth, context, &tt, &ttStats);
      bestDepth = maxSearchDepth;
    }

    const chrono::steady_clock::time_point searchEnd =
      chrono::steady_clock::now();
    result.searchSeconds =
      chrono::duration<double>(searchEnd - searchStart).count();

    result.rootReport = bestReport;
    result.rootFront = bestReport.rootFront;
    result.depthSearched = bestDepth;
    result.valid = ! bestReport.rootFront.vectors.empty();
    result.ttProbes = ttStats.probes;
    result.ttHits = ttStats.hits;
    result.ttStores = ttStats.stores;

    // Choose the move with the highest mu
    if (! bestReport.children.empty())
    {
      double bestMu = -1.0;
      unsigned bestIdx = 0;
      for (unsigned i = 0; i < bestReport.children.size(); i++)
      {
        const double mu = bestReport.children[i].front.Mu();
        if (mu > bestMu)
        {
          bestMu = mu;
          bestIdx = i;
        }
      }
      result.chosenMove = bestReport.children[bestIdx].move;
    }

    result.totalSeconds =
      chrono::duration<double>(chrono::steady_clock::now() - totalStart).count();
    return result;
  }

void ReportAlphaMuSolveResult(const AlphaMuSolveResult& result)
  {
    cout.setf(ios::fixed);
    cout << setprecision(3);

    cout << "=== Alpha-Mu Solve Result ===" << endl;

    if (! result.valid)
    {
      cout << "  Status: no valid result (no surviving worlds or empty front)" << endl;
      cout << "  Total time: " << result.totalSeconds << "s" << endl;
      return;
    }

    cout << "  Chosen move: " << SuitName(result.chosenMove.suit)
         << " " << result.chosenMove.rank << endl;
    cout << "  Depth searched: " << result.depthSearched << " tricks" << endl;
    cout << "  Worlds: " << result.survivingWorldCount
         << " surviving / " << result.worldCount << " raw" << endl;
    cout << "  Root front: " << result.rootFront.vectors.size()
         << " vectors, mu=" << setprecision(4) << result.rootFront.Mu() << endl;

    cout << setprecision(3);
    cout << "  Timing: " << result.totalSeconds << "s total ("
         << result.worldGenerationSeconds << "s world-gen, "
         << result.searchSeconds << "s search)" << endl;
    cout << "  TT: " << result.ttStores << " stores, "
         << result.ttHits << " hits / " << result.ttProbes << " probes"
         << endl;

    if (! result.rootReport.children.empty())
    {
      cout << "  Candidate moves:" << endl;
      for (unsigned i = 0; i < result.rootReport.children.size(); i++)
      {
        const BridgeRootChildReport& child = result.rootReport.children[i];
        cout << "    " << SuitName(child.move.suit) << " " << child.move.rank
             << ": mu=" << setprecision(4) << child.front.Mu()
             << ", vectors=" << child.front.vectors.size()
             << ", worlds=" << child.validWorlds.PopCount()
             << endl;
      }
    }
    cout << setprecision(3);
  }
}
