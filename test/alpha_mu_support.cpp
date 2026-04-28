/**
 * @file alpha_mu_support.cpp
 * @brief Support utilities: constraint derivation, constructor pruning,
 *        CLI driver helpers, and test-infrastructure plumbing.
 *
 * Copyright 2026 by David Jenkins.  All rights reserved.
 */

#include "alpha_mu_core.h"

#include <cctype>
#include <fstream>

namespace alpha_mu
{
  using namespace std;

  namespace
  {
    string TrimText(const string& text)
    {
      size_t begin = 0;
      while (begin < text.size() &&
             isspace(static_cast<unsigned char>(text[begin])))
      {
        begin++;
      }

      size_t end = text.size();
      while (end > begin &&
             isspace(static_cast<unsigned char>(text[end - 1])))
      {
        end--;
      }
      return text.substr(begin, end - begin);
    }

    string UppercaseText(const string& text)
    {
      string upper(text);
      for (unsigned i = 0; i < upper.size(); i++)
      {
        upper[i] = static_cast<char>(toupper(
          static_cast<unsigned char>(upper[i])));
      }
      return upper;
    }

    bool TryParsePBNTagLine(
      const string& line,
      string& tag,
      string& value)
    {
      const string trimmed = TrimText(line);
      if (trimmed.size() < 4 || trimmed[0] != '[' ||
          trimmed[trimmed.size() - 1] != ']')
      {
        return false;
      }

      const size_t firstSpace = trimmed.find(' ');
      if (firstSpace == string::npos || firstSpace <= 1)
        return false;

      tag = trimmed.substr(1, firstSpace - 1);
      string rawValue = TrimText(trimmed.substr(firstSpace + 1,
        trimmed.size() - firstSpace - 2));
      if (rawValue.size() >= 2 && rawValue[0] == '"' &&
          rawValue[rawValue.size() - 1] == '"')
      {
        rawValue = rawValue.substr(1, rawValue.size() - 2);
      }
      value = rawValue;
      return true;
    }

    string StripBraceComments(const string& line)
    {
      string cleaned;
      int braceDepth = 0;
      for (unsigned i = 0; i < line.size(); i++)
      {
        if (line[i] == '{')
        {
          braceDepth++;
          continue;
        }
        if (line[i] == '}')
        {
          if (braceDepth > 0)
            braceDepth--;
          continue;
        }
        if (braceDepth == 0)
          cleaned.push_back(line[i]);
      }
      return cleaned;
    }

    bool IsCardToken(const string& token)
    {
      if (token.size() != 2)
        return false;

      const string upper = UppercaseText(token);
      const char suit = upper[0];
      const char rank = upper[1];
      return (suit == 'S' || suit == 'H' || suit == 'D' || suit == 'C') &&
        string("AKQJT98765432").find(rank) != string::npos;
    }

    void AppendPlayTokens(
      const string& line,
      vector<string>& tokens)
    {
      istringstream iss(StripBraceComments(line));
      string token;
      while (iss >> token)
      {
        const string upper = UppercaseText(token);
        if (upper.empty() || upper == "-" || upper == "*")
          continue;

        bool numericMarker = true;
        for (unsigned i = 0; i < upper.size(); i++)
        {
          if (! isdigit(static_cast<unsigned char>(upper[i])) &&
              upper[i] != '.')
          {
            numericMarker = false;
            break;
          }
        }
        if (numericMarker)
          continue;

        Check(IsCardToken(upper),
          string("unsupported token in PBN [Play] section: ") + token);
        tokens.push_back(upper);
      }
    }

    int ParsePBNSeatToken(const string& text)
    {
      const string upper = UppercaseText(TrimText(text));
      Check(upper.size() == 1,
        "PBN seat tags should be one of N, E, S, or W");
      return SeatIndex(upper[0]);
    }

    int ParsePBNTrumpToken(const string& text)
    {
      const string upper = UppercaseText(TrimText(text));
      if (upper == "NT" || upper == "N")
        return -1;
      Check(upper.size() == 1,
        "PBN contract denomination should be S, H, D, C, or NT");
      return SuitFromPlayChar(upper[0]);
    }

    void ParsePBNContract(
      const string& text,
      int& level,
      int& trumpSuit)
    {
      const string trimmed = UppercaseText(TrimText(text));
      Check(! trimmed.empty(),
        "PBN [Contract] should not be empty");
      Check(trimmed != "PASS" && trimmed != "AP" && trimmed != "ALLPASS",
        "PBN exact recommendation requires a played contract, not pass-out");

      size_t split = 0;
      while (split < trimmed.size() &&
             isdigit(static_cast<unsigned char>(trimmed[split])))
      {
        split++;
      }
      Check(split > 0 && split < trimmed.size(),
        "PBN [Contract] should look like 3NT, 4S, 5H, 6D, or 7C");

      level = atoi(trimmed.substr(0, split).c_str());
      Check(level >= 1 && level <= 7,
        "PBN contract level should be between 1 and 7");
      trumpSuit = ParsePBNTrumpToken(trimmed.substr(split));
    }
  }

  static string gAlphaMuExecutablePath = "./build/alpha_mu";

  void SetAlphaMuExecutablePath(const string& path)
  {
    gAlphaMuExecutablePath = path;
  }

  string GetAlphaMuExecutablePath()
  {
    return gAlphaMuExecutablePath;
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

  void EnableDefaultDecisionPointBridgeSearchControls(
    SearchExecutionContext& context)
  {
    context.bridgeSearch.enableAncestorCuts = true;
    context.bridgeSearch.requireExactTTFronts = true;
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

  PBNBoardRecord LoadPBNBoardRecord(const string& filePath)
  {
    ifstream fin(filePath.c_str());
    Check(fin.is_open(),
      string("could not open PBN file ") + filePath);

    PBNBoardRecord board;
    board.sourcePath = filePath;

    bool haveDeal = false;
    bool haveDeclarer = false;
    bool haveContract = false;
    bool inPlaySection = false;
    string playLeaderToken;
    vector<string> playTokens;

    string line;
    while (getline(fin, line))
    {
      string tag;
      string value;
      if (TryParsePBNTagLine(line, tag, value))
      {
        const string upperTag = UppercaseText(tag);
        if (upperTag == "DEAL" && haveDeal)
          break;

        inPlaySection = false;
        if (upperTag == "BOARD" && board.boardLabel.empty())
          board.boardLabel = TrimText(value);
        else if (upperTag == "DEAL" && ! haveDeal)
        {
          const string dealText = UppercaseText(TrimText(value));
          Check(! dealText.empty(),
            "PBN [Deal] should not be empty");
          Check(dealText.size() < sizeof(board.deal.remainCards),
            "PBN [Deal] should fit into dealPBN.remainCards");
          strcpy(board.deal.remainCards, dealText.c_str());
          haveDeal = true;
        }
        else if (upperTag == "DECLARER" && ! haveDeclarer)
        {
          board.declarerSeat = ParsePBNSeatToken(value);
          haveDeclarer = true;
        }
        else if (upperTag == "CONTRACT" && ! haveContract)
        {
          int trumpSuit = -1;
          ParsePBNContract(value, board.contractLevel, trumpSuit);
          board.deal.trump = (trumpSuit < 0 ? 4 : trumpSuit);
          haveContract = true;
        }
        else if (upperTag == "PLAY")
        {
          playLeaderToken = TrimText(value);
          inPlaySection = true;
        }
        continue;
      }

      if (inPlaySection)
        AppendPlayTokens(line, playTokens);
    }

    Check(haveDeal,
      "PBN exact recommendation requires a [Deal] tag");
    Check(haveDeclarer,
      "PBN exact recommendation requires a [Declarer] tag");
    Check(haveContract,
      "PBN exact recommendation requires a [Contract] tag");
    ValidateFullDealPBN(board.deal, "PBN [Deal]");

    board.deal.first = (board.declarerSeat + 1) % 4;
    if (! playLeaderToken.empty())
    {
      const int playLeader = ParsePBNSeatToken(playLeaderToken);
      Check(playLeader == board.deal.first,
        "PBN [Play] leader should match the opening leader implied by [Declarer]");
    }

    if (! playTokens.empty())
    {
      playTracePBN play;
      memset(&play, 0, sizeof(play));
      play.number = static_cast<int>(playTokens.size());

      string cards;
      for (unsigned i = 0; i < playTokens.size(); i++)
        cards += playTokens[i];
      Check(cards.size() < sizeof(play.cards),
        "PBN [Play] section should fit into playTracePBN.cards");
      strcpy(play.cards, cards.c_str());

      board.playHistory = ParsePBNPlayHistory(play, board.deal.first,
        (board.deal.trump == 4 ? -1 : board.deal.trump));
    }

    return board;
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

  void ValidateFullDealPBN(
    const dealPBN& deal,
    const string& context)
  {
    const ParsedWorld world = ParsePBNWorld(deal.remainCards);
    const string ranks = "AKQJT98765432";
    bool seen[4][13];
    memset(seen, 0, sizeof(seen));

    unsigned seatCounts[4] = {0, 0, 0, 0};
    for (int seat = 0; seat < 4; seat++)
    {
      for (int suit = 0; suit < 4; suit++)
      {
        const string& cards = world.suits[seat][suit];
        for (unsigned i = 0; i < cards.size(); i++)
        {
          const size_t rankIndex = ranks.find(cards[i]);
          Check(rankIndex != string::npos,
            context + " should use only standard bridge ranks in remainCards");
          Check(! seen[suit][rankIndex],
            context + " should not contain duplicate card " +
            CardName(BridgeMove(suit, cards[i])));
          seen[suit][rankIndex] = true;
          seatCounts[seat]++;
        }
      }
    }

    if (seatCounts[SEAT_NORTH] != 13 || seatCounts[SEAT_EAST] != 13 ||
        seatCounts[SEAT_SOUTH] != 13 || seatCounts[SEAT_WEST] != 13)
    {
      ostringstream oss;
      oss << context
          << " should describe a full 52-card deal with 13 cards per seat, but counts were N="
          << seatCounts[SEAT_NORTH]
          << " E=" << seatCounts[SEAT_EAST]
          << " S=" << seatCounts[SEAT_SOUTH]
          << " W=" << seatCounts[SEAT_WEST];
      Fail(oss.str());
    }
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

  string PlausibilityHintLabel(const WorldPlausibilityHint& hint)
  {
    if (! hint.label.empty())
      return hint.label;
    return ConstraintToString(hint.constraint);
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
        if (playedCount[event.player][event.leadSuit] == 0)
        {
          constraints.push_back(WorldConstraint::VoidSuit(
            event.player,
            event.leadSuit));
        }
        else
        {
          constraints.push_back(WorldConstraint::MaxLength(
            event.player,
            event.leadSuit,
            playedCount[event.player][event.leadSuit]));
        }
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
          constraint.kind != CONSTRAINT_NOT_HAS_CARD &&
          constraint.kind != CONSTRAINT_VOID_SUIT)
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
        else if (constraint.kind == CONSTRAINT_NOT_HAS_CARD)
          hiddenCards[j].allowedSeatsMask &= ~SeatBit(constraint.player);
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
}

