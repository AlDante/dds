/*
   DDS, a bridge double dummy solver.

   Copyright (C) 2006-2014 by Bo Haglund /
   2014-2018 by Bo Haglund & Soren Hein.

   See LICENSE and README.
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../include/dll.h"
#include "../examples/hands.h"

int main()
{
  SetMaxThreads(0);

  for (int handno = 0; handno < 3; handno++)
  {
    dealPBN dlPBN;
    playTracePBN playPBN;
    solvedPlay solved;
    char line[80];

    dlPBN.trump = trump[handno];
    dlPBN.first = first[handno];

    dlPBN.currentTrickSuit[0] = 0;
    dlPBN.currentTrickSuit[1] = 0;
    dlPBN.currentTrickSuit[2] = 0;

    dlPBN.currentTrickRank[0] = 0;
    dlPBN.currentTrickRank[1] = 0;
    dlPBN.currentTrickRank[2] = 0;

    std::strcpy(dlPBN.remainCards, PBN[handno]);

    playPBN.number = playNo[handno];
    std::strcpy(playPBN.cards, play[handno]);

    const int res = AnalysePlayPBN(dlPBN, playPBN, &solved, 0);
    if (res != RETURN_NO_FAULT)
    {
      ErrorMessage(res, line);
      std::fprintf(stderr, "play_analysis_benchmark: DDS error on hand %d: %s\n",
        handno + 1, line);
      return 1;
    }

    if (! ComparePlay(&solved, handno))
    {
      std::fprintf(stderr,
        "play_analysis_benchmark: play trace mismatch on hand %d\n",
        handno + 1);
      return 1;
    }

    std::printf("play_analysis_benchmark: hand %d OK\n", handno + 1);
  }

  return 0;
}

