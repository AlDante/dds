/*
   alpha_mu, an alpha-mu bridge solver

   Copyright © 2026 by David Jenkins
   All rights reserved.
*/

#include "alpha_mu_core.h"
#include "alpha_mu_tests.h"

namespace alpha_mu
{
  using namespace std;

  static string ShellQuote(const string& text)
  {
    string quoted("'");
    for (unsigned i = 0; i < text.size(); i++)
    {
      if (text[i] == '\'')
        quoted += "'\\''";
      else
        quoted += text[i];
    }
    quoted += "'";
    return quoted;
  }

  static string ReadWholeFile(const string& path)
  {
    FILE * fp = fopen(path.c_str(), "rb");
    Check(fp != NULL,
      "debug assertion regression should be able to read its captured child-process output");
    string contents;
    char buffer[1024];
    while (true)
    {
      const size_t n = fread(buffer, 1, sizeof(buffer), fp);
      if (n == 0)
        break;
      contents.append(buffer, n);
    }
    fclose(fp);
    return contents;
  }

  static void TestParetoInsert()
  {
    ParetoFront front(3);
    front.Insert(MakeBinaryOutcome("100"));
    front.Insert(MakeBinaryOutcome("011"));
    front.Insert(MakeBinaryOutcome("110"));

    Check(front.vectors.size() == 2,
      "Pareto insert should remove dominated vectors");
    Check(FrontContains(front, MakeBinaryOutcome("110")),
      "front should contain [1 1 0]");
    Check(FrontContains(front, MakeBinaryOutcome("011")),
      "front should contain [0 1 1]");
  }


  static void TestNonLocalityExample()
  {
    ToyNode leaf100("leaf100", TOY_LEAF, 3);
    leaf100.leafFront = MakeFront(3, vector<string>(1, "100"));

    ToyNode leaf011("leaf011", TOY_LEAF, 3);
    leaf011.leafFront = MakeFront(3, vector<string>(1, "011"));

    ToyNode leaf000a("leaf000a", TOY_LEAF, 3);
    leaf000a.leafFront = MakeFront(3, vector<string>(1, "000"));

    ToyNode leaf000b("leaf000b", TOY_LEAF, 3);
    leaf000b.leafFront = MakeFront(3, vector<string>(1, "000"));

    ToyNode d("d", TOY_MAX, 3);
    AddChild(d, leaf100);
    AddChild(d, leaf011);

    ToyNode e("e", TOY_MAX, 3);
    AddChild(e, leaf000a);
    AddChild(e, leaf100);

    ToyNode b("b", TOY_MIN, 3);
    AddChild(b, d);
    AddChild(b, e);

    ToyNode f("f", TOY_MAX, 3);
    AddChild(f, leaf000b);

    ToyNode c("c", TOY_MIN, 3);
    AddChild(c, f);

    ToyNode a("a", TOY_MAX, 3);
    AddChild(a, b);
    AddChild(a, c);

    SearchStats stats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront front = SearchToy(a, 2, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), tt, true, -1.0, stats,
      rootCutTriggered, exactComplete);

    Check(front.vectors.size() == 1,
      "non-locality example should collapse to one best root vector");
    Check(FrontContains(front, MakeBinaryOutcome("100")),
      "non-locality example should prefer [1 0 0] at the root");
    Check(! rootCutTriggered,
      "non-locality example should not need a root cut");
  }


  static void TestEarlyCutExample()
  {
    ToyNode bestLeaf("bestLeaf", TOY_LEAF, 3);
    {
      vector<string> fronts;
      fronts.push_back("110");
      fronts.push_back("011");
      bestLeaf.leafFront = MakeFront(3, fronts);
    }

    ToyNode cutLeaf1("cutLeaf1", TOY_LEAF, 3);
    cutLeaf1.leafFront = MakeFront(3, vector<string>(1, "110"));

    ToyNode shouldNotVisit("shouldNotVisit", TOY_LEAF, 3);
    shouldNotVisit.leafFront = MakeFront(3, vector<string>(1, "001"));

    ToyNode candidateMin("candidateMin", TOY_MIN, 3);
    AddChild(candidateMin, cutLeaf1);
    AddChild(candidateMin, shouldNotVisit);

    ToyNode root("root", TOY_MAX, 3);
    AddChild(root, bestLeaf);
    AddChild(root, candidateMin);

    SearchStats stats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront front = SearchToy(root, 2, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), tt, true, -1.0, stats,
      rootCutTriggered, exactComplete);

    Check(front.vectors.size() == 2,
      "early-cut example should preserve the first move's Pareto front");
    Check(stats.earlyCuts == 1,
      "early-cut example should trigger exactly one early cut");
    Check(find(stats.visitOrder.begin(), stats.visitOrder.end(),
      string("shouldNotVisit")) == stats.visitOrder.end(),
      "early cut should stop before visiting the dominated continuation");
    Check(! rootCutTriggered,
      "early-cut example should not trigger a root cut");
  }


  static void TestUsefulWorldMaintenance()
  {
    ToyNode minFirst("minFirst", TOY_LEAF, 3);
    minFirst.leafFront = MakeFront(3, vector<string>(1, "101"));

    ToyNode minSecond("minSecond", TOY_LEAF, 3);
    minSecond.leafFront = MakeFront(3, vector<string>(1, "010"));

    ToyNode candidateMin("candidateMin", TOY_MIN, 3);
    AddChild(candidateMin, minFirst);
    AddChild(candidateMin, minSecond);

    SearchStats stats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront front = SearchToy(candidateMin, 2, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), tt, false, -1.0, stats,
      rootCutTriggered, exactComplete);

    Check(FrontContains(front, MakeBinaryOutcome("000")),
      "useful-world example should reduce the Min continuation to [0 0 0]");
    Check(stats.usefulWorldUpdates == 2,
      "useful-world example should record two useful-world updates at the Min node");
    Check(stats.leafWorldEvaluations == 5,
      "useful-world example should evaluate only 5 leaf worlds instead of 6");
    Check(! rootCutTriggered,
      "useful-world example should not trigger a root cut");
  }


  static void TestRootCutExample()
  {
    ToyNode stableBest("stableBest", TOY_LEAF, 3);
    stableBest.leafFront = MakeFront(3, vector<string>(1, "110"));

    ToyNode skippedByRootCut("skippedByRootCut", TOY_LEAF, 3);
    skippedByRootCut.leafFront = MakeFront(3, vector<string>(1, "001"));

    ToyNode root("root", TOY_MAX, 3);
    AddChild(root, stableBest);
    AddChild(root, skippedByRootCut);

    const IterativeResult result = RunIterativeDeepening(root, 2);

    Check(result.depthReached == 2,
      "iterative deepening should reach the second depth before cutting");
    Check(result.rootCutTriggered,
      "root-cut example should trigger a root cut");
    Check(result.statsPerDepth.size() == 2,
      "iterative deepening should record two depth passes");
    Check(result.statsPerDepth[1].rootCuts == 1,
      "second depth pass should contain one root cut");
    Check(find(result.statsPerDepth[1].visitOrder.begin(),
      result.statsPerDepth[1].visitOrder.end(),
      string("skippedByRootCut")) == result.statsPerDepth[1].visitOrder.end(),
      "root cut should stop before the second root move is searched");
  }


  static void TestWorldCuts()
  {
    ToyNode zeroLeaf("zeroLeaf", TOY_LEAF, 3);
    zeroLeaf.leafFront = MakeFront(3, vector<string>(1, "101"));

    SearchStats zeroStats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront zeroFront = SearchToy(
      zeroLeaf,
      1,
      WorldMask::None(3),
      vector<const ParetoFront *>(),
      OutcomeVector(3),
      tt,
      false,
      -1.0,
      zeroStats,
      rootCutTriggered,
      exactComplete);

    Check(zeroStats.worldCutsZero == 1,
      "empty useful-world mask should trigger a zero-world cut");
    Check(zeroStats.leafWorldEvaluations == 0,
      "zero-world cut should avoid all leaf evaluations");
    Check(FrontContains(zeroFront, MakeBinaryOutcome("000")),
      "zero-world cut should return the all-zero vector");

    ToyNode left("left", TOY_LEAF, 3);
    left.leafFront = MakeFront(3, vector<string>(1, "010"));

    ToyNode right("right", TOY_LEAF, 3);
    right.leafFront = MakeFront(3, vector<string>(1, "001"));

    ToyNode root("root", TOY_MAX, 3);
    AddChild(root, left);
    AddChild(root, right);

    SearchStats singleStats;
    const WorldMask onlyWorld1(3, 1ULL << 1);
    const ParetoFront singleFront = SearchToy(
      root,
      1,
      onlyWorld1,
      vector<const ParetoFront *>(),
      OutcomeVector(3),
      tt,
      true,
      -1.0,
      singleStats,
      rootCutTriggered,
      exactComplete);

    Check(singleStats.worldCutsSingle == 1,
      "single useful world should trigger a single-world cut");
    Check(singleStats.leafWorldEvaluations == 2,
      "single-world cut should evaluate only one world through the collapsed search");
    Check(FrontContains(singleFront, MakeBinaryOutcome("x1x")),
      "single-world cut should return the exact one-world result as a sparse vector");
  }


  static void TestParetoFrontTT()
  {
    ToyNode sharedLeaf("sharedLeaf", TOY_LEAF, 3);
    sharedLeaf.leafFront = MakeFront(3, vector<string>(1, "110"));

    ToyNode root("root", TOY_MAX, 3);
    AddChild(root, sharedLeaf);
    AddChild(root, sharedLeaf);

    SearchStats stats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront front = SearchToy(root, 1, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), tt, true, -1.0, stats,
      rootCutTriggered, exactComplete);

    Check(exactComplete,
      "transposition-table example should complete the shared subtree exactly");
    Check(stats.ttHits == 1,
      "transposition-table example should record exactly one TT hit on the repeated subtree");
    Check(stats.leafWorldEvaluations == 3,
      "transposition-table example should evaluate the shared leaf only once");
    Check(stats.ttStores >= 2,
      "transposition-table example should store both subtree and root fronts");
    Check(tt.entries.size() >= 2,
      "transposition-table example should keep at least the shared leaf and root entries");
    Check(FrontContains(front, MakeBinaryOutcome("110")),
      "transposition-table example should preserve the repeated leaf outcome");
    Check(! rootCutTriggered,
      "transposition-table example should not trigger a root cut");
  }


  static void TestPossibleWorldGeneration()
  {
    vector<string> pbns;
    pbns.push_back(
      "N:AKQ2.JT9.AKQ.JT9 765.8765.JT9.876 JT98.AKQ.432.AKQ 43.432.8765.5432");
    pbns.push_back(
      "N:AKQ2.JT9.AKQ.JT9 7654.876.JT9.876 JT98.AKQ.432.AKQ 3.5432.8765.5432");
    pbns.push_back(
      "N:AKQ2.JT9.AKQ.JT9 76543.876.JT.876 JT98.AKQ.432.AKQ .5432.98765.5432");
    pbns.push_back(
      "N:AKQ2.JT9.AKQ.JT9 76543..JT987.876 JT98.AKQ.432.AKQ .8765432.65.5432");

    vector<ParsedWorld> worlds;
    for (unsigned i = 0; i < pbns.size(); i++)
      worlds.push_back(ParsePBNWorld(pbns[i]));

    Check(WorldHasCard(worlds[0], SEAT_NORTH, SUIT_SPADES, 'A'),
      "possible-world parser should preserve known declarer cards");
    Check(WorldSuitLength(worlds[3], SEAT_WEST, SUIT_SPADES) == 0,
      "possible-world parser should preserve empty suits");

    vector<ParsedWorld> biddingWorlds;
    biddingWorlds.push_back(ParsePBNWorld(
      "N:T987.8765.432.32 AQJ3.KQ2.K98.654 6543.T43.A65.KQJ 2.A9.KQJT7.T9876"));
    biddingWorlds.push_back(ParsePBNWorld(
      "N:T987.8765.432.32 KQ32.AJ2.Q98.A54 6543.T43.A65.KQJ 2.A9.KQJT7.T9876"));
    biddingWorlds.push_back(ParsePBNWorld(
      "N:T987.8765.432.32 AKQ2.JQ3.Q98.A54 6543.T43.A65.KQJ 2.A9.KQJT7.T9876"));
    biddingWorlds.push_back(ParsePBNWorld(
      "N:T987.8765.432.32 AKQJ9.2.9876.543 6543.T43.A65.KQJ 2.A9.KQJT7.T9876"));

    Check(WorldHighCardPoints(biddingWorlds[0], SEAT_EAST) == 15,
      "bidding-style world generation should count East's high-card points correctly");
    Check(WorldHasBalancedShape(biddingWorlds[1], SEAT_EAST),
      "bidding-style world generation should recognize a balanced East hand shape");
    Check(! WorldHasBalancedShape(biddingWorlds[3], SEAT_EAST),
      "bidding-style world generation should reject clearly unbalanced hand shapes");

    vector<ParsedWorld> handTypeWorlds;
    handTypeWorlds.push_back(ParsePBNWorld(
      "N:... AKQJ.T98.765.432 ... ..."));
    handTypeWorlds.push_back(ParsePBNWorld(
      "N:... AKQJT4.9.654.432 ... ..."));
    handTypeWorlds.push_back(ParsePBNWorld(
      "N:... AKQJT.98765.4.32 ... ..."));
    handTypeWorlds.push_back(ParsePBNWorld(
      "N:... AKQJ.T987.6543.2 ... ..."));

    Check(WorldHasHandType(handTypeWorlds[0], SEAT_EAST, HAND_TYPE_BALANCED),
      "hand-type classification should recognize a balanced East hand");
    Check(WorldHasHandType(handTypeWorlds[1], SEAT_EAST, HAND_TYPE_ONE_SUITER),
      "hand-type classification should recognize a one-suiter East hand");
    Check(WorldHasHandType(handTypeWorlds[2], SEAT_EAST, HAND_TYPE_TWO_SUITER),
      "hand-type classification should recognize a two-suiter East hand");
    Check(WorldHasHandType(handTypeWorlds[3], SEAT_EAST, HAND_TYPE_THREE_SUITER),
      "hand-type classification should recognize a three-suiter East hand");

    BridgeInformationState oneNoTrumpInfo;
    oneNoTrumpInfo.biddingConstraints.push_back(
      WorldConstraint::Balanced(SEAT_EAST));
    oneNoTrumpInfo.biddingConstraints.push_back(
      WorldConstraint::MinHCP(SEAT_EAST, 15));
    oneNoTrumpInfo.biddingConstraints.push_back(
      WorldConstraint::MaxHCP(SEAT_EAST, 17));
    WorldGenerationStats oneNoTrumpStats;
    const WorldMask oneNoTrumpMask = GeneratePossibleWorlds(biddingWorlds,
      oneNoTrumpInfo, &oneNoTrumpStats);
    Check(oneNoTrumpMask == WorldMask(4, 0x3U),
      "1NT-style balanced 15-17 HCP bidding constraints should keep only the balanced medium-strength East worlds");
    Check(oneNoTrumpStats.afterBiddingCount == 2,
      "bidding-style HCP and balanced-shape constraints should leave exactly two matching worlds");

    BridgeInformationState handTypeInfo;
    handTypeInfo.biddingConstraints.push_back(
      WorldConstraint::HandTypeConstraint(SEAT_EAST, HAND_TYPE_TWO_SUITER));
    WorldGenerationStats handTypeStats;
    const WorldMask handTypeMask = GeneratePossibleWorlds(handTypeWorlds,
      handTypeInfo, &handTypeStats);
    Check(handTypeMask == WorldMask(4, 0x4U),
      "hand-type-only bidding constraints should keep only the two-suiter world");
    Check(handTypeStats.afterBiddingCount == 1,
      "hand-type-only bidding constraints should narrow the world pool during the bidding stage");

    const WorldGenerationExplanation handTypeExplanation =
      ExplainPossibleWorldGeneration(handTypeWorlds, handTypeInfo);
    Check(! handTypeExplanation.worlds[0].accepted &&
          handTypeExplanation.worlds[0].rejectionStage == "bidding" &&
          handTypeExplanation.worlds[0].rejectionReason.find("two-suiter") != string::npos,
      "hand-type-only explanation should reject non-matching hand types at the bidding stage");

    BridgeInformationState biddingInfo;
    biddingInfo.biddingConstraints.push_back(WorldConstraint::MinLength(
      SEAT_EAST, SUIT_SPADES, 5));
    WorldGenerationStats biddingStats;
    const WorldMask biddingMask = GeneratePossibleWorlds(worlds, biddingInfo,
      &biddingStats);
    Check(biddingMask == WorldMask(4, 0xCU),
      "bidding-style spade-length constraint should keep the last two worlds");
    Check(biddingStats.candidateWorldCount == 4,
      "world-generation stats should count the initial candidate pool");
    Check(biddingStats.afterBiddingCount == 2,
      "world-generation stats should record the post-bidding surviving worlds");

    BridgeInformationState lengthRangeInfo;
    lengthRangeInfo.biddingConstraints.push_back(WorldConstraint::MinLength(
      SEAT_EAST, SUIT_SPADES, 5));
    lengthRangeInfo.biddingConstraints.push_back(WorldConstraint::MaxLength(
      SEAT_EAST, SUIT_SPADES, 5));
    lengthRangeInfo.biddingConstraints.push_back(WorldConstraint::MaxLength(
      SEAT_EAST, SUIT_HEARTS, 0));
    WorldGenerationStats lengthRangeStats;
    const WorldMask lengthRangeMask = GeneratePossibleWorlds(worlds,
      lengthRangeInfo, &lengthRangeStats);
    Check(lengthRangeMask == WorldMask(4, 0x8U),
      "suit-length-only bidding constraints should keep only the world matching the supplied East suit-length bounds");
    Check(lengthRangeStats.afterBiddingCount == 1,
      "suit-length-range bidding constraints should narrow the world pool during the bidding stage");

    vector<ParsedWorld> combinedAuctionWorlds;
    combinedAuctionWorlds.push_back(ParsePBNWorld(
      "N:... AKQJT4.9.654.432 ... ..."));
    combinedAuctionWorlds.push_back(ParsePBNWorld(
      "N:... AKQJT.98765.4.32 ... ..."));
    combinedAuctionWorlds.push_back(ParsePBNWorld(
      "N:... KQJT9.9.7654.432 ... ..."));

    BridgeInformationState combinedAuctionInfo;
    combinedAuctionInfo.biddingConstraints.push_back(
      WorldConstraint::MinHCP(SEAT_EAST, 10));
    combinedAuctionInfo.biddingConstraints.push_back(
      WorldConstraint::MaxHCP(SEAT_EAST, 10));
    combinedAuctionInfo.biddingConstraints.push_back(
      WorldConstraint::HandTypeConstraint(SEAT_EAST, HAND_TYPE_ONE_SUITER));
    combinedAuctionInfo.biddingConstraints.push_back(
      WorldConstraint::MinLength(SEAT_EAST, SUIT_SPADES, 6));
    combinedAuctionInfo.biddingConstraints.push_back(
      WorldConstraint::MaxLength(SEAT_EAST, SUIT_SPADES, 6));
    const WorldMask combinedAuctionMask = GeneratePossibleWorlds(
      combinedAuctionWorlds, combinedAuctionInfo, NULL);
    Check(combinedAuctionMask == WorldMask(3, 0x1U),
      "combined HCP, hand-type, and suit-length bidding constraints should identify a single matching world");

    vector<ParsedWorld> partnershipWorlds;
    partnershipWorlds.push_back(ParsePBNWorld(
      "N:AKQ.JT9.AKQ.JT9 765.98765.JT9.87 JT98.AKQ.432.AKQ 43.432.8765.5432"));
    partnershipWorlds.push_back(ParsePBNWorld(
      "N:AKQ.JT9.AKQ.JT9 7654.8765.JT9.87 JT98.AKQ.432.AKQ 32.432.8765.5432"));
    partnershipWorlds.push_back(ParsePBNWorld(
      "N:AKQ.JT9.AKQ.JT9 76543.87.JT9.876 JT98.AKQ.432.AKQ 2.432.8765.65432"));

    BridgeInformationState partnershipInfo;
    partnershipInfo.biddingConstraints.push_back(
      WorldConstraint::PartnershipMinLength(SEAT_EAST, SUIT_HEARTS, 8));
    WorldGenerationStats partnershipStats;
    const WorldMask partnershipMask = GeneratePossibleWorlds(partnershipWorlds,
      partnershipInfo, &partnershipStats);
    Check(partnershipMask == WorldMask(3, 0x1U),
      "partnership-fit bidding constraints should keep only worlds where East/West have the required combined heart length");
    Check(partnershipStats.afterBiddingCount == 1,
      "partnership-fit bidding constraints should narrow the world pool during the bidding stage");

    const WorldGenerationExplanation partnershipExplanation =
      ExplainPossibleWorldGeneration(partnershipWorlds, partnershipInfo);
    Check(partnershipExplanation.worlds.size() == 3,
      "partnership-fit explanation should cover every candidate world");
    Check(partnershipExplanation.worlds[0].accepted,
      "the world meeting the partnership heart-fit constraint should survive the explanation pipeline");
    Check(! partnershipExplanation.worlds[1].accepted &&
          partnershipExplanation.worlds[1].rejectionStage == "bidding" &&
          partnershipExplanation.worlds[1].rejectionReason.find("East/West partnership must hold at least 8 cards in hearts") != string::npos,
      "partnership-fit explanation should reject short East/West heart fits at the bidding stage");

    vector<ParsedWorld> partnershipRangeWorlds;
    partnershipRangeWorlds.push_back(ParsePBNWorld(
      "N:T... .K.. 9... J..."));
    partnershipRangeWorlds.push_back(ParsePBNWorld(
      "N:T... .K.. 9... J.Q.."));
    partnershipRangeWorlds.push_back(ParsePBNWorld(
      "N:T... .KJ.. 9... J.Q.."));

    BridgeInformationState partnershipRangeInfo;
    partnershipRangeInfo.biddingConstraints.push_back(
      WorldConstraint::PartnershipMinLength(SEAT_EAST, SUIT_HEARTS, 2));
    partnershipRangeInfo.biddingConstraints.push_back(
      WorldConstraint::PartnershipMaxLength(SEAT_EAST, SUIT_HEARTS, 2));
    WorldGenerationStats partnershipRangeStats;
    const WorldMask partnershipRangeMask = GeneratePossibleWorlds(
      partnershipRangeWorlds, partnershipRangeInfo, &partnershipRangeStats);
    Check(partnershipRangeMask == WorldMask(3, 0x2U),
      "partnership length-range bidding constraints should keep only worlds where East/West stay inside the exact combined heart-fit target");
    Check(partnershipRangeStats.afterBiddingCount == 1,
      "partnership length-range bidding constraints should narrow the world pool during the bidding stage");

    const WorldGenerationExplanation partnershipRangeExplanation =
      ExplainPossibleWorldGeneration(partnershipRangeWorlds,
        partnershipRangeInfo);
    Check(! partnershipRangeExplanation.worlds[0].accepted &&
          partnershipRangeExplanation.worlds[0].rejectionStage == "bidding" &&
          partnershipRangeExplanation.worlds[0].rejectionReason.find("East/West partnership must hold at least 2 cards in hearts") != string::npos,
      "partnership length-range explanation should reject worlds below the combined fit floor at the bidding stage");
    Check(partnershipRangeExplanation.worlds[1].accepted,
      "the world meeting the exact partnership heart-fit target should survive the explanation pipeline");
    Check(! partnershipRangeExplanation.worlds[2].accepted &&
          partnershipRangeExplanation.worlds[2].rejectionStage == "bidding" &&
          partnershipRangeExplanation.worlds[2].rejectionReason.find("East/West partnership must hold at most 2 cards in hearts") != string::npos,
      "partnership length-range explanation should reject worlds above the combined fit ceiling at the bidding stage");

    vector<ParsedWorld> partnershipHcpWorlds;
    partnershipHcpWorlds.push_back(ParsePBNWorld(
      "N:T... K... 9... Q..."));
    partnershipHcpWorlds.push_back(ParsePBNWorld(
      "N:T... Q... 9... J..."));
    partnershipHcpWorlds.push_back(ParsePBNWorld(
      "N:T... A... 9... K..."));

    BridgeInformationState partnershipHcpInfo;
    partnershipHcpInfo.biddingConstraints.push_back(
      WorldConstraint::PartnershipMinHCP(SEAT_EAST, 5));
    partnershipHcpInfo.biddingConstraints.push_back(
      WorldConstraint::PartnershipMaxHCP(SEAT_EAST, 5));
    WorldGenerationStats partnershipHcpStats;
    const WorldMask partnershipHcpMask = GeneratePossibleWorlds(
      partnershipHcpWorlds, partnershipHcpInfo, &partnershipHcpStats);
    Check(partnershipHcpMask == WorldMask(3, 0x1U),
      "partnership HCP-range bidding constraints should keep only worlds where East/West stay inside the combined HCP range");
    Check(partnershipHcpStats.afterBiddingCount == 1,
      "partnership HCP-range bidding constraints should narrow the world pool during the bidding stage");

    const WorldGenerationExplanation partnershipHcpExplanation =
      ExplainPossibleWorldGeneration(partnershipHcpWorlds, partnershipHcpInfo);
    Check(partnershipHcpExplanation.worlds.size() == 3,
      "partnership HCP-range explanation should cover every candidate world");
    Check(partnershipHcpExplanation.worlds[0].accepted,
      "the world meeting the partnership HCP range should survive the explanation pipeline");
    Check(! partnershipHcpExplanation.worlds[1].accepted &&
          partnershipHcpExplanation.worlds[1].rejectionStage == "bidding" &&
          partnershipHcpExplanation.worlds[1].rejectionReason.find("East/West partnership must hold at least 5 HCP") != string::npos,
      "partnership HCP-range explanation should reject worlds below the combined HCP floor at the bidding stage");
    Check(! partnershipHcpExplanation.worlds[2].accepted &&
          partnershipHcpExplanation.worlds[2].rejectionStage == "bidding" &&
          partnershipHcpExplanation.worlds[2].rejectionReason.find("East/West partnership must hold at most 5 HCP") != string::npos,
      "partnership HCP-range explanation should reject worlds above the combined HCP ceiling at the bidding stage");

    BridgeInformationState playInfo;
    playInfo.knownCardConstraints.push_back(
      WorldConstraint::HasCard(SEAT_EAST, SUIT_DIAMONDS, '8'));
    playInfo.biddingConstraints.push_back(
      WorldConstraint::VoidSuit(SEAT_WEST, SUIT_SPADES));
    WorldGenerationStats playStats;
    const WorldMask playMask = GeneratePossibleWorlds(worlds, playInfo,
      &playStats);
    Check(playMask == WorldMask(4, 0x8U),
      "play-style void and card-location constraints should isolate the final world");
    Check(playStats.afterKnownCardCount == 1,
      "world-generation stats should record the known-card filter before later stages");
    Check(playStats.afterSamplingCount == 1,
      "world-generation stats should report the unchanged count when no sampling is requested");
    Check(playStats.finalWorldCount == 1,
      "world-generation stats should record the final surviving world count");

    BridgeInformationState combinedInfo;
    combinedInfo.knownCardConstraints.push_back(
      WorldConstraint::HasCard(SEAT_EAST, SUIT_DIAMONDS, '8'));
    combinedInfo.biddingConstraints.push_back(
      WorldConstraint::MinLength(SEAT_EAST, SUIT_SPADES, 5));
    combinedInfo.biddingConstraints.push_back(
      WorldConstraint::MaxLength(SEAT_EAST, SUIT_HEARTS, 0));
    const WorldMask combinedMask = GeneratePossibleWorlds(worlds, combinedInfo,
      NULL);
    Check(combinedMask == WorldMask(4, 0x8U),
      "combined bidding/play constraints should identify a single possible world");

    vector<ParsedWorld> duplicateWorlds(worlds);
    duplicateWorlds.push_back(worlds[3]);
    BridgeInformationState dedupInfo;
    dedupInfo.biddingConstraints.push_back(WorldConstraint::MinLength(
      SEAT_EAST, SUIT_SPADES, 5));
    dedupInfo.deduplicateEquivalentWorlds = true;
    WorldGenerationStats dedupStats;
    const WorldMask dedupMask = GeneratePossibleWorlds(duplicateWorlds,
      dedupInfo, &dedupStats);
    Check(dedupStats.duplicateWorldsRemoved == 1,
      "world-generation should remove one duplicate world after staged filtering");
    Check(dedupMask == WorldMask(5, 0xCU),
      "world deduplication should keep the first equivalent surviving world and drop later duplicates");

    BridgeInformationState notHasInfo;
    notHasInfo.knownCardConstraints.push_back(
      WorldConstraint::NotHasCard(SEAT_WEST, SUIT_SPADES, '3'));
    const WorldMask notHasMask = GeneratePossibleWorlds(worlds, notHasInfo, NULL);
    Check(notHasMask == WorldMask(4, 0xCU),
      "explicit cannot-hold-card constraints should keep only worlds where West does not hold the specified spade");
  }


  static void TestPlayHistoryFiltering()
  {
    vector<ParsedWorld> worlds;
    worlds.push_back(ParsePBNWorld("N:A... K.Q.. 2... 3..."));
    worlds.push_back(ParsePBNWorld("N:A... .Q.. K... 3..."));
    worlds.push_back(ParsePBNWorld("N:A... .Q.. K... 3..."));

    BridgeInformationState info;
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_HEARTS, 'Q')));
    info.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    info.deduplicateEquivalentWorlds = true;

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(worlds, info, &stats);

    Check(mask == WorldMask(3, 0x2ULL),
      "play-history filtering should keep only the world where East could legally fail to follow spades and duplicate removal should keep the first equivalent survivor");
    Check(stats.afterPlayHistoryCount == 2,
      "play-history filtering should leave exactly the two equivalent legal worlds before deduplication");
    Check(stats.afterCurrentTrickCount == 2,
      "current-trick filtering should preserve worlds that can replay the current partial trick history");
    Check(stats.duplicateWorldsRemoved == 1,
      "play-history world generation should report duplicate removal after legality filtering");
    Check(stats.finalWorldCount == 1,
      "play-history world generation should finish with one deduplicated surviving world");
  }


  static void TestFollowSuitImplicationsAndWorldExplanation()
  {
    vector<ParsedWorld> worlds;
    worlds.push_back(ParsePBNWorld("N:A... K.Q.. 2... 3..."));
    worlds.push_back(ParsePBNWorld("N:A... K9.Q.. 2... 3..."));
    worlds.push_back(ParsePBNWorld("N:A... K.J.. 2... 3..."));
    worlds.push_back(ParsePBNWorld("N:2... K.Q.. A... 3..."));

    BridgeInformationState info;
    info.knownCardConstraints.push_back(
      WorldConstraint::HasCard(SEAT_NORTH, SUIT_SPADES, 'A'));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'K')));
    info.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_HEARTS, 'Q')));

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(worlds, info, &stats);
    Check(mask == WorldMask(4, 0x1ULL),
      "explicit follow-suit implications plus detailed replay checks should leave only the one fully legal world");
    Check(stats.afterFollowSuitCount == 2,
      "derived follow-suit implications should remove the world where East still has a spare spade before replaying history");
    Check(stats.afterCurrentTrickCount == 1,
      "current-trick replay should remove the world that lacks the required discard card after passing follow-suit filtering");

    const WorldGenerationExplanation explanation =
      ExplainPossibleWorldGeneration(worlds, info);
    Check(explanation.appliedFollowSuitConstraints.size() == 1,
      "follow-suit explanation should expose the single derived spade-length implication from the discard history");
    Check(explanation.appliedFollowSuitConstraints[0].kind == CONSTRAINT_MAX_LENGTH &&
          explanation.appliedFollowSuitConstraints[0].player == SEAT_EAST &&
          explanation.appliedFollowSuitConstraints[0].suit == SUIT_SPADES &&
          explanation.appliedFollowSuitConstraints[0].count == 1,
      "follow-suit explanation should derive that East can have held at most one spade before the later discard on a spade lead");
    Check(explanation.finalWorldMask == WorldMask(4, 0x1ULL),
      "world-generation explanation should preserve the same final surviving mask as the filtering pipeline");

    Check(explanation.worlds.size() == 4,
      "world-generation explanation should include every candidate world");
    Check(explanation.worlds[0].accepted,
      "the fully legal world should be marked as accepted in the explanation trace");
    Check(explanation.worlds[0].rejectionStage.empty(),
      "the accepted world should not record a rejection stage");
    Check(! explanation.worlds[1].accepted &&
          explanation.worlds[1].rejectionStage == "follow_suit" &&
          explanation.worlds[1].rejectionReason.find("at most 1 cards in spades") != string::npos,
      "the explanation trace should reject world 1 at the explicit follow-suit stage with the derived max-length reason");
    Check(! explanation.worlds[2].accepted &&
          explanation.worlds[2].rejectionStage == "current_trick" &&
          explanation.worlds[2].rejectionReason.find("requires East to hold Q of hearts") != string::npos,
      "the explanation trace should reject world 2 at the current-trick stage because the required discard card is absent");
    Check(! explanation.worlds[3].accepted &&
          explanation.worlds[3].rejectionStage == "known_cards" &&
          explanation.worlds[3].rejectionReason.find("North must hold A of spades") != string::npos,
      "the explanation trace should reject world 3 immediately on the known-card requirement");
  }


  static void TestWorldPlausibilityReporting()
  {
    vector<ParsedWorld> worlds;
    worlds.push_back(ParsePBNWorld("N:A... K.Q.. 2... 3..."));
    worlds.push_back(ParsePBNWorld("N:A... K.J.. 2... 3..."));
    worlds.push_back(ParsePBNWorld("N:A... .Q.. K... 3..."));

    BridgeInformationState information;
    information.plausibilityHints.push_back(WorldPlausibilityHint::Prefer(
      WorldConstraint::HasCard(SEAT_EAST, SUIT_SPADES, 'K'),
      5,
      "East likely started with K of spades"));
    information.plausibilityHints.push_back(WorldPlausibilityHint::Prefer(
      WorldConstraint::HasCard(SEAT_EAST, SUIT_HEARTS, 'Q'),
      3,
      "East likely started with Q of hearts"));

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(worlds, information, &stats);
    Check(mask == WorldMask(3, 0x7ULL),
      "plausibility hints alone should not change the surviving world set");
    Check(stats.finalWorldCount == 3,
      "plausibility hints alone should leave all three candidate worlds alive");

    const WorldGenerationExplanation explanation =
      ExplainPossibleWorldGeneration(worlds, information);
    Check(explanation.finalWorldMask == WorldMask(3, 0x7ULL),
      "plausibility reporting should preserve the same surviving mask as hard filtering");
    Check(explanation.plausibilityRankedWorldIndices.size() == 3,
      "plausibility reporting should rank all surviving worlds");
    Check(explanation.plausibilityRankedWorldIndices[0] == 0 &&
          explanation.plausibilityRankedWorldIndices[1] == 1 &&
          explanation.plausibilityRankedWorldIndices[2] == 2,
      "plausibility reporting should order surviving worlds by descending score, then canonically");

    Check(explanation.worlds[0].accepted && explanation.worlds[1].accepted &&
          explanation.worlds[2].accepted,
      "plausibility reporting should not mark any world rejected when no hard constraint fails");
    Check(explanation.worlds[0].plausibilityScore == 8 &&
          explanation.worlds[1].plausibilityScore == 5 &&
          explanation.worlds[2].plausibilityScore == 3,
      "plausibility reporting should sum the matched weighted hints per world");
    Check(explanation.worlds[0].plausibilityMaxScore == 8,
      "plausibility reporting should expose the total available hint weight");
    Check(explanation.worlds[1].unsatisfiedPlausibilityHints.size() == 1 &&
          explanation.worlds[1].unsatisfiedPlausibilityHints[0] ==
            "East likely started with Q of hearts",
      "plausibility reporting should list unmatched hints for partially plausible worlds");
  }


  static void TestActiveWorldPlausibilityWeightsRespectCompactedOrdering()
  {
    WorldGenerationExplanation explanation;
    explanation.worlds.resize(3);
    explanation.worlds[0].worldIndex = 0;
    explanation.worlds[0].plausibilityScore = 1;
    explanation.worlds[1].worldIndex = 1;
    explanation.worlds[1].plausibilityScore = 0;
    explanation.worlds[2].worldIndex = 2;
    explanation.worlds[2].plausibilityScore = 4;

    vector<unsigned> activeWorldIndices;
    activeWorldIndices.push_back(2U);
    activeWorldIndices.push_back(0U);

    const vector<int> weights = BuildActiveWorldPlausibilityWeights(
      activeWorldIndices, explanation);
    Check(weights.size() == 2U && weights[0] == 4 && weights[1] == 1,
      "active-world plausibility weights should follow compacted search-world ordering rather than raw candidate ordering");
  }


  static void TestWeightedDecisionPolicySelectsRootChildExplicitly()
  {
    BridgeRootReport report(2);

    BridgeRootChildReport left(2);
    left.move = BridgeMove(SUIT_CLUBS, 'A');
    left.front = MakeFront(2, vector<string>(1, "10"));
    left.mu = left.front.Mu();
    report.children.push_back(left);

    BridgeRootChildReport right(2);
    right.move = BridgeMove(SUIT_DIAMONDS, 'A');
    right.front = MakeFront(2, vector<string>(1, "01"));
    right.mu = right.front.Mu();
    report.children.push_back(right);

    vector<int> worldWeights;
    worldWeights.push_back(5);
    worldWeights.push_back(1);

    AlphaMuDecisionPolicy appliedPolicy = ALPHA_MU_DECISION_POLICY_MU;
    double chosenWeightedScore = 0.0;
    const unsigned weightedIndex = SelectRootChildIndex(report, worldWeights,
      ALPHA_MU_DECISION_POLICY_WEIGHTED, appliedPolicy, &chosenWeightedScore);
    Check(appliedPolicy == ALPHA_MU_DECISION_POLICY_WEIGHTED,
      "weighted decision selection should keep the weighted policy when at least one surviving world has positive plausibility weight");
    Check(weightedIndex == 0U,
      "weighted decision selection should prefer the child winning in the heavier surviving world when mu ties");
    Check(fabs(chosenWeightedScore - (5.0 / 6.0)) < 1e-9,
      "weighted decision selection should expose the chosen root child's weighted score");

    appliedPolicy = ALPHA_MU_DECISION_POLICY_WEIGHTED;
    const unsigned muIndex = SelectRootChildIndex(report, worldWeights,
      ALPHA_MU_DECISION_POLICY_MU, appliedPolicy, &chosenWeightedScore);
    Check(appliedPolicy == ALPHA_MU_DECISION_POLICY_MU && muIndex == 1U,
      "plain-mu selection should remain the default even when plausibility weights are available and should keep the canonical move-order tie-break");

    vector<int> zeroWeights(2, 0);
    appliedPolicy = ALPHA_MU_DECISION_POLICY_WEIGHTED;
    const unsigned fallbackIndex = SelectRootChildIndex(report, zeroWeights,
      ALPHA_MU_DECISION_POLICY_WEIGHTED, appliedPolicy, &chosenWeightedScore);
    Check(appliedPolicy == ALPHA_MU_DECISION_POLICY_MU,
      "weighted decision selection should fall back to plain mu when no surviving world carries positive plausibility weight");
    Check(fallbackIndex == 1U,
      "weighted decision fallback should use the same deterministic child choice as plain mu");
  }


  static void TestHistoryDerivedCandidateWorldConstruction()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld("N:A.9.. K.Q.. 2.8.. J.T..");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);

    BridgeInformationState info;
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'K')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));

    const HistoryDerivedConstructionResult constructed =
      ConstructCandidateWorldsFromHistory(spec, info);
    Check(constructed.hiddenCards.size() == 4,
      "history-derived construction should collect all hidden East/West cards into a candidate pool");
    Check(constructed.worlds.size() == 2,
      "history-derived construction should use played-card ownership to reduce the hidden-seat candidate pool to the two heart-swap worlds");
    Check(constructed.visibleSeedWorld.suits[SEAT_EAST][SUIT_SPADES].empty() &&
          constructed.visibleSeedWorld.suits[SEAT_WEST][SUIT_HEARTS].empty(),
      "history-derived construction should clear hidden-seat cards from the visible partial-information seed");

    bool sawEastHeartQ = false;
    bool sawEastHeartT = false;
    for (unsigned i = 0; i < constructed.worlds.size(); i++)
    {
      Check(WorldHasCard(constructed.worlds[i], SEAT_EAST, SUIT_SPADES, 'K'),
        "history-derived construction should pin East's played spade to East in every constructed world");
      Check(WorldHasCard(constructed.worlds[i], SEAT_WEST, SUIT_SPADES, 'J'),
        "history-derived construction should pin West's played spade to West in every constructed world");
      if (WorldHasCard(constructed.worlds[i], SEAT_EAST, SUIT_HEARTS, 'Q'))
        sawEastHeartQ = true;
      if (WorldHasCard(constructed.worlds[i], SEAT_EAST, SUIT_HEARTS, 'T'))
        sawEastHeartT = true;
    }
    Check(sawEastHeartQ && sawEastHeartT,
      "history-derived construction should leave the unplayed heart ownership unresolved across the two constructed worlds");

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(constructed.worlds, info, &stats);
    Check(mask == WorldMask(2, 0x3ULL),
      "without extra hidden-card location evidence the staged filtering pipeline should keep both history-derived heart-swap worlds");
    Check(stats.candidateWorldCount == 2,
      "world-generation stats should report the history-derived constructed candidate count");
    Check(stats.afterKnownCardCount == 2,
      "without explicit hidden-seat known-card constraints the known-card stage should leave both constructed heart-swap worlds intact");
    Check(stats.afterPlayHistoryCount == 2,
      "both constructed heart-swap worlds should replay the recorded trick history legally");

    const WorldGenerationExplanation explanation =
      ExplainPossibleWorldGeneration(constructed.worlds, info);
    Check(explanation.worlds.size() == 2,
      "world-generation explanation should cover each history-derived candidate world");
    Check(explanation.worlds[0].accepted,
      "the first history-derived candidate world should survive when no extra hidden-card evidence distinguishes the heart swap");
    Check(explanation.worlds[1].accepted,
      "the second history-derived candidate world should also survive when no extra hidden-card evidence distinguishes the heart swap");
  }


  static void TestHistoryDerivedConstructionUsesKnownCardLocation()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld("N:A.9.. K.Q.. 2.8.. J.T..");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);

    BridgeInformationState unconstrainedInfo;
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'K')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));

    const HistoryDerivedConstructionResult unconstrained =
      ConstructCandidateWorldsFromHistory(spec, unconstrainedInfo);
    Check(unconstrained.worlds.size() == 2,
      "without constructor-local known-card pruning the hidden-seat pool should keep both legal heart-swap worlds");

    BridgeInformationState constrainedInfo(unconstrainedInfo);
    constrainedInfo.knownCardConstraints.push_back(
      WorldConstraint::HasCard(SEAT_EAST, SUIT_HEARTS, 'Q'));

    const HistoryDerivedConstructionResult constrained =
      ConstructCandidateWorldsFromHistory(spec, constrainedInfo);
    Check(constrained.worlds.size() == 1,
      "constructor-local known-card pruning should collapse the hidden-seat pool to the one world where East holds the known heart queen");
    Check(WorldHasCard(constrained.worlds[0], SEAT_EAST, SUIT_HEARTS, 'Q') &&
          WorldHasCard(constrained.worlds[0], SEAT_WEST, SUIT_HEARTS, 'T'),
      "constructor-local known-card pruning should assign the required hidden heart queen to East before later filtering");

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(constrained.worlds,
      constrainedInfo, &stats);
    Check(mask == WorldMask(1, 0x1ULL),
      "the world surviving constructor-local known-card pruning should also survive the later staged filters");
    Check(stats.afterKnownCardCount == 1,
      "later known-card filtering should see only the constructor-pruned known-card survivor");
    Check(stats.afterPlayHistoryCount == 1,
      "the constructor-pruned known-card survivor should still replay the recorded trick history legally");
  }


  static void TestHistoryDerivedConstructionUsesKnownCardExclusion()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld("N:A.9.. K.Q.. 2.8.. J.T..");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);

    BridgeInformationState unconstrainedInfo;
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'K')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));

    const HistoryDerivedConstructionResult unconstrained =
      ConstructCandidateWorldsFromHistory(spec, unconstrainedInfo);
    Check(unconstrained.worlds.size() == 2,
      "without constructor-local known-card exclusion pruning the hidden-seat pool should keep both legal heart-swap worlds");

    BridgeInformationState constrainedInfo(unconstrainedInfo);
    constrainedInfo.knownCardConstraints.push_back(
      WorldConstraint::NotHasCard(SEAT_EAST, SUIT_HEARTS, 'Q'));

    const HistoryDerivedConstructionResult constrained =
      ConstructCandidateWorldsFromHistory(spec, constrainedInfo);
    Check(constrained.worlds.size() == 1,
      "constructor-local known-card exclusion pruning should collapse the hidden-seat pool to the one world where East cannot still hold the heart queen");
    Check(WorldHasCard(constrained.worlds[0], SEAT_EAST, SUIT_HEARTS, 'T') &&
          WorldHasCard(constrained.worlds[0], SEAT_WEST, SUIT_HEARTS, 'Q'),
      "constructor-local known-card exclusion pruning should remove the forbidden hidden heart-queen assignment before later filtering");

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(constrained.worlds,
      constrainedInfo, &stats);
    Check(mask == WorldMask(1, 0x1ULL),
      "the world surviving constructor-local known-card exclusion pruning should also survive the later staged filters");
    Check(stats.afterKnownCardCount == 1,
      "later known-card filtering should see only the constructor-pruned exclusion survivor");
    Check(stats.afterPlayHistoryCount == 1,
      "the constructor-pruned exclusion survivor should still replay the recorded trick history legally");
  }


  static void TestHistoryDerivedConstructionExplanationTracksCardLocationNarrowing()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld("N:A.9.. K.Q.. 2.8.. J.T..");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);

    BridgeInformationState info;
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'K')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));
    info.knownCardConstraints.push_back(
      WorldConstraint::HasCard(SEAT_EAST, SUIT_HEARTS, 'Q'));

    const HistoryDerivedConstructionExplanation explanation =
      ExplainHistoryDerivedConstruction(spec, info);
    Check(explanation.stats.rawAssignmentCount == 6,
      "constructor-local explanation should report the full six-world hidden-card assignment pool before ownership evidence is applied");
    Check(explanation.stats.afterOwnershipCount == 2,
      "constructor-local explanation should report ownership pinning reducing the heart-swap fixture from six worlds to two");
    Check(explanation.stats.afterCardLocationCount == 1,
      "constructor-local explanation should report known-card location pruning collapsing the heart-swap fixture to one world before later constructor checks");
    Check(explanation.stats.afterConstructorLengthCount == 1 &&
          explanation.stats.afterConstructorHCPCount == 1 &&
          explanation.stats.afterConstructorBalancedCount == 1,
      "constructor-local explanation should preserve the lone known-card survivor through length, HCP, and balanced constructor stages when no later constructor pruning applies");
    Check(explanation.stats.afterConstructorConstraintCount == 1 &&
          explanation.stats.finalWorldCount == 1,
      "constructor-local explanation should report one final surviving world after card-location narrowing leaves nothing further to prune");
    Check(explanation.worlds.size() == 1 && explanation.worlds[0].accepted,
      "constructor-local explanation should include the lone surviving known-card world as accepted");
  }


  static void TestHistoryDerivedConstructionUsesPartnershipHCPRange()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld("N:A.9.. K.Q.. 2.8.. J.T..");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_SOUTH);

    BridgeInformationState unconstrainedInfo;
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'K')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));

    const HistoryDerivedConstructionResult unconstrained =
      ConstructCandidateWorldsFromHistory(spec, unconstrainedInfo);
    Check(unconstrained.worlds.size() == 2,
      "without constructor-local partnership HCP pruning the East-versus-South heart-honor swap should keep both legal worlds");

    BridgeInformationState constrainedInfo(unconstrainedInfo);
    constrainedInfo.biddingConstraints.push_back(
      WorldConstraint::PartnershipMinHCP(SEAT_EAST, 6));
    constrainedInfo.biddingConstraints.push_back(
      WorldConstraint::PartnershipMaxHCP(SEAT_EAST, 6));

    const HistoryDerivedConstructionResult constrained =
      ConstructCandidateWorldsFromHistory(spec, constrainedInfo);
    Check(constrained.worlds.size() == 1,
      "constructor-local partnership HCP-range pruning should keep only the world where East/West reach the exact combined HCP target");
    Check(WorldHasCard(constrained.worlds[0], SEAT_EAST, SUIT_HEARTS, 'Q') &&
          ! WorldHasCard(constrained.worlds[0], SEAT_SOUTH, SUIT_HEARTS, 'Q'),
      "constructor-local partnership HCP-range pruning should keep the world where East receives the hidden heart queen and East/West reach six combined HCP");

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(constrained.worlds,
      constrainedInfo, &stats);
    Check(mask == WorldMask(1, 0x1ULL),
      "the world surviving constructor-local partnership HCP-range pruning should also survive the later staged filters");
    Check(stats.afterBiddingCount == 1,
      "later bidding-stage filtering should see only the constructor-pruned partnership-HCP survivor");

    const HistoryDerivedConstructionExplanation explanation =
      ExplainHistoryDerivedConstruction(spec, constrainedInfo);
    unsigned acceptedWorlds = 0;
    unsigned rejectedAtConstructorHcp = 0;
    for (unsigned i = 0; i < explanation.worlds.size(); i++)
    {
      if (explanation.worlds[i].accepted)
        acceptedWorlds++;
      else if (explanation.worlds[i].rejectionStage == "constructor_hcp" &&
               explanation.worlds[i].rejectionReason.find("East/West partnership must hold at least 6 HCP") != string::npos)
      {
        rejectedAtConstructorHcp++;
      }
    }
    Check(explanation.stats.rawAssignmentCount == 6,
      "constructor-local partnership HCP explanation should report the six raw East/South hidden-card assignments before ownership evidence is applied");
    Check(explanation.stats.afterOwnershipCount == 2 &&
          explanation.stats.afterCardLocationCount == 2,
      "constructor-local partnership HCP explanation should report ownership pinning reducing the pool to the two heart-honor swap worlds before HCP pruning");
    Check(explanation.stats.afterConstructorLengthCount == 2 &&
          explanation.stats.afterConstructorHCPCount == 1 &&
          explanation.stats.afterConstructorBalancedCount == 1,
      "constructor-local partnership HCP explanation should keep both ownership-consistent worlds through length pruning, then narrow to one at the HCP stage");
    Check(explanation.stats.afterConstructorConstraintCount == 1 &&
          explanation.stats.finalWorldCount == 1,
      "constructor-local partnership HCP explanation should report the two ownership-consistent worlds collapsing to one after constructor HCP pruning");
    Check(acceptedWorlds == 1 && rejectedAtConstructorHcp == 1,
      "constructor-local partnership HCP explanation should show one surviving world and one constructor_hcp rejection at the partnership HCP floor");
  }


  static void TestHistoryDerivedConstructionUsesPartnershipLengthRange()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld("N:A... K.Q.. 2..8. J.T9..");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_SOUTH);

    BridgeInformationState unconstrainedInfo;
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'K')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));

    const HistoryDerivedConstructionResult unconstrained =
      ConstructCandidateWorldsFromHistory(spec, unconstrainedInfo);
    Check(unconstrained.worlds.size() == 2,
      "without constructor-local partnership length pruning the East-versus-South heart swap should keep both legal worlds");

    BridgeInformationState constrainedInfo(unconstrainedInfo);
    constrainedInfo.biddingConstraints.push_back(
      WorldConstraint::PartnershipMinLength(SEAT_EAST, SUIT_HEARTS, 2));
    constrainedInfo.biddingConstraints.push_back(
      WorldConstraint::PartnershipMaxLength(SEAT_EAST, SUIT_HEARTS, 2));

    const HistoryDerivedConstructionResult constrained =
      ConstructCandidateWorldsFromHistory(spec, constrainedInfo);
    Check(constrained.worlds.size() == 1,
      "constructor-local partnership length-range pruning should keep only the world where East/West reach the exact combined heart-fit target");
    Check(! WorldHasCard(constrained.worlds[0], SEAT_EAST, SUIT_HEARTS, 'Q') &&
          WorldHasCard(constrained.worlds[0], SEAT_SOUTH, SUIT_HEARTS, 'Q'),
      "constructor-local partnership length-range pruning should keep the world where East does not receive the extra hidden heart and East/West stay at exactly two hearts");

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(constrained.worlds,
      constrainedInfo, &stats);
    Check(mask == WorldMask(1, 0x1ULL),
      "the world surviving constructor-local partnership length-range pruning should also survive the later staged filters");
    Check(stats.afterBiddingCount == 1,
      "later bidding-stage filtering should see only the constructor-pruned partnership-length survivor");

    const HistoryDerivedConstructionExplanation explanation =
      ExplainHistoryDerivedConstruction(spec, constrainedInfo);
    unsigned acceptedWorlds = 0;
    unsigned rejectedAtConstructorLength = 0;
    for (unsigned i = 0; i < explanation.worlds.size(); i++)
    {
      if (explanation.worlds[i].accepted)
        acceptedWorlds++;
      else if (explanation.worlds[i].rejectionStage == "constructor_length" &&
               explanation.worlds[i].rejectionReason.find("East/West partnership must hold at most 2 cards in hearts") != string::npos)
      {
        rejectedAtConstructorLength++;
      }
    }
    Check(explanation.stats.rawAssignmentCount == 6,
      "constructor-local partnership length explanation should report the six raw East/South hidden-card assignments before ownership evidence is applied");
    Check(explanation.stats.afterOwnershipCount == 2 &&
          explanation.stats.afterCardLocationCount == 2,
      "constructor-local partnership length explanation should report ownership pinning reducing the pool to the two heart-versus-diamond swap worlds before length pruning");
    Check(explanation.stats.afterConstructorLengthCount == 1 &&
          explanation.stats.afterConstructorHCPCount == 1 &&
          explanation.stats.afterConstructorBalancedCount == 1,
      "constructor-local partnership length explanation should narrow to one world at the length stage and preserve that survivor through later constructor stages");
    Check(explanation.stats.afterConstructorConstraintCount == 1 &&
          explanation.stats.finalWorldCount == 1,
      "constructor-local partnership length explanation should report the two ownership-consistent worlds collapsing to one after constructor length pruning");
    Check(acceptedWorlds == 1 && rejectedAtConstructorLength == 1,
      "constructor-local partnership length explanation should show one surviving world and one constructor_length rejection at the partnership fit ceiling");
  }


  static void TestHistoryDerivedConstructionExplanationTracksPartnershipRangeStageCounts()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld("N:A... K.Q.Q. 2..8. J...");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_SOUTH);

    BridgeInformationState info;
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'K')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));
    info.biddingConstraints.push_back(
      WorldConstraint::PartnershipMinLength(SEAT_EAST, SUIT_HEARTS, 1));
    info.biddingConstraints.push_back(
      WorldConstraint::PartnershipMaxLength(SEAT_EAST, SUIT_HEARTS, 1));
    info.biddingConstraints.push_back(
      WorldConstraint::PartnershipMinHCP(SEAT_EAST, 6));
    info.biddingConstraints.push_back(
      WorldConstraint::PartnershipMaxHCP(SEAT_EAST, 6));

    const HistoryDerivedConstructionExplanation explanation =
      ExplainHistoryDerivedConstruction(spec, info);
    Check(explanation.stats.rawAssignmentCount == 10,
      "constructor-stage accounting should report the full ten raw East/South hidden-card assignments before ownership evidence is applied in the mixed partnership-range fixture");
    Check(explanation.stats.afterOwnershipCount == 3,
      "constructor-stage accounting should report ownership pinning reducing the mixed partnership-range fixture to three candidate worlds");
    Check(explanation.stats.afterCardLocationCount == 3,
      "constructor-stage accounting should preserve all three ownership-consistent worlds before partnership-range pruning when no extra card-location evidence applies");
    Check(explanation.stats.afterConstructorLengthCount == 2,
      "constructor-stage accounting should show partnership length-range pruning reducing the mixed fixture from three worlds to two");
    Check(explanation.stats.afterConstructorHCPCount == 1,
      "constructor-stage accounting should show partnership HCP-range pruning reducing the mixed fixture from two worlds to one");
    Check(explanation.stats.afterConstructorBalancedCount == 1 &&
          explanation.stats.afterConstructorConstraintCount == 1 &&
          explanation.stats.finalWorldCount == 1,
      "constructor-stage accounting should preserve the lone mixed partnership-range survivor through the balanced and final constructor checkpoints");

    unsigned rejectedAtLength = 0;
    unsigned rejectedAtHcp = 0;
    unsigned acceptedWorlds = 0;
    for (unsigned i = 0; i < explanation.worlds.size(); i++)
    {
      if (explanation.worlds[i].accepted)
        acceptedWorlds++;
      else if (explanation.worlds[i].rejectionStage == "constructor_length")
        rejectedAtLength++;
      else if (explanation.worlds[i].rejectionStage == "constructor_hcp")
        rejectedAtHcp++;
    }
    Check(explanation.worlds.size() == 3,
      "constructor-stage accounting should enumerate the three ownership-consistent worlds in the mixed partnership-range fixture");
    Check(acceptedWorlds == 1 && rejectedAtLength == 1 && rejectedAtHcp == 1,
      "constructor-stage accounting should show one world rejected at partnership length, one at partnership HCP, and one final survivor in the mixed fixture");
  }


  static void TestHistoryDerivedConstructionExplanationTracksFollowSuitRejections()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld(
      "N:AK93.A93.A83.A83 J.876.JT.J96 842.KQ2.KQ2.KQ72 T765.54.654.T");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);
    spec.inferHiddenCardsFromVisibleHands = true;

    BridgeInformationState info;
    info.deriveFollowSuitConstraints = true;
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '5')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, '9')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_HEARTS, 'J')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '8')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'T')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_CLUBS, 'A')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_CLUBS,
      BridgeMove(SUIT_CLUBS, 'J')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_CLUBS,
      BridgeMove(SUIT_CLUBS, '2')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_CLUBS,
      BridgeMove(SUIT_CLUBS, 'T')));
    info.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_CLUBS, '8')));
    info.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_CLUBS,
      BridgeMove(SUIT_CLUBS, '6')));
    info.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_CLUBS,
      BridgeMove(SUIT_CLUBS, '7')));
    info.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_CLUBS,
      BridgeMove(SUIT_HEARTS, '5')));

    const HistoryDerivedConstructionExplanation explanation =
      ExplainHistoryDerivedConstruction(spec, info);
    Check(explanation.stats.rawAssignmentCount == 35,
      "constructor-local explanation should report the full thirty-five-world visible-seed assignment pool before ownership evidence is applied");
    Check(explanation.stats.afterOwnershipCount == 20,
      "constructor-local explanation should report ownership pinning reducing the visible-seed pool from thirty-five worlds to twenty");
    Check(explanation.stats.afterCardLocationCount == 20,
      "constructor-local explanation should preserve the twenty ownership-consistent visible-seed worlds when no extra card-location constraints are present");
    Check(explanation.stats.afterConstructorConstraintCount == 3 &&
          explanation.stats.finalWorldCount == 3,
      "constructor-local explanation should report the longer visible-seed history narrowing from twenty worlds to three before later staged filtering");

    unsigned acceptedWorlds = 0;
    unsigned rejectedAtConstructorLength = 0;
    for (unsigned i = 0; i < explanation.worlds.size(); i++)
    {
      if (explanation.worlds[i].accepted)
        acceptedWorlds++;
      else if (explanation.worlds[i].rejectionStage == "constructor_length")
        rejectedAtConstructorLength++;
    }
    Check(explanation.worlds.size() == 20,
      "constructor-local explanation should enumerate every visible-seed world that survives ownership and card-location narrowing");
    Check(acceptedWorlds == 3 && rejectedAtConstructorLength == 17,
      "constructor-local explanation should show the longer visible-seed history splitting twenty ownership-consistent worlds into three survivors and seventeen constructor-length rejections");
    Check(explanation.worlds[0].steps[0].stage == "constructor_length",
      "constructor-local explanation should begin with the constructor-length stage for visible-seed follow-suit pruning");
  }


  static void TestHistoryDerivedConstructionRespectsCurrentTrick()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld("N:A.9.. K.Q.. 2.8.. J.T..");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);

    BridgeInformationState info;
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'K')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));
    info.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_HEARTS, '9')));
    info.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_HEARTS,
      BridgeMove(SUIT_HEARTS, 'Q')));

    const HistoryDerivedConstructionResult constructed =
      ConstructCandidateWorldsFromHistory(spec, info);
    Check(constructed.worlds.size() == 1,
      "history-derived construction should use current-trick ownership as well as prior play to resolve the remaining hidden heart location");
    Check(WorldHasCard(constructed.worlds[0], SEAT_EAST, SUIT_HEARTS, 'Q') &&
          WorldHasCard(constructed.worlds[0], SEAT_WEST, SUIT_HEARTS, 'T'),
      "history-derived construction should pin the current-trick heart to East and leave West with the other hidden heart");

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(constructed.worlds, info, &stats);
    Check(mask == WorldMask(1, 0x1ULL),
      "the uniquely constructed world should survive both play-history and current-trick replay filtering");
    Check(stats.afterCurrentTrickCount == 1,
      "current-trick replay should confirm the uniquely constructed world after seed-based hidden-seat construction");
  }


  static void TestHistoryDerivedConstructionUsesBiddingCardLocation()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld("N:A.9.. K.Q.. 2.8.. J.T..");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);

    BridgeInformationState info;
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'K')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    info.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));
    info.biddingConstraints.push_back(
      WorldConstraint::HasCard(SEAT_EAST, SUIT_HEARTS, 'Q'));

    const HistoryDerivedConstructionResult constructed =
      ConstructCandidateWorldsFromHistory(spec, info);
    Check(constructed.worlds.size() == 1,
      "constructor-local bidding card-location pruning should collapse the hidden-seat pool to the one world where East holds the bid-implied heart queen");
    Check(WorldHasCard(constructed.worlds[0], SEAT_EAST, SUIT_HEARTS, 'Q') &&
          WorldHasCard(constructed.worlds[0], SEAT_WEST, SUIT_HEARTS, 'T'),
      "constructor-local bidding card-location pruning should assign the constrained heart queen to East before later filtering");

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(constructed.worlds, info, &stats);
    Check(mask == WorldMask(1, 0x1ULL),
      "the world surviving constructor-local bidding card-location pruning should also survive the later staged filters");
  }


  static void TestHistoryDerivedConstructionUsesBiddingLength()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld("N:A... K.QT.. 2... J..98.");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);

    BridgeInformationState unconstrainedInfo;
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'K')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));

    const HistoryDerivedConstructionResult unconstrained =
      ConstructCandidateWorldsFromHistory(spec, unconstrainedInfo);
    Check(unconstrained.worlds.size() == 6,
      "without constructor-local bidding length pruning the hidden-seat pool should keep all six legal heart-versus-diamond assignment worlds");

    BridgeInformationState constrainedInfo(unconstrainedInfo);
    constrainedInfo.biddingConstraints.push_back(
      WorldConstraint::MinLength(SEAT_EAST, SUIT_HEARTS, 2));

    const HistoryDerivedConstructionResult constrained =
      ConstructCandidateWorldsFromHistory(spec, constrainedInfo);
    Check(constrained.worlds.size() == 1,
      "constructor-local bidding length pruning should collapse the hidden-seat pool to the one world where East keeps both bid-implied hearts");
    for (unsigned i = 0; i < constrained.worlds.size(); i++)
    {
      Check(WorldSuitLength(constrained.worlds[i], SEAT_EAST, SUIT_HEARTS) >= 2,
        "every world surviving constructor-local bidding length pruning should satisfy the East heart-length lower bound immediately");
    }

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(constrained.worlds,
      constrainedInfo, &stats);
    Check(mask == WorldMask(1, 0x1ULL),
      "worlds surviving constructor-local bidding length pruning should also survive the later staged filters unchanged");
    Check(stats.afterBiddingCount == 1,
      "later bidding-stage filtering should see only the constructor-pruned worlds for the hidden-seat length-bounded case");
  }


  static void TestHistoryDerivedConstructionUsesBiddingMinHCP()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld("N:A.9.. K.Q.. 2.8.. J.T..");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);

    BridgeInformationState unconstrainedInfo;
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'K')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));

    const HistoryDerivedConstructionResult unconstrained =
      ConstructCandidateWorldsFromHistory(spec, unconstrainedInfo);
    Check(unconstrained.worlds.size() == 2,
      "without constructor-local HCP pruning the hidden-seat pool should keep both legal heart-swap worlds");

    BridgeInformationState constrainedInfo(unconstrainedInfo);
    constrainedInfo.biddingConstraints.push_back(
      WorldConstraint::MinHCP(SEAT_EAST, 5));

    const HistoryDerivedConstructionResult constrained =
      ConstructCandidateWorldsFromHistory(spec, constrainedInfo);
    Check(constrained.worlds.size() == 1,
      "constructor-local MinHCP pruning should keep only the world where East receives enough honor strength");
    Check(WorldHighCardPoints(constrained.worlds[0], SEAT_EAST) >= 5,
      "the world surviving constructor-local MinHCP pruning should satisfy East's minimum honor-point bound immediately");
    Check(WorldHasCard(constrained.worlds[0], SEAT_EAST, SUIT_HEARTS, 'Q'),
      "constructor-local MinHCP pruning should keep the world where East receives the hidden heart queen");

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(constrained.worlds,
      constrainedInfo, &stats);
    Check(mask == WorldMask(1, 0x1ULL),
      "the world surviving constructor-local MinHCP pruning should also survive the later staged filters");
    Check(stats.afterBiddingCount == 1,
      "later bidding-stage filtering should see only the constructor-pruned MinHCP survivor");
  }


  static void TestHistoryDerivedConstructionUsesBiddingMaxHCP()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld("N:A.9.. K.Q.. 2.8.. J.T..");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);

    BridgeInformationState unconstrainedInfo;
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'K')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));

    const HistoryDerivedConstructionResult unconstrained =
      ConstructCandidateWorldsFromHistory(spec, unconstrainedInfo);
    Check(unconstrained.worlds.size() == 2,
      "without constructor-local HCP pruning the hidden-seat pool should keep both legal heart-swap worlds for the MaxHCP case as well");

    BridgeInformationState constrainedInfo(unconstrainedInfo);
    constrainedInfo.biddingConstraints.push_back(
      WorldConstraint::MaxHCP(SEAT_EAST, 3));

    const HistoryDerivedConstructionResult constrained =
      ConstructCandidateWorldsFromHistory(spec, constrainedInfo);
    Check(constrained.worlds.size() == 1,
      "constructor-local MaxHCP pruning should keep only the world where East stays under the honor-point cap");
    Check(WorldHighCardPoints(constrained.worlds[0], SEAT_EAST) <= 3,
      "the world surviving constructor-local MaxHCP pruning should satisfy East's maximum honor-point bound immediately");
    Check(WorldHasCard(constrained.worlds[0], SEAT_EAST, SUIT_HEARTS, 'T'),
      "constructor-local MaxHCP pruning should keep the world where East does not receive the hidden heart queen");

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(constrained.worlds,
      constrainedInfo, &stats);
    Check(mask == WorldMask(1, 0x1ULL),
      "the world surviving constructor-local MaxHCP pruning should also survive the later staged filters");
    Check(stats.afterBiddingCount == 1,
      "later bidding-stage filtering should see only the constructor-pruned MaxHCP survivor");
  }


  static void TestHistoryDerivedConstructionUsesBiddingBalancedShape()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld(
      "N:A3.AKT7.AKT7.A32 QJ98.QJ98.QJ9.QJ K2.65432.65432.K T7654..8.T987654");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);

    BridgeInformationState unconstrainedInfo;
    const auto addHasCards =
      [&](const int seat, const int suit, const string& ranks)
      {
        for (unsigned i = 0; i < ranks.size(); i++)
        {
          unconstrainedInfo.biddingConstraints.push_back(
            WorldConstraint::HasCard(seat, suit, ranks[i]));
        }
      };

    addHasCards(SEAT_EAST, SUIT_SPADES, "QJ98");
    addHasCards(SEAT_EAST, SUIT_HEARTS, "QJ98");
    addHasCards(SEAT_EAST, SUIT_DIAMONDS, "QJ9");
    addHasCards(SEAT_EAST, SUIT_CLUBS, "Q");
    addHasCards(SEAT_WEST, SUIT_SPADES, "7654");
    addHasCards(SEAT_WEST, SUIT_DIAMONDS, "8");
    addHasCards(SEAT_WEST, SUIT_CLUBS, "T987654");

    const HistoryDerivedConstructionResult unconstrained =
      ConstructCandidateWorldsFromHistory(spec, unconstrainedInfo);
    Check(unconstrained.worlds.size() == 2,
      "without constructor-local balanced pruning the full hidden-hand fixture should keep the two legal ambiguous spade-versus-club worlds");

    BridgeInformationState constrainedInfo(unconstrainedInfo);
    constrainedInfo.biddingConstraints.push_back(
      WorldConstraint::Balanced(SEAT_EAST));

    const HistoryDerivedConstructionResult constrained =
      ConstructCandidateWorldsFromHistory(spec, constrainedInfo);
    Check(constrained.worlds.size() == 1,
      "constructor-local balanced pruning should keep only the world where East can still reach a balanced 13-card shape");
    Check(WorldHasBalancedShape(constrained.worlds[0], SEAT_EAST),
      "the world surviving constructor-local balanced pruning should give East a balanced full-hand shape");
    Check(WorldHasCard(constrained.worlds[0], SEAT_EAST, SUIT_CLUBS, 'J'),
      "constructor-local balanced pruning should keep the world where East receives the ambiguous club instead of the ambiguous spade");

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(constrained.worlds,
      constrainedInfo, &stats);
    Check(mask == WorldMask(1, 0x1ULL),
      "the world surviving constructor-local balanced pruning should also survive the later staged filters");
    Check(stats.afterBiddingCount == 1,
      "later bidding-stage filtering should see only the constructor-pruned balanced-shape survivor");
  }


  static void TestHistoryDerivedConstructionUsesCombinedBiddingProfile()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld(
      "N:A3.AKT7.AKT7.A32 QJ98.QJ98.QJ9.QJ K2.65432.65432.K T7654..8.T987654");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);

    BridgeInformationState unconstrainedInfo;
    const auto addHasCards =
      [&](const int seat, const int suit, const string& ranks)
      {
        for (unsigned i = 0; i < ranks.size(); i++)
        {
          unconstrainedInfo.biddingConstraints.push_back(
            WorldConstraint::HasCard(seat, suit, ranks[i]));
        }
      };

    addHasCards(SEAT_EAST, SUIT_SPADES, "QJ98");
    addHasCards(SEAT_EAST, SUIT_HEARTS, "QJ98");
    addHasCards(SEAT_EAST, SUIT_DIAMONDS, "QJ9");
    addHasCards(SEAT_EAST, SUIT_CLUBS, "Q");
    addHasCards(SEAT_WEST, SUIT_SPADES, "7654");
    addHasCards(SEAT_WEST, SUIT_DIAMONDS, "8");
    addHasCards(SEAT_WEST, SUIT_CLUBS, "T987654");

    const HistoryDerivedConstructionResult unconstrained =
      ConstructCandidateWorldsFromHistory(spec, unconstrainedInfo);
    Check(unconstrained.worlds.size() == 2,
      "without the combined bidding profile the full hidden-hand fixture should keep the two legal ambiguous spade-versus-club worlds");

    BridgeInformationState constrainedInfo(unconstrainedInfo);
    constrainedInfo.biddingConstraints.push_back(
      WorldConstraint::MinLength(SEAT_EAST, SUIT_CLUBS, 2));
    constrainedInfo.biddingConstraints.push_back(
      WorldConstraint::MaxLength(SEAT_EAST, SUIT_CLUBS, 2));
    constrainedInfo.biddingConstraints.push_back(
      WorldConstraint::MinHCP(SEAT_EAST, 12));
    constrainedInfo.biddingConstraints.push_back(
      WorldConstraint::MaxHCP(SEAT_EAST, 12));
    constrainedInfo.biddingConstraints.push_back(
      WorldConstraint::Balanced(SEAT_EAST));

    const HistoryDerivedConstructionResult constrained =
      ConstructCandidateWorldsFromHistory(spec, constrainedInfo);
    Check(constrained.worlds.size() == 1,
      "constructor-local combined bidding-profile pruning should keep only the full-hand world matching East's club length, exact HCP, and balanced-shape profile");
    Check(WorldSuitLength(constrained.worlds[0], SEAT_EAST, SUIT_CLUBS) == 2,
      "the world surviving constructor-local combined bidding-profile pruning should give East exactly two clubs");
    Check(WorldHighCardPoints(constrained.worlds[0], SEAT_EAST) == 12,
      "the world surviving constructor-local combined bidding-profile pruning should give East exactly twelve HCP");
    Check(WorldHasBalancedShape(constrained.worlds[0], SEAT_EAST),
      "the world surviving constructor-local combined bidding-profile pruning should keep East balanced");
    Check(WorldHasCard(constrained.worlds[0], SEAT_EAST, SUIT_CLUBS, 'J'),
      "constructor-local combined bidding-profile pruning should keep the world where East receives the ambiguous club rather than the ambiguous spade");

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(constrained.worlds,
      constrainedInfo, &stats);
    Check(mask == WorldMask(1, 0x1ULL),
      "the world surviving constructor-local combined bidding-profile pruning should also survive the later staged filters");
    Check(stats.afterBiddingCount == 1,
      "later bidding-stage filtering should see only the constructor-pruned combined bidding-profile survivor");

    const HistoryDerivedConstructionExplanation explanation =
      ExplainHistoryDerivedConstruction(spec, constrainedInfo);
    Check(explanation.stats.afterConstructorLengthCount == 1 &&
          explanation.stats.afterConstructorHCPCount == 1 &&
          explanation.stats.afterConstructorBalancedCount == 1,
      "constructor-local combined bidding-profile explanation should narrow at the length stage and preserve the lone survivor through the later HCP and balanced stages");
    Check(explanation.stats.afterConstructorConstraintCount == 1 &&
          explanation.stats.finalWorldCount == 1,
      "constructor-local combined bidding-profile explanation should report a single final survivor");
    Check(explanation.worlds.size() >= 2,
      "constructor-local combined bidding-profile explanation should still describe both the accepted full-hand world and at least one rejected alternative");

    unsigned acceptedWorlds = 0;
    unsigned rejectedAtConstructorLength = 0;
    for (unsigned i = 0; i < explanation.worlds.size(); i++)
    {
      if (explanation.worlds[i].accepted)
        acceptedWorlds++;
      else if (explanation.worlds[i].rejectionStage == "constructor_length" &&
               explanation.worlds[i].rejectionReason.find("at least 2 cards in clubs") != string::npos)
      {
        rejectedAtConstructorLength++;
      }
    }
    Check(acceptedWorlds == 1 && rejectedAtConstructorLength >= 1,
      "constructor-local combined bidding-profile explanation should show one accepted full-hand world and at least one constructor_length rejection at East's club-length requirement");
  }


  static void TestHistoryDerivedConstructionUsesBiddingHandType()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld(
      "N:... AKQJ9T.987.65.43 ... 8765.654.432.AK2");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);

    BridgeInformationState unconstrainedInfo;
    const auto addHasCards =
      [&](const int seat, const int suit, const string& ranks)
      {
        for (unsigned i = 0; i < ranks.size(); i++)
        {
          unconstrainedInfo.biddingConstraints.push_back(
            WorldConstraint::HasCard(seat, suit, ranks[i]));
        }
      };

    addHasCards(SEAT_EAST, SUIT_SPADES, "AKQJ9");
    addHasCards(SEAT_EAST, SUIT_HEARTS, "987");
    addHasCards(SEAT_EAST, SUIT_DIAMONDS, "65");
    addHasCards(SEAT_EAST, SUIT_CLUBS, "43");
    addHasCards(SEAT_WEST, SUIT_SPADES, "8765");
    addHasCards(SEAT_WEST, SUIT_HEARTS, "654");
    addHasCards(SEAT_WEST, SUIT_DIAMONDS, "432");
    addHasCards(SEAT_WEST, SUIT_CLUBS, "AK");

    const HistoryDerivedConstructionResult unconstrained =
      ConstructCandidateWorldsFromHistory(spec, unconstrainedInfo);
    Check(unconstrained.worlds.size() == 2,
      "without constructor-local hand-type pruning the full hidden-hand fixture should keep the two legal ambiguous spade-versus-club worlds");

    BridgeInformationState constrainedInfo(unconstrainedInfo);
    constrainedInfo.biddingConstraints.push_back(
      WorldConstraint::HandTypeConstraint(SEAT_EAST, HAND_TYPE_ONE_SUITER));

    const HistoryDerivedConstructionResult constrained =
      ConstructCandidateWorldsFromHistory(spec, constrainedInfo);
    Check(constrained.worlds.size() == 1,
      "constructor-local hand-type pruning should keep only the world where East remains a one-suiter");
    Check(WorldHasHandType(constrained.worlds[0], SEAT_EAST, HAND_TYPE_ONE_SUITER),
      "the world surviving constructor-local hand-type pruning should classify East as a one-suiter");
    Check(WorldHasCard(constrained.worlds[0], SEAT_EAST, SUIT_SPADES, 'T'),
      "constructor-local hand-type pruning should keep the world where East receives the ambiguous spade instead of the ambiguous club");

    const HistoryDerivedConstructionExplanation explanation =
      ExplainHistoryDerivedConstruction(spec, constrainedInfo);
    Check(explanation.stats.afterConstructorLengthCount == 2 &&
          explanation.stats.afterConstructorHCPCount == 2 &&
          explanation.stats.afterConstructorBalancedCount == 1,
      "constructor-local hand-type explanation should preserve both full-hand candidates through length and HCP stages, then narrow to one at the hand-type stage");
  }


  static void TestHistoryDerivedConstructionHandTypeDefersOnIncompleteHands()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld("N:A.9.. K.Q.. 2.8.. J.T..");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);

    BridgeInformationState unconstrainedInfo;
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'K')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));

    BridgeInformationState constrainedInfo(unconstrainedInfo);
    constrainedInfo.biddingConstraints.push_back(
      WorldConstraint::HandTypeConstraint(SEAT_EAST, HAND_TYPE_ONE_SUITER));

    const HistoryDerivedConstructionResult constrained =
      ConstructCandidateWorldsFromHistory(spec, constrainedInfo);
    Check(constrained.worlds.size() == 2,
      "constructor-local hand-type pruning should defer on incomplete hidden hands instead of pruning the short toy fixture early");

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(constrained.worlds,
      constrainedInfo, &stats);
    Check(mask == WorldMask(2, 0x0ULL),
      "the later full bidding filter should still reject incomplete short hidden-hand worlds for a hand-type requirement");
    Check(stats.afterBiddingCount == 0,
      "later bidding-stage filtering should reject every incomplete short hidden-hand world under a hand-type requirement");
  }


  static void TestHistoryDerivedConstructionBalancedShapeDefersOnIncompleteHands()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld("N:A.9.. K.Q.. 2.8.. J.T..");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);

    BridgeInformationState unconstrainedInfo;
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'K')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));

    const HistoryDerivedConstructionResult unconstrained =
      ConstructCandidateWorldsFromHistory(spec, unconstrainedInfo);
    Check(unconstrained.worlds.size() == 2,
      "without balanced constraints the short hidden-seat fixture should keep both legal heart-swap worlds");

    BridgeInformationState constrainedInfo(unconstrainedInfo);
    constrainedInfo.biddingConstraints.push_back(
      WorldConstraint::Balanced(SEAT_EAST));

    const HistoryDerivedConstructionResult constrained =
      ConstructCandidateWorldsFromHistory(spec, constrainedInfo);
    Check(constrained.worlds.size() == 2,
      "constructor-local balanced pruning should defer on incomplete hidden hands instead of pruning the short toy fixture early");

    WorldGenerationStats stats;
    const WorldMask mask = GeneratePossibleWorlds(constrained.worlds,
      constrainedInfo, &stats);
    Check(mask == WorldMask(2, 0x0ULL),
      "the later full bidding filter should still reject the incomplete short hidden-hand worlds for a balanced-shape requirement");
    Check(stats.afterBiddingCount == 0,
      "later bidding-stage filtering should reject every incomplete short hidden-hand world under a balanced-shape requirement");
  }


  static void TestHistoryDerivedConstructionUsesDerivedFollowSuitLength()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld("N:A9... KQ.JT.. 2... J..98.");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);

    BridgeInformationState unconstrainedInfo;
    unconstrainedInfo.deriveFollowSuitConstraints = false;
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'K')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));
    unconstrainedInfo.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, '9')));
    unconstrainedInfo.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_HEARTS, 'J')));

    const HistoryDerivedConstructionResult unconstrained =
      ConstructCandidateWorldsFromHistory(spec, unconstrainedInfo);
    Check(unconstrained.worlds.size() == 6,
      "without constructor-local follow-suit pruning the longer post-lead hidden-seat fixture should keep all six legal queen-plus-red-card assignment worlds");

    BridgeInformationState constrainedInfo(unconstrainedInfo);
    constrainedInfo.deriveFollowSuitConstraints = true;

    WorldGenerationStats stagedStats;
    const WorldMask stagedMask = GeneratePossibleWorlds(unconstrained.worlds,
      constrainedInfo, &stagedStats);
    Check(stagedMask.PopCount() == 3,
      "the later staged filters should narrow the unconstrained longer-history pool to the three worlds where East cannot still hold a hidden spade after discarding on the second spade lead");
    Check(stagedStats.afterFollowSuitCount == 3,
      "the explicit follow-suit stage should account for the longer-history narrowing before current-trick replay is checked");

    const HistoryDerivedConstructionResult constrained =
      ConstructCandidateWorldsFromHistory(spec, constrainedInfo);
    Check(constrained.worlds.size() == 3,
      "constructor-local derived follow-suit pruning should reduce the longer post-lead hidden-seat pool to the same three worlds before later filtering");

    for (unsigned i = 0; i < constrained.worlds.size(); i++)
    {
      Check(! WorldHasCard(constrained.worlds[i], SEAT_EAST, SUIT_SPADES, 'Q'),
        "every world surviving constructor-local follow-suit pruning should move the remaining hidden spade queen away from East after the second-round discard");
      Check(WorldHasCard(constrained.worlds[i], SEAT_EAST, SUIT_HEARTS, 'J'),
        "every world surviving constructor-local follow-suit pruning should still pin East's current-trick heart discard to East");
    }

    bool sawEastHeartT = false;
    bool sawEastDiamond9 = false;
    for (unsigned i = 0; i < constrained.worlds.size(); i++)
    {
      if (WorldHasCard(constrained.worlds[i], SEAT_EAST, SUIT_HEARTS, 'T'))
        sawEastHeartT = true;
      if (WorldHasCard(constrained.worlds[i], SEAT_EAST, SUIT_DIAMONDS, '9'))
        sawEastDiamond9 = true;
    }
    Check(sawEastHeartT && sawEastDiamond9,
      "constructor-local follow-suit pruning should still leave multiple longer-history red-card assignments unresolved across the surviving worlds");

    WorldGenerationStats constructorStats;
    const WorldMask constructorMask = GeneratePossibleWorlds(constrained.worlds,
      constrainedInfo, &constructorStats);
    Check(constructorMask == WorldMask(3, 0x7ULL),
      "worlds surviving constructor-local follow-suit pruning should pass the later staged filters unchanged");
    Check(constructorStats.candidateWorldCount == 3,
      "world-generation stats should report the constructor-pruned three-world pool for the longer-history fixture");

    const WorldGenerationExplanation explanation =
      ExplainPossibleWorldGeneration(unconstrained.worlds, constrainedInfo);
    unsigned rejectedAtFollowSuit = 0;
    unsigned acceptedWorlds = 0;
    for (unsigned i = 0; i < explanation.worlds.size(); i++)
    {
      if (explanation.worlds[i].accepted)
        acceptedWorlds++;
      else if (explanation.worlds[i].rejectionStage == "follow_suit")
        rejectedAtFollowSuit++;
    }
    Check(acceptedWorlds == 3 && rejectedAtFollowSuit == 3,
      "world-generation explanation should show the longer-history pool splitting into three accepted worlds and three follow-suit rejections");
  }


  static void TestHistoryDerivedConstructionCarriesVoidSuitIntoConstructor()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld("N:A... K.Q.. 2... J.T..");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);

    BridgeInformationState unconstrainedInfo;
    unconstrainedInfo.deriveFollowSuitConstraints = false;
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_HEARTS, 'Q')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));

    const HistoryDerivedConstructionResult unconstrained =
      ConstructCandidateWorldsFromHistory(spec, unconstrainedInfo);
    Check(unconstrained.worlds.size() == 2,
      "without constructor-local void-suit carry-through the first-show-out fixture should keep both legal hidden spade-versus-heart swap worlds");

    BridgeInformationState constrainedInfo(unconstrainedInfo);
    constrainedInfo.deriveFollowSuitConstraints = true;

    WorldGenerationStats stagedStats;
    const WorldMask stagedMask = GeneratePossibleWorlds(unconstrained.worlds,
      constrainedInfo, &stagedStats);
    Check(stagedMask.PopCount() == 1,
      "the later staged filters should narrow the first-show-out fixture to the single world where East is actually void in spades");
    Check(stagedStats.afterFollowSuitCount == 1,
      "the staged follow-suit filter should account for the first-show-out void-suit narrowing");

    const HistoryDerivedConstructionResult constrained =
      ConstructCandidateWorldsFromHistory(spec, constrainedInfo);
    Check(constrained.worlds.size() == 1,
      "constructor-local void-suit carry-through should reduce the first-show-out fixture to the same single world before later filtering");
    Check(! WorldHasCard(constrained.worlds[0], SEAT_EAST, SUIT_SPADES, 'K') &&
          WorldHasCard(constrained.worlds[0], SEAT_WEST, SUIT_SPADES, 'K') &&
          WorldHasCard(constrained.worlds[0], SEAT_EAST, SUIT_HEARTS, 'T'),
      "constructor-local void-suit carry-through should move the remaining hidden spade away from East and leave East with the remaining hidden heart");

    WorldGenerationStats constructorStats;
    const WorldMask constructorMask = GeneratePossibleWorlds(constrained.worlds,
      constrainedInfo, &constructorStats);
    Check(constructorMask == WorldMask(1, 0x1ULL),
      "the world surviving constructor-local void-suit pruning should pass the later staged filters unchanged");
    Check(constructorStats.candidateWorldCount == 1 &&
          constructorStats.afterFollowSuitCount == 1,
      "world-generation stats should see the constructor-pruned one-world pool unchanged at the follow-suit stage");

    const HistoryDerivedConstructionExplanation constructorExplanation =
      ExplainHistoryDerivedConstruction(spec, constrainedInfo);
    unsigned acceptedConstructorWorlds = 0;
    unsigned rejectedAtConstructorLength = 0;
    for (unsigned i = 0; i < constructorExplanation.worlds.size(); i++)
    {
      if (constructorExplanation.worlds[i].accepted)
        acceptedConstructorWorlds++;
      else if (constructorExplanation.worlds[i].rejectionStage == "constructor_length" &&
               constructorExplanation.worlds[i].rejectionReason.find("void in spades") != string::npos)
      {
        rejectedAtConstructorLength++;
      }
    }
    Check(acceptedConstructorWorlds == 1 && rejectedAtConstructorLength == 1,
      "constructor-local void-suit explanation should show one accepted world and one constructor_length rejection at the derived void-suit constraint");

    const WorldGenerationExplanation stagedExplanation =
      ExplainPossibleWorldGeneration(unconstrained.worlds, constrainedInfo);
    unsigned acceptedStagedWorlds = 0;
    unsigned rejectedAtFollowSuit = 0;
    for (unsigned i = 0; i < stagedExplanation.worlds.size(); i++)
    {
      if (stagedExplanation.worlds[i].accepted)
        acceptedStagedWorlds++;
      else if (stagedExplanation.worlds[i].rejectionStage == "follow_suit" &&
               stagedExplanation.worlds[i].rejectionReason.find("void in spades") != string::npos)
      {
        rejectedAtFollowSuit++;
      }
    }
    Check(acceptedStagedWorlds == 1 && rejectedAtFollowSuit == 1,
      "staged world-generation explanation should still describe the same first-show-out narrowing as a follow_suit rejection at the derived void-suit constraint");
  }


  static void TestHistoryDerivedConstructionInfersHiddenCardsFromVisibleHands()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld(
      "N:AK93.A93.A83.A83 J.876.JT7.J96 842.KQ2.KQ2.KQ72 T765.54.654.T4");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);
    spec.inferHiddenCardsFromVisibleHands = true;

    BridgeInformationState unconstrainedInfo;
    unconstrainedInfo.deriveFollowSuitConstraints = false;
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '5')));
    unconstrainedInfo.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, '9')));
    unconstrainedInfo.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_HEARTS, 'J')));

    const HistoryDerivedConstructionResult unconstrained =
      ConstructCandidateWorldsFromHistory(spec, unconstrainedInfo);
    Check(unconstrained.hiddenCards.size() == 5,
      "visible-seed history-derived construction should infer the five-card hidden East/West complement from the full deck");
    Check(unconstrained.worlds.size() == 6,
      "without derived follow-suit pruning the visible-seed longer-history fixture should keep all six legal allocations of the remaining four ambiguous hidden cards");
    Check(unconstrained.visibleSeedWorld.suits[SEAT_EAST][SUIT_SPADES] == "J" &&
          unconstrained.visibleSeedWorld.suits[SEAT_WEST][SUIT_CLUBS] == "T4",
      "visible-seed construction should preserve already-known hidden-seat cards instead of clearing them like curated full hidden hands");

    bool sawHiddenSpadeQ = false;
    bool sawPinnedHeartJ = false;
    bool sawHiddenClub5 = false;
    for (unsigned i = 0; i < unconstrained.hiddenCards.size(); i++)
    {
      if (unconstrained.hiddenCards[i].card == BridgeMove(SUIT_SPADES, 'Q'))
        sawHiddenSpadeQ = true;
      if (unconstrained.hiddenCards[i].card == BridgeMove(SUIT_HEARTS, 'J') &&
          unconstrained.hiddenCards[i].allowedSeatsMask == SeatBit(SEAT_EAST))
      {
        sawPinnedHeartJ = true;
      }
      if (unconstrained.hiddenCards[i].card == BridgeMove(SUIT_CLUBS, '5'))
        sawHiddenClub5 = true;
    }
    Check(sawHiddenSpadeQ && sawPinnedHeartJ && sawHiddenClub5,
      "visible-seed construction should infer the missing hidden-card complement and still pin current-trick ownership for cards already shown by play history");

    BridgeInformationState constrainedInfo(unconstrainedInfo);
    constrainedInfo.deriveFollowSuitConstraints = true;

    WorldGenerationStats stagedStats;
    const WorldMask stagedMask = GeneratePossibleWorlds(unconstrained.worlds,
      constrainedInfo, &stagedStats);
    Check(stagedMask.PopCount() == 3,
      "later staged filtering should narrow the visible-seed longer-history pool to the three worlds where East cannot still hold the hidden spade queen after discarding on the second spade lead");
    Check(stagedStats.afterFollowSuitCount == 3,
      "the explicit follow-suit stage should report the visible-seed longer-history narrowing before current-trick replay is checked");

    const HistoryDerivedConstructionResult constrained =
      ConstructCandidateWorldsFromHistory(spec, constrainedInfo);
    Check(constrained.worlds.size() == 3,
      "constructor-local visible-seed follow-suit pruning should reduce the inferred hidden-card pool to the same three worlds before later filtering");

    bool sawEastHeartT = false;
    bool sawEastDiamond9 = false;
    bool sawEastClub5 = false;
    for (unsigned i = 0; i < constrained.worlds.size(); i++)
    {
      Check(! WorldHasCard(constrained.worlds[i], SEAT_EAST, SUIT_SPADES, 'Q'),
        "every visible-seed world surviving constructor-local follow-suit pruning should move the hidden spade queen away from East after the second-round discard");
      Check(WorldHasCard(constrained.worlds[i], SEAT_EAST, SUIT_HEARTS, 'J'),
        "every visible-seed world surviving constructor-local follow-suit pruning should still pin East's current-trick heart discard to East");
      if (WorldHasCard(constrained.worlds[i], SEAT_EAST, SUIT_HEARTS, 'T'))
        sawEastHeartT = true;
      if (WorldHasCard(constrained.worlds[i], SEAT_EAST, SUIT_DIAMONDS, '9'))
        sawEastDiamond9 = true;
      if (WorldHasCard(constrained.worlds[i], SEAT_EAST, SUIT_CLUBS, '5'))
        sawEastClub5 = true;
    }
    Check(sawEastHeartT && sawEastDiamond9 && sawEastClub5,
      "visible-seed construction should still leave multiple longer-history red-and-club assignments unresolved across the surviving inferred worlds");

    WorldGenerationStats constructorStats;
    const WorldMask constructorMask = GeneratePossibleWorlds(constrained.worlds,
      constrainedInfo, &constructorStats);
    Check(constructorMask == WorldMask(3, 0x7ULL),
      "visible-seed worlds surviving constructor-local follow-suit pruning should pass the later staged filters unchanged");
    Check(constructorStats.candidateWorldCount == 3,
      "world-generation stats should report the constructor-pruned three-world inferred visible-seed pool for the longer-history fixture");

    const WorldGenerationExplanation explanation =
      ExplainPossibleWorldGeneration(unconstrained.worlds, constrainedInfo);
    unsigned rejectedAtFollowSuit = 0;
    unsigned acceptedWorlds = 0;
    for (unsigned i = 0; i < explanation.worlds.size(); i++)
    {
      if (explanation.worlds[i].accepted)
        acceptedWorlds++;
      else if (explanation.worlds[i].rejectionStage == "follow_suit")
        rejectedAtFollowSuit++;
    }
    Check(acceptedWorlds == 3 && rejectedAtFollowSuit == 3,
      "world-generation explanation should show the inferred visible-seed longer-history pool splitting into three accepted worlds and three follow-suit rejections");
  }


  static void TestHistoryDerivedConstructionSupportsModeratelyLargerVisibleSeedPools()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld(
      "N:AK93.A93.A83.A83 J.876.JT.J96 842.KQ2.KQ2.KQ72 T765.54.654.T");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);
    spec.inferHiddenCardsFromVisibleHands = true;

    BridgeInformationState unconstrainedInfo;
    unconstrainedInfo.deriveFollowSuitConstraints = false;
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '5')));
    unconstrainedInfo.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, '9')));
    unconstrainedInfo.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_HEARTS, 'J')));

    const HistoryDerivedConstructionResult unconstrained =
      ConstructCandidateWorldsFromHistory(spec, unconstrainedInfo);
    Check(unconstrained.hiddenCards.size() == 7,
      "the moderate visible-seed fixture should infer a seven-card hidden East/West complement from the full deck");
    Check(unconstrained.worlds.size() == 20,
      "without constructor-local follow-suit pruning the moderate visible-seed fixture should keep all twenty legal assignments of the six remaining ambiguous hidden cards after East's pinned discard card");

    BridgeInformationState constrainedInfo(unconstrainedInfo);
    constrainedInfo.deriveFollowSuitConstraints = true;

    WorldGenerationStats constrainedStats;
    const WorldMask constrainedMask = GeneratePossibleWorlds(unconstrained.worlds,
      constrainedInfo, &constrainedStats);
    Check(constrainedMask.PopCount() == 10,
      "the staged filters should narrow the moderate visible-seed pool from twenty worlds to ten once East's second-round spade discard rules out the hidden queen of spades");
    Check(constrainedStats.afterFollowSuitCount == 10,
      "follow-suit stats should report the moderate visible-seed narrowing before later stages");

    const HistoryDerivedConstructionResult constrained =
      ConstructCandidateWorldsFromHistory(spec, constrainedInfo);
    Check(constrained.worlds.size() == 10,
      "constructor-local follow-suit pruning should reduce the moderate visible-seed pool to the same ten worlds before later filtering");

    for (unsigned i = 0; i < constrained.worlds.size(); i++)
    {
      Check(! WorldHasCard(constrained.worlds[i], SEAT_EAST, SUIT_SPADES, 'Q'),
        "every world surviving constructor-local pruning in the moderate visible-seed fixture should move the hidden spade queen away from East");
      Check(WorldHasCard(constrained.worlds[i], SEAT_EAST, SUIT_HEARTS, 'J'),
        "every world surviving constructor-local pruning in the moderate visible-seed fixture should keep East's current-trick heart discard pinned to East");
    }

    BridgeInformationState sampledA(constrainedInfo);
    sampledA.sampleLimit = 4;
    sampledA.samplingSeed = 2;
    WorldGenerationStats sampledAStats;
    const WorldMask sampleA = GeneratePossibleWorlds(constrained.worlds,
      sampledA, &sampledAStats);
    const WorldMask sampleARepeat = GeneratePossibleWorlds(constrained.worlds,
      sampledA, NULL);

    BridgeInformationState sampledB(sampledA);
    sampledB.samplingSeed = 5;
    WorldGenerationStats sampledBStats;
    const WorldMask sampleB = GeneratePossibleWorlds(constrained.worlds,
      sampledB, &sampledBStats);

    Check(sampleA == sampleARepeat,
      "deterministic sampling over the moderate visible-seed pool should reproduce the same sampled world mask for the same seed");
    Check(sampleA.PopCount() == 4,
      "deterministic sampling over the moderate visible-seed pool should cap the surviving worlds at the configured sample limit");
    Check(sampleB.PopCount() == 4,
      "deterministic sampling over the moderate visible-seed pool should retain the requested number of worlds for a different seed as well");
    Check(! (sampleA == sampleB),
      "deterministic sampling over the moderate visible-seed pool should allow the sampling seed to shift which canonical worlds survive");
    Check(sampledAStats.afterFollowSuitCount == 10,
      "sampling should leave the moderate visible-seed post-follow-suit count measurable before downselection");
    Check(sampledAStats.afterSamplingCount == 4 && sampledAStats.sampledOutWorlds == 6,
      "sampling stats should report the moderate visible-seed pool shrinking from ten constructor-pruned worlds to four retained worlds");
    Check(sampledBStats.finalWorldCount == 4,
      "the final world count for the moderate visible-seed sampled pool should equal the configured sample limit");

    const WorldGenerationExplanation explanation =
      ExplainPossibleWorldGeneration(constrained.worlds, sampledA);
    unsigned acceptedWorlds = 0;
    unsigned sampledOutWorlds = 0;
    for (unsigned i = 0; i < explanation.worlds.size(); i++)
    {
      if (explanation.worlds[i].accepted)
        acceptedWorlds++;
      else if (explanation.worlds[i].rejectionStage == "sampling")
        sampledOutWorlds++;
    }
    Check(acceptedWorlds == 4 && sampledOutWorlds == 6,
      "world-generation explanation should show the moderate visible-seed constructor-pruned pool retaining four deterministic samples and rejecting six worlds only at the sampling stage");
  }


  static void TestHistoryDerivedConstructionUsesLongerVisibleSeedHistory()
  {
    HistoryDerivedWorldSpec spec;
    spec.seedWorld = ParsePBNWorld(
      "N:AK93.A93.A83.A83 J.876.JT.J96 842.KQ2.KQ2.KQ72 T765.54.654.T");
    spec.hiddenSeats.push_back(SEAT_EAST);
    spec.hiddenSeats.push_back(SEAT_WEST);
    spec.inferHiddenCardsFromVisibleHands = true;

    BridgeInformationState unconstrainedInfo;
    unconstrainedInfo.deriveFollowSuitConstraints = false;
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, 'A')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'J')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '2')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '5')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_SPADES, '9')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_SPADES,
      BridgeMove(SUIT_HEARTS, 'J')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, '8')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_SPADES,
      BridgeMove(SUIT_SPADES, 'T')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_CLUBS, 'A')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_CLUBS,
      BridgeMove(SUIT_CLUBS, 'J')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_CLUBS,
      BridgeMove(SUIT_CLUBS, '2')));
    unconstrainedInfo.playHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_CLUBS,
      BridgeMove(SUIT_CLUBS, 'T')));
    unconstrainedInfo.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_NORTH,
      -1,
      BridgeMove(SUIT_CLUBS, '8')));
    unconstrainedInfo.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_EAST,
      SUIT_CLUBS,
      BridgeMove(SUIT_CLUBS, '6')));
    unconstrainedInfo.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_SOUTH,
      SUIT_CLUBS,
      BridgeMove(SUIT_CLUBS, '7')));
    unconstrainedInfo.currentTrickHistory.push_back(PlayHistoryEvent(
      SEAT_WEST,
      SUIT_CLUBS,
      BridgeMove(SUIT_HEARTS, '5')));

    const HistoryDerivedConstructionResult unconstrained =
      ConstructCandidateWorldsFromHistory(spec, unconstrainedInfo);
    Check(unconstrained.hiddenCards.size() == 7,
      "the longer visible-seed history fixture should still infer the seven-card hidden East/West complement from the full deck");
    Check(unconstrained.worlds.size() == 20,
      "without constructor-local follow-suit pruning the longer visible-seed history fixture should keep the full twenty-world hidden-card pool");

    BridgeInformationState constrainedInfo(unconstrainedInfo);
    constrainedInfo.deriveFollowSuitConstraints = true;

    WorldGenerationStats stagedStats;
    const WorldMask stagedMask = GeneratePossibleWorlds(unconstrained.worlds,
      constrainedInfo, &stagedStats);
    Check(stagedMask.PopCount() == 3,
      "the staged filters should narrow the longer visible-seed history pool from twenty worlds to three once East's second spade discard and West's second club discard are both enforced");
    Check(stagedStats.afterFollowSuitCount == 3,
      "the explicit follow-suit stage should account for the entire longer-history narrowing before replay legality is checked");

    const HistoryDerivedConstructionResult constrained =
      ConstructCandidateWorldsFromHistory(spec, constrainedInfo);
    Check(constrained.worlds.size() == 3,
      "constructor-local follow-suit pruning should reduce the longer visible-seed history pool to the same three worlds before later filtering");

    bool sawEastHeartT = false;
    bool sawEastDiamond9 = false;
    bool sawEastDiamond7 = false;
    for (unsigned i = 0; i < constrained.worlds.size(); i++)
    {
      Check(WorldHasCard(constrained.worlds[i], SEAT_WEST, SUIT_SPADES, 'Q'),
        "every world surviving the longer-history constructor pruning should move the hidden spade queen to West after East discards on the second spade lead");
      Check(WorldHasCard(constrained.worlds[i], SEAT_EAST, SUIT_CLUBS, '5') &&
            WorldHasCard(constrained.worlds[i], SEAT_EAST, SUIT_CLUBS, '4'),
        "every world surviving the longer-history constructor pruning should move both hidden clubs to East after West discards on the second club lead");
      Check(WorldHasCard(constrained.worlds[i], SEAT_EAST, SUIT_HEARTS, 'J'),
        "every world surviving the longer-history constructor pruning should still pin East's earlier heart discard to East");
      if (WorldHasCard(constrained.worlds[i], SEAT_EAST, SUIT_HEARTS, 'T'))
        sawEastHeartT = true;
      if (WorldHasCard(constrained.worlds[i], SEAT_EAST, SUIT_DIAMONDS, '9'))
        sawEastDiamond9 = true;
      if (WorldHasCard(constrained.worlds[i], SEAT_EAST, SUIT_DIAMONDS, '7'))
        sawEastDiamond7 = true;
    }
    Check(sawEastHeartT && sawEastDiamond9 && sawEastDiamond7,
      "the longer-history constructor pruning should still leave a three-way ambiguity over East's last hidden red card after the black-suit deductions are applied");

    WorldGenerationStats constructorStats;
    const WorldMask constructorMask = GeneratePossibleWorlds(constrained.worlds,
      constrainedInfo, &constructorStats);
    Check(constructorMask == WorldMask(3, 0x7ULL),
      "worlds surviving longer-history constructor pruning should pass the later staged filters unchanged");
    Check(constructorStats.candidateWorldCount == 3,
      "world-generation stats should report the constructor-pruned three-world longer-history pool");

    const WorldGenerationExplanation explanation =
      ExplainPossibleWorldGeneration(unconstrained.worlds, constrainedInfo);
    unsigned acceptedWorlds = 0;
    unsigned rejectedAtFollowSuit = 0;
    for (unsigned i = 0; i < explanation.worlds.size(); i++)
    {
      if (explanation.worlds[i].accepted)
        acceptedWorlds++;
      else if (explanation.worlds[i].rejectionStage == "follow_suit")
        rejectedAtFollowSuit++;
    }
    Check(acceptedWorlds == 3 && rejectedAtFollowSuit == 17,
      "world-generation explanation should show the longer visible-seed history splitting the twenty-world pool into three accepted worlds and seventeen follow-suit rejections");
  }


  static void TestDeterministicWorldSampling()
  {
    vector<ParsedWorld> worlds;
    worlds.push_back(ParsePBNWorld(
      "N:AKQ2.JT9.AKQ.JT9 765.8765.JT9.876 JT98.AKQ.432.AKQ 43.432.8765.5432"));
    worlds.push_back(ParsePBNWorld(
      "N:AKQ2.JT9.AKQ.JT9 7654.876.JT9.876 JT98.AKQ.432.AKQ 3.5432.8765.5432"));
    worlds.push_back(ParsePBNWorld(
      "N:AKQ2.JT9.AKQ.JT9 76543.876.JT.876 JT98.AKQ.432.AKQ .5432.98765.5432"));
    worlds.push_back(ParsePBNWorld(
      "N:AKQ2.JT9.AKQ.JT9 76543..JT987.876 JT98.AKQ.432.AKQ .8765432.65.5432"));

    BridgeInformationState first;
    first.sampleLimit = 2;
    first.samplingSeed = 0;
    WorldGenerationStats firstStats;
    const WorldMask sampleA = GeneratePossibleWorlds(worlds, first, &firstStats);
    const WorldMask sampleARepeat = GeneratePossibleWorlds(worlds, first, NULL);

    BridgeInformationState second(first);
    second.samplingSeed = 1;
    WorldGenerationStats secondStats;
    const WorldMask sampleB = GeneratePossibleWorlds(worlds, second, &secondStats);

    Check(sampleA == sampleARepeat,
      "deterministic sampling should reproduce the same sampled world mask for the same seed");
    Check(sampleA.PopCount() == 2,
      "deterministic sampling should cap the world mask at the configured sample limit");
    Check(sampleB.PopCount() == 2,
      "deterministic sampling should keep the requested number of worlds for a different seed as well");
    Check(! (sampleA == sampleB),
      "deterministic sampling should allow the seed to shift which canonical worlds are retained");
    Check(firstStats.sampledOutWorlds == 2,
      "deterministic sampling stats should report how many worlds were dropped by sample limiting");
    Check(firstStats.afterSamplingCount == 2,
      "deterministic sampling stats should record the post-sampling surviving world count");
    Check(secondStats.finalWorldCount == 2,
      "deterministic sampling should leave the final world count equal to the sample limit");
  }


  static void TestBridgeMoveGeneration()
  {
    BridgeState state;
    state.playerToMove = SEAT_EAST;
    state.leadSuit = SUIT_DIAMONDS;
    state.possibleWorlds = WorldMask(2, 0x3ULL);

    state.worlds.push_back(ParsePBNWorld(
      "N:AKQ2.JT9.AKQ.JT9 76543.876.JT.876 JT98.AKQ.432.AKQ .5432.98765.5432"));
    state.worlds.push_back(ParsePBNWorld(
      "N:AKQ2.JT9.AKQ.JT9 76543..JT987.876 JT98.AKQ.432.AKQ .8765432.65.5432"));

    const vector<BridgeMove> moves = GenerateBridgeMoves(state);
    Check(moves.size() == 5,
      "bridge move generation should union legal diamond plays across possible worlds");
    Check(moves[0] == BridgeMove(SUIT_DIAMONDS, '7'),
      "bridge move generation should include the lowest legal diamond from the union");
    Check(moves[4] == BridgeMove(SUIT_DIAMONDS, 'J'),
      "bridge move generation should include the highest legal diamond from the union");

    const BridgeState afterD8 = PlayBridgeMove(state,
      BridgeMove(SUIT_DIAMONDS, '8'));
    Check(afterD8.possibleWorlds == WorldMask(2, 0x2ULL),
      "playing D8 should eliminate the world where East could not legally play that diamond");
    Check(afterD8.playerToMove == SEAT_SOUTH,
      "bridge move generation should advance turn order after a play");
    Check(afterD8.leadSuit == SUIT_DIAMONDS,
      "bridge move generation should preserve the established lead suit within the trick");
    Check(! WorldHasCard(afterD8.worlds[1], SEAT_EAST, SUIT_DIAMONDS, '8'),
      "bridge move application should remove the played card from surviving worlds");

    const vector<BridgeChild> children = ExpandBridgeChildren(state);
    Check(children.size() == moves.size(),
      "bridge child expansion should create one child per legal bridge move");
    Check(children[1].move == BridgeMove(SUIT_DIAMONDS, '8'),
      "bridge child expansion should preserve move ordering");
    Check(children[1].state.possibleWorlds == WorldMask(2, 0x2ULL),
      "bridge child expansion should carry the filtered possible-world mask into the child state");
  }


  static void TestBridgeSearchControl()
  {
    BridgeState state;
    state.playerToMove = SEAT_NORTH;
    state.maxSide = 0;
    state.trumpSuit = -1;
    state.possibleWorlds = WorldMask(2, 0x3ULL);

    state.worlds.push_back(ParsePBNWorld("N:A... K... 2... 3..."));
    state.worlds.push_back(ParsePBNWorld("N:Q... K... A... 3..."));

    const vector<BridgeMove> moves = GenerateBridgeMoves(state);
    Check(moves.size() == 2,
      "bridge search control should offer the union of North's possible opening leads");
    Check(moves[0] == BridgeMove(SUIT_SPADES, 'Q'),
      "bridge search control should order opening leads by rank");
    Check(moves[1] == BridgeMove(SUIT_SPADES, 'A'),
      "bridge search control should include the alternative opening lead");

    BridgeState manual = PlayBridgeMove(state, BridgeMove(SUIT_SPADES, 'Q'));
    manual = PlayBridgeMove(manual, BridgeMove(SUIT_SPADES, 'K'));
    manual = PlayBridgeMove(manual, BridgeMove(SUIT_SPADES, 'A'));
    manual = PlayBridgeMove(manual, BridgeMove(SUIT_SPADES, '3'));

    Check(manual.currentTrick.empty(),
      "bridge search control should clear the trick after the fourth card");
    Check(manual.playerToMove == SEAT_SOUTH,
      "bridge search control should advance to the trick winner after completion");
    Check(manual.maxTricksWon == 1,
      "bridge search control should count a won trick for the Max side");

    const ParetoFront front = SearchBridgeState(state, 1);
    Check(front.vectors.size() == 2,
      "bridge search control should keep one sparse winning vector per viable opening lead");
    Check(FrontContains(front, MakeBinaryOutcome("1x")),
      "bridge search control should keep the lead that wins only in the first world");
    Check(FrontContains(front, MakeBinaryOutcome("x1")),
      "bridge search control should keep the lead that wins only in the second world");

    SearchExecutionContext singleWorldContext;
    singleWorldContext.bridgeSearch.hasUsefulWorlds = true;
    singleWorldContext.bridgeSearch.usefulWorlds = WorldMask(2, 0x1ULL);
    SearchExecutionContext emptyUsefulContext;
    emptyUsefulContext.bridgeSearch.hasUsefulWorlds = true;
    emptyUsefulContext.bridgeSearch.usefulWorlds = WorldMask(2, 0x0ULL);
    const ParetoFront zeroUsefulFront = SearchBridgeState(state, 1,
      emptyUsefulContext);
    OutcomeVector zeroUseful(2);
    zeroUseful.valid = WorldMask(2, 0x3ULL);
    Check(FrontContains(zeroUsefulFront, zeroUseful),
      "bridge search control should apply the real zero-useful-world cut without introducing worlds outside the active bridge-state mask");
  }


  static BridgeState MakeBridgeAncestorCutMinFixture()
  {
    BridgeState state;
    state.playerToMove = SEAT_EAST;
    state.maxSide = 0;
    state.trumpSuit = -1;
    state.trickLeader = SEAT_NORTH;
    state.leadSuit = SUIT_SPADES;
    state.currentTrick.push_back(BridgeMove(SUIT_SPADES, 'A'));
    state.currentTrickPlayers.push_back(SEAT_NORTH);
    state.possibleWorlds = WorldMask(2, 0x3ULL);
    state.worlds.push_back(ParsePBNWorld("N:... K... 2... 3..."));
    state.worlds.push_back(ParsePBNWorld("N:... Q... 2... 3..."));
    return state;
  }


  static BridgeState MakeBridgeAncestorCutRootFixture()
  {
    BridgeState state;
    state.playerToMove = SEAT_NORTH;
    state.maxSide = 0;
    state.trumpSuit = -1;
    state.trickLeader = SEAT_NORTH;
    state.leadSuit = -1;
    state.possibleWorlds = WorldMask(2, 0x3ULL);
    state.worlds.push_back(ParsePBNWorld("N:A.K.. K.2.. 2.3.. 3.4.."));
    state.worlds.push_back(ParsePBNWorld("N:A.K.. Q.2.. 2.3.. 3.4.."));
    return state;
  }


  static BridgeState MakeBridgeGuaranteedWinRootFixture()
  {
    BridgeState state;
    state.playerToMove = SEAT_NORTH;
    state.maxSide = 0;
    state.trumpSuit = -1;
    state.trickLeader = SEAT_NORTH;
    state.leadSuit = -1;
    state.possibleWorlds = WorldMask(2, 0x3ULL);
    state.worlds.push_back(ParsePBNWorld("N:A...2 K...3 Q...4 J...5"));
    state.worlds.push_back(ParsePBNWorld("N:A...2 Q...3 K...4 J...5"));
    return state;
  }


  static void TestBridgeAncestorEarlyCutAtRootMax()
  {
    const BridgeState state = MakeBridgeAncestorCutMinFixture();

    ParetoFront dominating(2);
    OutcomeVector dominatingVec(2);
    dominatingVec.valid = WorldMask(2, 0x3ULL);
    dominatingVec.values[0] = 1;
    dominatingVec.values[1] = 1;
    dominating.Insert(dominatingVec);

    SearchExecutionContext context;
    context.bridgeSearch.enableAncestorCuts = true;
    context.bridgeSearch.upperMaxFronts.push_back(&dominating);

    BridgeSearchStats stats;
    SetActiveBridgeSearchStats(&stats);
    const ParetoFront front = SearchBridgeState(state, 1, context);
    SetActiveBridgeSearchStats(NULL);


    const ParetoFront expectedCutFront = MakeSingleWorldFront(2, 1, 1);

    Check(front.ToString() == expectedCutFront.ToString(),
      "opt-in bridge ancestor cuts should stop after the first Min child once the optimistic completion is already dominated by the nearest ancestor Max front");
    Check(stats.optimisticCompletions >= 1,
      "opt-in bridge ancestor cuts should record at least one optimistic completion in the recursive bridge fixture");
    Check(stats.earlyAlphaCuts == 1,
      "opt-in bridge ancestor cuts should trigger exactly one nearest-ancestor early cut in the recursive bridge fixture");
  }


  static void TestBridgeAncestorEarlyCutPreservesExactTTReuse()
  {
    const BridgeState state = MakeBridgeAncestorCutMinFixture();

    ParetoFront dominating(2);
    OutcomeVector dominatingVec(2);
    dominatingVec.valid = WorldMask(2, 0x3ULL);
    dominatingVec.values[0] = 1;
    dominatingVec.values[1] = 1;
    dominating.Insert(dominatingVec);

    SearchExecutionContext cutContext;
    cutContext.bridgeSearch.enableAncestorCuts = true;
    cutContext.bridgeSearch.upperMaxFronts.push_back(&dominating);

    BridgeSearchStats stats;
    SetActiveBridgeSearchStats(&stats);
    const ParetoFront cutFront = SearchBridgeState(state, 1, cutContext);
    SetActiveBridgeSearchStats(NULL);

    const ParetoFront expectedCutFront = MakeSingleWorldFront(2, 1, 1);
    Check(cutFront.ToString() == expectedCutFront.ToString(),
      "opt-in bridge ancestor cuts should stop after the first Min child once the optimistic completion is already dominated by the ancestor Max front");
    Check(stats.optimisticCompletions >= 1,
      "opt-in Min-node bridge cuts should record the optimistic completion used for ancestor-front comparison");
    Check(stats.earlyAlphaCuts == 1,
      "opt-in Min-node bridge cuts should trigger exactly one nearest-ancestor early cut in the one-trick fixture");

    OutcomeVector exact(2);
    exact.valid = WorldMask(2, 0x3ULL);
    exact.values[0] = 1;
    exact.values[1] = 1;
    const ParetoFront directExactFront = SearchBridgeState(state, 1);
    Check(FrontContains(directExactFront, exact),
      "without the opt-in ancestor cut the same Min-node bridge fixture should evaluate to the exact [1 1] continuation");

    InitZobrist();
    BridgeTranspositionTable tt(1U << 8);
    BridgeTTStats cutTTStats;
    const ParetoFront cutTTFront = SearchBridgeStateWithTT(state, 1,
      cutContext, &tt, &cutTTStats);
    Check(cutTTFront.ToString() == expectedCutFront.ToString(),
      "TT-backed opt-in bridge ancestor cuts should preserve the same partial front returned by the optimistic early cut");

    BridgeTTStats exactTTStats;
    const ParetoFront exactTTFront = SearchBridgeStateWithTT(state, 1,
      SearchExecutionContext(), &tt, &exactTTStats);
    Check(FrontContains(exactTTFront, exact),
      "exact TT-backed bridge search should not reuse an inexact optimistic-cut front when the ancestor-cut context is absent");
  }


  static void TestBridgeDeepAlphaCut()
  {
    const BridgeState state = MakeBridgeAncestorCutMinFixture();

    ParetoFront deepDominating(2);
    OutcomeVector deepDominatingVec(2);
    deepDominatingVec.valid = WorldMask(2, 0x3ULL);
    deepDominatingVec.values[0] = 1;
    deepDominatingVec.values[1] = 1;
    deepDominating.Insert(deepDominatingVec);

    ParetoFront nearestNonDominating(2);

    SearchExecutionContext context;
    context.bridgeSearch.enableAncestorCuts = true;
    context.bridgeSearch.upperMaxFronts.push_back(&deepDominating);
    context.bridgeSearch.upperMaxFronts.push_back(&nearestNonDominating);

    BridgeSearchStats stats;
    SetActiveBridgeSearchStats(&stats);
    const ParetoFront front = SearchBridgeState(state, 1, context);
    SetActiveBridgeSearchStats(NULL);

    const ParetoFront expectedCutFront = MakeSingleWorldFront(2, 1, 1);
    Check(front.ToString() == expectedCutFront.ToString(),
      "bridge deep alpha cut should stop when an earlier ancestor Max front dominates the optimistic Min front even if the nearest ancestor does not");
    Check(stats.deepAlphaCuts >= 1,
      "bridge deep alpha cut regression should trigger at least one deep alpha cut in the focused bridge-backed fixture");
  }


  static void TestBridgeCutOnWin()
  {
    const BridgeState state = MakeBridgeGuaranteedWinRootFixture();

    SearchExecutionContext context;
    context.bridgeSearch.enableAncestorCuts = true;

    BridgeSearchStats stats;
    SetActiveBridgeSearchStats(&stats);
    const ParetoFront front = SearchBridgeState(state, 2, context);
    SetActiveBridgeSearchStats(NULL);

    OutcomeVector exact(2);
    exact.valid = WorldMask(2, 0x3ULL);
    exact.values[0] = 1;
    exact.values[1] = 1;


    Check(front.vectors.size() == 1 && FrontContains(front, exact),
      "bridge cut-on-win should preserve the exact winning root front when the first Max child already wins in every useful world");
    Check(stats.cutOnWinCuts == 1,
      "bridge cut-on-win regression should trigger exactly one cut-on-win at the Max node");
  }


  static void TestBridgeRootCut()
  {
    const BridgeState state = MakeBridgeGuaranteedWinRootFixture();
    SearchExecutionContext context;
    context.bridgeSearch.enableAncestorCuts = true;

    InitZobrist();
    BridgeTranspositionTable tt(1U << 8);
    BridgeTTStats firstStats;
    const BridgeRootReport first = AnalyzeBridgeRootWithTT(state, 2,
      context, &tt, &firstStats, NULL);

    BridgeSearchStats stats;
    SetActiveBridgeSearchStats(&stats);
    BridgeTTStats secondStats;
    const BridgeRootReport second = AnalyzeBridgeRootWithTT(state, 3,
      context, &tt, &secondStats, &first);
    SetActiveBridgeSearchStats(NULL);

    OutcomeVector exact(2);
    exact.valid = WorldMask(2, 0x3ULL);
    exact.values[0] = 1;
    exact.values[1] = 1;

    Check(first.rootFront.Mu() == second.rootFront.Mu(),
      "bridge root-cut regression should use a fixture whose root mu stabilizes across iterative deepening depths");
    Check(second.children.size() == 1,
      "bridge root cut should stop the deeper root analysis after the first dominating child when the root mu has already stabilized");
    Check(FrontContains(second.rootFront, exact),
      "bridge root cut should preserve the dominating exact root vector accumulated before cutting the remaining root children");
    Check(stats.rootCuts == 1,
      "bridge root-cut regression should trigger exactly one root cut in the deeper iteration");
  }


  static void TestBridgeRootReportThreeWorldContinuation()
  {
    BridgeState state;
    state.playerToMove = SEAT_NORTH;
    state.maxSide = 0;
    state.trumpSuit = -1;
    state.trickLeader = SEAT_NORTH;
    state.leadSuit = -1;
    state.possibleWorlds = WorldMask(3, 0x7ULL);
    state.worlds.push_back(ParsePBNWorld("N:2.K.. A.Q.. 3.2.. 4.3.."));
    state.worlds.push_back(ParsePBNWorld("N:2.Q.. A.J.. 3.2.. 4.3.."));
    state.worlds.push_back(ParsePBNWorld("N:2.J.. A.T.. 3.2.. 4.3.."));

    const vector<BridgeMove> moves = GenerateBridgeMoves(state);
    Check(moves.size() == 4,
      "three-world continuation should expose one merged spade lead plus three world-specific heart leads");
    Check(moves[0] == BridgeMove(SUIT_SPADES, '2'),
      "three-world continuation should include the merged spade lead first");
    Check(moves[1] == BridgeMove(SUIT_HEARTS, 'J') &&
          moves[2] == BridgeMove(SUIT_HEARTS, 'Q') &&
          moves[3] == BridgeMove(SUIT_HEARTS, 'K'),
      "three-world continuation should include the three world-specific heart leads after the shared spade lead");

    const BridgeRootReport report = AnalyzeBridgeRoot(state, 2);
    Check(report.children.size() == 4,
      "bridge root reporting should produce one child report per legal root move");

    const ParetoFront rootFront = SearchBridgeState(state, 2);
    Check(report.rootFront.vectors.size() == rootFront.vectors.size(),
      "bridge root reporting should reproduce the same root front size as direct search");

    OutcomeVector mergedSpade(3);
    mergedSpade.valid = WorldMask(3, 0x7ULL);
    mergedSpade.values[0] = 1;
    mergedSpade.values[1] = 1;
    mergedSpade.values[2] = 1;

    OutcomeVector world0Heart(3);
    world0Heart.valid = WorldMask(3, 0x1ULL);
    world0Heart.values[0] = 1;

    OutcomeVector world1Heart(3);
    world1Heart.valid = WorldMask(3, 0x2ULL);
    world1Heart.values[1] = 1;

    OutcomeVector world2Heart(3);
    world2Heart.valid = WorldMask(3, 0x4ULL);
    world2Heart.values[2] = 1;

    Check(FrontContains(report.rootFront, mergedSpade),
      "bridge root reporting should preserve the merged three-world continuation front [1 1 1]");
    Check(FrontContains(report.rootFront, world0Heart),
      "bridge root reporting should preserve the world-0-only continuation front [1 x x]");
    Check(FrontContains(report.rootFront, world1Heart),
      "bridge root reporting should preserve the world-1-only continuation front [x 1 x]");
    Check(FrontContains(report.rootFront, world2Heart),
      "bridge root reporting should preserve the world-2-only continuation front [x x 1]");

    bool sawMerged = false;
    bool sawWorld0 = false;
    bool sawWorld1 = false;
    bool sawWorld2 = false;
    for (unsigned i = 0; i < report.children.size(); i++)
    {
      const BridgeRootChildReport& child = report.children[i];
      if (child.move == BridgeMove(SUIT_SPADES, '2'))
      {
        sawMerged = true;
        Check(child.validWorlds == WorldMask(3, 0x7ULL),
          "the shared spade lead should keep all three worlds valid after the deeper continuation");
        Check(FrontContains(child.front, mergedSpade),
          "the shared spade lead should yield the exact merged continuation front [1 1 1]");
      }
      else if (child.move == BridgeMove(SUIT_HEARTS, 'K'))
      {
        sawWorld0 = true;
        Check(child.validWorlds == WorldMask(3, 0x1ULL),
          "the heart-K lead should isolate world 0 only");
        Check(FrontContains(child.front, world0Heart),
          "the heart-K lead should yield the world-0-only continuation front [1 x x]");
      }
      else if (child.move == BridgeMove(SUIT_HEARTS, 'Q'))
      {
        sawWorld1 = true;
        Check(child.validWorlds == WorldMask(3, 0x2ULL),
          "the heart-Q lead should isolate world 1 only");
        Check(FrontContains(child.front, world1Heart),
          "the heart-Q lead should yield the world-1-only continuation front [x 1 x]");
      }
      else if (child.move == BridgeMove(SUIT_HEARTS, 'J'))
      {
        sawWorld2 = true;
        Check(child.validWorlds == WorldMask(3, 0x4ULL),
          "the heart-J lead should isolate world 2 only");
        Check(FrontContains(child.front, world2Heart),
          "the heart-J lead should yield the world-2-only continuation front [x x 1]");
      }
    }

    Check(sawMerged && sawWorld0 && sawWorld1 && sawWorld2,
      "bridge root reporting should cover the merged branch and all three split branches in the three-world continuation");
  }


  static void TestBridgeMultiTrickDDSLeaf()
  {
    SetMaxThreads(0);

    HandFileData data;
    LoadHandFile(kAlphaMuPlayHandFile, data);
    Check(data.number >= 1,
      "alpha_mu_play.txt should provide at least one real DDS world for the multi-trick bridge test");

    const int handno = 0;
    BridgeState state;
    state.worlds.push_back(ParsePBNWorld(data.dealList[handno].remainCards));
    state.possibleWorlds = WorldMask(1, 0x1ULL);
    state.playerToMove = data.dealList[handno].first;
    state.maxSide = SeatSide(state.playerToMove);
    state.trumpSuit = (data.dealList[handno].trump == 4 ? -1 :
      data.dealList[handno].trump);
    state.trickLeader = state.playerToMove;
    state.leadSuit = -1;

    const vector<BridgeMove> moves = GenerateBridgeMoves(state);
    Check(! moves.empty(),
      "multi-trick bridge test should expose at least one legal opening move in the real DDS world");

    OutcomeVector optimum(1);
    optimum.valid = WorldMask(1, 0x1ULL);
    optimum.values[0] = BestScore(data.futList[handno]);

    const ParetoFront directLeaf = SearchBridgeState(state, 0);
    Check(directLeaf.vectors.size() == 1,
      "bridge DDS leaf evaluation should collapse to a single exact-score vector in a one-world state");
    Check(FrontContains(directLeaf, optimum),
      "bridge DDS leaf evaluation should match the golden FUT optimum on a real DDS world");

    const ParetoFront front = SearchBridgeState(state, 1);
    Check(front.vectors.size() == 1,
      "one full searched trick plus a DDS bridge leaf should still collapse to a single exact-score vector in a one-world state");
    Check(FrontContains(front, optimum),
      "one full searched trick plus a DDS bridge leaf should preserve the golden FUT optimum on a real DDS world");


    BridgeState multiLeafState;
    multiLeafState.playerToMove = SEAT_NORTH;
    multiLeafState.maxSide = 0;
    multiLeafState.trumpSuit = -1;
    multiLeafState.trickLeader = SEAT_NORTH;
    multiLeafState.leadSuit = -1;
    multiLeafState.possibleWorlds = WorldMask(2, 0x3ULL);
    multiLeafState.worlds.push_back(ParsePBNWorld("N:A... K... 2... 3..."));
    multiLeafState.worlds.push_back(ParsePBNWorld("N:Q... K... A... 3..."));

    OutcomeVector multiLeafExact(2);
    multiLeafExact.valid = WorldMask(2, 0x3ULL);
    multiLeafExact.values[0] = ExactBridgeDDSScoreForWorld(multiLeafState, 0);
    multiLeafExact.values[1] = ExactBridgeDDSScoreForWorld(multiLeafState, 1);
    Check(multiLeafExact.values[0] == 1 && multiLeafExact.values[1] == 1,
      "multi-world bridge DDS leaf should return the expected exact one-trick values before any searched continuation");

    const ParetoFront multiLeafFront = SearchBridgeState(multiLeafState, 0);
    Check(multiLeafFront.vectors.size() == 1,
      "multi-world bridge DDS leaf evaluation should collapse to one exact vector before any searched continuation");
    Check(FrontContains(multiLeafFront, multiLeafExact),
      "multi-world bridge DDS leaf evaluation should match the direct DDS exact trick counts across surviving worlds");

    BridgeState splitState;
    splitState.playerToMove = SEAT_NORTH;
    splitState.maxSide = 0;
    splitState.trumpSuit = -1;
    splitState.trickLeader = SEAT_EAST;
    splitState.leadSuit = SUIT_HEARTS;
    splitState.possibleWorlds = WorldMask(2, 0x3ULL);
    splitState.currentTrick.push_back(BridgeMove(SUIT_HEARTS, 'K'));
    splitState.currentTrick.push_back(BridgeMove(SUIT_HEARTS, 'T'));
    splitState.currentTrick.push_back(BridgeMove(SUIT_HEARTS, 'J'));
    splitState.currentTrickPlayers.push_back(SEAT_EAST);
    splitState.currentTrickPlayers.push_back(SEAT_SOUTH);
    splitState.currentTrickPlayers.push_back(SEAT_WEST);
    splitState.worlds.push_back(ParsePBNWorld("N:K2.A.. J3... A4... Q5..."));
    splitState.worlds.push_back(ParsePBNWorld("N:A2.Q.. Q3... K4... J5..."));

    const vector<BridgeMove> splitMoves = GenerateBridgeMoves(splitState);
    Check(splitMoves.size() == 2,
      "multi-world bridge DDS continuation should expose exactly the two world-distinguishing heart plays");
    Check(splitMoves[0] == BridgeMove(SUIT_HEARTS, 'Q'),
      "multi-world bridge DDS continuation should include the world-1 heart completion");
    Check(splitMoves[1] == BridgeMove(SUIT_HEARTS, 'A'),
      "multi-world bridge DDS continuation should include the world-0 heart completion");

    const ParetoFront splitDirectLeaf = SearchBridgeState(splitState, 0);
    OutcomeVector splitLeaf(2);
    splitLeaf.valid = WorldMask(2, 0x3ULL);
    splitLeaf.values[0] = 3;
    splitLeaf.values[1] = 2;
    Check(splitDirectLeaf.vectors.size() == 1,
      "partial-trick bridge DDS leaf evaluation should collapse to one exact vector before any searched continuation");
    Check(FrontContains(splitDirectLeaf, splitLeaf),
      "partial-trick bridge DDS leaf evaluation should match the exact multi-world continuation trick counts across surviving worlds");

    const ParetoFront splitContinuation = SearchBridgeState(splitState, 2);
    OutcomeVector world0Only(2);
    world0Only.valid = WorldMask(2, 0x1ULL);
    world0Only.values[0] = 3;
    OutcomeVector world1Only(2);
    world1Only.valid = WorldMask(2, 0x2ULL);
    world1Only.values[1] = 2;

    Check(splitContinuation.vectors.size() == 2,
      "multi-world bridge DDS continuation should keep two sparse exact-score vectors after the searched continuation");
    Check(FrontContains(splitContinuation, world0Only),
      "multi-world bridge DDS continuation should preserve the world-0-only exact continuation [3 x]");
    Check(FrontContains(splitContinuation, world1Only),
      "multi-world bridge DDS continuation should preserve the world-1-only exact continuation [x 2]");

    BridgeState threeWorldState;
    threeWorldState.playerToMove = SEAT_NORTH;
    threeWorldState.maxSide = 0;
    threeWorldState.trumpSuit = -1;
    threeWorldState.trickLeader = SEAT_NORTH;
    threeWorldState.leadSuit = -1;
    threeWorldState.possibleWorlds = WorldMask(3, 0x7ULL);
    threeWorldState.worlds.push_back(ParsePBNWorld("N:2.K.. A.Q.. 3.2.. 4.3.."));
    threeWorldState.worlds.push_back(ParsePBNWorld("N:2.Q.. A.J.. 3.2.. 4.3.."));
    threeWorldState.worlds.push_back(ParsePBNWorld("N:2.J.. A.T.. 3.2.. 4.3.."));

    const vector<BridgeMove> threeWorldMoves = GenerateBridgeMoves(threeWorldState);
    Check(threeWorldMoves.size() == 4,
      "three-world bridge DDS continuation should expose one shared spade lead and three world-specific heart leads");

    const BridgeRootReport threeWorldReport = AnalyzeBridgeRoot(threeWorldState, 1);
    Check(threeWorldReport.children.size() == 4,
      "three-world bridge DDS continuation should report one root child per legal lead before DDS handoff");

    const ParetoFront threeWorldFront = SearchBridgeState(threeWorldState, 1);
    Check(threeWorldReport.rootFront.vectors.size() == threeWorldFront.vectors.size(),
      "three-world bridge DDS continuation root reporting should match the direct DDS-backed root front size");

    OutcomeVector mergedThreeWorld(3);
    mergedThreeWorld.valid = WorldMask(3, 0x7ULL);
    mergedThreeWorld.values[0] = 1;
    mergedThreeWorld.values[1] = 1;
    mergedThreeWorld.values[2] = 1;

    OutcomeVector threeWorld0Only(3);
    threeWorld0Only.valid = WorldMask(3, 0x1ULL);
    threeWorld0Only.values[0] = 1;

    OutcomeVector threeWorld1Only(3);
    threeWorld1Only.valid = WorldMask(3, 0x2ULL);
    threeWorld1Only.values[1] = 1;

    OutcomeVector threeWorld2Only(3);
    threeWorld2Only.valid = WorldMask(3, 0x4ULL);
    threeWorld2Only.values[2] = 1;

    Check(FrontContains(threeWorldFront, mergedThreeWorld),
      "three-world bridge DDS continuation should preserve the merged DDS-backed root front [1 1 1]");
    Check(FrontContains(threeWorldFront, threeWorld0Only),
      "three-world bridge DDS continuation should preserve the world-0-only DDS-backed root front [1 x x]");
    Check(FrontContains(threeWorldFront, threeWorld1Only),
      "three-world bridge DDS continuation should preserve the world-1-only DDS-backed root front [x 1 x]");
    Check(FrontContains(threeWorldFront, threeWorld2Only),
      "three-world bridge DDS continuation should preserve the world-2-only DDS-backed root front [x x 1]");

    bool sawSharedSpade = false;
    bool sawHeartK = false;
    bool sawHeartQ = false;
    bool sawHeartJ = false;
    for (unsigned i = 0; i < threeWorldReport.children.size(); i++)
    {
      const BridgeRootChildReport& child = threeWorldReport.children[i];
      if (child.move == BridgeMove(SUIT_SPADES, '2'))
      {
        sawSharedSpade = true;
        Check(child.validWorlds == WorldMask(3, 0x7ULL),
          "the shared spade lead should keep all three worlds valid before the DDS leaf handoff");
        Check(FrontContains(child.front, mergedThreeWorld),
          "the shared spade lead should produce the merged DDS-backed continuation front [1 1 1]");
      }
      else if (child.move == BridgeMove(SUIT_HEARTS, 'K'))
      {
        sawHeartK = true;
        Check(child.validWorlds == WorldMask(3, 0x1ULL),
          "the heart-K lead should isolate world 0 before the DDS leaf handoff");
        Check(FrontContains(child.front, threeWorld0Only),
          "the heart-K lead should produce the world-0-only DDS-backed continuation front [1 x x]");
      }
      else if (child.move == BridgeMove(SUIT_HEARTS, 'Q'))
      {
        sawHeartQ = true;
        Check(child.validWorlds == WorldMask(3, 0x2ULL),
          "the heart-Q lead should isolate world 1 before the DDS leaf handoff");
        Check(FrontContains(child.front, threeWorld1Only),
          "the heart-Q lead should produce the world-1-only DDS-backed continuation front [x 1 x]");
      }
      else if (child.move == BridgeMove(SUIT_HEARTS, 'J'))
      {
        sawHeartJ = true;
        Check(child.validWorlds == WorldMask(3, 0x4ULL),
          "the heart-J lead should isolate world 2 before the DDS leaf handoff");
        Check(FrontContains(child.front, threeWorld2Only),
          "the heart-J lead should produce the world-2-only DDS-backed continuation front [x x 1]");
      }
    }

    Check(sawSharedSpade && sawHeartK && sawHeartQ && sawHeartJ,
      "three-world bridge DDS continuation should cover the shared branch and all three split branches in root reporting");
  }


  static void TestEmptyEntryInteriorFronts()
  {
    ToyNode bestLeaf("bestLeaf", TOY_LEAF, 3);
    bestLeaf.leafFront = MakeFront(3, vector<string>(1, "110"));

    ToyNode partialA("partialA", TOY_LEAF, 3);
    partialA.leafFront = MakeFront(3, vector<string>(1, "010"));

    ToyNode partialB("partialB", TOY_LEAF, 3);
    partialB.leafFront = MakeFront(3, vector<string>(1, "110"));

    ToyNode shouldNotVisit("shouldNotVisit", TOY_LEAF, 3);
    shouldNotVisit.leafFront = MakeFront(3, vector<string>(1, "001"));

    ToyNode candidateMin("candidateMin", TOY_MIN, 3);
    AddChild(candidateMin, partialA, WorldMask(3, 0x6ULL));
    AddChild(candidateMin, partialB, WorldMask(3, 0x3ULL));
    AddChild(candidateMin, shouldNotVisit);

    ToyNode root("root", TOY_MAX, 3);
    AddChild(root, bestLeaf);
    AddChild(root, candidateMin);

    SearchStats stats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront front = SearchToy(root, 2, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), tt, true, -1.0, stats,
      rootCutTriggered, exactComplete);

    Check(stats.earlyCuts == 1,
      "empty-entry example should trigger one early cut after the interior front is completed");
    Check(find(stats.visitOrder.begin(), stats.visitOrder.end(),
      string("shouldNotVisit")) == stats.visitOrder.end(),
      "empty-entry example should cut before visiting the remaining Min child");
    Check(FrontContains(front, MakeBinaryOutcome("110")),
      "empty-entry example should preserve the dominating root outcome");
    Check(MakeBinaryOutcome("x10").ToString() == "[x 1 0]",
      "empty-entry parsing should support sparse vectors");

    const ParetoFront sparseA = partialA.leafFront.RestrictToUseful(
      WorldMask(3, 0x6ULL));
    const ParetoFront sparseB = partialB.leafFront.RestrictToUseful(
      WorldMask(3, 0x3ULL));
    const ParetoFront combined = ParetoFront::MinProduct(sparseA, sparseB);
    Check(FrontContains(combined, MakeBinaryOutcome("110")),
      "empty-entry example should combine sparse child fronts into [1 1 0]");
    Check(FrontContains(partialA.leafFront.RestrictToUseful(WorldMask(3, 0x6ULL)),
      MakeBinaryOutcome("x10")),
      "restricting to child worlds should produce an empty entry in the skipped world");
  }


  static void TestOptimisticImpossibleWorlds()
  {
    ToyNode rootBest("rootBest", TOY_LEAF, 3);
    rootBest.leafFront = MakeFront(3, vector<string>(1, "110"));

    ToyNode impossibleReply("impossibleReply", TOY_LEAF, 3);
    impossibleReply.leafFront = MakeFront(3, vector<string>(1, "000"));

    ToyNode shouldNotVisit("shouldNotVisit", TOY_LEAF, 3);
    shouldNotVisit.leafFront = MakeFront(3, vector<string>(1, "111"));

    ToyNode candidateMin("candidateMin", TOY_MIN, 3);
    candidateMin.optimisticValues = MakeBinaryOutcome("011");
    AddChild(candidateMin, impossibleReply, WorldMask(3, 0x4ULL));
    AddChild(candidateMin, shouldNotVisit);

    ToyNode root("root", TOY_MAX, 3);
    AddChild(root, rootBest);
    AddChild(root, candidateMin);

    SearchStats stats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront front = SearchToy(root, 2, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), tt, true, -1.0, stats,
      rootCutTriggered, exactComplete);

    const ParetoFront sparseImpossible = impossibleReply.leafFront.RestrictToUseful(
      WorldMask(3, 0x4ULL));
    Check(FrontContains(sparseImpossible, MakeBinaryOutcome("xx0")),
      "optimistic example should first produce a sparse impossible-world vector [x x 0]");

    const ParetoFront optimisticImpossible = sparseImpossible.CompleteOptimistically(
      WorldMask::All(3), candidateMin.optimisticValues);
    Check(FrontContains(optimisticImpossible, MakeBinaryOutcome("010")),
      "optimistic example should complete [x x 0] to [0 1 0] using the closest known world values");

    Check(stats.optimisticCompletions >= 1,
      "optimistic example should record at least one optimistic completion");
    Check(stats.earlyCuts == 1,
      "optimistic example should trigger an early cut once the impossible world is completed optimistically");
    Check(find(stats.visitOrder.begin(), stats.visitOrder.end(),
      string("shouldNotVisit")) == stats.visitOrder.end(),
      "optimistic example should cut before visiting the remaining Min child");
    Check(FrontContains(front, MakeBinaryOutcome("110")),
      "optimistic example should preserve the dominating root outcome");
    Check(! rootCutTriggered,
      "optimistic example should not be reported as a root cut");
  }


  static void TestDeepAlphaCut()
  {
    ToyNode rootBest("rootBest", TOY_LEAF, 3);
    rootBest.leafFront = MakeFront(3, vector<string>(1, "110"));

    ToyNode innerBest("innerBest", TOY_LEAF, 3);
    innerBest.leafFront = MakeFront(3, vector<string>(1, "001"));

    ToyNode candidateFirst("candidateFirst", TOY_LEAF, 3);
    candidateFirst.leafFront = MakeFront(3, vector<string>(1, "110"));

    ToyNode skippedByDeepAlpha("skippedByDeepAlpha", TOY_LEAF, 3);
    skippedByDeepAlpha.leafFront = MakeFront(3, vector<string>(1, "111"));

    ToyNode deepMin("deepMin", TOY_MIN, 3);
    AddChild(deepMin, candidateFirst);
    AddChild(deepMin, skippedByDeepAlpha);

    ToyNode innerMax("innerMax", TOY_MAX, 3);
    AddChild(innerMax, innerBest);
    AddChild(innerMax, deepMin);

    ToyNode outerMin("outerMin", TOY_MIN, 3);
    AddChild(outerMin, innerMax);

    ToyNode root("root", TOY_MAX, 3);
    AddChild(root, rootBest);
    AddChild(root, outerMin);

    SearchStats stats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront front = SearchToy(root, 3, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), tt, true, -1.0, stats,
      rootCutTriggered, exactComplete);

    Check(stats.deepAlphaCuts == 1,
      "deep-alpha example should trigger exactly one deep alpha cut");
    Check(stats.earlyCuts == 0,
      "deep-alpha example should cut via an ancestor Max front rather than the immediate one");
    Check(find(stats.visitOrder.begin(), stats.visitOrder.end(),
      string("skippedByDeepAlpha")) == stats.visitOrder.end(),
      "deep-alpha cut should stop before searching the remaining deep Min child");
    Check(FrontContains(front, MakeBinaryOutcome("110")),
      "deep-alpha example should preserve the dominating root outcome");
    Check(! rootCutTriggered,
      "deep-alpha example should not be reported as a root cut");
  }


  static void TestCutOnWin()
  {
    ToyNode winningMove("winningMove", TOY_LEAF, 3);
    winningMove.leafFront = MakeFront(3, vector<string>(1, "111"));

    ToyNode skippedSibling("skippedSibling", TOY_LEAF, 3);
    skippedSibling.leafFront = MakeFront(3, vector<string>(1, "001"));

    ToyNode root("root", TOY_MAX, 3);
    AddChild(root, winningMove);
    AddChild(root, skippedSibling);

    SearchStats stats;
    bool rootCutTriggered = false;
    bool exactComplete = false;
    TranspositionTable tt;
    const ParetoFront front = SearchToy(root, 1, WorldMask::All(3),
      vector<const ParetoFront *>(), OutcomeVector(3), tt, true, -1.0, stats,
      rootCutTriggered, exactComplete);

    Check(stats.cutOnWinCuts == 1,
      "cut-on-win example should trigger exactly one cut on win");
    Check(find(stats.visitOrder.begin(), stats.visitOrder.end(),
      string("skippedSibling")) == stats.visitOrder.end(),
      "cut on win should stop before searching the remaining sibling move");
    Check(FrontContains(front, MakeBinaryOutcome("111")),
      "cut-on-win example should keep the fully winning move");
    Check(! rootCutTriggered,
      "cut-on-win example should not be reported as a root cut");
  }


  static void TestDDSLeafDemo()
  {
    SetMaxThreads(0);

    HandFileData data;
    LoadHandFile(kAlphaMuPlayHandFile, data);

    Check(data.number == 3,
      "alpha_mu_play.txt should provide three DDS worlds for the leaf demo");

    const int target = 4;
    const DDSLeafEvalResult serial = EvaluateDDSLeafThresholdSerial(data, target);
    const DDSLeafEvalResult parallel = EvaluateDDSLeafThresholdParallel(
      data,
      target,
      data.number);

    Check(serial.leaf.ToString() == "[1 1 0]",
      "DDS leaf demo should yield the expected threshold vector [1 1 0]");
    Check(parallel.leaf.ToString() == "[1 1 0]",
      "parallel DDS leaf demo should yield the expected threshold vector [1 1 0]");
    Check(serial.leaf.ToString() == parallel.leaf.ToString(),
      "serial and parallel DDS leaf evaluation should return the same threshold vector");
    Check(serial.bestScores == parallel.bestScores,
      "serial and parallel DDS leaf evaluation should return the same world scores");
    Check(parallel.workerCount >= 1,
      "parallel DDS leaf evaluation should configure at least one worker");
    Check(parallel.workerCount <= data.number,
      "parallel DDS leaf evaluation should not configure more worker slots than worlds");
  }


  static void TestBridgeSearchExplicitExecutionContext()
  {
    SetMaxThreads(0);

    HandFileData data;
    LoadHandFile(kAlphaMuPlayHandFile, data);
    Check(data.number >= 1,
      "explicit execution-context parity test requires at least one bridge DDS fixture");

    const BridgeState state = MakeBridgeStateFromDDSDeal(data.dealList[0]);
    const SearchExecutionContext context = MakeSearchExecutionContext(0, NULL,
      ALPHA_MU_PARALLEL_SERIAL, 1, 1);

    Check(! context.bridgeSearch.enableAncestorCuts,
      "explicit execution context should default Stage 1 ancestor-cut scaffolding to disabled so reporting paths stay exact until they opt in");
    Check(! context.bridgeSearch.hasUsefulWorlds,
      "explicit execution context should default Stage 1 useful-world scaffolding to disabled");
    Check(context.bridgeSearch.upperMaxFronts.empty(),
      "explicit execution context should default Stage 1 ancestor-front scaffolding to an empty set");
    Check(! context.bridgeSearch.hasOptimisticValues,
      "explicit execution context should default Stage 1 optimistic-value scaffolding to disabled");
    Check(! context.bridgeSearch.requireExactTTFronts,
      "explicit execution context should default Stage 1 exact-TT scaffolding to the current permissive bridge-search mode");

    const ParetoFront defaultLeafFront = SearchBridgeState(state, 0);
    const ParetoFront explicitLeafFront = SearchBridgeState(state, 0, context);
    Check(defaultLeafFront.ToString() == explicitLeafFront.ToString(),
      "explicit serial execution context should preserve the direct bridge DDS leaf front");

    const ParetoFront defaultFront = SearchBridgeState(state, 1);
    const ParetoFront explicitFront = SearchBridgeState(state, 1, context);
    Check(defaultFront.ToString() == explicitFront.ToString(),
      "explicit serial execution context should preserve bridge search fronts");

    Check(ExactBridgeDDSScoreForWorld(state, 0) ==
          ExactBridgeDDSScoreForWorld(state, 0, context),
      "explicit serial execution context should preserve exact bridge DDS leaf scores");
  }


  static void TestAlphaMuBenchmarkOptionNormalization()
  {
    AlphaMuBenchmarkOptions options;
    options.handFile = "../hands/list1.txt";
    options.depth = 2;
    options.maxBoards = 3;
    options.skipSpec = "2";
    options.parallelMode = ALPHA_MU_PARALLEL_ROOT;
    options.boardWorkers = 0;
    options.rootWorkers = -7;
    options.ddsThreadId = -3;

    const AlphaMuBenchmarkOptions normalized =
      NormalizeAlphaMuBenchmarkOptions(options);
    Check(normalized.handFile == options.handFile &&
          normalized.depth == options.depth &&
          normalized.maxBoards == options.maxBoards &&
          normalized.skipSpec == options.skipSpec,
      "benchmark option normalization should preserve workload-selection fields");
    Check(normalized.parallelMode == ALPHA_MU_PARALLEL_ROOT,
      "benchmark option normalization should preserve the requested parallel mode");
    Check(normalized.boardWorkers == 1,
      "benchmark option normalization should clamp board workers to at least one");
    Check(normalized.rootWorkers == 1,
      "benchmark option normalization should clamp root workers to at least one");
    Check(normalized.ddsThreadId == 0,
      "benchmark option normalization should clamp DDS thread slots to a non-negative id");

    Check(AlphaMuParallelModeName(ALPHA_MU_PARALLEL_SERIAL) == "serial" &&
          AlphaMuParallelModeName(ALPHA_MU_PARALLEL_BOARD) == "board" &&
          AlphaMuParallelModeName(ALPHA_MU_PARALLEL_ROOT) == "root",
      "parallel-mode names should render in command-line-friendly text");
    Check(ParseAlphaMuParallelModeName("serial") == ALPHA_MU_PARALLEL_SERIAL &&
          ParseAlphaMuParallelModeName("board") == ALPHA_MU_PARALLEL_BOARD &&
          ParseAlphaMuParallelModeName("root") == ALPHA_MU_PARALLEL_ROOT,
      "parallel-mode parsing should accept the supported serial, board, and root names");
  }


  static void TestAlphaMuBenchmarkOptionsOverloadParity()
  {
    SetMaxThreads(0);

    AlphaMuBenchmarkOptions options;
    options.handFile = "../hands/list1.txt";
    options.depth = 0;
    options.maxBoards = 1;

    const BenchmarkMethodSummary legacy = BenchmarkAlphaMuExactBoards(
      options.handFile, options.depth, options.maxBoards, options.skipSpec);
    const BenchmarkMethodSummary explicitSummary =
      BenchmarkAlphaMuExactBoards(options);

    Check(legacy.method == explicitSummary.method &&
          legacy.handFile == explicitSummary.handFile &&
          legacy.boardsTested == explicitSummary.boardsTested &&
          legacy.depth == explicitSummary.depth &&
          legacy.parallelMode == explicitSummary.parallelMode &&
          legacy.boardWorkers == explicitSummary.boardWorkers &&
          legacy.rootWorkers == explicitSummary.rootWorkers &&
          legacy.ddsThreadId == explicitSummary.ddsThreadId &&
          legacy.configuredBoardWorkers == explicitSummary.configuredBoardWorkers &&
          legacy.mismatches == explicitSummary.mismatches,
      "benchmark options overload should preserve the legacy benchmark summary fields and serial execution metadata");
    Check(legacy.perBoardSeconds.size() == explicitSummary.perBoardSeconds.size(),
      "benchmark options overload should preserve the number of measured per-board timings");
    Check(legacy.mismatches == 0 && explicitSummary.mismatches == 0,
      "benchmark options overload should preserve exact alpha-mu benchmark scores");
  }


  static void TestBoardParallelBenchmarkParity()
  {
    SetMaxThreads(0);

    AlphaMuBenchmarkOptions serialOptions;
    serialOptions.handFile = "../hands/list2.txt";
    serialOptions.depth = 0;
    serialOptions.maxBoards = 2;

    AlphaMuBenchmarkOptions parallelOptions(serialOptions);
    parallelOptions.parallelMode = ALPHA_MU_PARALLEL_BOARD;
    parallelOptions.boardWorkers = 2;

    const BenchmarkMethodSummary serialSummary =
      BenchmarkAlphaMuExactBoards(serialOptions);
    const BenchmarkMethodSummary parallelSummary =
      BenchmarkAlphaMuExactBoards(parallelOptions);

    Check(serialSummary.method == parallelSummary.method &&
          serialSummary.handFile == parallelSummary.handFile &&
          serialSummary.boardsTested == parallelSummary.boardsTested &&
          serialSummary.depth == parallelSummary.depth,
      "board-parallel benchmark mode should preserve the benchmark identity fields from the serial path");
    Check(serialSummary.parallelMode == ALPHA_MU_PARALLEL_SERIAL &&
          serialSummary.boardWorkers == 1 &&
          serialSummary.rootWorkers == 1 &&
          serialSummary.configuredBoardWorkers == 1,
      "serial benchmark summaries should report serial execution metadata");
    Check(parallelSummary.parallelMode == ALPHA_MU_PARALLEL_BOARD &&
          parallelSummary.boardWorkers == 2 &&
          parallelSummary.rootWorkers == 1 &&
          parallelSummary.ddsThreadId >= 0 &&
          parallelSummary.configuredBoardWorkers >= 1 &&
          parallelSummary.configuredBoardWorkers <= 2,
      "board-parallel benchmark summaries should report the requested mode and the configured worker count");
    Check(serialSummary.boardsTested == 2 &&
          parallelSummary.boardsTested == 2,
      "board-parallel benchmark parity test should exercise a tiny two-board workload");
    Check(serialSummary.mismatches == 0 && parallelSummary.mismatches == 0,
      "board-parallel benchmark mode should preserve exact alpha-mu scores on every tested board");
    Check(serialSummary.perBoardSeconds.size() == 2 &&
          parallelSummary.perBoardSeconds.size() == 2,
      "board-parallel benchmark mode should still report one per-board timing per selected board");
  }


  static void TestRepeatedDDSReinitializationKeepsThreadContext()
  {
    HandFileData data;
    LoadHandFile(kAlphaMuPlayHandFile, data);
    Check(data.number >= 1,
      "repeated DDS reinitialization test requires at least one bridge DDS fixture");

    const BridgeState state = MakeBridgeStateFromDDSDeal(data.dealList[0]);
    for (int i = 0; i < 16; i++)
    {
      SetMaxThreads(0);
      const SearchExecutionContext context = MakeSearchExecutionContext(0, NULL,
        ALPHA_MU_PARALLEL_SERIAL, 1, 1);
      const ParetoFront front = SearchBridgeState(state, 0, context);
      Check(! front.vectors.empty(),
        "repeated DDS reinitialization should leave a valid bridge DDS leaf front available");
      const int exactScore = ExactBridgeDDSScoreForWorld(state, 0, context);
      Check(exactScore >= 0,
        "repeated DDS reinitialization should keep an exact bridge DDS score available on thread slot zero");
    }
  }

  namespace
  {
    struct NamedTest
    {
      const char * successMessage;
      void (*run)();
    };
  }

  /**
   * @brief Stage 1 test: build multi-world BridgeState from partial information.
   *
   * Uses a known deal and the first trick of play to construct a partial-
   * information world set from the declaring side's perspective.  Verifies:
   *  - Multiple candidate worlds are generated (not just one)
   *  - Play history evidence narrows the candidate count
   *  - The true defenders' hands are consistent with at least one generated world
   */
  /**
   * Regression: play.number exceeding the actual card-pair count in the
   * play string previously caused ParsePBNPlayHistory to silently return
   * fewer events than requested, leading to downstream failures.
   */
  void TestParsePlayHistoryValidation()
  {
    // Correct case: 4 cards, 8-char string
    {
      playTracePBN play;
      memset(&play, 0, sizeof(play));
      play.number = 4;
      strcpy(play.cards, "CTC4CACJ");
      const vector<PlayHistoryEvent> history =
        ParsePBNPlayHistory(play, SEAT_NORTH, 0);
      Check(history.size() == 4,
        "should parse exactly 4 events from a 4-card string");
    }

    // Regression: play.number larger than available card pairs must fail
    {
      playTracePBN play;
      memset(&play, 0, sizeof(play));
      play.number = 8;
      strcpy(play.cards, "CTC4CACJ");  // only 4 card pairs (8 chars)
      bool caught = false;
      try
      {
        ParsePBNPlayHistory(play, SEAT_NORTH, 0);
      }
      catch (...)
      {
        caught = true;
      }
      Check(caught,
        "ParsePBNPlayHistory should reject play.number exceeding "
        "available card pairs in the string");
    }

    // Empty / zero cases should return empty without error
    {
      playTracePBN play;
      memset(&play, 0, sizeof(play));
      play.number = 0;
      const vector<PlayHistoryEvent> history =
        ParsePBNPlayHistory(play, SEAT_NORTH, 0);
      Check(history.empty(),
        "zero play.number should produce empty history");
    }
  }

  void TestPartialInformationWorldGeneration()
  {
    // Use a deal late in the play to keep the hidden-card count small.
    // Board 1 from alpha_mu_play.txt:
    // PBN 0 0 0 0 "N:QJ6.K652.J85.T98 873.J97.AT764.Q4 K5.T83.KQ9.A7652 AT942.AQ4.32.KJ3"
    // PLAY 45 "CTC4CACJH8H4HKH9D5DAD9D2S7S5S2SQD8D4DQD3H3HAH6H7C3C8CQC2S3SKSAS6HQH5HJHTCKC9D6C5S4SJS8C6DJ"
    // Trump=0 (spades), opening leader=0 (N)
    //
    // We take South as declarer (dummy=North). After 9 tricks (36 cards),
    // each hand has 4 cards left. The hidden hands (E, W) have 8 cards total,
    // giving C(8,4)=70 raw assignments — manageable.

    dealPBN deal;
    memset(&deal, 0, sizeof(deal));
    deal.trump = 0;  // spades
    deal.first = 0;  // North leads
    strcpy(deal.remainCards,
      "N:QJ6.K652.J85.T98 873.J97.AT764.Q4 K5.T83.KQ9.A7652 AT942.AQ4.32.KJ3");

    const int declarerSeat = SEAT_SOUTH;
    const int trumpSuit = 0;

    // Parse first 40 cards (10 complete tricks) — leaves 3 cards per hand,
    // 6 hidden cards total, C(6,3)=20 raw assignments.
    playTracePBN play;
    memset(&play, 0, sizeof(play));
    play.number = 40;
    strcpy(play.cards,
      "CTC4CACJH8H4HKH9D5DAD9D2S7S5S2SQD8D4DQD3H3HAH6H7C3C8CQC2S3SKSAS6HQH5HJHTCKC9D6C5");

    const vector<PlayHistoryEvent> history =
      ParsePBNPlayHistory(play, deal.first, trumpSuit);

    Check(history.size() == 40,
      "should parse 40 play events from ten tricks");

    // Build the partial-information state after 10 tricks
    const BridgeInformationState information = BuildInformationStateFromPlay(
      deal, declarerSeat, history, 50);
    WorldGenerationStats worldStats;
    DecisionWorldPipelineResult pipeline;
    const BridgeState state = MakeBridgeStateFromInformationState(
      deal, declarerSeat, history, information, &worldStats, &pipeline);

    Check(worldStats.candidateWorldCount == 20,
      "partial-information state should begin from 20 raw candidate worlds (C(6,3)) before the shared Stage 2 pipeline filters them");
    Check(pipeline.candidateWorlds.size() == 20,
      "partial-information state should preserve the 20 raw candidate worlds in the shared decision pipeline even after compaction to the active search set");

    const unsigned worldCount = state.possibleWorlds.PopCount();
    Check(worldCount > 0,
      "at least one world should survive follow-suit filtering");
    Check(worldCount <= 20,
      "follow-suit filtering should not add worlds");
    Check(worldCount <= 50,
      "world count should respect the sample limit");
    Check(state.worlds.size() == worldStats.finalWorldCount,
      "the compacted bridge state should contain exactly the worlds surviving the shared Stage 2 pipeline");

    // Check that surviving worlds have the right number of cards per hidden defender
    // After 10 tricks, each hand has 3 cards left.
    bool foundValidWorld = false;
    for (unsigned i = 0; i < state.worlds.size(); i++)
    {
      if (! ((state.possibleWorlds.bits >> i) & 1ULL))
        continue;

      unsigned eastCards = 0;
      unsigned westCards = 0;
      for (int s = 0; s < 4; s++)
      {
        eastCards += static_cast<unsigned>(state.worlds[i].suits[SEAT_EAST][s].size());
        westCards += static_cast<unsigned>(state.worlds[i].suits[SEAT_WEST][s].size());
      }
      if (eastCards == 3 && westCards == 3)
      {
        foundValidWorld = true;
        break;
      }
    }

    Check(foundValidWorld,
      "at least one surviving world should have 3 cards per hidden defender");
  }


  static void TestBuildWorldSpecTracksUnevenSeatCounts()
  {
    dealPBN deal;
    memset(&deal, 0, sizeof(deal));
    deal.trump = 0;
    deal.first = 0;
    strcpy(deal.remainCards,
      "N:QJ6.K652.J85.T98 873.J97.AT764.Q4 K5.T83.KQ9.A7652 AT942.AQ4.32.KJ3");

    playTracePBN play;
    memset(&play, 0, sizeof(play));
    play.number = 34;
    strcpy(play.cards,
      "CTC4CACJH8H4HKH9D5DAD9D2S7S5S2SQD8D4DQD3H3HAH6H7C3C8CQC2S3SKSAS6HQH5");

    const vector<PlayHistoryEvent> history =
      ParsePBNPlayHistory(play, deal.first, deal.trump);
    Check(history.size() == 34,
      "uneven-prefix fixture should parse 34 play events");

    int playedBySeat[4] = {0, 0, 0, 0};
    for (unsigned i = 0; i < history.size(); i++)
      playedBySeat[history[i].player]++;

    const HistoryDerivedWorldSpec spec =
      BuildWorldSpecFromDeal(deal, SEAT_SOUTH, history);

    Check(spec.hiddenSeats.size() == 2,
      "seed-world construction should track both hidden defenders");
    for (int seat = 0; seat < 4; seat++)
    {
      ostringstream msg;
      msg << "seed-world construction should leave "
          << SeatName(seat)
          << " with exactly 13 minus that seat's parsed played-card count";
      Check(WorldSeatCardCount(spec.seedWorld, seat) ==
            static_cast<unsigned>(13 - playedBySeat[seat]),
        msg.str());
    }
    Check(WorldCardCount(spec.seedWorld) == 18,
      "seed-world construction should preserve the 18 remaining unseen cards after 34 cards of play");
  }


  static void TestPartialInformationUnevenCurrentTrickCounts()
  {
    dealPBN deal;
    memset(&deal, 0, sizeof(deal));
    deal.trump = 0;
    deal.first = 0;
    strcpy(deal.remainCards,
      "N:QJ6.K652.J85.T98 873.J97.AT764.Q4 K5.T83.KQ9.A7652 AT942.AQ4.32.KJ3");

    playTracePBN play;
    memset(&play, 0, sizeof(play));
    play.number = 34;
    strcpy(play.cards,
      "CTC4CACJH8H4HKH9D5DAD9D2S7S5S2SQD8D4DQD3H3HAH6H7C3C8CQC2S3SKSAS6HQH5");

    const vector<PlayHistoryEvent> history =
      ParsePBNPlayHistory(play, deal.first, deal.trump);
    const BridgeState state = MakeBridgeStateFromPartialInformation(
      deal, SEAT_SOUTH, history, 50);
    int playedBySeat[4] = {0, 0, 0, 0};
    for (unsigned i = 0; i < history.size(); i++)
      playedBySeat[history[i].player]++;

    Check(state.currentTrick.size() == 2,
      "partial-information state should preserve the two cards already played into the current trick");
    Check(state.trickLeader == history[32].player,
      "partial-information state should preserve the parsed leader of the current unfinished trick");
    Check(state.playerToMove == (state.trickLeader + 2) % 4,
      "partial-information state should advance the player-to-move by the two cards already played into the current trick");
    Check(! state.worlds.empty(),
      "partial-information state should construct candidate worlds for the uneven current-trick fixture");
    Check(state.possibleWorlds.PopCount() > 0,
      "partial-information state should keep at least one surviving world for the uneven current-trick fixture");

    for (unsigned i = 0; i < state.worlds.size(); i++)
    {
      for (int seat = 0; seat < 4; seat++)
      {
        ostringstream msg;
        msg << "every constructed world should leave "
            << SeatName(seat)
            << " with exactly 13 minus that seat's parsed played-card count in the uneven current-trick fixture";
        Check(WorldSeatCardCount(state.worlds[i], seat) ==
              static_cast<unsigned>(13 - playedBySeat[seat]),
          msg.str());
      }
      Check(WorldCardCount(state.worlds[i]) == 18,
        "every constructed world should preserve the 18 cards still held in hand during the uneven current trick");
    }

    const int tricksRemaining = static_cast<int>(
      (WorldCardCount(state.worlds[0]) + state.currentTrick.size()) / 4U);
    Check(tricksRemaining == 5,
      "the uneven current-trick fixture should still have 5 full tricks remaining once the two current-trick cards are counted back in");
  }

  /**
   * Test that follow-suit evidence from the play history reduces the number
   * of candidate worlds.  Uses a deal where a defender shows out early.
   *
   * Deal (notrump, N leads):
   *   N: AKQ.AKQ.AKQ.AKQJ  (13 cards)
   *   E: JT9.JT9.JT9.T987  (13 cards)
   *   S: 876.876.876.6543   (13 cards)
   *   W: 5432.5432.5432.2   (13 cards — only 1 club)
   *
   * Play 12 cards (3 tricks):
   *   Trick 1: N leads CA, E plays C7, S plays C3, W plays C2 (all follow)
   *   Trick 2: N leads CK, E plays C8, S plays C4, W plays S2 (W shows out of clubs!)
   *   Trick 3: N leads CQ, E plays C9, S plays C5, W plays S3 (W discards again)
   *
   * After 3 tricks (12 cards played), each hand has 10 cards left.
   * Declarer = South, dummy = North. Hidden = East, West.
   * East has 10 remaining, West has 10 remaining → 20 hidden cards.
   * C(20,10)=184756 raw assignments.
   *
   * But we cap construction and the key point is:
   * W showed out of clubs at trick 2, so W has 0 remaining clubs.
   * Any world giving W clubs must be rejected.
   *
   * This is too large for uncapped enumeration so we use a smaller position.
   * Let's use 11 tricks (44 cards) instead.
   */
  void TestFollowSuitNarrowingInPartialInformation()
  {
    // Synthetic deal designed so West has only 1 club and shows out on trick 2.
    //
    // Deal (notrump=4, N leads):
    //   N: AK.AK.AK.AKQJT98
    //   E: QJ.QJ.QJ.7654     (4 clubs)
    //   S: T9.T9.T9.32       (2 clubs)
    //   W: 8765.8765.8765.   (0 clubs!)
    //
    // Play 8 cards (2 tricks):
    //   Trick 1: N leads CA, E plays C4, S plays C2, W plays S5 (W shows out!)
    //   Trick 2: N leads CK, E plays C5, S plays C3, W plays S6 (W discards again)
    //
    // After 2 tricks, each hand has 11 remaining cards.
    // Hidden = E(11) + W(11) = 22 cards.
    // This is too many for full enumeration, but the constructor will
    // use constraints to prune. Since the only constraints are from
    // visible hands (known-card) plus the follow-suit void, let's
    // use a smaller deal.
    //
    // Better: use a 4-card-per-hand mini-deal.
    //
    // Deal (notrump=4, N leads):
    //   N: A...AKQJ  →  S:A, H:-, D:-, C:AKQJ
    //   E: ...T987   →  S:-, H:-, D:-, C:T987
    //   S: K...6543  →  S:K, H:-, D:-, C:6543
    //   W: QJ..T9.   →  S:QJ, H:-, D:T9, C:-
    //
    // Wait, this is getting complicated with PBN format. Let me use
    // a simpler approach: directly test the filter function by building
    // worlds and play history manually.

    // Approach: construct a world set and play history where
    // we know the filter should reject some worlds.
    // After 2 tricks where West shows out of clubs,
    // any world giving West remaining clubs is invalid.

    // Construct worlds directly
    vector<ParsedWorld> worlds;

    // World 0: East has remaining clubs, West has none (valid)
    ParsedWorld w0;
    w0.suits[SEAT_EAST][SUIT_CLUBS] = "T9";
    w0.suits[SEAT_WEST][SUIT_DIAMONDS] = "T9";
    worlds.push_back(w0);

    // World 1: West has remaining clubs, East has none (invalid — West is void)
    ParsedWorld w1;
    w1.suits[SEAT_WEST][SUIT_CLUBS] = "T9";
    w1.suits[SEAT_EAST][SUIT_DIAMONDS] = "T9";
    worlds.push_back(w1);

    // World 2: Split clubs (invalid — West gets a club)
    ParsedWorld w2;
    w2.suits[SEAT_EAST][SUIT_CLUBS] = "T";
    w2.suits[SEAT_WEST][SUIT_CLUBS] = "9";
    w2.suits[SEAT_EAST][SUIT_DIAMONDS] = "9";
    w2.suits[SEAT_WEST][SUIT_DIAMONDS] = "T";
    worlds.push_back(w2);

    // Play history: 2 tricks, West shows out of clubs on trick 1
    vector<PlayHistoryEvent> playedCards;
    // Trick 1: N leads clubs, W plays a spade (shows out)
    playedCards.push_back(PlayHistoryEvent(SEAT_NORTH, SUIT_CLUBS,
      BridgeMove(SUIT_CLUBS, 'A')));
    playedCards.push_back(PlayHistoryEvent(SEAT_EAST, SUIT_CLUBS,
      BridgeMove(SUIT_CLUBS, '8')));
    playedCards.push_back(PlayHistoryEvent(SEAT_SOUTH, SUIT_CLUBS,
      BridgeMove(SUIT_CLUBS, '6')));
    playedCards.push_back(PlayHistoryEvent(SEAT_WEST, SUIT_CLUBS,
      BridgeMove(SUIT_SPADES, '5')));  // shows out!

    // Build the void set from play history
    bool voidSuits[4][4];
    memset(voidSuits, 0, sizeof(voidSuits));
    for (unsigned i = 0; i < playedCards.size(); i++)
    {
      const PlayHistoryEvent& ev = playedCards[i];
      if (ev.leadSuit >= 0 && ev.move.suit != ev.leadSuit)
        voidSuits[ev.player][ev.leadSuit] = true;
    }

    Check(voidSuits[SEAT_WEST][SUIT_CLUBS],
      "West should be void in clubs after the play history");

    // Apply the filter
    vector<int> hiddenSeats;
    hiddenSeats.push_back(SEAT_EAST);
    hiddenSeats.push_back(SEAT_WEST);

    unsigned validCount = 0;
    for (unsigned w = 0; w < worlds.size(); w++)
    {
      bool valid = true;
      for (unsigned h = 0; h < hiddenSeats.size() && valid; h++)
      {
        const int seat = hiddenSeats[h];
        for (int s = 0; s < 4 && valid; s++)
        {
          if (voidSuits[seat][s] && ! worlds[w].suits[seat][s].empty())
            valid = false;
        }
      }
      if (valid)
        validCount++;
    }

    Check(validCount == 1,
      "follow-suit filtering should keep only the world where West has no clubs");
  }

  /**
   * End-to-end test: SolveAlphaMu on alpha_mu_play.txt board 1
   * with a 10-trick play prefix at depth 1.
   */
  void TestEndToEndSolveAlphaMu()
  {
    // Board 1 from alpha_mu_play.txt
    dealPBN deal;
    memset(&deal, 0, sizeof(deal));
    deal.trump = 0;  // spades
    deal.first = 0;  // North leads
    strcpy(deal.remainCards,
      "N:QJ6.K652.J85.T98 873.J97.AT764.Q4 K5.T83.KQ9.A7652 AT942.AQ4.32.KJ3");

    const int declarerSeat = SEAT_SOUTH;

    // 10-trick play prefix (40 cards)
    playTracePBN play;
    memset(&play, 0, sizeof(play));
    play.number = 40;
    strcpy(play.cards,
      "CTC4CACJH8H4HKH9D5DAD9D2S7S5S2SQD8D4DQD3H3HAH6H7C3C8CQC2S3SKSAS6HQH5HJHTCKC9D6C5");

    const AlphaMuSolveResult result = SolveAlphaMu(
      deal, declarerSeat, play, 1, 50);

    Check(result.valid,
      "end-to-end solve should produce a valid result");
    Check(result.worldCount > 0,
      "end-to-end solve should generate at least one world");
    Check(result.survivingWorldCount > 0,
      "end-to-end solve should have at least one surviving world");
    Check(result.depthSearched == 1,
      "end-to-end solve should search to the requested depth");
    Check(result.rootFront.vectors.size() > 0,
      "end-to-end solve should produce a non-empty root front");
    Check(result.chosenMove.suit >= 0 && result.chosenMove.suit <= 3,
      "end-to-end solve should choose a valid suit");
    Check(result.totalSeconds > 0.0,
      "end-to-end solve should record positive elapsed time");
    Check(result.totalSeconds < 30.0,
      "end-to-end solve at depth 1 should complete within 30 seconds");
    Check(result.searchSeconds >= 0.0,
      "end-to-end solve should record non-negative search time");
    Check(result.ddsLeafSeconds >= 0.0 &&
          result.ddsLeafSeconds <= result.searchSeconds,
      "end-to-end solve should keep DDS leaf time within the total search time");
    Check(result.worldGenerationSeconds >= 0.0,
      "end-to-end solve should record non-negative world-generation time");

    // The root report should have at least one child move
    Check(result.rootReport.children.size() > 0,
      "end-to-end solve should report at least one candidate move");
  }

  static void TestDecisionPointComparisonReporting()
  {
    dealPBN deal;
    memset(&deal, 0, sizeof(deal));
    deal.trump = 0;
    deal.first = 0;
    strcpy(deal.remainCards,
      "N:QJ6.K652.J85.T98 873.J97.AT764.Q4 K5.T83.KQ9.A7652 AT942.AQ4.32.KJ3");

    playTracePBN play;
    memset(&play, 0, sizeof(play));
    play.number = 45;
    strcpy(play.cards,
      "CTC4CACJH8H4HKH9D5DAD9D2S7S5S2SQD8D4DQD3H3HAH6H7C3C8CQC2S3SKSAS6HQH5HJHTCKC9D6C5S4SJS8C6DJ");

    const int declarerSeat = (deal.first + 3) % 4;
    const AlphaMuSolveResult result = SolveAlphaMu(
      deal, declarerSeat, play, 1, 50, 0.0, 40, 7U);

    Check(result.valid,
      "decision-point solve should produce a valid result on the truncated real-board prefix");
    Check(result.prefixPlayLength == 40 && result.fullPlayLength == 45,
      "decision-point solve should preserve both the requested prefix length and the full play length");
    Check(result.playerToMove == SEAT_WEST && result.decisionOnDeclarerSide,
      "decision-point solve should stop on a declarer-side turn for the board-1 ten-trick prefix");
    Check(result.hasActualPlayedMove && result.actualPlayedBy == SEAT_WEST &&
          result.actualPlayedMove == BridgeMove(SUIT_SPADES, '4'),
      "decision-point solve should expose the actual next played card after the selected prefix");
    Check(result.hasDDSBestMove,
      "decision-point solve should report a DDS omniscient comparison move");
    Check(result.hasChosenMoveMu && result.hasChosenMoveDDSScore,
      "decision-point solve should report both alpha-mu and DDS detail for the chosen move");
    Check(result.hasActualMoveMu && result.hasActualMoveDDSScore,
      "decision-point solve should report both alpha-mu and DDS detail for the actual next move when it remains legal");
    Check(result.searchNodes > 0 && result.ddsLeafCalls > 0,
      "decision-point solve should report non-zero search-node and DDS-leaf counts");
    Check(result.ddsLeafSeconds > 0.0 &&
          result.ddsLeafSeconds <= result.searchSeconds,
      "decision-point solve should report a positive DDS-leaf time that stays within the total search time");
    Check(result.bridgeSearchStats.frontInsertAttempts > 0 &&
          result.bridgeSearchStats.frontAcceptedInserts > 0,
      "decision-point solve should report frontier insert activity");
    Check(result.bridgeSearchStats.maxMergeCalls > 0 ||
          result.bridgeSearchStats.minProductCalls > 0,
      "decision-point solve should report frontier merge or product activity");
    Check(result.bridgeSearchStats.ddsLeafCuts > 0,
      "decision-point solve should report DDS-leaf cut activity");
    Check(result.worldExplanation.worlds.size() == result.worldCount,
      "decision-point solve should attach a world explanation covering every raw candidate world in the shared decision-world pipeline");
    Check(result.worldExplanation.plausibilityRankedWorldIndices.size() ==
            result.survivingWorldCount,
      "decision-point solve should rank every surviving world for reporting");
    Check(result.worldExplanation.appliedFollowSuitConstraints.size() > 0,
      "decision-point solve should preserve the derived follow-suit implications used by the staged world pipeline");
    Check(result.worldCount == result.worldGenerationStats.candidateWorldCount,
      "decision-point solve should report the raw candidate-world count from the shared Stage 2 pipeline");
    Check(result.worldExplanation.finalWorldIndices == result.activeWorldIndices,
      "decision-point solve should keep the explanation final world ids aligned with the compacted search-world ids");
  }

  static void TestExplicitDecisionPointRequestAPI()
  {
    AlphaMuDecisionPointRequest request;
    request.declarerSeat = SEAT_WEST;
    request.contractLevel = 4;
    request.deal.trump = 0;
    request.deal.first = SEAT_NORTH;
    strcpy(request.deal.remainCards,
      "N:QJ6.K652.J85.T98 873.J97.AT764.Q4 K5.T83.KQ9.A7652 AT942.AQ4.32.KJ3");
    request.depth = 1;
    request.maxWorlds = 50U;
    request.prefixCards = 40;
    request.samplingSeed = 7U;
    request.informationOverrides.biddingConstraints.push_back(
      WorldConstraint::MinHCP(SEAT_EAST, 0));

    playTracePBN play;
    memset(&play, 0, sizeof(play));
    play.number = 45;
    strcpy(play.cards,
      "CTC4CACJH8H4HKH9D5DAD9D2S7S5S2SQD8D4DQD3H3HAH6H7C3C8CQC2S3SKSAS6HQH5HJHTCKC9D6C5S4SJS8C6DJ");
    request.playHistory = ParsePBNPlayHistory(play, request.deal.first,
      request.deal.trump);

    const AlphaMuSolveResult first = SolveAlphaMuDecisionPoint(request);
    const AlphaMuSolveResult second = SolveAlphaMuDecisionPoint(request);

    Check(first.valid && second.valid,
      "explicit decision-point request API should produce valid results on repeated runs");
    Check(first.chosenMove == second.chosenMove,
      "explicit decision-point request API should be deterministic under a fixed seed and configuration");
    Check(first.declarerSeat == SEAT_WEST && first.leaderSeat == SEAT_NORTH &&
          first.contractLevel == 4 && first.contractTrumpSuit == SUIT_SPADES,
      "explicit decision-point request API should preserve the supplied declarer, leader, and contract metadata");
    Check(first.biddingConstraintTexts.size() == 1 &&
          first.biddingConstraintTexts[0].find("East must hold at least 0 HCP") != string::npos,
      "explicit decision-point request API should surface the supplied bidding-derived constraints in reporting text");
    Check(first.worldExplanation.worlds.size() == second.worldExplanation.worlds.size(),
      "explicit decision-point request API should keep the decision-point world set stable across repeated runs");
    for (unsigned i = 0; i < first.worldExplanation.worlds.size(); i++)
    {
      Check(first.worldExplanation.worlds[i].serializedWorld ==
            second.worldExplanation.worlds[i].serializedWorld,
        "explicit decision-point request API should preserve deterministic world ordering across repeated runs");
    }
    Check(first.activeWorldIndices == second.activeWorldIndices,
      "explicit decision-point request API should preserve the active raw world identities across repeated runs");
    Check(first.worldExplanation.finalWorldIndices == first.activeWorldIndices,
      "explicit decision-point request API should keep the explanation final world ids aligned with the compacted decision-point world set");
  }

  static void TestDecisionPointWorldPipelineConsistency()
  {
    AlphaMuDecisionPointRequest request;
    request.declarerSeat = SEAT_WEST;
    request.contractLevel = 4;
    request.deal.trump = 0;
    request.deal.first = SEAT_NORTH;
    strcpy(request.deal.remainCards,
      "N:QJ6.K652.J85.T98 873.J97.AT764.Q4 K5.T83.KQ9.A7652 AT942.AQ4.32.KJ3");
    request.depth = 1;
    request.maxWorlds = 4U;
    request.prefixCards = 40;
    request.samplingSeed = 7U;

    playTracePBN play;
    memset(&play, 0, sizeof(play));
    play.number = 45;
    strcpy(play.cards,
      "CTC4CACJH8H4HKH9D5DAD9D2S7S5S2SQD8D4DQD3H3HAH6H7C3C8CQC2S3SKSAS6HQH5HJHTCKC9D6C5S4SJS8C6DJ");
    request.playHistory = ParsePBNPlayHistory(play, request.deal.first,
      request.deal.trump);

    const AlphaMuSolveResult result = SolveAlphaMuDecisionPoint(request);

    Check(result.valid,
      "decision-point pipeline consistency regression should produce a valid result");
    Check(result.worldCount == result.worldGenerationStats.candidateWorldCount,
      "decision-point pipeline consistency regression should report the raw candidate world count from the shared pipeline");
    Check(result.survivingWorldCount == result.worldExplanation.finalWorldIndices.size(),
      "decision-point pipeline consistency regression should keep the surviving-world count aligned with the explanation final world ids");
    Check(result.activeWorldIndices == result.worldExplanation.finalWorldIndices,
      "decision-point pipeline consistency regression should keep the compacted search-world ids aligned with the explanation final world ids");
    Check(result.activeWorldSerializations.size() == result.survivingWorldCount,
      "decision-point pipeline consistency regression should preserve one serialized active world per compacted search world");
    for (unsigned i = 0; i < result.activeWorldIndices.size(); i++)
    {
      const unsigned worldIndex = result.activeWorldIndices[i];
      Check(worldIndex < result.worldExplanation.worlds.size(),
        "decision-point pipeline consistency regression should reference only explanation worlds that exist");
      Check(result.activeWorldSerializations[i] ==
            result.worldExplanation.worlds[worldIndex].serializedWorld,
        "decision-point pipeline consistency regression should preserve the same world serialization from explanation to compacted search state");
      Check(result.worldExplanation.worlds[worldIndex].accepted,
        "decision-point pipeline consistency regression should keep every compacted search world marked accepted in the explanation trace");
    }
    Check(result.worldCount >= result.survivingWorldCount,
      "decision-point pipeline consistency regression should never report more compacted search worlds than raw candidate worlds");
  }

  static void TestDecisionPointReportingEmitsWorldPipelineMetrics()
  {
    AlphaMuDecisionPointRequest request;
    request.declarerSeat = SEAT_WEST;
    request.contractLevel = 4;
    request.deal.trump = 0;
    request.deal.first = SEAT_NORTH;
    strcpy(request.deal.remainCards,
      "N:QJ6.K652.J85.T98 873.J97.AT764.Q4 K5.T83.KQ9.A7652 AT942.AQ4.32.KJ3");
    request.depth = 1;
    request.maxWorlds = 4U;
    request.prefixCards = 40;
    request.samplingSeed = 7U;

    playTracePBN play;
    memset(&play, 0, sizeof(play));
    play.number = 45;
    strcpy(play.cards,
      "CTC4CACJH8H4HKH9D5DAD9D2S7S5S2SQD8D4DQD3H3HAH6H7C3C8CQC2S3SKSAS6HQH5HJHTCKC9D6C5S4SJS8C6DJ");
    request.playHistory = ParsePBNPlayHistory(play, request.deal.first,
      request.deal.trump);

    const AlphaMuSolveResult result = SolveAlphaMuDecisionPoint(request);
    Check(result.valid,
      "decision-point reporting regression should produce a valid result");

    ostringstream activeIds;
    for (unsigned i = 0; i < result.activeWorldIndices.size(); i++)
    {
      if (i != 0)
        activeIds << ",";
      activeIds << result.activeWorldIndices[i];
    }

    unsigned childFrontVectorTotal = 0;
    unsigned childFrontVectorMax = 0;
    for (unsigned i = 0; i < result.rootReport.children.size(); i++)
    {
      const unsigned childVectors = static_cast<unsigned>(
        result.rootReport.children[i].front.vectors.size());
      childFrontVectorTotal += childVectors;
      if (childVectors > childFrontVectorMax)
        childFrontVectorMax = childVectors;
    }

    const unsigned long long ttProbeMisses =
      (result.ttProbes >= result.ttHits ? result.ttProbes - result.ttHits : 0ULL);
    const double ttHitRate = (result.ttProbes == 0ULL ? 0.0 :
      static_cast<double>(result.ttHits) / static_cast<double>(result.ttProbes));
    const double nonDDSSearchSeconds =
      max(0.0, result.searchSeconds - result.ddsLeafSeconds);

    const auto friendlyStageName =
      [&](const string& stage) -> string
      {
        if (stage == "known_cards")
          return "known cards";
        if (stage == "bidding")
          return "bidding";
        if (stage == "follow_suit")
          return "follow suit";
        if (stage == "play_history")
          return "play history";
        if (stage == "current_trick")
          return "current trick";
        if (stage == "deduplication")
          return "deduplication";
        if (stage == "sampling")
          return "sampling";
        if (stage == "decision_world_set")
          return "decision world set";
        if (stage == "constructor_length")
          return "constructor length";
        if (stage == "constructor_hcp")
          return "constructor HCP";
        if (stage == "constructor_balanced")
          return "constructor shape";
        return stage;
      };

    const auto passedPathSummary =
      [&](const WorldExplanation& world) -> string
      {
        ostringstream oss;
        bool first = true;
        for (unsigned i = 0; i < world.steps.size(); i++)
        {
          if (! world.steps[i].passed)
            break;
          if (! first)
            oss << " -> ";
          oss << friendlyStageName(world.steps[i].stage);
          first = false;
        }
        if (first)
          return "none";
        return oss.str();
      };

    ostringstream expectedDecisionLine;
    expectedDecisionLine << "ALPHA_MU_DECISION raw_worlds="
                         << result.worldCount
                         << " surviving_worlds=" << result.survivingWorldCount
                         << " root_vectors="
                         << result.rootFront.vectors.size()
                         << " root_valid_worlds="
                         << result.rootReport.validWorlds.PopCount()
                         << " root_useful_worlds="
                         << result.rootReport.usefulWorlds.PopCount()
                         << " child_count="
                         << result.rootReport.children.size()
                         << " child_vectors_total=" << childFrontVectorTotal
                         << " child_vectors_max=" << childFrontVectorMax
                         << " tt_exact_stores=" << result.ttStores
                         << " tt_exact_reuses=" << result.ttHits
                         << " tt_probe_misses=" << ttProbeMisses
                         << " tt_collisions=" << result.ttCollisions
                         << " tt_reuse_cuts=" << result.bridgeSearchStats.ttCuts
                         << " tt_hit_rate=";
    expectedDecisionLine.setf(ios::fixed);
    expectedDecisionLine << setprecision(6) << ttHitRate
                         << " cut_empty_world="
                         << result.bridgeSearchStats.emptyWorldCuts
                         << " cut_tt=" << result.bridgeSearchStats.ttCuts
                         << " cut_early_alpha="
                         << result.bridgeSearchStats.earlyAlphaCuts
                         << " cut_deep_alpha="
                         << result.bridgeSearchStats.deepAlphaCuts
                         << " cut_on_win="
                         << result.bridgeSearchStats.cutOnWinCuts
                         << " cut_root="
                         << result.bridgeSearchStats.rootCuts
                         << " cut_dds_leaf="
                         << result.bridgeSearchStats.ddsLeafCuts
                         << " cut_no_move="
                         << result.bridgeSearchStats.noMoveLeafCuts
                         << " terminal_fronts="
                         << result.bridgeSearchStats.terminalFronts
                         << " world_gen_seconds="
                         << result.worldGenerationSeconds
                         << " search_seconds=" << result.searchSeconds
                         << " bridge_search_seconds="
                         << nonDDSSearchSeconds
                         << " dds_leaf_seconds="
                         << result.ddsLeafSeconds
                         << " total_seconds=" << result.totalSeconds
                         << " after_known_cards="
                         << result.worldGenerationStats.afterKnownCardCount
                         << " after_bidding="
                         << result.worldGenerationStats.afterBiddingCount
                         << " after_follow_suit="
                         << result.worldGenerationStats.afterFollowSuitCount
                         << " after_play_history="
                         << result.worldGenerationStats.afterPlayHistoryCount
                         << " after_current_trick="
                         << result.worldGenerationStats.afterCurrentTrickCount
                         << " after_sampling="
                         << result.worldGenerationStats.afterSamplingCount
                         << " duplicates_removed="
                         << result.worldGenerationStats.duplicateWorldsRemoved
                         << " sampled_out="
                         << result.worldGenerationStats.sampledOutWorlds
                         << " active_world_ids="
                         << activeIds.str()
                         << " front_insert_attempts="
                         << result.bridgeSearchStats.frontInsertAttempts
                         << " front_insert_accepts="
                         << result.bridgeSearchStats.frontAcceptedInserts
                         << " front_dominated_rejects="
                         << result.bridgeSearchStats.frontDominatedRejects
                         << " front_dominated_removed="
                         << result.bridgeSearchStats.frontDominatedRemoved
                         << " max_merge_calls="
                         << result.bridgeSearchStats.maxMergeCalls
                         << " min_product_calls="
                         << result.bridgeSearchStats.minProductCalls
                         << " optimistic_completions="
                         << result.bridgeSearchStats.optimisticCompletions
                         << " decision_policy="
                         << AlphaMuDecisionPolicyName(result.appliedDecisionPolicy)
                         << " chosen_weighted="
                         << result.chosenMoveWeightedScore;

    ostringstream expectedHumanReadablePipeline;
    expectedHumanReadablePipeline << "  World pipeline: known="
                                  << result.worldGenerationStats.afterKnownCardCount
                                  << ", bidding="
                                  << result.worldGenerationStats.afterBiddingCount
                                  << ", follow-suit="
                                  << result.worldGenerationStats.afterFollowSuitCount
                                  << ", history="
                                  << result.worldGenerationStats.afterPlayHistoryCount
                                  << ", current-trick="
                                  << result.worldGenerationStats.afterCurrentTrickCount
                                  << ", sampled="
                                  << result.worldGenerationStats.afterSamplingCount
                                  << ", duplicates-removed="
                                  << result.worldGenerationStats.duplicateWorldsRemoved
                                  << ", sampled-out="
                                  << result.worldGenerationStats.sampledOutWorlds;

    ostringstream expectedActiveIdsLine;
    expectedActiveIdsLine << "  Active raw world ids: "
                          << activeIds.str();

    ostringstream expectedFrontSummaryLine;
    expectedFrontSummaryLine << "  Front summary: root_vectors="
                             << result.rootFront.vectors.size()
                             << ", root_valid_worlds="
                             << result.rootReport.validWorlds.PopCount()
                             << ", root_useful_worlds="
                             << result.rootReport.usefulWorlds.PopCount()
                             << ", child_count="
                             << result.rootReport.children.size()
                             << ", child_vectors_total="
                             << childFrontVectorTotal
                             << ", child_vectors_max="
                             << childFrontVectorMax;

    ostringstream expectedTTLine;
    expectedTTLine.setf(ios::fixed);
    expectedTTLine << setprecision(3)
                   << "  TT: exact_stores=" << result.ttStores
                   << ", exact_reuses=" << result.ttHits
                   << ", probes=" << result.ttProbes
                   << ", probe_misses=" << ttProbeMisses
                   << ", collisions=" << result.ttCollisions
                   << ", reuse_cuts=" << result.bridgeSearchStats.ttCuts
                   << ", hit_rate=" << ttHitRate;

    ostringstream expectedTopWorldLine;
    const unsigned topWorldIndex =
      result.worldExplanation.plausibilityRankedWorldIndices[0];
    const WorldExplanation& topWorld =
      result.worldExplanation.worlds[topWorldIndex];
    expectedTopWorldLine << "    [" << topWorldIndex << "] score="
                         << topWorld.plausibilityScore << "/"
                         << topWorld.plausibilityMaxScore
                         << " " << topWorld.serializedWorld;
    if (! topWorld.satisfiedPlausibilityHints.empty())
      expectedTopWorldLine << " | matched="
                           << topWorld.satisfiedPlausibilityHints[0];
    expectedTopWorldLine << " | passed_path="
                         << passedPathSummary(topWorld);

    unsigned rejectedWorldIndex = result.worldExplanation.worlds.size();
    for (unsigned i = 0; i < result.worldExplanation.worlds.size(); i++)
    {
      if (! result.worldExplanation.worlds[i].accepted)
      {
        rejectedWorldIndex = i;
        break;
      }
    }
    Check(rejectedWorldIndex < result.worldExplanation.worlds.size(),
      "decision-point reporting regression should have at least one rejected world to describe in plain language");
    const WorldExplanation& rejectedWorld =
      result.worldExplanation.worlds[rejectedWorldIndex];
    ostringstream expectedRejectedWorldLine;
    expectedRejectedWorldLine << "    [" << rejectedWorld.worldIndex << "] "
                              << "rejected at "
                              << friendlyStageName(rejectedWorld.rejectionStage)
                              << " because "
                              << rejectedWorld.rejectionReason
                              << " | passed_path="
                              << passedPathSummary(rejectedWorld);

    ostringstream expectedPolicyLine;
    expectedPolicyLine << "  Decision policy: "
                       << AlphaMuDecisionPolicyName(result.appliedDecisionPolicy);

    ostringstream capture;
    streambuf * const original = cout.rdbuf(capture.rdbuf());
    ReportAlphaMuSolveResult(result);
    cout.rdbuf(original);

    const string output = capture.str();
    Check(output.find(expectedDecisionLine.str()) != string::npos,
      "decision-point reporting regression should emit the machine-readable Stage 6.1 world-pipeline summary");
    Check(output.find(expectedHumanReadablePipeline.str()) != string::npos,
      "decision-point reporting regression should emit the human-readable world-pipeline summary");
    Check(output.find(expectedActiveIdsLine.str()) != string::npos,
      "decision-point reporting regression should emit the active raw-world identities");
    Check(output.find(expectedFrontSummaryLine.str()) != string::npos,
      "decision-point reporting regression should emit the Stage 6.2 front-summary aggregates");
    Check(output.find(expectedTTLine.str()) != string::npos,
      "decision-point reporting regression should emit the Stage 6.3 TT exactness and reuse summary");
    Check(output.find(expectedPolicyLine.str()) != string::npos,
      "decision-point reporting regression should emit the explicit Stage 4 decision-policy label");
    Check(output.find(expectedTopWorldLine.str()) != string::npos,
      "decision-point reporting regression should emit a plain-language passed path for a top surviving world");
    Check(output.find(expectedRejectedWorldLine.str()) != string::npos,
      "decision-point reporting regression should emit a plain-language rejection path for a rejected world");
  }

  static void TestPracticalMultiWorldContinuationDepth2Stable()
  {
    dealPBN deal;
    memset(&deal, 0, sizeof(deal));
    deal.trump = 0;
    deal.first = 0;
    strcpy(deal.remainCards,
      "N:QJ6.K652.J85.T98 873.J97.AT764.Q4 K5.T83.KQ9.A7652 AT942.AQ4.32.KJ3");

    playTracePBN play;
    memset(&play, 0, sizeof(play));
    play.number = 45;
    strcpy(play.cards,
      "CTC4CACJH8H4HKH9D5DAD9D2S7S5S2SQD8D4DQD3H3HAH6H7C3C8CQC2S3SKSAS6HQH5HJHTCKC9D6C5S4SJS8C6DJ");

    const int declarerSeat = (deal.first + 3) % 4;
    const AlphaMuSolveResult first = SolveAlphaMu(
      deal, declarerSeat, play, 2, 8, 0.0, 40, 7U);
    const AlphaMuSolveResult second = SolveAlphaMu(
      deal, declarerSeat, play, 2, 8, 0.0, 40, 7U);

    Check(first.valid && second.valid,
      "practical multi-world depth-2 continuation should produce valid results on repeated runs");
    Check(first.survivingWorldCount > 2,
      "practical multi-world depth-2 continuation should keep more than two surviving worlds");
    Check(first.depthSearched == 2,
      "practical multi-world depth-2 continuation should search the requested two tricks");
    Check(first.rootReport.children.size() >= 2,
      "practical multi-world depth-2 continuation should expose multiple candidate moves at the root");
    Check(first.chosenMove == second.chosenMove,
      "practical multi-world depth-2 continuation should keep the chosen move stable across repeated runs");
    Check(first.rootReport.validWorlds == first.rootFront.ValidWorlds(),
      "practical multi-world depth-2 continuation should record the root valid-world summary");
    Check(first.rootReport.usefulWorlds == first.rootFront.UsefulWorlds(),
      "practical multi-world depth-2 continuation should record the root useful-world summary");
    Check(first.rootReport.searchNodes > 0 && first.rootReport.ddsLeafCalls > 0,
      "practical multi-world depth-2 continuation should record aggregate root search and DDS-leaf activity");
    for (unsigned i = 0; i < first.rootReport.children.size(); i++)
    {
      Check(first.rootReport.children[i].validWorlds.PopCount() > 0,
        "every practical multi-world depth-2 child should cover at least one surviving world");
      Check(first.rootReport.children[i].searchNodes > 0,
        "every practical multi-world depth-2 child should record search activity");
    }
  }

  static void TestPracticalPartialTrickContinuationDepth2Stable()
  {
    dealPBN deal;
    memset(&deal, 0, sizeof(deal));
    deal.trump = 0;
    deal.first = 0;
    strcpy(deal.remainCards,
      "N:QJ6.K652.J85.T98 873.J97.AT764.Q4 K5.T83.KQ9.A7652 AT942.AQ4.32.KJ3");

    playTracePBN play;
    memset(&play, 0, sizeof(play));
    play.number = 45;
    strcpy(play.cards,
      "CTC4CACJH8H4HKH9D5DAD9D2S7S5S2SQD8D4DQD3H3HAH6H7C3C8CQC2S3SKSAS6HQH5HJHTCKC9D6C5S4SJS8C6DJ");

    const int declarerSeat = (deal.first + 3) % 4;
    const AlphaMuSolveResult first = SolveAlphaMu(
      deal, declarerSeat, play, 2, 8, 0.0, 34, 7U);
    const AlphaMuSolveResult second = SolveAlphaMu(
      deal, declarerSeat, play, 2, 8, 0.0, 34, 7U);

    Check(first.valid && second.valid,
      "practical partial-trick depth-2 continuation should produce valid results on repeated runs");
    Check(first.survivingWorldCount > 2,
      "practical partial-trick depth-2 continuation should keep more than two surviving worlds");
    Check(first.depthSearched == 2,
      "practical partial-trick depth-2 continuation should search the requested two tricks");
    Check(first.prefixPlayLength == 34,
      "practical partial-trick depth-2 continuation should preserve the selected 34-card prefix");
    Check(first.playerToMove == SEAT_EAST,
      "practical partial-trick depth-2 continuation should stop on East after the 34-card prefix");
    Check(first.hasActualPlayedMove && first.actualPlayedMove == BridgeMove(SUIT_HEARTS, 'J'),
      "practical partial-trick depth-2 continuation should expose the next-card transition across the partial trick");
    Check(first.chosenMove == second.chosenMove,
      "practical partial-trick depth-2 continuation should keep the chosen move stable across repeated runs");
    Check(first.rootReport.children.size() == 1,
      "practical partial-trick depth-2 continuation should preserve the single forced continuation at this decision point");
    Check(first.rootReport.children[0].ddsLeafCalls > 0,
      "practical partial-trick depth-2 continuation should reach DDS leaves after crossing the partial-trick boundary");
  }

  static void TestPracticalMultiWorldContinuationDepth3Stable()
  {
    dealPBN deal;
    memset(&deal, 0, sizeof(deal));
    deal.trump = 0;
    deal.first = 0;
    strcpy(deal.remainCards,
      "N:QJ6.K652.J85.T98 873.J97.AT764.Q4 K5.T83.KQ9.A7652 AT942.AQ4.32.KJ3");

    playTracePBN play;
    memset(&play, 0, sizeof(play));
    play.number = 45;
    strcpy(play.cards,
      "CTC4CACJH8H4HKH9D5DAD9D2S7S5S2SQD8D4DQD3H3HAH6H7C3C8CQC2S3SKSAS6HQH5HJHTCKC9D6C5S4SJS8C6DJ");

    const int declarerSeat = (deal.first + 3) % 4;
    const AlphaMuSolveResult first = SolveAlphaMu(
      deal, declarerSeat, play, 3, 8, 0.0, 32, 7U);
    const AlphaMuSolveResult second = SolveAlphaMu(
      deal, declarerSeat, play, 3, 8, 0.0, 32, 7U);

    Check(first.valid && second.valid,
      "practical multi-world depth-3 continuation should produce valid results on repeated runs");
    Check(first.prefixPlayLength == 32,
      "practical multi-world depth-3 continuation should preserve the selected 32-card prefix");
    Check(first.survivingWorldCount > 1,
      "practical multi-world depth-3 continuation should keep more than one surviving world");
    Check(first.depthSearched == 3,
      "practical multi-world depth-3 continuation should search the requested three tricks");
    Check(first.rootReport.children.size() >= 2,
      "practical multi-world depth-3 continuation should expose multiple candidate moves at the root");
    Check(first.chosenMove == second.chosenMove,
      "practical multi-world depth-3 continuation should keep the chosen move stable across repeated runs");
    Check(first.rootReport.searchNodes > 0 && first.rootReport.ddsLeafCalls > 0,
      "practical multi-world depth-3 continuation should record aggregate search and DDS-leaf activity");
    Check(first.ttStores > 0,
      "practical multi-world depth-3 continuation should store practical exact bridge fronts in the TT");
    Check(first.rootReport.validWorlds == first.rootFront.ValidWorlds() &&
          first.rootReport.usefulWorlds == first.rootFront.UsefulWorlds(),
      "practical multi-world depth-3 continuation should keep root world summaries aligned with the searched front");
    for (unsigned i = 0; i < first.rootReport.children.size(); i++)
    {
      Check(first.rootReport.children[i].validWorlds.PopCount() > 0,
        "every practical multi-world depth-3 child should cover at least one surviving world");
      Check(first.rootReport.children[i].ddsLeafCalls > 0,
        "every practical multi-world depth-3 child should reach DDS leaves on the deeper practical continuation");
    }
  }

  void TestBridgeTranspositionTable()
  {
    // Use the same board as TestEndToEndSolveAlphaMu
    dealPBN deal;
    memset(&deal, 0, sizeof(deal));
    deal.trump = 0;  // spades
    deal.first = 0;  // North leads
    strcpy(deal.remainCards,
      "N:QJ6.K652.J85.T98 873.J97.AT764.Q4 K5.T83.KQ9.A7652 AT942.AQ4.32.KJ3");

    const int declarerSeat = SEAT_SOUTH;

    playTracePBN play;
    memset(&play, 0, sizeof(play));
    play.number = 40;
    strcpy(play.cards,
      "CTC4CACJH8H4HKH9D5DAD9D2S7S5S2SQD8D4DQD3H3HAH6H7C3C8CQC2S3SKSAS6HQH5HJHTCKC9D6C5");

    const int trumpSuit = deal.trump;
    const vector<PlayHistoryEvent> history =
      ParsePBNPlayHistory(play, deal.first, trumpSuit);

    const unsigned maxWorlds = 50;
    BridgeState state = MakeBridgeStateFromPartialInformation(
      deal, declarerSeat, history, maxWorlds);

    Check(state.possibleWorlds.PopCount() > 0,
      "TT test: should have surviving worlds");

    unsigned singleWorldIndex = 0;
    while (singleWorldIndex < state.possibleWorlds.count &&
           ! state.possibleWorlds.Has(singleWorldIndex))
    {
      singleWorldIndex++;
    }
    Check(singleWorldIndex < state.possibleWorlds.count,
      "TT test: should expose at least one active world for the Stage 1 single-world cut");

    const int depth = 1;
    SearchExecutionContext context;

    SearchExecutionContext singleWorldContext;
    singleWorldContext.bridgeSearch.hasUsefulWorlds = true;
    singleWorldContext.bridgeSearch.usefulWorlds = WorldMask(
      state.possibleWorlds.count,
      (1ULL << singleWorldIndex));

    const ParetoFront singleWorldFront = SearchBridgeState(
      state, depth, singleWorldContext);
    const ParetoFront expectedSingleWorldFront = MakeSingleWorldFront(
      state.possibleWorlds.count,
      singleWorldIndex,
      ExactBridgeDDSScoreForWorld(state, singleWorldIndex, singleWorldContext));
    Check(singleWorldFront.ToString() == expectedSingleWorldFront.ToString(),
      "TT test: real bridge search should collapse a single useful world to the exact DDS-backed front");

    const ParetoFront singleWorldTTCapableFront = SearchBridgeStateWithTT(
      state, depth, singleWorldContext, NULL, NULL);
    Check(singleWorldTTCapableFront.ToString() == expectedSingleWorldFront.ToString(),
      "TT test: TT-capable bridge search should preserve the same exact single-world cut result");

    // Search without TT
    const ParetoFront frontNoTT = SearchBridgeStateInternal(
      state, depth, context);

    // Search with TT
    InitZobrist();
    BridgeTranspositionTable tt(1U << 16);
    BridgeTTStats ttStats;
    const ParetoFront frontWithTT = SearchBridgeStateWithTT(
      state, depth, context, &tt, &ttStats);

    // Verify identical results
    Check(frontNoTT.vectors.size() == frontWithTT.vectors.size(),
      "TT-backed search must produce same number of front vectors");

    for (unsigned i = 0; i < frontNoTT.vectors.size(); i++)
    {
      Check(SameOutcome(frontNoTT.vectors[i], frontWithTT.vectors[i]),
        "TT-backed search must produce identical outcome vectors");
    }

    // Verify TT was actually used
    Check(ttStats.stores > 0,
      "TT should have stored at least one entry");

    // Run a second search — should get hits
    BridgeTTStats ttStats2;
    const ParetoFront frontSecond = SearchBridgeStateWithTT(
      state, depth, context, &tt, &ttStats2);

    Check(ttStats2.hits > 0,
      "Second search should produce TT hits");

    // Second search should also produce identical results
    Check(frontNoTT.vectors.size() == frontSecond.vectors.size(),
      "Second TT-backed search must produce same number of front vectors");

    for (unsigned i = 0; i < frontNoTT.vectors.size(); i++)
    {
      Check(SameOutcome(frontNoTT.vectors[i], frontSecond.vectors[i]),
        "Second TT-backed search must produce identical outcome vectors");
    }
  }

  void TestIterativeDeepeningDepth3()
  {
    // Board 1 from alpha_mu_play.txt with an 8-trick (32-card) prefix,
    // leaving 5 cards per hand and therefore 5 remaining tricks.
    dealPBN deal;
    memset(&deal, 0, sizeof(deal));
    deal.trump = 0;  // spades
    deal.first = 0;  // North leads
    strcpy(deal.remainCards,
      "N:QJ6.K652.J85.T98 873.J97.AT764.Q4 K5.T83.KQ9.A7652 AT942.AQ4.32.KJ3");

    const int declarerSeat = SEAT_SOUTH;

    // Use first 8 tricks = 32 cards, leaving 5 cards per hand.
    // This tests deeper search than the 10-trick (3-card) prefix.
    playTracePBN play;
    memset(&play, 0, sizeof(play));
    play.number = 32;
    strcpy(play.cards,
      "CTC4CACJH8H4HKH9D5DAD9D2S7S5S2SQD8D4DQD3H3HAH6H7C3C8CQC2S3SKSAS6");

    const vector<PlayHistoryEvent> history =
      ParsePBNPlayHistory(play, deal.first, deal.trump);
    Check(history.size() == 32,
      "depth-3 fixture should parse the 32-card eight-trick prefix exactly");

    const BridgeState state = MakeBridgeStateFromPartialInformation(
      deal, declarerSeat, history, 1);
    Check(state.currentTrick.empty(),
      "depth-3 fixture should end on a trick boundary before search starts");
    Check(state.worlds.size() == 1,
      "depth-3 fixture should compact the oversized 252-world candidate pool to the one requested sampled world before search");
    Check(state.possibleWorlds == WorldMask(1, 0x1ULL),
      "depth-3 fixture should keep the compacted one-world bridge state fully active before search");
    for (unsigned i = 0; i < state.worlds.size(); i++)
    {
      Check(WorldSeatCardCount(state.worlds[i], SEAT_NORTH) == 5 &&
            WorldSeatCardCount(state.worlds[i], SEAT_EAST) == 5 &&
            WorldSeatCardCount(state.worlds[i], SEAT_SOUTH) == 5 &&
            WorldSeatCardCount(state.worlds[i], SEAT_WEST) == 5,
        "depth-3 fixture should leave 5 cards in every hand after the eight-trick prefix");
      Check(WorldCardCount(state.worlds[i]) == 20,
        "depth-3 fixture should leave 20 cards in hand across the four seats after the eight-trick prefix");
    }

    const int tricksRemaining = static_cast<int>(
      WorldCardCount(state.worlds[0]) / 4U);
    Check(tricksRemaining == 5,
      "depth-3 fixture should leave 5 full tricks remaining after the eight-trick prefix");

    const AlphaMuSolveResult result = SolveAlphaMu(
      deal, declarerSeat, play, 3, 1, 60.0);

    Check(result.valid,
      "depth-3 iterative deepening should produce a valid result");
    Check(result.depthSearched == 3,
      "depth-3 iterative deepening should complete all three requested trick depths on the one-world five-card fixture");
    Check(result.rootFront.vectors.size() > 0,
      "depth-3 iterative deepening should produce a non-empty front");
    Check(result.chosenMove.suit >= 0 && result.chosenMove.suit <= 3,
      "depth-3 iterative deepening should choose a valid suit");
    Check(result.totalSeconds < 60.0,
      "depth-3 iterative deepening should complete within 60 seconds");
    Check(result.ttStores > 0,
      "depth-3 iterative deepening should use TT (stores > 0)");
  }


  void TestDebugWorldMaskCapacityAssertion()
  {
#ifdef NDEBUG
    Check(true,
      "debug-only world-mask assertion regression is skipped in release builds");
#else
    const string exe = GetAlphaMuExecutablePath();
    Check(! exe.empty(),
      "debug assertion regression should know the current alpha-mu executable path");

    const string outputPath =
      "/tmp/alpha_mu_worldmask_assert_output.txt";
    remove(outputPath.c_str());

    string command;
    const char * dyldLibraryPath = getenv("DYLD_LIBRARY_PATH");
    if (dyldLibraryPath != NULL && *dyldLibraryPath != '\0')
      command += string("DYLD_LIBRARY_PATH=") + ShellQuote(dyldLibraryPath) + " ";

    command += ShellQuote(exe) +
      " debug_assert_worldmask_capacity > " +
      ShellQuote(outputPath) + " 2>&1";
    const int status = system(command.c_str());
    Check(status != 0,
      "debug-only world-mask assertion regression should fail in the child process");

    const string output = ReadWholeFile(outputPath);
    remove(outputPath.c_str());
    Check(output.find(
      "GeneratePossibleWorlds requires at most 64 worlds because WorldMask is backed by one 64-bit word") != string::npos,
      "debug-only world-mask assertion regression should report the expected capacity invariant message");
#endif
  }


  void RunDebugWorldMaskCapacityAssertionTrigger()
  {
#ifdef NDEBUG
    Fail("debug_assert_worldmask_capacity mode is only available in debug builds");
#else
    vector<ParsedWorld> worlds(65);
    BridgeInformationState information;
    GeneratePossibleWorlds(worlds, information, NULL);
    Fail("debug_assert_worldmask_capacity mode should have failed before returning");
#endif
  }

  void RunBridgeDDSTestSuite()
  {
    TestBridgeAncestorEarlyCutAtRootMax();
    PrintAlphaMuStatus("bridge optimistic completion and early cut OK");
    TestBridgeAncestorEarlyCutPreservesExactTTReuse();
    PrintAlphaMuStatus("bridge optimistic-cut TT exactness OK");
    TestBridgeDeepAlphaCut();
    PrintAlphaMuStatus("bridge deep alpha cut OK");
    TestBridgeCutOnWin();
    PrintAlphaMuStatus("bridge cut-on-win OK");
    TestBridgeRootCut();
    PrintAlphaMuStatus("bridge root-cut OK");
    TestBridgeMultiTrickDDSLeaf();
    PrintAlphaMuStatus("multi-trick bridge DDS leaf search OK");
    PrintAlphaMuStatus("all checks passed");
  }

  void RunDefaultTestSuite()
  {
    const NamedTest tests[] = {
      {"Pareto insert test OK", &TestParetoInsert},
      {"non-locality toy search OK", &TestNonLocalityExample},
      {"early cut toy search OK", &TestEarlyCutExample},
      {"useful-world maintenance OK", &TestUsefulWorldMaintenance},
      {"world cuts OK", &TestWorldCuts},
      {"Pareto-front TT OK", &TestParetoFrontTT},
      {"possible-world generation OK", &TestPossibleWorldGeneration},
      {"play-history filtering OK", &TestPlayHistoryFiltering},
      {"uneven seed-world seat counts OK", &TestBuildWorldSpecTracksUnevenSeatCounts},
      {"uneven current-trick world counts OK", &TestPartialInformationUnevenCurrentTrickCounts},
      {"follow-suit implications and world explanations OK", &TestFollowSuitImplicationsAndWorldExplanation},
      {"world plausibility reporting OK", &TestWorldPlausibilityReporting},
      {"active-world plausibility weights OK", &TestActiveWorldPlausibilityWeightsRespectCompactedOrdering},
      {"weighted decision selection OK", &TestWeightedDecisionPolicySelectsRootChildExplicitly},
      {"history-derived candidate world construction OK", &TestHistoryDerivedCandidateWorldConstruction},
      {"history-derived known-card construction OK", &TestHistoryDerivedConstructionUsesKnownCardLocation},
      {"history-derived known-card exclusion construction OK", &TestHistoryDerivedConstructionUsesKnownCardExclusion},
      {"history-derived construction explanation accounting OK", &TestHistoryDerivedConstructionExplanationTracksCardLocationNarrowing},
      {"history-derived partnership HCP construction OK", &TestHistoryDerivedConstructionUsesPartnershipHCPRange},
      {"history-derived partnership length construction OK", &TestHistoryDerivedConstructionUsesPartnershipLengthRange},
      {"history-derived constructor stage accounting OK", &TestHistoryDerivedConstructionExplanationTracksPartnershipRangeStageCounts},
      {"history-derived construction explanation pruning OK", &TestHistoryDerivedConstructionExplanationTracksFollowSuitRejections},
      {"history-derived current-trick construction OK", &TestHistoryDerivedConstructionRespectsCurrentTrick},
      {"history-derived bidding card-location construction OK", &TestHistoryDerivedConstructionUsesBiddingCardLocation},
      {"history-derived bidding length construction OK", &TestHistoryDerivedConstructionUsesBiddingLength},
      {"history-derived bidding MinHCP construction OK", &TestHistoryDerivedConstructionUsesBiddingMinHCP},
      {"history-derived bidding MaxHCP construction OK", &TestHistoryDerivedConstructionUsesBiddingMaxHCP},
      {"history-derived bidding balanced construction OK", &TestHistoryDerivedConstructionUsesBiddingBalancedShape},
      {"history-derived combined bidding profile construction OK", &TestHistoryDerivedConstructionUsesCombinedBiddingProfile},
      {"history-derived bidding hand-type construction OK", &TestHistoryDerivedConstructionUsesBiddingHandType},
      {"history-derived balanced deferral on incomplete hands OK", &TestHistoryDerivedConstructionBalancedShapeDefersOnIncompleteHands},
      {"history-derived hand-type deferral on incomplete hands OK", &TestHistoryDerivedConstructionHandTypeDefersOnIncompleteHands},
      {"history-derived follow-suit construction OK", &TestHistoryDerivedConstructionUsesDerivedFollowSuitLength},
      {"history-derived void-suit carry-through construction OK", &TestHistoryDerivedConstructionCarriesVoidSuitIntoConstructor},
      {"history-derived visible-seed construction OK", &TestHistoryDerivedConstructionInfersHiddenCardsFromVisibleHands},
      {"history-derived moderate visible-seed pools OK", &TestHistoryDerivedConstructionSupportsModeratelyLargerVisibleSeedPools},
      {"history-derived longer visible-seed history OK", &TestHistoryDerivedConstructionUsesLongerVisibleSeedHistory},
      {"deterministic world sampling OK", &TestDeterministicWorldSampling},
      {"bridge move generation OK", &TestBridgeMoveGeneration},
      {"bridge search control OK", &TestBridgeSearchControl},
      {"bridge optimistic completion and early cut OK", &TestBridgeAncestorEarlyCutAtRootMax},
      {"bridge optimistic-cut TT exactness OK", &TestBridgeAncestorEarlyCutPreservesExactTTReuse},
      {"bridge root reporting OK", &TestBridgeRootReportThreeWorldContinuation},
      {"empty-entry interior fronts OK", &TestEmptyEntryInteriorFronts},
      {"optimistic impossible worlds OK", &TestOptimisticImpossibleWorlds},
      {"deep alpha cuts OK", &TestDeepAlphaCut},
      {"cut on win OK", &TestCutOnWin},
      {"root cut toy search OK", &TestRootCutExample},
       {"DDS leaf demo and leaf parallelization OK", &TestDDSLeafDemo},
       {"bridge search execution context parity OK", &TestBridgeSearchExplicitExecutionContext},
       {"benchmark option normalization OK", &TestAlphaMuBenchmarkOptionNormalization},
       {"benchmark overload parity OK", &TestAlphaMuBenchmarkOptionsOverloadParity},
       {"board-parallel benchmark parity OK", &TestBoardParallelBenchmarkParity},
       {"repeated DDS reinitialization OK", &TestRepeatedDDSReinitializationKeepsThreadContext},
       {"play history parse validation OK", &TestParsePlayHistoryValidation},
       {"partial-information world generation OK", &TestPartialInformationWorldGeneration},
       {"follow-suit narrowing in partial information OK", &TestFollowSuitNarrowingInPartialInformation},
       {"end-to-end alpha-mu solve OK", &TestEndToEndSolveAlphaMu},
       {"decision-point comparison reporting OK", &TestDecisionPointComparisonReporting},
       {"explicit decision-point request API OK", &TestExplicitDecisionPointRequestAPI},
       {"decision-point world pipeline consistency OK", &TestDecisionPointWorldPipelineConsistency},
       {"decision-point world pipeline reporting OK", &TestDecisionPointReportingEmitsWorldPipelineMetrics},
       {"practical multi-world depth-2 continuation OK", &TestPracticalMultiWorldContinuationDepth2Stable},
       {"practical partial-trick depth-2 continuation OK", &TestPracticalPartialTrickContinuationDepth2Stable},
        {"practical multi-world depth-3 continuation OK", &TestPracticalMultiWorldContinuationDepth3Stable},
       {"bridge transposition table OK", &TestBridgeTranspositionTable},
       {"iterative deepening depth-3 OK", &TestIterativeDeepeningDepth3},
#ifndef NDEBUG
       {"debug world-mask assertion regression OK", &TestDebugWorldMaskCapacityAssertion}
#endif
    };

    for (unsigned i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
      tests[i].run();
      PrintAlphaMuStatus(tests[i].successMessage);
    }

    PrintAlphaMuStatus("all checks passed");
  }
}
