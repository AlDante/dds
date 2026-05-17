/**
 * @file PlayAnalyser.h
 * @brief Helpers for DDS played-line analysis over one board or a batch.
 *
 * Play analysis repeatedly solves the position after each played card so DDS can
 * report how many tricks declarer could still take from every prefix of the
 * trace.
 *
 * Copyright (C) 2006-2014 by Bo Haglund /
 * 2014-2018 by Bo Haglund & Soren Hein.
 *
 * See LICENSE and README.
 */

#ifndef DDS_PLAYANALYSER_H
#define DDS_PLAYANALYSER_H

#include <vector>

#include "dds.h"

using namespace std;


/** @brief Analyse one played line on one worker thread. */
void PlaySingleCommon(
  const int thrId,
  const int bno);

/** @brief Worker loop for batched play-analysis jobs. */
void PlayChunkCommon(
  const int thrId);

/** @brief Detect repeated play-analysis inputs whose answers can be copied. */
void DetectPlayDuplicates(
  const boards& bds,
  vector<int>& uniques,
  vector<int>& crossrefs);

/** @brief Copy play-analysis results from an equivalent earlier batch entry. */
void CopyPlaySingle(
  const vector<int>& crossrefs);

#endif
