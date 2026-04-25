/**
 * @file alpha_mu_tests.h
 * @brief Entry points for the alpha-mu regression bundles.
 */

#ifndef DDS_TEST_ALPHA_MU_TESTS_H
#define DDS_TEST_ALPHA_MU_TESTS_H

namespace alpha_mu
{
  /** @brief Run the focused bridge-continuation and DDS leaf regression subset. */
  void RunBridgeDDSTestSuite();
  /** @brief Run the complete alpha-mu regression suite. */
  void RunDefaultTestSuite();
  /** @brief Run the partial-information world generation test. */
  void TestPartialInformationWorldGeneration();
  /** @brief Run play-history parse validation tests. */
  void TestParsePlayHistoryValidation();
  /** @brief Run the follow-suit narrowing test for partial-information worlds. */
  void TestFollowSuitNarrowingInPartialInformation();
  /** @brief Run the end-to-end SolveAlphaMu smoke test. */
  void TestEndToEndSolveAlphaMu();
  /** @brief Verify bridge TT produces identical fronts to TT-free search, with hits > 0. */
  void TestBridgeTranspositionTable();
  /** @brief Verify iterative deepening with TT reaches depth 3 on a 5-card-per-hand position. */
  void TestIterativeDeepeningDepth3();
  /** @brief Verify the debug-only world-mask capacity assertion fires through a child process. */
  void TestDebugWorldMaskCapacityAssertion();
  /** @brief Trigger the debug-only world-mask capacity assertion path in a dedicated process mode. */
  void RunDebugWorldMaskCapacityAssertionTrigger();
}

#endif
