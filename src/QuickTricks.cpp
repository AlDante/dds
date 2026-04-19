/*
   DDS, a bridge double dummy solver.

   Copyright (C) 2006-2014 by Bo Haglund /
   2014-2018 by Bo Haglund & Soren Hein.

   See LICENSE and README.
*/


#include <algorithm>

#include "QuickTricks.h"


namespace
{
  // Replace highestRank[] table lookup with CLZ intrinsic (§9.4).
  // On ARM64 this compiles to a single CLZ instruction — no memory access.
  inline int highestRankFast(unsigned short ranks)
  {
    return ranks
      ? (31 - __builtin_clz(static_cast<unsigned int>(ranks)))
      : 0;
  }

  // Advance to next suit, skipping trump (§9.2).
  // Pre-condition: trump != DDS_NOTRUMP and suit != trump.
  // Branchless: the (suit == trump) term is 0 or 1.
  inline int nextSuitSkipTrump(int suit, int trump)
  {
    suit++;
    suit += (suit == trump);
    return suit;
  }

  // General suit advancement that handles both trump-suit and
  // non-trump-suit cases.  When the current suit IS trump, jump
  // to the first non-trump suit; otherwise increment and skip trump.
  inline int advanceSuit(int suit, int trump)
  {
    if ((trump != DDS_NOTRUMP) && (suit == trump))
      return (trump == 0) ? 1 : 0;
    return nextSuitSkipTrump(suit, trump);
  }
}


// Forward declarations of sub-functions (now taking QtricksContext).
QtricksResult QtricksLeadHandTrump(QtricksContext& ctx);
QtricksResult QtricksLeadHandNT(QtricksContext& ctx);
QtricksResult QuickTricksPartnerHandTrump(QtricksContext& ctx);
QtricksResult QuickTricksPartnerHandNT(QtricksContext& ctx);


int QuickTricks(
  pos& tpos,
  const int hand,
  const int depth,
  const int target,
  const int trump,
  bool& result,
  const ThreadData& thrd)
{
  int suit, commRank = 0, commSuit = -1;
  int lhoTrumpRanks = 0, rhoTrumpRanks = 0;
  int cutoff, lowestQtricks = 0;

  result = true;

  if (thrd.nodeTypeStore[hand] == MAXNODE)
    cutoff = target - tpos.tricksMAX;
  else
    cutoff = tpos.tricksMAX - target + (depth >> 2) + 2;

  bool commPartner = false;
  const unsigned short (* ris)[DDS_SUITS] = tpos.rankInSuit;
  const unsigned char (* len)[DDS_SUITS] = tpos.length;
  highCardType const * winner = tpos.winner;

  for (int s = 0; s < DDS_SUITS; s++)
  {
    if ((trump != DDS_NOTRUMP) && (trump != s))
    {
      /* Trump game, and we lead a non-trump suit */
      if (winner[s].hand == partner[hand])
      {
        /* Partner has winning card */
        if (ris[hand][s] != 0 && /* Own hand has card */
            (((ris[lho[hand]][s] != 0) || /* LHO not void */
              (ris[lho[hand]][trump] == 0)) && /* LHO no trump */
             ((ris[rho[hand]][s] != 0) || /* RHO not void */
              (ris[rho[hand]][trump] == 0)))) /* RHO no trump */
        {
          commPartner = true;
          commSuit = s;
          commRank = winner[s].rank;
          break;
        }
      }
      else if ((tpos.secondBest[s].hand == partner[hand]) &&
               (winner[s].hand == hand) &&
               (len[hand][s] >= 2) &&
               (len[partner[hand]][s] >= 2))
      {
        /* Can cross to partner's card: Type Kx opposite Ax */
        if (((ris[lho[hand]][s] != 0) || /* LHO not void */
             (ris[lho[hand]][trump] == 0)) /* LHO no trump */
            && ((ris[rho[hand]][s] != 0) || /* RHO not void */
                (ris[rho[hand]][trump] == 0))) /* RHO no trump */
        {
          commPartner = true;
          commSuit = s;
          commRank = tpos.secondBest[s].rank;
          break;
        }
      }
    }
    else if (trump == DDS_NOTRUMP)
    {
      if (winner[s].hand == partner[hand])
      {
        /* Partner has winning card in NT */
        if (ris[hand][s] != 0) /* Own hand has card */
        {
          commPartner = true;
          commSuit = s;
          commRank = winner[s].rank;
          break;
        }
      }
      else if ((tpos.secondBest[s].hand == partner[hand]) &&
               (winner[s].hand == hand) &&
               (len[hand][s] >= 2) &&
               (len[partner[hand]][s] >= 2))
      {
        /* Can cross to partner's card: Type Kx opposite Ax */
        commPartner = true;
        commSuit = s;
        commRank = tpos.secondBest[s].rank;
        break;
      }
    }
  }

  if ((trump != DDS_NOTRUMP) && (!commPartner) &&
      (ris[hand][trump] != 0) &&
      (winner[trump].hand == partner[hand]))
  {
    /* Communication in trump suit */
    commPartner = true;
    commSuit = trump;
    commRank = winner[trump].rank;
  }

  if (trump != DDS_NOTRUMP)
  {
    suit = trump;
    lhoTrumpRanks = len[lho[hand]][trump];
    rhoTrumpRanks = len[rho[hand]][trump];
  }
  else
    suit = 0;

  // Build the context struct for sub-function calls (§9.1).
  QtricksContext ctx = {
    tpos, thrd, hand, depth, cutoff, trump,
    suit, /*qtricks=*/0,
    /*countOwn=*/0, /*countLho=*/0, /*countRho=*/0, /*countPart=*/0,
    lhoTrumpRanks, rhoTrumpRanks,
    commSuit, commRank, commPartner
  };

  do
  {
    ctx.countOwn = len[hand][ctx.suit];
    ctx.countLho = len[lho[hand]][ctx.suit];
    ctx.countRho = len[rho[hand]][ctx.suit];
    ctx.countPart = len[partner[hand]][ctx.suit];
    int opps = ctx.countLho | ctx.countRho;

    if (!opps && (ctx.countPart == 0))
    {
      if (ctx.countOwn == 0)
      {
        /* Continue with next suit. */
        ctx.suit = advanceSuit(ctx.suit, ctx.trump);
        continue;
      }

      /* Long tricks when only leading hand have cards in the suit. */
      if ((trump != DDS_NOTRUMP) && (trump != ctx.suit))
      {
        if ((ctx.lhoTrumpRanks == 0) && (ctx.rhoTrumpRanks == 0))
        {
          ctx.qtricks += ctx.countOwn;
          if (ctx.qtricks >= cutoff)
            return ctx.qtricks;
        }
        ctx.suit = nextSuitSkipTrump(ctx.suit, trump);
        continue;
      }
      else
      {
        ctx.qtricks += ctx.countOwn;
        if (ctx.qtricks >= cutoff)
          return ctx.qtricks;

        ctx.suit = advanceSuit(ctx.suit, trump);
        continue;
      }
    }
    else
    {
      if (!opps && (trump != DDS_NOTRUMP) && (ctx.suit == trump))
      {
        /* The partner but not the opponents have cards in
           the trump suit. */

        int sum = std::max(ctx.countOwn, ctx.countPart);
        for (int s = 0; s < DDS_SUITS; s++)
        {
          if ((sum > 0) &&
              (s != trump) &&
              (ctx.countOwn >= ctx.countPart) &&
              (len[hand][s] > 0) &&
              (len[partner[hand]][s] == 0))
          {
            sum++;
            break;
          }
        }
        /* If the additional trick by ruffing causes a cutoff.
           (ctx.qtricks not incremented.) */
        if (sum >= cutoff)
          return sum;
      }
      else if (!opps)
      {
        /* The partner but not the opponents have cards in the suit. */
        int sum = std::min(ctx.countOwn, ctx.countPart);
        if (trump == DDS_NOTRUMP)
        {
          if (sum >= cutoff)
            return sum;
        }
        else if ((ctx.suit != trump) &&
                 (ctx.lhoTrumpRanks == 0) &&
                 (ctx.rhoTrumpRanks == 0))
        {
          if (sum >= cutoff)
            return sum;
        }
      }

      if (ctx.commPartner)
      {
        if (!opps && (ctx.countOwn == 0))
        {
          if ((trump != DDS_NOTRUMP) && (trump != ctx.suit))
          {
            if ((ctx.lhoTrumpRanks == 0) && (ctx.rhoTrumpRanks == 0))
            {
              ctx.qtricks += ctx.countPart;
              tpos.winRanks[depth][ctx.commSuit] |=
                bitMapRank[ctx.commRank];

              if (ctx.qtricks >= cutoff)
                return ctx.qtricks;
            }
            ctx.suit = nextSuitSkipTrump(ctx.suit, trump);
            continue;
          }
          else
          {
            ctx.qtricks += ctx.countPart;
            tpos.winRanks[depth][ctx.commSuit] |=
              bitMapRank[ctx.commRank];

            if (ctx.qtricks >= cutoff)
              return ctx.qtricks;

            ctx.suit = advanceSuit(ctx.suit, trump);
            continue;
          }
        }
        else
        {
          if (!opps && (trump != DDS_NOTRUMP) && (ctx.suit == trump))
          {
            int sum = std::max(ctx.countOwn, ctx.countPart);
            for (int s = 0; s < DDS_SUITS; s++)
            {
              if ((sum > 0) &&
                  (s != trump) &&
                  (ctx.countOwn <= ctx.countPart) &&
                  (len[partner[hand]][s] > 0) &&
                  (len[hand][s] == 0))
              {
                sum++;
                break;
              }
            }
            if (sum >= cutoff)
            {
              tpos.winRanks[depth][ctx.commSuit] |=
                bitMapRank[ctx.commRank];
              return sum;
            }
          }
          else if (!opps)
          {
            int sum = std::min(ctx.countOwn, ctx.countPart);
            if (trump == DDS_NOTRUMP)
            {
              if (sum >= cutoff)
                return sum;
            }
            else if ((ctx.suit != trump) &&
                     (ctx.lhoTrumpRanks == 0) &&
                     (ctx.rhoTrumpRanks == 0))
            {
              if (sum >= cutoff)
                return sum;
            }
          }
        }
      }
    }

    if (winner[ctx.suit].rank == 0)
    {
      ctx.suit = advanceSuit(ctx.suit, trump);
      continue;
    }

    if (winner[ctx.suit].hand == hand)
    {
      if ((trump != DDS_NOTRUMP) && (trump != ctx.suit))
      {
        QtricksResult r = QtricksLeadHandTrump(ctx);
        ctx.qtricks = r.qtricks;

        if (r.action == 1)
          return ctx.qtricks;
        else if (r.action == 2)
        {
          ctx.suit = nextSuitSkipTrump(ctx.suit, trump);
          continue;
        }
      }
      else
      {
        QtricksResult r = QtricksLeadHandNT(ctx);
        ctx.qtricks = r.qtricks;

        if (r.action == 1)
          return ctx.qtricks;
        else if (r.action == 2)
        {
          ctx.suit = advanceSuit(ctx.suit, trump);
          continue;
        }
      }
    }

    /* It was not possible to take a quick trick by own winning
       card in the suit */
    else
    {
      /* Partner winning card? */
      if (winner[ctx.suit].hand == partner[hand])
      {
        /* Winner found at partner*/
        if (ctx.commPartner)
        {
          /* There is communication with the partner */
          if ((trump != DDS_NOTRUMP) && (trump != ctx.suit))
          {
            QtricksResult r = QuickTricksPartnerHandTrump(ctx);
            ctx.qtricks = r.qtricks;

            if (r.action == 1)
              return ctx.qtricks;
            else if (r.action == 2)
            {
              ctx.suit = nextSuitSkipTrump(ctx.suit, trump);
              continue;
            }
          }
          else
          {
            QtricksResult r = QuickTricksPartnerHandNT(ctx);
            ctx.qtricks = r.qtricks;

            if (r.action == 1)
              return ctx.qtricks;
            else if (r.action == 2)
            {
              ctx.suit = advanceSuit(ctx.suit, trump);
              continue;
            }
          }
        }
      }
    }
    if ((trump != DDS_NOTRUMP) && (ctx.suit != trump) &&
        (ctx.countOwn > 0) && (lowestQtricks == 0) &&
        ((ctx.qtricks == 0) ||
         ((winner[ctx.suit].hand != hand) &&
          (winner[ctx.suit].hand != partner[hand]) &&
          (winner[trump].hand != hand) &&
          (winner[trump].hand != partner[hand]))))
    {
      if ((ctx.countPart == 0) && (len[partner[hand]][trump] > 0))
      {
        if (((ctx.countRho > 0) || (len[rho[hand]][trump] == 0)) &&
            ((ctx.countLho > 0) || (len[lho[hand]][trump] == 0)))
        {
          lowestQtricks = 1;
          if (1 >= cutoff)
            return 1;
          ctx.suit = nextSuitSkipTrump(ctx.suit, trump);
          continue;
        }
        else if ((ctx.countRho == 0) && (ctx.countLho == 0))
        {
          if ((ris[lho[hand]][trump] |
               ris[rho[hand]][trump]) <
              ris[partner[hand]][trump])
          {
            lowestQtricks = 1;

            int rr = highestRankFast(ris[partner[hand]][trump]);
            if (rr != 0)
            {
              tpos.winRanks[depth][trump] |= bitMapRank[rr];
              if (1 >= cutoff)
                return 1;
            }
          }
          ctx.suit = nextSuitSkipTrump(ctx.suit, trump);
          continue;
        }
        else if (ctx.countLho == 0)
        {
          if (ris[lho[hand]][trump] <
              ris[partner[hand]][trump])
          {
            lowestQtricks = 1;
            for (int rr = 14; rr >= 2; rr--)
            {
              if ((ris[partner[hand]][trump] & bitMapRank[rr]) != 0)
              {
                tpos.winRanks[depth][trump] |= bitMapRank[rr];
                break;
              }
            }
            if (1 >= cutoff)
              return 1;
          }
          ctx.suit = nextSuitSkipTrump(ctx.suit, trump);
          continue;
        }
        else if (ctx.countRho == 0)
        {
          if (ris[rho[hand]][trump] <
              ris[partner[hand]][trump])
          {
            lowestQtricks = 1;
            for (int rr = 14; rr >= 2; rr--)
            {
              if ((ris[partner[hand]][trump] & bitMapRank[rr]) != 0)
              {
                tpos.winRanks[depth][trump] |= bitMapRank[rr];
                break;
              }
            }
            if (1 >= cutoff)
              return 1;
          }
          ctx.suit = nextSuitSkipTrump(ctx.suit, trump);
          continue;
        }
      }
    }

    if (ctx.qtricks >= cutoff)
      return ctx.qtricks;

    ctx.suit = advanceSuit(ctx.suit, trump);
  }
  while (ctx.suit <= 3);

  if (ctx.qtricks == 0)
  {
    if ((trump == DDS_NOTRUMP) || (winner[trump].hand == -1))
    {
      for (int ss = 0; ss < DDS_SUITS; ss++)
      {
        if (winner[ss].hand == -1)
          continue;
        if (len[hand][ss] > 0)
        {
          tpos.winRanks[depth][ss] = bitMapRank[winner[ss].rank];
        }
      }

      int cutoff2;
      if (thrd.nodeTypeStore[hand] != MAXNODE)
        cutoff2 = target - tpos.tricksMAX;
      else
      {
        cutoff2 = tpos.tricksMAX - target + (depth >> 2) + 2;
      }

      if (1 >= cutoff2)
        return 0;
    }
  }

  result = false;
  return ctx.qtricks;
}


QtricksResult QtricksLeadHandTrump(QtricksContext& ctx)
{
  /* action=0 Continue with same suit.
     action=1 Cutoff.
     action=2 Continue with next suit. */

  int qt = ctx.qtricks;
  if (((ctx.countLho != 0) ||
       (ctx.lhoTrumpRanks == 0)) &&
      ((ctx.countRho != 0) || (ctx.rhoTrumpRanks == 0)))
  {
    ctx.tpos.winRanks[ctx.depth][ctx.suit] |=
      bitMapRank[ctx.tpos.winner[ctx.suit].rank];
    qt++;
    if (qt >= ctx.cutoff)
      return {qt, 1};

    if ((ctx.countLho <= 1) &&
        (ctx.countRho <= 1) &&
        (ctx.countPart <= 1) &&
        (ctx.lhoTrumpRanks == 0) &&
        (ctx.rhoTrumpRanks == 0))
    {
      qt += ctx.countOwn - 1;
      if (qt >= ctx.cutoff)
        return {qt, 1};
      return {qt, 2};
    }
  }

  if (ctx.tpos.secondBest[ctx.suit].hand == ctx.hand)
  {
    if ((ctx.lhoTrumpRanks == 0) && (ctx.rhoTrumpRanks == 0))
    {
      ctx.tpos.winRanks[ctx.depth][ctx.suit] |=
        bitMapRank[ctx.tpos.secondBest[ctx.suit].rank];
      qt++;
      if (qt >= ctx.cutoff)
        return {qt, 1};
      if ((ctx.countLho <= 2) && (ctx.countRho <= 2) &&
          (ctx.countPart <= 2))
      {
        qt += ctx.countOwn - 2;
        if (qt >= ctx.cutoff)
          return {qt, 1};
        return {qt, 2};
      }
    }
  }
  else if ((ctx.tpos.secondBest[ctx.suit].hand == partner[ctx.hand])
           && (ctx.countOwn > 1) && (ctx.countPart > 1))
  {
    /* Second best at partner and suit length of own
       hand and partner > 1 */
    if ((ctx.lhoTrumpRanks == 0) && (ctx.rhoTrumpRanks == 0))
    {
      ctx.tpos.winRanks[ctx.depth][ctx.suit] |=
        bitMapRank[ctx.tpos.secondBest[ctx.suit].rank];
      qt++;
      if (qt >= ctx.cutoff)
        return {qt, 1};
      if ((ctx.countLho <= 2) &&
          (ctx.countRho <= 2) &&
          ((ctx.countPart <= 2) || (ctx.countOwn <= 2)))
      {
        qt += std::max(ctx.countOwn - 2, ctx.countPart - 2);
        if (qt >= ctx.cutoff)
          return {qt, 1};
        return {qt, 2};
      }
    }
  }
  return {qt, 0};
}

QtricksResult QtricksLeadHandNT(QtricksContext& ctx)
{
  /* action=0 Continue with same suit.
     action=1 Cutoff.
     action=2 Continue with next suit. */

  int qt = ctx.qtricks;
  ctx.tpos.winRanks[ctx.depth][ctx.suit] |=
    bitMapRank[ctx.tpos.winner[ctx.suit].rank];

  qt++;
  if (qt >= ctx.cutoff)
    return {qt, 1};
  if ((ctx.trump == ctx.suit) &&
      ((!ctx.commPartner) || (ctx.suit != ctx.commSuit)))
  {
    ctx.lhoTrumpRanks = std::max(0, ctx.lhoTrumpRanks - 1);
    ctx.rhoTrumpRanks = std::max(0, ctx.rhoTrumpRanks - 1);
  }

  if ((ctx.countLho <= 1) && (ctx.countRho <= 1) &&
      (ctx.countPart <= 1))
  {
    qt += ctx.countOwn - 1;
    if (qt >= ctx.cutoff)
      return {qt, 1};
    return {qt, 2};
  }

  if (ctx.tpos.secondBest[ctx.suit].hand == ctx.hand)
  {
    ctx.tpos.winRanks[ctx.depth][ctx.suit] |=
      bitMapRank[ctx.tpos.secondBest[ctx.suit].rank];
    qt++;
    if (qt >= ctx.cutoff)
      return {qt, 1};
    if ((ctx.trump == ctx.suit) &&
        ((!ctx.commPartner) || (ctx.suit != ctx.commSuit)))
    {
      ctx.lhoTrumpRanks = std::max(0, ctx.lhoTrumpRanks - 1);
      ctx.rhoTrumpRanks = std::max(0, ctx.rhoTrumpRanks - 1);
    }
    if ((ctx.countLho <= 2) && (ctx.countRho <= 2) &&
        (ctx.countPart <= 2))
    {
      qt += ctx.countOwn - 2;
      if (qt >= ctx.cutoff)
        return {qt, 1};
      return {qt, 2};
    }
  }
  else if ((ctx.tpos.secondBest[ctx.suit].hand == partner[ctx.hand])
           && (ctx.countOwn > 1) && (ctx.countPart > 1))
  {
    /* Second best at partner and suit length of own
       hand and partner > 1 */
    ctx.tpos.winRanks[ctx.depth][ctx.suit] |=
      bitMapRank[ctx.tpos.secondBest[ctx.suit].rank];
    qt++;
    if (qt >= ctx.cutoff)
      return {qt, 1};
    if ((ctx.trump == ctx.suit) &&
        ((!ctx.commPartner) || (ctx.suit != ctx.commSuit)))
    {
      ctx.lhoTrumpRanks = std::max(0, ctx.lhoTrumpRanks - 1);
      ctx.rhoTrumpRanks = std::max(0, ctx.rhoTrumpRanks - 1);
    }
    if ((ctx.countLho <= 2) &&
        (ctx.countRho <= 2) &&
        ((ctx.countPart <= 2) || (ctx.countOwn <= 2)))
    {
      qt += std::max(ctx.countOwn - 2, ctx.countPart - 2);
      if (qt >= ctx.cutoff)
        return {qt, 1};
      return {qt, 2};
    }
  }

  return {qt, 0};
}


QtricksResult QuickTricksPartnerHandTrump(QtricksContext& ctx)
{
  /* action=0 Continue with same suit.
     action=1 Cutoff.
     action=2 Continue with next suit. */

  int qt = ctx.qtricks;
  if (((ctx.countLho != 0) || (ctx.lhoTrumpRanks == 0)) &&
      ((ctx.countRho != 0) || (ctx.rhoTrumpRanks == 0)))
  {
    ctx.tpos.winRanks[ctx.depth][ctx.suit] |=
      bitMapRank[ctx.tpos.winner[ctx.suit].rank];

    ctx.tpos.winRanks[ctx.depth][ctx.commSuit] |=
      bitMapRank[ctx.commRank];

    qt++; /* A trick can be taken */
    if (qt >= ctx.cutoff)
      return {qt, 1};
    if ((ctx.countLho <= 1) &&
        (ctx.countRho <= 1) &&
        (ctx.countOwn <= 1) &&
        (ctx.lhoTrumpRanks == 0) &&
        (ctx.rhoTrumpRanks == 0))
    {
      qt += ctx.countPart - 1;
      if (qt >= ctx.cutoff)
        return {qt, 1};
      return {qt, 2};
    }
  }

  if (ctx.tpos.secondBest[ctx.suit].hand == partner[ctx.hand])
  {
    /* Second best found in partners hand */
    if ((ctx.lhoTrumpRanks == 0) && (ctx.rhoTrumpRanks == 0))
    {
      /* Opponents have no trump */
      ctx.tpos.winRanks[ctx.depth][ctx.suit] |=
        bitMapRank[ctx.tpos.secondBest[ctx.suit].rank];

      ctx.tpos.winRanks[ctx.depth][ctx.commSuit] |=
        bitMapRank[ctx.commRank];
      qt++;
      if (qt >= ctx.cutoff)
        return {qt, 1};
      if ((ctx.countLho <= 2) && (ctx.countRho <= 2) &&
          (ctx.countOwn <= 2))
      {
        qt += ctx.countPart - 2;
        if (qt >= ctx.cutoff)
          return {qt, 1};
        return {qt, 2};
      }
    }
  }
  else if ((ctx.tpos.secondBest[ctx.suit].hand == ctx.hand) &&
           (ctx.countPart > 1) &&
           (ctx.countOwn > 1))
  {
    /* Second best found in own hand and suit lengths of own hand
       and partner > 1*/

    if ((ctx.lhoTrumpRanks == 0) && (ctx.rhoTrumpRanks == 0))
    {
      /* Opponents have no trump */
      ctx.tpos.winRanks[ctx.depth][ctx.suit] |=
        bitMapRank[ctx.tpos.secondBest[ctx.suit].rank];

      ctx.tpos.winRanks[ctx.depth][ctx.commSuit] |=
        bitMapRank[ctx.commRank];

      qt++;
      if (qt >= ctx.cutoff)
        return {qt, 1};
      if ((ctx.countLho <= 2) &&
          (ctx.countRho <= 2) &&
          ((ctx.countOwn <= 2) || (ctx.countPart <= 2)))
      {
        qt += std::max(ctx.countPart - 2, ctx.countOwn - 2);
        if (qt >= ctx.cutoff)
          return {qt, 1};
        return {qt, 2};
      }
    }
  }
  else if ((ctx.suit == ctx.commSuit) &&
           (ctx.tpos.secondBest[ctx.suit].hand == lho[ctx.hand]) &&
           ((ctx.countLho >= 2) || (ctx.lhoTrumpRanks == 0)) &&
           ((ctx.countRho >= 2) || (ctx.rhoTrumpRanks == 0)))
  {
    unsigned short ranks = 0;
    for (int h = 0; h < DDS_HANDS; h++)
      ranks |= ctx.tpos.rankInSuit[h][ctx.suit];

    if (ctx.thrd.rel[ranks].absRank[3][ctx.suit].hand ==
        partner[ctx.hand])
    {
      ctx.tpos.winRanks[ctx.depth][ctx.suit] |= bitMapRank[
        static_cast<int>(
          ctx.thrd.rel[ranks].absRank[3][ctx.suit].rank) ];

      ctx.tpos.winRanks[ctx.depth][ctx.commSuit] |=
        bitMapRank[ctx.commRank];

      qt++;
      if (qt >= ctx.cutoff)
        return {qt, 1};
      if ((ctx.countOwn <= 2) &&
          (ctx.countLho <= 2) &&
          (ctx.countRho <= 2) &&
          (ctx.lhoTrumpRanks == 0) &&
          (ctx.rhoTrumpRanks == 0))
      {
        qt += ctx.countPart - 2;
        if (qt >= ctx.cutoff)
          return {qt, 1};
      }
    }
  }
  return {qt, 0};
}


QtricksResult QuickTricksPartnerHandNT(QtricksContext& ctx)
{
  int qt = ctx.qtricks;

  ctx.tpos.winRanks[ctx.depth][ctx.suit] |=
    bitMapRank[ctx.tpos.winner[ctx.suit].rank];

  ctx.tpos.winRanks[ctx.depth][ctx.commSuit] |=
    bitMapRank[ctx.commRank];

  qt++;
  if (qt >= ctx.cutoff)
    return {qt, 1};
  if ((ctx.countLho <= 1) && (ctx.countRho <= 1) &&
      (ctx.countOwn <= 1))
  {
    qt += ctx.countPart - 1;
    if (qt >= ctx.cutoff)
      return {qt, 1};
    return {qt, 2};
  }

  if (ctx.tpos.secondBest[ctx.suit].hand == partner[ctx.hand])
  {
    /* Second best found in partners hand */
    ctx.tpos.winRanks[ctx.depth][ctx.suit] |=
      bitMapRank[ctx.tpos.secondBest[ctx.suit].rank];

    qt++;
    if (qt >= ctx.cutoff)
      return {qt, 1};
    if ((ctx.countLho <= 2) && (ctx.countRho <= 2) &&
        (ctx.countOwn <= 2))
    {
      qt += ctx.countPart - 2;
      if (qt >= ctx.cutoff)
        return {qt, 1};
      return {qt, 2};
    }
  }
  else if ((ctx.tpos.secondBest[ctx.suit].hand == ctx.hand)
           && (ctx.countPart > 1) && (ctx.countOwn > 1))
  {
    /* Second best found in own hand and own and
       partner's suit length > 1 */
    ctx.tpos.winRanks[ctx.depth][ctx.suit] |=
      bitMapRank[ctx.tpos.secondBest[ctx.suit].rank];

    qt++;
    if (qt >= ctx.cutoff)
      return {qt, 1};
    if ((ctx.countLho <= 2) &&
        (ctx.countRho <= 2) &&
        ((ctx.countOwn <= 2) || (ctx.countPart <= 2)))
    {
      qt += std::max(ctx.countPart - 2, ctx.countOwn - 2);
      if (qt >= ctx.cutoff)
        return {qt, 1};
      return {qt, 2};
    }
  }
  else if ((ctx.suit == ctx.commSuit) &&
           (ctx.tpos.secondBest[ctx.suit].hand == lho[ctx.hand]))
  {
    unsigned short ranks = 0;
    for (int h = 0; h < DDS_HANDS; h++)
      ranks |= ctx.tpos.rankInSuit[h][ctx.suit];

    if (ctx.thrd.rel[ranks].absRank[3][ctx.suit].hand ==
        partner[ctx.hand])
    {
      ctx.tpos.winRanks[ctx.depth][ctx.suit] |= bitMapRank[
        static_cast<int>(
          ctx.thrd.rel[ranks].absRank[3][ctx.suit].rank) ];
      qt++;
      if (qt >= ctx.cutoff)
        return {qt, 1};
      if ((ctx.countOwn <= 2) && (ctx.countLho <= 2) &&
          (ctx.countRho <= 2))
      {
        // TODO: Is the fix to qt correct?
        // qtricks += countPart - 2;
        qt += ctx.countPart - 2;
        if (qt >= ctx.cutoff)
          return {qt, 1};
      }
    }
  }
  return {qt, 0};
}


bool QuickTricksSecondHand(
  pos& tpos,
  const int hand,
  const int depth,
  const int target,
  const int trump,
  const ThreadData& thrd)
{
  if (depth == thrd.iniDepth)
    return false;

  int ss = tpos.move[depth + 1].suit;
  unsigned short (*ris)[DDS_SUITS] = tpos.rankInSuit;
  unsigned short ranks = static_cast<unsigned short>
                         (ris[hand][ss] | ris[partner[hand]][ss]);

  for (int s = 0; s < DDS_SUITS; s++)
    tpos.winRanks[depth][s] = 0;

  if ((trump != DDS_NOTRUMP) && (ss != trump) &&
      (((ris[hand][ss] == 0) && (ris[hand][trump] != 0)) ||
       ((ris[partner[hand]][ss] == 0) &&
        (ris[partner[hand]][trump] != 0))))
  {
    if ((ris[lho[hand]][ss] == 0) &&
        (ris[lho[hand]][trump] != 0))
      return false;

    /* Own side can ruff, their side can't. */
  }

  else if (ranks > (bitMapRank[tpos.move[depth + 1].rank] |
                    ris[lho[hand]][ss]))
  {
    if ((trump != DDS_NOTRUMP) && (ss != trump) &&
        (ris[lho[hand]][trump] != 0) &&
        (ris[lho[hand]][ss] == 0))
      return false;

    /* Own side has highest card in suit, which LHO can't ruff. */

    int rr = highestRankFast(ranks);
    tpos.winRanks[depth][ss] = bitMapRank[rr];
  }
  else
  {
    /* No easy way to win current trick for own side. */
    return false;
  }

  int qtricks = 1;

  int cutoff;
  if (thrd.nodeTypeStore[hand] == MAXNODE)
    cutoff = target - tpos.tricksMAX;
  else
    cutoff = tpos.tricksMAX - target + (depth >> 2) + 3;

  if (qtricks >= cutoff)
    return true;

  if (trump != DDS_NOTRUMP)
    return false;

  /* In NT, second winner (by rank) in same suit. */

  int hh;
  if (ris[hand][ss] > ris[partner[hand]][ss])
    hh = hand; /* Hand to lead next trick */
  else
    hh = partner[hand];

  if ((tpos.winner[ss].hand == hh) &&
      (tpos.secondBest[ss].rank != 0) &&
      (tpos.secondBest[ss].hand == hh))
  {
    qtricks++;
    tpos.winRanks[depth][ss] |=
      bitMapRank[tpos.secondBest[ss].rank];

    if (qtricks >= cutoff)
      return true;
  }

  for (int s = 0; s < DDS_SUITS; s++)
  {
    if ((s == ss) || (tpos.length[hh][s] == 0))
      continue;

    if ((tpos.length[lho[hh]][s] == 0) &&
        (tpos.length[rho[hh]][s] == 0) &&
        (tpos.length[partner[hh]][s] == 0))
    {
      /* Long other suit which nobody else holds. */
      qtricks += counttable[ris[hh][s]];
      if (qtricks >= cutoff)
        return true;
    }
    else if ((tpos.winner[s].rank != 0) &&
             (tpos.winner[s].hand == hh))
    {
      /* Top winners in other suits. */
      qtricks++;
      tpos.winRanks[depth][s] |=
        bitMapRank[tpos.winner[s].rank];

      if (qtricks >= cutoff)
        return true;
    }
  }

  return false;
}

