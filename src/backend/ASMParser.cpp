#include <algorithm>
#include <iostream>

#include "ASMParser.h"
#include "LinearRegAllocator.h"

using namespace std;

static unordered_map<unsigned, unsigned> num2powerMap = {
    {0x00000002, 1},  {0x00000004, 2},  {0x00000008, 3},  {0x00000010, 4},
    {0x00000020, 5},  {0x00000040, 6},  {0x00000080, 7},  {0x00000100, 8},
    {0x00000200, 9},  {0x00000400, 10}, {0x00000800, 11}, {0x00001000, 12},
    {0x00002000, 13}, {0x00004000, 14}, {0x00008000, 15}, {0x00010000, 16},
    {0x00020000, 17}, {0x00040000, 18}, {0x00080000, 19}, {0x00100000, 20},
    {0x00200000, 21}, {0x00400000, 22}, {0x00800000, 23}, {0x01000000, 24},
    {0x02000000, 25}, {0x04000000, 26}, {0x08000000, 27}, {0x10000000, 28},
    {0x20000000, 29}, {0x40000000, 30}, {0x80000000, 31}};
static unordered_map<unsigned, pair<unsigned, unsigned>> num2power2Map = {
    {0x00000003, {0, 1}},   {0x00000005, {0, 2}},   {0x00000006, {1, 2}},
    {0x00000009, {0, 3}},   {0x0000000a, {1, 3}},   {0x0000000c, {2, 3}},
    {0x00000011, {0, 4}},   {0x00000012, {1, 4}},   {0x00000014, {2, 4}},
    {0x00000018, {3, 4}},   {0x00000021, {0, 5}},   {0x00000022, {1, 5}},
    {0x00000024, {2, 5}},   {0x00000028, {3, 5}},   {0x00000030, {4, 5}},
    {0x00000041, {0, 6}},   {0x00000042, {1, 6}},   {0x00000044, {2, 6}},
    {0x00000048, {3, 6}},   {0x00000050, {4, 6}},   {0x00000060, {5, 6}},
    {0x00000081, {0, 7}},   {0x00000082, {1, 7}},   {0x00000084, {2, 7}},
    {0x00000088, {3, 7}},   {0x00000090, {4, 7}},   {0x000000a0, {5, 7}},
    {0x000000c0, {6, 7}},   {0x00000101, {0, 8}},   {0x00000102, {1, 8}},
    {0x00000104, {2, 8}},   {0x00000108, {3, 8}},   {0x00000110, {4, 8}},
    {0x00000120, {5, 8}},   {0x00000140, {6, 8}},   {0x00000180, {7, 8}},
    {0x00000201, {0, 9}},   {0x00000202, {1, 9}},   {0x00000204, {2, 9}},
    {0x00000208, {3, 9}},   {0x00000210, {4, 9}},   {0x00000220, {5, 9}},
    {0x00000240, {6, 9}},   {0x00000280, {7, 9}},   {0x00000300, {8, 9}},
    {0x00000401, {0, 10}},  {0x00000402, {1, 10}},  {0x00000404, {2, 10}},
    {0x00000408, {3, 10}},  {0x00000410, {4, 10}},  {0x00000420, {5, 10}},
    {0x00000440, {6, 10}},  {0x00000480, {7, 10}},  {0x00000500, {8, 10}},
    {0x00000600, {9, 10}},  {0x00000801, {0, 11}},  {0x00000802, {1, 11}},
    {0x00000804, {2, 11}},  {0x00000808, {3, 11}},  {0x00000810, {4, 11}},
    {0x00000820, {5, 11}},  {0x00000840, {6, 11}},  {0x00000880, {7, 11}},
    {0x00000900, {8, 11}},  {0x00000a00, {9, 11}},  {0x00000c00, {10, 11}},
    {0x00001001, {0, 12}},  {0x00001002, {1, 12}},  {0x00001004, {2, 12}},
    {0x00001008, {3, 12}},  {0x00001010, {4, 12}},  {0x00001020, {5, 12}},
    {0x00001040, {6, 12}},  {0x00001080, {7, 12}},  {0x00001100, {8, 12}},
    {0x00001200, {9, 12}},  {0x00001400, {10, 12}}, {0x00001800, {11, 12}},
    {0x00002001, {0, 13}},  {0x00002002, {1, 13}},  {0x00002004, {2, 13}},
    {0x00002008, {3, 13}},  {0x00002010, {4, 13}},  {0x00002020, {5, 13}},
    {0x00002040, {6, 13}},  {0x00002080, {7, 13}},  {0x00002100, {8, 13}},
    {0x00002200, {9, 13}},  {0x00002400, {10, 13}}, {0x00002800, {11, 13}},
    {0x00003000, {12, 13}}, {0x00004001, {0, 14}},  {0x00004002, {1, 14}},
    {0x00004004, {2, 14}},  {0x00004008, {3, 14}},  {0x00004010, {4, 14}},
    {0x00004020, {5, 14}},  {0x00004040, {6, 14}},  {0x00004080, {7, 14}},
    {0x00004100, {8, 14}},  {0x00004200, {9, 14}},  {0x00004400, {10, 14}},
    {0x00004800, {11, 14}}, {0x00005000, {12, 14}}, {0x00006000, {13, 14}},
    {0x00008001, {0, 15}},  {0x00008002, {1, 15}},  {0x00008004, {2, 15}},
    {0x00008008, {3, 15}},  {0x00008010, {4, 15}},  {0x00008020, {5, 15}},
    {0x00008040, {6, 15}},  {0x00008080, {7, 15}},  {0x00008100, {8, 15}},
    {0x00008200, {9, 15}},  {0x00008400, {10, 15}}, {0x00008800, {11, 15}},
    {0x00009000, {12, 15}}, {0x0000a000, {13, 15}}, {0x0000c000, {14, 15}},
    {0x00010001, {0, 16}},  {0x00010002, {1, 16}},  {0x00010004, {2, 16}},
    {0x00010008, {3, 16}},  {0x00010010, {4, 16}},  {0x00010020, {5, 16}},
    {0x00010040, {6, 16}},  {0x00010080, {7, 16}},  {0x00010100, {8, 16}},
    {0x00010200, {9, 16}},  {0x00010400, {10, 16}}, {0x00010800, {11, 16}},
    {0x00011000, {12, 16}}, {0x00012000, {13, 16}}, {0x00014000, {14, 16}},
    {0x00018000, {15, 16}}, {0x00020001, {0, 17}},  {0x00020002, {1, 17}},
    {0x00020004, {2, 17}},  {0x00020008, {3, 17}},  {0x00020010, {4, 17}},
    {0x00020020, {5, 17}},  {0x00020040, {6, 17}},  {0x00020080, {7, 17}},
    {0x00020100, {8, 17}},  {0x00020200, {9, 17}},  {0x00020400, {10, 17}},
    {0x00020800, {11, 17}}, {0x00021000, {12, 17}}, {0x00022000, {13, 17}},
    {0x00024000, {14, 17}}, {0x00028000, {15, 17}}, {0x00030000, {16, 17}},
    {0x00040001, {0, 18}},  {0x00040002, {1, 18}},  {0x00040004, {2, 18}},
    {0x00040008, {3, 18}},  {0x00040010, {4, 18}},  {0x00040020, {5, 18}},
    {0x00040040, {6, 18}},  {0x00040080, {7, 18}},  {0x00040100, {8, 18}},
    {0x00040200, {9, 18}},  {0x00040400, {10, 18}}, {0x00040800, {11, 18}},
    {0x00041000, {12, 18}}, {0x00042000, {13, 18}}, {0x00044000, {14, 18}},
    {0x00048000, {15, 18}}, {0x00050000, {16, 18}}, {0x00060000, {17, 18}},
    {0x00080001, {0, 19}},  {0x00080002, {1, 19}},  {0x00080004, {2, 19}},
    {0x00080008, {3, 19}},  {0x00080010, {4, 19}},  {0x00080020, {5, 19}},
    {0x00080040, {6, 19}},  {0x00080080, {7, 19}},  {0x00080100, {8, 19}},
    {0x00080200, {9, 19}},  {0x00080400, {10, 19}}, {0x00080800, {11, 19}},
    {0x00081000, {12, 19}}, {0x00082000, {13, 19}}, {0x00084000, {14, 19}},
    {0x00088000, {15, 19}}, {0x00090000, {16, 19}}, {0x000a0000, {17, 19}},
    {0x000c0000, {18, 19}}, {0x00100001, {0, 20}},  {0x00100002, {1, 20}},
    {0x00100004, {2, 20}},  {0x00100008, {3, 20}},  {0x00100010, {4, 20}},
    {0x00100020, {5, 20}},  {0x00100040, {6, 20}},  {0x00100080, {7, 20}},
    {0x00100100, {8, 20}},  {0x00100200, {9, 20}},  {0x00100400, {10, 20}},
    {0x00100800, {11, 20}}, {0x00101000, {12, 20}}, {0x00102000, {13, 20}},
    {0x00104000, {14, 20}}, {0x00108000, {15, 20}}, {0x00110000, {16, 20}},
    {0x00120000, {17, 20}}, {0x00140000, {18, 20}}, {0x00180000, {19, 20}},
    {0x00200001, {0, 21}},  {0x00200002, {1, 21}},  {0x00200004, {2, 21}},
    {0x00200008, {3, 21}},  {0x00200010, {4, 21}},  {0x00200020, {5, 21}},
    {0x00200040, {6, 21}},  {0x00200080, {7, 21}},  {0x00200100, {8, 21}},
    {0x00200200, {9, 21}},  {0x00200400, {10, 21}}, {0x00200800, {11, 21}},
    {0x00201000, {12, 21}}, {0x00202000, {13, 21}}, {0x00204000, {14, 21}},
    {0x00208000, {15, 21}}, {0x00210000, {16, 21}}, {0x00220000, {17, 21}},
    {0x00240000, {18, 21}}, {0x00280000, {19, 21}}, {0x00300000, {20, 21}},
    {0x00400001, {0, 22}},  {0x00400002, {1, 22}},  {0x00400004, {2, 22}},
    {0x00400008, {3, 22}},  {0x00400010, {4, 22}},  {0x00400020, {5, 22}},
    {0x00400040, {6, 22}},  {0x00400080, {7, 22}},  {0x00400100, {8, 22}},
    {0x00400200, {9, 22}},  {0x00400400, {10, 22}}, {0x00400800, {11, 22}},
    {0x00401000, {12, 22}}, {0x00402000, {13, 22}}, {0x00404000, {14, 22}},
    {0x00408000, {15, 22}}, {0x00410000, {16, 22}}, {0x00420000, {17, 22}},
    {0x00440000, {18, 22}}, {0x00480000, {19, 22}}, {0x00500000, {20, 22}},
    {0x00600000, {21, 22}}, {0x00800001, {0, 23}},  {0x00800002, {1, 23}},
    {0x00800004, {2, 23}},  {0x00800008, {3, 23}},  {0x00800010, {4, 23}},
    {0x00800020, {5, 23}},  {0x00800040, {6, 23}},  {0x00800080, {7, 23}},
    {0x00800100, {8, 23}},  {0x00800200, {9, 23}},  {0x00800400, {10, 23}},
    {0x00800800, {11, 23}}, {0x00801000, {12, 23}}, {0x00802000, {13, 23}},
    {0x00804000, {14, 23}}, {0x00808000, {15, 23}}, {0x00810000, {16, 23}},
    {0x00820000, {17, 23}}, {0x00840000, {18, 23}}, {0x00880000, {19, 23}},
    {0x00900000, {20, 23}}, {0x00a00000, {21, 23}}, {0x00c00000, {22, 23}},
    {0x01000001, {0, 24}},  {0x01000002, {1, 24}},  {0x01000004, {2, 24}},
    {0x01000008, {3, 24}},  {0x01000010, {4, 24}},  {0x01000020, {5, 24}},
    {0x01000040, {6, 24}},  {0x01000080, {7, 24}},  {0x01000100, {8, 24}},
    {0x01000200, {9, 24}},  {0x01000400, {10, 24}}, {0x01000800, {11, 24}},
    {0x01001000, {12, 24}}, {0x01002000, {13, 24}}, {0x01004000, {14, 24}},
    {0x01008000, {15, 24}}, {0x01010000, {16, 24}}, {0x01020000, {17, 24}},
    {0x01040000, {18, 24}}, {0x01080000, {19, 24}}, {0x01100000, {20, 24}},
    {0x01200000, {21, 24}}, {0x01400000, {22, 24}}, {0x01800000, {23, 24}},
    {0x02000001, {0, 25}},  {0x02000002, {1, 25}},  {0x02000004, {2, 25}},
    {0x02000008, {3, 25}},  {0x02000010, {4, 25}},  {0x02000020, {5, 25}},
    {0x02000040, {6, 25}},  {0x02000080, {7, 25}},  {0x02000100, {8, 25}},
    {0x02000200, {9, 25}},  {0x02000400, {10, 25}}, {0x02000800, {11, 25}},
    {0x02001000, {12, 25}}, {0x02002000, {13, 25}}, {0x02004000, {14, 25}},
    {0x02008000, {15, 25}}, {0x02010000, {16, 25}}, {0x02020000, {17, 25}},
    {0x02040000, {18, 25}}, {0x02080000, {19, 25}}, {0x02100000, {20, 25}},
    {0x02200000, {21, 25}}, {0x02400000, {22, 25}}, {0x02800000, {23, 25}},
    {0x03000000, {24, 25}}, {0x04000001, {0, 26}},  {0x04000002, {1, 26}},
    {0x04000004, {2, 26}},  {0x04000008, {3, 26}},  {0x04000010, {4, 26}},
    {0x04000020, {5, 26}},  {0x04000040, {6, 26}},  {0x04000080, {7, 26}},
    {0x04000100, {8, 26}},  {0x04000200, {9, 26}},  {0x04000400, {10, 26}},
    {0x04000800, {11, 26}}, {0x04001000, {12, 26}}, {0x04002000, {13, 26}},
    {0x04004000, {14, 26}}, {0x04008000, {15, 26}}, {0x04010000, {16, 26}},
    {0x04020000, {17, 26}}, {0x04040000, {18, 26}}, {0x04080000, {19, 26}},
    {0x04100000, {20, 26}}, {0x04200000, {21, 26}}, {0x04400000, {22, 26}},
    {0x04800000, {23, 26}}, {0x05000000, {24, 26}}, {0x06000000, {25, 26}},
    {0x08000001, {0, 27}},  {0x08000002, {1, 27}},  {0x08000004, {2, 27}},
    {0x08000008, {3, 27}},  {0x08000010, {4, 27}},  {0x08000020, {5, 27}},
    {0x08000040, {6, 27}},  {0x08000080, {7, 27}},  {0x08000100, {8, 27}},
    {0x08000200, {9, 27}},  {0x08000400, {10, 27}}, {0x08000800, {11, 27}},
    {0x08001000, {12, 27}}, {0x08002000, {13, 27}}, {0x08004000, {14, 27}},
    {0x08008000, {15, 27}}, {0x08010000, {16, 27}}, {0x08020000, {17, 27}},
    {0x08040000, {18, 27}}, {0x08080000, {19, 27}}, {0x08100000, {20, 27}},
    {0x08200000, {21, 27}}, {0x08400000, {22, 27}}, {0x08800000, {23, 27}},
    {0x09000000, {24, 27}}, {0x0a000000, {25, 27}}, {0x0c000000, {26, 27}},
    {0x10000001, {0, 28}},  {0x10000002, {1, 28}},  {0x10000004, {2, 28}},
    {0x10000008, {3, 28}},  {0x10000010, {4, 28}},  {0x10000020, {5, 28}},
    {0x10000040, {6, 28}},  {0x10000080, {7, 28}},  {0x10000100, {8, 28}},
    {0x10000200, {9, 28}},  {0x10000400, {10, 28}}, {0x10000800, {11, 28}},
    {0x10001000, {12, 28}}, {0x10002000, {13, 28}}, {0x10004000, {14, 28}},
    {0x10008000, {15, 28}}, {0x10010000, {16, 28}}, {0x10020000, {17, 28}},
    {0x10040000, {18, 28}}, {0x10080000, {19, 28}}, {0x10100000, {20, 28}},
    {0x10200000, {21, 28}}, {0x10400000, {22, 28}}, {0x10800000, {23, 28}},
    {0x11000000, {24, 28}}, {0x12000000, {25, 28}}, {0x14000000, {26, 28}},
    {0x18000000, {27, 28}}, {0x20000001, {0, 29}},  {0x20000002, {1, 29}},
    {0x20000004, {2, 29}},  {0x20000008, {3, 29}},  {0x20000010, {4, 29}},
    {0x20000020, {5, 29}},  {0x20000040, {6, 29}},  {0x20000080, {7, 29}},
    {0x20000100, {8, 29}},  {0x20000200, {9, 29}},  {0x20000400, {10, 29}},
    {0x20000800, {11, 29}}, {0x20001000, {12, 29}}, {0x20002000, {13, 29}},
    {0x20004000, {14, 29}}, {0x20008000, {15, 29}}, {0x20010000, {16, 29}},
    {0x20020000, {17, 29}}, {0x20040000, {18, 29}}, {0x20080000, {19, 29}},
    {0x20100000, {20, 29}}, {0x20200000, {21, 29}}, {0x20400000, {22, 29}},
    {0x20800000, {23, 29}}, {0x21000000, {24, 29}}, {0x22000000, {25, 29}},
    {0x24000000, {26, 29}}, {0x28000000, {27, 29}}, {0x30000000, {28, 29}},
    {0x40000001, {0, 30}},  {0x40000002, {1, 30}},  {0x40000004, {2, 30}},
    {0x40000008, {3, 30}},  {0x40000010, {4, 30}},  {0x40000020, {5, 30}},
    {0x40000040, {6, 30}},  {0x40000080, {7, 30}},  {0x40000100, {8, 30}},
    {0x40000200, {9, 30}},  {0x40000400, {10, 30}}, {0x40000800, {11, 30}},
    {0x40001000, {12, 30}}, {0x40002000, {13, 30}}, {0x40004000, {14, 30}},
    {0x40008000, {15, 30}}, {0x40010000, {16, 30}}, {0x40020000, {17, 30}},
    {0x40040000, {18, 30}}, {0x40080000, {19, 30}}, {0x40100000, {20, 30}},
    {0x40200000, {21, 30}}, {0x40400000, {22, 30}}, {0x40800000, {23, 30}},
    {0x41000000, {24, 30}}, {0x42000000, {25, 30}}, {0x44000000, {26, 30}},
    {0x48000000, {27, 30}}, {0x50000000, {28, 30}}, {0x60000000, {29, 30}},
    {0x80000001, {0, 31}},  {0x80000002, {1, 31}},  {0x80000004, {2, 31}},
    {0x80000008, {3, 31}},  {0x80000010, {4, 31}},  {0x80000020, {5, 31}},
    {0x80000040, {6, 31}},  {0x80000080, {7, 31}},  {0x80000100, {8, 31}},
    {0x80000200, {9, 31}},  {0x80000400, {10, 31}}, {0x80000800, {11, 31}},
    {0x80001000, {12, 31}}, {0x80002000, {13, 31}}, {0x80004000, {14, 31}},
    {0x80008000, {15, 31}}, {0x80010000, {16, 31}}, {0x80020000, {17, 31}},
    {0x80040000, {18, 31}}, {0x80080000, {19, 31}}, {0x80100000, {20, 31}},
    {0x80200000, {21, 31}}, {0x80400000, {22, 31}}, {0x80800000, {23, 31}},
    {0x81000000, {24, 31}}, {0x82000000, {25, 31}}, {0x84000000, {26, 31}},
    {0x88000000, {27, 31}}, {0x90000000, {28, 31}}, {0xa0000000, {29, 31}},
    {0xc0000000, {30, 31}}};
static unordered_map<unsigned, pair<unsigned, unsigned>> num2lineMap = {
    {0x00000007, {0, 2}},   {0x0000000e, {1, 3}},   {0x0000000f, {0, 3}},
    {0x0000001c, {2, 4}},   {0x0000001e, {1, 4}},   {0x0000001f, {0, 4}},
    {0x00000038, {3, 5}},   {0x0000003c, {2, 5}},   {0x0000003e, {1, 5}},
    {0x0000003f, {0, 5}},   {0x00000070, {4, 6}},   {0x00000078, {3, 6}},
    {0x0000007c, {2, 6}},   {0x0000007e, {1, 6}},   {0x0000007f, {0, 6}},
    {0x000000e0, {5, 7}},   {0x000000f0, {4, 7}},   {0x000000f8, {3, 7}},
    {0x000000fc, {2, 7}},   {0x000000fe, {1, 7}},   {0x000000ff, {0, 7}},
    {0x000001c0, {6, 8}},   {0x000001e0, {5, 8}},   {0x000001f0, {4, 8}},
    {0x000001f8, {3, 8}},   {0x000001fc, {2, 8}},   {0x000001fe, {1, 8}},
    {0x000001ff, {0, 8}},   {0x00000380, {7, 9}},   {0x000003c0, {6, 9}},
    {0x000003e0, {5, 9}},   {0x000003f0, {4, 9}},   {0x000003f8, {3, 9}},
    {0x000003fc, {2, 9}},   {0x000003fe, {1, 9}},   {0x000003ff, {0, 9}},
    {0x00000700, {8, 10}},  {0x00000780, {7, 10}},  {0x000007c0, {6, 10}},
    {0x000007e0, {5, 10}},  {0x000007f0, {4, 10}},  {0x000007f8, {3, 10}},
    {0x000007fc, {2, 10}},  {0x000007fe, {1, 10}},  {0x000007ff, {0, 10}},
    {0x00000e00, {9, 11}},  {0x00000f00, {8, 11}},  {0x00000f80, {7, 11}},
    {0x00000fc0, {6, 11}},  {0x00000fe0, {5, 11}},  {0x00000ff0, {4, 11}},
    {0x00000ff8, {3, 11}},  {0x00000ffc, {2, 11}},  {0x00000ffe, {1, 11}},
    {0x00000fff, {0, 11}},  {0x00001c00, {10, 12}}, {0x00001e00, {9, 12}},
    {0x00001f00, {8, 12}},  {0x00001f80, {7, 12}},  {0x00001fc0, {6, 12}},
    {0x00001fe0, {5, 12}},  {0x00001ff0, {4, 12}},  {0x00001ff8, {3, 12}},
    {0x00001ffc, {2, 12}},  {0x00001ffe, {1, 12}},  {0x00001fff, {0, 12}},
    {0x00003800, {11, 13}}, {0x00003c00, {10, 13}}, {0x00003e00, {9, 13}},
    {0x00003f00, {8, 13}},  {0x00003f80, {7, 13}},  {0x00003fc0, {6, 13}},
    {0x00003fe0, {5, 13}},  {0x00003ff0, {4, 13}},  {0x00003ff8, {3, 13}},
    {0x00003ffc, {2, 13}},  {0x00003ffe, {1, 13}},  {0x00003fff, {0, 13}},
    {0x00007000, {12, 14}}, {0x00007800, {11, 14}}, {0x00007c00, {10, 14}},
    {0x00007e00, {9, 14}},  {0x00007f00, {8, 14}},  {0x00007f80, {7, 14}},
    {0x00007fc0, {6, 14}},  {0x00007fe0, {5, 14}},  {0x00007ff0, {4, 14}},
    {0x00007ff8, {3, 14}},  {0x00007ffc, {2, 14}},  {0x00007ffe, {1, 14}},
    {0x00007fff, {0, 14}},  {0x0000e000, {13, 15}}, {0x0000f000, {12, 15}},
    {0x0000f800, {11, 15}}, {0x0000fc00, {10, 15}}, {0x0000fe00, {9, 15}},
    {0x0000ff00, {8, 15}},  {0x0000ff80, {7, 15}},  {0x0000ffc0, {6, 15}},
    {0x0000ffe0, {5, 15}},  {0x0000fff0, {4, 15}},  {0x0000fff8, {3, 15}},
    {0x0000fffc, {2, 15}},  {0x0000fffe, {1, 15}},  {0x0000ffff, {0, 15}},
    {0x0001c000, {14, 16}}, {0x0001e000, {13, 16}}, {0x0001f000, {12, 16}},
    {0x0001f800, {11, 16}}, {0x0001fc00, {10, 16}}, {0x0001fe00, {9, 16}},
    {0x0001ff00, {8, 16}},  {0x0001ff80, {7, 16}},  {0x0001ffc0, {6, 16}},
    {0x0001ffe0, {5, 16}},  {0x0001fff0, {4, 16}},  {0x0001fff8, {3, 16}},
    {0x0001fffc, {2, 16}},  {0x0001fffe, {1, 16}},  {0x0001ffff, {0, 16}},
    {0x00038000, {15, 17}}, {0x0003c000, {14, 17}}, {0x0003e000, {13, 17}},
    {0x0003f000, {12, 17}}, {0x0003f800, {11, 17}}, {0x0003fc00, {10, 17}},
    {0x0003fe00, {9, 17}},  {0x0003ff00, {8, 17}},  {0x0003ff80, {7, 17}},
    {0x0003ffc0, {6, 17}},  {0x0003ffe0, {5, 17}},  {0x0003fff0, {4, 17}},
    {0x0003fff8, {3, 17}},  {0x0003fffc, {2, 17}},  {0x0003fffe, {1, 17}},
    {0x0003ffff, {0, 17}},  {0x00070000, {16, 18}}, {0x00078000, {15, 18}},
    {0x0007c000, {14, 18}}, {0x0007e000, {13, 18}}, {0x0007f000, {12, 18}},
    {0x0007f800, {11, 18}}, {0x0007fc00, {10, 18}}, {0x0007fe00, {9, 18}},
    {0x0007ff00, {8, 18}},  {0x0007ff80, {7, 18}},  {0x0007ffc0, {6, 18}},
    {0x0007ffe0, {5, 18}},  {0x0007fff0, {4, 18}},  {0x0007fff8, {3, 18}},
    {0x0007fffc, {2, 18}},  {0x0007fffe, {1, 18}},  {0x0007ffff, {0, 18}},
    {0x000e0000, {17, 19}}, {0x000f0000, {16, 19}}, {0x000f8000, {15, 19}},
    {0x000fc000, {14, 19}}, {0x000fe000, {13, 19}}, {0x000ff000, {12, 19}},
    {0x000ff800, {11, 19}}, {0x000ffc00, {10, 19}}, {0x000ffe00, {9, 19}},
    {0x000fff00, {8, 19}},  {0x000fff80, {7, 19}},  {0x000fffc0, {6, 19}},
    {0x000fffe0, {5, 19}},  {0x000ffff0, {4, 19}},  {0x000ffff8, {3, 19}},
    {0x000ffffc, {2, 19}},  {0x000ffffe, {1, 19}},  {0x000fffff, {0, 19}},
    {0x001c0000, {18, 20}}, {0x001e0000, {17, 20}}, {0x001f0000, {16, 20}},
    {0x001f8000, {15, 20}}, {0x001fc000, {14, 20}}, {0x001fe000, {13, 20}},
    {0x001ff000, {12, 20}}, {0x001ff800, {11, 20}}, {0x001ffc00, {10, 20}},
    {0x001ffe00, {9, 20}},  {0x001fff00, {8, 20}},  {0x001fff80, {7, 20}},
    {0x001fffc0, {6, 20}},  {0x001fffe0, {5, 20}},  {0x001ffff0, {4, 20}},
    {0x001ffff8, {3, 20}},  {0x001ffffc, {2, 20}},  {0x001ffffe, {1, 20}},
    {0x001fffff, {0, 20}},  {0x00380000, {19, 21}}, {0x003c0000, {18, 21}},
    {0x003e0000, {17, 21}}, {0x003f0000, {16, 21}}, {0x003f8000, {15, 21}},
    {0x003fc000, {14, 21}}, {0x003fe000, {13, 21}}, {0x003ff000, {12, 21}},
    {0x003ff800, {11, 21}}, {0x003ffc00, {10, 21}}, {0x003ffe00, {9, 21}},
    {0x003fff00, {8, 21}},  {0x003fff80, {7, 21}},  {0x003fffc0, {6, 21}},
    {0x003fffe0, {5, 21}},  {0x003ffff0, {4, 21}},  {0x003ffff8, {3, 21}},
    {0x003ffffc, {2, 21}},  {0x003ffffe, {1, 21}},  {0x003fffff, {0, 21}},
    {0x00700000, {20, 22}}, {0x00780000, {19, 22}}, {0x007c0000, {18, 22}},
    {0x007e0000, {17, 22}}, {0x007f0000, {16, 22}}, {0x007f8000, {15, 22}},
    {0x007fc000, {14, 22}}, {0x007fe000, {13, 22}}, {0x007ff000, {12, 22}},
    {0x007ff800, {11, 22}}, {0x007ffc00, {10, 22}}, {0x007ffe00, {9, 22}},
    {0x007fff00, {8, 22}},  {0x007fff80, {7, 22}},  {0x007fffc0, {6, 22}},
    {0x007fffe0, {5, 22}},  {0x007ffff0, {4, 22}},  {0x007ffff8, {3, 22}},
    {0x007ffffc, {2, 22}},  {0x007ffffe, {1, 22}},  {0x007fffff, {0, 22}},
    {0x00e00000, {21, 23}}, {0x00f00000, {20, 23}}, {0x00f80000, {19, 23}},
    {0x00fc0000, {18, 23}}, {0x00fe0000, {17, 23}}, {0x00ff0000, {16, 23}},
    {0x00ff8000, {15, 23}}, {0x00ffc000, {14, 23}}, {0x00ffe000, {13, 23}},
    {0x00fff000, {12, 23}}, {0x00fff800, {11, 23}}, {0x00fffc00, {10, 23}},
    {0x00fffe00, {9, 23}},  {0x00ffff00, {8, 23}},  {0x00ffff80, {7, 23}},
    {0x00ffffc0, {6, 23}},  {0x00ffffe0, {5, 23}},  {0x00fffff0, {4, 23}},
    {0x00fffff8, {3, 23}},  {0x00fffffc, {2, 23}},  {0x00fffffe, {1, 23}},
    {0x00ffffff, {0, 23}},  {0x01c00000, {22, 24}}, {0x01e00000, {21, 24}},
    {0x01f00000, {20, 24}}, {0x01f80000, {19, 24}}, {0x01fc0000, {18, 24}},
    {0x01fe0000, {17, 24}}, {0x01ff0000, {16, 24}}, {0x01ff8000, {15, 24}},
    {0x01ffc000, {14, 24}}, {0x01ffe000, {13, 24}}, {0x01fff000, {12, 24}},
    {0x01fff800, {11, 24}}, {0x01fffc00, {10, 24}}, {0x01fffe00, {9, 24}},
    {0x01ffff00, {8, 24}},  {0x01ffff80, {7, 24}},  {0x01ffffc0, {6, 24}},
    {0x01ffffe0, {5, 24}},  {0x01fffff0, {4, 24}},  {0x01fffff8, {3, 24}},
    {0x01fffffc, {2, 24}},  {0x01fffffe, {1, 24}},  {0x01ffffff, {0, 24}},
    {0x03800000, {23, 25}}, {0x03c00000, {22, 25}}, {0x03e00000, {21, 25}},
    {0x03f00000, {20, 25}}, {0x03f80000, {19, 25}}, {0x03fc0000, {18, 25}},
    {0x03fe0000, {17, 25}}, {0x03ff0000, {16, 25}}, {0x03ff8000, {15, 25}},
    {0x03ffc000, {14, 25}}, {0x03ffe000, {13, 25}}, {0x03fff000, {12, 25}},
    {0x03fff800, {11, 25}}, {0x03fffc00, {10, 25}}, {0x03fffe00, {9, 25}},
    {0x03ffff00, {8, 25}},  {0x03ffff80, {7, 25}},  {0x03ffffc0, {6, 25}},
    {0x03ffffe0, {5, 25}},  {0x03fffff0, {4, 25}},  {0x03fffff8, {3, 25}},
    {0x03fffffc, {2, 25}},  {0x03fffffe, {1, 25}},  {0x03ffffff, {0, 25}},
    {0x07000000, {24, 26}}, {0x07800000, {23, 26}}, {0x07c00000, {22, 26}},
    {0x07e00000, {21, 26}}, {0x07f00000, {20, 26}}, {0x07f80000, {19, 26}},
    {0x07fc0000, {18, 26}}, {0x07fe0000, {17, 26}}, {0x07ff0000, {16, 26}},
    {0x07ff8000, {15, 26}}, {0x07ffc000, {14, 26}}, {0x07ffe000, {13, 26}},
    {0x07fff000, {12, 26}}, {0x07fff800, {11, 26}}, {0x07fffc00, {10, 26}},
    {0x07fffe00, {9, 26}},  {0x07ffff00, {8, 26}},  {0x07ffff80, {7, 26}},
    {0x07ffffc0, {6, 26}},  {0x07ffffe0, {5, 26}},  {0x07fffff0, {4, 26}},
    {0x07fffff8, {3, 26}},  {0x07fffffc, {2, 26}},  {0x07fffffe, {1, 26}},
    {0x07ffffff, {0, 26}},  {0x0e000000, {25, 27}}, {0x0f000000, {24, 27}},
    {0x0f800000, {23, 27}}, {0x0fc00000, {22, 27}}, {0x0fe00000, {21, 27}},
    {0x0ff00000, {20, 27}}, {0x0ff80000, {19, 27}}, {0x0ffc0000, {18, 27}},
    {0x0ffe0000, {17, 27}}, {0x0fff0000, {16, 27}}, {0x0fff8000, {15, 27}},
    {0x0fffc000, {14, 27}}, {0x0fffe000, {13, 27}}, {0x0ffff000, {12, 27}},
    {0x0ffff800, {11, 27}}, {0x0ffffc00, {10, 27}}, {0x0ffffe00, {9, 27}},
    {0x0fffff00, {8, 27}},  {0x0fffff80, {7, 27}},  {0x0fffffc0, {6, 27}},
    {0x0fffffe0, {5, 27}},  {0x0ffffff0, {4, 27}},  {0x0ffffff8, {3, 27}},
    {0x0ffffffc, {2, 27}},  {0x0ffffffe, {1, 27}},  {0x0fffffff, {0, 27}},
    {0x1c000000, {26, 28}}, {0x1e000000, {25, 28}}, {0x1f000000, {24, 28}},
    {0x1f800000, {23, 28}}, {0x1fc00000, {22, 28}}, {0x1fe00000, {21, 28}},
    {0x1ff00000, {20, 28}}, {0x1ff80000, {19, 28}}, {0x1ffc0000, {18, 28}},
    {0x1ffe0000, {17, 28}}, {0x1fff0000, {16, 28}}, {0x1fff8000, {15, 28}},
    {0x1fffc000, {14, 28}}, {0x1fffe000, {13, 28}}, {0x1ffff000, {12, 28}},
    {0x1ffff800, {11, 28}}, {0x1ffffc00, {10, 28}}, {0x1ffffe00, {9, 28}},
    {0x1fffff00, {8, 28}},  {0x1fffff80, {7, 28}},  {0x1fffffc0, {6, 28}},
    {0x1fffffe0, {5, 28}},  {0x1ffffff0, {4, 28}},  {0x1ffffff8, {3, 28}},
    {0x1ffffffc, {2, 28}},  {0x1ffffffe, {1, 28}},  {0x1fffffff, {0, 28}},
    {0x38000000, {27, 29}}, {0x3c000000, {26, 29}}, {0x3e000000, {25, 29}},
    {0x3f000000, {24, 29}}, {0x3f800000, {23, 29}}, {0x3fc00000, {22, 29}},
    {0x3fe00000, {21, 29}}, {0x3ff00000, {20, 29}}, {0x3ff80000, {19, 29}},
    {0x3ffc0000, {18, 29}}, {0x3ffe0000, {17, 29}}, {0x3fff0000, {16, 29}},
    {0x3fff8000, {15, 29}}, {0x3fffc000, {14, 29}}, {0x3fffe000, {13, 29}},
    {0x3ffff000, {12, 29}}, {0x3ffff800, {11, 29}}, {0x3ffffc00, {10, 29}},
    {0x3ffffe00, {9, 29}},  {0x3fffff00, {8, 29}},  {0x3fffff80, {7, 29}},
    {0x3fffffc0, {6, 29}},  {0x3fffffe0, {5, 29}},  {0x3ffffff0, {4, 29}},
    {0x3ffffff8, {3, 29}},  {0x3ffffffc, {2, 29}},  {0x3ffffffe, {1, 29}},
    {0x3fffffff, {0, 29}},  {0x70000000, {28, 30}}, {0x78000000, {27, 30}},
    {0x7c000000, {26, 30}}, {0x7e000000, {25, 30}}, {0x7f000000, {24, 30}},
    {0x7f800000, {23, 30}}, {0x7fc00000, {22, 30}}, {0x7fe00000, {21, 30}},
    {0x7ff00000, {20, 30}}, {0x7ff80000, {19, 30}}, {0x7ffc0000, {18, 30}},
    {0x7ffe0000, {17, 30}}, {0x7fff0000, {16, 30}}, {0x7fff8000, {15, 30}},
    {0x7fffc000, {14, 30}}, {0x7fffe000, {13, 30}}, {0x7ffff000, {12, 30}},
    {0x7ffff800, {11, 30}}, {0x7ffffc00, {10, 30}}, {0x7ffffe00, {9, 30}},
    {0x7fffff00, {8, 30}},  {0x7fffff80, {7, 30}},  {0x7fffffc0, {6, 30}},
    {0x7fffffe0, {5, 30}},  {0x7ffffff0, {4, 30}},  {0x7ffffff8, {3, 30}},
    {0x7ffffffc, {2, 30}},  {0x7ffffffe, {1, 30}},  {0x7fffffff, {0, 30}},
    {0xe0000000, {29, 31}}, {0xf0000000, {28, 31}}, {0xf8000000, {27, 31}},
    {0xfc000000, {26, 31}}, {0xfe000000, {25, 31}}, {0xff000000, {24, 31}},
    {0xff800000, {23, 31}}, {0xffc00000, {22, 31}}, {0xffe00000, {21, 31}},
    {0xfff00000, {20, 31}}, {0xfff80000, {19, 31}}, {0xfffc0000, {18, 31}},
    {0xfffe0000, {17, 31}}, {0xffff0000, {16, 31}}, {0xffff8000, {15, 31}},
    {0xffffc000, {14, 31}}, {0xffffe000, {13, 31}}, {0xfffff000, {12, 31}},
    {0xfffff800, {11, 31}}, {0xfffffc00, {10, 31}}, {0xfffffe00, {9, 31}},
    {0xffffff00, {8, 31}},  {0xffffff80, {7, 31}},  {0xffffffc0, {6, 31}},
    {0xffffffe0, {5, 31}},  {0xfffffff0, {4, 31}},  {0xfffffff8, {3, 31}},
    {0xfffffffc, {2, 31}},  {0xfffffffe, {1, 31}}};

ASMParser::ASMParser(pair<unsigned, unsigned> lineno,
                     unordered_map<Symbol *, vector<IR *>> &funcIRs,
                     vector<Symbol *> &consts, vector<Symbol *> &globalVars,
                     unordered_map<Symbol *, vector<Symbol *>> &localVars) {
  this->isProcessed = false;
  this->labelId = 0;
  this->funcIRs = funcIRs;
  this->consts = consts;
  this->globalVars = globalVars;
  this->localVars = localVars;
  this->startLineno = lineno.first;
  this->stopLineno = lineno.second;
}

ASMParser::~ASMParser() {
  for (unordered_map<Symbol *, vector<ASM *>>::iterator it = funcASMs.begin();
       it != funcASMs.end(); it++)
    for (ASM *a : it->second)
      delete a;
}

int ASMParser::calcCallArgSize(const vector<IR *> &irs) {
  int size = 0;
  for (IR *ir : irs)
    if (ir->type == IR::CALL) {
      int iCnt = 0, fCnt = 0;
      for (Symbol *param : ir->items[0]->symbol->params) {
        if (!param->dimensions.empty() || param->dataType == Symbol::INT)
          iCnt++;
        else
          fCnt++;
      }
      size = max(size, max(iCnt - 4, 0) + max(fCnt - 16, 0));
    }
  return size * 4;
}

bool ASMParser::canBeLoadInSingleInstruction(unsigned imm) {
  if (imm <= 0xffff)
    return true;
  if (isByteShiftImm(imm))
    return true;
  if (isByteShiftImm(~imm))
    return true;
  return false;
}

unordered_map<Symbol *, vector<ASM *>> ASMParser::getFuncASMs() {
  if (!isProcessed)
    parse();
  return funcASMs;
}

unsigned ASMParser::float2Unsigned(float fVal) {
  union {
    float fVal;
    unsigned uVal;
  } unionData;
  unionData.fVal = fVal;
  return unionData.uVal;
}

void ASMParser::loadFromSP(vector<ASM *> &asms, Reg::Type target,
                           unsigned offset) {
  ASM::ASMOpType op = isFloatReg(target) ? ASM::VLDR : ASM::LDR;
  unsigned maxOffset = isFloatReg(target) ? 1020 : 4095;
  if (!offset) {
    asms.push_back(new ASM(op, {new ASMItem(target), new ASMItem(Reg::SP)}));
    return;
  }
  if (offset <= maxOffset) {
    asms.push_back(new ASM(
        op, {new ASMItem(target), new ASMItem(Reg::SP), new ASMItem(offset)}));
    return;
  }
  loadImmToReg(asms, Reg::A4, offset);
  asms.push_back(new ASM(
      op, {new ASMItem(target), new ASMItem(Reg::SP), new ASMItem(Reg::A4)}));
}

void ASMParser::loadImmToReg(vector<ASM *> &asms, Reg::Type reg, float val) {
  unsigned data = float2Unsigned(val);
  int exp = (data >> 23) & 0xff;
  if (exp >= 124 && exp <= 131 && !(data & 0x7ffff)) {
    asms.push_back(new ASM(ASM::VMOV, {new ASMItem(reg), new ASMItem(data)}));
    return;
  }
  loadImmToReg(asms, Reg::A4, data);
  asms.push_back(new ASM(ASM::VMOV, {new ASMItem(reg), new ASMItem(Reg::A4)}));
}

void ASMParser::loadImmToReg(vector<ASM *> &asms, Reg::Type reg, unsigned val) {
  if (val <= 0xffff || isByteShiftImm(val)) {
    asms.push_back(new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(val)}));
    return;
  }
  if (isByteShiftImm(~val)) {
    asms.push_back(new ASM(ASM::MVN, {new ASMItem(reg), new ASMItem(~val)}));
    return;
  }
  asms.push_back(
      new ASM(ASM::MOV, {new ASMItem(reg), new ASMItem(val & 0xffff)}));
  asms.push_back(new ASM(
      ASM::MOVT, {new ASMItem(reg), new ASMItem(((unsigned)val) >> 16)}));
}

void ASMParser::initFrame() {
  savedRegs = 0;
  frameOffset = 0;
  ftemp2Reg.clear();
  itemp2Reg.clear();
  temp2SpillReg.clear();
  spillOffsets.clear();
  offsets.clear();
}

bool ASMParser::isByteShiftImm(unsigned imm) {
  unsigned mask = 0xffffff;
  for (int i = 0; i < 16; i++) {
    if (!(imm & mask))
      return true;
    mask = (mask << 2) | (mask >> 30);
  }
  return false;
}

bool ASMParser::isFloatImm(float imm) {
  unsigned data = float2Unsigned(imm);
  int exp = (data >> 23) & 0xff;
  return exp >= 124 && exp <= 131 && !(data & 0x7ffff);
}

bool ASMParser::isFloatReg(Reg::Type reg) { return reg >= Reg::S0; }

void ASMParser::makeFrame(vector<ASM *> &asms, vector<IR *> &irs,
                          Symbol *func) {
  LinearRegAllocator *allocator = new LinearRegAllocator(irs);
  usedRegNum = allocator->getUsedRegNum();
  itemp2Reg = allocator->getItemp2Reg();
  ftemp2Reg = allocator->getFtemp2Reg();
  temp2SpillReg = allocator->getTemp2SpillReg();
  irs = allocator->getIRs();
  delete allocator;
  int callArgSize = calcCallArgSize(irs);
  for (unordered_map<unsigned, unsigned>::iterator it = temp2SpillReg.begin();
       it != temp2SpillReg.end(); it++)
    spillOffsets[it->first] = callArgSize + it->second * 4;
  saveUsedRegs(asms);
  offsets.clear();
  saveArgRegs(asms, func);
  int localVarSize = 0;
  for (Symbol *localVarSymbol : localVars[func]) {
    int size = 4;
    for (int dimension : localVarSymbol->dimensions)
      size *= dimension;
    localVarSize += size;
    offsets[localVarSymbol] = -localVarSize - savedRegs * 4;
  }
  frameOffset = callArgSize + usedRegNum[2] * 4 + localVarSize + savedRegs * 4;
  if ((frameOffset + (usedRegNum[0] + usedRegNum[1]) * 4) % 8 == 0)
    frameOffset += 4;
  for (unordered_map<Symbol *, int>::iterator it = offsets.begin();
       it != offsets.end(); it++)
    it->second += frameOffset;
  moveFromSP(asms, Reg::SP, (int)savedRegs * 4 - frameOffset);
}

void ASMParser::moveFromSP(vector<ASM *> &asms, Reg::Type target, int offset) {
  if (target == Reg::SP && !offset)
    return;
  if (isByteShiftImm(offset))
    asms.push_back(new ASM(ASM::ADD, {new ASMItem(target), new ASMItem(Reg::SP),
                                      new ASMItem(offset)}));
  else if (isByteShiftImm(-offset))
    asms.push_back(new ASM(ASM::SUB, {new ASMItem(target), new ASMItem(Reg::SP),
                                      new ASMItem(-offset)}));
  else if (canBeLoadInSingleInstruction(offset) ||
           canBeLoadInSingleInstruction(-offset)) {
    loadImmToReg(asms, Reg::A4, (unsigned)offset);
    asms.push_back(new ASM(ASM::ADD, {new ASMItem(target), new ASMItem(Reg::SP),
                                      new ASMItem(Reg::A4)}));
  } else {
    loadImmToReg(asms, Reg::A4, (unsigned)(-offset));
    asms.push_back(new ASM(ASM::SUB, {new ASMItem(target), new ASMItem(Reg::SP),
                                      new ASMItem(Reg::A4)}));
  }
}

void ASMParser::mulRegValue(vector<ASM *> &asms, Reg::Type target,
                            Reg::Type source, int val) {
  if (!val)
    loadImmToReg(asms, target, 0u);
  else if (val == 1)
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(target), new ASMItem(source)}));
  else if (val == -1)
    asms.push_back(new ASM(
        ASM::RSB, {new ASMItem(target), new ASMItem(source), new ASMItem(0)}));
  else if (num2powerMap.find(val) != num2powerMap.end())
    asms.push_back(new ASM(ASM::LSL, {new ASMItem(target), new ASMItem(source),
                                      new ASMItem(num2powerMap[val])}));
  else if (num2power2Map.find(val) != num2power2Map.end() && (val & 0x1) == 0x1)
    asms.push_back(
        new ASM(ASM::ADD,
                {new ASMItem(target), new ASMItem(source), new ASMItem(source),
                 new ASMItem(ASMItem::LSL, num2power2Map[val].second)}));
  else if (num2lineMap.find(val) != num2lineMap.end() &&
           !num2lineMap[val].first)
    asms.push_back(
        new ASM(ASM::RSB,
                {new ASMItem(target), new ASMItem(source), new ASMItem(source),
                 new ASMItem(ASMItem::LSL, num2lineMap[val].second + 1)}));
  else if (num2power2Map.find(val) != num2power2Map.end()) {
    asms.push_back(
        new ASM(ASM::ADD,
                {new ASMItem(target), new ASMItem(source), new ASMItem(source),
                 new ASMItem(ASMItem::LSL, num2power2Map[val].second -
                                               num2power2Map[val].first)}));
    asms.push_back(new ASM(ASM::LSL, {new ASMItem(target), new ASMItem(target),
                                      new ASMItem(num2power2Map[val].first)}));
  } else if (num2lineMap.find(val) != num2lineMap.end()) {
    asms.push_back(
        new ASM(ASM::RSB,
                {new ASMItem(target), new ASMItem(source), new ASMItem(source),
                 new ASMItem(ASMItem::LSL, num2lineMap[val].second -
                                               num2lineMap[val].first + 1)}));
    asms.push_back(new ASM(ASM::LSL, {new ASMItem(target), new ASMItem(target),
                                      new ASMItem(num2lineMap[val].first)}));
  } else {
    loadImmToReg(asms, target, (unsigned)val);
    asms.push_back(new ASM(ASM::MUL, {new ASMItem(target), new ASMItem(source),
                                      new ASMItem(target)}));
  }
}

void ASMParser::parse() {
  isProcessed = true;
  preProcess();
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++)
    funcASMs[it->first] = parseFunc(it->first, it->second);
}

void ASMParser::parseAdd(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP)
    parseAddItempItemp(asms, ir);
  else if (ir->items[1]->type == IRItem::ITEMP &&
           ir->items[2]->type == IRItem::INT)
    parseAddItempInt(asms, ir);
  else if (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::ITEMP) {
    swap(ir->items[1], ir->items[2]);
    parseAddItempInt(asms, ir);
  } else if (ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FTEMP)
    parseAddFtempFtemp(asms, ir);
  else if (ir->items[1]->type == IRItem::FTEMP &&
           ir->items[2]->type == IRItem::FLOAT)
    parseAddFtempFloat(asms, ir);
  else if (ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FTEMP) {
    swap(ir->items[1], ir->items[2]);
    parseAddFtempFloat(asms, ir);
  }
}

void ASMParser::parseAddFtempFloat(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
  if (canBeLoadInSingleInstruction(float2Unsigned(ir->items[2]->fVal)) ||
      !canBeLoadInSingleInstruction(float2Unsigned(-ir->items[2]->fVal))) {
    loadImmToReg(asms, Reg::S2, ir->items[2]->fVal);
    asms.push_back(
        new ASM(ASM::VADD,
                {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
                 new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
                 new ASMItem(Reg::S2)}));
  } else {
    loadImmToReg(asms, Reg::S2, -ir->items[2]->fVal);
    asms.push_back(
        new ASM(ASM::VSUB,
                {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
                 new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
                 new ASMItem(Reg::S2)}));
  }
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseAddFtempFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
       flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::S2, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::VADD,
              {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::S2 : ftemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseAddItempInt(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (isByteShiftImm(ir->items[2]->iVal)) {
    asms.push_back(new ASM(
        ASM::ADD, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ir->items[2]->iVal)}));
  } else if (isByteShiftImm(-ir->items[2]->iVal)) {
    asms.push_back(new ASM(
        ASM::SUB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(-ir->items[2]->iVal)}));
  } else if (canBeLoadInSingleInstruction(ir->items[2]->iVal)) {
    loadImmToReg(asms, Reg::A3, (unsigned)ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::ADD, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A3)}));
  } else if (canBeLoadInSingleInstruction(-ir->items[2]->iVal)) {
    loadImmToReg(asms, Reg::A3, (unsigned)(-ir->items[2]->iVal));
    asms.push_back(new ASM(
        ASM::SUB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A3)}));
  } else {
    loadImmToReg(asms, Reg::A3, (unsigned)ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::ADD, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A3)}));
  }
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseAddItempItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
       flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::ADD,
              {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseB(vector<ASM *> &asms, IR *ir) {
  parseLCmp(asms, ir);
  switch (ir->type) {
  case IR::BEQ:
    asms.push_back(
        new ASM(ASM::B, ASM::EQ,
                {new ASMItem(ASMItem::LABEL, irLabels[ir->items[0]->ir])}));
    break;
  case IR::BGE:
    asms.push_back(
        new ASM(ASM::B, ASM::GE,
                {new ASMItem(ASMItem::LABEL, irLabels[ir->items[0]->ir])}));
    break;
  case IR::BGT:
    asms.push_back(
        new ASM(ASM::B, ASM::GT,
                {new ASMItem(ASMItem::LABEL, irLabels[ir->items[0]->ir])}));
    break;
  case IR::BLE:
    asms.push_back(
        new ASM(ASM::B, ASM::LE,
                {new ASMItem(ASMItem::LABEL, irLabels[ir->items[0]->ir])}));
    break;
  case IR::BLT:
    asms.push_back(
        new ASM(ASM::B, ASM::LT,
                {new ASMItem(ASMItem::LABEL, irLabels[ir->items[0]->ir])}));
    break;
  case IR::BNE:
    asms.push_back(
        new ASM(ASM::B, ASM::NE,
                {new ASMItem(ASMItem::LABEL, irLabels[ir->items[0]->ir])}));
    break;
  default:
    break;
  }
}

void ASMParser::parseCall(vector<ASM *> &asms, IR *ir) {
  unsigned iCnt = 0, fCnt = 0, offset = 0;
  for (unsigned i = 1; i < ir->items.size(); i++) {
    if (ir->items[i]->type == IRItem::ITEMP) {
      if (iCnt < 4) {
        iCnt++;
        continue;
      }
      if (itemp2Reg.find(ir->items[i]->iVal) == itemp2Reg.end()) {
        loadFromSP(asms, Reg::A1, spillOffsets[ir->items[i]->iVal]);
        storeFromSP(asms, Reg::A1, offset);
      } else
        storeFromSP(asms, itemp2Reg[ir->items[i]->iVal], offset);
    } else if (ir->items[i]->type == IRItem::FTEMP) {
      if (fCnt < 16) {
        fCnt++;
        continue;
      }
      if (ftemp2Reg.find(ir->items[i]->iVal) == ftemp2Reg.end()) {
        loadFromSP(asms, Reg::A1, spillOffsets[ir->items[i]->iVal]);
        storeFromSP(asms, Reg::A1, offset);
      } else
        storeFromSP(asms, ftemp2Reg[ir->items[i]->iVal], offset);
    } else if (ir->items[i]->type == IRItem::INT) {
      if (iCnt < 4) {
        iCnt++;
        continue;
      }
      loadImmToReg(asms, Reg::A1, (unsigned)ir->items[i]->iVal);
      storeFromSP(asms, Reg::A1, offset);
    } else if (ir->items[i]->type == IRItem::FLOAT) {
      if (fCnt < 16) {
        fCnt++;
        continue;
      }
      loadImmToReg(asms, Reg::A1, (unsigned)ir->items[i]->iVal);
      storeFromSP(asms, Reg::A1, offset);
    }
    offset += 4;
  }
  iCnt = 0;
  fCnt = 0;
  for (unsigned i = 1; i < ir->items.size(); i++) {
    if (ir->items[i]->type == IRItem::ITEMP) {
      if (iCnt < 4) {
        if (itemp2Reg.find(ir->items[i]->iVal) == itemp2Reg.end())
          loadFromSP(asms, aIRegs[iCnt++], spillOffsets[ir->items[i]->iVal]);
        else
          asms.push_back(
              new ASM(ASM::MOV, {new ASMItem(aIRegs[iCnt++]),
                                 new ASMItem(itemp2Reg[ir->items[i]->iVal])}));
      }
    } else if (ir->items[i]->type == IRItem::FTEMP) {
      if (fCnt < 16) {
        if (ftemp2Reg.find(ir->items[i]->iVal) == ftemp2Reg.end())
          loadFromSP(asms, aFRegs[fCnt++], spillOffsets[ir->items[i]->iVal]);
        else
          asms.push_back(
              new ASM(ASM::VMOV, {new ASMItem(aFRegs[fCnt++]),
                                  new ASMItem(ftemp2Reg[ir->items[i]->iVal])}));
      }
    } else if (ir->items[i]->type == IRItem::INT) {
      if (iCnt < 4)
        loadImmToReg(asms, aIRegs[iCnt++], (unsigned)ir->items[i]->iVal);
    } else if (ir->items[i]->type == IRItem::FLOAT) {
      if (fCnt < 16)
        loadImmToReg(asms, aFRegs[fCnt++], ir->items[i]->fVal);
    }
  }
  if (!ir->items[0]->symbol->name.compare("_sysy_starttime"))
    loadImmToReg(asms, Reg::A1, startLineno);
  if (!ir->items[0]->symbol->name.compare("_sysy_stoptime"))
    loadImmToReg(asms, Reg::A1, stopLineno);
  asms.push_back(new ASM(ASM::BL, {new ASMItem(ir->items[0]->symbol->name)}));
}

void ASMParser::parseCmp(vector<ASM *> &asms, IR *ir) {
  parseLCmp(asms, ir);
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end();
  Reg::Type targetReg = flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal];
  switch (ir->type) {
  case IR::EQ:
    asms.push_back(
        new ASM(ASM::MOV, ASM::EQ, {new ASMItem(targetReg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::NE, {new ASMItem(targetReg), new ASMItem(0)}));
    break;
  case IR::GE:
    asms.push_back(
        new ASM(ASM::MOV, ASM::GE, {new ASMItem(targetReg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::LT, {new ASMItem(targetReg), new ASMItem(0)}));
    break;
  case IR::GT:
    asms.push_back(
        new ASM(ASM::MOV, ASM::GT, {new ASMItem(targetReg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::LE, {new ASMItem(targetReg), new ASMItem(0)}));
    break;
  case IR::LE:
    asms.push_back(
        new ASM(ASM::MOV, ASM::LE, {new ASMItem(targetReg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::GT, {new ASMItem(targetReg), new ASMItem(0)}));
    break;
  case IR::LT:
    asms.push_back(
        new ASM(ASM::MOV, ASM::LT, {new ASMItem(targetReg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::GE, {new ASMItem(targetReg), new ASMItem(0)}));
    break;
  case IR::NE:
    asms.push_back(
        new ASM(ASM::MOV, ASM::NE, {new ASMItem(targetReg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::EQ, {new ASMItem(targetReg), new ASMItem(0)}));
    break;
  default:
    break;
  }
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseDiv(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP)
    parseDivItempItemp(asms, ir);
  else if (ir->items[1]->type == IRItem::ITEMP &&
           ir->items[2]->type == IRItem::INT)
    parseDivItempInt(asms, ir);
  else if (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::ITEMP)
    parseDivIntItemp(asms, ir);
  else if (ir->items[1]->type == IRItem::FTEMP &&
           ir->items[2]->type == IRItem::FTEMP)
    parseDivFtempFtemp(asms, ir);
  else if (ir->items[1]->type == IRItem::FTEMP &&
           ir->items[2]->type == IRItem::FLOAT)
    parseDivFtempFloat(asms, ir);
  else if (ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FTEMP)
    parseDivFloatFtemp(asms, ir);
}

void ASMParser::parseDivFloatFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
  loadImmToReg(asms, Reg::S1, ir->items[1]->fVal);
  if (flag3)
    loadFromSP(asms, Reg::S2, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::VDIV,
              {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
               new ASMItem(Reg::S1),
               new ASMItem(flag3 ? Reg::S2 : ftemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseDivFtempFloat(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  if (ir->items[2]->fVal == 1.0f) {
    if (flag1 && flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    } else if (flag1 && !flag2)
      storeFromSP(asms, ftemp2Reg[ir->items[1]->iVal],
                  spillOffsets[ir->items[0]->iVal]);
    else if (!flag1 && flag2)
      loadFromSP(asms, ftemp2Reg[ir->items[0]->iVal],
                 spillOffsets[ir->items[1]->iVal]);
    else if (!flag1 && !flag2)
      asms.push_back(
          new ASM(ASM::VMOV, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                              new ASMItem(ftemp2Reg[ir->items[0]->iVal])}));
    return;
  }
  if (flag2)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
  if (ir->items[2]->fVal == -1.0f) {
    if (flag1)
      storeFromSP(asms, flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal],
                  spillOffsets[ir->items[0]->iVal]);
    else
      asms.push_back(new ASM(
          ASM::VNEG,
          {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
           new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal])}));
    return;
  }
  loadImmToReg(asms, Reg::S2, ir->items[2]->fVal);
  asms.push_back(new ASM(
      ASM::VDIV, {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
                  new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
                  new ASMItem(Reg::S2)}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseDivFtempFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
       flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::S2, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::VDIV,
              {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::S2 : ftemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseDivIntItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  loadImmToReg(asms, Reg::A2, (unsigned)ir->items[1]->iVal);
  if (flag3)
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::DIV,
              {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
               new ASMItem(Reg::A2),
               new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseDivItempInt(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (ir->items[2]->iVal == 1)
    asms.push_back(new ASM(
        ASM::MOV,
        {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal])}));
  else if (ir->items[2]->iVal == -1)
    asms.push_back(new ASM(
        ASM::RSB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(0)}));
  else if (ir->items[2]->iVal == INT32_MIN)
    asms.push_back(new ASM(
        ASM::MOV, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(0)}));
  else {
    unsigned div = ir->items[2]->iVal;
    bool flag = true;
    if (((int)div) < 0) {
      flag = false;
      div = abs(((int)div));
    }
    unsigned shift = 0;
    while (1ull << (shift + 32) <= (0x7fffffff - 0x80000000 % div) *
                                       (div - (1ull << (shift + 32)) % div))
      shift++;
    unsigned magic = (1ull << (shift + 32)) / div + 1;
    loadImmToReg(asms, flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal], magic);
    if (magic <= 0x7fffffff)
      asms.push_back(new ASM(
          ASM::SMMUL,
          {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
           new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal])}));
    else
      asms.push_back(new ASM(
          ASM::SMMLA,
          {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
           new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal])}));
    if (shift)
      asms.push_back(
          new ASM(ASM::ASR,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(shift)}));
    if (flag && magic <= 0x7fffffff)
      asms.push_back(
          new ASM(ASM::ADD,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ASMItem::LSR, 31)}));
    else if (flag && magic > 0x7fffffff)
      asms.push_back(
          new ASM(ASM::ADD,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ASMItem::LSR, 31)}));
    else if (!flag && magic <= 0x7fffffff)
      asms.push_back(
          new ASM(ASM::RSB,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ASMItem::ASR, 31)}));
    else if (!flag && magic > 0x7fffffff)
      asms.push_back(
          new ASM(ASM::RSB,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(Reg::A2), new ASMItem(ASMItem::ASR, 31)}));
  }
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseDivItempItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
       flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::DIV,
              {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseF2I(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S0, spillOffsets[ir->items[1]->iVal]);
  asms.push_back(
      new ASM(ASM::VCVTFS,
              {new ASMItem(flag2 ? Reg::S0 : ftemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag2 ? Reg::S0 : ftemp2Reg[ir->items[1]->iVal])}));
  asms.push_back(
      new ASM(ASM::VMOV,
              {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag2 ? Reg::S0 : ftemp2Reg[ir->items[1]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

vector<ASM *> ASMParser::parseFunc(Symbol *func, vector<IR *> &irs) {
  vector<ASM *> asms;
  initFrame();
  makeFrame(asms, irs, func);
  for (unsigned i = 0; i < irs.size(); i++) {
    switch (irs[i]->type) {
    case IR::ADD:
      parseAdd(asms, irs[i]);
      break;
    case IR::BEQ:
    case IR::BGE:
    case IR::BGT:
    case IR::BLE:
    case IR::BLT:
    case IR::BNE:
      parseB(asms, irs[i]);
      break;
    case IR::CALL:
      parseCall(asms, irs[i]);
      break;
    case IR::DIV:
      parseDiv(asms, irs[i]);
      break;
    case IR::EQ:
    case IR::GE:
    case IR::GT:
    case IR::LE:
    case IR::LT:
    case IR::NE:
      parseCmp(asms, irs[i]);
      break;
    case IR::F2I:
      parseF2I(asms, irs[i]);
      break;
    case IR::GOTO:
      asms.push_back(new ASM(
          ASM::B,
          {new ASMItem(ASMItem::LABEL, irLabels[irs[i]->items[0]->ir])}));
      break;
    case IR::I2F:
      parseI2F(asms, irs[i]);
      break;
    case IR::L_NOT:
      parseLNot(asms, irs[i]);
      break;
    case IR::LABEL:
      asms.push_back(
          new ASM(ASM::LABEL, {new ASMItem(ASMItem::LABEL, irLabels[irs[i]])}));
      break;
    case IR::MOD:
      parseMod(asms, irs[i]);
      break;
    case IR::MOV:
      parseMov(asms, irs[i]);
      break;
    case IR::MUL:
      parseMul(asms, irs[i]);
      break;
    case IR::NEG:
      parseNeg(asms, irs[i]);
      break;
    case IR::SUB:
      parseSub(asms, irs[i]);
      break;
    default:
      break;
    }
  }
  moveFromSP(asms, Reg::SP, frameOffset);
  popArgs(asms);
  return asms;
}

void ASMParser::parseI2F(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal],
               spillOffsets[ir->items[1]->iVal]);
  else
    asms.push_back(
        new ASM(ASM::VMOV,
                {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
                 new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
  asms.push_back(
      new ASM(ASM::VCVTSF,
              {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseLCmp(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP)
    parseLCmpItempItemp(asms, ir);
  else if (ir->items[1]->type == IRItem::ITEMP &&
           ir->items[2]->type == IRItem::INT)
    parseLCmpItempInt(asms, ir);
  else if (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::ITEMP) {
    switchLCmpLogic(ir);
    parseLCmpItempInt(asms, ir);
  } else if (ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FTEMP)
    parseLCmpFtempFtemp(asms, ir);
  else if (ir->items[1]->type == IRItem::FTEMP &&
           ir->items[2]->type == IRItem::FLOAT)
    parseLCmpFtempFloat(asms, ir);
  else if (ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FTEMP) {
    switchLCmpLogic(ir);
    parseAddFtempFloat(asms, ir);
  }
}

void ASMParser::parseLCmpFtempFloat(vector<ASM *> &asms, IR *ir) {
  bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S0, spillOffsets[ir->items[1]->iVal]);
  if (ir->items[2]->fVal) {
    loadImmToReg(asms, Reg::S1, ir->items[2]->fVal);
    asms.push_back(
        new ASM(ASM::VCMP,
                {new ASMItem(flag2 ? Reg::S0 : ftemp2Reg[ir->items[1]->iVal]),
                 new ASMItem(Reg::S1)}));
  } else
    asms.push_back(
        new ASM(ASM::VCMP,
                {new ASMItem(flag2 ? Reg::S0 : ftemp2Reg[ir->items[1]->iVal]),
                 new ASMItem(0.0f)}));
  asms.push_back(new ASM(ASM::VMRS, {}));
}

void ASMParser::parseLCmpFtempFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
       flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S0, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::VCMP,
              {new ASMItem(flag2 ? Reg::S0 : ftemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::S1 : ftemp2Reg[ir->items[2]->iVal])}));
  asms.push_back(new ASM(ASM::VMRS, {}));
}

void ASMParser::parseLCmpItempInt(vector<ASM *> &asms, IR *ir) {
  bool flag = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag)
    loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
  if (isByteShiftImm(ir->items[2]->iVal))
    asms.push_back(new ASM(
        ASM::CMP, {new ASMItem(flag ? Reg::A1 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ir->items[2]->iVal)}));
  else if (isByteShiftImm(-ir->items[2]->iVal))
    asms.push_back(new ASM(
        ASM::CMN, {new ASMItem(flag ? Reg::A1 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(-ir->items[2]->iVal)}));
  else if (canBeLoadInSingleInstruction(ir->items[2]->iVal) ||
           canBeLoadInSingleInstruction(-ir->items[2]->iVal)) {
    loadImmToReg(asms, Reg::A2, (unsigned)ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::CMP, {new ASMItem(flag ? Reg::A1 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A2)}));
  } else {
    loadImmToReg(asms, Reg::A2, (unsigned)(-ir->items[2]->iVal));
    asms.push_back(new ASM(
        ASM::CMN, {new ASMItem(flag ? Reg::A1 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A2)}));
  }
}

void ASMParser::parseLCmpItempItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  if (flag1)
    loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::CMP,
              {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[2]->iVal])}));
}

void ASMParser::parseLNot(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end();
  switch (ir->items[1]->type) {
  case IRItem::ITEMP: {
    bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    Reg::Type targetReg = flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal];
    if (flag2) {
      loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
      asms.push_back(new ASM(ASM::CMP, {new ASMItem(Reg::A1), new ASMItem(0)}));
    } else
      asms.push_back(
          new ASM(ASM::CMP, {new ASMItem(itemp2Reg[ir->items[1]->iVal]),
                             new ASMItem(0)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::EQ, {new ASMItem(targetReg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::NE, {new ASMItem(targetReg), new ASMItem(0)}));
    if (flag1)
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    break;
  }
  case IRItem::FTEMP: {
    bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    Reg::Type targetReg = flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal];
    if (flag2) {
      loadFromSP(asms, Reg::S0, spillOffsets[ir->items[1]->iVal]);
      asms.push_back(
          new ASM(ASM::VCMP, {new ASMItem(Reg::S0), new ASMItem(0.0f)}));
    } else {
      asms.push_back(
          new ASM(ASM::VCMP, {new ASMItem(ftemp2Reg[ir->items[1]->iVal]),
                              new ASMItem(0.0f)}));
    }
    asms.push_back(new ASM(ASM::VMRS, {}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::EQ, {new ASMItem(targetReg), new ASMItem(1)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::NE, {new ASMItem(targetReg), new ASMItem(0)}));
    if (flag1)
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    break;
  }
  default:
    break;
  }
}

void ASMParser::parseMod(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP)
    parseModItempItemp(asms, ir);
  else if (ir->items[1]->type == IRItem::ITEMP &&
           ir->items[2]->type == IRItem::INT)
    parseModItempInt(asms, ir);
  else if (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::ITEMP)
    parseModIntItemp(asms, ir);
}

void ASMParser::parseModIntItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  loadImmToReg(asms, Reg::A2, (unsigned)ir->items[1]->iVal);
  if (flag3)
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::DIV,
              {new ASMItem(Reg::A1), new ASMItem(Reg::A2),
               new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  asms.push_back(new ASM(ASM::MUL, {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
                                    new ASMItem(Reg::A3)}));
  asms.push_back(
      new ASM(ASM::SUB,
              {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
               new ASMItem(Reg::A2),
               new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseModItempInt(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (ir->items[2]->iVal == INT32_MIN) {
    asms.push_back(new ASM(
        ASM::MOV,
        {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal])}));
    asms.push_back(new ASM(
        ASM::CMP, {new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(INT32_MIN)}));
    asms.push_back(
        new ASM(ASM::MOV, ASM::EQ,
                {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                 new ASMItem(0)}));
  } else if (abs(ir->items[2]->iVal) == 1)
    asms.push_back(new ASM(
        ASM::MOV, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(0)}));
  else {
    unsigned div = abs(ir->items[2]->iVal);
    unsigned shift = 0;
    while (1ull << (shift + 32) <= (0x7fffffff - 0x80000000 % div) *
                                       (div - (1ull << (shift + 32)) % div))
      shift++;
    unsigned magic = (1ull << (shift + 32)) / div + 1;
    loadImmToReg(asms, flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal], magic);
    if (magic <= 0x7fffffff)
      asms.push_back(new ASM(
          ASM::SMMUL,
          {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
           new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal])}));
    else
      asms.push_back(new ASM(
          ASM::SMMLA,
          {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
           new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal])}));
    if (shift)
      asms.push_back(
          new ASM(ASM::ASR,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(shift)}));
    if (magic <= 0x7fffffff)
      asms.push_back(
          new ASM(ASM::ADD,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ASMItem::LSR, 31)}));
    else
      asms.push_back(
          new ASM(ASM::ADD,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ASMItem::LSR, 31)}));
    if (num2power2Map.find(ir->items[2]->iVal) != num2power2Map.end() &&
        (ir->items[2]->iVal & 0x1) == 0x1)
      asms.push_back(
          new ASM(ASM::ADD,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(ASMItem::LSL,
                               num2power2Map[ir->items[2]->iVal].second)}));
    else if (num2power2Map.find(ir->items[2]->iVal) != num2power2Map.end()) {
      asms.push_back(
          new ASM(ASM::ADD,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(ASMItem::LSL,
                               num2power2Map[ir->items[2]->iVal].second -
                                   num2power2Map[ir->items[2]->iVal].first)}));
      asms.push_back(
          new ASM(ASM::LSL,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(num2power2Map[ir->items[2]->iVal].first)}));
    } else if (num2lineMap.find(ir->items[2]->iVal) != num2lineMap.end()) {
      asms.push_back(new ASM(
          ASM::RSB,
          {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem(ASMItem::LSL, num2lineMap[ir->items[2]->iVal].second -
                                         num2lineMap[ir->items[2]->iVal].first +
                                         1)}));
      asms.push_back(
          new ASM(ASM::LSL,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(num2lineMap[ir->items[2]->iVal].first)}));
    } else {
      loadImmToReg(asms, Reg::A3, (unsigned)ir->items[2]->iVal);
      asms.push_back(
          new ASM(ASM::MUL,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(Reg::A3)}));
    }
    asms.push_back(new ASM(
        ASM::SUB,
        {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal])}));
  }
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseModItempItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
       flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::DIV,
              {new ASMItem(Reg::A1),
               new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  asms.push_back(
      new ASM(ASM::MUL,
              {new ASMItem(Reg::A1), new ASMItem(Reg::A1),
               new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  asms.push_back(new ASM(
      ASM::SUB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                 new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                 new ASMItem(Reg::A1)}));
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseMov(vector<ASM *> &asms, IR *ir) {
  if (ir->items[0]->type == IRItem::FTEMP &&
      ir->items[1]->type == IRItem::FLOAT)
    parseMovFtempFloat(asms, ir);
  else if (ir->items[0]->type == IRItem::FTEMP &&
           ir->items[1]->type == IRItem::FTEMP)
    parseMovFtempFtemp(asms, ir);
  else if (ir->items[0]->type == IRItem::FTEMP &&
           ir->items[1]->type == IRItem::RETURN)
    parseMovFtempReturn(asms, ir);
  else if (ir->items[0]->type == IRItem::FTEMP &&
           ir->items[1]->type == IRItem::SYMBOL)
    parseMovFtempSymbol(asms, ir);
  else if (ir->items[0]->type == IRItem::ITEMP &&
           ir->items[1]->type == IRItem::INT)
    parseMovItempInt(asms, ir);
  else if (ir->items[0]->type == IRItem::ITEMP &&
           ir->items[1]->type == IRItem::ITEMP)
    parseMovItempItemp(asms, ir);
  else if (ir->items[0]->type == IRItem::ITEMP &&
           ir->items[1]->type == IRItem::RETURN)
    parseMovItempReturn(asms, ir);
  else if (ir->items[0]->type == IRItem::ITEMP &&
           ir->items[1]->type == IRItem::SYMBOL)
    parseMovItempSymbol(asms, ir);
  else if (ir->items[0]->type == IRItem::RETURN &&
           ir->items[1]->type == IRItem::FLOAT)
    parseMovReturnFloat(asms, ir);
  else if (ir->items[0]->type == IRItem::RETURN &&
           ir->items[1]->type == IRItem::FTEMP)
    parseMovReturnFtemp(asms, ir);
  else if (ir->items[0]->type == IRItem::RETURN &&
           ir->items[1]->type == IRItem::INT)
    parseMovReturnInt(asms, ir);
  else if (ir->items[0]->type == IRItem::RETURN &&
           ir->items[1]->type == IRItem::ITEMP)
    parseMovReturnItemp(asms, ir);
  else if (ir->items[0]->type == IRItem::RETURN &&
           ir->items[1]->type == IRItem::SYMBOL)
    parseMovReturnSymbol(asms, ir);
  else if (ir->items[0]->type == IRItem::SYMBOL &&
           ir->items[1]->type == IRItem::FTEMP)
    parseMovSymbolFtemp(asms, ir);
  else if (ir->items[0]->type == IRItem::SYMBOL &&
           (ir->items[1]->type == IRItem::FLOAT ||
            ir->items[1]->type == IRItem::INT))
    parseMovSymbolImm(asms, ir);
  else if (ir->items[0]->type == IRItem::SYMBOL &&
           ir->items[1]->type == IRItem::ITEMP)
    parseMovSymbolItemp(asms, ir);
  else if (ir->items[0]->type == IRItem::SYMBOL &&
           ir->items[1]->type == IRItem::RETURN)
    parseMovSymbolReturn(asms, ir);
}

void ASMParser::parseMovFtempFloat(vector<ASM *> &asms, IR *ir) {
  if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end()) {
    loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
  } else
    loadImmToReg(asms, ftemp2Reg[ir->items[0]->iVal], ir->items[1]->fVal);
}

void ASMParser::parseMovFtempFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  if (flag1 && flag2) {
    loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (flag1 && !flag2)
    storeFromSP(asms, ftemp2Reg[ir->items[1]->iVal],
                spillOffsets[ir->items[0]->iVal]);
  else if (!flag1 && flag2)
    loadFromSP(asms, ftemp2Reg[ir->items[0]->iVal],
               spillOffsets[ir->items[1]->iVal]);
  else if (!flag1 && !flag2)
    asms.push_back(
        new ASM(ASM::VMOV, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                            new ASMItem(ftemp2Reg[ir->items[1]->iVal])}));
}

void ASMParser::parseMovFtempReturn(vector<ASM *> &asms, IR *ir) {
  if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end())
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
  else
    asms.push_back(
        new ASM(ASM::VMOV, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                            new ASMItem(Reg::S0)}));
}

void ASMParser::parseMovFtempSymbol(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->symbol->dimensions.empty()) {
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end();
    switch (ir->items[1]->symbol->symbolType) {
    case Symbol::CONST:
    case Symbol::GLOBAL_VAR:
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      asms.push_back(
          new ASM(flag1 ? ASM::LDR : ASM::VLDR,
                  {new ASMItem(flag1 ? Reg::A1 : ftemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(Reg::A1)}));
      break;
    case Symbol::LOCAL_VAR:
    case Symbol::PARAM:
      loadFromSP(asms, flag1 ? Reg::A1 : ftemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      break;
    default:
      break;
    }
    if (flag1)
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    return;
  }
  vector<unsigned> sizes({4});
  for (int i = ir->items[1]->symbol->dimensions.size() - 1; i > 0; i--)
    sizes.push_back(sizes.back() * ir->items[1]->symbol->dimensions[i]);
  reverse(sizes.begin(), sizes.end());
  unsigned offset = 0;
  vector<pair<unsigned, unsigned>> temps;
  for (unsigned i = 0; i < ir->items.size() - 2; i++) {
    if (ir->items[i + 2]->type == IRItem::INT)
      offset += ir->items[i + 2]->iVal * sizes[i];
    else
      temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
  }
  switch (ir->items[1]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A2),
                    new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A2),
                    new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
    break;
  case Symbol::LOCAL_VAR:
    moveFromSP(asms, Reg::A2, offsets[ir->items[1]->symbol]);
    break;
  case Symbol::PARAM:
    if (ir->items[1]->symbol->dimensions.empty())
      moveFromSP(asms, Reg::A2, offsets[ir->items[1]->symbol]);
    else
      loadFromSP(asms, Reg::A2, offsets[ir->items[1]->symbol]);
    break;
  default:
    break;
  }
  for (pair<unsigned, unsigned> temp : temps) {
    bool flag3 = itemp2Reg.find(temp.first) == itemp2Reg.end();
    if (flag3)
      loadFromSP(asms, Reg::A3, spillOffsets[temp.first]);
    if (num2powerMap.find(temp.second) == num2powerMap.end()) {
      loadImmToReg(asms, Reg::A4, temp.second);
      asms.push_back(new ASM(
          ASM::MLA, {new ASMItem(Reg::A2),
                     new ASMItem(flag3 ? Reg::A3 : itemp2Reg[temp.first]),
                     new ASMItem(Reg::A4), new ASMItem(Reg::A2)}));
    } else
      asms.push_back(new ASM(
          ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                     new ASMItem(flag3 ? Reg::A3 : itemp2Reg[temp.first]),
                     new ASMItem(ASMItem::LSL, num2powerMap[temp.second])}));
  }
  if (offset) {
    loadImmToReg(asms, Reg::A3, offset);
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                           new ASMItem(Reg::A3)}));
  }
  if (ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end()) {
    asms.push_back(
        new ASM(ASM::LDR, {new ASMItem(Reg::A2), new ASMItem(Reg::A2)}));
    storeFromSP(asms, Reg::A2, spillOffsets[ir->items[0]->iVal]);
  } else
    asms.push_back(
        new ASM(ASM::VLDR, {new ASMItem(ftemp2Reg[ir->items[0]->iVal]),
                            new ASMItem(Reg::A2)}));
}

void ASMParser::parseMovItempInt(vector<ASM *> &asms, IR *ir) {
  if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end()) {
    loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
  } else
    loadImmToReg(asms, itemp2Reg[ir->items[0]->iVal],
                 (unsigned)ir->items[1]->iVal);
}

void ASMParser::parseMovItempItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag1 && flag2) {
    loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (flag1 && !flag2)
    storeFromSP(asms, itemp2Reg[ir->items[1]->iVal],
                spillOffsets[ir->items[0]->iVal]);
  else if (!flag1 && flag2)
    loadFromSP(asms, itemp2Reg[ir->items[0]->iVal],
               spillOffsets[ir->items[1]->iVal]);
  else if (!flag1 && !flag2)
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                           new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
}

void ASMParser::parseMovItempReturn(vector<ASM *> &asms, IR *ir) {
  if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
  else
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                           new ASMItem(Reg::A1)}));
}

void ASMParser::parseMovItempSymbol(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end();
  if (ir->items[1]->symbol->dimensions.empty()) {
    switch (ir->items[1]->symbol->symbolType) {
    case Symbol::CONST:
    case Symbol::GLOBAL_VAR:
      asms.push_back(
          new ASM(ASM::MOVW,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
      asms.push_back(
          new ASM(ASM::MOVT,
                  {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::LDR,
          {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
           new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal])}));
      break;
    case Symbol::LOCAL_VAR:
    case Symbol::PARAM:
      loadFromSP(asms, flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal],
                 offsets[ir->items[1]->symbol]);
      break;
    default:
      break;
    }
    if (flag1)
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
    return;
  }
  vector<unsigned> sizes({4});
  for (int i = ir->items[1]->symbol->dimensions.size() - 1; i > 0; i--)
    sizes.push_back(sizes.back() * ir->items[1]->symbol->dimensions[i]);
  reverse(sizes.begin(), sizes.end());
  unsigned offset = 0;
  vector<pair<unsigned, unsigned>> temps;
  for (unsigned i = 0; i < ir->items.size() - 2; i++) {
    if (ir->items[i + 2]->type == IRItem::INT)
      offset += ir->items[i + 2]->iVal * sizes[i];
    else
      temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
  }
  switch (ir->items[1]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A2),
                    new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A2),
                    new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
    break;
  case Symbol::LOCAL_VAR:
    moveFromSP(asms, Reg::A2, offsets[ir->items[1]->symbol]);
    break;
  case Symbol::PARAM:
    if (ir->items[1]->symbol->dimensions.empty())
      moveFromSP(asms, Reg::A2, offsets[ir->items[1]->symbol]);
    else
      loadFromSP(asms, Reg::A2, offsets[ir->items[1]->symbol]);
    break;
  default:
    break;
  }
  for (pair<unsigned, unsigned> temp : temps) {
    bool flag3 = itemp2Reg.find(temp.first) == itemp2Reg.end();
    if (flag3)
      loadFromSP(asms, Reg::A3, spillOffsets[temp.first]);
    if (num2powerMap.find(temp.second) == num2powerMap.end()) {
      loadImmToReg(asms, Reg::A4, temp.second);
      asms.push_back(new ASM(
          ASM::MLA, {new ASMItem(Reg::A2),
                     new ASMItem(flag3 ? Reg::A3 : itemp2Reg[temp.first]),
                     new ASMItem(Reg::A4), new ASMItem(Reg::A2)}));
    } else
      asms.push_back(new ASM(
          ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                     new ASMItem(flag3 ? Reg::A3 : itemp2Reg[temp.first]),
                     new ASMItem(ASMItem::LSL, num2powerMap[temp.second])}));
  }
  if (offset) {
    loadImmToReg(asms, Reg::A3, offset);
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                           new ASMItem(Reg::A3)}));
  }
  if (ir->items[1]->symbol->dimensions.size() + 2 == ir->items.size())
    asms.push_back(
        new ASM(ASM::LDR, {new ASMItem(Reg::A2), new ASMItem(Reg::A2)}));
  if (itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end())
    storeFromSP(asms, Reg::A2, spillOffsets[ir->items[0]->iVal]);
  else
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(itemp2Reg[ir->items[0]->iVal]),
                           new ASMItem(Reg::A2)}));
}

void ASMParser::parseMovReturnFloat(vector<ASM *> &asms, IR *ir) {
  loadImmToReg(asms, Reg::S0, ir->items[1]->fVal);
}

void ASMParser::parseMovReturnFtemp(vector<ASM *> &asms, IR *ir) {
  if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end())
    loadFromSP(asms, Reg::S0, spillOffsets[ir->items[1]->iVal]);
  else
    asms.push_back(
        new ASM(ASM::VMOV, {new ASMItem(Reg::S0),
                            new ASMItem(ftemp2Reg[ir->items[1]->iVal])}));
}

void ASMParser::parseMovReturnInt(vector<ASM *> &asms, IR *ir) {
  loadImmToReg(asms, Reg::A1, (unsigned)ir->items[1]->iVal);
}

void ASMParser::parseMovReturnItemp(vector<ASM *> &asms, IR *ir) {
  if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end())
    loadFromSP(asms, Reg::A1, spillOffsets[ir->items[1]->iVal]);
  else
    asms.push_back(
        new ASM(ASM::MOV, {new ASMItem(Reg::A1),
                           new ASMItem(itemp2Reg[ir->items[1]->iVal])}));
}

void ASMParser::parseMovReturnSymbol(vector<ASM *> &asms, IR *ir) {
  vector<unsigned> sizes({4});
  for (int i = ir->items[1]->symbol->dimensions.size() - 1; i > 0; i--)
    sizes.push_back(sizes.back() * ir->items[1]->symbol->dimensions[i]);
  reverse(sizes.begin(), sizes.end());
  unsigned offset = 0;
  vector<pair<unsigned, unsigned>> temps;
  for (unsigned i = 0; i < ir->items.size() - 2; i++) {
    if (ir->items[i + 2]->type == IRItem::INT)
      offset += ir->items[i + 2]->iVal * sizes[i];
    else
      temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
  }
  switch (ir->items[1]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A2),
                    new ASMItem("#:lower16:" + ir->items[1]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A2),
                    new ASMItem("#:upper16:" + ir->items[1]->symbol->name)}));
    break;
  case Symbol::LOCAL_VAR:
    moveFromSP(asms, Reg::A2, offsets[ir->items[1]->symbol]);
    break;
  case Symbol::PARAM:
    if (ir->items[1]->symbol->dimensions.empty())
      moveFromSP(asms, Reg::A2, offsets[ir->items[1]->symbol]);
    else
      loadFromSP(asms, Reg::A2, offsets[ir->items[1]->symbol]);
    break;
  default:
    break;
  }
  for (pair<unsigned, unsigned> temp : temps) {
    if (itemp2Reg.find(temp.first) == itemp2Reg.end()) {
      loadFromSP(asms, Reg::A3, spillOffsets[temp.first]);
      loadImmToReg(asms, Reg::A4, temp.second);
      asms.push_back(
          new ASM(ASM::MUL, {new ASMItem(Reg::A3), new ASMItem(Reg::A3),
                             new ASMItem(Reg::A4)}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                             new ASMItem(Reg::A3)}));
    } else {
      loadImmToReg(asms, Reg::A3, temp.second);
      asms.push_back(new ASM(ASM::MUL, {new ASMItem(Reg::A3),
                                        new ASMItem(itemp2Reg[temp.first]),
                                        new ASMItem(Reg::A3)}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                             new ASMItem(Reg::A3)}));
    }
  }
  if (offset) {
    loadImmToReg(asms, Reg::A3, offset);
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                           new ASMItem(Reg::A3)}));
  }
  if (ir->items[1]->symbol->dataType == Symbol::INT)
    asms.push_back(
        new ASM(ASM::LDR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
  else
    asms.push_back(
        new ASM(ASM::VLDR, {new ASMItem(Reg::S0), new ASMItem(Reg::A2)}));
}

void ASMParser::parseMovSymbolFtemp(vector<ASM *> &asms, IR *ir) {
  if (ir->items[0]->symbol->dimensions.empty()) {
    bool flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
    switch (ir->items[0]->symbol->symbolType) {
    case Symbol::CONST:
    case Symbol::GLOBAL_VAR:
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      asms.push_back(
          new ASM(ASM::STR,
                  {new ASMItem(flag2 ? Reg::A2 : ftemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A1)}));
      break;
    case Symbol::LOCAL_VAR:
    case Symbol::PARAM:
      storeFromSP(asms, flag2 ? Reg::A2 : ftemp2Reg[ir->items[1]->iVal],
                  offsets[ir->items[0]->symbol]);
      break;
    default:
      break;
    }
    return;
  }
  vector<unsigned> sizes({4});
  for (int i = ir->items[0]->symbol->dimensions.size() - 1; i > 0; i--)
    sizes.push_back(sizes.back() * ir->items[0]->symbol->dimensions[i]);
  reverse(sizes.begin(), sizes.end());
  unsigned offset = 0;
  vector<pair<unsigned, unsigned>> temps;
  for (unsigned i = 0; i < ir->items.size() - 2; i++) {
    if (ir->items[i + 2]->type == IRItem::INT)
      offset += ir->items[i + 2]->iVal * sizes[i];
    else
      temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
  }
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A2),
                    new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A2),
                    new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
    break;
  case Symbol::LOCAL_VAR:
    moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    break;
  case Symbol::PARAM:
    if (ir->items[0]->symbol->dimensions.empty())
      moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    else
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    break;
  default:
    break;
  }
  for (pair<unsigned, unsigned> temp : temps) {
    bool flag3 = itemp2Reg.find(temp.first) == itemp2Reg.end();
    if (flag3)
      loadFromSP(asms, Reg::A3, spillOffsets[temp.first]);
    if (num2powerMap.find(temp.second) == num2powerMap.end()) {
      loadImmToReg(asms, Reg::A4, temp.second);
      asms.push_back(new ASM(
          ASM::MLA, {new ASMItem(Reg::A2),
                     new ASMItem(flag3 ? Reg::A3 : itemp2Reg[temp.first]),
                     new ASMItem(Reg::A4), new ASMItem(Reg::A2)}));
    } else
      asms.push_back(new ASM(
          ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                     new ASMItem(flag3 ? Reg::A3 : itemp2Reg[temp.first]),
                     new ASMItem(ASMItem::LSL, num2powerMap[temp.second])}));
  }
  if (offset) {
    loadImmToReg(asms, Reg::A3, offset);
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                           new ASMItem(Reg::A3)}));
  }
  if (ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end()) {
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[1]->iVal]);
    asms.push_back(
        new ASM(ASM::STR, {new ASMItem(Reg::A3), new ASMItem(Reg::A2)}));
  } else
    asms.push_back(
        new ASM(ASM::VSTR, {new ASMItem(ftemp2Reg[ir->items[1]->iVal]),
                            new ASMItem(Reg::A2)}));
}

void ASMParser::parseMovSymbolImm(vector<ASM *> &asms, IR *ir) {
  vector<unsigned> sizes({4});
  for (int i = ir->items[0]->symbol->dimensions.size() - 1; i > 0; i--)
    sizes.push_back(sizes.back() * ir->items[0]->symbol->dimensions[i]);
  reverse(sizes.begin(), sizes.end());
  unsigned offset = 0;
  vector<pair<unsigned, unsigned>> temps;
  for (unsigned i = 0; i < ir->items.size() - 2; i++) {
    if (ir->items[i + 2]->type == IRItem::INT)
      offset += ir->items[i + 2]->iVal * sizes[i];
    else
      temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
  }
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A2),
                    new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A2),
                    new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
    break;
  case Symbol::LOCAL_VAR:
    moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    break;
  case Symbol::PARAM:
    if (ir->items[0]->symbol->dimensions.empty())
      moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    else
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    break;
  default:
    break;
  }
  for (pair<unsigned, unsigned> temp : temps) {
    if (itemp2Reg.find(temp.first) == itemp2Reg.end()) {
      loadFromSP(asms, Reg::A3, spillOffsets[temp.first]);
      loadImmToReg(asms, Reg::A4, temp.second);
      asms.push_back(
          new ASM(ASM::MUL, {new ASMItem(Reg::A3), new ASMItem(Reg::A3),
                             new ASMItem(Reg::A4)}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                             new ASMItem(Reg::A3)}));
    } else {
      loadImmToReg(asms, Reg::A3, temp.second);
      asms.push_back(new ASM(ASM::MUL, {new ASMItem(Reg::A3),
                                        new ASMItem(itemp2Reg[temp.first]),
                                        new ASMItem(Reg::A3)}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                             new ASMItem(Reg::A3)}));
    }
  }
  if (offset) {
    loadImmToReg(asms, Reg::A3, offset);
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                           new ASMItem(Reg::A3)}));
  }
  loadImmToReg(asms, Reg::A3, (unsigned)ir->items[1]->iVal);
  asms.push_back(
      new ASM(ASM::STR, {new ASMItem(Reg::A3), new ASMItem(Reg::A2)}));
}

void ASMParser::parseMovSymbolItemp(vector<ASM *> &asms, IR *ir) {
  bool flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (ir->items[0]->symbol->dimensions.empty()) {
    if (flag2)
      loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
    switch (ir->items[0]->symbol->symbolType) {
    case Symbol::CONST:
    case Symbol::GLOBAL_VAR:
      asms.push_back(new ASM(
          ASM::MOVW, {new ASMItem(Reg::A1),
                      new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
      asms.push_back(new ASM(
          ASM::MOVT, {new ASMItem(Reg::A1),
                      new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
      asms.push_back(
          new ASM(ASM::STR,
                  {new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A1)}));
      break;
    case Symbol::LOCAL_VAR:
    case Symbol::PARAM:
      storeFromSP(asms, flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal],
                  offsets[ir->items[0]->symbol]);
      break;
    default:
      break;
    }
    return;
  }
  vector<unsigned> sizes({4});
  for (int i = ir->items[0]->symbol->dimensions.size() - 1; i > 0; i--)
    sizes.push_back(sizes.back() * ir->items[0]->symbol->dimensions[i]);
  reverse(sizes.begin(), sizes.end());
  unsigned offset = 0;
  vector<pair<unsigned, unsigned>> temps;
  for (unsigned i = 0; i < ir->items.size() - 2; i++) {
    if (ir->items[i + 2]->type == IRItem::INT)
      offset += ir->items[i + 2]->iVal * sizes[i];
    else
      temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
  }
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A2),
                    new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A2),
                    new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
    break;
  case Symbol::LOCAL_VAR:
    moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    break;
  case Symbol::PARAM:
    if (ir->items[0]->symbol->dimensions.empty())
      moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    else
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    break;
  default:
    break;
  }
  for (pair<unsigned, unsigned> temp : temps) {
    bool flag3 = itemp2Reg.find(temp.first) == itemp2Reg.end();
    if (flag3)
      loadFromSP(asms, Reg::A3, spillOffsets[temp.first]);
    if (num2powerMap.find(temp.second) == num2powerMap.end()) {
      loadImmToReg(asms, Reg::A4, temp.second);
      asms.push_back(new ASM(
          ASM::MLA, {new ASMItem(Reg::A2),
                     new ASMItem(flag3 ? Reg::A3 : itemp2Reg[temp.first]),
                     new ASMItem(Reg::A4), new ASMItem(Reg::A2)}));
    } else
      asms.push_back(new ASM(
          ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                     new ASMItem(flag3 ? Reg::A3 : itemp2Reg[temp.first]),
                     new ASMItem(ASMItem::LSL, num2powerMap[temp.second])}));
  }
  if (offset) {
    loadImmToReg(asms, Reg::A3, offset);
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                           new ASMItem(Reg::A3)}));
  }
  if (itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end()) {
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[1]->iVal]);
    asms.push_back(
        new ASM(ASM::STR, {new ASMItem(Reg::A3), new ASMItem(Reg::A2)}));
  } else
    asms.push_back(
        new ASM(ASM::STR, {new ASMItem(itemp2Reg[ir->items[1]->iVal]),
                           new ASMItem(Reg::A2)}));
}

void ASMParser::parseMovSymbolReturn(vector<ASM *> &asms, IR *ir) {
  vector<unsigned> sizes({4});
  for (int i = ir->items[0]->symbol->dimensions.size() - 1; i > 0; i--)
    sizes.push_back(sizes.back() * ir->items[0]->symbol->dimensions[i]);
  reverse(sizes.begin(), sizes.end());
  unsigned offset = 0;
  vector<pair<unsigned, unsigned>> temps;
  for (unsigned i = 0; i < ir->items.size() - 2; i++) {
    if (ir->items[i + 2]->type == IRItem::INT)
      offset += ir->items[i + 2]->iVal * sizes[i];
    else
      temps.emplace_back(ir->items[i + 2]->iVal, sizes[i]);
  }
  switch (ir->items[0]->symbol->symbolType) {
  case Symbol::CONST:
  case Symbol::GLOBAL_VAR:
    asms.push_back(new ASM(
        ASM::MOVW, {new ASMItem(Reg::A2),
                    new ASMItem("#:lower16:" + ir->items[0]->symbol->name)}));
    asms.push_back(new ASM(
        ASM::MOVT, {new ASMItem(Reg::A2),
                    new ASMItem("#:upper16:" + ir->items[0]->symbol->name)}));
    break;
  case Symbol::LOCAL_VAR:
    moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    break;
  case Symbol::PARAM:
    if (ir->items[0]->symbol->dimensions.empty())
      moveFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    else
      loadFromSP(asms, Reg::A2, offsets[ir->items[0]->symbol]);
    break;
  default:
    break;
  }
  for (pair<unsigned, unsigned> temp : temps) {
    if (itemp2Reg.find(temp.first) == itemp2Reg.end()) {
      loadFromSP(asms, Reg::A3, spillOffsets[temp.first]);
      loadImmToReg(asms, Reg::A4, temp.second);
      asms.push_back(
          new ASM(ASM::MUL, {new ASMItem(Reg::A3), new ASMItem(Reg::A3),
                             new ASMItem(Reg::A4)}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                             new ASMItem(Reg::A3)}));
    } else {
      loadImmToReg(asms, Reg::A3, temp.second);
      asms.push_back(new ASM(ASM::MUL, {new ASMItem(Reg::A3),
                                        new ASMItem(itemp2Reg[temp.first]),
                                        new ASMItem(Reg::A3)}));
      asms.push_back(
          new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                             new ASMItem(Reg::A3)}));
    }
  }
  if (offset) {
    loadImmToReg(asms, Reg::A3, offset);
    asms.push_back(
        new ASM(ASM::ADD, {new ASMItem(Reg::A2), new ASMItem(Reg::A2),
                           new ASMItem(Reg::A3)}));
  }
  if (ir->items[0]->symbol->dataType == Symbol::FLOAT)
    asms.push_back(
        new ASM(ASM::VSTR, {new ASMItem(Reg::S0), new ASMItem(Reg::A2)}));
  else
    asms.push_back(
        new ASM(ASM::STR, {new ASMItem(Reg::A1), new ASMItem(Reg::A2)}));
}

void ASMParser::parseMul(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP)
    parseMulItempItemp(asms, ir);
  else if (ir->items[1]->type == IRItem::ITEMP &&
           ir->items[2]->type == IRItem::INT)
    parseMulItempInt(asms, ir);
  else if (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::ITEMP) {
    swap(ir->items[1], ir->items[2]);
    parseMulItempInt(asms, ir);
  } else if (ir->items[1]->type == IRItem::FTEMP &&
             ir->items[2]->type == IRItem::FTEMP)
    parseMulFtempFtemp(asms, ir);
  else if (ir->items[1]->type == IRItem::FTEMP &&
           ir->items[2]->type == IRItem::FLOAT)
    parseMulFtempFloat(asms, ir);
  else if (ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FTEMP)
    parseMulFloatFtemp(asms, ir);
}

void ASMParser::parseMulFloatFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
  loadImmToReg(asms, Reg::S1, ir->items[1]->fVal);
  if (flag3)
    loadFromSP(asms, Reg::S2, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::VMUL,
              {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
               new ASMItem(Reg::S1),
               new ASMItem(flag3 ? Reg::S2 : ftemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseMulFtempFloat(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
  loadImmToReg(asms, Reg::S2, ir->items[2]->fVal);
  asms.push_back(new ASM(
      ASM::VMUL, {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
                  new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
                  new ASMItem(Reg::S2)}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseMulFtempFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
       flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::S2, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::VMUL,
              {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::S2 : ftemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseMulItempInt(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (!ir->items[2]->iVal)
    loadImmToReg(asms, flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal], 0u);
  else if (ir->items[2]->iVal == 1)
    asms.push_back(new ASM(
        ASM::MOV,
        {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal])}));
  else if (ir->items[2]->iVal == -1)
    asms.push_back(new ASM(
        ASM::RSB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(0)}));
  else if (num2powerMap.find(ir->items[2]->iVal) != num2powerMap.end())
    asms.push_back(new ASM(
        ASM::LSL, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(num2powerMap[ir->items[2]->iVal])}));
  else if (num2power2Map.find(ir->items[2]->iVal) != num2power2Map.end() &&
           (ir->items[2]->iVal & 0x1) == 0x1)
    asms.push_back(new ASM(
        ASM::ADD,
        {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
         new ASMItem(ASMItem::LSL, num2power2Map[ir->items[2]->iVal].second)}));
  else if (num2lineMap.find(ir->items[2]->iVal) != num2lineMap.end() &&
           !num2lineMap[ir->items[2]->iVal].first)
    asms.push_back(new ASM(
        ASM::RSB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ASMItem::LSL,
                               num2lineMap[ir->items[2]->iVal].second + 1)}));
  else if (num2power2Map.find(ir->items[2]->iVal) != num2power2Map.end()) {
    asms.push_back(new ASM(
        ASM::ADD, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ASMItem::LSL,
                               num2power2Map[ir->items[2]->iVal].second -
                                   num2power2Map[ir->items[2]->iVal].first)}));
    asms.push_back(new ASM(
        ASM::LSL, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(num2power2Map[ir->items[2]->iVal].first)}));
  } else if (num2lineMap.find(ir->items[2]->iVal) != num2lineMap.end()) {
    asms.push_back(
        new ASM(ASM::RSB,
                {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                 new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                 new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                 new ASMItem(ASMItem::LSL,
                             num2lineMap[ir->items[2]->iVal].second -
                                 num2lineMap[ir->items[2]->iVal].first + 1)}));
    asms.push_back(new ASM(
        ASM::LSL, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(num2lineMap[ir->items[2]->iVal].first)}));
  } else {
    loadImmToReg(asms, Reg::A3, (unsigned)ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::MUL, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A3)}));
  }
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseMulItempItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
       flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::MUL,
              {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseNeg(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP) {
    bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
         flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
    if (flag2)
      loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
    asms.push_back(new ASM(
        ASM::RSB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(0)}));
    if (flag1)
      storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
  } else if (ir->items[1]->type == IRItem::FTEMP) {
    bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
         flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
    if (flag2)
      loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
    asms.push_back(new ASM(
        ASM::VNEG,
        {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
         new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal])}));
    if (flag1)
      storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
  }
}

void ASMParser::parseSub(vector<ASM *> &asms, IR *ir) {
  if (ir->items[1]->type == IRItem::ITEMP &&
      ir->items[2]->type == IRItem::ITEMP)
    parseSubItempItemp(asms, ir);
  else if (ir->items[1]->type == IRItem::ITEMP &&
           ir->items[2]->type == IRItem::INT)
    parseSubItempInt(asms, ir);
  else if (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::ITEMP)
    parseSubIntItemp(asms, ir);
  else if (ir->items[1]->type == IRItem::FTEMP &&
           ir->items[2]->type == IRItem::FTEMP)
    parseSubFtempFtemp(asms, ir);
  else if (ir->items[1]->type == IRItem::FTEMP &&
           ir->items[2]->type == IRItem::FLOAT)
    parseSubFtempFloat(asms, ir);
  else if (ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FTEMP)
    parseSubFloatFtemp(asms, ir);
}

void ASMParser::parseSubFloatFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
  loadImmToReg(asms, Reg::S1, ir->items[1]->fVal);
  if (flag3)
    loadFromSP(asms, Reg::S2, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::VSUB,
              {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
               new ASMItem(Reg::S1),
               new ASMItem(flag3 ? Reg::S2 : ftemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseSubFtempFloat(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
  loadImmToReg(asms, Reg::S2, ir->items[2]->fVal);
  asms.push_back(new ASM(
      ASM::VSUB, {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
                  new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
                  new ASMItem(Reg::S2)}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseSubFtempFtemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = ftemp2Reg.find(ir->items[0]->iVal) == ftemp2Reg.end(),
       flag2 = ftemp2Reg.find(ir->items[1]->iVal) == ftemp2Reg.end(),
       flag3 = ftemp2Reg.find(ir->items[2]->iVal) == ftemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::S1, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::S2, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::VSUB,
              {new ASMItem(flag1 ? Reg::S0 : ftemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag2 ? Reg::S1 : ftemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::S2 : ftemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::S0, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseSubIntItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  if (flag3)
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[2]->iVal]);
  if (isByteShiftImm(ir->items[1]->iVal)) {
    asms.push_back(new ASM(
        ASM::RSB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal]),
                   new ASMItem(ir->items[1]->iVal)}));
  } else if (canBeLoadInSingleInstruction(ir->items[1]->iVal)) {
    loadImmToReg(asms, Reg::A2, (unsigned)ir->items[1]->iVal);
    asms.push_back(new ASM(
        ASM::SUB,
        {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(Reg::A2),
         new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  } else if (canBeLoadInSingleInstruction(-ir->items[1]->iVal)) {
    loadImmToReg(asms, Reg::A2, (unsigned)(-ir->items[1]->iVal));
    asms.push_back(new ASM(
        ASM::ADD,
        {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(Reg::A2),
         new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  } else {
    loadImmToReg(asms, Reg::A3, (unsigned)ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::SUB,
        {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
         new ASMItem(Reg::A2),
         new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  }
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseSubItempInt(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (isByteShiftImm(ir->items[2]->iVal)) {
    asms.push_back(new ASM(
        ASM::SUB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(ir->items[2]->iVal)}));
  } else if (isByteShiftImm(-ir->items[2]->iVal)) {
    asms.push_back(new ASM(
        ASM::ADD, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(-ir->items[2]->iVal)}));
  } else if (canBeLoadInSingleInstruction(ir->items[2]->iVal)) {
    loadImmToReg(asms, Reg::A3, (unsigned)ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::SUB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A3)}));
  } else if (canBeLoadInSingleInstruction(-ir->items[2]->iVal)) {
    loadImmToReg(asms, Reg::A3, (unsigned)(-ir->items[2]->iVal));
    asms.push_back(new ASM(
        ASM::ADD, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A3)}));
  } else {
    loadImmToReg(asms, Reg::A3, (unsigned)ir->items[2]->iVal);
    asms.push_back(new ASM(
        ASM::SUB, {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
                   new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
                   new ASMItem(Reg::A3)}));
  }
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::parseSubItempItemp(vector<ASM *> &asms, IR *ir) {
  bool flag1 = itemp2Reg.find(ir->items[0]->iVal) == itemp2Reg.end(),
       flag2 = itemp2Reg.find(ir->items[1]->iVal) == itemp2Reg.end(),
       flag3 = itemp2Reg.find(ir->items[2]->iVal) == itemp2Reg.end();
  if (flag2)
    loadFromSP(asms, Reg::A2, spillOffsets[ir->items[1]->iVal]);
  if (flag3)
    loadFromSP(asms, Reg::A3, spillOffsets[ir->items[2]->iVal]);
  asms.push_back(
      new ASM(ASM::SUB,
              {new ASMItem(flag1 ? Reg::A1 : itemp2Reg[ir->items[0]->iVal]),
               new ASMItem(flag2 ? Reg::A2 : itemp2Reg[ir->items[1]->iVal]),
               new ASMItem(flag3 ? Reg::A3 : itemp2Reg[ir->items[2]->iVal])}));
  if (flag1)
    storeFromSP(asms, Reg::A1, spillOffsets[ir->items[0]->iVal]);
}

void ASMParser::popArgs(vector<ASM *> &asms) {
  ASM *tempASM = new ASM(ASM::VPOP, {});
  for (unsigned i = 0; i < usedRegNum[1]; i++)
    tempASM->items.push_back(new ASMItem(vFRegs[i]));
  if (tempASM->items.empty())
    delete tempASM;
  else
    asms.push_back(tempASM);
  tempASM = new ASM(ASM::POP, {});
  for (unsigned i = 0; i < usedRegNum[0]; i++)
    tempASM->items.push_back(new ASMItem(vIRegs[i]));
  tempASM->items.push_back(new ASMItem(Reg::PC));
  asms.push_back(tempASM);
}

void ASMParser::preProcess() {
  if (!getenv("DRM")) {
    int id = 0;
    for (Symbol *symbol : consts)
      symbol->name = "var" + to_string(id++);
    for (Symbol *symbol : globalVars)
      symbol->name = "var" + to_string(id++);
    id = 0;
    for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
         it != funcIRs.end(); it++)
      if (it->first->name.compare("main"))
        it->first->name = "f" + to_string(id++);
  }
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++)
    for (IR *ir : it->second)
      if (ir->type == IR::LABEL)
        irLabels[ir] = labelId++;
}

void ASMParser::saveArgRegs(vector<ASM *> &asms, Symbol *func) {
  unsigned iCnt = 0, fCnt = 0, offset = 0;
  for (unsigned i = 0; i < func->params.size(); i++) {
    if (!func->params[i]->dimensions.empty() ||
        func->params[i]->dataType == Symbol::INT) {
      if (iCnt < 4) {
        asms.push_back(new ASM(ASM::PUSH, {new ASMItem(aIRegs[iCnt++])}));
        offsets[func->params[i]] = -4 * (min(iCnt, 4u) + min(fCnt, 16u));
        savedRegs++;
      } else {
        offsets[func->params[i]] =
            offset + (usedRegNum[0] + usedRegNum[1] + 1) * 4;
        offset += 4;
      }
    } else {
      if (fCnt < 16) {
        asms.push_back(new ASM(ASM::VPUSH, {new ASMItem(aFRegs[fCnt++])}));
        offsets[func->params[i]] = -4 * (min(iCnt, 4u) + min(fCnt, 16u));
        savedRegs++;
      } else {
        offsets[func->params[i]] =
            offset + (usedRegNum[0] + usedRegNum[1] + 1) * 4;
        offset += 4;
      }
    }
  }
}

void ASMParser::saveUsedRegs(vector<ASM *> &asms) {
  ASM *tempASM = new ASM(ASM::PUSH, {});
  for (unsigned i = 0; i < usedRegNum[0]; i++)
    tempASM->items.push_back(new ASMItem(vIRegs[i]));
  tempASM->items.push_back(new ASMItem(Reg::LR));
  asms.push_back(tempASM);
  tempASM = new ASM(ASM::VPUSH, {});
  for (unsigned i = 0; i < usedRegNum[1]; i++)
    tempASM->items.push_back(new ASMItem(vFRegs[i]));
  if (tempASM->items.empty())
    delete tempASM;
  else
    asms.push_back(tempASM);
}

void ASMParser::storeFromSP(vector<ASM *> &asms, Reg::Type source,
                            unsigned offset) {
  ASM::ASMOpType op = isFloatReg(source) ? ASM::VSTR : ASM::STR;
  unsigned maxOffset = isFloatReg(source) ? 1020 : 4095;
  if (!offset) {
    asms.push_back(new ASM(op, {new ASMItem(source), new ASMItem(Reg::SP)}));
    return;
  }
  if (offset <= maxOffset) {
    asms.push_back(new ASM(
        op, {new ASMItem(source), new ASMItem(Reg::SP), new ASMItem(offset)}));
    return;
  }
  loadImmToReg(asms, Reg::A4, offset);
  asms.push_back(new ASM(
      op, {new ASMItem(source), new ASMItem(Reg::SP), new ASMItem(Reg::A4)}));
}

void ASMParser::switchLCmpLogic(IR *ir) {
  switch (ir->type) {
  case IR::BEQ:
    ir->type = IR::BEQ;
    break;
  case IR::BGE:
    ir->type = IR::BLE;
    break;
  case IR::BGT:
    ir->type = IR::BLT;
    break;
  case IR::BLE:
    ir->type = IR::BGE;
    break;
  case IR::BLT:
    ir->type = IR::BGT;
    break;
  case IR::BNE:
    ir->type = IR::BNE;
    break;
  case IR::EQ:
    ir->type = IR::EQ;
    break;
  case IR::GE:
    ir->type = IR::LE;
    break;
  case IR::GT:
    ir->type = IR::LT;
    break;
  case IR::LE:
    ir->type = IR::GE;
    break;
  case IR::LT:
    ir->type = IR::GT;
    break;
  case IR::NE:
    ir->type = IR::NE;
    break;
  default:
    break;
  }
  swap(ir->items[1], ir->items[2]);
}
