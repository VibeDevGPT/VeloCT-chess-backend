// Automatically generated single-file component
#define UNIFIED_BUILD


// ==== START OF FILE: attacks.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "attacks.h"

#include <array>

#include "misc.h"

namespace VeloCT::Attacks {

namespace {

Bitboard LineBB[SQUARE_NB][SQUARE_NB];
Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
Bitboard RayPassBB[SQUARE_NB][SQUARE_NB];

#ifdef USE_DUAL_HYPERBOLA_QUINT
alignas(64) DualMagic DualMagics[SQUARE_NB];
#else
alignas(64) Magic Magics[SQUARE_NB][2];
#endif

}

[[maybe_unused]] static Bitboard line_mask(Square sq, Direction d1, Direction d2) {
    Bitboard mask = 0, dest;
    for (Direction d : {d1, d2})
    {
        Square s = sq;
        while ((dest = safe_destination(s, d)))
        {
            mask |= dest;
            s += d;
        }
    }
    return mask;
}

#ifdef USE_HYPERBOLA_QUINT
static void init_magics(Magic magics[][2]) {
    for (Square s = SQ_A1; s <= SQ_H8; ++s)
    {
        Magic& rook = magics[s][ROOK - BISHOP];
        rook.mask1  = line_mask(s, NORTH, SOUTH);
        rook.mask2  = line_mask(s, EAST, WEST);

        Magic& bishop = magics[s][BISHOP - BISHOP];
        bishop.mask1  = line_mask(s, NORTH_EAST, SOUTH_WEST);
        bishop.mask2  = line_mask(s, NORTH_WEST, SOUTH_EAST);
    }
}

#elif defined(USE_DUAL_HYPERBOLA_QUINT)

// Sliding attacks within a rank, indexed by the slider's file and the
// 8-bit rank occupancy, yielding the 8-bit attack set on that rank
constexpr auto RankAttacks = []() {
    std::array<std::array<u8, 256>, FILE_NB> table{};
    for (int file = 0; file < 8; ++file)
        for (int occ = 0; occ < 256; ++occ)
            table[file][occ] = u8(sliding_attack(ROOK, Square(file), occ));
    return table;
}();

static void init_dual_magics(DualMagic magics[]) {
    for (Square s = SQ_A1; s <= SQ_H8; ++s)
    {
        DualMagic& m        = magics[s];
        m.maskFile          = line_mask(s, NORTH, SOUTH);
        m.maskDiag          = line_mask(s, NORTH_EAST, SOUTH_WEST);
        m.maskNone          = 0;
        m.maskAntidiag      = line_mask(s, NORTH_WEST, SOUTH_EAST);
        m.r                 = square_bb(s) * 2;
        m.rr                = square_bb(Square(63 - int(s))) * 2;
        m.rankAttacksLookup = RankAttacks[int(file_of(s))].data();
        m.shift             = 8 * int(rank_of(s));
    }
}

#else

namespace {
void init_magics(PieceType pt, Bitboard table[], Magic magics[][2]) {

    int seeds[][RANK_NB] = {{8977, 44560, 54343, 38998, 5731, 95205, 104912, 17020},
                            {728, 10316, 55013, 32803, 12281, 15100, 16645, 255}};

    Bitboard occupancy[4096];
    int      epoch[4096] = {}, cnt = 0;
    Bitboard reference[4096] = {};
    int      size            = 0;

    for (Square s = SQ_A1; s <= SQ_H8; ++s)
    {
        Bitboard edges = ((Rank1BB | Rank8BB) & ~rank_bb(s)) | ((FileABB | FileHBB) & ~file_bb(s));

        Magic&   m       = magics[s][pt - BISHOP];
        Bitboard attacks = sliding_attack(pt, s, 0);
        m.mask           = attacks & ~edges;
        m.shift          = (Is64Bit ? 64 : 32) - popcount(m.mask);
        m.attacks        = s == SQ_A1 ? table : magics[s - 1][pt - BISHOP].attacks + size;
        size             = 0;

        Bitboard b = 0;
        do
        {
            occupancy[size] = b;
            reference[size] = sliding_attack(pt, s, b);

            size++;
            b = (b - m.mask) & m.mask;
        } while (b);

        PRNG rng(seeds[Is64Bit][rank_of(s)]);

        for (int i = 0; i < size;)
        {
            for (m.magic = 0; popcount((m.magic * m.mask) >> 56) < 6;)
                m.magic = rng.sparse_rand<Bitboard>();

            for (++cnt, i = 0; i < size; ++i)
            {
                unsigned idx = m.index(occupancy[i]);

                if (epoch[idx] < cnt)
                {
                    epoch[idx]     = cnt;
                    m.attacks[idx] = reference[i];
                }
                else if (m.attacks[idx] != reference[i])
                    break;
            }
        }
    }
}

    #if !defined(USE_DUAL_HYPERBOLA_QUINT) && !defined(USE_HYPERBOLA_QUINT)
static std::array<Bitboard, 0x19000> RookTable;
static std::array<Bitboard, 0x1480>  BishopTable;
    #endif
}

#endif

void init() {

#ifdef USE_HYPERBOLA_QUINT
    init_magics(Magics);
#elif defined(USE_DUAL_HYPERBOLA_QUINT)
    init_dual_magics(DualMagics);
#else
    init_magics(ROOK, RookTable.data(), Magics);
    init_magics(BISHOP, BishopTable.data(), Magics);
#endif

    for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1)
    {
        for (PieceType pt : {BISHOP, ROOK})
            for (Square s2 = SQ_A1; s2 <= SQ_H8; ++s2)
            {
                if (PseudoAttacks[pt][s1] & s2)
                {
                    LineBB[s1][s2] = (attacks_bb(pt, s1, 0) & attacks_bb(pt, s2, 0)) | s1 | s2;
                    BetweenBB[s1][s2] =
                      (attacks_bb(pt, s1, square_bb(s2)) & attacks_bb(pt, s2, square_bb(s1)));
                    RayPassBB[s1][s2] =
                      attacks_bb(pt, s1, 0) & (attacks_bb(pt, s2, square_bb(s1)) | s2);
                }
                BetweenBB[s1][s2] |= s2;
            }
    }
}

#ifdef USE_DUAL_HYPERBOLA_QUINT
const DualMagic& dual_magic(Square s) { return DualMagics[s]; }
#else
const Magic& magic(Square s, PieceType pt) {
    assert((pt == BISHOP || pt == ROOK) && is_ok(s));
    return Magics[s][pt - BISHOP];
}
#endif

Bitboard line_bb(Square s1, Square s2) {
    assert(is_ok(s1) && is_ok(s2));
    return LineBB[s1][s2];
}

Bitboard between_bb(Square s1, Square s2) {
    assert(is_ok(s1) && is_ok(s2));
    return BetweenBB[s1][s2];
}

Bitboard ray_pass_bb(Square s1, Square s2) {
    assert(is_ok(s1) && is_ok(s2));
    return RayPassBB[s1][s2];
}

}  // namespace VeloCT::Attacks

// ==== END OF FILE: attacks.cpp ====

// ==== START OF FILE: benchmark.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "benchmark.h"
#include "numa.h"
#include "misc.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

// clang-format off
const std::vector<std::string> Defaults = {
  "setoption name UCI_Chess960 value false",
  "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
  "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 10",
  "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 11",
  "4rrk1/pp1n3p/3q2pQ/2p1pb2/2PP4/2P3N1/P2B2PP/4RRK1 b - - 7 19",
  "rq3rk1/ppp2ppp/1bnpb3/3N2B1/3NP3/7P/PPPQ1PP1/2KR3R w - - 7 14 moves d4e6",
  "r1bq1r1k/1pp1n1pp/1p1p4/4p2Q/4Pp2/1BNP4/PPP2PPP/3R1RK1 w - - 2 14 moves g2g4",
  "r3r1k1/2p2ppp/p1p1bn2/8/1q2P3/2NPQN2/PPP3PP/R4RK1 b - - 2 15",
  "r1bbk1nr/pp3p1p/2n5/1N4p1/2Np1B2/8/PPP2PPP/2KR1B1R w kq - 0 13",
  "r1bq1rk1/ppp1nppp/4n3/3p3Q/3P4/1BP1B3/PP1N2PP/R4RK1 w - - 1 16",
  "4r1k1/r1q2ppp/ppp2n2/4P3/5Rb1/1N1BQ3/PPP3PP/R5K1 w - - 1 17",
  "2rqkb1r/ppp2p2/2npb1p1/1N1Nn2p/2P1PP2/8/PP2B1PP/R1BQK2R b KQ - 0 11",
  "r1bq1r1k/b1p1npp1/p2p3p/1p6/3PP3/1B2NN2/PP3PPP/R2Q1RK1 w - - 1 16",
  "3r1rk1/p5pp/bpp1pp2/8/q1PP1P2/b3P3/P2NQRPP/1R2B1K1 b - - 6 22",
  "r1q2rk1/2p1bppp/2Pp4/p6b/Q1PNp3/4B3/PP1R1PPP/2K4R w - - 2 18",
  "4k2r/1pb2ppp/1p2p3/1R1p4/3P4/2r1PN2/P4PPP/1R4K1 b - - 3 22",
  "3q2k1/pb3p1p/4pbp1/2r5/PpN2N2/1P2P2P/5PP1/Q2R2K1 b - - 4 26",
  "6k1/6p1/6Pp/ppp5/3pn2P/1P3K2/1PP2P2/3N4 b - - 0 1",
  "3b4/5kp1/1p1p1p1p/pP1PpP1P/P1P1P3/3KN3/8/8 w - - 0 1",
  "2K5/p7/7P/5pR1/8/5k2/r7/8 w - - 0 1 moves g5g6 f3e3 g6g5 e3f3",
  "8/6pk/1p6/8/PP3p1p/5P2/4KP1q/3Q4 w - - 0 1",
  "7k/3p2pp/4q3/8/4Q3/5Kp1/P6b/8 w - - 0 1",
  "8/2p5/8/2kPKp1p/2p4P/2P5/3P4/8 w - - 0 1",
  "8/1p3pp1/7p/5P1P/2k3P1/8/2K2P2/8 w - - 0 1",
  "8/pp2r1k1/2p1p3/3pP2p/1P1P1P1P/P5KR/8/8 w - - 0 1",
  "8/3p4/p1bk3p/Pp6/1Kp1PpPp/2P2P1P/2P5/5B2 b - - 0 1",
  "5k2/7R/4P2p/5K2/p1r2P1p/8/8/8 b - - 0 1",
  "6k1/6p1/P6p/r1N5/5p2/7P/1b3PP1/4R1K1 w - - 0 1",
  "1r3k2/4q3/2Pp3b/3Bp3/2Q2p2/1p1P2P1/1P2KP2/3N4 w - - 0 1",
  "6k1/4pp1p/3p2p1/P1pPb3/R7/1r2P1PP/3B1P2/6K1 w - - 0 1",
  "8/3p3B/5p2/5P2/p7/PP5b/k7/6K1 w - - 0 1",
  "5rk1/q6p/2p3bR/1pPp1rP1/1P1Pp3/P3B1Q1/1K3P2/R7 w - - 93 90",
  "4rrk1/1p1nq3/p7/2p1P1pp/3P2bp/3Q1Bn1/PPPB4/1K2R1NR w - - 40 21",
  "r3k2r/3nnpbp/q2pp1p1/p7/Pp1PPPP1/4BNN1/1P5P/R2Q1RK1 w kq - 0 16",
  "3Qb1k1/1r2ppb1/pN1n2q1/Pp1Pp1Pr/4P2p/4BP2/4B1R1/1R5K b - - 11 40",
  "4k3/3q1r2/1N2r1b1/3ppN2/2nPP3/1B1R2n1/2R1Q3/3K4 w - - 5 1",
  "1r6/1P4bk/3qr1p1/N6p/3pp2P/6R1/3Q1PP1/1R4K1 w - - 1 42",

  // Positions with high numbers of changed threats
  "k7/2n1n3/1nbNbn2/2NbRBn1/1nbRQR2/2NBRBN1/3N1N2/7K w - - 0 1",
  "K7/8/8/BNQNQNB1/N5N1/R1Q1q2r/n5n1/bnqnqnbk w - - 0 1",

  // 5-man positions
  "8/8/8/8/5kp1/P7/8/1K1N4 w - - 0 1",     // Kc2 - mate
  "8/8/8/5N2/8/p7/8/2NK3k w - - 0 1",      // Na2 - mate
  "8/3k4/8/8/8/4B3/4KB2/2B5 w - - 0 1",    // draw

  // 6-man positions
  "8/8/1P6/5pr1/8/4R3/7k/2K5 w - - 0 1",   // Re5 - mate
  "8/2p4P/8/kr6/6R1/8/8/1K6 w - - 0 1",    // Ka2 - mate
  "8/8/3P3k/8/1p6/8/1P6/1K3n2 b - - 0 1",  // Nd2 - draw

  // 7-man positions
  "8/R7/2q5/8/6k1/8/1P5p/K6R w - - 0 124", // Draw

  // Mate and stalemate positions
  "6k1/3b3r/1p1p4/p1n2p2/1PPNpP1q/P3Q1p1/1R1RB1P1/5K2 b - - 0 1",
  "r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1",
  "8/8/8/8/8/6k1/6p1/6K1 w - -",
  "7k/7P/6K1/8/3B4/8/8/8 b - -",

  // Chess 960
  "setoption name UCI_Chess960 value true",
  "bbqnnrkr/pppppppp/8/8/8/8/PPPPPPPP/BBQNNRKR w HFhf - 0 1 moves g2g3 d7d5 d2d4 c8h3 c1g5 e8d6 g5e7 f7f6",
  "nqbnrkrb/pppppppp/8/8/8/8/PPPPPPPP/NQBNRKRB w KQkq - 0 1",
  "setoption name UCI_Chess960 value false"
};
// clang-format on

// clang-format off
// human-randomly picked 5 games with <60 moves from
// https://tests.veloctchess.org/tests/view/665c71f9fd45fb0f907c21e0
// only moves for one side
const std::vector<std::vector<std::string>> BenchmarkPositions = {
    {
        "rnbq1k1r/ppp1bppp/4pn2/8/2B5/2NP1N2/PPP2PPP/R1BQR1K1 b - - 2 8",
        "rnbq1k1r/pp2bppp/4pn2/2p5/2B2B2/2NP1N2/PPP2PPP/R2QR1K1 b - - 1 9",
        "r1bq1k1r/pp2bppp/2n1pn2/2p5/2B1NB2/3P1N2/PPP2PPP/R2QR1K1 b - - 3 10",
        "r1bq1k1r/pp2bppp/2n1p3/2p5/2B1PB2/5N2/PPP2PPP/R2QR1K1 b - - 0 11",
        "r1b2k1r/pp2bppp/2n1p3/2p5/2B1PB2/5N2/PPP2PPP/3RR1K1 b - - 0 12",
        "r1b1k2r/pp2bppp/2n1p3/2p5/2B1PB2/2P2N2/PP3PPP/3RR1K1 b - - 0 13",
        "r1b1k2r/1p2bppp/p1n1p3/2p5/4PB2/2P2N2/PP2BPPP/3RR1K1 b - - 1 14",
        "r1b1k2r/4bppp/p1n1p3/1pp5/P3PB2/2P2N2/1P2BPPP/3RR1K1 b - - 0 15",
        "r1b1k2r/4bppp/p1n1p3/1P6/2p1PB2/2P2N2/1P2BPPP/3RR1K1 b - - 0 16",
        "r1b1k2r/4bppp/2n1p3/1p6/2p1PB2/1PP2N2/4BPPP/3RR1K1 b - - 0 17",
        "r3k2r/3bbppp/2n1p3/1p6/2P1PB2/2P2N2/4BPPP/3RR1K1 b - - 0 18",
        "r3k2r/3bbppp/2n1p3/8/1pP1P3/2P2N2/3BBPPP/3RR1K1 b - - 1 19",
        "1r2k2r/3bbppp/2n1p3/8/1pPNP3/2P5/3BBPPP/3RR1K1 b - - 3 20",
        "1r2k2r/3bbppp/2n1p3/8/2PNP3/2B5/4BPPP/3RR1K1 b - - 0 21",
        "1r2k2r/3bb1pp/2n1pp2/1N6/2P1P3/2B5/4BPPP/3RR1K1 b - - 1 22",
        "1r2k2r/3b2pp/2n1pp2/1N6/1BP1P3/8/4BPPP/3RR1K1 b - - 0 23",
        "1r2k2r/3b2pp/4pp2/1N6/1nP1P3/8/3RBPPP/4R1K1 b - - 1 24",
        "1r5r/3bk1pp/4pp2/1N6/1nP1PP2/8/3RB1PP/4R1K1 b - - 0 25",
        "1r5r/3bk1pp/2n1pp2/1N6/2P1PP2/8/3RBKPP/4R3 b - - 2 26",
        "1r5r/3bk1pp/2n2p2/1N2p3/2P1PP2/6P1/3RBK1P/4R3 b - - 0 27",
        "1r1r4/3bk1pp/2n2p2/1N2p3/2P1PP2/6P1/3RBK1P/R7 b - - 2 28",
        "1r1r4/N3k1pp/2n1bp2/4p3/2P1PP2/6P1/3RBK1P/R7 b - - 4 29",
        "1r1r4/3bk1pp/2N2p2/4p3/2P1PP2/6P1/3RBK1P/R7 b - - 0 30",
        "1r1R4/4k1pp/2b2p2/4p3/2P1PP2/6P1/4BK1P/R7 b - - 0 31",
        "3r4/4k1pp/2b2p2/4P3/2P1P3/6P1/4BK1P/R7 b - - 0 32",
        "3r4/R3k1pp/2b5/4p3/2P1P3/6P1/4BK1P/8 b - - 1 33",
        "8/3rk1pp/2b5/R3p3/2P1P3/6P1/4BK1P/8 b - - 3 34",
        "8/3r2pp/2bk4/R1P1p3/4P3/6P1/4BK1P/8 b - - 0 35",
        "8/2kr2pp/2b5/R1P1p3/4P3/4K1P1/4B2P/8 b - - 2 36",
        "1k6/3r2pp/2b5/RBP1p3/4P3/4K1P1/7P/8 b - - 4 37",
        "8/1k1r2pp/2b5/R1P1p3/4P3/3BK1P1/7P/8 b - - 6 38",
        "1k6/3r2pp/2b5/2P1p3/4P3/3BK1P1/7P/R7 b - - 8 39",
        "1k6/r5pp/2b5/2P1p3/4P3/3BK1P1/7P/5R2 b - - 10 40",
        "1k3R2/6pp/2b5/2P1p3/4P3/r2BK1P1/7P/8 b - - 12 41",
        "5R2/2k3pp/2b5/2P1p3/4P3/r2B2P1/3K3P/8 b - - 14 42",
        "5R2/2k3pp/2b5/2P1p3/4P3/3BK1P1/r6P/8 b - - 16 43",
        "5R2/2k3pp/2b5/2P1p3/4P3/r2B2P1/4K2P/8 b - - 18 44",
        "5R2/2k3pp/2b5/2P1p3/4P3/3B1KP1/r6P/8 b - - 20 45",
        "8/2k2Rpp/2b5/2P1p3/4P3/r2B1KP1/7P/8 b - - 22 46",
        "3k4/5Rpp/2b5/2P1p3/4P3/r2B2P1/4K2P/8 b - - 24 47",
        "3k4/5Rpp/2b5/2P1p3/4P3/3B1KP1/r6P/8 b - - 26 48",
        "3k4/5Rpp/2b5/2P1p3/4P3/r2B2P1/4K2P/8 b - - 28 49",
        "3k4/5Rpp/2b5/2P1p3/4P3/3BK1P1/r6P/8 b - - 30 50",
        "3k4/5Rpp/2b5/2P1p3/4P3/r2B2P1/3K3P/8 b - - 32 51",
        "3k4/5Rpp/2b5/2P1p3/4P3/2KB2P1/r6P/8 b - - 34 52",
        "3k4/5Rpp/2b5/2P1p3/4P3/r2B2P1/2K4P/8 b - - 36 53",
        "3k4/5Rpp/2b5/2P1p3/4P3/1K1B2P1/r6P/8 b - - 38 54",
        "3k4/6Rp/2b5/2P1p3/4P3/1K1B2P1/7r/8 b - - 0 55",
        "3k4/8/2b3Rp/2P1p3/4P3/1K1B2P1/7r/8 b - - 1 56",
        "8/2k3R1/2b4p/2P1p3/4P3/1K1B2P1/7r/8 b - - 3 57",
        "3k4/8/2b3Rp/2P1p3/4P3/1K1B2P1/7r/8 b - - 5 58",
        "8/2k5/2b3Rp/2P1p3/1K2P3/3B2P1/7r/8 b - - 7 59",
        "8/2k5/2b3Rp/2P1p3/4P3/2KB2P1/3r4/8 b - - 9 60",
        "8/2k5/2b3Rp/2P1p3/1K2P3/3B2P1/6r1/8 b - - 11 61",
        "8/2k5/2b3Rp/2P1p3/4P3/2KB2P1/3r4/8 b - - 13 62",
        "8/2k5/2b3Rp/2P1p3/2K1P3/3B2P1/6r1/8 b - - 15 63",
        "4b3/2k3R1/7p/2P1p3/2K1P3/3B2P1/6r1/8 b - - 17 64",
    },
    {
        "r1bqkbnr/npp1pppp/p7/3P4/4pB2/2N5/PPP2PPP/R2QKBNR w KQkq - 1 6",
        "r1bqkb1r/npp1pppp/p4n2/3P4/4pB2/2N5/PPP1QPPP/R3KBNR w KQkq - 3 7",
        "r2qkb1r/npp1pppp/p4n2/3P1b2/4pB2/2N5/PPP1QPPP/2KR1BNR w kq - 5 8",
        "r2qkb1r/1pp1pppp/p4n2/1n1P1b2/4pB2/2N4P/PPP1QPP1/2KR1BNR w kq - 1 9",
        "r2qkb1r/1pp1pppp/5n2/1p1P1b2/4pB2/7P/PPP1QPP1/2KR1BNR w kq - 0 10",
        "r2qkb1r/1ppbpppp/5n2/1Q1P4/4pB2/7P/PPP2PP1/2KR1BNR w kq - 1 11",
        "3qkb1r/1Qpbpppp/5n2/3P4/4pB2/7P/rPP2PP1/2KR1BNR w k - 0 12",
        "q3kb1r/1Qpbpppp/5n2/3P4/4pB2/7P/rPP2PP1/1K1R1BNR w k - 2 13",
        "r3kb1r/2pbpppp/5n2/3P4/4pB2/7P/1PP2PP1/1K1R1BNR w k - 0 14",
        "r3kb1r/2Bb1ppp/4pn2/3P4/4p3/7P/1PP2PP1/1K1R1BNR w k - 0 15",
        "r3kb1r/2Bb2pp/4pn2/8/4p3/7P/1PP2PP1/1K1R1BNR w k - 0 16",
        "r3k2r/2Bb2pp/4pn2/2b5/4p3/7P/1PP1NPP1/1K1R1B1R w k - 2 17",
        "r6r/2Bbk1pp/4pn2/2b5/3Np3/7P/1PP2PP1/1K1R1B1R w - - 4 18",
        "r6r/b2bk1pp/4pn2/4B3/3Np3/7P/1PP2PP1/1K1R1B1R w - - 6 19",
        "r1r5/b2bk1pp/4pn2/4B3/2BNp3/7P/1PP2PP1/1K1R3R w - - 8 20",
        "r7/b2bk1pp/4pn2/2r1B3/2BNp3/1P5P/2P2PP1/1K1R3R w - - 1 21",
        "rb6/3bk1pp/4pn2/2r1B3/2BNpP2/1P5P/2P3P1/1K1R3R w - - 1 22",
        "1r6/3bk1pp/4pn2/2r5/2BNpP2/1P5P/2P3P1/1K1R3R w - - 0 23",
        "1r6/3bk1p1/4pn1p/2r5/2BNpP2/1P5P/2P3P1/2KR3R w - - 0 24",
        "8/3bk1p1/1r2pn1p/2r5/2BNpP1P/1P6/2P3P1/2KR3R w - - 1 25",
        "8/3bk3/1r2pnpp/2r5/2BNpP1P/1P6/2P3P1/2K1R2R w - - 0 26",
        "2b5/4k3/1r2pnpp/2r5/2BNpP1P/1P4P1/2P5/2K1R2R w - - 1 27",
        "8/1b2k3/1r2pnpp/2r5/2BNpP1P/1P4P1/2P5/2K1R1R1 w - - 3 28",
        "8/1b1nk3/1r2p1pp/2r5/2BNpPPP/1P6/2P5/2K1R1R1 w - - 1 29",
        "8/1b2k3/1r2p1pp/2r1nP2/2BNp1PP/1P6/2P5/2K1R1R1 w - - 1 30",
        "8/1b2k3/1r2p1p1/2r1nPp1/2BNp2P/1P6/2P5/2K1R1R1 w - - 0 31",
        "8/1b2k3/1r2p1n1/2r3p1/2BNp2P/1P6/2P5/2K1R1R1 w - - 0 32",
        "8/1b2k3/1r2p1n1/6r1/2BNp2P/1P6/2P5/2K1R3 w - - 0 33",
        "8/1b2k3/1r2p3/4n1P1/2BNp3/1P6/2P5/2K1R3 w - - 1 34",
        "8/1b2k3/1r2p3/4n1P1/2BN4/1P2p3/2P5/2K4R w - - 0 35",
        "8/1b2k3/1r2p2R/6P1/2nN4/1P2p3/2P5/2K5 w - - 0 36",
        "8/1b2k3/3rp2R/6P1/2PN4/4p3/2P5/2K5 w - - 1 37",
        "8/4k3/3rp2R/6P1/2PN4/2P1p3/6b1/2K5 w - - 1 38",
        "8/4k3/r3p2R/2P3P1/3N4/2P1p3/6b1/2K5 w - - 1 39",
        "8/3k4/r3p2R/2P2NP1/8/2P1p3/6b1/2K5 w - - 3 40",
        "8/3k4/4p2R/2P3P1/8/2P1N3/6b1/r1K5 w - - 1 41",
        "8/3k4/4p2R/2P3P1/8/2P1N3/3K2b1/6r1 w - - 3 42",
        "8/3k4/4p2R/2P3P1/8/2PKNb2/8/6r1 w - - 5 43",
        "8/4k3/4p1R1/2P3P1/8/2PKNb2/8/6r1 w - - 7 44",
        "8/4k3/4p1R1/2P3P1/3K4/2P1N3/8/6rb w - - 9 45",
        "8/3k4/4p1R1/2P1K1P1/8/2P1N3/8/6rb w - - 11 46",
        "8/3k4/4p1R1/2P3P1/5K2/2P1N3/8/4r2b w - - 13 47",
        "8/3k4/2b1p2R/2P3P1/5K2/2P1N3/8/4r3 w - - 15 48",
        "8/3k4/2b1p3/2P3P1/5K2/2P1N2R/8/6r1 w - - 17 49",
        "2k5/7R/2b1p3/2P3P1/5K2/2P1N3/8/6r1 w - - 19 50",
        "2k5/7R/4p3/2P3P1/b1P2K2/4N3/8/6r1 w - - 1 51",
        "2k5/3bR3/4p3/2P3P1/2P2K2/4N3/8/6r1 w - - 3 52",
        "3k4/3b2R1/4p3/2P3P1/2P2K2/4N3/8/6r1 w - - 5 53",
        "3kb3/6R1/4p1P1/2P5/2P2K2/4N3/8/6r1 w - - 1 54",
        "3kb3/6R1/4p1P1/2P5/2P2KN1/8/8/2r5 w - - 3 55",
        "3kb3/6R1/4p1P1/2P1N3/2P2K2/8/8/5r2 w - - 5 56",
        "3kb3/6R1/4p1P1/2P1N3/2P5/4K3/8/4r3 w - - 7 57",
    },
    {
        "rnbq1rk1/ppp1npb1/4p1p1/3P3p/3PP3/2N2N2/PP2BPPP/R1BQ1RK1 b - - 0 8",
        "rnbq1rk1/ppp1npb1/6p1/3pP2p/3P4/2N2N2/PP2BPPP/R1BQ1RK1 b - - 0 9",
        "rn1q1rk1/ppp1npb1/6p1/3pP2p/3P2b1/2N2N2/PP2BPPP/R1BQR1K1 b - - 2 10",
        "r2q1rk1/ppp1npb1/2n3p1/3pP2p/3P2bN/2N5/PP2BPPP/R1BQR1K1 b - - 4 11",
        "r4rk1/pppqnpb1/2n3p1/3pP2p/3P2bN/2N4P/PP2BPP1/R1BQR1K1 b - - 0 12",
        "r4rk1/pppqnpb1/2n3p1/3pP2p/3P3N/7P/PP2NPP1/R1BQR1K1 b - - 0 13",
        "r4rk1/pppq1pb1/2n3p1/3pPN1p/3P4/7P/PP2NPP1/R1BQR1K1 b - - 0 14",
        "r4rk1/ppp2pb1/2n3p1/3pPq1p/3P1N2/7P/PP3PP1/R1BQR1K1 b - - 1 15",
        "r4rk1/pppq1pb1/2n3p1/3pP2p/P2P1N2/7P/1P3PP1/R1BQR1K1 b - - 0 16",
        "r2n1rk1/pppq1pb1/6p1/3pP2p/P2P1N2/R6P/1P3PP1/2BQR1K1 b - - 2 17",
        "r4rk1/pppq1pb1/4N1p1/3pP2p/P2P4/R6P/1P3PP1/2BQR1K1 b - - 0 18",
        "r4rk1/ppp2pb1/4q1p1/3pP1Bp/P2P4/R6P/1P3PP1/3QR1K1 b - - 1 19",
        "r3r1k1/ppp2pb1/4q1p1/3pP1Bp/P2P1P2/R6P/1P4P1/3QR1K1 b - - 0 20",
        "r3r1k1/ppp3b1/4qpp1/3pP2p/P2P1P1B/R6P/1P4P1/3QR1K1 b - - 1 21",
        "r3r1k1/ppp3b1/4q1p1/3pP2p/P4P1B/R6P/1P4P1/3QR1K1 b - - 0 22",
        "r4rk1/ppp3b1/4q1p1/3pP1Bp/P4P2/R6P/1P4P1/3QR1K1 b - - 2 23",
        "r4rk1/pp4b1/4q1p1/2ppP1Bp/P4P2/3R3P/1P4P1/3QR1K1 b - - 1 24",
        "r4rk1/pp4b1/4q1p1/2p1P1Bp/P2p1PP1/3R3P/1P6/3QR1K1 b - - 0 25",
        "r4rk1/pp4b1/4q1p1/2p1P1B1/P2p1PP1/3R4/1P6/3QR1K1 b - - 0 26",
        "r5k1/pp3rb1/4q1p1/2p1P1B1/P2p1PP1/6R1/1P6/3QR1K1 b - - 2 27",
        "5rk1/pp3rb1/4q1p1/2p1P1B1/P2pRPP1/6R1/1P6/3Q2K1 b - - 4 28",
        "5rk1/1p3rb1/p3q1p1/P1p1P1B1/3pRPP1/6R1/1P6/3Q2K1 b - - 0 29",
        "4r1k1/1p3rb1/p3q1p1/P1p1P1B1/3pRPP1/1P4R1/8/3Q2K1 b - - 0 30",
        "4r1k1/5rb1/pP2q1p1/2p1P1B1/3pRPP1/1P4R1/8/3Q2K1 b - - 0 31",
        "4r1k1/5rb1/pq4p1/2p1P1B1/3pRPP1/1P4R1/4Q3/6K1 b - - 1 32",
        "4r1k1/1r4b1/pq4p1/2p1P1B1/3pRPP1/1P4R1/2Q5/6K1 b - - 3 33",
        "4r1k1/1r4b1/1q4p1/p1p1P1B1/3p1PP1/1P4R1/2Q5/4R1K1 b - - 1 34",
        "4r1k1/3r2b1/1q4p1/p1p1P1B1/2Qp1PP1/1P4R1/8/4R1K1 b - - 3 35",
        "4r1k1/3r2b1/4q1p1/p1p1P1B1/2Qp1PP1/1P4R1/5K2/4R3 b - - 5 36",
        "4r1k1/3r2b1/6p1/p1p1P1B1/2Pp1PP1/6R1/5K2/4R3 b - - 0 37",
        "4r1k1/3r2b1/6p1/p1p1P1B1/2P2PP1/3p2R1/5K2/3R4 b - - 1 38",
        "5rk1/3r2b1/6p1/p1p1P1B1/2P2PP1/3p2R1/8/3RK3 b - - 3 39",
        "5rk1/6b1/6p1/p1p1P1B1/2Pr1PP1/3R4/8/3RK3 b - - 0 40",
        "5rk1/3R2b1/6p1/p1p1P1B1/2r2PP1/8/8/3RK3 b - - 1 41",
        "5rk1/3R2b1/6p1/p1p1P1B1/4rPP1/8/3K4/3R4 b - - 3 42",
        "1r4k1/3R2b1/6p1/p1p1P1B1/4rPP1/2K5/8/3R4 b - - 5 43",
        "1r4k1/3R2b1/6p1/p1p1P1B1/2K2PP1/4r3/8/3R4 b - - 7 44",
        "1r3bk1/8/3R2p1/p1p1P1B1/2K2PP1/4r3/8/3R4 b - - 9 45",
        "1r3bk1/8/6R1/2p1P1B1/p1K2PP1/4r3/8/3R4 b - - 0 46",
        "1r3b2/5k2/R7/2p1P1B1/p1K2PP1/4r3/8/3R4 b - - 2 47",
        "5b2/1r3k2/R7/2p1P1B1/p1K2PP1/4r3/8/7R b - - 4 48",
        "5b2/5k2/R7/2pKP1B1/pr3PP1/4r3/8/7R b - - 6 49",
        "5b2/5k2/R1K5/2p1P1B1/p2r1PP1/4r3/8/7R b - - 8 50",
        "8/R4kb1/2K5/2p1P1B1/p2r1PP1/4r3/8/7R b - - 10 51",
        "8/R5b1/2K3k1/2p1PPB1/p2r2P1/4r3/8/7R b - - 0 52",
        "8/6R1/2K5/2p1PPk1/p2r2P1/4r3/8/7R b - - 0 53",
        "8/6R1/2K5/2p1PP2/p2r1kP1/4r3/8/5R2 b - - 2 54",
        "8/6R1/2K2P2/2p1P3/p2r2P1/4r1k1/8/5R2 b - - 0 55",
        "8/5PR1/2K5/2p1P3/p2r2P1/4r3/6k1/5R2 b - - 0 56",
    },
    {
        "rn1qkb1r/p1pbpppp/5n2/8/2pP4/2N5/1PQ1PPPP/R1B1KBNR w KQkq - 0 7",
        "r2qkb1r/p1pbpppp/2n2n2/8/2pP4/2N2N2/1PQ1PPPP/R1B1KB1R w KQkq - 2 8",
        "r2qkb1r/p1pbpppp/5n2/8/1npPP3/2N2N2/1PQ2PPP/R1B1KB1R w KQkq - 1 9",
        "r2qkb1r/p1pb1ppp/4pn2/8/1npPP3/2N2N2/1P3PPP/R1BQKB1R w KQkq - 0 10",
        "r2qk2r/p1pbbppp/4pn2/8/1nBPP3/2N2N2/1P3PPP/R1BQK2R w KQkq - 1 11",
        "r2q1rk1/p1pbbppp/4pn2/8/1nBPP3/2N2N2/1P3PPP/R1BQ1RK1 w - - 3 12",
        "r2q1rk1/2pbbppp/p3pn2/8/1nBPPB2/2N2N2/1P3PPP/R2Q1RK1 w - - 0 13",
        "r2q1rk1/2p1bppp/p3pn2/1b6/1nBPPB2/2N2N2/1P3PPP/R2QR1K1 w - - 2 14",
        "r2q1rk1/4bppp/p1p1pn2/1b6/1nBPPB2/1PN2N2/5PPP/R2QR1K1 w - - 0 15",
        "r4rk1/3qbppp/p1p1pn2/1b6/1nBPPB2/1PN2N2/3Q1PPP/R3R1K1 w - - 2 16",
        "r4rk1/1q2bppp/p1p1pn2/1b6/1nBPPB2/1PN2N1P/3Q1PP1/R3R1K1 w - - 1 17",
        "r3r1k1/1q2bppp/p1p1pn2/1b6/1nBPPB2/1PN2N1P/4QPP1/R3R1K1 w - - 3 18",
        "r3r1k1/1q1nbppp/p1p1p3/1b6/1nBPPB2/1PN2N1P/4QPP1/3RR1K1 w - - 5 19",
        "r3rbk1/1q1n1ppp/p1p1p3/1b6/1nBPPB2/1PN2N1P/3RQPP1/4R1K1 w - - 7 20",
        "r3rbk1/1q3ppp/pnp1p3/1b6/1nBPPB2/1PN2N1P/3RQPP1/4R2K w - - 9 21",
        "2r1rbk1/1q3ppp/pnp1p3/1b6/1nBPPB2/1PN2N1P/3RQPP1/1R5K w - - 11 22",
        "2r1rbk1/1q4pp/pnp1pp2/1b6/1nBPPB2/1PN2N1P/4QPP1/1R1R3K w - - 0 23",
        "2r1rbk1/5qpp/pnp1pp2/1b6/1nBPP3/1PN1BN1P/4QPP1/1R1R3K w - - 2 24",
        "2r1rbk1/5qp1/pnp1pp1p/1b6/1nBPP3/1PN1BN1P/4QPP1/1R1R2K1 w - - 0 25",
        "2r1rbk1/5qp1/pnp1pp1p/1b6/2BPP3/1P2BN1P/n3QPP1/1R1R2K1 w - - 0 26",
        "r3rbk1/5qp1/pnp1pp1p/1b6/2BPP3/1P2BN1P/Q4PP1/1R1R2K1 w - - 1 27",
        "rr3bk1/5qp1/pnp1pp1p/1b6/2BPP3/1P2BN1P/Q4PP1/R2R2K1 w - - 3 28",
        "rr2qbk1/6p1/pnp1pp1p/1b6/2BPP3/1P2BN1P/4QPP1/R2R2K1 w - - 5 29",
        "rr2qbk1/6p1/1np1pp1p/pb6/2BPP3/1P1QBN1P/5PP1/R2R2K1 w - - 0 30",
        "rr2qbk1/6p1/1n2pp1p/pp6/3PP3/1P1QBN1P/5PP1/R2R2K1 w - - 0 31",
        "rr2qbk1/6p1/1n2pp1p/1p1P4/p3P3/1P1QBN1P/5PP1/R2R2K1 w - - 0 32",
        "rr2qbk1/3n2p1/3Ppp1p/1p6/p3P3/1P1QBN1P/5PP1/R2R2K1 w - - 1 33",
        "rr3bk1/3n2p1/3Ppp1p/1p5q/pP2P3/3QBN1P/5PP1/R2R2K1 w - - 1 34",
        "rr3bk1/3n2p1/3Ppp1p/1p5q/1P2P3/p2QBN1P/5PP1/2RR2K1 w - - 0 35",
        "1r3bk1/3n2p1/r2Ppp1p/1p5q/1P2P3/pQ2BN1P/5PP1/2RR2K1 w - - 2 36",
        "1r2qbk1/2Rn2p1/r2Ppp1p/1p6/1P2P3/pQ2BN1P/5PP1/3R2K1 w - - 4 37",
        "1r2qbk1/2Rn2p1/r2Ppp1p/1pB5/1P2P3/1Q3N1P/p4PP1/3R2K1 w - - 0 38",
        "1r2q1k1/2Rn2p1/r2bpp1p/1pB5/1P2P3/1Q3N1P/p4PP1/R5K1 w - - 0 39",
        "1r2q1k1/2Rn2p1/3rpp1p/1p6/1P2P3/1Q3N1P/p4PP1/R5K1 w - - 0 40",
        "2r1q1k1/2Rn2p1/3rpp1p/1p6/1P2P3/5N1P/Q4PP1/R5K1 w - - 1 41",
        "1r2q1k1/1R1n2p1/3rpp1p/1p6/1P2P3/5N1P/Q4PP1/R5K1 w - - 3 42",
        "2r1q1k1/2Rn2p1/3rpp1p/1p6/1P2P3/5N1P/Q4PP1/R5K1 w - - 5 43",
        "1r2q1k1/1R1n2p1/3rpp1p/1p6/1P2P3/5N1P/Q4PP1/R5K1 w - - 7 44",
        "1rq3k1/R2n2p1/3rpp1p/1p6/1P2P3/5N1P/Q4PP1/R5K1 w - - 9 45",
        "2q3k1/Rr1n2p1/3rpp1p/1p6/1P2P3/5N1P/4QPP1/R5K1 w - - 11 46",
        "Rrq3k1/3n2p1/3rpp1p/1p6/1P2P3/5N1P/4QPP1/R5K1 w - - 13 47",
    },
    {
        "rn1qkb1r/1pp2ppp/p4p2/3p1b2/5P2/1P2PN2/P1PP2PP/RN1QKB1R b KQkq - 1 6",
        "r2qkb1r/1pp2ppp/p1n2p2/3p1b2/3P1P2/1P2PN2/P1P3PP/RN1QKB1R b KQkq - 0 7",
        "r2qkb1r/1pp2ppp/p4p2/3p1b2/1n1P1P2/1P1BPN2/P1P3PP/RN1QK2R b KQkq - 2 8",
        "r2qkb1r/1pp2ppp/p4p2/3p1b2/3P1P2/1P1PPN2/P5PP/RN1QK2R b KQkq - 0 9",
        "r2qk2r/1pp2ppp/p2b1p2/3p1b2/3P1P2/1PNPPN2/P5PP/R2QK2R b KQkq - 2 10",
        "r2qk2r/1p3ppp/p1pb1p2/3p1b2/3P1P2/1PNPPN2/P5PP/R2Q1RK1 b kq - 1 11",
        "r2q1rk1/1p3ppp/p1pb1p2/3p1b2/3P1P2/1PNPPN2/P2Q2PP/R4RK1 b - - 3 12",
        "r2qr1k1/1p3ppp/p1pb1p2/3p1b2/3P1P2/1P1PPN2/P2QN1PP/R4RK1 b - - 5 13",
        "r3r1k1/1p3ppp/pqpb1p2/3p1b2/3P1P2/1P1PPNN1/P2Q2PP/R4RK1 b - - 7 14",
        "r3r1k1/1p3ppp/pqp2p2/3p1b2/1b1P1P2/1P1PPNN1/P1Q3PP/R4RK1 b - - 9 15",
        "r3r1k1/1p1b1ppp/pqp2p2/3p4/1b1P1P2/1P1PPNN1/P4QPP/R4RK1 b - - 11 16",
        "2r1r1k1/1p1b1ppp/pqp2p2/3p4/1b1PPP2/1P1P1NN1/P4QPP/R4RK1 b - - 0 17",
        "2r1r1k1/1p1b1ppp/pq3p2/2pp4/1b1PPP2/PP1P1NN1/5QPP/R4RK1 b - - 0 18",
        "2r1r1k1/1p1b1ppp/pq3p2/2Pp4/4PP2/PPbP1NN1/5QPP/R4RK1 b - - 0 19",
        "2r1r1k1/1p1b1ppp/p4p2/2Pp4/4PP2/PqbP1NN1/5QPP/RR4K1 b - - 1 20",
        "2r1r1k1/1p1b1ppp/p4p2/2Pp4/q3PP2/P1bP1NN1/R4QPP/1R4K1 b - - 3 21",
        "2r1r1k1/1p3ppp/p4p2/1bPP4/q4P2/P1bP1NN1/R4QPP/1R4K1 b - - 0 22",
        "2r1r1k1/1p3ppp/p4p2/2PP4/q4P2/P1bb1NN1/R4QPP/2R3K1 b - - 1 23",
        "2r1r1k1/1p3ppp/p2P1p2/2P5/2q2P2/P1bb1NN1/R4QPP/2R3K1 b - - 0 24",
        "2rr2k1/1p3ppp/p2P1p2/2P5/2q2P2/P1bb1NN1/R4QPP/2R4K b - - 2 25",
        "2rr2k1/1p3ppp/p2P1p2/2Q5/5P2/P1bb1NN1/R5PP/2R4K b - - 0 26",
        "3r2k1/1p3ppp/p2P1p2/2r5/5P2/P1bb1N2/R3N1PP/2R4K b - - 1 27",
        "3r2k1/1p3ppp/p2P1p2/2r5/5P2/P1b2N2/4R1PP/2R4K b - - 0 28",
        "3r2k1/1p3ppp/p2P1p2/2r5/1b3P2/P4N2/4R1PP/3R3K b - - 2 29",
        "3r2k1/1p2Rppp/p2P1p2/b1r5/5P2/P4N2/6PP/3R3K b - - 4 30",
        "3r2k1/1R3ppp/p1rP1p2/b7/5P2/P4N2/6PP/3R3K b - - 0 31",
        "3r2k1/1R3ppp/p2R1p2/b7/5P2/P4N2/6PP/7K b - - 0 32",
        "6k1/1R3ppp/p2r1p2/b7/5P2/P4NP1/7P/7K b - - 0 33",
        "6k1/1R3p1p/p2r1pp1/b7/5P1P/P4NP1/8/7K b - - 0 34",
        "6k1/3R1p1p/pr3pp1/b7/5P1P/P4NP1/8/7K b - - 2 35",
        "6k1/5p2/pr3pp1/b2R3p/5P1P/P4NP1/8/7K b - - 1 36",
        "6k1/5p2/pr3pp1/7p/5P1P/P1bR1NP1/8/7K b - - 3 37",
        "6k1/5p2/p1r2pp1/7p/5P1P/P1bR1NP1/6K1/8 b - - 5 38",
        "6k1/5p2/p1r2pp1/b2R3p/5P1P/P4NP1/6K1/8 b - - 7 39",
        "6k1/5p2/p4pp1/b2R3p/5P1P/P4NPK/2r5/8 b - - 9 40",
        "6k1/2b2p2/p4pp1/7p/5P1P/P2R1NPK/2r5/8 b - - 11 41",
        "6k1/2b2p2/5pp1/p6p/3N1P1P/P2R2PK/2r5/8 b - - 1 42",
        "6k1/2b2p2/5pp1/p6p/3N1P1P/P1R3PK/r7/8 b - - 3 43",
        "6k1/5p2/1b3pp1/p6p/5P1P/P1R3PK/r1N5/8 b - - 5 44",
        "8/5pk1/1bR2pp1/p6p/5P1P/P5PK/r1N5/8 b - - 7 45",
        "3b4/5pk1/2R2pp1/p4P1p/7P/P5PK/r1N5/8 b - - 0 46",
        "8/4bpk1/2R2pp1/p4P1p/6PP/P6K/r1N5/8 b - - 0 47",
        "8/5pk1/2R2pP1/p6p/6PP/b6K/r1N5/8 b - - 0 48",
        "8/6k1/2R2pp1/p6P/7P/b6K/r1N5/8 b - - 0 49",
        "8/6k1/2R2p2/p6p/7P/b5K1/r1N5/8 b - - 1 50",
        "8/8/2R2pk1/p6p/7P/b4K2/r1N5/8 b - - 3 51",
        "8/8/2R2pk1/p6p/7P/4NK2/rb6/8 b - - 5 52",
        "2R5/8/5pk1/7p/p6P/4NK2/rb6/8 b - - 1 53",
        "6R1/8/5pk1/7p/p6P/4NK2/1b6/r7 b - - 3 54",
        "R7/5k2/5p2/7p/p6P/4NK2/1b6/r7 b - - 5 55",
        "R7/5k2/5p2/7p/7P/p3N3/1b2K3/r7 b - - 1 56",
        "8/R4k2/5p2/7p/7P/p3N3/1b2K3/7r b - - 3 57",
        "8/8/5pk1/7p/R6P/p3N3/1b2K3/7r b - - 5 58",
        "8/8/5pk1/7p/R6P/p7/4K3/2bN3r b - - 7 59",
        "8/8/5pk1/7p/R6P/p7/4KN1r/2b5 b - - 9 60",
        "8/8/5pk1/7p/R6P/p3K3/1b3N1r/8 b - - 11 61",
        "8/8/R4pk1/7p/7P/p1b1K3/5N1r/8 b - - 13 62",
        "8/8/5pk1/7p/7P/2b1K3/R4N1r/8 b - - 0 63",
        "8/8/5pk1/7p/3K3P/8/R4N1r/4b3 b - - 2 64",
    }
};
// clang-format on

}  // namespace

namespace VeloCT::Benchmark {

// Builds a list of UCI commands to be run by bench. There
// are five parameters: TT size in MB, number of search threads that
// should be used, the limit value spent for each position, a file name
// where to look for positions in FEN format, and the type of the limit:
// depth, perft, nodes and movetime (in milliseconds). Examples:
//
// bench                            : search default positions up to depth 13
// bench 64 1 15                    : search default positions up to depth 15 (TT = 64MB)
// bench 64 1 100000 default nodes  : search default positions for 100K nodes each
// bench 64 4 5000 current movetime : search current position with 4 threads for 5 sec
// bench 16 1 5 blah perft          : run a perft 5 on positions in file "blah"
std::vector<std::string> setup_bench(const std::string& currentFen, std::istream& is) {

    std::vector<std::string> fens, list;
    std::string              go, token;

    // Assign default values to missing arguments
    std::string ttSize    = (is >> token) ? token : "16";
    std::string threads   = (is >> token) ? token : "1";
    std::string limit     = (is >> token) ? token : "13";
    std::string fenFile   = (is >> token) ? token : "default";
    std::string limitType = (is >> token) ? token : "depth";

    go = limitType == "eval" ? "eval" : "go " + limitType + " " + limit;

    if (fenFile == "default")
        fens = Defaults;

    else if (fenFile == "current")
        fens.push_back(currentFen);

    else
    {
        std::string   fen;
        std::ifstream file(fenFile);

        if (!file.is_open())
        {
            std::cerr << "Unable to open file " << fenFile << std::endl;
            exit(EXIT_FAILURE);
        }

        while (getline(file, fen))
            if (!fen.empty())
                fens.push_back(fen);

        file.close();
    }

    list.emplace_back("setoption name Threads value " + threads);
    list.emplace_back("setoption name Hash value " + ttSize);
    list.emplace_back("ucinewgame");

    for (const std::string& fen : fens)
        if (fen.find("setoption") != std::string::npos)
            list.emplace_back(fen);
        else
        {
            list.emplace_back("position fen " + fen);
            list.emplace_back(go);
        }

    return list;
}

BenchmarkSetup setup_benchmark(std::istream& is) {
    // TT_SIZE_PER_THREAD is chosen such that roughly half of the hash is used all positions
    // for the current sequence have been searched.
    static constexpr int TT_SIZE_PER_THREAD = 128;

    static constexpr int DEFAULT_DURATION_S = 150;

    BenchmarkSetup setup{};

    // Assign default values to missing arguments
    int desiredTimeS;

    if (!(is >> setup.threads))
        setup.threads = int(get_hardware_concurrency());
    else
        setup.originalInvocation += std::to_string(setup.threads);

    if (!(is >> setup.ttSize))
        setup.ttSize = TT_SIZE_PER_THREAD * setup.threads;
    else
        setup.originalInvocation += " " + std::to_string(setup.ttSize);

    if (!(is >> desiredTimeS))
        desiredTimeS = DEFAULT_DURATION_S;
    else
        setup.originalInvocation += " " + std::to_string(desiredTimeS);

    setup.filledInvocation += std::to_string(setup.threads) + " " + std::to_string(setup.ttSize)
                            + " " + std::to_string(desiredTimeS);

    auto getCorrectedTime = [&](int ply) {
        // time per move is fit roughly based on LTC games
        // seconds = 50/{ply+15}
        // ms = 50000/{ply+15}
        // with this fit 10th move gets 2000ms
        // adjust for desired 10th move time
        return 50000.0 / (static_cast<double>(ply) + 15.0);
    };

    float totalTime = 0;
    for (const auto& game : BenchmarkPositions)
        for (usize i = 0; i < game.size(); ++i)
            totalTime += float(getCorrectedTime(int(i + 1)));

    float timeScaleFactor = static_cast<float>(desiredTimeS * 1000) / totalTime;

    for (const auto& game : BenchmarkPositions)
    {
        setup.commands.emplace_back("ucinewgame");
        int ply = 1;
        for (const std::string& fen : game)
        {
            setup.commands.emplace_back("position fen " + fen);
            const int correctedTime = static_cast<int>(getCorrectedTime(ply++) * timeScaleFactor);
            setup.commands.emplace_back("go movetime " + std::to_string(correctedTime));
        }
    }

    return setup;
}

}  // namespace VeloCT

// ==== END OF FILE: benchmark.cpp ====

// ==== START OF FILE: bitboard.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "bitboard.h"

#include <bitset>

namespace VeloCT {

u8 PopCnt16[1 << 16];

// Returns an ASCII representation of a bitboard suitable
// to be printed to standard output. Useful for debugging.
std::string Bitboards::pretty(Bitboard b) {

    std::string s = "+---+---+---+---+---+---+---+---+\n";

    for (Rank r = RANK_8;; --r)
    {
        for (File f = FILE_A; f <= FILE_H; ++f)
            s += b & make_square(f, r) ? "| X " : "|   ";

        s += "| " + std::to_string(1 + r) + "\n+---+---+---+---+---+---+---+---+\n";

        if (r == RANK_1)
            break;
    }
    s += "  a   b   c   d   e   f   g   h\n";

    return s;
}

// Initializes the popcount table at startup.
void Bitboards::init() {
    for (unsigned i = 0; i < (1 << 16); ++i)
        PopCnt16[i] = u8(std::bitset<16>(i).count());
}

}  // namespace VeloCT

// ==== END OF FILE: bitboard.cpp ====

// ==== START OF FILE: engine.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "engine.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <deque>
#include <iosfwd>
#include <memory>
#include <ostream>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "evaluate.h"
#include "misc.h"
#include "nnue/network.h"
#include "nnue/nnue_common.h"
#include "numa.h"
#include "perft.h"
#include "position.h"
#include "search.h"
#include "shm.h"
#include "syzygy/tbprobe.h"
#include "types.h"
#include "uci.h"
#include "ucioption.h"

namespace VeloCT {

namespace NN = Eval::NNUE;

constexpr int MaxHashMB  = Is64Bit ? 33554432 : 2048;
int           MaxThreads = std::max(1024, 4 * int(get_hardware_concurrency()));

// The default configuration will attempt to group L3 domains up to 32 threads.
// This size was found to be a good balance between the Elo gain of increased
// history sharing and the speed loss from more cross-cache accesses (see
// PR#6526). The user can always explicitly override this behavior.
constexpr NumaAutoPolicy DefaultNumaPolicy = BundledL3Policy{32};

Engine::Engine(std::optional<std::filesystem::path> path) :
    binaryDirectory(path ? CommandLine::get_binary_directory(*path) : std::filesystem::path{}),
    numaContext(NumaConfig::from_system(DefaultNumaPolicy)),
    states(new std::deque<StateInfo>(1)),
    threads(),
    networkFile{std::nullopt, ""},
    network(numaContext) {

    pos.set(StartFEN, false, &states->back());

    options.add(  //
      "Debug Log File", Option("", [](const Option& o) {
          start_logger(path_from_utf8(std::string(o)));
          return std::nullopt;
      }));

    options.add(  //
      "NumaPolicy", Option("auto", [this](const Option& o) {
          if (!set_numa_config_from_option(o))
              return "NumaPolicy: invalid value '" + std::string(o) + "', keeping previous config.";
          return numa_config_information_as_string() + "\n"
               + thread_allocation_information_as_string();
      }));

    options.add(  //
      "Threads", Option(1, 1, MaxThreads, [this](const Option&) {
          resize_threads();
          return thread_allocation_information_as_string();
      }));

    options.add(  //
      "Hash", Option(16, 1, MaxHashMB, [this](const Option& o) {
          set_tt_size(o);
          return std::nullopt;
      }));

    options.add(  //
      "Clear Hash", Option([this](const Option&) {
          search_clear();
          return std::nullopt;
      }));

    options.add(  //
      "Ponder", Option(false));

    options.add(  //
      "MultiPV", Option(1, 1, MAX_MOVES));

    options.add("Skill Level", Option(20, 0, 20));

    options.add("Move Overhead", Option(10, 0, 5000));

    options.add("nodestime", Option(0, 0, 10000));

    options.add("UCI_Chess960", Option(false));

    options.add("UCI_LimitStrength", Option(false));

    options.add("UCI_Elo",
                Option(VeloCT::Search::Skill::LowestElo, VeloCT::Search::Skill::LowestElo,
                       VeloCT::Search::Skill::HighestElo));

    options.add("UCI_ShowWDL", Option(false));

    options.add(  //
      "SyzygyPath", Option("", [](const Option& o) {
          Tablebases::init(o);
          return std::nullopt;
      }));

    options.add("SyzygyProbeDepth", Option(1, 1, 100));

    options.add("Syzygy50MoveRule", Option(true));

    options.add("SyzygyProbeLimit", Option(7, 0, 7));

    options.add(  //
      "EvalFile", Option(EvalFileDefaultName, [this](const Option& o) {
          load_network(path_from_utf8(std::string(o)));
          return std::nullopt;
      }));

    network = get_default_network();
    threads.clear();
    threads.ensure_network_replicated();
    resize_threads();
}

std::variant<u64, PositionSetError>
Engine::perft(const std::string& fen, Depth depth, bool isChess960) {
    verify_network();

    return Benchmark::perft(fen, depth, isChess960);
}

void Engine::go(Search::LimitsType& limits) {
    assert(limits.perft == 0);
    verify_network();

    threads.start_thinking(options, pos, states, limits);
}
void Engine::stop() { threads.stop = true; }

void Engine::search_clear() {
    wait_for_search_finished();

    tt.clear(threads);
    threads.clear();

    // TODO: does not work with multiple instances
    Tablebases::init(options["SyzygyPath"]);  // Free mapped files
}

void Engine::set_on_update_no_moves(std::function<void(const Engine::InfoShort&)>&& f) {
    updateContext.onUpdateNoMoves = std::move(f);
}

void Engine::set_on_update_full(std::function<void(const Engine::InfoFull&)>&& f) {
    updateContext.onUpdateFull = std::move(f);
}

void Engine::set_on_iter(std::function<void(const Engine::InfoIter&)>&& f) {
    updateContext.onIter = std::move(f);
}

void Engine::set_on_bestmove(std::function<void(std::string_view, std::string_view)>&& f) {
    updateContext.onBestmove = std::move(f);
}

void Engine::set_on_verify_network(std::function<void(std::string_view)>&& f) {
    onVerifyNetwork = std::move(f);
}

void Engine::wait_for_search_finished() { threads.main_thread()->wait_for_search_finished(); }

std::optional<PositionSetError> Engine::set_position(const std::string&              fen,
                                                     const std::vector<std::string>& moves) {
    // Drop the old state and create a new one
    states   = StateListPtr(new std::deque<StateInfo>(1));
    auto err = pos.set(fen, options["UCI_Chess960"], &states->back());
    if (err.has_value())
        return err;

    for (const auto& move : moves)
    {
        auto m = UCIEngine::to_move(pos, move);

        if (m == Move::none())
            return PositionSetError("Illegal move: " + move);

        states->emplace_back();
        pos.do_move(m, states->back());
    }

    return std::nullopt;
}

// modifiers

bool Engine::set_numa_config_from_option(const std::string& o) {
    if (o == "auto" || o == "system")
    {
        numaContext.set_numa_config(NumaConfig::from_system(DefaultNumaPolicy));
    }
    else if (o == "hardware")
    {
        // Don't respect affinity set in the system.
        numaContext.set_numa_config(NumaConfig::from_system(DefaultNumaPolicy, false));
    }
    else if (o == "none")
    {
        numaContext.set_numa_config(NumaConfig{});
    }
    else
    {
        auto parsed = NumaConfig::from_string(o);
        if (!parsed.has_value())
            return false;
        numaContext.set_numa_config(std::move(*parsed));
    }

    // Force reallocation of threads in case affinities need to change.
    resize_threads();
    threads.ensure_network_replicated();
    return true;
}

void Engine::resize_threads() {
    threads.wait_for_search_finished();
    threads.set(numaContext.get_numa_config(), {options, threads, tt, sharedHists, network},
                updateContext);

    // Reallocate the hash with the new threadpool size
    set_tt_size(options["Hash"]);
    threads.ensure_network_replicated();
}

void Engine::set_tt_size(usize mb) {
    wait_for_search_finished();
    tt.resize(mb, threads);
}

void Engine::set_ponderhit(bool b) { threads.main_manager()->ponder = b; }

// network related

void Engine::verify_network() const {
    const auto file = path_from_utf8(std::string(options["EvalFile"]));
    network->verify(onVerifyNetwork, networkFile, file);

    auto statuses = network.get_status_and_errors();
    for (usize i = 0; i < statuses.size(); ++i)
    {
        const auto [status, error] = statuses[i];
        std::string message        = "Network replica " + std::to_string(i + 1) + ": ";
        if (status == SystemWideSharedConstantAllocationStatus::NoAllocation)
        {
            message += "No allocation.";
        }
        else if (status == SystemWideSharedConstantAllocationStatus::LocalMemory)
        {
            message += "Local memory.";
        }
        else if (status == SystemWideSharedConstantAllocationStatus::SharedMemory)
        {
            message += "Shared memory.";
        }
        else
        {
            message += "Unknown status.";
        }

        if (error.has_value())
        {
            message += " " + *error;
        }

        onVerifyNetwork(message);
    }
}

std::unique_ptr<Eval::NNUE::Network> Engine::get_default_network() {

    auto network_ = std::make_unique<NN::Network>();

    network_->load(binaryDirectory, std::filesystem::path{}, networkFile);

    return network_;
}

void Engine::load_network(const std::filesystem::path& file) {
    network.modify_and_replicate(
      [this, &file](NN::Network& network_) { network_.load(binaryDirectory, file, networkFile); });
    threads.clear();
    threads.ensure_network_replicated();
}

void Engine::save_network(const std::optional<std::filesystem::path>& file) {
    network.modify_and_replicate(
      [&file, this](NN::Network& network_) { network_.save(networkFile, file); });
}

// utility functions

void Engine::trace_eval() const {
    StateListPtr trace_states(new std::deque<StateInfo>(1));
    Position     p;
    p.set(pos.fen(), options["UCI_Chess960"], &trace_states->back());

    verify_network();

    sync_cout << "\n" << Eval::trace(p, *network) << sync_endl;
}

const OptionsMap& Engine::get_options() const { return options; }
OptionsMap&       Engine::get_options() { return options; }

std::string Engine::fen() const { return pos.fen(); }

std::optional<PositionSetError> Engine::flip() { return pos.flip(); }

std::string Engine::visualize() const {
    std::stringstream ss;
    ss << pos;
    return ss.str();
}

int Engine::get_hashfull(int maxAge) const { return tt.hashfull(maxAge); }

std::vector<std::pair<usize, usize>> Engine::get_bound_thread_count_by_numa_node() const {
    auto                                 counts = threads.get_bound_thread_count_by_numa_node();
    const NumaConfig&                    cfg    = numaContext.get_numa_config();
    std::vector<std::pair<usize, usize>> ratios;
    NumaIndex                            n = 0;
    for (; n < counts.size(); ++n)
        ratios.emplace_back(counts[n], cfg.num_cpus_in_numa_node(n));
    if (!counts.empty())
        for (; n < cfg.num_numa_nodes(); ++n)
            ratios.emplace_back(0, cfg.num_cpus_in_numa_node(n));
    return ratios;
}

std::string Engine::get_numa_config_as_string() const {
    return numaContext.get_numa_config().to_string();
}

std::string Engine::numa_config_information_as_string() const {
    auto cfgStr = get_numa_config_as_string();
    return "Available processors: " + cfgStr;
}

std::string Engine::thread_binding_information_as_string() const {
    auto              boundThreadsByNode = get_bound_thread_count_by_numa_node();
    std::stringstream ss;
    if (boundThreadsByNode.empty())
        return ss.str();

    bool isFirst = true;

    for (auto&& [current, total] : boundThreadsByNode)
    {
        if (!isFirst)
            ss << ":";
        ss << current << "/" << total;
        isFirst = false;
    }

    return ss.str();
}

std::string Engine::thread_allocation_information_as_string() const {
    std::stringstream ss;

    usize threadsSize = threads.size();
    ss << "Using " << threadsSize << (threadsSize > 1 ? " threads" : " thread");

    auto boundThreadsByNodeStr = thread_binding_information_as_string();
    if (boundThreadsByNodeStr.empty())
        return ss.str();

    ss << " with NUMA node thread binding: ";
    ss << boundThreadsByNodeStr;

    return ss.str();
}
}

// ==== END OF FILE: engine.cpp ====

// ==== START OF FILE: evaluate.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "evaluate.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

#include "misc.h"
#include "nnue/network.h"
#include "nnue/nnue_misc.h"
#include "position.h"
#include "types.h"
#include "uci.h"
#include "nnue/nnue_accumulator.h"

namespace VeloCT {

// Evaluate is the evaluator for the outer world. It returns a static evaluation
// of the position from the point of view of the side to move.
Value Eval::evaluate(const Eval::NNUE::Network&     network,
                     const Position&                pos,
                     Eval::NNUE::AccumulatorStack&  accumulators,
                     Eval::NNUE::AccumulatorCaches& caches,
                     int                            optimism) {

    assert(!pos.checkers());

    auto [psqt, positional] = network.evaluate(pos, accumulators, caches);

    Value nnue = psqt + positional;

    // Blend optimism and eval with nnue complexity
    int nnueComplexity = std::abs(psqt - positional);
    optimism += optimism * i64(nnueComplexity) / 476;
    nnue -= nnue * i64(nnueComplexity) / 18236;

    int material = 534 * pos.count<PAWN>() + pos.non_pawn_material();
    int v        = (nnue * i64(77871 + material) + optimism * i64(7191 + material)) / 77871;

    // Damp down the evaluation linearly when shuffling
    v -= v * pos.rule50_count() / 199;

    // Guarantee evaluation does not hit the tablebase range
    v = std::clamp(v, VALUE_TB_LOSS_IN_MAX_PLY + 1, VALUE_TB_WIN_IN_MAX_PLY - 1);

    return v;
}

// Like evaluate(), but instead of returning a value, it returns
// a string (suitable for outputting to stdout) that contains the detailed
// descriptions and values of each evaluation term. Useful for debugging.
// Trace scores are from white's point of view
std::string Eval::trace(Position& pos, const Eval::NNUE::Network& network) {

    if (pos.checkers())
        return "Final evaluation: none (in check)";

    auto accumulators = std::make_unique<Eval::NNUE::AccumulatorStack>();
    auto caches       = std::make_unique<Eval::NNUE::AccumulatorCaches>(network);

    std::stringstream ss;
    ss << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2);
    ss << '\n' << NNUE::trace(pos, network, *caches) << '\n';

    ss << std::showpoint << std::showpos << std::fixed << std::setprecision(2) << std::setw(15);

    auto [psqt, positional] = network.evaluate(pos, *accumulators, *caches);
    Value v                 = psqt + positional;
    ss << "NNUE evaluation          " << v << " (side to move, internal units)\n";
    v = pos.side_to_move() == WHITE ? v : -v;
    ss << "NNUE evaluation        " << 0.01 * UCIEngine::to_cp(v, pos) << " (white side)\n";

    v = evaluate(network, pos, *accumulators, *caches, VALUE_ZERO);
    v = pos.side_to_move() == WHITE ? v : -v;

    ss << "Final evaluation      ";
    ss << 0.01 * UCIEngine::to_cp(v, pos) << " (white side)";
    ss << " [with scaled NNUE, ...]\n";

    return ss.str();
}

}  // namespace VeloCT

// ==== END OF FILE: evaluate.cpp ====

// ==== START OF FILE: memory.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "memory.h"

#include <cstdlib>
#include <iostream>  // std::cerr

#if __has_include("features.h")
    #include <features.h>
#endif

#if defined(__linux__) && !defined(__ANDROID__)
    #include <errno.h>
    #include <sys/mman.h>
    // IWYU pragma: no_include <bits/mman-map-flags-generic.h>
    #include <cstring>
    #include <mutex>
    #include <map>
#endif

#if defined(__APPLE__) || defined(__ANDROID__) || defined(__OpenBSD__) \
  || (defined(__GLIBCXX__) && !defined(_GLIBCXX_HAVE_ALIGNED_ALLOC) && !defined(_WIN32)) \
  || defined(__e2k__)
    #define POSIXALIGNEDALLOC
    #include <stdlib.h>
#endif

#ifdef _WIN32
    #if _WIN32_WINNT < 0x0601
        #undef _WIN32_WINNT
        #define _WIN32_WINNT 0x0601  // Force to include needed API prototypes
    #endif

    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    #include <ios>  // std::hex, std::dec
    #include <windows.h>

// The needed Windows API for processor groups could be missed from old Windows
// versions, so instead of calling them directly (forcing the linker to resolve
// the calls at compile time), try to load them at runtime. To do this we need
// first to define the corresponding function pointers.

#endif


namespace VeloCT {

// Wrappers for systems where the c++17 implementation does not guarantee the
// availability of aligned_alloc(). Memory allocated with std_aligned_alloc()
// must be freed with std_aligned_free().

void* std_aligned_alloc(usize alignment, usize size) {
#if defined(_ISOC11_SOURCE)
    return aligned_alloc(alignment, size);
#elif defined(POSIXALIGNEDALLOC)
    void* mem = nullptr;
    posix_memalign(&mem, alignment, size);
    return mem;
#elif defined(_WIN32) && !defined(_M_ARM) && !defined(_M_ARM64)
    return _mm_malloc(size, alignment);
#elif defined(_WIN32)
    return _aligned_malloc(size, alignment);
#else
    return std::aligned_alloc(alignment, size);
#endif
}

void std_aligned_free(void* ptr) {

#if defined(POSIXALIGNEDALLOC)
    free(ptr);
#elif defined(_WIN32) && !defined(_M_ARM) && !defined(_M_ARM64)
    _mm_free(ptr);
#elif defined(_WIN32)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

// aligned_large_pages_alloc() will return suitably aligned memory,
// if possible using large pages.

#if defined(_WIN32)

static void* aligned_large_pages_alloc_windows([[maybe_unused]] usize allocSize) {

    return windows_try_with_large_page_priviliges(
      [&](usize largePageSize) {
          // Round up size to full pages and allocate
          allocSize = (allocSize + largePageSize - 1) & ~usize(largePageSize - 1);
          return VirtualAlloc(nullptr, allocSize, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES,
                              PAGE_READWRITE);
      },
      []() { return (void*) nullptr; });
}

void* aligned_large_pages_alloc_with_hint(usize allocSize, bool) {

    // Try to allocate large pages
    void* mem = aligned_large_pages_alloc_windows(allocSize);

    // Fall back to regular, page-aligned, allocation if necessary
    if (!mem)
        mem = VirtualAlloc(nullptr, allocSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    return mem;
}

#else

    #if defined(__linux__) && defined(MAP_HUGE_SHIFT) && defined(__x86_64__)
        #define HAS_HUGE_PAGES

static std::map<void*, usize> huge_pages;
static std::mutex             huge_pages_mtx;

static void* try_huge_pages_alloc(usize allocSize) {
    usize size = ((allocSize + HugePageSize - 1) / HugePageSize) * HugePageSize;
    void* mem  = mmap(NULL, size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | (30 << MAP_HUGE_SHIFT), -1, 0);

    if (mem == MAP_FAILED)
        return nullptr;

    std::lock_guard lg(huge_pages_mtx);
    huge_pages[mem] = size;
    return mem;
}
    #endif  // defined(__linux__) && defined(MAP_HUGE_SHIFT) && defined(__x86_64__)

void* aligned_large_pages_alloc_with_hint(usize allocSize, [[maybe_unused]] bool hugePageHint) {
    #ifdef HAS_HUGE_PAGES
    if (hugePageHint && allocSize >= HugePageSize)
    {
        void* mem = try_huge_pages_alloc(allocSize);
        if (mem)
            return mem;
    }
    #endif

    #if defined(__linux__)
    constexpr usize alignment = 2 * 1024 * 1024;  // 2MB page size assumed
    #else
    constexpr usize alignment = 4096;  // small page size assumed
    #endif

    // Round up to multiples of alignment
    usize size = ((allocSize + alignment - 1) / alignment) * alignment;
    void* mem  = std_aligned_alloc(alignment, size);
    #if defined(MADV_HUGEPAGE)
    madvise(mem, size, MADV_HUGEPAGE);
    #endif
    return mem;
}

#endif

void* aligned_large_pages_alloc(usize size) {
    return aligned_large_pages_alloc_with_hint(size, false);
}

bool has_large_pages() {

#if defined(_WIN32)

    constexpr usize page_size = 2 * 1024 * 1024;  // 2MB page size assumed
    void*           mem       = aligned_large_pages_alloc_windows(page_size);
    if (mem == nullptr)
    {
        return false;
    }
    else
    {
        aligned_large_pages_free(mem);
        return true;
    }

#elif defined(__linux__)

    #if defined(MADV_HUGEPAGE)
    return true;
    #else
    return false;
    #endif

#else

    return false;

#endif
}


// aligned_large_pages_free() will free the previously memory allocated
// by aligned_large_pages_alloc(). The effect is a nop if mem == nullptr.

#if defined(_WIN32)

void aligned_large_pages_free(void* mem) {

    if (mem && !VirtualFree(mem, 0, MEM_RELEASE))
    {
        DWORD err = GetLastError();
        std::cerr << "Failed to free large page memory. Error code: 0x" << std::hex << err
                  << std::dec << std::endl;
        exit(EXIT_FAILURE);
    }
}

#else

void aligned_large_pages_free(void* mem) {
    if (!mem)
        return;

    #ifdef HAS_HUGE_PAGES
    std::lock_guard lg(huge_pages_mtx);
    if (auto it = huge_pages.find(mem); it != huge_pages.end())
    {
        if (munmap(mem, it->second) != 0)
        {
            std::cerr << "munmap failed: " << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }
        huge_pages.erase(it);
        return;
    }
    #endif

    std_aligned_free(mem);
}

#endif
}  // namespace VeloCT

// ==== END OF FILE: memory.cpp ====

// ==== START OF FILE: misc.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "misc.h"

#include <array>
#include <atomic>
#include <cassert>
#include <cctype>
#include <cstring>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <mutex>
#include <sstream>
#include <string_view>

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <direct.h>
    #include <windows.h>
    #include <shellapi.h>
#endif

#include "types.h"

namespace VeloCT {

namespace fs = std::filesystem;

namespace {

// Version number or dev.
constexpr std::string_view version = "dev";

// Our fancy logging facility. The trick here is to replace cin.rdbuf() and
// cout.rdbuf() with two Tie objects that tie cin and cout to a file stream. We
// can toggle the logging of std::cout and std::cin at runtime whilst preserving
// usual I/O functionality, all without changing a single line of code!
// Idea from http://groups.google.com/group/comp.lang.c++/msg/1d941c0f26ea0d81

struct Tie: public std::streambuf {  // MSVC requires split streambuf for cin and cout

    Tie(std::streambuf* b, std::streambuf* l) :
        buf(b),
        logBuf(l) {}

    int sync() override { return logBuf->pubsync(), buf->pubsync(); }
    int overflow(int c) override { return log(buf->sputc(char(c)), "<< "); }
    int underflow() override { return buf->sgetc(); }
    int uflow() override { return log(buf->sbumpc(), ">> "); }

    std::streambuf *buf, *logBuf;

    int log(int c, const char* prefix) {

        static int last = '\n';  // Single log file

        if (last == '\n')
            logBuf->sputn(prefix, 3);

        return last = logBuf->sputc(char(c));
    }
};

class Logger {

    Logger() :
        in(std::cin.rdbuf(), file.rdbuf()),
        out(std::cout.rdbuf(), file.rdbuf()) {}
    ~Logger() { start(""); }

    std::ofstream file;
    Tie           in, out;

   public:
    static void start(const fs::path& fname) {

        static Logger l;

        if (l.file.is_open())
        {
            std::cout.rdbuf(l.out.buf);
            std::cin.rdbuf(l.in.buf);
            l.file.close();
        }

        if (!fname.empty())
        {
            l.file.open(fname, std::ifstream::out);

            if (!l.file.is_open())
            {
                std::cerr << "Unable to open debug log file " << fname << std::endl;
                exit(EXIT_FAILURE);
            }

            std::cin.rdbuf(&l.in);
            std::cout.rdbuf(&l.out);
        }
    }
};

}  // namespace


// Returns the full name of the current VeloCT version.
//
// For local dev compiles we try to append the commit SHA and
// commit date from git. If that fails only the local compilation
// date is set and "nogit" is specified:
//      VeloCT dev-YYYYMMDD-SHA
//      or
//      VeloCT dev-YYYYMMDD-nogit
//
// For releases (non-dev builds) we only include the version number:
//      VeloCT version
std::string engine_version_info() {
    std::stringstream ss;
    ss << "VeloCT " << version << std::setfill('0');

    if constexpr (version == "dev")
    {
        ss << "-";
#ifdef GIT_DATE
        ss << stringify(GIT_DATE);
#else
        constexpr std::string_view months("Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec");

        std::string       month, day, year;
        std::stringstream date(__DATE__);  // From compiler, format is "Sep 21 2008"

        date >> month >> day >> year;
        ss << year << std::setw(2) << std::setfill('0') << (1 + months.find(month) / 4)
           << std::setw(2) << std::setfill('0') << day;
#endif

#ifdef GIT_DIFFINDEX
        ss << "-m";
#endif

        ss << "-";

#ifdef GIT_SHA
        ss << stringify(GIT_SHA);
#else
        ss << "nogit";
#endif
    }

    return ss.str();
}

std::string engine_info(bool to_uci) {
    return engine_version_info() + (to_uci ? "\nid author " : " by ")
         + "the VeloCT developers (see AUTHORS file)";
}


// Returns a string trying to describe the compiler we use
std::string compiler_info() {

#define make_version_string(major, minor, patch) \
    stringify(major) "." stringify(minor) "." stringify(patch)

    // Predefined macros hell:
    //
    // __GNUC__                Compiler is GCC, Clang or ICX
    // __clang__               Compiler is Clang or ICX
    // __INTEL_LLVM_COMPILER   Compiler is ICX
    // _MSC_VER                Compiler is MSVC
    // _WIN32                  Building on Windows (any)
    // _WIN64                  Building on Windows 64 bit

    std::string compiler = "\nCompiled by                : ";

#if defined(__INTEL_LLVM_COMPILER)
    compiler += "ICX ";
    compiler += stringify(__INTEL_LLVM_COMPILER);
#elif defined(__clang__)
    compiler += "clang++ ";
    compiler += make_version_string(__clang_major__, __clang_minor__, __clang_patchlevel__);
#elif _MSC_VER
    compiler += "MSVC ";
    compiler += "(version ";
    compiler += stringify(_MSC_FULL_VER) "." stringify(_MSC_BUILD);
    compiler += ")";
#elif defined(__e2k__) && defined(__LCC__)
    #define dot_ver2(n) \
        compiler += char('.'); \
        compiler += char('0' + (n) / 10); \
        compiler += char('0' + (n) % 10);

    compiler += "MCST LCC ";
    compiler += "(version ";
    compiler += std::to_string(__LCC__ / 100);
    dot_ver2(__LCC__ % 100) dot_ver2(__LCC_MINOR__) compiler += ")";
#elif __GNUC__
    compiler += "g++ (GNUC) ";
    compiler += make_version_string(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
    compiler += "Unknown compiler ";
    compiler += "(unknown version)";
#endif

#if defined(__APPLE__)
    compiler += " on Apple";
#elif defined(__CYGWIN__)
    compiler += " on Cygwin";
#elif defined(__MINGW64__)
    compiler += " on MinGW64";
#elif defined(__MINGW32__)
    compiler += " on MinGW32";
#elif defined(__ANDROID__)
    compiler += " on Android";
#elif defined(__linux__)
    compiler += " on Linux";
#elif defined(_WIN64)
    compiler += " on Microsoft Windows 64-bit";
#elif defined(_WIN32)
    compiler += " on Microsoft Windows 32-bit";
#else
    compiler += " on unknown system";
#endif

    compiler += "\nCompilation architecture   : ";
#if defined(ARCH)
    compiler += stringify(ARCH);
#else
    compiler += "(undefined architecture)";
#endif

    compiler += "\nCompilation settings       : ";
    compiler += (Is64Bit ? "64bit" : "32bit");
#if defined(USE_AVX512ICL)
    compiler += " AVX512ICL";
#endif
#if defined(USE_VNNI)
    compiler += " VNNI";
#endif
#if defined(USE_AVX512)
    compiler += " AVX512";
#endif
    compiler += (HasPext ? " BMI2" : "");
#if defined(USE_AVX2)
    compiler += " AVX2";
#endif
#if defined(USE_SSE41)
    compiler += " SSE41";
#endif
#if defined(USE_SSSE3)
    compiler += " SSSE3";
#endif
#if defined(USE_SSE2)
    compiler += " SSE2";
#endif
#if defined(USE_NEON_DOTPROD)
    compiler += " NEON_DOTPROD";
#elif defined(USE_NEON)
    compiler += " NEON";
#endif
#if defined(USE_LASX)
    compiler += " LASX";
#endif
#if defined(USE_LSX)
    compiler += " LSX";
#endif
    compiler += (HasPopCnt ? " POPCNT" : "");

#if !defined(NDEBUG)
    compiler += " DEBUG";
#endif

    compiler += "\nCompiler __VERSION__ macro : ";
#ifdef __VERSION__
    compiler += __VERSION__;
#else
    compiler += "(undefined macro)";
#endif

    compiler += "\n";

    return compiler;
}


// Debug functions used mainly to collect run-time statistics
constexpr int MaxDebugSlots = 32;

namespace {

template<usize N>
struct DebugInfo {
    std::array<std::atomic<i64>, N> data = {0};

    [[nodiscard]] constexpr std::atomic<i64>& operator[](usize index) {
        assert(index < N);
        return data[index];
    }

    constexpr DebugInfo& operator=(const DebugInfo& other) {
        for (usize i = 0; i < N; i++)
            data[i].store(other.data[i].load());
        return *this;
    }
};

struct DebugExtremes: public DebugInfo<3> {
    DebugExtremes() {
        data[1] = std::numeric_limits<i64>::min();
        data[2] = std::numeric_limits<i64>::max();
    }
};

std::array<DebugInfo<2>, MaxDebugSlots>  hit;
std::array<DebugInfo<2>, MaxDebugSlots>  mean;
std::array<DebugInfo<3>, MaxDebugSlots>  stdev;
std::array<DebugInfo<6>, MaxDebugSlots>  correl;
std::array<DebugExtremes, MaxDebugSlots> extremes;

}  // namespace

void dbg_hit_on(bool cond, int slot) {

    ++hit.at(slot)[0];
    if (cond)
        ++hit.at(slot)[1];
}

void dbg_mean_of(i64 value, int slot) {

    ++mean.at(slot)[0];
    mean.at(slot)[1] += value;
}

void dbg_stdev_of(i64 value, int slot) {

    ++stdev.at(slot)[0];
    stdev.at(slot)[1] += value;
    stdev.at(slot)[2] += value * value;
}

void dbg_extremes_of(i64 value, int slot) {
    ++extremes.at(slot)[0];

    i64 current_max = extremes.at(slot)[1].load();
    while (current_max < value && !extremes.at(slot)[1].compare_exchange_weak(current_max, value))
    {}

    i64 current_min = extremes.at(slot)[2].load();
    while (current_min > value && !extremes.at(slot)[2].compare_exchange_weak(current_min, value))
    {}
}

void dbg_correl_of(i64 value1, i64 value2, int slot) {

    ++correl.at(slot)[0];
    correl.at(slot)[1] += value1;
    correl.at(slot)[2] += value1 * value1;
    correl.at(slot)[3] += value2;
    correl.at(slot)[4] += value2 * value2;
    correl.at(slot)[5] += value1 * value2;
}

void dbg_print() {

    i64  n;
    auto E   = [&n](i64 x) { return double(x) / n; };
    auto sqr = [](double x) { return x * x; };

    for (int i = 0; i < MaxDebugSlots; ++i)
        if ((n = hit[i][0]))
            std::cerr << "Hit #" << i << ": Total " << n << " Hits " << hit[i][1]
                      << " Hit Rate (%) " << 100.0 * E(hit[i][1]) << std::endl;

    for (int i = 0; i < MaxDebugSlots; ++i)
        if ((n = mean[i][0]))
        {
            std::cerr << "Mean #" << i << ": Total " << n << " Mean " << E(mean[i][1]) << std::endl;
        }

    for (int i = 0; i < MaxDebugSlots; ++i)
        if ((n = stdev[i][0]))
        {
            double r = sqrt(E(stdev[i][2]) - sqr(E(stdev[i][1])));
            std::cerr << "Stdev #" << i << ": Total " << n << " Stdev " << r << std::endl;
        }

    for (int i = 0; i < MaxDebugSlots; ++i)
        if ((n = extremes[i][0]))
        {
            std::cerr << "Extremity #" << i << ": Total " << n << " Min " << extremes[i][2]
                      << " Max " << extremes[i][1] << std::endl;
        }

    for (int i = 0; i < MaxDebugSlots; ++i)
        if ((n = correl[i][0]))
        {
            double r = (E(correl[i][5]) - E(correl[i][1]) * E(correl[i][3]))
                     / (sqrt(E(correl[i][2]) - sqr(E(correl[i][1])))
                        * sqrt(E(correl[i][4]) - sqr(E(correl[i][3]))));
            std::cerr << "Correl. #" << i << ": Total " << n << " Coefficient " << r << std::endl;
        }
}

void dbg_clear() {
    hit.fill({});
    mean.fill({});
    stdev.fill({});
    correl.fill({});
    extremes.fill({});
}

// Used to serialize access to std::cout
// to avoid multiple threads writing at the same time.
std::ostream& operator<<(std::ostream& os, SyncCout sc) {

    static std::mutex m;

    if (sc == IO_LOCK)
        m.lock();

    if (sc == IO_UNLOCK)
        m.unlock();

    return os;
}

void sync_cout_start() { std::cout << IO_LOCK; }
void sync_cout_end() { std::cout << IO_UNLOCK; }

// Hash function based on public domain MurmurHash64A, by Austin Appleby.
u64 hash_bytes(const char* data, usize size) {
    const u64 m = 0xc6a4a7935bd1e995ull;
    const int r = 47;

    u64 h = size * m;

    const char* end = data + (size & ~(usize) 7);

    for (const char* p = data; p != end; p += 8)
    {
        u64 k;
        std::memcpy(&k, p, sizeof(k));

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    if (size & 7)
    {
        u64 k = 0;
        for (int i = (size & 7) - 1; i >= 0; i--)
            k = (k << 8) | u64(end[i]);

        h ^= k;
        h *= m;
    }

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

// Trampoline helper to avoid moving Logger to misc.h
void start_logger(const fs::path& fname) { Logger::start(fname); }

std::string utf8_from_wstring(std::wstring_view s) {
#ifdef _WIN32
    if (s.empty())
        return {};

    int size =
      WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return {};

    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), out.data(), size, nullptr, nullptr);
    return out;
#else
    return std::string(s.begin(), s.end());
#endif
}

fs::path path_from_utf8(const std::string& path) {
#ifdef _WIN32
    int u8len = static_cast<int>(path.size());
    int wlen  = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), u8len, NULL, 0);

    std::wstring wstr(static_cast<usize>(wlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), u8len, wstr.data(), wlen);
    return {wstr};
#else
    return {path};
#endif
}

CommandLine::CommandLine(int _argc, char** _argv) :
    argc(_argc),
    argv(_argv) {
#ifdef _WIN32
    // Convert any non-ANSI characters passed on the command line to UTF-8
    int wargc = 0;
    if (LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc))
    {
        for (int i = 0; i < wargc; ++i)
            argv_storage.push_back(utf8_from_wstring(wargv[i]));
        LocalFree(wargv);

        for (std::string& s : argv_storage)
            argv_utf8.push_back(s.data());
        argv_utf8.push_back(nullptr);

        argc = wargc;
        argv = argv_utf8.data();
    }
#endif
}


std::optional<usize> str_to_size_t(const std::string& s) {
    if (s.empty() || s[0] == '-')
        return std::nullopt;
    errno                           = 0;
    char*                    endptr = nullptr;
    const unsigned long long value  = std::strtoull(s.c_str(), &endptr, 10);
    if (errno == ERANGE || *endptr != '\0' || value > std::numeric_limits<usize>::max())
        return std::nullopt;
    return static_cast<usize>(value);
}

std::optional<std::string> read_file_to_string(const std::string& path) {
    std::ifstream f(path, std::ios_base::binary);
    if (!f)
        return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

void remove_whitespace(std::string& s) {
    s.erase(std::remove_if(s.begin(), s.end(), [](char c) { return std::isspace(c); }), s.end());
}

bool is_whitespace(std::string_view s) {
    return std::all_of(s.begin(), s.end(), [](char c) { return std::isspace(c); });
}

fs::path CommandLine::get_binary_directory(fs::path argv0) {

#ifdef _WIN32
    #ifdef _MSC_VER
    // Prefer the executable path reported by the CRT when available.
    wchar_t* pgmptr = nullptr;
    if (!_get_wpgmptr(&pgmptr) && pgmptr != nullptr && *pgmptr)
        argv0 = fs::path(pgmptr);
    #endif
#endif

    auto binaryDirectory = argv0.parent_path();
    if (binaryDirectory.empty())
        binaryDirectory = fs::path(".");
    return binaryDirectory;
}

fs::path CommandLine::get_working_directory() { return std::filesystem::current_path(); }

void set_console_utf8() {
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
}

}  // namespace VeloCT

// ==== END OF FILE: misc.cpp ====

// ==== START OF FILE: movegen.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "movegen.h"

#include <cassert>
#include <initializer_list>

#include "attacks.h"
#include "bitboard.h"
#include "position.h"

#if defined(USE_AVX512ICL)
    #include <array>
    #include <algorithm>
    #include <immintrin.h>
#endif

namespace VeloCT {

namespace {

#if defined(USE_AVX512ICL)

template<Direction offset>
inline Move* splat_pawn_moves(Move* moveList, Bitboard to_bb) {
    assert(popcount(to_bb) <= 8);  // <= 8 pawns per side

    const __m128i toSquares =
      _mm_cvtepi8_epi16(_mm512_castsi512_si128(_mm512_maskz_compress_epi8(to_bb, AllSquares)));
    const __m128i fromSquares = _mm_subs_epi16(toSquares, _mm_set1_epi16(offset));
    const __m128i moves       = _mm_or_si128(_mm_slli_epi16(fromSquares, Move::FromSqShift),
                                             _mm_slli_epi16(toSquares, Move::ToSqShift));

    _mm_storeu_si128(reinterpret_cast<__m128i*>(moveList), moves);
    return moveList + popcount(to_bb);
}

inline Move* splat_moves(Move* moveList, Square from, Bitboard to_bb) {
    assert(popcount(to_bb) <= 32);  // Q can attack up to 27 squares

    const __m512i fromVec = _mm512_set1_epi16(Move(from, SQUARE_ZERO).raw());
    const __m512i toSquares =
      _mm512_cvtepi8_epi16(_mm512_castsi512_si256(_mm512_maskz_compress_epi8(to_bb, AllSquares)));
    const __m512i moves = _mm512_or_si512(fromVec, _mm512_slli_epi16(toSquares, Move::ToSqShift));

    _mm512_storeu_si512(moveList, moves);
    return moveList + popcount(to_bb);
}

#else

template<Direction offset>
inline Move* splat_pawn_moves(Move* moveList, Bitboard to_bb) {
    while (to_bb)
    {
        Square to   = pop_lsb(to_bb);
        *moveList++ = Move(to - offset, to);
    }
    return moveList;
}

inline Move* splat_moves(Move* moveList, Square from, Bitboard to_bb) {
    while (to_bb)
        *moveList++ = Move(from, pop_lsb(to_bb));
    return moveList;
}

#endif

template<GenType Type, Direction D, bool Enemy>
Move* make_promotions(Move* moveList, [[maybe_unused]] Square to) {

    constexpr bool all = Type == EVASIONS || Type == NON_EVASIONS;

    if constexpr (Type == CAPTURES || all)
        *moveList++ = Move::make<PROMOTION>(to - D, to, QUEEN);

    if constexpr ((Type == CAPTURES && Enemy) || (Type == QUIETS && !Enemy) || all)
    {
        *moveList++ = Move::make<PROMOTION>(to - D, to, ROOK);
        *moveList++ = Move::make<PROMOTION>(to - D, to, BISHOP);
        *moveList++ = Move::make<PROMOTION>(to - D, to, KNIGHT);
    }

    return moveList;
}


template<Color Us, GenType Type>
Move* generate_pawn_moves(const Position& pos, Move* moveList, Bitboard target) {

    constexpr Color     Them     = ~Us;
    constexpr Bitboard  TRank7BB = (Us == WHITE ? Rank7BB : Rank2BB);
    constexpr Bitboard  TRank3BB = (Us == WHITE ? Rank3BB : Rank6BB);
    constexpr Direction Up       = pawn_push(Us);
    constexpr Direction UpRight  = (Us == WHITE ? NORTH_EAST : SOUTH_WEST);
    constexpr Direction UpLeft   = (Us == WHITE ? NORTH_WEST : SOUTH_EAST);

    const Bitboard emptySquares = ~pos.pieces();
    const Bitboard enemies      = Type == EVASIONS ? pos.checkers() : pos.pieces(Them);

    Bitboard pawnsOn7    = pos.pieces(Us, PAWN) & TRank7BB;
    Bitboard pawnsNotOn7 = pos.pieces(Us, PAWN) & ~TRank7BB;

    // Single and double pawn pushes, no promotions
    if constexpr (Type != CAPTURES)
    {
        Bitboard b1 = shift<Up>(pawnsNotOn7) & emptySquares;
        Bitboard b2 = shift<Up>(b1 & TRank3BB) & emptySquares;

        if constexpr (Type == EVASIONS)  // Consider only blocking squares
        {
            b1 &= target;
            b2 &= target;
        }

        moveList = splat_pawn_moves<Up>(moveList, b1);
        moveList = splat_pawn_moves<Up + Up>(moveList, b2);
    }

    // Promotions and underpromotions
    if (pawnsOn7)
    {
        Bitboard b1 = shift<UpRight>(pawnsOn7) & enemies;
        Bitboard b2 = shift<UpLeft>(pawnsOn7) & enemies;
        Bitboard b3 = shift<Up>(pawnsOn7) & emptySquares;

        if constexpr (Type == EVASIONS)
            b3 &= target;

        while (b1)
            moveList = make_promotions<Type, UpRight, true>(moveList, pop_lsb(b1));

        while (b2)
            moveList = make_promotions<Type, UpLeft, true>(moveList, pop_lsb(b2));

        while (b3)
            moveList = make_promotions<Type, Up, false>(moveList, pop_lsb(b3));
    }

    // Standard and en passant captures
    if constexpr (Type == CAPTURES || Type == EVASIONS || Type == NON_EVASIONS)
    {
        Bitboard b1 = shift<UpRight>(pawnsNotOn7) & enemies;
        Bitboard b2 = shift<UpLeft>(pawnsNotOn7) & enemies;

        moveList = splat_pawn_moves<UpRight>(moveList, b1);
        moveList = splat_pawn_moves<UpLeft>(moveList, b2);

        if (pos.ep_square() != SQ_NONE)
        {
            assert(rank_of(pos.ep_square()) == relative_rank(Us, RANK_6));

            // An en passant capture cannot resolve a discovered check
            if (Type == EVASIONS && (target & (pos.ep_square() + Up)))
                return moveList;

            b1 = pawnsNotOn7 & Attacks::attacks_bb<PAWN>(pos.ep_square(), Them);

            assert(b1);

            while (b1)
                *moveList++ = Move::make<EN_PASSANT>(pop_lsb(b1), pos.ep_square());
        }
    }

    return moveList;
}


template<Color Us, PieceType Pt>
Move* generate_moves(const Position& pos, Move* moveList, Bitboard target) {

    static_assert(Pt != KING && Pt != PAWN, "Unsupported piece type in generate_moves()");

    Bitboard bb = pos.pieces(Us, Pt);

    while (bb)
    {
        Square   from = pop_lsb(bb);
        Bitboard b    = Attacks::attacks_bb<Pt>(from, pos.pieces()) & target;

        moveList = splat_moves(moveList, from, b);
    }

    return moveList;
}


template<Color Us, GenType Type>
Move* generate_all(const Position& pos, Move* moveList) {

    static_assert(Type != LEGAL, "Unsupported type in generate_all()");

    const Square ksq = pos.square<KING>(Us);
    Bitboard     target;

    // Skip generating non-king moves when in double check
    if (Type != EVASIONS || !more_than_one(pos.checkers()))
    {
        target = Type == EVASIONS     ? Attacks::between_bb(ksq, lsb(pos.checkers()))
               : Type == NON_EVASIONS ? ~pos.pieces(Us)
               : Type == CAPTURES     ? pos.pieces(~Us)
                                      : ~pos.pieces();  // QUIETS

        moveList = generate_pawn_moves<Us, Type>(pos, moveList, target);
        moveList = generate_moves<Us, KNIGHT>(pos, moveList, target);
        moveList = generate_moves<Us, BISHOP>(pos, moveList, target);
        moveList = generate_moves<Us, ROOK>(pos, moveList, target);
        moveList = generate_moves<Us, QUEEN>(pos, moveList, target);
    }

    Bitboard b = Attacks::attacks_bb<KING>(ksq) & (Type == EVASIONS ? ~pos.pieces(Us) : target);

    moveList = splat_moves(moveList, ksq, b);

    if ((Type == QUIETS || Type == NON_EVASIONS) && pos.can_castle(Us & ANY_CASTLING))
        for (CastlingRights cr : {Us & KING_SIDE, Us & QUEEN_SIDE})
            if (!pos.castling_impeded(cr) && pos.can_castle(cr))
                *moveList++ = Move::make<CASTLING>(ksq, pos.castling_rook_square(cr));

    return moveList;
}

}  // namespace


// <CAPTURES>     Generates all pseudo-legal captures plus queen promotions
// <QUIETS>       Generates all pseudo-legal non-captures and underpromotions
// <EVASIONS>     Generates all pseudo-legal check evasions
// <NON_EVASIONS> Generates all pseudo-legal captures and non-captures
//
// Returns a pointer to the end of the move list.
template<GenType Type>
Move* generate(const Position& pos, Move* moveList) {

    static_assert(Type != LEGAL, "Unsupported type in generate()");
    assert((Type == EVASIONS) == bool(pos.checkers()));

    Color us = pos.side_to_move();

    return us == WHITE ? generate_all<WHITE, Type>(pos, moveList)
                       : generate_all<BLACK, Type>(pos, moveList);
}

// Explicit template instantiations
template Move* generate<CAPTURES>(const Position&, Move*);
template Move* generate<QUIETS>(const Position&, Move*);
template Move* generate<EVASIONS>(const Position&, Move*);
template Move* generate<NON_EVASIONS>(const Position&, Move*);

// generate<LEGAL> generates all the legal moves in the given position

template<>
Move* generate<LEGAL>(const Position& pos, Move* moveList) {

    Color    us     = pos.side_to_move();
    Bitboard pinned = pos.blockers_for_king(us) & pos.pieces(us);
    Square   ksq    = pos.square<KING>(us);
    Move*    cur    = moveList;

    moveList =
      pos.checkers() ? generate<EVASIONS>(pos, moveList) : generate<NON_EVASIONS>(pos, moveList);
    while (cur != moveList)
        if (((pinned & cur->from_sq()) || cur->from_sq() == ksq || cur->type_of() == EN_PASSANT)
            && !pos.legal(*cur))
            *cur = *(--moveList);
        else
            ++cur;

    return moveList;
}

}  // namespace VeloCT

// ==== END OF FILE: movegen.cpp ====

// ==== START OF FILE: movepick.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "movepick.h"

#include <cassert>
#include <limits>
#include <utility>

#include "bitboard.h"
#include "misc.h"
#include "position.h"

namespace VeloCT {

namespace {

enum Stages {
    // generate main search moves
    MAIN_TT,
    CAPTURE_INIT,
    GOOD_CAPTURE,
    QUIET_INIT,
    GOOD_QUIET,
    BAD_CAPTURE,
    BAD_QUIET,

    // generate evasion moves
    EVASION_TT,
    EVASION_INIT,
    EVASION,

    // generate probcut moves
    PROBCUT_TT,
    PROBCUT_INIT,
    PROBCUT,

    // generate qsearch moves
    QSEARCH_TT,
    QCAPTURE_INIT,
    QCAPTURE
};

#ifdef USE_AVX512
// Load the Move, and the ExtMove value, into all lanes of 512-bit registers
static void splat_extmove(const ExtMove& m, __m512i& move, __m512i& value) {
    move  = _mm512_set1_epi32(m.raw());
    value = _mm512_set1_epi32(m.value);
}

// Sorts up to 16 moves.
struct MoveSorter {
    static constexpr int MAX_ELEMENTS = 16;
    __m512i              sortedValues, sortedMoves;

    explicit MoveSorter(const ExtMove& first) {
        splat_extmove(first, sortedMoves, sortedValues);

        // Set the uninitialized move values to INT_MIN, so that they sort less than any other move
        sortedValues = _mm512_mask_set1_epi32(sortedValues, ~1, std::numeric_limits<int>::min());
    }

    void insert(const ExtMove& m) {
        __m512i move, value;
        splat_extmove(m, move, value);

        // Mask of all elements except the insertion point
        assert(m.value != std::numeric_limits<int>::min());
        const u16 expand = _kadd_mask16(_mm512_cmplt_epi32_mask(sortedValues, value), -1);

        sortedValues = _mm512_mask_expand_epi32(value, expand, sortedValues);
        sortedMoves  = _mm512_mask_expand_epi32(move, expand, sortedMoves);
    }

    void write_sorted(ExtMove* moves, isize count) const {
        static_assert(sizeof(ExtMove) == 8);
        assert(count <= MAX_ELEMENTS);

        // Because values and moves are stored separately, we need to reassemble the ExtMoves
        auto write = [&](int offset, const __m512i indices) {
            const __m512i extMoves = _mm512_permutex2var_epi32(sortedMoves, indices, sortedValues);
            const isize   storeCount = count - offset;

            if (storeCount > 0)
                _mm512_mask_storeu_epi64(moves + offset, (1 << storeCount) - 1, extMoves);
        };

        write(0, _mm512_setr_epi32(0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23));
        write(8, _mm512_setr_epi32(8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31));
    }
};
#endif

// Sort moves in descending order up to and including a given limit.
// The order of moves smaller than the limit is left unspecified.
void partial_insertion_sort(ExtMove* begin, ExtMove* end, int limit) {
    ExtMove *sortedEnd = begin, *p = begin + 1;

#ifdef USE_AVX512
    if (begin == end)
        return;

    MoveSorter sorter(*begin);
    for (; p < end; ++p)
    {
        if (p->value >= limit)
        {
            if (sortedEnd - begin + 1 >= MoveSorter::MAX_ELEMENTS)  // sorter full
                break;

            sorter.insert(*p);
            *p = *++sortedEnd;
        }
    }
    sorter.write_sorted(begin, sortedEnd - begin + 1);
    // Use scalar implementation for any remaining elements
#endif

    for (; p < end; ++p)
        if (p->value >= limit)
        {
            ExtMove tmp = *p, *q;
            *p          = *++sortedEnd;
            for (q = sortedEnd; q != begin && *(q - 1) < tmp; --q)
                *q = *(q - 1);
            *q = tmp;
        }
}

}  // namespace


// Constructors of the MovePicker class. As arguments, we pass information
// to decide which class of moves to emit, to help sorting the (presumably)
// good moves first, and how important move ordering is at the current node.

// MovePicker constructor for the main search and for the quiescence search
MovePicker::MovePicker(const Position&              p,
                       Move                         ttm,
                       Depth                        d,
                       const ButterflyHistory*      mh,
                       const LowPlyHistory*         lph,
                       const CapturePieceToHistory* cph,
                       const PieceToHistory**       ch,
                       const SharedHistories*       sh,
                       int                          pl) :
    pos(p),
    mainHistory(mh),
    lowPlyHistory(lph),
    captureHistory(cph),
    continuationHistory(ch),
    sharedHistory(sh),
    ttMove(ttm),
    depth(d),
    ply(pl) {

    if (pos.checkers())
        stage = EVASION_TT + !(ttm && pos.pseudo_legal(ttm));

    else
        stage = (depth > 0 ? MAIN_TT : QSEARCH_TT) + !(ttm && pos.pseudo_legal(ttm));
}

// MovePicker constructor for ProbCut: we generate captures with Static Exchange
// Evaluation (SEE) greater than or equal to the given threshold.
MovePicker::MovePicker(const Position& p, Move ttm, int th, const CapturePieceToHistory* cph) :
    pos(p),
    captureHistory(cph),
    ttMove(ttm),
    threshold(th) {
    assert(!pos.checkers());

    stage = PROBCUT_TT + !(ttm && pos.capture_stage(ttm) && pos.pseudo_legal(ttm));
}

// Assigns a numerical value to each move in a list, used for sorting.
// Captures are ordered by Most Valuable Victim (MVV), preferring captures
// with a good history. Quiet moves are ordered using the history tables.
template<GenType Type>
ExtMove* MovePicker::score(const MoveList<Type>& ml) {

    static_assert(Type == CAPTURES || Type == QUIETS || Type == EVASIONS, "Wrong type");

    Color us = pos.side_to_move();

    [[maybe_unused]] Bitboard threatByLesser[KING + 1];
    if constexpr (Type == QUIETS)
    {
        threatByLesser[PAWN]   = 0;
        threatByLesser[KNIGHT] = threatByLesser[BISHOP] = pos.attacks_by<PAWN>(~us);
        threatByLesser[ROOK] =
          pos.attacks_by<KNIGHT>(~us) | pos.attacks_by<BISHOP>(~us) | threatByLesser[KNIGHT];
        threatByLesser[QUEEN] = pos.attacks_by<ROOK>(~us) | threatByLesser[ROOK];
        threatByLesser[KING]  = 0;
    }

    ExtMove* it = cur;
    for (auto move : ml)
    {
        ExtMove& m = *it++;
        m          = move;

        const Square    from          = m.from_sq();
        const Square    to            = m.to_sq();
        const Piece     pc            = pos.moved_piece(m);
        const PieceType pt            = type_of(pc);
        const Piece     capturedPiece = pos.piece_on(to);

        if constexpr (Type == CAPTURES)
            m.value = (*captureHistory)[pc][to][type_of(capturedPiece)]
                    + 7 * int(PieceValue[capturedPiece]);

        else if constexpr (Type == QUIETS)
        {
            // histories
            m.value = 2 * (*mainHistory)[us][m.raw()];
            m.value += 2 * sharedHistory->pawn_entry(pos)[pc][to];
            m.value += (*continuationHistory[0])[pc][to];
            m.value += (*continuationHistory[1])[pc][to];
            m.value += (*continuationHistory[2])[pc][to];
            m.value += (*continuationHistory[3])[pc][to];
            m.value += (*continuationHistory[5])[pc][to];

            // bonus for checks
            m.value += ((pos.check_squares(pt) & to) && pos.see_ge(m, -75)) * 16384;

            // penalty for moving to a square threatened by a lesser piece
            // or bonus for escaping an attack by a lesser piece.
            int v = 20 * (bool(threatByLesser[pt] & from) - bool(threatByLesser[pt] & to));
            m.value += PieceValue[pt] * v;


            if (ply < LOW_PLY_HISTORY_SIZE)
                m.value += 8 * (*lowPlyHistory)[ply][m.raw()] / (1 + ply);
        }

        else  // Type == EVASIONS
        {
            if (pos.capture_stage(m))
                m.value = PieceValue[capturedPiece] + (1 << 28);
            else
                m.value = (*mainHistory)[us][m.raw()] + (*continuationHistory[0])[pc][to];
        }
    }
    return it;
}

// Returns the next move satisfying a predicate function.
// This never returns the TT move, as it was emitted before.
template<typename Pred>
Move MovePicker::select(Pred filter) {

    for (; cur < endCur; ++cur)
        if (*cur != ttMove && filter())
            return *cur++;

    return Move::none();
}

// This is the most important method of the MovePicker class. We emit one
// new pseudo-legal move on every call until there are no more moves left,
// picking the move with the highest score from a list of generated moves.
Move MovePicker::next_move() {

    constexpr int goodQuietThreshold = -14000;
top:
    switch (stage)
    {

    case MAIN_TT :
    case EVASION_TT :
    case QSEARCH_TT :
    case PROBCUT_TT :
        ++stage;
        return ttMove;

    case CAPTURE_INIT :
    case PROBCUT_INIT :
    case QCAPTURE_INIT : {
        MoveList<CAPTURES> ml(pos);

        cur = endBadCaptures = moves;
        endCur = endCaptures = score<CAPTURES>(ml);

        partial_insertion_sort(cur, endCur, std::numeric_limits<int>::min());
        ++stage;
        goto top;
    }

    case GOOD_CAPTURE :
        if (select([&]() {
                if (pos.see_ge(*cur, -cur->value / 18))
                    return true;
                std::swap(*endBadCaptures++, *cur);
                return false;
            }))
            return *(cur - 1);

        ++stage;
        [[fallthrough]];

    case QUIET_INIT :
        if (!skipQuiets)
        {
            MoveList<QUIETS> ml(pos);

            endCur = endGenerated = score<QUIETS>(ml);

            partial_insertion_sort(cur, endCur, -3560 * depth);
        }

        ++stage;
        [[fallthrough]];

    case GOOD_QUIET :
        if (!skipQuiets && select([&]() { return cur->value > goodQuietThreshold; }))
            return *(cur - 1);

        // Prepare the pointers to loop over the bad captures
        cur    = moves;
        endCur = endBadCaptures;

        ++stage;
        [[fallthrough]];

    case BAD_CAPTURE :
        if (select([]() { return true; }))
            return *(cur - 1);

        // Prepare the pointers to loop over quiets again
        cur    = endCaptures;
        endCur = endGenerated;

        ++stage;
        [[fallthrough]];

    case BAD_QUIET :
        if (!skipQuiets)
            return select([&]() { return cur->value <= goodQuietThreshold; });

        return Move::none();

    case EVASION_INIT : {
        MoveList<EVASIONS> ml(pos);

        cur    = moves;
        endCur = endGenerated = score<EVASIONS>(ml);

        partial_insertion_sort(cur, endCur, std::numeric_limits<int>::min());
        ++stage;
        [[fallthrough]];
    }

    case EVASION :
    case QCAPTURE :
        return select([]() { return true; });

    case PROBCUT :
        return select([&]() { return pos.see_ge(*cur, threshold); });
    }

    assert(false);
    return Move::none();  // Silence warning
}

void MovePicker::skip_quiet_moves() { skipQuiets = true; }

}  // namespace VeloCT

// ==== END OF FILE: movepick.cpp ====

// ==== START OF FILE: position.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "position.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>
#include <utility>

#include "bitboard.h"
#include "history.h"
#include "misc.h"
#include "movegen.h"
#include "syzygy/tbprobe.h"
#include "tt.h"
#include "uci.h"

using std::string;

namespace VeloCT {

using namespace Attacks;

namespace Zobrist {

Key psq[PIECE_NB][SQUARE_NB];
Key enpassant[FILE_NB];
Key castling[CASTLING_RIGHT_NB];
Key side, noPawns;

}

namespace {

constexpr std::string_view PieceToChar(" PNBRQK  pnbrqk");

static constexpr Piece Pieces[] = {W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
                                   B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING};
}  // namespace


// Returns an ASCII representation of the position
std::ostream& operator<<(std::ostream& os, const Position& pos) {

    os << "\n +---+---+---+---+---+---+---+---+\n";

    for (Rank r = RANK_8;; --r)
    {
        for (File f = FILE_A; f <= FILE_H; ++f)
            os << " | " << PieceToChar[pos.piece_on(make_square(f, r))];

        os << " | " << (1 + r) << "\n +---+---+---+---+---+---+---+---+\n";

        if (r == RANK_1)
            break;
    }

    os << "   a   b   c   d   e   f   g   h\n"
       << "\nFen: " << pos.fen() << "\nKey: " << std::hex << std::uppercase << std::setfill('0')
       << std::setw(16) << pos.key() << std::setfill(' ') << std::dec << "\nCheckers: ";

    for (Bitboard b = pos.checkers(); b;)
        os << UCIEngine::square(pop_lsb(b)) << " ";

    if (Tablebases::MaxCardinality >= popcount(pos.pieces()) && !pos.can_castle(ANY_CASTLING))
    {
        StateInfo st;

        Position p;
        p.set(pos.fen(), pos.is_chess960(), &st);
        Tablebases::ProbeState s1, s2;
        Tablebases::WDLScore   wdl = Tablebases::probe_wdl(p, &s1);
        int                    dtz = Tablebases::probe_dtz(p, &s2);
        os << "\nTablebases WDL: " << std::setw(4) << wdl << " (" << s1 << ")"
           << "\nTablebases DTZ: " << std::setw(4) << dtz << " (" << s2 << ")";
    }

    return os;
}


// Implements Marcel van Kervinck's cuckoo algorithm to detect repetition of positions
// for 3-fold repetition draws. The algorithm uses two hash tables with Zobrist hashes
// to allow fast detection of recurring positions. For details see:
// http://web.archive.org/web/20201107002606/https://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf

// First and second hash functions for indexing the cuckoo tables
inline int H1(Key h) { return h & 0x1fff; }
inline int H2(Key h) { return (h >> 16) & 0x1fff; }

// Cuckoo tables with Zobrist hashes of valid reversible moves, and the moves themselves
static std::array<Key, 8192>  cuckoo;
static std::array<Move, 8192> cuckooMove;

// Initializes at startup the various arrays used to compute hash keys
void Position::init() {

    PRNG rng(1070372);

    for (Piece pc : Pieces)
        for (Square s = SQ_A1; s <= SQ_H8; ++s)
            Zobrist::psq[pc][s] = rng.rand<Key>();
    // pawns on these squares will promote
    std::fill_n(Zobrist::psq[W_PAWN] + SQ_A8, 8, 0);
    std::fill_n(Zobrist::psq[B_PAWN], 8, 0);

    for (File f = FILE_A; f <= FILE_H; ++f)
        Zobrist::enpassant[f] = rng.rand<Key>();

    for (int cr = NO_CASTLING; cr <= ANY_CASTLING; ++cr)
        Zobrist::castling[cr] = rng.rand<Key>();

    Zobrist::side    = rng.rand<Key>();
    Zobrist::noPawns = rng.rand<Key>();

    // Prepare the cuckoo tables
    cuckoo.fill(0);
    cuckooMove.fill(Move::none());
    [[maybe_unused]] int count = 0;
    for (Piece pc : Pieces)
        for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1)
            for (Square s2 = Square(s1 + 1); s2 <= SQ_H8; ++s2)
                if ((type_of(pc) != PAWN) && (attacks_bb(type_of(pc), s1, 0) & s2))
                {
                    Move move = Move(s1, s2);
                    Key  key  = Zobrist::psq[pc][s1] ^ Zobrist::psq[pc][s2] ^ Zobrist::side;
                    int  i    = H1(key);
                    while (true)
                    {
                        std::swap(cuckoo[i], key);
                        std::swap(cuckooMove[i], move);
                        if (move == Move::none())  // Arrived at empty slot?
                            break;
                        i = (i == H1(key)) ? H2(key) : H1(key);  // Push victim to alternative slot
                    }
                    count++;
                }
    assert(count == 3668);
}


// Initializes the position object with the given FEN string.
// The FEN string is strictly validated; if it is invalid or inconsistent,
// a PositionSetError describing the problem is returned, otherwise std::nullopt.
std::optional<PositionSetError>
Position::set(const string& fenStr, bool isChess960, StateInfo* si) {
    /*
   A FEN string defines a particular position using only the ASCII character set.

   A FEN string contains six fields separated by a space. The fields are:

   1) Piece placement (from white's perspective). Each rank is described, starting
      with rank 8 and ending with rank 1. Within each rank, the contents of each
      square are described from file A through file H. Following the Standard
      Algebraic Notation (SAN), each piece is identified by a single letter taken
      from the standard English names. White pieces are designated using upper-case
      letters ("PNBRQK") whilst Black uses lowercase ("pnbrqk"). Blank squares are
      noted using digits 1 through 8 (the number of blank squares), and "/"
      separates ranks.

   2) Active color. "w" means white moves next, "b" means black.

   3) Castling availability. If neither side can castle, this is "-". Otherwise,
      this has one or more letters: "K" (White can castle kingside), "Q" (White
      can castle queenside), "k" (Black can castle kingside), and/or "q" (Black
      can castle queenside).

   4) En passant target square (in algebraic notation). If there's no en passant
      target square, this is "-". If a pawn has just made a 2-square move, this
      is the position "behind" the pawn. Following X-FEN standard, this is recorded
      only if there is a pawn in position to make an en passant capture, and if
      there really is a pawn that might have advanced two squares.

   5) Halfmove clock. This is the number of halfmoves since the last pawn advance
      or capture. This is used to determine if a draw can be claimed under the
      fifty-move rule.

   6) Fullmove number. The number of the full move. It starts at 1, and is
      incremented after Black's move.
*/

    unsigned char      token;
    std::istringstream ss(fenStr);

    std::memset(reinterpret_cast<char*>(this), 0, sizeof(Position));
    std::memset(si, 0, sizeof(StateInfo));
    st = si;

    ss >> std::noskipws;

    int numPieces = 0;
    int file      = FILE_A;
    int rank      = RANK_8;

    // 1. Piece placement
    for (;;)
    {
        if (!(ss >> token))
            return PositionSetError("Invalid FEN. Unexpected end of stream.");

        if (isspace(token))
            break;

        if (isdigit(token))
        {
            const int diff = (token - '0');
            if (diff < 1 || diff > 8)
                return PositionSetError("Invalid FEN. Invalid number of squares to skip.");

            file += diff;
            if (file > FILE_NB)
                return PositionSetError("Invalid FEN. Invalid file reached.");
        }
        else if (token == '/')
        {
            if (file != FILE_NB)
                return PositionSetError(
                  "Invalid FEN. Trying to end rank when not at the end of it.");

            --rank;
            file = FILE_A;

            if (rank < RANK_1)
                return PositionSetError("Invalid FEN. Invalid rank reached.");
        }
        else
        {
            if (file >= FILE_NB)
                return PositionSetError("Invalid FEN. Invalid file reached.");

            const usize idx = PieceToChar.find(token);
            if (idx == string::npos)
                return PositionSetError(std::string("Invalid FEN. Invalid piece: ")
                                        + std::string(1, token));

            if (++numPieces > 32)
                return PositionSetError("Invalid FEN. More than 32 pieces on the board.");

            const Square sq = make_square(File(file), Rank(rank));
            put_piece(Piece(idx), sq);

            ++file;
        }
    }

    if (rank != RANK_1 || file != FILE_NB)
        return PositionSetError("Invalid FEN. Board state encoding ended but cursor not at end.");

    if (pieces(PAWN) & (Rank1BB | Rank8BB))
        return PositionSetError("Unsupported position. Pawns on the first or eighth rank.");

    if (count<KING>(WHITE) != 1 || count<KING>(BLACK) != 1)
        return PositionSetError("Unsupported position. Incorrect number of kings.");

    for (Color c : {WHITE, BLACK})
    {
        if (count<PAWN>(c) > 8)
            return PositionSetError(std::string("Unsupported position. ")
                                    + (c == WHITE ? "WHITE" : "BLACK") + " has more than 8 pawns.");

        int additional = std::max(count<KNIGHT>(c) - 2, 0) + std::max(count<BISHOP>(c) - 2, 0)
                       + std::max(count<ROOK>(c) - 2, 0) + std::max(count<QUEEN>(c) - 1, 0);
        if (additional > 8 - count<PAWN>(c))
            return PositionSetError(std::string("Unsupported position. Too many pieces for ")
                                    + (c == WHITE ? "WHITE." : "BLACK."));
    }

    // 2. Active color
    if (!(ss >> token))
        return PositionSetError("Invalid FEN. Unexpected end of stream.");
    if (token != 'w' && token != 'b')
        return PositionSetError(std::string("Invalid FEN. Invalid side to move: ")
                                + std::string(1, token));
    sideToMove = (token == 'w' ? WHITE : BLACK);
    if (!(ss >> token) || !isspace(token) || ss.eof())
        return PositionSetError("Invalid FEN. Expected whitespace after side to move.");

    // 3. Castling availability. Compatible with 3 standards: Normal FEN standard,
    // Shredder-FEN that uses the letters of the columns on which the rooks began
    // the game instead of KQkq and also X-FEN standard that, in case of Chess960,
    // if an inner rook is associated with the castling right, the castling tag is
    // replaced by the file letter of the involved rook, as for the Shredder-FEN.
    //
    // NOTE: Due to the prevalence of incorrect (or missing) castling rights the
    // validation is less strict. However, incorrect castling rights are still sanitized.
    int num_castling_rights = 0;
    for (;;)
    {
        if (!(ss >> token))
            break;

        if (isspace(token))
            break;

        if (num_castling_rights == 0 && token == '-')
        {
            ss >> std::ws;
            break;
        }

        if (++num_castling_rights > 4)
            return PositionSetError("Invalid FEN. Maximum of 4 castling rights can be specified.");

        Square rsq  = SQ_NONE;
        Square ksq  = SQ_NONE;
        Color  c    = islower(token) ? BLACK : WHITE;
        Piece  rook = make_piece(c, ROOK);
        Piece  king = make_piece(c, KING);

        token = char(toupper(token));

        if (token == 'K' || token == 'Q')
        {
            const int dir = token == 'K' ? -1 : 1;
            Square    sq  = relative_square(c, token == 'K' ? SQ_H1 : SQ_A1);
            // Look for a rook and a king for the castling. King must come later.
            // Only the first rook is noted.
            // If the castling rights are available the king must always be between files 2 and 7 inclusive
            // so there is no need to check the last square.
            for (int i = 0; i < 7; ++i, sq = Square(sq + dir))
            {
                const Piece pc = piece_on(sq);
                if (pc == king)
                {
                    ksq = sq;
                    break;
                }
                else if (pc == rook && rsq == SQ_NONE)
                {
                    rsq = sq;
                }
            }
        }
        else if (token >= 'A' && token <= 'H')
        {
            const Square rsqCandidate = make_square(File(token - 'A'), relative_rank(c, RANK_1));
            if (piece_on(rsqCandidate) == rook)
                rsq = rsqCandidate;

            // If the castling rights are available the king must always be between files 2 and 7 inclusive.
            Square sq = relative_square(c, SQ_B1);
            for (int i = 0; i < 6; ++i, ++sq)
            {
                if (piece_on(sq) == king)
                    ksq = sq;
            }
        }
        else
        {
            return PositionSetError(std::string("Invalid FEN. Expected castling rights. Got: ")
                                    + std::string(1, token));
        }

        // Only apply castling rights if they can be valid.
        if (ksq != SQ_NONE && rsq != SQ_NONE)
            set_castling_right(c, rsq);
    }

    // 4. En passant square.
    // Ignore if square is invalid or not on side to move relative rank 6.
    bool          enpassant = false, legalEP = false;
    unsigned char col = '-', row;
    ss >> col;
    if (col != '-')
    {
        if (!(ss >> row))
            return PositionSetError("Invalid FEN. Unexpected end of stream.");

        if ((col >= 'a' && col <= 'h') && (row == (sideToMove == WHITE ? '6' : '3')))
        {
            st->epSquare = make_square(File(col - 'a'), Rank(row - '1'));

            Bitboard pawns = attacks_bb<PAWN>(st->epSquare, ~sideToMove) & pieces(sideToMove, PAWN);
            Bitboard target = (pieces(~sideToMove, PAWN) & (st->epSquare + pawn_push(~sideToMove)));
            Bitboard occ    = pieces() ^ target ^ st->epSquare;

            // En passant square will be considered only if
            // a) side to move have a pawn threatening epSquare
            // b) there is an enemy pawn in front of epSquare
            // c) there is no piece on epSquare or behind epSquare
            enpassant = pawns && target
                     && !(pieces() & (st->epSquare | (st->epSquare + pawn_push(sideToMove))));

            // If no pawn can execute the en passant capture without leaving the king in check, don't record the epSquare
            while (pawns)
                legalEP |= !(attackers_to(square<KING>(sideToMove), occ ^ pop_lsb(pawns))
                             & pieces(~sideToMove) & ~target);
        }
        else
            return PositionSetError("Invalid FEN. Invalid en-passant square.");
    }

    if (!enpassant || !legalEP)
        st->epSquare = SQ_NONE;

    // 5-6. Halfmove clock and fullmove number
    ss >> std::skipws >> st->rule50 >> gamePly;

    // Normally values larger than 99 would be pointless but we do support ignoring 50 move rule for TB purposes.
    // Limit at 2**15 as it's used multiplicatively with position evaluation during search.
    if (st->rule50 < 0 || st->rule50 > 32767)
        return PositionSetError("Unsupported position. Rule50 counter out of range.");

    if (gamePly < 0 || gamePly > 100000)
        return PositionSetError("Unsupported position. Game ply out of range.");

    // Convert from fullmove starting from 1 to gamePly starting from 0,
    // handle also common incorrect FEN with fullmove = 0.
    gamePly = std::max(2 * (gamePly - 1), 0) + (sideToMove == BLACK);

    chess960 = isChess960;
    set_state();

    if (attackers_to_exist(square<KING>(~sideToMove), pieces(), sideToMove))
        return PositionSetError("Unsupported position. King can be captured.");

    assert(pos_is_ok());

    return std::nullopt;
}


// Helper function used to set castling
// rights given the corresponding color and the rook starting square.
void Position::set_castling_right(Color c, Square rfrom) {

    Square         kfrom = square<KING>(c);
    CastlingRights cr    = c & (kfrom < rfrom ? KING_SIDE : QUEEN_SIDE);

    st->castlingRights |= cr;
    castlingRightsMask[kfrom] |= cr;
    castlingRightsMask[rfrom] |= cr;
    castlingRookSquare[cr] = rfrom;

    Square kto = relative_square(c, cr & KING_SIDE ? SQ_G1 : SQ_C1);
    Square rto = relative_square(c, cr & KING_SIDE ? SQ_F1 : SQ_D1);

    castlingPath[cr] = (between_bb(rfrom, rto) | between_bb(kfrom, kto)) & ~(kfrom | rfrom);
}


// Sets king attacks to detect if a move gives check
void Position::set_check_info() const {

    update_slider_blockers(WHITE);
    update_slider_blockers(BLACK);

    Square ksq = square<KING>(~sideToMove);

    st->checkSquares[PAWN]   = attacks_bb<PAWN>(ksq, ~sideToMove);
    st->checkSquares[KNIGHT] = attacks_bb<KNIGHT>(ksq);
    st->checkSquares[BISHOP] = attacks_bb<BISHOP>(ksq, pieces());
    st->checkSquares[ROOK]   = attacks_bb<ROOK>(ksq, pieces());
    st->checkSquares[QUEEN]  = st->checkSquares[BISHOP] | st->checkSquares[ROOK];
    st->checkSquares[KING]   = 0;
}


// Computes the hash keys of the position, and other
// data that once computed is updated incrementally as moves are made.
// The function is only used when a new position is set up
void Position::set_state() const {

    st->key               = 0;
    st->minorPieceKey     = 0;
    st->nonPawnKey[WHITE] = st->nonPawnKey[BLACK] = 0;
    st->pawnKey                                   = Zobrist::noPawns;
    st->nonPawnMaterial[WHITE] = st->nonPawnMaterial[BLACK] = VALUE_ZERO;
    st->checkersBB = attackers_to(square<KING>(sideToMove)) & pieces(~sideToMove);

    set_check_info();

    for (Bitboard b = pieces(); b;)
    {
        Square s  = pop_lsb(b);
        Piece  pc = piece_on(s);
        st->key ^= Zobrist::psq[pc][s];

        if (type_of(pc) == PAWN)
            st->pawnKey ^= Zobrist::psq[pc][s];

        else
        {
            st->nonPawnKey[color_of(pc)] ^= Zobrist::psq[pc][s];

            if (type_of(pc) != KING)
            {
                st->nonPawnMaterial[color_of(pc)] += PieceValue[pc];

                if (type_of(pc) <= BISHOP)
                    st->minorPieceKey ^= Zobrist::psq[pc][s];
            }
        }
    }

    if (st->epSquare != SQ_NONE)
        st->key ^= Zobrist::enpassant[file_of(st->epSquare)];

    if (sideToMove == BLACK)
        st->key ^= Zobrist::side;

    st->key ^= Zobrist::castling[st->castlingRights];
    st->materialKey = compute_material_key();
}

Key Position::compute_material_key() const {
    Key k = 0;
    for (Piece pc : Pieces)
        for (int cnt = 0; cnt < pieceCount[pc]; ++cnt)
            k ^= Zobrist::psq[pc][8 + cnt];
    return k;
}


// Overload to initialize the position object with the given endgame code string
// like "KBPKN". It's mainly a helper to get the material key out of an endgame code.
std::optional<PositionSetError> Position::set(const string& code, Color c, StateInfo* si) {

    assert(code[0] == 'K');

    string sides[] = {code.substr(code.find('K', 1)),                                // Weak
                      code.substr(0, std::min(code.find('v'), code.find('K', 1)))};  // Strong

    assert(sides[0].length() > 0 && sides[0].length() < 8);
    assert(sides[1].length() > 0 && sides[1].length() < 8);

    std::transform(sides[c].begin(), sides[c].end(), sides[c].begin(), tolower);

    string fenStr = "8/" + sides[0] + char(8 - sides[0].length() + '0') + "/8/8/8/8/" + sides[1]
                  + char(8 - sides[1].length() + '0') + "/8 w - - 0 10";

    return set(fenStr, false, si);
}


// Returns a FEN representation of the position. In case of
// Chess960 the Shredder-FEN notation is used. This is mainly a debugging function.
string Position::fen() const {

    int                emptyCnt;
    std::ostringstream ss;

    for (Rank r = RANK_8;; --r)
    {
        for (File f = FILE_A; f <= FILE_H; ++f)
        {
            for (emptyCnt = 0; f <= FILE_H && empty(make_square(f, r)); ++f)
                ++emptyCnt;

            if (emptyCnt)
                ss << emptyCnt;

            if (f <= FILE_H)
                ss << PieceToChar[piece_on(make_square(f, r))];
        }

        if (r == RANK_1)
            break;
        ss << '/';
    }

    ss << (sideToMove == WHITE ? " w " : " b ");

    if (can_castle(WHITE_OO))
        ss << (chess960 ? char('A' + file_of(castling_rook_square(WHITE_OO))) : 'K');

    if (can_castle(WHITE_OOO))
        ss << (chess960 ? char('A' + file_of(castling_rook_square(WHITE_OOO))) : 'Q');

    if (can_castle(BLACK_OO))
        ss << (chess960 ? char('a' + file_of(castling_rook_square(BLACK_OO))) : 'k');

    if (can_castle(BLACK_OOO))
        ss << (chess960 ? char('a' + file_of(castling_rook_square(BLACK_OOO))) : 'q');

    if (!can_castle(ANY_CASTLING))
        ss << '-';

    ss << (ep_square() == SQ_NONE ? " - " : " " + UCIEngine::square(ep_square()) + " ")
       << st->rule50 << " " << 1 + (gamePly - (sideToMove == BLACK)) / 2;

    return ss.str();
}

// Calculates st->blockersForKing[c] and st->pinners[~c],
// which store respectively the pieces preventing king of color c from being in check
// and the slider pieces of color ~c pinning pieces of color c to the king.
void Position::update_slider_blockers(Color c) const {

    Square ksq = square<KING>(c);

    st->blockersForKing[c] = 0;
    st->pinners[~c]        = 0;

    // Snipers are sliders that attack 's' when a piece and other snipers are removed
    Bitboard snipers = ((attacks_bb<ROOK>(ksq) & pieces(QUEEN, ROOK))
                        | (attacks_bb<BISHOP>(ksq) & pieces(QUEEN, BISHOP)))
                     & pieces(~c);
    Bitboard occupancy = pieces() ^ snipers;

    while (snipers)
    {
        Square   sniperSq = pop_lsb(snipers);
        Bitboard b        = between_bb(ksq, sniperSq) & occupancy;

        if (b && !more_than_one(b))
        {
            st->blockersForKing[c] |= b;
            if (b & pieces(c))
                st->pinners[~c] |= sniperSq;
        }
    }
}


// Computes a bitboard of all pieces which attack a given square.
// Slider attacks use the occupied bitboard to indicate occupancy.
Bitboard Position::attackers_to(Square s, Bitboard occupied) const {

    return (attacks_bb<ROOK>(s, occupied) & pieces(ROOK, QUEEN))
         | (attacks_bb<BISHOP>(s, occupied) & pieces(BISHOP, QUEEN))
         | (attacks_bb<PAWN>(s, BLACK) & pieces(WHITE, PAWN))
         | (attacks_bb<PAWN>(s, WHITE) & pieces(BLACK, PAWN))
         | (attacks_bb<KNIGHT>(s) & pieces(KNIGHT)) | (attacks_bb<KING>(s) & pieces(KING));
}

bool Position::attackers_to_exist(Square s, Bitboard occupied, Color c) const {

    return (attacks_bb<ROOK>(s, occupied) & pieces(c, ROOK, QUEEN))
        || (attacks_bb<BISHOP>(s, occupied) & pieces(c, BISHOP, QUEEN))
        || (attacks_bb<PAWN>(s, ~c) & pieces(c, PAWN))
        || (attacks_bb<KNIGHT>(s) & pieces(c, KNIGHT)) || (attacks_bb<KING>(s) & pieces(c, KING));
}

// Tests whether a pseudo-legal move is legal
bool Position::legal(Move m) const {

    assert(m.is_ok());

    Color  us   = sideToMove;
    Square from = m.from_sq();
    Square to   = m.to_sq();

    assert(color_of(moved_piece(m)) == us);
    assert(piece_on(square<KING>(us)) == make_piece(us, KING));

    // Castling moves generation does not check if the castling path is clear of
    // enemy attacks, it is delayed at a later time: now!
    if (m.type_of() == CASTLING)
    {
        // After castling, the rook and king final positions are the same in
        // Chess960 as they would be in standard chess.
        to             = relative_square(us, to > from ? SQ_G1 : SQ_C1);
        Direction step = to > from ? WEST : EAST;

        for (Square s = to; s != from; s += step)
            if (attackers_to_exist(s, pieces(), ~us))
                return false;

        // In case of Chess960, verify if the Rook blocks some checks.
        // For instance an enemy queen in SQ_A1 when castling rook is in SQ_B1.
        return !chess960 || !(blockers_for_king(us) & m.to_sq());
    }

    // If the moving piece is a king, check whether the destination square is
    // attacked by the opponent.
    if (type_of(piece_on(from)) == KING)
        return !(attackers_to_exist(to, pieces() ^ from, ~us));

    // A non-king move is legal if and only if it is not pinned or it
    // is moving along the ray towards or away from the king.
    return !(blockers_for_king(us) & from) || line_bb(from, to) & pieces(us, KING);
}


// Takes a random move and tests whether the move is
// pseudo-legal. It is used to validate moves from TT that can be corrupted
// due to SMP concurrent access or hash position key aliasing.
bool Position::pseudo_legal(const Move m) const {

    Color  us   = sideToMove;
    Square from = m.from_sq();
    Square to   = m.to_sq();
    Piece  pc   = moved_piece(m);

    // Use a slower but simpler function for uncommon cases
    // yet we skip the legality check of MoveList<LEGAL>().
    if (m.type_of() != NORMAL)
        return checkers() ? MoveList<EVASIONS>(*this).contains(m)
                          : MoveList<NON_EVASIONS>(*this).contains(m);

    // Is not a promotion, so the promotion piece must be empty
    assert(m.promotion_type() - KNIGHT == NO_PIECE_TYPE);

    // If the 'from' square is not occupied by a piece belonging to the side to
    // move, the move is obviously not legal.
    if (pc == NO_PIECE || color_of(pc) != us)
        return false;

    // The destination square cannot be occupied by a friendly piece
    if (pieces(us) & to)
        return false;

    // Handle the special case of a pawn move
    if (type_of(pc) == PAWN)
    {
        // We have already handled promotion moves, so destination cannot be on the 8th/1st rank
        if ((Rank8BB | Rank1BB) & to)
            return false;

        // Check if it's a valid capture, single push, or double push
        const bool isCapture    = bool(attacks_bb<PAWN>(from, us) & pieces(~us) & to);
        const bool isSinglePush = (from + pawn_push(us) == to) && empty(to);
        const bool isDoublePush = (from + 2 * pawn_push(us) == to)
                               && (relative_rank(us, from) == RANK_2) && empty(to)
                               && empty(to - pawn_push(us));

        if (!(isCapture || isSinglePush || isDoublePush))
            return false;
    }
    else if (!(attacks_bb(type_of(pc), from, pieces()) & to))
        return false;

    if (checkers())
        return MoveList<EVASIONS>(*this).contains(m);

    return true;
}


// Tests whether a pseudo-legal move gives a check
bool Position::gives_check(Move m) const {

    assert(m.is_ok());
    assert(color_of(moved_piece(m)) == sideToMove);

    Square from = m.from_sq();
    Square to   = m.to_sq();

    // Is there a direct check?
    if (check_squares(type_of(piece_on(from))) & to)
        return true;

    // Is there a discovered check?
    if (blockers_for_king(~sideToMove) & from)
        return !(line_bb(from, to) & pieces(~sideToMove, KING)) || m.type_of() == CASTLING;

    switch (m.type_of())
    {
    case NORMAL :
        return false;

    case PROMOTION :
        return attacks_bb(m.promotion_type(), to, pieces() ^ from) & pieces(~sideToMove, KING);

    // En passant capture with check? We have already handled the case of direct
    // checks and ordinary discovered check, so the only case we need to handle
    // is the unusual case of a discovered check through the captured pawn.
    case EN_PASSANT : {
        Square   capsq = make_square(file_of(to), rank_of(from));
        Bitboard b     = (pieces() ^ from ^ capsq) | to;

        return (attacks_bb<ROOK>(square<KING>(~sideToMove), b) & pieces(sideToMove, QUEEN, ROOK))
             | (attacks_bb<BISHOP>(square<KING>(~sideToMove), b)
                & pieces(sideToMove, QUEEN, BISHOP));
    }
    default :  //CASTLING
    {
        // Castling is encoded as 'king captures the rook'
        Square rto = relative_square(sideToMove, to > from ? SQ_F1 : SQ_D1);

        return check_squares(ROOK) & rto;
    }
    }
}


// Makes a move, and saves all information necessary
// to a StateInfo object. The move is assumed to be legal. Pseudo-legal
// moves should be filtered out before this function is called.
// If a pointer to the TT table is passed, the entry for the new position
// will be prefetched, and likewise for shared history.
void Position::do_move(Move                      m,
                       StateInfo&                newSt,
                       bool                      givesCheck,
                       DirtyPiece&               dp,
                       DirtyThreats&             dts,
                       const TranspositionTable* tt      = nullptr,
                       const SharedHistories*    history = nullptr) {

    assert(m.is_ok());
    assert(&newSt != st);

    Key k = st->key ^ Zobrist::side;

    // Copy some fields of the old state to our new StateInfo object except the
    // ones which are going to be recalculated from scratch anyway and then switch
    // our state pointer to point to the new (ready to be updated) state.
    std::memcpy(&newSt, st, offsetof(StateInfo, key));
    newSt.previous = st;
    st             = &newSt;

    // Increment ply counters. In particular, rule50 will be reset to zero later on
    // in case of a capture or a pawn move.
    ++gamePly;
    ++st->rule50;
    ++st->pliesFromNull;

    Color  us       = sideToMove;
    Color  them     = ~us;
    Square from     = m.from_sq();
    Square to       = m.to_sq();
    Piece  pc       = piece_on(from);
    Piece  captured = m.type_of() == EN_PASSANT ? make_piece(them, PAWN) : piece_on(to);

    dp.pc     = pc;
    dp.from   = from;
    dp.to     = to;
    dp.add_sq = SQ_NONE;

    assert(color_of(pc) == us);
    assert(captured == NO_PIECE || color_of(captured) == (m.type_of() != CASTLING ? them : us));
    assert(type_of(captured) != KING);

    if (m.type_of() == CASTLING)
    {
        assert(pc == make_piece(us, KING));
        assert(captured == make_piece(us, ROOK));

        Square rfrom, rto;
        do_castling<true>(us, from, to, rfrom, rto, &dts, &dp);

        k ^= Zobrist::psq[captured][rfrom] ^ Zobrist::psq[captured][rto];
        st->nonPawnKey[us] ^= Zobrist::psq[captured][rfrom] ^ Zobrist::psq[captured][rto];
        captured = NO_PIECE;
    }
    else if (captured)
    {
        Square capsq = to;

        // If the captured piece is a pawn, update pawn hash key, otherwise
        // update non-pawn material.
        if (type_of(captured) == PAWN)
        {
            if (m.type_of() == EN_PASSANT)
            {
                capsq -= pawn_push(us);

                assert(pc == make_piece(us, PAWN));
                assert(to == st->epSquare);
                assert(relative_rank(us, to) == RANK_6);
                assert(piece_on(to) == NO_PIECE);
                assert(piece_on(capsq) == make_piece(them, PAWN));

                // Update board and piece lists in ep case, normal captures are updated later
                remove_piece(capsq, &dts);
            }

            st->pawnKey ^= Zobrist::psq[captured][capsq];
        }
        else
        {
            st->nonPawnMaterial[them] -= PieceValue[captured];
            st->nonPawnKey[them] ^= Zobrist::psq[captured][capsq];

            if (type_of(captured) <= BISHOP)
                st->minorPieceKey ^= Zobrist::psq[captured][capsq];
        }

        dp.remove_pc = captured;
        dp.remove_sq = capsq;

        k ^= Zobrist::psq[captured][capsq];
        st->materialKey ^=
          Zobrist::psq[captured][8 + pieceCount[captured] - (m.type_of() != EN_PASSANT)];

        // Reset rule 50 counter
        st->rule50 = 0;
    }
    else
        dp.remove_sq = SQ_NONE;

    // Update hash key
    k ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];

    // Reset en passant square
    if (st->epSquare != SQ_NONE)
    {
        k ^= Zobrist::enpassant[file_of(st->epSquare)];
        st->epSquare = SQ_NONE;
    }

    // Update castling rights.
    k ^= Zobrist::castling[st->castlingRights];
    st->castlingRights &= ~(castlingRightsMask[from] | castlingRightsMask[to]);
    k ^= Zobrist::castling[st->castlingRights];

    // If the moving piece is a pawn do some special extra work
    if (type_of(pc) == PAWN)
    {
        // Check if the en passant square needs to be set. Accurate e.p. info is needed
        // for correct zobrist key generation and 3-fold checking.
        if ((int(to) ^ int(from)) == 16)
        {
            Square   epSquare = to - pawn_push(us);
            Bitboard pawns    = attacks_bb<PAWN>(epSquare, us) & pieces(them, PAWN);

            // If there are no pawns attacking the ep square, ep is not possible.
            if (pawns)
            {
                Square   ksq         = square<KING>(them);
                Bitboard notBlockers = ~st->previous->blockersForKing[them];
                bool     noDiscovery = (from & notBlockers) || file_of(from) == file_of(ksq);

                // If the pawn gives discovered check, ep is never legal. Else, if at least one
                // pawn was not a blocker for the enemy king or lies on the same line as the
                // enemy king and en passant square, a legal capture exists.
                if (noDiscovery && (pawns & (notBlockers | line_bb(epSquare, ksq))))
                {
                    st->epSquare = epSquare;
                    k ^= Zobrist::enpassant[file_of(epSquare)];
                }
            }
        }

        else if (m.type_of() == PROMOTION)
        {
            PieceType pt        = m.promotion_type();
            Piece     promotion = make_piece(us, pt);

            assert(relative_rank(us, to) == RANK_8);
            assert(pt >= KNIGHT && pt <= QUEEN);

            dp.add_pc = promotion;
            dp.add_sq = to;
            dp.to     = SQ_NONE;

            // Update hash keys
            // Zobrist::psq[pc][to] is zero, so we don't need to clear it
            k ^= Zobrist::psq[promotion][to];
            st->materialKey ^= Zobrist::psq[promotion][8 + pieceCount[promotion]]
                             ^ Zobrist::psq[pc][8 + pieceCount[pc] - 1];
            st->nonPawnKey[us] ^= Zobrist::psq[promotion][to];

            if (pt <= BISHOP)
                st->minorPieceKey ^= Zobrist::psq[promotion][to];

            // Update material
            st->nonPawnMaterial[us] += PieceValue[promotion];
        }

        // Update pawn hash key
        st->pawnKey ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];

        // Reset rule 50 draw counter
        st->rule50 = 0;
    }

    else
    {
        st->nonPawnKey[us] ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];

        if (type_of(pc) <= BISHOP)
            st->minorPieceKey ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
    }

    if (tt)
        prefetch(tt->first_entry(adjust_key50(k)));
    // Update the key with the final value
    st->key = k;

    if (history)
    {
        prefetch(&history->pawn_entry(*this)[pc][to]);
        prefetch(&history->pawn_correction_entry(*this));
        prefetch(&history->minor_piece_correction_entry(*this));
        prefetch(&history->nonpawn_correction_entry<WHITE>(*this));
        prefetch(&history->nonpawn_correction_entry<BLACK>(*this));
    }

    // Move the piece. The tricky Chess960 castling is handled earlier
    if (m.type_of() != CASTLING)
    {
        Piece toPc = pc;
        if (m.type_of() == PROMOTION)
            toPc = make_piece(us, m.promotion_type());

        if (captured && m.type_of() != EN_PASSANT)
        {
            remove_piece(from, &dts);
            swap_piece(to, toPc, &dts);
        }
        else if (pc == toPc)
            move_piece(from, to, &dts);
        else
        {
            remove_piece(from, &dts);
            put_piece(toPc, to, &dts);
        }
    }

    // Set capture piece
    st->capturedPiece = captured;

    // Calculate checkers bitboard (if move gives check)
    st->checkersBB = givesCheck ? attackers_to(square<KING>(them)) & pieces(us) : 0;

    sideToMove = ~sideToMove;

    // Update king attacks used for fast check detection
    set_check_info();

    // Calculate the repetition info. It is the ply distance from the previous
    // occurrence of the same position, negative in the 3-fold case, or zero
    // if the position was not repeated.
    st->repetition = 0;
    int end        = std::min(st->rule50, st->pliesFromNull);
    if (end >= 4)
    {
        StateInfo* stp = st->previous->previous;
        for (int i = 4; i <= end; i += 2)
        {
            stp = stp->previous->previous;
            if (stp->key == st->key)
            {
                st->repetition = stp->repetition ? -i : i;
                break;
            }
        }
    }

    assert(pos_is_ok());

    assert(dp.pc != NO_PIECE);
    assert(!(bool(captured) || m.type_of() == CASTLING) ^ (dp.remove_sq != SQ_NONE));
    assert(dp.from != SQ_NONE);
    assert(!(dp.add_sq != SQ_NONE) ^ (m.type_of() == PROMOTION || m.type_of() == CASTLING));
}


// Unmakes a move. When it returns, the position should
// be restored to exactly the same state as before the move was made.
void Position::undo_move(Move m) {

    assert(m.is_ok());

    sideToMove = ~sideToMove;

    Color  us   = sideToMove;
    Square from = m.from_sq();
    Square to   = m.to_sq();
    Piece  pc   = piece_on(to);

    assert(empty(from) || m.type_of() == CASTLING);
    assert(type_of(st->capturedPiece) != KING);

    if (m.type_of() == PROMOTION)
    {
        assert(relative_rank(us, to) == RANK_8);
        assert(type_of(pc) == m.promotion_type());
        assert(type_of(pc) >= KNIGHT && type_of(pc) <= QUEEN);

        pc = make_piece(us, PAWN);
        swap_piece(to, pc);
    }

    if (m.type_of() == CASTLING)
    {
        Square rfrom, rto;
        do_castling<false>(us, from, to, rfrom, rto);
    }
    else
    {
        move_piece(to, from);  // Put the piece back at the source square

        if (st->capturedPiece)
        {
            Square capsq = to;

            if (m.type_of() == EN_PASSANT)
            {
                capsq -= pawn_push(us);

                assert(type_of(pc) == PAWN);
                assert(to == st->previous->epSquare);
                assert(relative_rank(us, to) == RANK_6);
                assert(piece_on(capsq) == NO_PIECE);
                assert(st->capturedPiece == make_piece(~us, PAWN));
            }

            put_piece(st->capturedPiece, capsq);  // Restore the captured piece
        }
    }

    // Finally point our state pointer back to the previous state
    st = st->previous;
    --gamePly;

    assert(pos_is_ok());
}

inline void add_dirty_threat(DirtyThreats* const dts,
                             bool                putPiece,
                             Piece               pc,
                             Piece               threatened,
                             Square              s,
                             Square              threatenedSq) {
    dts->list.push_back({pc, threatened, s, threatenedSq, putPiece});
}

#ifdef USE_AVX512ICL
// Given a DirtyThreat template and bit offsets to insert the piece type and square, write the threats
// present at the given bitboard.
template<int SqShift, int PcShift>
void write_multiple_dirties(const Position& p,
                            Bitboard        mask,
                            DirtyThreat     dt_template,
                            DirtyThreats*   dts) {
    static_assert(sizeof(DirtyThreat) == 4);

    const __m512i board    = _mm512_loadu_si512(p.piece_array().data());
    const int     dt_count = popcount(mask);
    assert(dt_count <= 16);

    const __m512i template_v = _mm512_set1_epi32(dt_template.raw());
    auto*         write      = dts->list.make_space(dt_count);

    // Extract the list of squares and upconvert to 32 bits. There are never more than 16
    // incoming threats so this is sufficient.
    __m512i threat_squares = _mm512_maskz_compress_epi8(mask, AllSquares);
    threat_squares         = _mm512_cvtepi8_epi32(_mm512_castsi512_si128(threat_squares));

    __m512i threat_pieces =
      _mm512_maskz_permutexvar_epi8(0x1111111111111111ULL, threat_squares, board);

    // Shift the piece and square into place
    threat_squares = _mm512_slli_epi32(threat_squares, SqShift);
    threat_pieces  = _mm512_slli_epi32(threat_pieces, PcShift);

    const __m512i dirties =
      _mm512_ternarylogic_epi32(template_v, threat_squares, threat_pieces, 254 /* A | B | C */);
    _mm512_storeu_si512(write, dirties);
}
#endif

constexpr bool can_slider_threat(Piece pc, Piece slider) {
    return type_of(pc) != QUEEN || type_of(slider) == QUEEN;
}

template<bool ComputeRay>
void Position::update_piece_threats(Piece               pc,
                                    bool                putPiece,
                                    Square              s,
                                    DirtyThreats* const dts,
                                    // Silence spurious warning on GCC 10
                                    [[maybe_unused]] Bitboard noRaysContaining) const {
    const Bitboard occupied     = pieces();
    const Bitboard rookQueens   = pieces(ROOK, QUEEN);
    const Bitboard bishopQueens = pieces(BISHOP, QUEEN);
    const Bitboard rAttacks     = attacks_bb<ROOK>(s, occupied);
    const Bitboard bAttacks     = attacks_bb<BISHOP>(s, occupied);
    const Bitboard occupiedNoK  = occupied ^ pieces(KING);

    Bitboard sliders       = (rookQueens & rAttacks) | (bishopQueens & bAttacks);
    Bitboard directSliders = type_of(pc) == QUEEN ? sliders & pieces(QUEEN) : sliders;

    auto process_sliders = [&](bool addDirectAttacks) {
        while (sliders)
        {
            Square sliderSq = pop_lsb(sliders);
            Piece  slider   = piece_on(sliderSq);

            const Bitboard ray        = ray_pass_bb(sliderSq, s);
            const Bitboard discovered = ray & (rAttacks | bAttacks) & occupiedNoK;

            assert(!more_than_one(discovered));
            if (discovered && (ray & noRaysContaining) != noRaysContaining)
            {
                const Square threatenedSq = lsb(discovered);
                const Piece  threatenedPc = piece_on(threatenedSq);
                if (can_slider_threat(threatenedPc, slider))
                    add_dirty_threat(dts, !putPiece, slider, threatenedPc, sliderSq, threatenedSq);
            }

            if (addDirectAttacks && can_slider_threat(pc, slider))
                add_dirty_threat(dts, putPiece, slider, pc, sliderSq, s);
        }
    };

    if (type_of(pc) == KING)
    {
        if constexpr (ComputeRay)
            process_sliders(false);
        return;
    }


    const Bitboard knights    = pieces(KNIGHT);
    const Bitboard whitePawns = pieces(WHITE, PAWN);
    const Bitboard blackPawns = pieces(BLACK, PAWN);


    Bitboard threatened       = attacks_bb(pc, s, occupied) & occupiedNoK;
    Bitboard incoming_threats = PseudoAttacks[KNIGHT][s] & knights;

    // Compute both incoming and outgoing pawn threats. Incoming pawn pushers are only
    // added if 'pc' is a pawn.
    Bitboard pawnThreats = 0;
    if (type_of(pc) == PAWN)
    {
        Bitboard whiteAttacks = PawnPushOrAttacks[WHITE][s];
        Bitboard blackAttacks = PawnPushOrAttacks[BLACK][s];

        threatened |= (color_of(pc) == WHITE ? whiteAttacks : blackAttacks) & pieces(PAWN);

        pawnThreats = whiteAttacks & blackPawns;
        pawnThreats |= blackAttacks & whitePawns;
    }
    else
    {
        pawnThreats =
          (attacks_bb<PAWN>(s, WHITE) & blackPawns) | (attacks_bb<PAWN>(s, BLACK) & whitePawns);
    }

    if (type_of(pc) == PAWN || type_of(pc) == KNIGHT || type_of(pc) == ROOK)
        incoming_threats |= pawnThreats;

    switch (type_of(pc))
    {
    case PAWN :
        threatened &= pieces(PAWN, KNIGHT, ROOK);
        break;
    case BISHOP :
    case ROOK :
        threatened &= pieces(PAWN, KNIGHT, BISHOP, ROOK);
        break;
    default :
        threatened &= occupiedNoK;
        break;
    }

#ifdef USE_AVX512ICL
    DirtyThreat dt_template{pc, NO_PIECE, s, Square(0), putPiece};
    write_multiple_dirties<DirtyThreat::ThreatenedSqOffset, DirtyThreat::ThreatenedPcOffset>(
      *this, threatened, dt_template, dts);

    Bitboard all_attackers = directSliders | incoming_threats;

    dt_template = {NO_PIECE, pc, Square(0), s, putPiece};
    write_multiple_dirties<DirtyThreat::PcSqOffset, DirtyThreat::PcOffset>(*this, all_attackers,
                                                                           dt_template, dts);
#else
    while (threatened)
    {
        Square threatenedSq = pop_lsb(threatened);
        Piece  threatenedPc = piece_on(threatenedSq);

        assert(threatenedSq != s);
        assert(threatenedPc);

        add_dirty_threat(dts, putPiece, pc, threatenedPc, s, threatenedSq);
    }
#endif

    if constexpr (ComputeRay)
    {
#ifndef USE_AVX512ICL
        process_sliders(true);
#else  // for ICL, direct threats were processed earlier (all_attackers)
        process_sliders(false);
#endif
    }
    else
    {
        incoming_threats |= directSliders;
    }

#ifndef USE_AVX512ICL
    while (incoming_threats)
    {
        Square srcSq = pop_lsb(incoming_threats);
        Piece  srcPc = piece_on(srcSq);

        assert(srcSq != s);
        assert(srcPc != NO_PIECE);

        add_dirty_threat(dts, putPiece, srcPc, pc, srcSq, s);
    }
#endif
}

Key Position::prefetch_key(Move m) const {
    Square from     = m.from_sq();
    Square to       = m.to_sq();
    Piece  pc       = piece_on(from);
    Piece  captured = piece_on(to);
    Key    k        = st->key ^ Zobrist::side;

    k ^= Zobrist::psq[captured][to] ^ Zobrist::psq[pc][to] ^ Zobrist::psq[pc][from];

    if (captured || type_of(pc) == PAWN)
        return k;

    return adjust_key50<true>(k);
}

// Helper used to do/undo a castling move. This is a bit
// tricky in Chess960 where from/to squares can overlap.
template<bool Do>
void Position::do_castling(Color               us,
                           Square              from,
                           Square&             to,
                           Square&             rfrom,
                           Square&             rto,
                           DirtyThreats* const dts,
                           DirtyPiece* const   dp) {

    bool kingSide = to > from;
    rfrom         = to;  // Castling is encoded as "king captures friendly rook"
    rto           = relative_square(us, kingSide ? SQ_F1 : SQ_D1);
    to            = relative_square(us, kingSide ? SQ_G1 : SQ_C1);

    assert(!Do || dp);

    if (Do)
    {
        dp->to        = to;
        dp->remove_pc = dp->add_pc = make_piece(us, ROOK);
        dp->remove_sq              = rfrom;
        dp->add_sq                 = rto;
    }

    // Remove both pieces first since squares could overlap in Chess960
    remove_piece(Do ? from : to, dts);
    remove_piece(Do ? rfrom : rto, dts);
    put_piece(make_piece(us, KING), Do ? to : from, dts);
    put_piece(make_piece(us, ROOK), Do ? rto : rfrom, dts);
}


// Used to do a "null move": it flips
// the side to move without executing any move on the board.
void Position::do_null_move(StateInfo& newSt) {

    assert(!checkers());
    assert(&newSt != st);

    std::memcpy(&newSt, st, sizeof(StateInfo));

    newSt.previous = st;
    st             = &newSt;

    if (st->epSquare != SQ_NONE)
    {
        st->key ^= Zobrist::enpassant[file_of(st->epSquare)];
        st->epSquare = SQ_NONE;
    }

    st->key ^= Zobrist::side;

    st->pliesFromNull = 0;

    st->capturedPiece = NO_PIECE;

    sideToMove = ~sideToMove;

    set_check_info();

    st->repetition = 0;

    assert(pos_is_ok());
}


// Must be used to undo a "null move"
void Position::undo_null_move() {

    assert(!checkers());

    st         = st->previous;
    sideToMove = ~sideToMove;
}


// Tests if the SEE (Static Exchange Evaluation)
// value of the move is greater or equal to the given threshold. We'll use an
// algorithm similar to alpha-beta pruning with a null window.
bool Position::see_ge(Move m, int threshold) const {

    assert(m.is_ok());

    // Only deal with normal moves, assume others pass a simple SEE
    if (m.type_of() != NORMAL)
        return VALUE_ZERO >= threshold;

    Square from = m.from_sq(), to = m.to_sq();

    assert(piece_on(from) != NO_PIECE);

    int swap = PieceValue[piece_on(to)] - threshold;
    if (swap < 0)
        return false;

    swap = PieceValue[piece_on(from)] - swap;
    if (swap <= 0)
        return true;

    assert(color_of(piece_on(from)) == sideToMove);
    Bitboard occupied  = pieces() ^ from ^ to;  // xoring to is important for pinned piece logic
    Color    stm       = sideToMove;
    Bitboard attackers = attackers_to(to, occupied);
    Bitboard stmAttackers, bb;
    int      res = 1;

    while (true)
    {
        stm = ~stm;
        attackers &= occupied;

        // If stm has no more attackers then give up: stm loses
        if (!(stmAttackers = attackers & pieces(stm)))
            break;

        // Don't allow pinned pieces to attack as long as there are
        // pinners on their original square.
        if (pinners(~stm) & occupied)
        {
            stmAttackers &= ~blockers_for_king(stm);

            if (!stmAttackers)
                break;
        }

        res ^= 1;

        // Locate and remove the next least valuable attacker, and add to
        // the bitboard 'attackers' any X-ray attackers behind it.
        if ((bb = stmAttackers & pieces(PAWN)))
        {
            if ((swap = PawnValue - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);

            attackers |= attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN);
        }

        else if ((bb = stmAttackers & pieces(KNIGHT)))
        {
            if ((swap = KnightValue - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);
        }

        else if ((bb = stmAttackers & pieces(BISHOP)))
        {
            if ((swap = BishopValue - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);

            attackers |= attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN);
        }

        else if ((bb = stmAttackers & pieces(ROOK)))
        {
            if ((swap = RookValue - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);

            attackers |= attacks_bb<ROOK>(to, occupied) & pieces(ROOK, QUEEN);
        }

        else if ((bb = stmAttackers & pieces(QUEEN)))
        {
            swap = QueenValue - swap;
            //  implies that the previous recapture was done by a higher rated piece than a Queen (King is excluded)
            assert(swap >= res);
            occupied ^= least_significant_square_bb(bb);

            attackers |= (attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN))
                       | (attacks_bb<ROOK>(to, occupied) & pieces(ROOK, QUEEN));
        }

        else  // KING
              // If we "capture" with the king but the opponent still has attackers,
              // reverse the result.
            return (attackers & ~pieces(stm)) ? res ^ 1 : res;
    }

    return bool(res);
}

// Tests whether the position is drawn by 50-move rule
// or by repetition. It does not detect stalemates.
bool Position::is_draw(int ply) const {

    if (st->rule50 > 99 && (!checkers() || MoveList<LEGAL>(*this).size()))
        return true;

    return is_repetition(ply);
}

// Return a draw score if a position repeats once earlier but strictly
// after the root, or repeats twice before or at the root.
bool Position::is_repetition(int ply) const { return st->repetition && st->repetition < ply; }

// Tests whether there has been at least one repetition
// of positions since the last capture or pawn move.
bool Position::has_repeated() const {

    StateInfo* stc = st;
    int        end = std::min(st->rule50, st->pliesFromNull);
    while (end-- >= 4)
    {
        if (stc->repetition)
            return true;

        stc = stc->previous;
    }
    return false;
}


// Tests if the position has a move which draws by repetition.
// This function accurately matches the outcome of is_draw() over all legal moves.
bool Position::upcoming_repetition(int ply) const {

    int j;

    int end = std::min(st->rule50, st->pliesFromNull);

    if (end < 3)
        return false;

    Key        originalKey = st->key;
    StateInfo* stp         = st->previous;
    Key        other       = originalKey ^ stp->key ^ Zobrist::side;

    for (int i = 3; i <= end; i += 2)
    {
        stp = stp->previous;
        other ^= stp->key ^ stp->previous->key ^ Zobrist::side;
        stp = stp->previous;

        if (other != 0)
            continue;

        Key moveKey = originalKey ^ stp->key;
        if ((j = H1(moveKey), cuckoo[j] == moveKey) || (j = H2(moveKey), cuckoo[j] == moveKey))
        {
            Move   move = cuckooMove[j];
            Square s1   = move.from_sq();
            Square s2   = move.to_sq();

            if (!((between_bb(s1, s2) ^ s2) & pieces()))
            {
                if (ply > i)
                    return true;

                // For nodes before or at the root, check that the move is a
                // repetition rather than a move to the current position.
                if (stp->repetition)
                    return true;
            }
        }
    }
    return false;
}


// Flips position with the white and black sides reversed. This
// is only useful for debugging e.g. for finding evaluation symmetry bugs.
std::optional<PositionSetError> Position::flip() {

    string            f, token;
    std::stringstream ss(fen());

    for (Rank r = RANK_8;; --r)  // Piece placement
    {
        std::getline(ss, token, r > RANK_1 ? '/' : ' ');
        f.insert(0, token + (f.empty() ? " " : "/"));

        if (r == RANK_1)
            break;
    }

    ss >> token;                        // Active color
    f += (token == "w" ? "B " : "W ");  // Will be lowercased later

    ss >> token;  // Castling availability
    f += token + " ";

    std::transform(f.begin(), f.end(), f.begin(),
                   [](char c) { return char(islower(c) ? toupper(c) : tolower(c)); });

    ss >> token;  // En passant square
    f += (token == "-" ? token : token.replace(1, 1, token[1] == '3' ? "6" : "3"));

    std::getline(ss, token);  // Half and full moves
    f += token;

    return set(f, is_chess960(), st);
}


bool Position::material_key_is_ok() const { return compute_material_key() == st->materialKey; }


// Performs some consistency checks for the position object
// and raise an assert if something wrong is detected.
// This is meant to be helpful when debugging.
bool Position::pos_is_ok() const {

    if ((sideToMove != WHITE && sideToMove != BLACK) || piece_on(square<KING>(WHITE)) != W_KING
        || piece_on(square<KING>(BLACK)) != B_KING
        || (ep_square() != SQ_NONE && relative_rank(sideToMove, ep_square()) != RANK_6))
        assert(0 && "pos_is_ok: Default");

    if (count<KING>(WHITE) != 1 || count<KING>(BLACK) != 1
        || attackers_to_exist(square<KING>(~sideToMove), pieces(), sideToMove))
        assert(0 && "pos_is_ok: Kings");

    if ((pieces(PAWN) & (Rank1BB | Rank8BB)) || count<PAWN>(WHITE) > 8 || count<PAWN>(BLACK) > 8)
        assert(0 && "pos_is_ok: Pawns");


    if (ep_square() != SQ_NONE)
    {
        Square ksq = square<KING>(sideToMove);

        Bitboard captured = (ep_square() + pawn_push(~sideToMove)) & pieces(~sideToMove, PAWN);
        Bitboard pawns    = attacks_bb<PAWN>(ep_square(), ~sideToMove) & pieces(sideToMove, PAWN);
        Bitboard potentialCheckers = pieces(~sideToMove) ^ captured;

        if (!captured || !pawns
            || ((attackers_to(ksq, pieces() ^ captured ^ ep_square() ^ lsb(pawns))
                 & potentialCheckers)
                && (attackers_to(ksq, pieces() ^ captured ^ ep_square() ^ msb(pawns))
                    & potentialCheckers)))
            assert(0 && "pos_is_ok: En passant square");
    }

    if ((pieces(WHITE) & pieces(BLACK)) || (pieces(WHITE) | pieces(BLACK)) != pieces()
        || popcount(pieces(WHITE)) > 16 || popcount(pieces(BLACK)) > 16)
        assert(0 && "pos_is_ok: Bitboards");

    for (PieceType p1 = PAWN; p1 <= KING; ++p1)
        for (PieceType p2 = PAWN; p2 <= KING; ++p2)
            if (p1 != p2 && (pieces(p1) & pieces(p2)))
                assert(0 && "pos_is_ok: Bitboards");

    for (Piece pc : Pieces)
        if (pieceCount[pc] != popcount(pieces(color_of(pc), type_of(pc)))
            || pieceCount[pc] != std::count(board.begin(), board.end(), pc))
            assert(0 && "pos_is_ok: Pieces");

    for (Color c : {WHITE, BLACK})
        for (CastlingRights cr : {c & KING_SIDE, c & QUEEN_SIDE})
        {
            if (!can_castle(cr))
                continue;

            if (piece_on(castling_rook_square(cr)) != make_piece(c, ROOK)
                || castlingRightsMask[castlingRookSquare[cr]] != cr
                || (castlingRightsMask[square<KING>(c)] & cr) != cr)
                assert(0 && "pos_is_ok: Castling");
        }

    assert(material_key_is_ok() && "pos_is_ok: materialKey");

    return true;
}

}  // namespace VeloCT

// ==== END OF FILE: position.cpp ====

// ==== START OF FILE: score.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "score.h"

#include <cassert>
#include <cmath>
#include <cstdlib>

#include "uci.h"

namespace VeloCT {

Score::Score(Value v, const Position& pos) {
    assert(-VALUE_INFINITE < v && v < VALUE_INFINITE);

    if (!is_decisive(v))
    {
        score = InternalUnits{UCIEngine::to_cp(v, pos)};
    }
    else if (std::abs(v) <= VALUE_TB)
    {
        auto distance = VALUE_TB - std::abs(v);
        score         = (v > 0) ? Tablebase{distance, true} : Tablebase{-distance, false};
    }
    else
    {
        auto distance = VALUE_MATE - std::abs(v);
        score         = (v > 0) ? Mate{distance} : Mate{-distance};
    }
}

}
// ==== END OF FILE: score.cpp ====

// ==== START OF FILE: search.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "search.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <list>
#include <ratio>
#include <string>
#include <utility>

#include "bitboard.h"
#include "evaluate.h"
#include "history.h"
#include "misc.h"
#include "movegen.h"
#include "movepick.h"
#include "nnue/network.h"
#include "nnue/nnue_accumulator.h"
#include "position.h"
#include "syzygy/tbprobe.h"
#include "thread.h"
#include "timeman.h"
#include "tt.h"
#include "types.h"
#include "uci.h"
#include "ucioption.h"

namespace VeloCT {

static constexpr std::array<int, 16> lmrDivisor = {3307, 2930, 2874, 2818, 3215, 3225, 3224, 2782,
                                                   2858, 2919, 3088, 3275, 3180, 2868, 3006, 3599};

namespace TB = Tablebases;

void syzygy_extend_pv(const OptionsMap&            options,
                      const Search::LimitsType&    limits,
                      VeloCT::Position&         pos,
                      VeloCT::Search::RootMove& rootMove,
                      Value&                       v);

using namespace Search;

namespace {

constexpr u64 NODES_LIMIT_OUTPUT = 10'000'000;

constexpr int SEARCHEDLIST_CAPACITY = 32;
using SearchedList                  = ValueList<Move, SEARCHEDLIST_CAPACITY>;

// (*Scalers):
// The values with Scaler asterisks have proven non-linear scaling.
// They are optimized to time controls of 180 + 1.8 and longer,
// so changing them or adding conditions that are similar requires
// tests at these types of time controls.

// (*Scaler) All tuned parameters at time controls shorter than
// optimized for require verifications at longer time controls

int correction_value(const Worker& w, const Position& pos, const Stack* const ss) {
    const Color us     = pos.side_to_move();
    const auto  m      = (ss - 1)->currentMove;
    const auto& shared = w.sharedHistory;
    const int   pcv    = shared.pawn_correction_entry(pos)[us].pawn;
    const int   micv   = shared.minor_piece_correction_entry(pos)[us].minor;
    const int   wnpcv  = shared.nonpawn_correction_entry<WHITE>(pos)[us].nonPawnWhite;
    const int   bnpcv  = shared.nonpawn_correction_entry<BLACK>(pos)[us].nonPawnBlack;
    const int   cntcv =
      m.is_ok()
          ? 8363
            * ((*(ss - 2)->continuationCorrectionHistory)[pos.piece_on(m.to_sq())][m.to_sq()]
               + (*(ss - 4)->continuationCorrectionHistory)[pos.piece_on(m.to_sq())][m.to_sq()])
          : 64549;

    return 13345 * pcv + 9280 * micv + 11840 * (wnpcv + bnpcv) + cntcv;
}

// Add correctionHistory value to raw staticEval and guarantee evaluation
// does not hit the tablebase range.
Value to_corrected_static_eval(const Value v, const int cv) {
    return std::clamp(v + cv / 131072, VALUE_TB_LOSS_IN_MAX_PLY + 1, VALUE_TB_WIN_IN_MAX_PLY - 1);
}

void update_correction_history(const Position& pos,
                               Stack* const    ss,
                               Search::Worker& workerThread,
                               const int       bonus) {
    const Move  m  = (ss - 1)->currentMove;
    const Color us = pos.side_to_move();

    constexpr int nonPawnWeight = 186;
    auto&         shared        = workerThread.sharedHistory;

    shared.pawn_correction_entry(pos)[us].pawn << bonus;
    shared.minor_piece_correction_entry(pos)[us].minor << bonus * 152 / 128;
    shared.nonpawn_correction_entry<WHITE>(pos)[us].nonPawnWhite << bonus * nonPawnWeight / 128;
    shared.nonpawn_correction_entry<BLACK>(pos)[us].nonPawnBlack << bonus * nonPawnWeight / 128;

    if (m.is_ok())
    {
        const Square to = m.to_sq();
        const Piece  pc = pos.piece_on(to);
        (*(ss - 2)->continuationCorrectionHistory)[pc][to] << bonus * 136 / 128;
        (*(ss - 4)->continuationCorrectionHistory)[pc][to] << bonus * 68 / 128;
    }
}

// Add a small random component to draw evaluations to avoid 3-fold blindness
Value value_draw(usize nodes) { return VALUE_DRAW - 1 + Value(nodes & 0x2); }
Value value_to_tt(Value v, int ply);
Value value_from_tt(Value v, int ply, int r50c);
void  update_continuation_histories(Stack* ss, Piece pc, Square to, int bonus);
void  update_quiet_histories(
   const Position& pos, Stack* ss, Search::Worker& workerThread, Move move, int bonus);
void update_all_stats(const Position& pos,
                      Stack*          ss,
                      Search::Worker& workerThread,
                      Move            bestMove,
                      Square          prevSq,
                      SearchedList&   quietsSearched,
                      SearchedList&   capturesSearched,
                      Depth           depth,
                      Move            ttMove,
                      bool            PvNode);

// Detect shuffling moves in order to limit search explosions
// Added in #6447 as non-regression, and so its parameters should not be tuned
bool is_shuffling(Move move, Stack* const ss, const Position& pos) {
    if (pos.capture_stage(move) || pos.rule50_count() < 10)
        return false;
    if (pos.state()->pliesFromNull < 6 || ss->ply < 20)
        return false;
    return move.from_sq() == (ss - 2)->currentMove.to_sq()
        && (ss - 2)->currentMove.from_sq() == (ss - 4)->currentMove.to_sq();
}

}  // namespace

Search::Worker::Worker(SharedState&                    sharedState,
                       std::unique_ptr<ISearchManager> sm,
                       usize                           threadId,
                       usize                           numaThreadId,
                       usize                           numaTotalThreads,
                       NumaReplicatedAccessToken       token) :
    // Unpack the SharedState struct into member variables
    sharedHistory(sharedState.sharedHistories.at(token.get_numa_index())),
    continuationHistory(sharedHistory.continuationHistory),
    threadIdx(threadId),
    numaThreadIdx(numaThreadId),
    numaTotal(numaTotalThreads),
    numaAccessToken(token),
    manager(std::move(sm)),
    options(sharedState.options),
    threads(sharedState.threads),
    tt(sharedState.tt),
    network(sharedState.network),
    refreshTable(network[token]) {
    clear();
}

void Search::Worker::ensure_network_replicated() {
    // Access once to force lazy initialization.
    // We do this because we want to avoid initialization during search.
    (void) (network[numaAccessToken]);
}

void Search::Worker::start_searching() {

    accumulatorStack.reset();

    // Non-main threads go directly to iterative_deepening()
    if (!is_mainthread())
    {
        iterative_deepening();
        return;
    }

    main_manager()->tm.init(limits, rootPos.side_to_move(), rootPos.game_ply(), options,
                            main_manager()->originalTimeAdjust);
    tt.new_search();

    if (rootMoves.empty())
    {
        main_manager()->updates.onUpdateNoMoves(
          {0, {rootPos.checkers() ? -VALUE_MATE : VALUE_DRAW, rootPos}});
        main_manager()->updates.onBestmove(UCIEngine::move(Move::none()), "");
        return;
    }

    // Main thread starts non-main threads, and begins own search.
    threads.start_searching();
    bool uciPvSent = iterative_deepening();

    // When we reach the maximum depth, we can arrive here without a raise of
    // threads.stop. However, if we are pondering or in an infinite search,
    // the UCI protocol states that we shouldn't print the best move before the
    // GUI sends a "stop" or "ponderhit" command. We therefore simply wait here
    // until the GUI sends one of those commands.
    while (!threads.stop && (main_manager()->ponder || limits.infinite))
    {}  // Busy wait for a stop or a ponder reset

    // Stop the threads if not already stopped (also raise the stop if
    // "ponderhit" just reset threads.ponder)
    threads.stop = true;

    // Wait until all threads have finished
    threads.wait_for_search_finished();

    // When playing in 'nodes as time' mode, subtract the searched nodes from
    // the available ones before exiting.
    if (limits.npmsec)
        main_manager()->tm.advance_nodes_time(threads.nodes_searched()
                                              - limits.inc[rootPos.side_to_move()]);

    Worker* bestThread = this;
    Skill   skill =
      Skill(options["Skill Level"], options["UCI_LimitStrength"] ? int(options["UCI_Elo"]) : 0);

    if (!limits.depth && !skill.enabled())
        bestThread = threads.get_best_thread()->worker.get();

    main_manager()->bestPreviousScore        = bestThread->rootMoves[0].score;
    main_manager()->bestPreviousAverageScore = bestThread->rootMoves[0].averageScore;

    if (bestThread->rootMoves[0].pv.size() == 1
        && bestThread->rootMoves[0].extract_ponder_from_tt(tt, rootPos))
        uciPvSent = false;

    // Send PV info if it has changed since last output in iterative_deepening().
    if (!uciPvSent || bestThread != this)
        main_manager()->output_pv(*bestThread, threads, tt, bestThread->rootDepth);

    // In rare cases, output_pv() may change the ponder move through syzygy_extend_pv().
    std::string ponder;
    if (bestThread->rootMoves[0].pv.size() > 1)
        ponder = UCIEngine::move(bestThread->rootMoves[0].pv[1], rootPos.is_chess960());

    auto bestmove = UCIEngine::move(bestThread->rootMoves[0].pv[0], rootPos.is_chess960());
    main_manager()->updates.onBestmove(bestmove, ponder);
}

// Main iterative deepening loop. It calls search()
// repeatedly with increasing depth until the allocated thinking time has been
// consumed, the user stops the search, or the maximum search depth is reached.
bool Search::Worker::iterative_deepening() {

    SearchManager* mainThread = (is_mainthread() ? main_manager() : nullptr);

    PVMoves pv;

    PVMoves lastBestMovePV;
    Depth   lastBestMoveDepth = 0;
    Value   lastBestMoveScore = -VALUE_INFINITE;

    Value  alpha, beta;
    Value  bestValue     = -VALUE_INFINITE;
    Color  us            = rootPos.side_to_move();
    double timeReduction = 1, totBestMoveChanges = 0;
    int    delta, iterIdx                        = 0;

    // Allocate stack with extra size to allow access from (ss - 7) to (ss + 2):
    // (ss - 7) is needed for update_continuation_histories(ss - 1) which accesses (ss - 6),
    // (ss + 2) is needed for initialization of cutOffCnt.
    Stack  stack[MAX_PLY + 10] = {};
    Stack* ss                  = stack + 7;

    for (int i = 7; i > 0; --i)
    {
        (ss - i)->continuationHistory =
          &continuationHistory[0][0][NO_PIECE][0];  // Use as a sentinel
        (ss - i)->continuationCorrectionHistory = &continuationCorrectionHistory[NO_PIECE][0];
        (ss - i)->staticEval                    = VALUE_NONE;
    }

    for (int i = 0; i <= MAX_PLY + 2; ++i)
        (ss + i)->ply = i;

    ss->pv = &pv;

    if (mainThread)
    {
        if (mainThread->bestPreviousScore == VALUE_INFINITE)
            mainThread->iterValue.fill(VALUE_ZERO);
        else
            mainThread->iterValue.fill(mainThread->bestPreviousScore);
    }

    usize multiPV = usize(options["MultiPV"]);
    Skill skill(options["Skill Level"], options["UCI_LimitStrength"] ? int(options["UCI_Elo"]) : 0);

    // When playing with strength handicap enable MultiPV search that we will
    // use behind-the-scenes to retrieve a set of possible moves.
    if (skill.enabled())
        multiPV = std::max(multiPV, usize(4));

    multiPV = std::min(multiPV, rootMoves.size());

    int  searchAgainCounter = 0;
    bool uciPvSent          = false;

    lowPlyHistory.fill(100);

    for (Color c : {WHITE, BLACK})
        for (int i = 0; i < UINT_16_HISTORY_SIZE; i++)
            mainHistory[c][i] = mainHistory[c][i] * 789 / 1024;

    // Iterative deepening loop until requested to stop or the target depth is reached
    while (rootDepth + 1 < MAX_PLY && !threads.stop
           && !(limits.depth && mainThread && rootDepth >= limits.depth))
    {
        rootDepth++;

        // Age out PV variability metric and signal the start of a new iteration.
        if (mainThread)
        {
            totBestMoveChanges /= 2;
            uciPvSent = false;
        }

        // Save the last iteration's scores before the first PV line is searched and
        // all the move scores except the (new) PV are set to -VALUE_INFINITE.
        for (usize i = 0; i < rootMoves.size(); ++i)
        {
            rootMoves[i].previousScore      = rootMoves[i].score;
            rootMoves[i].previousPV         = rootMoves[i].pv;
            rootMoves[i].previousScoreExact = i < multiPV;
        }

        usize pvFirst = pvLast = 0;

        if (!threads.increaseDepth)
            searchAgainCounter++;

        // MultiPV loop. We perform a full root search for each PV line
        for (pvIdx = 0; pvIdx < multiPV; ++pvIdx)
        {
            if (pvIdx == pvLast)
            {
                pvFirst = pvLast;
                for (pvLast++; pvLast < rootMoves.size(); pvLast++)
                    if (rootMoves[pvLast].tbRank != rootMoves[pvFirst].tbRank)
                        break;
            }

            lastIterationIdxPV = rootMoves[pvIdx].previousPV;

            // Reset UCI info selDepth for each depth and each PV line
            selDepth = 0;

            // Reset aspiration window starting size
            delta     = 5 + threadIdx % 8 + std::abs(rootMoves[pvIdx].meanSquaredScore) / 10588;
            Value avg = rootMoves[pvIdx].averageScore;
            alpha     = std::max(avg - delta, -VALUE_INFINITE);
            beta      = std::min(avg + delta, VALUE_INFINITE);

            // Adjust optimism based on root move's averageScore
            optimism[us]  = 137 * avg / (std::abs(avg) + 81);
            optimism[~us] = -optimism[us];

            // Start with a small aspiration window and, in the case of a fail
            // high/low, re-search with a bigger window until we don't fail
            // high/low anymore.
            int failedHighCnt = 0;
            while (true)
            {
                // Adjust the effective depth searched, but ensure at least one
                // effective increment for every four searchAgain steps (see issue #2717).
                Depth adjustedDepth =
                  std::max(1, rootDepth - failedHighCnt - 3 * (searchAgainCounter + 1) / 4);
                rootDelta = beta - alpha;
                bestValue = search<Root>(rootPos, ss, alpha, beta, adjustedDepth, false);

                // Bring the best move to the front. It is critical that sorting
                // is done with a stable algorithm because all the values but the
                // first and eventually the new best one is set to -VALUE_INFINITE
                // and we want to keep the same order for all the moves except the
                // new PV that goes to the front. Note that in the case of MultiPV
                // search the already searched PV lines are preserved.
                std::stable_sort(rootMoves.begin() + pvIdx, rootMoves.begin() + pvLast);

                // If search has been stopped, we break immediately. Sorting is
                // safe because RootMoves is still valid, although it refers to
                // the previous iteration.
                if (threads.stop)
                    break;

                // When failing high/low give some update before a re-search. To avoid
                // excessive output that could hang GUIs like Fritz 19, only start
                // at nodes > 10M (rather than depth N, which can be reached quickly)
                if (mainThread && multiPV == 1 && (bestValue <= alpha || bestValue >= beta)
                    && nodes > NODES_LIMIT_OUTPUT)
                    main_manager()->output_pv(*this, threads, tt, rootDepth);

                // In case of failing low/high increase aspiration window and re-search,
                // otherwise exit the loop.
                if (bestValue <= alpha)
                {
                    beta  = alpha;
                    alpha = std::max(bestValue - delta, -VALUE_INFINITE);

                    failedHighCnt = 0;
                    if (mainThread)
                        mainThread->stopOnPonderhit = false;
                }
                else if (bestValue >= beta)
                {
                    alpha = std::max(beta - delta, alpha);
                    beta  = std::min(bestValue + delta, VALUE_INFINITE);
                    ++failedHighCnt;
                }
                else
                    break;

                delta += 44 * delta / 128;

                assert(alpha >= -VALUE_INFINITE && beta <= VALUE_INFINITE);
            }

            if (threads.stop && pvIdx)
            {
                // In multiPV analysis we do not let aborted searches spoil mated-in/
                // TB loss scores from a completed search in an earlier PV line.
                // Hence we guard against an aborted pvIdx line overtaking pvIdx - 1
                // when pvIdx - 1 is a proven loss.
                // Moreover, we do not trust an exact loss score from an aborted search.
                if ((is_loss(rootMoves[pvIdx - 1].score) && rootMoves[pvIdx] < rootMoves[pvIdx - 1])
                    || rootMoves[pvIdx].score_is_exact_loss())
                {
                    // If previousScore is exact and worse than pvIdx - 1, we can safely use it.
                    // If it is equal, we make sure it cannot overtake pvIdx - 1.
                    if (rootMoves[pvIdx].previousScore != -VALUE_INFINITE
                        && rootMoves[pvIdx].previousScoreExact
                        && rootMoves[pvIdx].previousScore <= rootMoves[pvIdx - 1].score)
                    {
                        rootMoves[pvIdx].score = rootMoves[pvIdx].uciScore =
                          rootMoves[pvIdx].previousScore;
                        rootMoves[pvIdx].previousScore = -VALUE_INFINITE;
                        rootMoves[pvIdx].pv            = rootMoves[pvIdx].previousPV;
                        rootMoves[pvIdx].unset_bound_flags();
                    }

                    // Otherwise, if we can, we cap the score to the best possible, and mark
                    // the score as a bound (also a valid excuse for the incomplete PV.)
                    else
                    {
                        if (is_loss(rootMoves[pvIdx - 1].score))
                        {
                            rootMoves[pvIdx].score = rootMoves[pvIdx].uciScore =
                              rootMoves[pvIdx - 1].score;
                            rootMoves[pvIdx].previousScore = -VALUE_INFINITE;
                            rootMoves[pvIdx].pv.resize(1);
                            rootMoves[pvIdx].scoreUpperbound = true;
                        }
                        else
                            rootMoves[pvIdx].scoreUpperbound = false;

                        rootMoves[pvIdx].scoreLowerbound = !rootMoves[pvIdx].scoreUpperbound;
                    }
                }

                // Finally, we mark all loss scores from partially searched moves as a bound.
                for (usize i = pvIdx + 1; i < multiPV; ++i)
                    if (rootMoves[i].score_is_exact_loss())
                        rootMoves[i].scoreLowerbound = true;
            }

            // Sort the PV lines searched so far and update the GUI
            std::stable_sort(rootMoves.begin() + pvFirst, rootMoves.begin() + pvIdx + 1);

            if (mainThread && !threads.stop && (pvIdx + 1 == multiPV || nodes > NODES_LIMIT_OUTPUT))
            {
                main_manager()->output_pv(*this, threads, tt, rootDepth);
                uciPvSent = (pvIdx + 1 == multiPV);
            }

            if (threads.stop)
                break;
        }

        const bool forgottenMate = lastBestMoveScore != -VALUE_INFINITE
                                && is_mate_or_mated(lastBestMoveScore)
                                && (std::abs(rootMoves[0].score) < std::abs(lastBestMoveScore)
                                    || rootMoves[0].score_is_bound());

        if (!threads.stop)
        {
            if (lastBestMovePV.empty() || lastBestMovePV[0] != rootMoves[0].pv[0])
                lastBestMoveDepth = rootDepth;

            // Do not replace (shorter) mate scores from a previous iteration.
            if (!forgottenMate)
            {
                lastBestMovePV    = rootMoves[0].pv;
                lastBestMoveScore = rootMoves[0].score;
            }
        }

        const bool abortedLossSearch = threads.stop && !pvIdx && rootMoves[0].score_is_exact_loss();

        // An exact mated-in/TB-loss score from an aborted search cannot be trusted: the
        // loss could be delayed or refuted upon exploring the remaining root-moves.
        // Thus here we roll back to the score from the previous iteration.
        // We do the same if a search has failed to recover a mate score that was found
        // in a previous iteration.
        if (abortedLossSearch || (rootMoves[0].score != -VALUE_INFINITE && forgottenMate))
        {
            // Bring the last best move to the front for best thread selection.
            if (!lastBestMovePV.empty())
            {
                Utility::move_to_front(rootMoves, [&lastPV = std::as_const(lastBestMovePV)](
                                                    const auto& rm) { return rm == lastPV[0]; });
                rootMoves[0].score = rootMoves[0].uciScore = lastBestMoveScore;
                rootMoves[0].pv                            = lastBestMovePV;
                rootMoves[0].unset_bound_flags();

                if (mainThread)
                    uciPvSent = false;
            }
            // For an aborted d1 search we label the loss score as a lower bound.
            else if (abortedLossSearch)
                rootMoves[0].scoreLowerbound = true;
        }

        // Have we found a "mate in x" after a completed iteration?
        if (limits.mate && !threads.stop && is_mate_or_mated(rootMoves[0].score)
            && VALUE_MATE - std::abs(rootMoves[0].score) <= 2 * limits.mate)
            threads.stop = true;

        if (!mainThread)
            continue;

        // If the skill level is enabled and time is up, pick a sub-optimal best move
        if (skill.enabled() && skill.time_to_pick(rootDepth))
            skill.pick_best(rootMoves, multiPV);

        // Use part of the gained time from a previous stable move for the current move
        for (auto&& th : threads)
        {
            totBestMoveChanges += th->worker->bestMoveChanges;
            th->worker->bestMoveChanges = 0;
        }

        // Do we have time for the next iteration? Can we stop searching now?
        if (limits.use_time_management() && !threads.stop && !mainThread->stopOnPonderhit)
        {
            u64 nodesEffort = rootMoves[0].effort * 100000 / std::max(u64(1), u64(nodes));

            double fallingEval = (11.87 + 2.21 * (mainThread->bestPreviousAverageScore - bestValue)
                                  + 1.0 * (mainThread->iterValue[iterIdx] - bestValue))
                               / 100.0;
            fallingEval = std::clamp(fallingEval, 0.572, 1.708);

            // If the bestMove is stable over several iterations, reduce time accordingly
            timeReduction =
              std::clamp(interpolate(double(rootDepth - lastBestMoveDepth), 5.0, 18.0, 0.65, 1.55),
                         0.65, 1.55);

            double reduction = (1.48 + mainThread->previousTimeReduction) / (2.157 * timeReduction);

            double bestMoveInstability = 1.096 + 2.29 * totBestMoveChanges / threads.size();

            double highBestMoveEffort = std::clamp(
              interpolate(i64(nodesEffort), i64(79219), i64(101822), 0.924, 0.71), 0.71, 0.924);

            double totalTime = mainThread->tm.optimum() * fallingEval * reduction
                             * bestMoveInstability * highBestMoveEffort;

            if (rootMoves.size() == 1)
                // Cap used time to 0.5s for a better viewer experience
                totalTime = std::min(500.0, totalTime);

            auto elapsedTime = elapsed();

            // Stop the search if we have exceeded totalTime or maximum time,
            // or if we know that there are no better moves in the analysed line(s)
            if (elapsedTime > std::min(totalTime, double(mainThread->tm.maximum()))
                || rootMoves[multiPV - 1].score >= mate_in(3) || rootMoves[0].score == mated_in(2))
            {
                // If we are allowed to ponder do not stop the search now but
                // keep pondering until the GUI sends "ponderhit" or "stop".
                if (mainThread->ponder)
                    mainThread->stopOnPonderhit = true;
                else
                    threads.stop = true;
            }
            else
                threads.increaseDepth = mainThread->ponder || elapsedTime <= totalTime * 0.50;
        }

        mainThread->iterValue[iterIdx] = bestValue;
        iterIdx                        = (iterIdx + 1) & 3;
    }

    if (!mainThread)
        return false;

    mainThread->previousTimeReduction = timeReduction;

    // If the skill level is enabled, swap the best PV line with the sub-optimal one
    if (skill.enabled())
        std::swap(rootMoves[0],
                  *std::find(rootMoves.begin(), rootMoves.end(),
                             skill.best ? skill.best : skill.pick_best(rootMoves, multiPV)));

    return uciPvSent;
}


void Search::Worker::do_move(Position& pos, const Move move, StateInfo& st, Stack* const ss) {
    do_move(pos, move, st, pos.gives_check(move), ss);
}

void Search::Worker::do_move(
  Position& pos, const Move move, StateInfo& st, const bool givesCheck, Stack* const ss) {
    // prefetch_key does not model castling, en passant or promotion keys
    // exactly; for rare moves the prefetch lands on an unused line.
    prefetch(tt.first_entry(pos.prefetch_key(move)));

    bool capture = pos.capture_stage(move);
    ++nodes;

    auto [dirtyPiece, dirtyThreats] = accumulatorStack.push();
    pos.do_move(move, st, givesCheck, dirtyPiece, dirtyThreats, &tt, &sharedHistory);

    if (ss != nullptr)
    {
        ss->currentMove = move;
        ss->continuationHistory =
          &continuationHistory[ss->inCheck][capture][dirtyPiece.pc][move.to_sq()];
        ss->continuationCorrectionHistory =
          &continuationCorrectionHistory[dirtyPiece.pc][move.to_sq()];
    }
}

void Search::Worker::do_null_move(Position& pos, StateInfo& st, Stack* const ss) {
    pos.do_null_move(st);
    ss->currentMove                   = Move::null();
    ss->continuationHistory           = &continuationHistory[0][0][NO_PIECE][0];
    ss->continuationCorrectionHistory = &continuationCorrectionHistory[NO_PIECE][0];
}

void Search::Worker::undo_move(Position& pos, const Move move) {
    pos.undo_move(move);
    accumulatorStack.pop();
}

void Search::Worker::undo_null_move(Position& pos) { pos.undo_null_move(); }


// Reset histories, usually before a new game
void Search::Worker::clear() {
    mainHistory.fill(-5);
    captureHistory.fill(-699);

    // Each thread is responsible for clearing their part of shared history
    sharedHistory.correctionHistory.clear_range(-6, numaThreadIdx, numaTotal);
    sharedHistory.pawnHistory.clear_range(-1262, numaThreadIdx, numaTotal);

    ttMoveHistory = 0;

    for (auto& to : continuationCorrectionHistory)
        for (auto& h : to)
            h.fill(5);

    for (bool inCheck : {false, true})
        for (StatsType c : {NoCaptures, Captures})
            for (auto& to : continuationHistory[inCheck][c])
                for (auto& h : to)
                    h.fill(-552);

    for (usize i = 1; i < reductions.size(); ++i)
        reductions[i] = int(2834 / 128.0 * std::log(i));

    refreshTable.clear(network[numaAccessToken]);
}


// Main search function for both PV and non-PV nodes
template<NodeType nodeType>
Value Search::Worker::search(
  Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, const bool cutNode) {

    constexpr bool PvNode   = nodeType != NonPV;
    constexpr bool rootNode = nodeType == Root;
    const bool     allNode  = !(PvNode || cutNode);

    // Dive into quiescence search when the depth reaches zero
    if (depth <= 0)
        return qsearch<PvNode ? PV : NonPV>(pos, ss, alpha, beta);

    // Limit the depth if extensions made it too large
    depth = std::min(depth, MAX_PLY - 1);

    // Check if we have an upcoming move that draws by repetition
    if (!rootNode && alpha < VALUE_DRAW && pos.upcoming_repetition(ss->ply))
    {
        alpha = value_draw(nodes);
        if (alpha >= beta)
            return alpha;
    }

    assert(-VALUE_INFINITE <= alpha && alpha < beta && beta <= VALUE_INFINITE);
    assert(PvNode || (alpha == beta - 1));
    assert(0 < depth && depth < MAX_PLY);
    assert(!(PvNode && cutNode));

    PVMoves   pv;
    StateInfo st;

    Key   posKey;
    Move  move, excludedMove, bestMove;
    Depth extension, newDepth;
    Value bestValue, value, eval, maxValue, probCutBeta;
    bool  givesCheck, improving, priorCapture, opponentWorsening;
    bool  capture, ttCapture;
    int   priorReduction;
    Piece movedPiece;

    SearchedList capturesSearched;
    SearchedList quietsSearched;

    // Step 1. Initialize node
    ss->inCheck   = pos.checkers();
    priorCapture  = pos.captured_piece();
    Color us      = pos.side_to_move();
    ss->moveCount = 0;
    bestValue     = -VALUE_INFINITE;
    maxValue      = VALUE_INFINITE;

    ss->followPV = rootNode
                || ((ss - 1)->followPV
                    && (static_cast<usize>(ss->ply - 1) < lastIterationIdxPV.size()
                        && (ss - 1)->currentMove == lastIterationIdxPV[ss->ply - 1]));

    // Check for the available remaining time
    if (is_mainthread())
        main_manager()->check_time(*this);

    // Used to send selDepth info to GUI (selDepth counts from 1, ply from 0)
    if (PvNode && selDepth < ss->ply + 1)
        selDepth = ss->ply + 1;

    if (!rootNode)
    {
        // Step 2. Check for aborted search and immediate draw
        if (threads.stop.load(std::memory_order_relaxed) || pos.is_draw(ss->ply)
            || ss->ply >= MAX_PLY)
            return (ss->ply >= MAX_PLY && !ss->inCheck) ? evaluate(pos) : value_draw(nodes);

        // Step 3. Mate distance pruning. Even if we mate at the next move our score
        // would be at best mate_in(ss->ply + 1), but if alpha is already bigger because
        // a shorter mate was found upward in the tree then there is no need to search
        // because we will never beat the current alpha. Same logic but with reversed
        // signs apply also in the opposite condition of being mated instead of giving
        // mate. In this case, return a fail-high score.
        alpha = std::max(mated_in(ss->ply), alpha);
        beta  = std::min(mate_in(ss->ply + 1), beta);
        if (alpha >= beta)
            return alpha;
    }

    assert(0 <= ss->ply && ss->ply < MAX_PLY);

    Square prevSq  = ((ss - 1)->currentMove).is_ok() ? ((ss - 1)->currentMove).to_sq() : SQ_NONE;
    bestMove       = Move::none();
    priorReduction = (ss - 1)->reduction;
    (ss - 1)->reduction = 0;
    ss->statScore       = 0;
    (ss + 2)->cutoffCnt = 0;

    const auto correctionValue = correction_value(*this, pos, ss);

    // Step 4. Transposition table lookup
    excludedMove                   = ss->excludedMove;
    posKey                         = pos.key();
    auto [ttHit, ttData, ttWriter] = tt.probe(posKey);
    // Need further processing of the saved data
    ss->ttHit    = ttHit;
    ttData.move  = rootNode ? rootMoves[pvIdx].pv[0] : ttHit ? ttData.move : Move::none();
    ttData.value = ttHit ? value_from_tt(ttData.value, ss->ply, pos.rule50_count()) : VALUE_NONE;
    ss->ttPv     = excludedMove ? ss->ttPv : PvNode || (ttHit && ttData.is_pv);
    ttCapture    = ttData.move && pos.capture_stage(ttData.move);

    // Step 5. Static evaluation of the position
    Value unadjustedStaticEval = VALUE_NONE;

    // Skip early pruning when in check
    if (ss->inCheck)
        ss->staticEval = eval = (ss - 2)->staticEval;
    else if (excludedMove)
        unadjustedStaticEval = eval = ss->staticEval;
    else if (ss->ttHit)
    {
        // Never assume anything about values stored in TT
        unadjustedStaticEval = ttData.eval;
        if (!is_valid(unadjustedStaticEval))
            unadjustedStaticEval = evaluate(pos);

        ss->staticEval = eval = to_corrected_static_eval(unadjustedStaticEval, correctionValue);

        // ttValue can be used as a better position evaluation
        if (is_valid(ttData.value)
            && (ttData.bound & (ttData.value > eval ? BOUND_LOWER : BOUND_UPPER)))
            eval = ttData.value;
    }
    else
    {
        unadjustedStaticEval = evaluate(pos);
        ss->staticEval = eval = to_corrected_static_eval(unadjustedStaticEval, correctionValue);

        // Static evaluation is saved as it was before adjustment by correction history
        ttWriter.write(posKey, VALUE_NONE, ss->ttPv, BOUND_NONE, DEPTH_UNSEARCHED, Move::none(),
                       unadjustedStaticEval, tt.generation());
    }

    // Set up the improving flag, which is true if current static evaluation is
    // bigger than the previous static evaluation at our turn (if we were in
    // check at our previous move we go back until we weren't in check) and is
    // false otherwise. The improving flag is used in various pruning heuristics.
    // Similarly, opponentWorsening is true if our static evaluation is better
    // for us than at the last ply.
    improving         = ss->staticEval > (ss - 2)->staticEval;
    opponentWorsening = ss->staticEval > -(ss - 1)->staticEval;

    // Hindsight adjustment of reductions based on static evaluation difference.
    if (priorReduction >= 3 && !opponentWorsening)
        depth++;
    if (priorReduction >= 2 && depth >= 2 && ss->staticEval + (ss - 1)->staticEval > 173)
        depth--;

    // At non-PV nodes we check for an early TT cutoff
    if (!PvNode && !excludedMove && ttData.depth > depth - (ttData.value <= beta)
        && is_valid(ttData.value)  // Can happen when !ttHit or when access race in probe()
        && (ttData.bound & (ttData.value >= beta ? BOUND_LOWER : BOUND_UPPER))
        && (cutNode == (ttData.value >= beta) || depth > 4))
    {
        // If ttMove is quiet, update move sorting heuristics on TT hit
        if (ttData.move && ttData.value >= beta)
        {
            // Bonus for a quiet ttMove that fails high
            if (!ttCapture)
                update_quiet_histories(pos, ss, *this, ttData.move, std::min(114 * depth, 724));

            // Extra penalty for early quiet moves of the previous ply
            if (prevSq != SQ_NONE && (ss - 1)->moveCount < 4 && !priorCapture)
                update_continuation_histories(ss - 1, pos.piece_on(prevSq), prevSq, -2187);
        }

        // Partial workaround for the graph history interaction problem
        // For high rule50 counts don't produce transposition table cutoffs.
        if (pos.rule50_count() < 96)
        {
            if (depth >= 7 && ttData.move && pos.pseudo_legal(ttData.move) && pos.legal(ttData.move)
                && !is_decisive(ttData.value))
            {
                pos.do_move(ttData.move, st);
                Key nextPosKey                             = pos.key();
                auto [ttHitNext, ttDataNext, ttWriterNext] = tt.probe(nextPosKey);
                pos.undo_move(ttData.move);

                // Check that the ttValue after the tt move would also trigger a cutoff
                if (!is_valid(ttDataNext.value))
                    return ttData.value;

                if ((ttData.value >= beta) == (-ttDataNext.value >= beta))
                    return ttData.value;
            }
            else
                return ttData.value;
        }
    }  // No cutoff, but why? Does the stored inexact value mismatch our aspiration window?
    else if (!PvNode && !excludedMove && ttData.depth > depth - (ttData.value <= beta)
             && is_valid(ttData.value) && ttData.bound != BOUND_EXACT
             && ttData.bound & (ttData.value >= beta ? BOUND_UPPER : BOUND_LOWER) && depth > 5)
    {  // If a window-bound mismatch is the only reason cutoff failed, penalize the now-useless tte
        ttWriter.penalize(1);
    }

    // Step 6. Tablebases probe
    if (!rootNode && !excludedMove && tbConfig.cardinality)
    {
        int piecesCount = pos.count<ALL_PIECES>();

        if (piecesCount <= tbConfig.cardinality
            && (piecesCount < tbConfig.cardinality || depth >= tbConfig.probeDepth)
            && pos.rule50_count() == 0 && !pos.can_castle(ANY_CASTLING))
        {
            TB::ProbeState err;
            TB::WDLScore   wdl = TB::probe_wdl(pos, &err);

            // Force check of time on the next occasion
            if (is_mainthread())
                main_manager()->callsCnt = 0;

            if (err != TB::ProbeState::FAIL)
            {
                ++tbHits;

                int drawScore = tbConfig.useRule50 ? 1 : 0;

                Value tbValue = VALUE_TB - ss->ply;

                // Use the range VALUE_TB to VALUE_TB_WIN_IN_MAX_PLY to score
                value = wdl < -drawScore ? -tbValue
                      : wdl > drawScore  ? tbValue
                                         : VALUE_DRAW + 2 * wdl * drawScore;

                Bound b = wdl < -drawScore ? BOUND_UPPER
                        : wdl > drawScore  ? BOUND_LOWER
                                           : BOUND_EXACT;

                if (b == BOUND_EXACT || (b == BOUND_LOWER ? value >= beta : value <= alpha))
                {
                    ttWriter.write(posKey, value_to_tt(value, ss->ply), ss->ttPv, b,
                                   std::min(MAX_PLY - 1, depth + 6), Move::none(), VALUE_NONE,
                                   tt.generation());

                    return value;
                }

                if (PvNode)
                {
                    if (b == BOUND_LOWER)
                        bestValue = value, alpha = std::max(alpha, bestValue);
                    else
                        maxValue = value;
                }
            }
        }
    }

    if (ss->inCheck)
        goto moves_loop;

    // Use static evaluation difference to improve quiet move ordering
    if (((ss - 1)->currentMove).is_ok() && !(ss - 1)->inCheck && !priorCapture)
    {
        int evalDiff = std::clamp(-int((ss - 1)->staticEval + ss->staticEval), -183, 180) + 62;
        mainHistory[~us][((ss - 1)->currentMove).raw()] << evalDiff * 10;
        if (!ttHit && type_of(pos.piece_on(prevSq)) != PAWN
            && ((ss - 1)->currentMove).type_of() != PROMOTION)
            sharedHistory.pawn_entry(pos)[pos.piece_on(prevSq)][prevSq] << evalDiff * 13;
    }


    // Step 7. Razoring
    // If eval is really low, skip search entirely and return the qsearch value.
    // For PvNodes, we must have a guard against mates being returned.
    if (!PvNode && eval < alpha - 465 - 300 * depth * depth)
        return qsearch<NonPV>(pos, ss, alpha, beta);

    // Step 8. Futility pruning: child node
    // The depth condition is important for mate finding.
    if (!ss->ttPv && depth < 17 && eval >= beta && (!ttData.move || ttCapture) && !is_loss(beta)
        && !is_win(eval))
    {
        Value futilityMult = std::min(40 + depth * 4, 80);
        futilityMult -= 20 * !ss->ttHit;

        Value futilityMargin = futilityMult * depth
                             - (2934 * improving + 343 * opponentWorsening) * futilityMult / 1024
                             + std::abs(correctionValue) / 182069;

        if (eval - futilityMargin >= beta)
            return (716 * beta + 308 * eval) / 1024;
    }

    // Step 9. Null move search with verification search
    if (cutNode && ss->staticEval >= beta - 14 * depth - 45 * improving + 374 && !excludedMove
        && pos.non_pawn_material(us) && ss->ply >= nmpMinPly && !is_loss(beta))
    {
        assert((ss - 1)->currentMove != Move::null());

        // Null move dynamic reduction based on depth
        Depth R = 7 + depth / 3;
        do_null_move(pos, st, ss);

        Value nullValue = -search<NonPV>(pos, ss + 1, -beta, -beta + 1, depth - R, false);

        undo_null_move(pos);

        // Do not return unproven mate or TB scores
        if (nullValue >= beta && !is_win(nullValue))
        {
            if (nmpMinPly || depth < 16)
                return nullValue;

            assert(!nmpMinPly);  // Recursive verification is not allowed

            // Do verification search at high depths, with null move pruning disabled
            // until ply exceeds nmpMinPly.
            nmpMinPly = ss->ply + 3 * (depth - R) / 4;

            Value v = search<NonPV>(pos, ss, beta - 1, beta, depth - R, false);

            nmpMinPly = 0;

            if (v >= beta)
                return nullValue;
        }
    }

    improving |= ss->staticEval >= beta;

    // Step 10. Internal iterative reductions
    // At sufficient depth, reduce depth for PV/Cut nodes without a TTMove.
    // (*Scaler) Making IIR more aggressive scales poorly.
    if (!ss->followPV && !allNode && depth >= 6 && !ttData.move)
        depth--;

    // Step 11. ProbCut
    // If we have a good enough capture (or queen promotion) and a reduced search
    // returns a value much above beta, we can (almost) safely prune the previous move.
    probCutBeta = beta + 214 - 59 * improving;
    if (depth >= 3
        && !is_decisive(beta)
        // If value from transposition table is lower than probCutBeta, don't attempt
        // probCut there
        && !(is_valid(ttData.value) && ttData.value < probCutBeta))
    {
        assert(probCutBeta < VALUE_INFINITE && probCutBeta > beta);

        MovePicker mp(pos, ttData.move, probCutBeta - ss->staticEval, &captureHistory);
        Depth      probCutDepth = depth - 4 - improving;

        while ((move = mp.next_move()) != Move::none())
        {
            assert(move.is_ok());

            if (move == excludedMove || !pos.legal(move))
                continue;

            assert(pos.capture_stage(move));

            do_move(pos, move, st, ss);

            // Perform a preliminary qsearch to verify that the move holds
            value = -qsearch<NonPV>(pos, ss + 1, -probCutBeta, -probCutBeta + 1);

            // If the qsearch held, perform the regular search
            if (value >= probCutBeta && probCutDepth > 0)
                value = -search<NonPV>(pos, ss + 1, -probCutBeta, -probCutBeta + 1, probCutDepth,
                                       !cutNode);

            undo_move(pos, move);

            if (value >= probCutBeta)
            {
                // Save ProbCut data into transposition table
                ttWriter.write(posKey, value_to_tt(value, ss->ply), ss->ttPv, BOUND_LOWER,
                               probCutDepth + 1, move, unadjustedStaticEval, tt.generation());

                if (!is_decisive(value))
                    return value - (probCutBeta - beta);
            }
        }
    }

moves_loop:  // When in check, search starts here

    // Step 12. A small Probcut idea
    probCutBeta = beta + 428;
    if ((ttData.bound & BOUND_LOWER) && ttData.depth >= depth - 4 && ttData.value >= probCutBeta
        && !is_decisive(beta) && is_valid(ttData.value) && !is_decisive(ttData.value))
        return probCutBeta;

    const PieceToHistory* contHist[] = {
      (ss - 1)->continuationHistory, (ss - 2)->continuationHistory, (ss - 3)->continuationHistory,
      (ss - 4)->continuationHistory, (ss - 5)->continuationHistory, (ss - 6)->continuationHistory};


    MovePicker mp(pos, ttData.move, depth, &mainHistory, &lowPlyHistory, &captureHistory, contHist,
                  &sharedHistory, ss->ply);

    value = bestValue;

    int moveCount = 0;

    // Step 13. Loop through all pseudo-legal moves until no moves remain
    // or a beta cutoff occurs.
    while ((move = mp.next_move()) != Move::none())
    {
        assert(move.is_ok());

        if (move == excludedMove)
            continue;

        // Check for legality
        if (!pos.legal(move))
            continue;

        // At root obey the "searchmoves" option and skip moves not listed in Root
        // Move List. In MultiPV mode we also skip PV moves that have been already
        // searched and those of lower "TB rank" if we are in a TB root position.
        if (rootNode && !std::count(rootMoves.begin() + pvIdx, rootMoves.begin() + pvLast, move))
            continue;

        ss->moveCount = ++moveCount;

        if (rootNode && is_mainthread() && nodes > NODES_LIMIT_OUTPUT)
        {
            main_manager()->updates.onIter(
              {depth, UCIEngine::move(move, pos.is_chess960()), moveCount + pvIdx});
        }
        if (PvNode)
            (ss + 1)->pv = nullptr;

        extension  = 0;
        capture    = pos.capture_stage(move);
        movedPiece = pos.moved_piece(move);
        givesCheck = pos.gives_check(move);

        // Calculate new depth for this move
        newDepth = depth - 1;

        int delta = beta - alpha;

        int r = reduction(improving, depth, moveCount, delta);

        // Increase reduction for ttPv nodes (*Scaler)
        // Larger values scale well
        if (ss->ttPv)
            r += 1006;

        // Step 14. Pruning at shallow depths.
        // Depth conditions are important for mate finding.
        if (!rootNode && pos.non_pawn_material(us) && !is_loss(bestValue))
        {
            // Skip quiet moves if movecount exceeds our threshold
            if (moveCount >= (3 + depth * depth) / (2 - improving))
                mp.skip_quiet_moves();

            // Reduced depth of the next LMR search
            int lmrDepth = newDepth - r / 1024;

            if (capture || givesCheck)
            {
                Piece capturedPiece = pos.piece_on(move.to_sq());
                int   captHist = captureHistory[movedPiece][move.to_sq()][type_of(capturedPiece)];

                // Futility pruning for captures
                if (!givesCheck && lmrDepth < 7)
                {
                    Value futilityValue = ss->staticEval + 231 + 232 * lmrDepth
                                        + PieceValue[capturedPiece] + 131 * captHist / 1024;

                    if (futilityValue <= alpha)
                        continue;
                }

                // SEE based pruning for captures and checks
                // Avoid pruning sacrifices of our last piece for stalemate
                int margin = 175 * depth + captHist * 34 / 1024;
                if ((alpha >= VALUE_DRAW || pos.non_pawn_material(us) != PieceValue[movedPiece])
                    && !pos.see_ge(move, -margin))
                    continue;
            }
            else if (!ss->followPV || !PvNode)
            {
                int dIndex  = std::min(int(depth), int(lmrDivisor.size())) - 1;
                int history = (*contHist[0])[movedPiece][move.to_sq()]
                            + (*contHist[1])[movedPiece][move.to_sq()]
                            + sharedHistory.pawn_entry(pos)[movedPiece][move.to_sq()];

                // Continuation history based pruning
                if (history < -4313 * depth)
                    continue;

                history += 64 * mainHistory[us][move.raw()] / 32;

                // (*Scaler): Generally, lower divisors scale well
                lmrDepth += history / lmrDivisor[dIndex];

                Value futilityValue = ss->staticEval + 40 + 138 * !bestMove + 117 * lmrDepth
                                    + 90 * (ss->staticEval > alpha);

                // Futility pruning: parent node
                // (*Scaler): Generally, more frequent futility pruning
                // scales well
                if (!ss->inCheck && lmrDepth < 12 && futilityValue <= alpha)
                {
                    if (bestValue <= futilityValue && !is_decisive(bestValue)
                        && !is_win(futilityValue))
                        bestValue = futilityValue;
                    continue;
                }

                lmrDepth = std::max(lmrDepth, 0);

                // Prune moves with negative SEE
                if (!pos.see_ge(move, -25 * lmrDepth * lmrDepth))
                    continue;
            }
        }

        // Step 15. Extensions
        // Singular extension search. If all moves but one
        // fail low on a search of (alpha-s, beta-s), and just one fails high on
        // (alpha, beta), then that move is singular and should be extended. To
        // verify this we do a reduced search on the position excluding the ttMove
        // and if the result is lower than ttValue minus a margin, then we will
        // extend the ttMove. Recursive singular search is avoided.

        // (*Scaler) Generally, higher singularBeta (i.e closer to ttValue)
        // and lower extension margins scale well.
        if (!rootNode && move == ttData.move && !excludedMove && depth >= 6 + ss->ttPv
            && is_valid(ttData.value) && !is_decisive(ttData.value) && (ttData.bound & BOUND_LOWER)
            && ttData.depth >= depth - 3 && !is_shuffling(move, ss, pos))
        {
            Value singularBeta  = ttData.value - (60 + 70 * (ss->ttPv && !PvNode)) * depth / 59;
            Depth singularDepth = newDepth / 2;

            ss->excludedMove = move;
            value = search<NonPV>(pos, ss, singularBeta - 1, singularBeta, singularDepth, cutNode);
            ss->excludedMove = Move::none();

            if (value < singularBeta)
            {
                int corrValAdj   = std::abs(correctionValue) / 194822;
                int doubleMargin = -3 + 201 * PvNode - 157 * !ttCapture - corrValAdj
                                 - 1081 * ttMoveHistory / 117824 - (ss->ply > rootDepth) * 41;
                int tripleMargin = 72 + 306 * PvNode - 188 * !ttCapture + 84 * ss->ttPv - corrValAdj
                                 - (ss->ply > rootDepth) * 45;

                extension =
                  1 + (value < singularBeta - doubleMargin) + (value < singularBeta - tripleMargin);

                depth++;
            }

            // Multi-cut pruning
            // Our ttMove is assumed to fail high based on the bound of the TT entry,
            // and if after excluding the ttMove with a reduced search we fail high
            // over the original beta, we assume this expected cut-node is not
            // singular (multiple moves fail high), and we can prune the whole
            // subtree by returning a softbound.
            else if (value >= beta && !is_decisive(value))
            {
                ttMoveHistory << -442 - 108 * depth;
                return value;
            }

            // Negative extensions
            // If other moves failed high over (ttValue - margin) without the
            // ttMove on a reduced search, but we cannot do multi-cut because
            // (ttValue - margin) is lower than the original beta, we do not know
            // if the ttMove is singular or can do a multi-cut, so we reduce the
            // ttMove in favor of other moves based on some conditions:

            // If the ttMove is assumed to fail high over current beta
            else if (ttData.value >= beta)
                extension = -3;

            // If we are on a cutNode but the ttMove is not assumed to fail high
            // over current beta
            else if (cutNode)
                extension = -2;
        }

        u64 nodeCount = rootNode ? u64(nodes) : 0;

        // Step 16. Make the move
        do_move(pos, move, st, givesCheck, ss);

        // Add extension to new depth
        newDepth += extension;

        // Decrease reduction for PvNodes (*Scaler)
        if (ss->ttPv)
            r -= 2766 + PvNode * 1017 + (ttData.value > alpha) * 838
               + (ttData.depth >= depth) * (923 + cutNode * 955);

        r += 714;  // Base reduction offset to compensate for other tweaks
        r -= moveCount * 62;
        r -= std::abs(correctionValue) / 26131;

        // Increase reduction for cut nodes
        if (cutNode)
            r += 3995 + 1059 * !ttData.move;

        // Increase reduction if ttMove is a capture
        if (ttCapture)
            r += 1039;

        // Increase reduction if next ply has a lot of fail high
        if ((ss + 1)->cutoffCnt > 1)
            r += 236 + 1079 * ((ss + 1)->cutoffCnt > 2) + 1143 * allNode;

        // For first picked move (ttMove) reduce reduction
        else if (move == ttData.move)
            r -= 2016;

        if (capture)
            ss->statScore = 809 * int(PieceValue[pos.captured_piece()]) / 128
                          + captureHistory[movedPiece][move.to_sq()][type_of(pos.captured_piece())];
        else
            ss->statScore = 2 * mainHistory[us][move.raw()]
                          + (*contHist[0])[movedPiece][move.to_sq()]
                          + (*contHist[1])[movedPiece][move.to_sq()];

        // Decrease/increase reduction for moves with a good/bad history
        r -= ss->statScore * 445 / 4096;

        // Scale up reductions for expected ALL nodes
        if (allNode)
            r += r * 272 / (256 * depth + 285);

        // Step 17. Late moves reduction / extension (LMR)
        if (depth >= 2 && moveCount > 1)
        {
            // In general we want to cap the LMR depth search at newDepth, but when
            // reduction is negative, we allow this move a limited search extension
            // beyond the first move depth.
            // To prevent problems when the max value is less than the min value,
            // std::clamp has been replaced by a more robust implementation.
            Depth d = std::max(1, std::min(newDepth - r / 1024, newDepth + 2)) + PvNode;

            ss->reduction = newDepth - d;
            value         = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha, d, true);
            ss->reduction = 0;

            // Do a full-depth search when reduced LMR search fails high
            // (*Scaler) Shallower searches here don't scale well
            if (value > alpha)
            {
                // Adjust full-depth search based on LMR results - if the result was
                // good enough search deeper, if it was bad enough search shallower.
                const bool doDeeperSearch    = d < newDepth && value > bestValue + 52;
                const bool doShallowerSearch = value < bestValue + 9;

                newDepth += doDeeperSearch - doShallowerSearch;

                if (newDepth > d)
                    value = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha, newDepth, !cutNode);

                // Post LMR continuation history updates
                update_continuation_histories(ss, movedPiece, move.to_sq(), 1415);
            }
        }

        // Step 18. Full-depth search when LMR is skipped
        else if (!PvNode || moveCount > 1)
        {
            // Increase reduction if ttMove is not present
            if (!ttData.move)
                r += 1085;

            // Note that if expected reduction is high, we reduce search depth here
            value = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha,
                                   newDepth - (r > 5039) - (r > 5223 && newDepth > 2), !cutNode);
        }

        // For PV nodes only, do a full PV search on the first move or after a fail high,
        // otherwise let the parent node fail low with value <= alpha and try another move.
        if (PvNode && (moveCount == 1 || value > alpha))
        {
            (ss + 1)->pv = &pv;
            (ss + 1)->pv->clear();

            // Extend move from transposition table if we are about to dive into qsearch.
            // decisive score handling improves mate finding and retrograde analysis.
            if (move == ttData.move
                && ((is_valid(ttData.value) && is_decisive(ttData.value) && ttData.depth > 0)
                    || ttData.depth > 1))
                newDepth = std::max(newDepth, 1);

            value = -search<PV>(pos, ss + 1, -beta, -alpha, newDepth, false);
        }

        // Step 19. Undo move
        undo_move(pos, move);

        assert(value > -VALUE_INFINITE && value < VALUE_INFINITE);

        // Step 20. Check for a new best move
        // Finished searching the move. If a stop occurred, the return value of
        // the search cannot be trusted, and we return immediately without updating
        // best move, principal variation nor transposition table.
        if (threads.stop.load(std::memory_order_relaxed))
            return VALUE_ZERO;

        if (rootNode)
        {
            RootMove& rm = *std::find(rootMoves.begin(), rootMoves.end(), move);

            rm.effort += nodes - nodeCount;

            u64 N      = nodes - nodeCount;
            u64 E_prev = std::max(u64(1), rm.effort - N);

            // Dynamic EMA parameters for root move
            constexpr u64 Scale          = 32;
            constexpr u64 ChiNumerator   = 3;
            constexpr u64 ChiDenominator = 2;   // Chi = 3/2 = 1.5
            constexpr u64 MinWeight      = 12;  // 37.5% minimum weight
            constexpr u64 MaxWeight      = 24;  // 75% maximum weight

            u64 w     = std::clamp((Scale * N * ChiDenominator)
                                     / (N * ChiDenominator + ChiNumerator * E_prev),
                                   MinWeight, MaxWeight);
            u64 w_mss = std::min(w, u64(16));
            i64 v2    = i64(value) * std::abs(value);

            if (rm.averageScore == -VALUE_INFINITE)
                rm.averageScore = value;
            else
                rm.averageScore = Value((value * w + rm.averageScore * (Scale - w)) / Scale);

            if (rm.meanSquaredScore == -VALUE_INFINITE * VALUE_INFINITE)
                rm.meanSquaredScore = value * std::abs(value);
            else
                rm.meanSquaredScore =
                  Value((v2 * w_mss + int64_t(rm.meanSquaredScore) * (Scale - w_mss)) / Scale);

            // PV move or new best move?
            if (moveCount == 1 || value > alpha)
            {
                rm.score = rm.uciScore = value;
                rm.selDepth            = selDepth;
                rm.unset_bound_flags();

                if (value >= beta)
                {
                    rm.scoreLowerbound = true;
                    rm.uciScore        = beta;
                }
                else if (value <= alpha)
                {
                    rm.scoreUpperbound = true;
                    rm.uciScore        = alpha;
                }

                rm.pv.resize(1);

                assert((ss + 1)->pv);

                for (Move pvMove : *(ss + 1)->pv)
                    rm.pv.push_back(pvMove);

                // We record how often the best move has been changed in each iteration.
                // This information is used for time management. In MultiPV mode,
                // we must take care to only do this for the first PV line.
                if (moveCount > 1 && !pvIdx)
                    ++bestMoveChanges;
            }
            else
                // All other moves but the PV, are set to the lowest value: this
                // is not a problem when sorting because the sort is stable and the
                // move position in the list is preserved - just the PV is pushed up.
                rm.score = -VALUE_INFINITE;
        }

        // In case we have an alternative move equal in eval to the current bestmove,
        // promote it to bestmove by pretending it just exceeds alpha (but not beta).
        int inc = (value == bestValue && ss->ply + 2 >= rootDepth && (int(nodes) & 14) == 0
                   && !is_win(std::abs(value) + 1));

        if (value + inc > bestValue)
        {
            bestValue = value;

            if (value + inc > alpha)
            {
                bestMove = move;

                if (PvNode && !rootNode)  // Update pv even in fail-high case
                    ss->pv->update(move, (ss + 1)->pv);

                if (value >= beta)
                {
                    // (*Scaler) Infrequent and small updates scale well
                    ss->cutoffCnt += (extension < 2) || PvNode;
                    assert(value >= beta);  // Fail high
                    break;
                }

                // Reduce other moves if we have found at least one score improvement
                if (depth > 2 && depth < 13 && !is_decisive(value))
                    depth -= 2;

                assert(depth > 0);
                alpha = value;  // Update alpha! Always alpha < beta
            }
        }

        // If the move is worse than some previously searched move,
        // remember it, to update its stats later.
        if (move != bestMove && moveCount <= SEARCHEDLIST_CAPACITY)
        {
            if (capture)
                capturesSearched.push_back(move);
            else
                quietsSearched.push_back(move);
        }
    }

    // Step 21. Check for mate and stalemate
    // All legal moves have been searched and if there are no legal moves, it
    // must be a mate or a stalemate. If we are in a singular extension search then
    // return a fail low score.

    assert(moveCount || !ss->inCheck || excludedMove || !MoveList<LEGAL>(pos).size());

    // Adjust best value for fail high cases
    if (bestValue >= beta && !is_decisive(bestValue) && !is_decisive(alpha))
        bestValue = (bestValue * depth + beta) / (depth + 1);

    if (!moveCount)
        bestValue = excludedMove ? alpha : ss->inCheck ? mated_in(ss->ply) : VALUE_DRAW;

    // If there is a move that produces search value greater than alpha,
    // we update the stats of searched moves.
    else if (bestMove)
    {
        update_all_stats(pos, ss, *this, bestMove, prevSq, quietsSearched, capturesSearched, depth,
                         ttData.move, PvNode);
        if (!PvNode)
            ttMoveHistory << (bestMove == ttData.move ? 792 : -779);
    }

    // Bonus for prior quiet countermove that caused the fail low
    else if (!priorCapture && prevSq != SQ_NONE)
    {
        int bonusScale = -245;
        bonusScale -= (ss - 1)->statScore / 98;
        bonusScale += std::min(59 * depth, 430);
        bonusScale += 191 * ((ss - 1)->moveCount > 8);
        bonusScale += 143 * (!ss->inCheck && bestValue <= ss->staticEval - 103);
        bonusScale += 151 * (!(ss - 1)->inCheck && bestValue <= -(ss - 1)->staticEval - 78);

        bonusScale = std::max(bonusScale, 0);

        // scaledBonus ranges from 0 to roughly 2.3M, overflows happen for multipliers larger than 900
        const int scaledBonus = std::min(141 * depth - 82, 1472) * bonusScale;

        update_continuation_histories(ss - 1, pos.piece_on(prevSq), prevSq,
                                      scaledBonus * 236 / 16384);

        mainHistory[~us][((ss - 1)->currentMove).raw()] << scaledBonus * 234 / 32768;

        if (type_of(pos.piece_on(prevSq)) != PAWN && ((ss - 1)->currentMove).type_of() != PROMOTION)
            sharedHistory.pawn_entry(pos)[pos.piece_on(prevSq)][prevSq] << scaledBonus * 322 / 8192;
    }

    // Bonus for prior capture countermove that caused the fail low
    else if (priorCapture && prevSq != SQ_NONE)
    {
        Piece capturedPiece = pos.captured_piece();
        assert(capturedPiece != NO_PIECE);
        captureHistory[pos.piece_on(prevSq)][prevSq][type_of(capturedPiece)] << 901;
    }

    if (PvNode)
        bestValue = std::min(bestValue, maxValue);

    // If no good move is found and the previous position was ttPv, then the previous
    // opponent move is probably good and the new position is added to the search tree.
    if (bestValue <= alpha)
        ss->ttPv = ss->ttPv || (ss - 1)->ttPv;

    // Write gathered information in transposition table. Note that the
    // static evaluation is saved as it was before correction history.
    if (!excludedMove && !(rootNode && pvIdx))
        ttWriter.write(posKey, value_to_tt(bestValue, ss->ply), ss->ttPv,
                       bestValue >= beta    ? BOUND_LOWER
                       : PvNode && bestMove ? BOUND_EXACT
                                            : BOUND_UPPER,
                       moveCount != 0 ? depth : std::min(MAX_PLY - 1, depth + 6), bestMove,
                       unadjustedStaticEval, tt.generation());

    // Adjust correction history if the best move is not a capture
    // and the error direction matches whether we are above/below bounds.
    if (!ss->inCheck && !(bestMove && pos.capture(bestMove))
        && (bestValue > ss->staticEval) == bool(bestMove))
    {
        auto bonus =
          std::clamp(int(bestValue - ss->staticEval) * depth * (bestMove ? 12 : 18) / 128,
                     -CORRECTION_HISTORY_LIMIT / 4, CORRECTION_HISTORY_LIMIT / 4);
        update_correction_history(pos, ss, *this, 1114 * bonus / 1024);
    }

    assert(bestValue > -VALUE_INFINITE && bestValue < VALUE_INFINITE);

    return bestValue;
}


// Quiescence search function, which is called by the main search function with
// depth zero, or recursively with further decreasing depth. With depth <= 0, we
// "should" be using static eval only, but tactical moves may confuse the static eval.
// To fight this horizon effect, we implement this qsearch of tactical moves.
// See https://www.chessprogramming.org/Horizon_Effect
// and https://www.chessprogramming.org/Quiescence_Search
template<NodeType nodeType>
Value Search::Worker::qsearch(Position& pos, Stack* ss, Value alpha, Value beta) {

    static_assert(nodeType != Root);
    constexpr bool PvNode = nodeType == PV;

    assert(alpha >= -VALUE_INFINITE && alpha < beta && beta <= VALUE_INFINITE);
    assert(PvNode || (alpha == beta - 1));

    // Check if we have an upcoming move that draws by repetition
    if (alpha < VALUE_DRAW && pos.upcoming_repetition(ss->ply))
    {
        alpha = value_draw(nodes);
        if (alpha >= beta)
            return alpha;
    }

    PVMoves   pv;
    StateInfo st;

    Key   posKey;
    Move  move, bestMove;
    Value bestValue, value, futilityBase;
    bool  pvHit, givesCheck, capture;
    int   moveCount;

    // Step 1. Initialize node
    if (PvNode)
    {
        (ss + 1)->pv = &pv;
        ss->pv->clear();
    }

    bestMove    = Move::none();
    ss->inCheck = pos.checkers();
    moveCount   = 0;

    // Used to send selDepth info to GUI (selDepth counts from 1, ply from 0)
    if (PvNode && selDepth < ss->ply + 1)
        selDepth = ss->ply + 1;

    // Step 2. Check for an immediate draw or maximum ply reached
    if (pos.is_draw(ss->ply) || ss->ply >= MAX_PLY)
        return (ss->ply >= MAX_PLY && !ss->inCheck) ? evaluate(pos) : VALUE_DRAW;

    assert(0 <= ss->ply && ss->ply < MAX_PLY);

    // Step 3. Transposition table lookup
    posKey                         = pos.key();
    auto [ttHit, ttData, ttWriter] = tt.probe(posKey);
    // Need further processing of the saved data
    ss->ttHit    = ttHit;
    ttData.move  = ttHit ? ttData.move : Move::none();
    ttData.value = ttHit ? value_from_tt(ttData.value, ss->ply, pos.rule50_count()) : VALUE_NONE;
    pvHit        = ttHit && ttData.is_pv;

    // At non-PV nodes we check for an early TT cutoff
    if (!PvNode && ttData.depth >= DEPTH_QS
        && is_valid(ttData.value)  // Can happen when !ttHit or when access race in probe()
        && (ttData.bound & (ttData.value >= beta ? BOUND_LOWER : BOUND_UPPER)))
        return ttData.value;

    // Step 4. Static evaluation of the position
    Value unadjustedStaticEval = VALUE_NONE;
    if (ss->inCheck)
        bestValue = futilityBase = -VALUE_INFINITE;
    else
    {
        const auto correctionValue = correction_value(*this, pos, ss);

        if (ss->ttHit)
        {
            // Never assume anything about values stored in TT
            unadjustedStaticEval = ttData.eval;

            if (!is_valid(unadjustedStaticEval))
                unadjustedStaticEval = evaluate(pos);

            ss->staticEval = bestValue =
              to_corrected_static_eval(unadjustedStaticEval, correctionValue);

            // ttValue can be used as a better position evaluation
            if (is_valid(ttData.value) && !is_decisive(ttData.value)
                && (ttData.bound & (ttData.value > bestValue ? BOUND_LOWER : BOUND_UPPER)))
                bestValue = ttData.value;
        }
        else
        {
            unadjustedStaticEval = evaluate(pos);
            ss->staticEval       = bestValue =
              to_corrected_static_eval(unadjustedStaticEval, correctionValue);
        }

        // Stand pat. Return immediately if static value is at least beta
        if (bestValue >= beta)
        {
            if (!is_decisive(bestValue))
                bestValue = (467 * bestValue + 557 * beta) / 1024;

            if (!ss->ttHit)
                ttWriter.write(posKey, VALUE_NONE, false, BOUND_LOWER, DEPTH_UNSEARCHED,
                               Move::none(), unadjustedStaticEval, tt.generation());
            return bestValue;
        }

        if (bestValue > alpha)
            alpha = bestValue;

        futilityBase = ss->staticEval + 335;
    }

    const PieceToHistory* contHist[] = {(ss - 1)->continuationHistory};

    Square prevSq = ((ss - 1)->currentMove).is_ok() ? ((ss - 1)->currentMove).to_sq() : SQ_NONE;

    // Initialize a MovePicker object for the current position, and prepare to search
    // the moves. We presently use two stages of move generator in quiescence search:
    // captures, or evasions only when in check.
    MovePicker mp(pos, ttData.move, DEPTH_QS, &mainHistory, &lowPlyHistory, &captureHistory,
                  contHist, &sharedHistory, ss->ply);

    // Step 5. Loop through all pseudo-legal moves until no moves remain or a beta
    // cutoff occurs.
    while ((move = mp.next_move()) != Move::none())
    {
        assert(move.is_ok());

        if (!pos.legal(move))
            continue;

        givesCheck = pos.gives_check(move);
        capture    = pos.capture_stage(move);

        moveCount++;

        // Step 6. Pruning
        if (!is_loss(bestValue))
        {
            // Futility pruning and moveCount pruning
            if (!givesCheck && move.to_sq() != prevSq && !is_loss(futilityBase)
                && move.type_of() != PROMOTION)
            {
                if (moveCount > 2)
                    continue;

                Value futilityValue = futilityBase + PieceValue[pos.piece_on(move.to_sq())];

                // If static eval + value of piece we are going to capture is
                // much lower than alpha, we can prune this move.
                if (futilityValue <= alpha)
                {
                    bestValue = std::max(bestValue, futilityValue);
                    continue;
                }

                // If static exchange evaluation is low enough
                // we can prune this move.
                if (!pos.see_ge(move, alpha - futilityBase))
                {
                    bestValue = std::max(bestValue, std::min(alpha, futilityBase));
                    continue;
                }
            }

            // Skip non-captures
            if (!capture)
                continue;

            // Do not search moves with bad enough SEE values
            if (!pos.see_ge(move, -74))
                continue;
        }

        // Step 7. Make and search the move
        do_move(pos, move, st, givesCheck, ss);

        value = -qsearch<nodeType>(pos, ss + 1, -beta, -alpha);
        undo_move(pos, move);

        assert(value > -VALUE_INFINITE && value < VALUE_INFINITE);

        // Step 8. Check for a new best move
        if (value > bestValue)
        {
            bestValue = value;

            if (value > alpha)
            {
                bestMove = move;

                if (PvNode)  // Update pv even in fail-high case
                    ss->pv->update(move, (ss + 1)->pv);

                if (value < beta)  // Update alpha here!
                    alpha = value;
                else
                    break;  // Fail high
            }
        }
    }

    // Step 9. Check for mate and stalemate
    // All legal moves have been searched. A special case: if we are
    // in check and no legal moves were found, it is checkmate.
    if (!moveCount)
    {
        if (ss->inCheck)  // Checkmate!
        {
            assert(!MoveList<LEGAL>(pos).size());
            return mated_in(ss->ply);  // Plies to mate from the root
        }

        // Only check for stalemate under specific conditions
        Color us = pos.side_to_move();
        if (!(pawn_single_push_bb(us, pos.pieces(us, PAWN)) & ~pos.pieces())
            && !pos.non_pawn_material(us) && type_of(pos.captured_piece()) >= KNIGHT
            && !MoveList<LEGAL>(pos).size())
            bestValue = VALUE_DRAW;
    }

    if (!is_decisive(bestValue) && bestValue > beta)
        bestValue = (481 * bestValue + 543 * beta) / 1024;

    // Save gathered info in transposition table. The static evaluation
    // is saved as it was before adjustment by correction history.
    ttWriter.write(posKey, value_to_tt(bestValue, ss->ply), pvHit,
                   bestValue >= beta ? BOUND_LOWER : BOUND_UPPER, DEPTH_QS, bestMove,
                   unadjustedStaticEval, tt.generation());

    assert(bestValue > -VALUE_INFINITE && bestValue < VALUE_INFINITE);

    return bestValue;
}

int Search::Worker::reduction(bool i, Depth d, int mn, int delta) const {
    int reductionScale = reductions[d] * reductions[mn];
    return reductionScale - delta * 617 / rootDelta + !i * reductionScale * 194 / 512 + 1027;
}

// elapsed() returns the time elapsed since the search started. If the
// 'nodestime' option is enabled, it will return the count of nodes searched
// instead. This function is called to check whether the search should be
// stopped based on predefined thresholds like time limits or nodes searched.
TimePoint Search::Worker::elapsed() const {
    return main_manager()->tm.elapsed([this]() { return threads.nodes_searched(); });
}

Value Search::Worker::evaluate(const Position& pos) {
    return Eval::evaluate(network[numaAccessToken], pos, accumulatorStack, refreshTable,
                          optimism[pos.side_to_move()]);
}

namespace {
// Adjusts a mate or TB score from "plies to mate from the root" to
// "plies to mate from the current position". Standard scores are unchanged.
// The function is called before storing a value in the transposition table.
Value value_to_tt(Value v, int ply) { return is_win(v) ? v + ply : is_loss(v) ? v - ply : v; }


// Inverse of value_to_tt(): it adjusts a mate or TB score from the transposition
// table (which refers to the plies to mate/be mated from current position) to
// "plies to mate/be mated (TB win/loss) from the root". However, to avoid
// potentially false mate or TB scores related to the 50 moves rule and the
// graph history interaction, we return the highest non-TB score instead.
Value value_from_tt(Value v, int ply, int r50c) {

    if (!is_valid(v))
        return VALUE_NONE;

    // handle TB win or better
    if (is_win(v))
    {
        // Downgrade a potentially false mate score
        if (is_mate(v) && VALUE_MATE - v > 100 - r50c)
            return VALUE_TB_WIN_IN_MAX_PLY - 1;

        // Downgrade a potentially false TB score.
        if (VALUE_TB - v > 100 - r50c)
            return VALUE_TB_WIN_IN_MAX_PLY - 1;

        return v - ply;
    }

    // handle TB loss or worse
    if (is_loss(v))
    {
        // Downgrade a potentially false mate score.
        if (is_mated(v) && VALUE_MATE + v > 100 - r50c)
            return VALUE_TB_LOSS_IN_MAX_PLY + 1;

        // Downgrade a potentially false TB score.
        if (VALUE_TB + v > 100 - r50c)
            return VALUE_TB_LOSS_IN_MAX_PLY + 1;

        return v + ply;
    }

    return v;
}


// Updates stats at the end of search() when a bestMove is found
void update_all_stats(const Position& pos,
                      Stack*          ss,
                      Search::Worker& workerThread,
                      Move            bestMove,
                      Square          prevSq,
                      SearchedList&   quietsSearched,
                      SearchedList&   capturesSearched,
                      Depth           depth,
                      Move            ttMove,
                      bool            PvNode) {

    CapturePieceToHistory& captureHistory = workerThread.captureHistory;
    Piece                  movedPiece     = pos.moved_piece(bestMove);
    PieceType              capturedPiece;

    int bonus =
      std::min(134 * depth - 79, 1572) + 382 * (bestMove == ttMove) + (ss - 1)->statScore / 30;
    int malus = std::min(1005 * depth - 205, 2218);

    if (!PvNode)
        // Important: don't remove the cast to a 64-bit number else the multiplication
        // can overflow on 32-bit platforms which would change the bench signature
        bonus += int(bonus * u64(quietsSearched.size() + capturesSearched.size()) / 256);

    if (!pos.capture_stage(bestMove))
    {
        update_quiet_histories(pos, ss, workerThread, bestMove, bonus * 824 / 1024);

        int actualMalus = malus * 1136 / 1024;
        // Decrease stats for all non-best quiet moves
        for (Move move : quietsSearched)
        {
            actualMalus = actualMalus * 956 / 1024;
            update_quiet_histories(pos, ss, workerThread, move, -actualMalus);
        }
    }
    else
    {
        // Increase stats for the best move in case it was a capture move
        capturedPiece = type_of(pos.piece_on(bestMove.to_sq()));
        captureHistory[movedPiece][bestMove.to_sq()][capturedPiece] << bonus * 1366 / 1024;
    }

    // Extra penalty for a quiet early move that was not a TT move in
    // previous ply when it gets refuted.
    if (prevSq != SQ_NONE && ((ss - 1)->moveCount == 1 + (ss - 1)->ttHit) && !pos.captured_piece())
        update_continuation_histories(ss - 1, pos.piece_on(prevSq), prevSq, -malus * 683 / 1024);

    // Decrease stats for all non-best capture moves
    for (Move move : capturesSearched)
    {
        movedPiece    = pos.moved_piece(move);
        capturedPiece = type_of(pos.piece_on(move.to_sq()));
        captureHistory[movedPiece][move.to_sq()][capturedPiece] << -malus * 1518 / 1024;
    }
}


// Updates the continuation histories for the move pairs formed by
// the current move and the moves played in previous plies.
void update_continuation_histories(Stack* ss, Piece pc, Square to, int bonus) {
    static constexpr std::array<ConthistBonus, 6> conthist_bonuses = {
      {{1, 1040}, {2, 780}, {3, 300}, {4, 537}, {5, 129}, {6, 423}}};

    // Multipliers for positive history consistency
    constexpr int CMHCMultipliers[] = {96, 113, 101, 105, 127, 121, 126};
    int           positiveCount     = 0;

    for (const auto [i, weight] : conthist_bonuses)
    {
        // Only update the first 2 continuation histories if we are in check
        if (ss->inCheck && i > 2)
            break;

        if (((ss - i)->currentMove).is_ok())
        {
            auto& historyEntry = (*(ss - i)->continuationHistory)[pc][to];
            if (historyEntry > 0)
                positiveCount++;

            int multiplier = CMHCMultipliers[positiveCount];
            historyEntry << (bonus * weight * multiplier / 131072) + 71 * (i < 2);
        }
    }
}

// Updates move sorting heuristics

void update_quiet_histories(
  const Position& pos, Stack* ss, Search::Worker& workerThread, Move move, int bonus) {

    Color us = pos.side_to_move();
    workerThread.mainHistory[us][move.raw()] << bonus;  // Untuned to prevent duplicate effort

    if (ss->ply < LOW_PLY_HISTORY_SIZE)
        workerThread.lowPlyHistory[ss->ply][move.raw()] << bonus * 663 / 1024;

    update_continuation_histories(ss, pos.moved_piece(move), move.to_sq(), bonus * 820 / 1024);

    workerThread.sharedHistory.pawn_entry(pos)[pos.moved_piece(move)][move.to_sq()]
      << bonus * (bonus > -7 ? 1038 : 525) / 1024;
}
}

// When playing with strength handicap, choose the best move among a set of
// RootMoves using a statistical rule dependent on 'level'. Idea by Heinz van Saanen.
Move Skill::pick_best(const RootMoves& rootMoves, usize multiPV) {
    static PRNG rng(now());  // PRNG sequence should be non-deterministic

    // With tablebases at the root, rootMoves are ordered by tbRank rather than by
    // score, so compute the score range explicitly to keep 'delta' non-negative.
    Value topScore = rootMoves[0].score;
    Value minScore = rootMoves[0].score;
    for (usize i = 1; i < multiPV; ++i)
    {
        topScore = std::max(topScore, rootMoves[i].score);
        minScore = std::min(minScore, rootMoves[i].score);
    }
    int    delta    = std::min(topScore - minScore, int(PawnValue));
    int    maxScore = -VALUE_INFINITE;
    double weakness = 120 - 2 * level;

    // Choose best move. For each move score we add two terms, both dependent on
    // weakness. One is deterministic and bigger for weaker levels, and one is
    // random. Then we choose the move with the resulting highest score.
    for (usize i = 0; i < multiPV; ++i)
    {
        // This is our magic formula
        int push = int(weakness * int(topScore - rootMoves[i].score)
                       + delta * (rng.rand<unsigned>() % int(weakness)))
                 / 128;

        if (rootMoves[i].score + push >= maxScore)
        {
            maxScore = rootMoves[i].score + push;
            best     = rootMoves[i].pv[0];
        }
    }

    return best;
}

// Used to print debug info and, more importantly, to detect
// when we are out of available time and thus stop the search.
void SearchManager::check_time(Search::Worker& worker) {
    if (--callsCnt > 0)
        return;

    // When using nodes, ensure checking rate is not lower than 0.1% of nodes
    callsCnt = worker.limits.nodes ? std::min(512, int(worker.limits.nodes / 1024)) : 512;

    static TimePoint lastInfoTime = now();

    TimePoint elapsed = tm.elapsed([&worker]() { return worker.threads.nodes_searched(); });
    TimePoint tick    = worker.limits.startTime + elapsed;

    if (tick - lastInfoTime >= 1000)
    {
        lastInfoTime = tick;
        dbg_print();
    }

    // We should not stop pondering until told so by the GUI
    if (ponder)
        return;

    if ((worker.limits.use_time_management() && (elapsed > tm.maximum() || stopOnPonderhit))
        || (worker.limits.movetime && elapsed >= worker.limits.movetime)
        || (worker.limits.nodes && worker.threads.nodes_searched() >= worker.limits.nodes))
        worker.threads.stop = true;
}

// Used to correct and extend PVs for moves that have a TB (but not a mate) score.
// Keeps the search based PV for as long as it is verified to maintain the game
// outcome, truncates afterwards. Finally, extends to mate the PV, providing a
// possible continuation (but not a proven mating line).
void syzygy_extend_pv(const OptionsMap&         options,
                      const Search::LimitsType& limits,
                      Position&                 pos,
                      RootMove&                 rootMove,
                      Value&                    v) {

    auto t_start      = std::chrono::steady_clock::now();
    int  moveOverhead = int(options["Move Overhead"]);
    bool rule50       = bool(options["Syzygy50MoveRule"]);

    // Do not use more than moveOverhead / 2 time, if time management is active
    auto time_abort = [&t_start, &moveOverhead, &limits]() -> bool {
        auto t_end = std::chrono::steady_clock::now();
        return limits.use_time_management()
            && 2 * std::chrono::duration<double, std::milli>(t_end - t_start).count()
                 > moveOverhead;
    };

    std::list<StateInfo> sts;

    // Step 0, do the rootMove, no correction allowed, as needed for MultiPV in TB.
    auto& stRoot = sts.emplace_back();
    pos.do_move(rootMove.pv[0], stRoot);
    int ply = 1;

    // Step 1, walk the PV to the last position in TB with correct decisive score
    while (usize(ply) < rootMove.pv.size())
    {
        Move& pvMove = rootMove.pv[ply];

        RootMoves legalMoves;
        for (const auto& m : MoveList<LEGAL>(pos))
            legalMoves.emplace_back(m);

        TB::Config config = TB::rank_root_moves(options, pos, legalMoves, false, time_abort);
        RootMove&  rm     = *std::find(legalMoves.begin(), legalMoves.end(), pvMove);

        if (legalMoves[0].tbRank != rm.tbRank)
            break;

        ply++;

        auto& st = sts.emplace_back();
        pos.do_move(pvMove, st);

        // Do not allow for repetitions or drawing moves along the PV in TB regime
        if (config.rootInTB && ((rule50 && pos.is_draw(ply)) || pos.is_repetition(ply)))
        {
            pos.undo_move(pvMove);
            ply--;
            break;
        }

        // Full PV shown will thus be validated and end in TB.
        // If we cannot validate the full PV in time, we do not show it.
        if (config.rootInTB && time_abort())
            break;
    }

    // Resize the PV to the correct part
    rootMove.pv.resize(ply);

    // Step 2, now extend the PV to mate, as if the user explored syzygy-tables.info
    // using top ranked moves (minimal DTZ), which gives optimal mates only for simple
    // endgames e.g. KRvK.
    while (!(rule50 && pos.is_draw(0)))
    {
        if (time_abort())
            break;

        RootMoves legalMoves;
        for (const auto& m : MoveList<LEGAL>(pos))
        {
            auto&     rm = legalMoves.emplace_back(m);
            StateInfo tmpSI;
            pos.do_move(m, tmpSI);
            // Give a score of each move to break DTZ ties restricting opponent mobility,
            // but not giving the opponent a capture.
            for (const auto& mOpp : MoveList<LEGAL>(pos))
                rm.tbRank -= pos.capture(mOpp) ? 100 : 1;
            pos.undo_move(m);
        }

        // Mate found
        if (legalMoves.size() == 0)
            break;

        // Sort moves according to their above assigned rank.
        // This will break ties for moves with equal DTZ in rank_root_moves.
        std::stable_sort(
          legalMoves.begin(), legalMoves.end(),
          [](const Search::RootMove& a, const Search::RootMove& b) { return a.tbRank > b.tbRank; });

        // The winning side tries to minimize DTZ, the losing side maximizes it
        TB::Config config = TB::rank_root_moves(options, pos, legalMoves, true, time_abort);

        // If DTZ is not available we might not find a mate, so we bail out
        if (!config.rootInTB || config.cardinality > 0)
            break;

        ply++;

        Move& pvMove = legalMoves[0].pv[0];
        rootMove.pv.push_back(pvMove);
        auto& st = sts.emplace_back();
        pos.do_move(pvMove, st);
    }

    // Finding a draw in this function is an exceptional case, that cannot happen when rule50 is false or
    // during engine game play, since we have a winning score, and play correctly
    // with TB support. However, it can be that a position is draw due to the 50 move
    // rule if it has been reached on the board with a non-optimal 50 move counter
    // (e.g. 8/8/6k1/3B4/3K4/4N3/8/8 w - - 54 106 ) which TB with dtz counter rounding
    // cannot always correctly rank. See also
    // https://github.com/official-veloct/VeloCT/issues/5175#issuecomment-2058893495
    // We adjust the score to match the found PV. Note that a TB loss score can be
    // displayed if the engine did not find a drawing move yet, but eventually search
    // will figure it out (e.g. 1kq5/q2r4/5K2/8/8/8/8/7Q w - - 96 1 )
    if (pos.is_draw(0))
        v = VALUE_DRAW;

    // Undo the PV moves
    for (usize i = rootMove.pv.size(); i > 0; --i)
        pos.undo_move(rootMove.pv[i - 1]);

    // Inform if we couldn't get a full extension in time
    if (time_abort())
        sync_cout
          << "info string Syzygy based PV extension requires more time, increase Move Overhead as needed."
          << sync_endl;
}

void SearchManager::output_pv(Search::Worker&           worker,
                              const ThreadPool&         threads,
                              const TranspositionTable& tt,
                              Depth                     depth) {

    const auto nodes     = threads.nodes_searched();
    auto&      rootMoves = worker.rootMoves;
    auto&      pos       = worker.rootPos;
    usize      multiPV   = std::min(usize(worker.options["MultiPV"]), rootMoves.size());
    u64        tbHits    = threads.tb_hits() + (worker.tbConfig.rootInTB ? rootMoves.size() : 0);

    for (usize i = 0; i < multiPV; ++i)
    {
        bool usePreviousScore = rootMoves[i].score == -VALUE_INFINITE;

        if (depth == 1 && usePreviousScore && i > 0)
            continue;

        Depth d = usePreviousScore ? std::max(1, depth - 1) : depth;
        Value v = usePreviousScore ? rootMoves[i].previousScore : rootMoves[i].uciScore;

        if (v == -VALUE_INFINITE)
            v = VALUE_ZERO;

        bool isTBScore = worker.tbConfig.rootInTB && !is_mate_or_mated(v);
        v              = isTBScore ? rootMoves[i].tbScore : v;

        // Potentially correct and extend the PV, and in exceptional cases v.
        // Previous PVs have already been extended. Bound flags indicate an unreliable PV.
        if (is_decisive(v) && !is_mate_or_mated(v) && !usePreviousScore
            && (!rootMoves[i].score_is_bound() || isTBScore))
            syzygy_extend_pv(worker.options, worker.limits, pos, rootMoves[i], v);

        std::string pv;
        for (Move m : usePreviousScore ? rootMoves[i].previousPV : rootMoves[i].pv)
            pv += UCIEngine::move(m, pos.is_chess960()) + " ";

        // Remove last whitespace
        if (!pv.empty())
            pv.pop_back();

        auto wdl   = worker.options["UCI_ShowWDL"] ? UCIEngine::wdl(v, pos) : "";
        auto bound = rootMoves[i].scoreLowerbound
                     ? "lowerbound"
                     : (rootMoves[i].scoreUpperbound ? "upperbound" : "");

        InfoFull info;

        info.depth    = d;
        info.selDepth = rootMoves[i].selDepth;
        info.multiPV  = i + 1;
        info.score    = {v, pos};
        info.wdl      = wdl;

        // TB and previous scores are exact, even though their bound flags may say otherwise.
        if (!(isTBScore || usePreviousScore))
            info.bound = bound;

        TimePoint time = std::max(TimePoint(1), tm.elapsed_time());
        info.timeMs    = time;
        info.nodes     = nodes;
        info.nps       = nodes * 1000 / time;
        info.tbHits    = tbHits;
        info.pv        = pv;
        info.hashfull  = tt.hashfull();

        updates.onUpdateFull(info);
    }
}

// Called in case we have no ponder move before exiting the search,
// for instance, in case we stop the search during a fail high at root.
// We try hard to have a ponder move to return to the GUI,
// otherwise in case of 'ponder on' we have nothing to think about.
bool RootMove::extract_ponder_from_tt(const TranspositionTable& tt, Position& pos) {

    assert(pv.size() == 1 && pv[0] != Move::none());

    StateInfo st;
    pos.do_move(pv[0], st, &tt);

    if (!pos.is_draw(1))
    {
        auto [ttHit, ttData, ttWriter] = tt.probe(pos.key());
        if (ttHit && MoveList<LEGAL>(pos).contains(ttData.move))
            pv.push_back(ttData.move);
    }

    pos.undo_move(pv[0]);
    return pv.size() > 1;
}


}  // namespace VeloCT

// ==== END OF FILE: search.cpp ====

// ==== START OF FILE: thread.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "thread.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "bitboard.h"
#include "history.h"
#include "memory.h"
#include "movegen.h"
#include "search.h"
#include "syzygy/tbprobe.h"
#include "timeman.h"
#include "types.h"
#include "uci.h"
#include "ucioption.h"

namespace VeloCT {

// Constructor launches the thread and waits until it goes to sleep
// in idle_loop(). Note that 'searching' and 'exit' should be already set.
Thread::Thread(Search::SharedState&                    sharedState,
               std::unique_ptr<Search::ISearchManager> sm,
               usize                                   n,
               usize                                   numaN,
               usize                                   totalNumaCount,
               OptionalThreadToNumaNodeBinder          binder) :
    idx(n),
    idxInNuma(numaN),
    totalNuma(totalNumaCount),
    nthreads(sharedState.options["Threads"]),
    stdThread(&Thread::idle_loop, this) {

    wait_for_search_finished();

    run_custom_job([this, &binder, &sharedState, &sm, n]() {
        // Use the binder to [maybe] bind the threads to a NUMA node before doing
        // the Worker allocation. Ideally we would also allocate the SearchManager
        // here, but that's minor.
        this->numaAccessToken = binder();
        this->worker          = make_unique_large_page<Search::Worker>(
          sharedState, std::move(sm), n, idxInNuma, totalNuma, this->numaAccessToken);
    });

    wait_for_search_finished();
}


// Destructor wakes up the thread in idle_loop() and waits
// for its termination. Thread should be already waiting.
Thread::~Thread() {

    assert(!searching);

    exit = true;
    start_searching();
    stdThread.join();
}

// Wakes up the thread that will start the search
void Thread::start_searching() {
    assert(worker != nullptr);
    run_custom_job([this]() { worker->start_searching(); });
}

// Clears the histories for the thread worker (usually before a new game)
void Thread::clear_worker() {
    assert(worker != nullptr);
    run_custom_job([this]() { worker->clear(); });
}

// Blocks on the condition variable until the thread has finished searching
void Thread::wait_for_search_finished() {

    std::unique_lock<std::mutex> lk(mutex);
    cv.wait(lk, [&] { return !searching; });
}

// Launching a function in the thread
void Thread::run_custom_job(std::function<void()> f) {
    {
        std::unique_lock<std::mutex> lk(mutex);
        cv.wait(lk, [&] { return !searching; });
        jobFunc   = std::move(f);
        searching = true;
    }
    cv.notify_one();
}

void Thread::ensure_network_replicated() { worker->ensure_network_replicated(); }

// Thread gets parked here, blocked on the condition variable
// when the thread has no work to do.

void Thread::idle_loop() {
    while (true)
    {
        std::unique_lock<std::mutex> lk(mutex);
        searching = false;
        cv.notify_one();  // Wake up anyone waiting for search finished
        cv.wait(lk, [&] { return searching; });

        if (exit)
            return;

        std::function<void()> job = std::move(jobFunc);
        jobFunc                   = nullptr;

        lk.unlock();

        if (job)
            job();
    }
}

Search::SearchManager* ThreadPool::main_manager() { return main_thread()->worker->main_manager(); }

u64 ThreadPool::nodes_searched() const { return accumulate(&Search::Worker::nodes); }
u64 ThreadPool::tb_hits() const { return accumulate(&Search::Worker::tbHits); }

static usize next_power_of_two(u64 count) { return count > 1 ? (2ULL << msb(count - 1)) : 1; }

// Creates/destroys threads to match the requested number.
// Created and launched threads will immediately go to sleep in idle_loop.
// Upon resizing, threads are recreated to allow for binding if necessary.
void ThreadPool::set(const NumaConfig&                           numaConfig,
                     Search::SharedState                         sharedState,
                     const Search::SearchManager::UpdateContext& updateContext) {

    if (threads.size() > 0)  // destroy any existing thread(s)
    {
        main_thread()->wait_for_search_finished();

        threads.clear();

        boundThreadToNumaNode.clear();
    }

    const usize requested = sharedState.options["Threads"];

    if (requested > 0)  // create new thread(s)
    {
        // Binding threads may be problematic when there's multiple NUMA nodes and
        // multiple VeloCT instances running. In particular, if each instance
        // runs a single thread then they would all be mapped to the first NUMA node.
        // This is undesirable, and so the default behaviour (i.e. when the user does not
        // change the NumaConfig UCI setting) is to not bind the threads to processors
        // unless we know for sure that we span NUMA nodes and replication is required.
        const std::string numaPolicy(sharedState.options["NumaPolicy"]);
        const bool        doBindThreads = [&]() {
            if (numaPolicy == "none")
                return false;

            if (numaPolicy == "auto")
                return numaConfig.suggests_binding_threads(requested);

            // numaPolicy == "system", or explicitly set by the user
            return true;
        }();

        std::map<NumaIndex, usize> counts;
        boundThreadToNumaNode = doBindThreads
                                ? numaConfig.distribute_threads_among_numa_nodes(requested)
                                : std::vector<NumaIndex>{};

        if (boundThreadToNumaNode.empty())
            counts[0] = requested;  // Pretend all threads are part of numa node 0
        else
        {
            for (usize i = 0; i < boundThreadToNumaNode.size(); ++i)
                counts[boundThreadToNumaNode[i]]++;
        }

        sharedState.sharedHistories.clear();
        for (auto pair : counts)
        {
            NumaIndex numaIndex = pair.first;
            u64       count     = pair.second;
            auto      f         = [&]() {
                sharedState.sharedHistories.try_emplace(numaIndex, next_power_of_two(count));
            };
            if (doBindThreads)
                numaConfig.execute_on_numa_node(numaIndex, f);
            else
                f();
        }

        auto threadsPerNode = counts;
        counts.clear();

        while (threads.size() < requested)
        {
            const usize     threadId      = threads.size();
            const NumaIndex numaId        = doBindThreads ? boundThreadToNumaNode[threadId] : 0;
            auto            create_thread = [&]() {
                auto manager = threadId == 0
                                          ? std::unique_ptr<Search::ISearchManager>(
                                   std::make_unique<Search::SearchManager>(updateContext))
                                          : std::make_unique<Search::NullSearchManager>();

                // When not binding threads we want to force all access to happen
                // from the same NUMA node, because in case of NUMA replicated memory
                // accesses we don't want to trash cache in case the threads get scheduled
                // on the same NUMA node.
                auto binder = doBindThreads ? OptionalThreadToNumaNodeBinder(numaConfig, numaId)
                                                       : OptionalThreadToNumaNodeBinder(numaId);

                threads.emplace_back(std::make_unique<Thread>(sharedState, std::move(manager),
                                                                         threadId, counts[numaId]++,
                                                                         threadsPerNode[numaId], binder));
            };

            // Ensure the worker thread inherits the intended NUMA affinity at creation.
            if (doBindThreads)
                numaConfig.execute_on_numa_node(numaId, create_thread);
            else
                create_thread();
        }

        clear();

        main_thread()->wait_for_search_finished();
    }
}


// Sets threadPool data to initial values
void ThreadPool::clear() {
    if (threads.size() == 0)
        return;

    for (auto&& th : threads)
        th->clear_worker();

    for (auto&& th : threads)
        th->wait_for_search_finished();

    // These two affect the time taken on the first move of a game:
    main_manager()->bestPreviousAverageScore = VALUE_INFINITE;
    main_manager()->previousTimeReduction    = 0.85;

    main_manager()->callsCnt           = 0;
    main_manager()->bestPreviousScore  = VALUE_INFINITE;
    main_manager()->originalTimeAdjust = -1;
    main_manager()->tm.clear();
}

void ThreadPool::run_on_thread(usize threadId, std::function<void()> f) {
    assert(threads.size() > threadId);
    threads[threadId]->run_custom_job(std::move(f));
}

void ThreadPool::wait_on_thread(usize threadId) {
    assert(threads.size() > threadId);
    threads[threadId]->wait_for_search_finished();
}

usize ThreadPool::num_threads() const { return threads.size(); }


// Wakes up main thread waiting in idle_loop() and returns immediately.
// Main thread will wake up other threads and start the search.
void ThreadPool::start_thinking(const OptionsMap&  options,
                                Position&          pos,
                                StateListPtr&      states,
                                Search::LimitsType limits) {

    main_thread()->wait_for_search_finished();

    main_manager()->stopOnPonderhit = stop = false;
    main_manager()->ponder                 = limits.ponderMode;

    increaseDepth = true;

    Search::RootMoves rootMoves;
    const auto        legalmoves = MoveList<LEGAL>(pos);

    for (const auto& uciMove : limits.searchmoves)
    {
        auto move = UCIEngine::to_move(pos, uciMove);

        if (std::find(legalmoves.begin(), legalmoves.end(), move) != legalmoves.end())
            rootMoves.emplace_back(move);
    }

    if (rootMoves.empty())
        for (const auto& m : legalmoves)
            rootMoves.emplace_back(m);

    Tablebases::Config tbConfig = Tablebases::rank_root_moves(options, pos, rootMoves);

    // After ownership transfer 'states' becomes empty, so if we stop the search
    // and call 'go' again without setting a new position states.get() == nullptr.
    assert(states.get() || setupStates.get());

    if (states.get())
        setupStates = std::move(states);  // Ownership transfer, states is now empty

    // We use Position::set() to set root position across threads. But there are
    // some StateInfo fields (previous, pliesFromNull, capturedPiece) that cannot
    // be deduced from a fen string, so set() clears them and they are set from
    // setupStates->back() later. The rootState is per thread, earlier states are
    // shared since they are read-only.
    for (auto&& th : threads)
    {
        th->run_custom_job([&]() {
            th->worker->limits = limits;
            th->worker->nodes = th->worker->tbHits = th->worker->bestMoveChanges = 0;
            th->worker->nmpMinPly                                                = 0;
            th->worker->rootDepth                                                = 0;
            th->worker->rootMoves                                                = rootMoves;
            th->worker->rootPos.set(pos.fen(), pos.is_chess960(), &th->worker->rootState);
            th->worker->rootState = setupStates->back();
            th->worker->tbConfig  = tbConfig;
        });
    }

    for (auto&& th : threads)
        th->wait_for_search_finished();

    main_thread()->start_searching();
}

Thread* ThreadPool::get_best_thread() const {

    Thread* bestThread = threads.front().get();
    Value   minScore   = VALUE_INFINITE;

    std::unordered_map<Move, i64, Move::MoveHash> votes(
      2 * std::min(size(), bestThread->worker->rootMoves.size()));

    for (auto&& th : threads)
        minScore = std::min(minScore, th->worker->rootMoves[0].score);

    // Vote according to score, and select the best thread
    for (auto&& th : threads)
        votes[th->worker->rootMoves[0].pv[0]] += th->worker->rootMoves[0].score - minScore + 14;

    for (auto&& th : threads)
    {
        const auto& bestThreadMove = bestThread->worker->rootMoves[0];
        const auto& newThreadMove  = th->worker->rootMoves[0];

        const auto bestThreadMoveVote = votes[bestThreadMove.pv[0]];
        const auto newThreadMoveVote  = votes[newThreadMove.pv[0]];

        // Aborted (d1) searches may lead to inexact win (or loss) scores.
        const bool bestThreadDecisive = bestThreadMove.score != -VALUE_INFINITE
                                     && is_decisive(bestThreadMove.score)
                                     && !bestThreadMove.score_is_bound();
        const bool newThreadDecisive = newThreadMove.score != -VALUE_INFINITE
                                    && is_decisive(newThreadMove.score)
                                    && !newThreadMove.score_is_bound();

        if (bestThreadDecisive)
        {
            // Make sure we pick the shortest mate / TB conversion.
            if (newThreadDecisive && std::abs(newThreadMove.score) > std::abs(bestThreadMove.score))
            {
                assert((is_win(bestThreadMove.score) && is_win(newThreadMove.score))
                       || (is_loss(bestThreadMove.score) && is_loss(newThreadMove.score)));

                bestThread = th.get();
            }
        }
        else if (newThreadDecisive
                 || (!is_loss(newThreadMove.score)
                     && (newThreadMoveVote > bestThreadMoveVote
                         || (newThreadMoveVote == bestThreadMoveVote
                             && newThreadMove.pv.size() > bestThreadMove.pv.size()))))
            bestThread = th.get();
    }

    return bestThread;
}


// Start non-main threads.
// Will be invoked by main thread after it has started searching.
void ThreadPool::start_searching() {

    for (auto&& th : threads)
        if (th != threads.front())
            th->start_searching();
}


// Wait for non-main threads
void ThreadPool::wait_for_search_finished() const {

    for (auto&& th : threads)
        if (th != threads.front())
            th->wait_for_search_finished();
}

std::vector<usize> ThreadPool::get_bound_thread_to_numa_node() const {
    return boundThreadToNumaNode;
}

std::vector<usize> ThreadPool::get_bound_thread_count_by_numa_node() const {
    std::vector<usize> counts;

    if (!boundThreadToNumaNode.empty())
    {
        NumaIndex highestNumaNode = 0;
        for (NumaIndex n : boundThreadToNumaNode)
            if (n > highestNumaNode)
                highestNumaNode = n;

        counts.resize(highestNumaNode + 1, 0);

        for (NumaIndex n : boundThreadToNumaNode)
            counts[n] += 1;
    }

    return counts;
}

usize ThreadPool::numa_nodes() const {
    std::unordered_set<usize> seen;
    for (NumaIndex n : boundThreadToNumaNode)
        seen.insert(n);
    return std::max(seen.size(), usize(1));
}

void ThreadPool::ensure_network_replicated() {
    for (auto&& th : threads)
        th->ensure_network_replicated();
}

}  // namespace VeloCT

// ==== END OF FILE: thread.cpp ====

// ==== START OF FILE: timeman.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "timeman.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "search.h"
#include "ucioption.h"

namespace VeloCT {

TimePoint TimeManagement::optimum() const { return optimumTime; }
TimePoint TimeManagement::maximum() const { return maximumTime; }

void TimeManagement::clear() {
    availableNodes = -1;  // When in 'nodes as time' mode
}

void TimeManagement::advance_nodes_time(i64 nodes) {
    assert(useNodesTime);
    availableNodes = std::max(i64(0), availableNodes - nodes);
}

// Called at the beginning of the search and calculates
// the bounds of time allowed for the current game ply. We currently support:
//      1) x basetime (+ z increment)
//      2) x moves in y seconds (+ z increment)
void TimeManagement::init(Search::LimitsType& limits,
                          Color               us,
                          int                 ply,
                          const OptionsMap&   options,
                          double&             originalTimeAdjust) {
    TimePoint npmsec = TimePoint(options["nodestime"]);

    // If we have no time, we don't need to fully initialize TM.
    // startTime is used by movetime and useNodesTime is used in elapsed calls.
    startTime    = limits.startTime;
    useNodesTime = npmsec != 0;

    if (limits.time[us] == 0)
        return;

    TimePoint moveOverhead = TimePoint(options["Move Overhead"]);

    // optScale is a percentage of available time to use for the current move.
    // maxScale is a multiplier applied to optimumTime.
    double optScale, maxScale;

    // If we have to play in 'nodes as time' mode, then convert from time
    // to nodes, and use resulting values in time management formulas.
    // WARNING: to avoid time losses, the given npmsec (nodes per millisecond)
    // must be much lower than the real engine speed.
    if (useNodesTime)
    {
        if (availableNodes == -1)                       // Only once at game start
            availableNodes = npmsec * limits.time[us];  // Time is in msec

        // Convert from milliseconds to nodes
        limits.time[us] = TimePoint(availableNodes);
        limits.inc[us] *= npmsec;
        limits.npmsec = npmsec;
        moveOverhead *= npmsec;
    }

    // These numbers are used where multiplications, divisions,
    // or comparisons with constants are involved.
    const i64       scaleFactor = useNodesTime ? npmsec : 1;
    const TimePoint scaledTime  = limits.time[us] / scaleFactor;

    // Maximum move horizon
    int mtg = limits.movestogo ? std::min(limits.movestogo, 50) : 50;

    // If less than one second, gradually reduce mtg
    if (scaledTime < 1000)
        mtg = int(scaledTime * 0.05);

    // Make sure timeLeft is > 0 since we may use it as a divisor
    TimePoint timeLeft = std::max(TimePoint(1), limits.time[us] + limits.inc[us] * (mtg - 1)
                                                  - moveOverhead * (2 + mtg));

    // x basetime (+ z increment)
    // If there is a healthy increment, timeLeft can exceed the actual available
    // game time for the current move, so also cap to a percentage of available game time.
    if (limits.movestogo == 0)
    {
        // Extra time according to timeLeft
        if (originalTimeAdjust < 0)
            originalTimeAdjust = 0.3272 * std::log10(timeLeft) - 0.4141;

        // Calculate time constants based on current time left.
        double logTimeInSec = std::log10(scaledTime / 1000.0);
        double optConstant  = std::min(0.0029869 + 0.00033554 * logTimeInSec, 0.004905);
        double maxConstant  = std::max(3.3744 + 3.0608 * logTimeInSec, 3.1441);

        optScale = std::min(0.012112 + std::pow(ply + 3.22713, 0.46866) * optConstant,
                            0.19404 * limits.time[us] / timeLeft)
                 * originalTimeAdjust;

        maxScale = std::min(6.873, maxConstant + ply / 12.352);
    }

    // x moves in y seconds (+ z increment)
    else
    {
        optScale = std::min((0.88 + ply / 116.4) / mtg, 0.88 * limits.time[us] / timeLeft);
        maxScale = 1.3 + 0.11 * mtg;
    }

    // Limit the maximum possible time for this move
    optimumTime = TimePoint(std::max(1.0, optScale * timeLeft));
    maximumTime =
      TimePoint(std::max(double(optimumTime), std::min(0.8097 * limits.time[us] - moveOverhead,
                                                       maxScale * optimumTime)));

    if (options["Ponder"])
        optimumTime += optimumTime / 4;
}

}  // namespace VeloCT

// ==== END OF FILE: timeman.cpp ====

// ==== START OF FILE: tt.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "tt.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <numeric>
#include <vector>

#include "memory.h"
#include "misc.h"
#include "syzygy/tbprobe.h"
#include "thread.h"

namespace VeloCT {


// TTEntry struct is the 10 bytes transposition table entry, defined as:
//
// key        16 bit
// depth       8 bit
// pv node     1 bit
// bound type  2 bit
// generation  5 bit
// move       16 bit
// value      16 bit
// evaluation 16 bit
//
// These fields are in the same order as accessed by TT::probe(), since memory is fastest sequentially.
// Equally, the store order in save() matches this order.
//
// We use `bool(depth8)` as the cheap internal occupancy check, corresponding to `depth == DEPTH_NONE`
// externally, so we offset the internal depth by DEPTH_NONE.
//
// Pv, bound and generation are packed in a single byte.
static constexpr u8 GENERATION_BITS = 5;
static constexpr u8 GENERATION_MASK = (1 << GENERATION_BITS) - 1;
static constexpr u8 BOUND_SHIFT     = GENERATION_BITS;
static constexpr u8 BOUND_MASK      = 0b11 << BOUND_SHIFT;
static constexpr u8 PV_SHIFT        = BOUND_SHIFT + 2;
static constexpr u8 PV_MASK         = 1 << PV_SHIFT;

struct TTEntry {

    // Convert internal bitfields to external types
    TTData read() const {
        return TTData{Move(move16),
                      Value(value16),
                      Value(eval16),
                      Depth(DEPTH_NONE + depth8),
                      Bound((genBound8 & BOUND_MASK) >> BOUND_SHIFT),
                      bool(genBound8 & PV_MASK)};
    }

    bool is_occupied() const { return bool(depth8); };
    void save(Key k, Value v, bool pv, Bound b, Depth d, Move m, Value ev, u8 curr_generation);
    u8   relative_age(const u8 curr_generation) const;

   private:
    friend class TranspositionTable;
    friend struct TTWriter;

    RelaxedAtomic<u16>  key16;
    RelaxedAtomic<u8>   depth8;
    RelaxedAtomic<u8>   genBound8;
    RelaxedAtomic<Move> move16;
    RelaxedAtomic<i16>  value16;
    RelaxedAtomic<i16>  eval16;
};

// Populates the TTEntry with a new node's data, possibly
// overwriting an old position. The update is non-atomic and can be racy.
void TTEntry::save(
  Key k, Value v, bool pv, Bound b, Depth d, Move m, Value ev, u8 curr_generation) {

    // Preserve the old ttmove if we don't have a new one
    if (m || u16(k) != key16)
        move16 = m;

    // Overwrite less valuable entries (cheapest checks first)
    if (b == BOUND_EXACT || u16(k) != key16 || d - DEPTH_NONE + 2 * pv > depth8 - 4
        || relative_age(curr_generation))
    {
        assert(d > DEPTH_NONE);
        assert(d - DEPTH_NONE < 256);
        assert(curr_generation <= GENERATION_MASK);  // TT::new_search() plays nice

        key16     = u16(k);
        depth8    = u8(d - DEPTH_NONE);
        genBound8 = u8(curr_generation | b << BOUND_SHIFT | u8(pv) << PV_SHIFT);
        value16   = i16(v);
        eval16    = i16(ev);
    }
    // Secondary aging. Important for elementary mate finding.
    // (*Scaler) Secondary aging on entries relevant to singular extensions
    // generally scales poorly and requires VVLTC verification.
    else if (depth8 + DEPTH_NONE >= 5
             && Bound((genBound8 & BOUND_MASK) >> BOUND_SHIFT) != BOUND_EXACT)
    {
        auto v16 = value16;
        if (std::abs(v16) < VALUE_INFINITE && is_decisive(v16))
            depth8 = std::max(int(depth8) - 1,
                              0);  // guard against racy underflows, default to "unoccupied"
    }
}


u8 TTEntry::relative_age(const u8 curr_generation) const {
    // Returns this entry's age. We count generations like clocks count hours,
    // i.e. we require 0 - 1 == 31. Unsigned subtraction guarantees the required
    // borrowing regardless of the upper pv/bound bits.
    return (curr_generation - genBound8) & GENERATION_MASK;
}


// TTWriter is but a very thin wrapper around the pointer
TTWriter::TTWriter(TTEntry* tte) :
    entry(tte) {}

void TTWriter::write(
  Key k, Value v, bool pv, Bound b, Depth d, Move m, Value ev, u8 curr_generation) {
    entry->save(k, v, pv, b, d, m, ev, curr_generation);
}

void TTWriter::penalize(int penalty) {
    // guard against racy underflows, default to "unoccupied"
    entry->depth8 = std::max(int(entry->depth8) - penalty, 0);
}


// A TranspositionTable is an array of Cluster, of size clusterCount. Each cluster consists of ClusterSize number
// of TTEntry. Each non-empty TTEntry contains information on exactly one position. The size of a Cluster should
// divide the size of a cache line for best performance, as the cacheline is prefetched when possible.

static constexpr int ClusterSize = 3;

struct Cluster {
    TTEntry entry[ClusterSize];
    char    padding[2];  // Pad to 32 bytes
};

static_assert(sizeof(Cluster) == 32, "Suboptimal Cluster size");


// Sets the size of the transposition table,
// measured in megabytes. Transposition table consists
// of clusters and each cluster consists of ClusterSize number of TTEntry.
void TranspositionTable::resize(usize mbSize, ThreadPool& threads) {
    aligned_large_pages_free(table);

    clusterCount  = mbSize * 1024 * 1024 / sizeof(Cluster);
    usize ttBytes = clusterCount * sizeof(Cluster);

    // Request 1GB pages if we'd get at least eight per NUMA node, to avoid
    // memory oversubscription
    bool hugePageHint = ttBytes >= threads.numa_nodes() * HugePageSize * 8;

    table = static_cast<Cluster*>(aligned_large_pages_alloc_with_hint(ttBytes, hugePageHint));

    if (!table)
    {
        std::cerr << "Failed to allocate " << mbSize << "MB for transposition table." << std::endl;
        exit(EXIT_FAILURE);
    }

    clear(threads);
}


// Initializes the entire transposition table to zero,
// in a multi-threaded way.
void TranspositionTable::clear(ThreadPool& threads) {
    generation8             = 0;
    const usize threadCount = threads.num_threads();

    std::vector<usize> threadToNuma = threads.get_bound_thread_to_numa_node();

    std::vector<usize> order(threadCount);
    std::iota(order.begin(), order.end(), 0);

    // To promote good NUMA distribution (esp. with huge pages), we permute threads so that
    // all threads in a NUMA node clear a contiguous region of the TT.
    if (threadToNuma.size() == threadCount)
    {
        std::stable_sort(order.begin(), order.end(), [&threadToNuma](usize t1, usize t2) {
            return threadToNuma.at(t1) < threadToNuma.at(t2);
        });
    }

    for (usize i = 0; i < threadCount; ++i)
    {
        threads.run_on_thread(order[i], [this, i, threadCount]() {
            // Each thread will zero its part of the hash table
            const usize stride = clusterCount / threadCount;
            const usize start  = stride * i;
            const usize len    = i + 1 != threadCount ? stride : clusterCount - start;

            std::memset(static_cast<void*>(&table[start]), 0, len * sizeof(Cluster));
        });
    }

    for (usize i = 0; i < threadCount; ++i)
        threads.wait_on_thread(i);
}


// Returns an approximation of the hashtable
// occupation during a search. The hash is x permill full, as per UCI protocol.
// Only counts entries which are younger than maxAge.
int TranspositionTable::hashfull(int maxAge) const {
    int cnt = 0;
    for (int i = 0; i < 1000; ++i)
        for (int j = 0; j < ClusterSize; ++j)
            cnt += table[i].entry[j].is_occupied()
                && table[i].entry[j].relative_age(generation8) <= maxAge;

    return cnt / ClusterSize;
}


void TranspositionTable::new_search() {
    ++generation8;
    // Don't overflow into the other bits of TTEntry::genBound8
    generation8 &= GENERATION_MASK;
}


u8 TranspositionTable::generation() const { return generation8; }


// Looks up the current position in the transposition table.
// It returns true if the key is found (which may be a collision), and has non-null data.
// Otherwise, it returns false and a pointer to an empty or least valuable TTEntry
// to be replaced later. The value of an entry is its depth minus 8 times its relative age.
std::tuple<bool, TTData, TTWriter> TranspositionTable::probe(const Key key) const {

    TTEntry* const tte   = first_entry(key);
    const u16      key16 = u16(key);  // Use the low 16 bits as key inside the cluster

    for (int i = 0; i < ClusterSize; ++i)
        if (tte[i].key16 == key16)
            // This gap is the main place for read races.
            // After `read()` completes that copy is final, but may be self-inconsistent.
            return {tte[i].is_occupied(), tte[i].read(), TTWriter(&tte[i])};

    // Find an entry to be replaced according to the replacement strategy
    TTEntry* replace = tte;
    for (int i = 1; i < ClusterSize; ++i)
        if (replace->depth8 - 8 * replace->relative_age(generation8)
            > tte[i].depth8 - 8 * tte[i].relative_age(generation8))
            replace = &tte[i];

    return {false, TTData{Move::none(), VALUE_NONE, VALUE_NONE, DEPTH_NONE, BOUND_NONE, false},
            TTWriter(replace)};
}


TTEntry* TranspositionTable::first_entry(const Key key) const {
    return &table[mul_hi64(key, clusterCount)].entry[0];
}

}  // namespace VeloCT

// ==== END OF FILE: tt.cpp ====

// ==== START OF FILE: tune.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "tune.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>

#include "ucioption.h"

using std::string;

namespace VeloCT {

bool          Tune::update_on_last;
const Option* LastOption = nullptr;
OptionsMap*   Tune::options;
namespace {
std::map<std::string, int> TuneResults;

std::optional<std::string> on_tune(const Option& o) {

    if (!Tune::update_on_last || LastOption == &o)
        Tune::read_options();

    return std::nullopt;
}
}

void Tune::make_option(OptionsMap* opts, const string& n, int v, const SetRange& r) {

    // Do not generate option when there is nothing to tune (ie. min = max)
    if (r(v).first == r(v).second)
        return;

    if (TuneResults.count(n))
        v = TuneResults[n];

    opts->add(n, Option(v, r(v).first, r(v).second, on_tune));
    LastOption = &((*opts)[n]);

    // Print formatted parameters, ready to be copy-pasted in Fishtest
    std::cout << n << ","                                  //
              << v << ","                                  //
              << r(v).first << ","                         //
              << r(v).second << ","                        //
              << (r(v).second - r(v).first) / 20.0 << ","  //
              << "0.0020" << std::endl;
}

string Tune::next(string& names, bool pop) {

    string name;

    do
    {
        string token = names.substr(0, names.find(','));

        if (pop)
            names.erase(0, token.size() + 1);

        std::stringstream ws(token);
        name += (ws >> token, token);  // Remove trailing whitespace

    } while (std::count(name.begin(), name.end(), '(') - std::count(name.begin(), name.end(), ')'));

    return name;
}


template<>
void Tune::Entry<int>::init_option() {
    make_option(options, name, value, range);
}

template<>
void Tune::Entry<int>::read_option() {
    if (options->count(name))
        value = int((*options)[name]);
}

// Instead of a variable here we have a PostUpdate function: just call it
template<>
void Tune::Entry<Tune::PostUpdate>::init_option() {}
template<>
void Tune::Entry<Tune::PostUpdate>::read_option() {
    value();
}

}  // namespace VeloCT


// Init options with tuning session results instead of default values. Useful to
// get correct bench signature after a tuning session or to test tuned values.
// Just copy fishtest tuning results in a result.txt file and extract the
// values with:
//
// cat results.txt | sed 's/^param: \([^,]*\), best: \([^,]*\).*/  TuneResults["\1"] = int(round(\2));/'
//
// Then paste the output below, as the function body


namespace VeloCT {

void Tune::read_results() { /* ...insert your values here... */ }

}  // namespace VeloCT

// ==== END OF FILE: tune.cpp ====

// ==== START OF FILE: uci.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "uci.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <sstream>
#include <string_view>
#include <filesystem>
#include <utility>
#include <variant>
#include <vector>

#include "benchmark.h"
#include "engine.h"
#include "memory.h"
#include "movegen.h"
#include "position.h"
#include "score.h"
#include "search.h"
#include "types.h"
#include "ucioption.h"

namespace VeloCT {

constexpr auto BenchmarkCommand = "speedtest";

template<typename... Ts>
struct overload: Ts... {
    using Ts::operator()...;
};

template<typename... Ts>
overload(Ts...) -> overload<Ts...>;

void UCIEngine::print_info_string(std::string_view str) {
    sync_cout_start();
    for (auto& line : split(str, "\n"))
    {
        if (!is_whitespace(line))
        {
            std::cout << "info string " << line << '\n';
        }
    }
    sync_cout_end();
}

UCIEngine::UCIEngine(CommandLine cli_) :
    engine(cli_.argc > 0 ? std::optional{path_from_utf8(cli_.argv[0])} : std::nullopt),
    cli(std::move(cli_)) {

    engine.get_options().add_info_listener([](const std::optional<std::string>& str) {
        if (str.has_value())
            print_info_string(*str);
    });

    init_search_update_listeners();
}

void UCIEngine::init_search_update_listeners() {
    engine.set_on_iter([](const auto& i) { on_iter(i); });
    engine.set_on_update_no_moves([](const auto& i) { on_update_no_moves(i); });
    engine.set_on_update_full(
      [this](const auto& i) { on_update_full(i, engine.get_options()["UCI_ShowWDL"]); });
    engine.set_on_bestmove([](const auto& bm, const auto& p) { on_bestmove(bm, p); });
    engine.set_on_verify_network([](const auto& s) { print_info_string(s); });
}

void UCIEngine::loop() {
    set_console_utf8();
    std::string token, cmd;

    for (int i = 1; i < cli.argc; ++i)
        cmd += std::string(cli.argv[i]) + " ";

    do
    {
        if (cli.argc == 1
            && !getline(std::cin, cmd))  // Wait for an input or an end-of-file (EOF) indication
            cmd = "quit";

        currentCmd = cmd;
        std::istringstream is(cmd);

        token.clear();  // Avoid a stale if getline() returns nothing or a blank line
        is >> token;

        if (token == "quit" || token == "stop")
            engine.stop();

        // The GUI sends 'ponderhit' to tell that the user has played the expected move.
        // So, 'ponderhit' is sent if pondering was done on the same move that the user
        // has played. The search should continue, but should also switch from pondering
        // to the normal search.
        else if (token == "ponderhit")
            engine.set_ponderhit(false);

        else if (token == "uci")
        {
            sync_cout << "id name " << engine_info(true) << "\n"
                      << engine.get_options() << sync_endl;

            sync_cout << "uciok" << sync_endl;
        }

        else if (token == "setoption")
            setoption(is);
        else if (token == "go")
        {
            // send info strings after the go command is sent for old GUIs and python-chess
            print_info_string(engine.numa_config_information_as_string());
            print_info_string(engine.thread_allocation_information_as_string());
            go(is);
        }
        else if (token == "position")
            position(is);
        else if (token == "ucinewgame")
            engine.search_clear();
        else if (token == "isready")
            sync_cout << "readyok" << sync_endl;

        // Add custom non-UCI commands, mainly for debugging purposes.
        else if (token == "flip")
        {
            if (auto err = engine.flip())
            {
                terminate_on_critical_error(err->what());
            }
        }
        else if (token == "bench")
            bench(is);
        else if (token == BenchmarkCommand)
            benchmark(is);
        else if (token == "d")
            sync_cout << engine.visualize() << sync_endl;
        else if (token == "eval")
            engine.trace_eval();
        else if (token == "compiler")
            sync_cout << compiler_info() << sync_endl;
        else if (token == "export_net")
        {
            std::optional<std::filesystem::path> file;
            std::string                          filename;

            if (is >> filename)
                file = path_from_utf8(filename);

            engine.save_network(file);
        }
        else if (token == "--help" || token == "help" || token == "--license" || token == "license")
            sync_cout
              << "\nVeloCT is a powerful chess engine for playing and analyzing."
                 "\nIt is released as free software licensed under the GNU GPLv3 License."
                 "\nVeloCT is normally used with a graphical user interface (GUI) and implements"
                 "\nthe Universal Chess Interface (UCI) protocol to communicate with a GUI, an API, etc."
                 "\nFor any further information, visit https://github.com/official-veloct/VeloCT#readme"
                 "\nor read the corresponding README.md and Copying.txt files distributed along with this program.\n"
              << sync_endl;
        else if (!token.empty() && token[0] != '#')
            sync_cout << "Unknown command: '" << cmd << "'. Type help for more information."
                      << sync_endl;

    } while (token != "quit" && cli.argc <= 1);  // The command-line arguments are one-shot
}

Search::LimitsType UCIEngine::parse_limits(std::istream& is) {
    Search::LimitsType limits;
    std::string        token;

    limits.startTime = now();  // The search starts as early as possible

    while (is >> token)
    {
        if (token == "searchmoves")  // Needs to be the last command on the line
        {
            while (is >> token)
                limits.searchmoves.push_back(to_lower(token));
            break;
        }

        else if (token == "wtime")
            is >> limits.time[WHITE];
        else if (token == "btime")
            is >> limits.time[BLACK];
        else if (token == "winc")
            is >> limits.inc[WHITE];
        else if (token == "binc")
            is >> limits.inc[BLACK];
        else if (token == "movestogo")
            is >> limits.movestogo;
        else if (token == "depth")
            is >> limits.depth;
        else if (token == "nodes")
            is >> limits.nodes;
        else if (token == "movetime")
            is >> limits.movetime;
        else if (token == "mate")
            is >> limits.mate;
        else if (token == "perft")
            is >> limits.perft;
        else if (token == "infinite")
            limits.infinite = 1;
        else if (token == "ponder")
            limits.ponderMode = true;

        if (is.fail())
            terminate_on_critical_error("Invalid argument for '" + token + "'");
    }

    return limits;
}

void UCIEngine::go(std::istringstream& is) {

    Search::LimitsType limits = parse_limits(is);

    if (limits.perft)
        perft(limits);
    else
        engine.go(limits);
}

void UCIEngine::bench(std::istream& args) {
    std::string token;
    u64         num, nodes = 0, cnt = 1;
    u64         nodesSearched = 0;
    const auto& options       = engine.get_options();

    engine.set_on_update_full([&](const auto& i) {
        nodesSearched = i.nodes;
        on_update_full(i, options["UCI_ShowWDL"]);
    });

    std::vector<std::string> list = Benchmark::setup_bench(engine.fen(), args);

    num = count_if(list.begin(), list.end(),
                   [](const std::string& s) { return s.find("go ") == 0 || s.find("eval") == 0; });

    TimePoint elapsed = now();

    for (const auto& cmd : list)
    {
        std::istringstream is(cmd);
        is >> token;

        if (token == "go" || token == "eval")
        {
            std::cerr << "\nPosition: " << cnt++ << '/' << num << " (" << engine.fen() << ")"
                      << std::endl;
            if (token == "go")
            {
                Search::LimitsType limits = parse_limits(is);

                if (limits.perft)
                    nodesSearched = perft(limits);
                else
                {
                    engine.go(limits);
                    engine.wait_for_search_finished();
                }

                nodes += nodesSearched;
                nodesSearched = 0;
            }
            else
                engine.trace_eval();
        }
        else if (token == "setoption")
            setoption(is);
        else if (token == "position")
            position(is);
        else if (token == "ucinewgame")
        {
            engine.search_clear();  // search_clear may take a while
            elapsed = now();
        }
    }

    elapsed = now() - elapsed + 1;  // Ensure positivity to avoid a 'divide by zero'

    dbg_print();

    std::cerr << "\n==========================="    //
              << "\nTotal time (ms) : " << elapsed  //
              << "\nNodes searched  : " << nodes    //
              << "\nNodes/second    : " << 1000 * nodes / elapsed << std::endl;

    // reset callback, to not capture a dangling reference to nodesSearched
    engine.set_on_update_full([&](const auto& i) { on_update_full(i, options["UCI_ShowWDL"]); });
}

void UCIEngine::benchmark(std::istream& args) {
    // Probably not very important for a test this long, but include for completeness and sanity.
    static constexpr int NUM_WARMUP_POSITIONS = 3;

    std::string token;
    u64         nodes = 0, cnt = 1;
    u64         nodesSearched = 0;

    engine.set_on_update_full([&](const Engine::InfoFull& i) { nodesSearched = i.nodes; });

    engine.set_on_iter([](const auto&) {});
    engine.set_on_update_no_moves([](const auto&) {});
    engine.set_on_bestmove([](const auto&, const auto&) {});
    engine.set_on_verify_network([](const auto&) {});

    Benchmark::BenchmarkSetup setup = Benchmark::setup_benchmark(args);

    const auto numGoCommands = count_if(setup.commands.begin(), setup.commands.end(),
                                        [](const std::string& s) { return s.find("go ") == 0; });

    TimePoint totalTime = 0;

    // Set options once at the start.
    auto ss = std::istringstream("name Threads value " + std::to_string(setup.threads));
    setoption(ss);
    ss = std::istringstream("name Hash value " + std::to_string(setup.ttSize));
    setoption(ss);
    ss = std::istringstream("name UCI_Chess960 value false");
    setoption(ss);

    // Warmup
    for (const auto& cmd : setup.commands)
    {
        std::istringstream is(cmd);
        is >> token;

        if (token == "go")
        {
            // One new line is produced by the search, so omit it here
            std::cerr << "\rWarmup position " << cnt++ << '/' << NUM_WARMUP_POSITIONS;

            Search::LimitsType limits = parse_limits(is);

            // Run with silenced network verification
            engine.go(limits);
            engine.wait_for_search_finished();
        }
        else if (token == "position")
            position(is);
        else if (token == "ucinewgame")
        {
            engine.search_clear();  // search_clear may take a while
        }

        if (cnt > NUM_WARMUP_POSITIONS)
            break;
    }

    std::cerr << "\n";

    cnt   = 1;
    nodes = 0;

    int           numHashfullReadings = 0;
    constexpr int hashfullAges[]      = {0, 999};  // Only normal hashfull and touched hash.
    constexpr int hashfullAgeCount    = std::size(hashfullAges);
    int           totalHashfull[hashfullAgeCount] = {0};
    int           maxHashfull[hashfullAgeCount]   = {0};

    auto updateHashfullReadings = [&]() {
        numHashfullReadings += 1;

        for (int i = 0; i < hashfullAgeCount; ++i)
        {
            const int hashfull = engine.get_hashfull(hashfullAges[i]);
            maxHashfull[i]     = std::max(maxHashfull[i], hashfull);
            totalHashfull[i] += hashfull;
        }
    };

    engine.search_clear();  // search_clear may take a while

    for (const auto& cmd : setup.commands)
    {
        std::istringstream is(cmd);
        is >> token;

        if (token == "go")
        {
            // One new line is produced by the search, so omit it here
            std::cerr << "\rPosition " << cnt++ << '/' << numGoCommands;

            Search::LimitsType limits = parse_limits(is);

            nodesSearched     = 0;
            TimePoint elapsed = now();

            // Run with silenced network verification
            engine.go(limits);
            engine.wait_for_search_finished();

            totalTime += now() - elapsed;

            updateHashfullReadings();

            nodes += nodesSearched;
        }
        else if (token == "position")
            position(is);
        else if (token == "ucinewgame")
        {
            engine.search_clear();  // search_clear may take a while
        }
    }

    totalTime = std::max<TimePoint>(totalTime, 1);  // Ensure positivity to avoid a 'divide by zero'

    dbg_print();

    std::cerr << "\n";

    static_assert(
      std::size(hashfullAges) == 2 && hashfullAges[0] == 0 && hashfullAges[1] == 999,
      "Hardcoded for display. Would complicate the code needlessly in the current state.");

    std::string threadBinding = engine.thread_binding_information_as_string();
    if (threadBinding.empty())
        threadBinding = "none";

    // clang-format off

    std::cerr << "==========================="
              << "\nVersion                    : "
              << engine_version_info()
              // "\nCompiled by                : "
              << compiler_info()
              << "Large pages                : " << (has_large_pages() ? "yes" : "no")
              << "\nUser invocation            : " << BenchmarkCommand << " "
              << setup.originalInvocation << "\nFilled invocation          : " << BenchmarkCommand
              << " " << setup.filledInvocation
              << "\nAvailable processors       : " << engine.get_numa_config_as_string()
              << "\nThread count               : " << setup.threads
              << "\nThread binding             : " << threadBinding
              << "\nTT size [MiB]              : " << setup.ttSize
              << "\nHash max, avg [per mille]  : "
              << "\n    single search          : " << maxHashfull[0] << ", "
              << totalHashfull[0] / numHashfullReadings
              << "\n    single game            : " << maxHashfull[1] << ", "
              << totalHashfull[1] / numHashfullReadings
              << "\nTotal nodes searched       : " << nodes
              << "\nTotal search time [s]      : " << totalTime / 1000.0
              << "\nNodes/second               : " << 1000 * nodes / totalTime << std::endl;

    // clang-format on

    init_search_update_listeners();
}

void UCIEngine::setoption(std::istringstream& is) {
    engine.wait_for_search_finished();
    engine.get_options().setoption(is);
}

u64 UCIEngine::perft(const Search::LimitsType& limits) {
    auto result = engine.perft(engine.fen(), limits.perft, engine.get_options()["UCI_Chess960"]);
    if (auto err = std::get_if<PositionSetError>(&result))
        terminate_on_critical_error(err->what());

    auto nodes = std::get<u64>(result);
    sync_cout << "\nNodes searched: " << nodes << "\n" << sync_endl;
    return nodes;
}

void UCIEngine::position(std::istringstream& is) {
    const std::string fullCommand = is.str();

    std::string token, fen;

    is >> token;

    if (token == "startpos")
    {
        fen = StartFEN;
        is >> token;  // Consume the "moves" token, if any
    }
    else if (token == "fen")
        while (is >> token && token != "moves")
            fen += token + " ";
    else
        return;

    std::vector<std::string> moves;

    while (is >> token)
    {
        moves.push_back(token);
    }

    auto err = engine.set_position(fen, moves);
    if (err.has_value())
    {
        terminate_on_critical_error(err->what());
    }
}

namespace {

struct WinRateParams {
    double a;
    double b;
};

WinRateParams win_rate_params(const Position& pos) {

    int material = pos.count<PAWN>() + 3 * pos.count<KNIGHT>() + 3 * pos.count<BISHOP>()
                 + 5 * pos.count<ROOK>() + 9 * pos.count<QUEEN>();

    // The fitted model only uses data for material counts in [17, 78], and is anchored at count 58.
    double m = std::clamp(material, 17, 78) / 58.0;

    // Return a = p_a(material) and b = p_b(material), see github.com/official-veloct/WDL_model
    constexpr double as[] = {-72.32565836, 185.93832038, -144.58862193, 416.44950446};
    constexpr double bs[] = {83.86794042, -136.06112997, 69.98820887, 47.62901433};

    double a = (((as[0] * m + as[1]) * m + as[2]) * m) + as[3];
    double b = (((bs[0] * m + bs[1]) * m + bs[2]) * m) + bs[3];

    return {a, b};
}

// The win rate model is 1 / (1 + exp((a - eval) / b)), where a = p_a(material) and b = p_b(material).
// It fits the LTC fishtest statistics rather accurately.
int win_rate_model(Value v, const Position& pos) {

    auto [a, b] = win_rate_params(pos);

    // Return the win rate in per mille units, rounded to the nearest integer.
    return int(0.5 + 1000 / (1 + std::exp((a - double(v)) / b)));
}
}

std::string UCIEngine::format_score(const Score& s) {
    constexpr int TB_CP = 20000;
    const auto    format =
      overload{[](Score::Mate mate) -> std::string {
                   auto m = (mate.plies > 0 ? (mate.plies + 1) : mate.plies) / 2;
                   return std::string("mate ") + std::to_string(m);
               },
               [](Score::Tablebase tb) -> std::string {
                   return std::string("cp ") + std::to_string((tb.win ? TB_CP : -TB_CP) - tb.plies);
               },
               [](Score::InternalUnits units) -> std::string {
                   return std::string("cp ") + std::to_string(units.value);
               }};

    return s.visit(format);
}

// Turns a Value to an integer centipawn number,
// without treatment of mate and similar special scores.
int UCIEngine::to_cp(Value v, const Position& pos) {

    // In general, the score can be defined via the WDL as
    // (log(1/L - 1) - log(1/W - 1)) / (log(1/L - 1) + log(1/W - 1)).
    // Based on our win_rate_model, this simply yields v / a.

    auto [a, b] = win_rate_params(pos);

    return int(std::round(100 * int(v) / a));
}

std::string UCIEngine::wdl(Value v, const Position& pos) {
    std::stringstream ss;

    int wdl_w = win_rate_model(v, pos);
    int wdl_l = win_rate_model(-v, pos);
    int wdl_d = 1000 - wdl_w - wdl_l;
    ss << wdl_w << " " << wdl_d << " " << wdl_l;

    return ss.str();
}

std::string UCIEngine::square(Square s) {
    return std::string{char('a' + file_of(s)), char('1' + rank_of(s))};
}

std::string UCIEngine::move(Move m, bool chess960) {
    if (m == Move::none())
        return "(none)";

    if (m == Move::null())
        return "0000";

    Square from = m.from_sq();
    Square to   = m.to_sq();

    if (m.type_of() == CASTLING && !chess960)
        to = make_square(to > from ? FILE_G : FILE_C, rank_of(from));

    std::string move = square(from) + square(to);

    if (m.type_of() == PROMOTION)
        move += " pnbrqk"[m.promotion_type()];

    return move;
}


std::string UCIEngine::to_lower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return str;
}

Move UCIEngine::to_move(const Position& pos, std::string str) {
    str = to_lower(str);

    for (const auto& m : MoveList<LEGAL>(pos))
        if (str == move(m, pos.is_chess960()))
            return m;

    return Move::none();
}

void UCIEngine::on_update_no_moves(const Engine::InfoShort& info) {
    sync_cout << "info depth " << info.depth << " score " << format_score(info.score) << sync_endl;
}

void UCIEngine::on_update_full(const Engine::InfoFull& info, bool showWDL) {
    std::stringstream ss;

    ss << "info";
    ss << " depth " << info.depth                 //
       << " seldepth " << info.selDepth           //
       << " multipv " << info.multiPV             //
       << " score " << format_score(info.score);  //

    if (!info.bound.empty())
        ss << " " << info.bound;

    if (showWDL)
        ss << " wdl " << info.wdl;

    ss << " nodes " << info.nodes        //
       << " nps " << info.nps            //
       << " hashfull " << info.hashfull  //
       << " tbhits " << info.tbHits      //
       << " time " << info.timeMs        //
       << " pv " << info.pv;             //

    sync_cout << ss.str() << sync_endl;
}

void UCIEngine::on_iter(const Engine::InfoIter& info) {
    std::stringstream ss;

    ss << "info";
    ss << " depth " << info.depth                     //
       << " currmove " << info.currmove               //
       << " currmovenumber " << info.currmovenumber;  //

    sync_cout << ss.str() << sync_endl;
}

void UCIEngine::on_bestmove(std::string_view bestmove, std::string_view ponder) {
    sync_cout << "bestmove " << bestmove;
    if (!ponder.empty())
        std::cout << " ponder " << ponder;
    std::cout << sync_endl;
}

void UCIEngine::terminate_on_critical_error(const std::string& message) {
    sync_cout << "info string CRITICAL ERROR: Command `" << currentCmd
              << "` failed. Reason: " << message << '\n'
              << sync_endl;
    std::exit(1);
}

}  // namespace VeloCT

// ==== END OF FILE: uci.cpp ====

// ==== START OF FILE: ucioption.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ucioption.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <utility>

#include "misc.h"

namespace VeloCT {

bool CaseInsensitiveLess::operator()(const std::string& s1, const std::string& s2) const {

    return std::lexicographical_compare(
      s1.begin(), s1.end(), s2.begin(), s2.end(),
      [](unsigned char c1, unsigned char c2) { return std::tolower(c1) < std::tolower(c2); });
}

void OptionsMap::add_info_listener(InfoListener&& message_func) { info = std::move(message_func); }

void OptionsMap::setoption(std::istringstream& is) {
    std::string token, name, value;

    is >> token;  // Consume the "name" token

    // Read the option name (can contain spaces)
    while (is >> token && token != "value")
        name += (name.empty() ? "" : " ") + token;

    // Read the option value (can contain spaces)
    while (is >> token)
        value += (value.empty() ? "" : " ") + token;

    if (options_map.count(name))
        options_map[name] = value;
    else
        sync_cout << "No such option: " << name << sync_endl;
}

const Option& OptionsMap::operator[](const std::string& name) const {
    auto it = options_map.find(name);
    assert(it != options_map.end());
    return it->second;
}

// Inits options and assigns idx in the correct printing order
void OptionsMap::add(const std::string& name, const Option& option) {
    if (!options_map.count(name))
    {
        static usize insert_order = 0;

        options_map[name] = option;

        options_map[name].parent = this;
        options_map[name].idx    = insert_order++;
    }
    else
    {
        std::cerr << "Option \"" << name << "\" was already added!" << std::endl;
        std::exit(EXIT_FAILURE);
    }
}


usize OptionsMap::count(const std::string& name) const { return options_map.count(name); }

Option::Option(const OptionsMap* map) :
    parent(map) {}

Option::Option(const char* v, OnChange f) :
    type("string"),
    min(0),
    max(0),
    on_change(std::move(f)) {
    defaultValue = currentValue = v;
}

Option::Option(bool v, OnChange f) :
    type("check"),
    min(0),
    max(0),
    on_change(std::move(f)) {
    defaultValue = currentValue = (v ? "true" : "false");
}

Option::Option(OnChange f) :
    type("button"),
    min(0),
    max(0),
    on_change(std::move(f)) {}

Option::Option(int v, int minv, int maxv, OnChange f) :
    type("spin"),
    min(minv),
    max(maxv),
    on_change(std::move(f)) {
    defaultValue = currentValue = std::to_string(v);
}

Option::Option(const char* v, const char* cur, OnChange f) :
    type("combo"),
    min(0),
    max(0),
    on_change(std::move(f)) {
    defaultValue = v;
    currentValue = cur;
}

Option::operator int() const {
    assert(type == "check" || type == "spin");
    return (type == "spin" ? std::stoi(currentValue) : currentValue == "true");
}

Option::operator std::string() const {
    assert(type == "string");
    return currentValue;
}

bool Option::operator==(const char* s) const {
    assert(type == "combo");
    return !CaseInsensitiveLess()(currentValue, s) && !CaseInsensitiveLess()(s, currentValue);
}

bool Option::operator!=(const char* s) const { return !(*this == s); }

static bool value_in_range(const std::string& v, int min, int max) {
    if (v.empty())
        return false;
    errno                  = 0;
    char*           end    = nullptr;
    const long long result = std::strtoll(v.c_str(), &end, 10);
    if (errno == ERANGE || *end != '\0')
        return false;
    return result >= min && result <= max;
}

// Updates currentValue and triggers on_change() action. It's up to
// the GUI to check for option's limits, but we could receive the new value
// from the user by console window, so let's check the bounds anyway.
Option& Option::operator=(const std::string& v) {

    assert(!type.empty());

    if ((type != "button" && type != "string" && v.empty())
        || (type == "check" && v != "true" && v != "false")
        || (type == "spin" && !value_in_range(v, min, max)))
        return *this;

    if (type == "combo")
    {
        OptionsMap         comboMap;  // To have case insensitive compare
        std::string        token;
        std::istringstream ss(defaultValue);
        while (ss >> token)
            comboMap.add(token, Option());
        if (!comboMap.count(v) || v == "var")
            return *this;
    }

    if (type == "string")
        currentValue = v == "<empty>" ? "" : v;
    else if (type != "button")
        currentValue = v;

    if (on_change)
    {
        const auto ret = on_change(*this);

        if (ret && parent != nullptr && parent->info != nullptr)
            parent->info(ret);
    }

    return *this;
}

std::ostream& operator<<(std::ostream& os, const OptionsMap& om) {
    for (usize idx = 0; idx < om.options_map.size(); ++idx)
        for (const auto& it : om.options_map)
            if (it.second.idx == idx)
            {
                const Option& o = it.second;
                os << "\noption name " << it.first << " type " << o.type;

                if (o.type == "check" || o.type == "combo")
                    os << " default " << o.defaultValue;

                else if (o.type == "string")
                {
                    std::string defaultValue = o.defaultValue.empty() ? "<empty>" : o.defaultValue;
                    os << " default " << defaultValue;
                }

                else if (o.type == "spin")
                    os << " default " << stoi(o.defaultValue) << " min " << o.min << " max "
                       << o.max;

                break;
            }

    return os;
}
}

// ==== END OF FILE: ucioption.cpp ====

// ==== START OF FILE: main.cpp ====
/*
  VeloCT, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The VeloCT developers (see AUTHORS file)

  VeloCT is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  VeloCT is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <memory>
#include <utility>

#include "attacks.h"
#include "bitboard.h"
#include "misc.h"
#include "position.h"
#include "tune.h"
#include "uci.h"

using namespace VeloCT;

#ifdef UNIVERSAL_BINARY
namespace VeloCT {

int main(int argc, char* argv[]);  // silence 'no previous declaration'

__attribute__((used)) // keep main alive
#endif

int main(int argc, char* argv[]) {
    std::cout << engine_info() << std::endl;

    Bitboards::init();
    Attacks::init();
    Position::init();

    auto cli = CommandLine(argc, argv);
    auto uci = std::make_unique<UCIEngine>(std::move(cli));

    Tune::init(uci->engine_options());

    uci->loop();

    return 0;
}

#ifdef UNIVERSAL_BINARY
}  // namespace VeloCT

    #ifdef UNIVERSAL_NEEDS_MAIN_SHIM
int main(int argc, char* argv[]) { return VeloCT::main(argc, argv); }
    #endif
#endif

// ==== END OF FILE: main.cpp ====
