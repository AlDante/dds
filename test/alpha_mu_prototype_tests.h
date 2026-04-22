/**
 * @file alpha_mu_prototype_tests.h
 * @brief Entry points for the alpha-mu prototype regression bundles.
 */

#ifndef DDS_TEST_ALPHA_MU_PROTOTYPE_TESTS_H
#define DDS_TEST_ALPHA_MU_PROTOTYPE_TESTS_H

namespace alpha_mu_prototype
{
  /** @brief Run the focused bridge-continuation and DDS leaf regression subset. */
  void RunBridgeDDSTestSuite();
  /** @brief Run the complete alpha-mu prototype regression suite. */
  void RunDefaultTestSuite();
  /** @brief Run the partial-information world generation test. */
  void TestPartialInformationWorldGeneration();
  /** @brief Run play-history parse validation tests. */
  void TestParsePlayHistoryValidation();
  /** @brief Run the follow-suit narrowing test for partial-information worlds. */
  void TestFollowSuitNarrowingInPartialInformation();
}

#endif
