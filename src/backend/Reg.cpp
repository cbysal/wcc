#include "Reg.h"

using namespace std;

unordered_map<Reg::Type, string> regTypeStr = {
    {Reg::A1, "a1"},   {Reg::A2, "a2"},   {Reg::A3, "a3"},   {Reg::A4, "a4"},
    {Reg::V1, "v1"},   {Reg::V2, "v2"},   {Reg::V3, "v3"},   {Reg::V4, "v4"},
    {Reg::V5, "v5"},   {Reg::V6, "v6"},   {Reg::V7, "v7"},   {Reg::V8, "v8"},
    {Reg::IP, "ip"},   {Reg::SP, "sp"},   {Reg::LR, "lr"},   {Reg::PC, "pc"},
    {Reg::S0, "s0"},   {Reg::S1, "s1"},   {Reg::S2, "s2"},   {Reg::S3, "s3"},
    {Reg::S4, "s4"},   {Reg::S5, "s5"},   {Reg::S6, "s6"},   {Reg::S7, "s7"},
    {Reg::S8, "s8"},   {Reg::S9, "s9"},   {Reg::S10, "s10"}, {Reg::S11, "s11"},
    {Reg::S12, "s12"}, {Reg::S13, "s13"}, {Reg::S14, "s14"}, {Reg::S15, "s15"},
    {Reg::S16, "s16"}, {Reg::S17, "s17"}, {Reg::S18, "s18"}, {Reg::S19, "s19"},
    {Reg::S20, "s20"}, {Reg::S21, "s21"}, {Reg::S22, "s22"}, {Reg::S23, "s23"},
    {Reg::S24, "s24"}, {Reg::S25, "s25"}, {Reg::S26, "s26"}, {Reg::S27, "s27"},
    {Reg::S28, "s28"}, {Reg::S29, "s29"}, {Reg::S30, "s30"}, {Reg::S31, "s31"}};
