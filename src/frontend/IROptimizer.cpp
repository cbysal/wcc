#include <algorithm>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <unordered_set>

#include "../GlobalData.h"
#include "IROptimizer.h"

using namespace std;

void IROptimizer::assignPass() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    vector<IR *> &irs = it->second;
    unsigned left = 0, right = 0;
    while (right < irs.size()) {
      while (right < irs.size()) {
        if (irs[right]->type == IR::BEQ || irs[right]->type == IR::BGE ||
            irs[right]->type == IR::BGT || irs[right]->type == IR::BLE ||
            irs[right]->type == IR::BLT || irs[right]->type == IR::BNE ||
            irs[right]->type == IR::CALL || irs[right]->type == IR::GOTO ||
            irs[right]->type == IR::LABEL)
          break;
        right++;
      }
      if (right < irs.size())
        right++;
      unordered_map<unsigned, unsigned> itempAssign;
      unordered_map<unsigned, unsigned> ftempAssign;
      for (unsigned i = left; i < right; i++) {
        for (unsigned j = 1; j < irs[i]->items.size(); j++) {
          if (irs[i]->items[j]->type == IRItem::FTEMP &&
              ftempAssign.find(irs[i]->items[j]->iVal) != ftempAssign.end())
            irs[i]->items[j]->iVal = ftempAssign[irs[i]->items[j]->iVal];
          else if (irs[i]->items[j]->type == IRItem::ITEMP &&
                   itempAssign.find(irs[i]->items[j]->iVal) !=
                       itempAssign.end())
            irs[i]->items[j]->iVal = itempAssign[irs[i]->items[j]->iVal];
        }
        if (!irs[i]->items.empty()) {
          if (irs[i]->items[0]->type == IRItem::FTEMP) {
            if (irs[i]->type == IR::MOV &&
                irs[i]->items[1]->type == IRItem::FTEMP) {
              ftempAssign[irs[i]->items[0]->iVal] = irs[i]->items[1]->iVal;
            } else
              ftempAssign.erase(irs[i]->items[0]->iVal);
            unordered_set<unsigned> eraseSet;
            for (unordered_map<unsigned, unsigned>::iterator it =
                     ftempAssign.begin();
                 it != ftempAssign.end(); it++) {
              if (it->second == (unsigned)irs[i]->items[0]->iVal)
                eraseSet.insert(it->first);
            }
            for (unsigned eraseItem : eraseSet)
              ftempAssign.erase(eraseItem);
          } else if (irs[i]->items[0]->type == IRItem::ITEMP) {
            if (irs[i]->type == IR::MOV &&
                irs[i]->items[1]->type == IRItem::ITEMP) {
              itempAssign[irs[i]->items[0]->iVal] = irs[i]->items[1]->iVal;
            } else
              itempAssign.erase(irs[i]->items[0]->iVal);
            unordered_set<unsigned> eraseSet;
            for (unordered_map<unsigned, unsigned>::iterator it =
                     itempAssign.begin();
                 it != itempAssign.end(); it++) {
              if (it->second == (unsigned)irs[i]->items[0]->iVal)
                eraseSet.insert(it->first);
            }
            for (unsigned eraseItem : eraseSet)
              itempAssign.erase(eraseItem);
          }
        }
      }
      left = right;
    }
    standardize(irs);
  }
}

void IROptimizer::constLoopExpand() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    vector<IR *> &irs = it->second;
    for (unsigned i = 0; i < irs.size(); i++) {
      unordered_map<IR *, unsigned> irIdMap;
      for (unsigned i = 0; i < irs.size(); i++)
        irIdMap[irs[i]] = i;
      if (irs[i]->type == IR::BEQ || irs[i]->type == IR::BGE ||
          irs[i]->type == IR::BGT || irs[i]->type == IR::BLE ||
          irs[i]->type == IR::BLT || irs[i]->type == IR::BNE) {
        unsigned beginId = irIdMap[irs[i]->items[0]->ir];
        unsigned endId = i;
        if (beginId >= endId)
          continue;
        if (irs[endId]->items[2]->type != IRItem::INT)
          continue;
        int loopTempId = irs[endId]->items[1]->iVal;
        int loopTempDefId = beginId - 1;
        while (loopTempDefId >= 0) {
          bool flag = false;
          for (IRItem *item : irs[loopTempDefId]->items)
            if (item->type == IRItem::ITEMP && item->iVal == loopTempId) {
              flag = true;
              break;
            }
          if (flag)
            break;
          loopTempDefId--;
        }
        if (loopTempDefId < 0)
          continue;
        if (irs[loopTempDefId]->type != IR::MOV ||
            irs[loopTempDefId]->items[0]->type != IRItem::ITEMP ||
            irs[loopTempDefId]->items[0]->iVal != loopTempId ||
            irs[loopTempDefId]->items[1]->type != IRItem::INT)
          continue;
        int loopVal = irs[loopTempDefId]->items[1]->iVal;
        bool flag = true;
        for (unsigned j = loopTempDefId + 1; j < endId; j++) {
          if (j == beginId)
            continue;
          if (irs[j]->type == IR::BEQ || irs[j]->type == IR::BGE ||
              irs[j]->type == IR::BGT || irs[j]->type == IR::BLE ||
              irs[j]->type == IR::BLT || irs[j]->type == IR::BNE ||
              irs[j]->type == IR::GOTO || irs[j]->type == IR::LABEL) {
            flag = false;
            break;
          }
          if (irs[j]->items[0]->type == IRItem::ITEMP &&
              irs[j]->items[0]->iVal == loopTempId) {
            for (unsigned k = 1; k < irs[j]->items.size(); k++) {
              if (irs[j]->items[k]->type == IRItem::ITEMP &&
                  irs[j]->items[k]->iVal != loopTempId) {
                flag = false;
                break;
              }
            }
          }
          if (!flag)
            break;
        }
        if (!flag)
          continue;
        vector<IR *> extraIRs;
        unsigned curId = beginId + 1;
        bool toContinue = true;
        bool emergency = false;
        do {
          switch (irs[curId]->type) {
          case IR::ADD:
            if (irs[curId]->items[0]->iVal == loopTempId) {
              loopVal += irs[curId]->items[2]->iVal;
              extraIRs.push_back(
                  new IR(IR::MOV, {new IRItem(IRItem::ITEMP, loopTempId),
                                   new IRItem(loopVal)}));
            } else
              extraIRs.push_back(irs[curId]->clone());
            break;
          case IR::BEQ:
            if (loopVal == irs[curId]->items[2]->iVal)
              curId = beginId;
            else
              toContinue = false;
            break;
          case IR::BGE:
            if (loopVal >= irs[curId]->items[2]->iVal)
              curId = beginId;
            else
              toContinue = false;
            break;
          case IR::BGT:
            if (loopVal > irs[curId]->items[2]->iVal)
              curId = beginId;
            else
              toContinue = false;
            break;
          case IR::BLE:
            if (loopVal <= irs[curId]->items[2]->iVal)
              curId = beginId;
            else
              toContinue = false;
            break;
          case IR::BLT:
            if (loopVal < irs[curId]->items[2]->iVal)
              curId = beginId;
            else
              toContinue = false;
            break;
          case IR::BNE:
            if (loopVal != irs[curId]->items[2]->iVal)
              curId = beginId;
            else
              toContinue = false;
            break;
          case IR::DIV:
            if (irs[curId]->items[0]->iVal == loopTempId) {
              loopVal /= irs[curId]->items[2]->iVal;
              extraIRs.push_back(
                  new IR(IR::MOV, {new IRItem(IRItem::ITEMP, loopTempId),
                                   new IRItem(loopVal)}));
            } else
              extraIRs.push_back(irs[curId]->clone());
            break;
          case IR::EQ:
            if (irs[curId]->items[0]->iVal == loopTempId) {
              loopVal = loopVal == irs[curId]->items[2]->iVal;
              extraIRs.push_back(
                  new IR(IR::MOV, {new IRItem(IRItem::ITEMP, loopTempId),
                                   new IRItem(loopVal)}));
            } else
              extraIRs.push_back(irs[curId]->clone());
            break;
          case IR::GE:
            if (irs[curId]->items[0]->iVal == loopTempId) {
              loopVal = loopVal >= irs[curId]->items[2]->iVal;
              extraIRs.push_back(
                  new IR(IR::MOV, {new IRItem(IRItem::ITEMP, loopTempId),
                                   new IRItem(loopVal)}));
            } else
              extraIRs.push_back(irs[curId]->clone());
            break;
          case IR::GT:
            if (irs[curId]->items[0]->iVal == loopTempId) {
              loopVal = loopVal > irs[curId]->items[2]->iVal;
              extraIRs.push_back(
                  new IR(IR::MOV, {new IRItem(IRItem::ITEMP, loopTempId),
                                   new IRItem(loopVal)}));
            } else
              extraIRs.push_back(irs[curId]->clone());
            break;
          case IR::L_NOT:
            if (irs[curId]->items[0]->iVal == loopTempId) {
              loopVal = !loopVal;
              extraIRs.push_back(
                  new IR(IR::MOV, {new IRItem(IRItem::ITEMP, loopTempId),
                                   new IRItem(loopVal)}));
            } else
              extraIRs.push_back(irs[curId]->clone());
            break;
          case IR::LE:
            if (irs[curId]->items[0]->iVal == loopTempId) {
              loopVal = loopVal <= irs[curId]->items[2]->iVal;
              extraIRs.push_back(
                  new IR(IR::MOV, {new IRItem(IRItem::ITEMP, loopTempId),
                                   new IRItem(loopVal)}));
            } else
              extraIRs.push_back(irs[curId]->clone());
            break;
          case IR::LT:
            if (irs[curId]->items[0]->iVal == loopTempId) {
              loopVal = loopVal < irs[curId]->items[2]->iVal;
              extraIRs.push_back(
                  new IR(IR::MOV, {new IRItem(IRItem::ITEMP, loopTempId),
                                   new IRItem(loopVal)}));
            } else
              extraIRs.push_back(irs[curId]->clone());
            break;
          case IR::MOD:
            if (irs[curId]->items[0]->iVal == loopTempId) {
              loopVal %= irs[curId]->items[2]->iVal;
              extraIRs.push_back(
                  new IR(IR::MOV, {new IRItem(IRItem::ITEMP, loopTempId),
                                   new IRItem(loopVal)}));
            } else
              extraIRs.push_back(irs[curId]->clone());
            break;
          case IR::MUL:
            if (irs[curId]->items[0]->iVal == loopTempId) {
              loopVal *= irs[curId]->items[2]->iVal;
              extraIRs.push_back(
                  new IR(IR::MOV, {new IRItem(IRItem::ITEMP, loopTempId),
                                   new IRItem(loopVal)}));
            } else
              extraIRs.push_back(irs[curId]->clone());
            break;
          case IR::NE:
            if (irs[curId]->items[0]->iVal == loopTempId) {
              loopVal = loopVal != irs[curId]->items[2]->iVal;
              extraIRs.push_back(
                  new IR(IR::MOV, {new IRItem(IRItem::ITEMP, loopTempId),
                                   new IRItem(loopVal)}));
            } else
              extraIRs.push_back(irs[curId]->clone());
            break;
          case IR::NEG:
            if (irs[curId]->items[0]->iVal == loopTempId) {
              loopVal = -loopVal;
              extraIRs.push_back(
                  new IR(IR::MOV, {new IRItem(IRItem::ITEMP, loopTempId),
                                   new IRItem(loopVal)}));
            } else
              extraIRs.push_back(irs[curId]->clone());
            break;
          case IR::SUB:
            if (irs[curId]->items[0]->iVal == loopTempId) {
              loopVal -= irs[curId]->items[2]->iVal;
              extraIRs.push_back(
                  new IR(IR::MOV, {new IRItem(IRItem::ITEMP, loopTempId),
                                   new IRItem(loopVal)}));
            } else
              extraIRs.push_back(irs[curId]->clone());
            break;
          default:
            extraIRs.push_back(irs[curId]->clone());
            break;
          }
          curId++;
          if (extraIRs.size() > 8192) {
            emergency = true;
            break;
          }
        } while (toContinue);
        if (!emergency) {
          irs.erase(irs.begin() + beginId, irs.begin() + endId + 1);
          irs.insert(irs.begin() + beginId, extraIRs.begin(), extraIRs.end());
          break;
        }
      }
    }
  }
}

void IROptimizer::constPassBlock() {
  bool toContinue = false;
  do {
    toContinue = false;
    for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
         it != funcIRs.end(); it++) {
      vector<IR *> &irs = it->second;
      unsigned left = 0, right = 0;
      while (right < irs.size()) {
        while (right < irs.size()) {
          if (irs[right]->type == IR::BEQ || irs[right]->type == IR::BGE ||
              irs[right]->type == IR::BGT || irs[right]->type == IR::BLE ||
              irs[right]->type == IR::BLT || irs[right]->type == IR::BNE ||
              irs[right]->type == IR::CALL || irs[right]->type == IR::GOTO ||
              irs[right]->type == IR::LABEL)
            break;
          right++;
        }
        if (right < irs.size())
          right++;
        unordered_map<unsigned, unsigned> temp2Int;
        unordered_map<unsigned, unsigned> temp2Float;
        unordered_set<Symbol *> modifiedArrays;
        unordered_map<Symbol *, unordered_map<unsigned, unsigned>> arr2Int;
        unordered_map<Symbol *, unordered_map<unsigned, unsigned>> arr2Float;
        for (unsigned i = left; i < right; i++) {
          for (unsigned j = 1; j < irs[i]->items.size(); j++) {
            if (irs[i]->items[j]->type == IRItem::FTEMP &&
                temp2Float.find(irs[i]->items[j]->iVal) != temp2Float.end()) {
              irs[i]->items[j]->type = IRItem::FLOAT;
              irs[i]->items[j]->iVal = temp2Float[irs[i]->items[j]->iVal];
              toContinue = true;
            } else if (irs[i]->items[j]->type == IRItem::ITEMP &&
                       temp2Int.find(irs[i]->items[j]->iVal) !=
                           temp2Int.end()) {
              irs[i]->items[j]->type = IRItem::INT;
              irs[i]->items[j]->iVal = temp2Int[irs[i]->items[j]->iVal];
              toContinue = true;
            }
          }
          if (irs[i]->type == IR::MOV) {
            if (irs[i]->items[0]->type == IRItem::FTEMP) {
              if (irs[i]->items[1]->type == IRItem::FLOAT)
                temp2Float[irs[i]->items[0]->iVal] = irs[i]->items[1]->iVal;
              else
                temp2Float.erase(irs[i]->items[0]->iVal);
            } else if (irs[i]->items[0]->type == IRItem::ITEMP) {
              if (irs[i]->items[1]->type == IRItem::INT)
                temp2Int[irs[i]->items[0]->iVal] = irs[i]->items[1]->iVal;
              else
                temp2Int.erase(irs[i]->items[0]->iVal);
            }
            bool flag = irs[i]->items.size() > 2;
            for (unsigned j = 2; j < irs[i]->items.size(); j++)
              if (irs[i]->items[j]->type != IRItem::INT) {
                flag = false;
                break;
              }
            if (flag) {
              if (irs[i]->items[0]->type == IRItem::SYMBOL &&
                  irs[i]->items[1]->type == IRItem::FLOAT &&
                  modifiedArrays.find(irs[i]->items[0]->symbol) ==
                      modifiedArrays.end() &&
                  irs[i]->items[0]->symbol->dimensions.size() + 2 ==
                      irs[i]->items.size()) {
                unsigned size = 0;
                for (unsigned j = 2; j < irs[i]->items.size(); j++)
                  size = size * irs[i]->items[0]->symbol->dimensions[j - 2] +
                         irs[i]->items[j]->iVal;
                arr2Float[irs[i]->items[0]->symbol][size] =
                    irs[i]->items[1]->iVal;
              }
              if (irs[i]->items[0]->type == IRItem::SYMBOL &&
                  irs[i]->items[1]->type == IRItem::INT &&
                  modifiedArrays.find(irs[i]->items[0]->symbol) ==
                      modifiedArrays.end() &&
                  irs[i]->items[0]->symbol->dimensions.size() + 2 ==
                      irs[i]->items.size()) {
                unsigned size = 0;
                for (unsigned j = 2; j < irs[i]->items.size(); j++)
                  size = size * irs[i]->items[0]->symbol->dimensions[j - 2] +
                         irs[i]->items[j]->iVal;
                arr2Int[irs[i]->items[0]->symbol][size] =
                    irs[i]->items[1]->iVal;
              }
              if (irs[i]->items[0]->type == IRItem::SYMBOL &&
                  irs[i]->items[1]->type == IRItem::FTEMP &&
                  modifiedArrays.find(irs[i]->items[0]->symbol) ==
                      modifiedArrays.end() &&
                  irs[i]->items[0]->symbol->dimensions.size() + 2 ==
                      irs[i]->items.size()) {
                unsigned size = 0;
                for (unsigned j = 2; j < irs[i]->items.size(); j++)
                  size = size * irs[i]->items[0]->symbol->dimensions[j - 2] +
                         irs[i]->items[j]->iVal;
                if (temp2Float.find(irs[i]->items[1]->iVal) == temp2Float.end())
                  arr2Float[irs[i]->items[0]->symbol].erase(size);
                else
                  arr2Float[irs[i]->items[0]->symbol][size] =
                      temp2Float[irs[i]->items[1]->iVal];
              }
              if (irs[i]->items[0]->type == IRItem::SYMBOL &&
                  irs[i]->items[1]->type == IRItem::ITEMP &&
                  modifiedArrays.find(irs[i]->items[0]->symbol) ==
                      modifiedArrays.end() &&
                  irs[i]->items[0]->symbol->dimensions.size() + 2 ==
                      irs[i]->items.size()) {
                unsigned size = 0;
                for (unsigned j = 2; j < irs[i]->items.size(); j++)
                  size = size * irs[i]->items[0]->symbol->dimensions[j - 2] +
                         irs[i]->items[j]->iVal;
                if (temp2Int.find(irs[i]->items[1]->iVal) == temp2Int.end())
                  arr2Int[irs[i]->items[0]->symbol].erase(size);
                else
                  arr2Int[irs[i]->items[0]->symbol][size] =
                      temp2Int[irs[i]->items[1]->iVal];
              }
              if (irs[i]->items[0]->type == IRItem::FTEMP &&
                  irs[i]->items[1]->type == IRItem::SYMBOL &&
                  modifiedArrays.find(irs[i]->items[1]->symbol) ==
                      modifiedArrays.end() &&
                  irs[i]->items[1]->symbol->dimensions.size() + 2 ==
                      irs[i]->items.size()) {
                unsigned size = 0;
                for (unsigned j = 2; j < irs[i]->items.size(); j++)
                  size = size * irs[i]->items[1]->symbol->dimensions[j - 2] +
                         irs[i]->items[j]->iVal;
                if (arr2Float.find(irs[i]->items[1]->symbol) !=
                        arr2Float.end() &&
                    arr2Float[irs[i]->items[1]->symbol].find(size) !=
                        arr2Float[irs[i]->items[1]->symbol].end()) {
                  irs[i]->items.resize(2);
                  irs[i]->items[1]->type = IRItem::FLOAT;
                  irs[i]->items[1]->iVal =
                      arr2Float[irs[i]->items[1]->symbol][size];
                  temp2Float[irs[i]->items[0]->iVal] = irs[i]->items[1]->iVal;
                  toContinue = true;
                }
              }
              if (irs[i]->items[0]->type == IRItem::ITEMP &&
                  irs[i]->items[1]->type == IRItem::SYMBOL &&
                  modifiedArrays.find(irs[i]->items[1]->symbol) ==
                      modifiedArrays.end() &&
                  irs[i]->items[1]->symbol->dimensions.size() + 2 ==
                      irs[i]->items.size()) {
                unsigned size = 0;
                for (unsigned j = 2; j < irs[i]->items.size(); j++)
                  size = size * irs[i]->items[1]->symbol->dimensions[j - 2] +
                         irs[i]->items[j]->iVal;
                if (arr2Int.find(irs[i]->items[1]->symbol) != arr2Int.end() &&
                    arr2Int[irs[i]->items[1]->symbol].find(size) !=
                        arr2Int[irs[i]->items[1]->symbol].end()) {
                  irs[i]->items.resize(2);
                  irs[i]->items[1]->type = IRItem::INT;
                  irs[i]->items[1]->iVal =
                      arr2Int[irs[i]->items[1]->symbol][size];
                  temp2Int[irs[i]->items[0]->iVal] = irs[i]->items[1]->iVal;
                  toContinue = true;
                }
              }
            } else {
              if (irs[i]->items[0]->type == IRItem::SYMBOL)
                modifiedArrays.insert(irs[i]->items[0]->symbol);
            }
          } else if (!irs[i]->items.empty()) {
            if (irs[i]->items[0]->type == IRItem::FTEMP) {
              temp2Float.erase(irs[i]->items[0]->iVal);
            } else if (irs[i]->items[0]->type == IRItem::ITEMP) {
              temp2Int.erase(irs[i]->items[0]->iVal);
            }
          }
        }
        left = right;
      }
      standardize(irs);
    }
  } while (toContinue);
}

void IROptimizer::constPassGlobal() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    vector<IR *> &irs = it->second;
    unordered_map<unsigned, unsigned> temp2Int;
    unordered_map<unsigned, unsigned> temp2Float;
    unordered_map<unsigned, unsigned> defCounter;
    for (IR *ir : irs) {
      if (ir->type == IR::MOV) {
        if (ir->items[0]->type == IRItem::FTEMP &&
            ir->items[1]->type == IRItem::FLOAT) {
          temp2Float[ir->items[0]->iVal] = ir->items[1]->iVal;
        } else if (ir->items[0]->type == IRItem::ITEMP &&
                   ir->items[1]->type == IRItem::INT) {
          temp2Int[ir->items[0]->iVal] = ir->items[1]->iVal;
        }
      }
      if (!ir->items.empty() && (ir->items[0]->type == IRItem::FTEMP ||
                                 ir->items[0]->type == IRItem::ITEMP))
        defCounter[ir->items[0]->iVal]++;
    }
    for (unordered_map<unsigned, unsigned>::iterator innerIt =
             defCounter.begin();
         innerIt != defCounter.end(); innerIt++)
      if (innerIt->second > 1) {
        temp2Float.erase(innerIt->first);
        temp2Int.erase(innerIt->first);
      }
    for (IR *ir : irs) {
      for (unsigned j = 1; j < ir->items.size(); j++) {
        if (ir->items[j]->type == IRItem::FTEMP &&
            temp2Float.find(ir->items[j]->iVal) != temp2Float.end()) {
          ir->items[j]->type = IRItem::FLOAT;
          ir->items[j]->iVal = temp2Float[ir->items[j]->iVal];
        } else if (ir->items[j]->type == IRItem::ITEMP &&
                   temp2Int.find(ir->items[j]->iVal) != temp2Int.end()) {
          ir->items[j]->type = IRItem::INT;
          ir->items[j]->iVal = temp2Int[ir->items[j]->iVal];
        }
      }
    }
    standardize(irs);
  }
}

void IROptimizer::deadArrayAssignElimination() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    vector<IR *> &irs = it->second;
    unsigned left = 0, right = 0;
    while (right < irs.size()) {
      while (right < irs.size()) {
        if (irs[right]->type == IR::BEQ || irs[right]->type == IR::BGE ||
            irs[right]->type == IR::BGT || irs[right]->type == IR::BLE ||
            irs[right]->type == IR::BLT || irs[right]->type == IR::BNE ||
            irs[right]->type == IR::CALL || irs[right]->type == IR::GOTO ||
            irs[right]->type == IR::LABEL)
          break;
        right++;
      }
      if (right < irs.size())
        right++;
      for (unsigned i = left; i < right; i++) {
        if (irs[i]->type != IR::MOV || irs[i]->items.size() <= 2 ||
            irs[i]->items[0]->type != IRItem::SYMBOL ||
            irs[i]->items[0]->symbol->dimensions.empty())
          continue;
        int leftSize = 0;
        for (unsigned j = 2; j < irs[i]->items.size(); j++)
          leftSize = leftSize * irs[i]->items[0]->symbol->dimensions[j - 2] +
                     irs[i]->items[j]->iVal;
        for (unsigned j = i + 1; j < right; j++) {
          if (irs[j]->type != IR::MOV || irs[j]->items.size() <= 2)
            continue;
          if (irs[j]->items[0]->type == IRItem::SYMBOL &&
              irs[j]->items[0]->symbol == irs[i]->items[0]->symbol) {
            bool flag = true;
            int rightSize = 0;
            for (unsigned k = 2; k < irs[j]->items.size(); k++) {
              rightSize =
                  rightSize * irs[j]->items[0]->symbol->dimensions[k - 2] +
                  irs[j]->items[k]->iVal;
              if (irs[j]->items[k]->type != IRItem::INT) {
                flag = false;
                break;
              }
            }
            if (flag && leftSize == rightSize) {
              irs.erase(irs.begin() + i);
              i--;
              right--;
              break;
            }
          }
          if (irs[j]->items[1]->type == IRItem::SYMBOL &&
              irs[j]->items[1]->symbol == irs[i]->items[0]->symbol) {
            bool flag = true;
            int rightSize = 0;
            for (unsigned k = 2; k < irs[j]->items.size(); k++) {
              rightSize =
                  rightSize * irs[j]->items[1]->symbol->dimensions[k - 2] +
                  irs[j]->items[k]->iVal;
              if (irs[j]->items[k]->type != IRItem::INT) {
                flag = false;
                break;
              }
            }
            if (!flag || leftSize == rightSize)
              break;
          }
        }
      }
      left = right;
    }
    standardize(irs);
  }
}

void IROptimizer::deadCodeElimination() {
  unordered_set<Symbol *> todoFuncs;
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++)
    if (!it->first->name.compare("main")) {
      todoFuncs.insert(it->first);
      break;
    }
  unordered_set<unsigned> temps;
  unordered_set<Symbol *> symbols;
  unordered_map<Symbol *, unordered_set<unsigned>> symbol2Temp;
  unordered_map<unsigned, unordered_set<Symbol *>> temp2Symbol;
  unordered_map<unsigned, unordered_set<unsigned>> temp2Temp;
  unsigned originSize;
  do {
    originSize = todoFuncs.size() + temps.size() + symbols.size();
    for (Symbol *func : todoFuncs) {
      if (funcIRs.find(func) == funcIRs.end())
        continue;
      for (Symbol *param : func->params)
        if (!param->dimensions.empty())
          symbols.insert(param);
      for (IR *ir : funcIRs[func]) {
        if (ir->type == IR::BEQ || ir->type == IR::BGE || ir->type == IR::BGT ||
            ir->type == IR::BLE || ir->type == IR::BLT || ir->type == IR::BNE) {
          if (ir->items[1]->type == IRItem::FTEMP ||
              ir->items[1]->type == IRItem::ITEMP)
            temps.insert(ir->items[1]->iVal);
          if (ir->items[2]->type == IRItem::FTEMP ||
              ir->items[2]->type == IRItem::ITEMP)
            temps.insert(ir->items[2]->iVal);
        } else if (ir->type == IR::CALL) {
          todoFuncs.insert(ir->items[0]->symbol);
          for (unsigned i = 1; i < ir->items.size(); i++)
            if (ir->items[i]->type == IRItem::FTEMP ||
                ir->items[i]->type == IRItem::ITEMP)
              temps.insert(ir->items[i]->iVal);
        } else if (ir->type == IR::MOV &&
                   ir->items[0]->type == IRItem::RETURN) {
          if ((ir->items[1]->type == IRItem::FTEMP ||
               ir->items[1]->type == IRItem::ITEMP))
            temps.insert(ir->items[1]->iVal);
          else if (ir->items[1]->type == IRItem::SYMBOL) {
            symbols.insert(ir->items[1]->symbol);
            for (unsigned i = 2; i < ir->items.size(); i++)
              if ((ir->items[1]->type == IRItem::FTEMP ||
                   ir->items[1]->type == IRItem::ITEMP))
                temps.insert(ir->items[i]->iVal);
          }
        }
        switch (ir->type) {
        case IR::ADD:
        case IR::DIV:
        case IR::EQ:
        case IR::GE:
        case IR::GT:
        case IR::LE:
        case IR::LT:
        case IR::MOD:
        case IR::MUL:
        case IR::NE:
        case IR::SUB:
          if (ir->items[1]->type == IRItem::FTEMP ||
              ir->items[1]->type == IRItem::ITEMP)
            temp2Temp[ir->items[0]->iVal].insert(ir->items[1]->iVal);
          if (ir->items[2]->type == IRItem::FTEMP ||
              ir->items[2]->type == IRItem::ITEMP)
            temp2Temp[ir->items[0]->iVal].insert(ir->items[2]->iVal);
          break;
        case IR::F2I:
        case IR::I2F:
        case IR::L_NOT:
        case IR::NEG:
          temp2Temp[ir->items[0]->iVal].insert(ir->items[1]->iVal);
          break;
        case IR::MOV:
          if (ir->items[0]->type == IRItem::FTEMP ||
              ir->items[0]->type == IRItem::ITEMP) {
            for (unsigned i = 1; i < ir->items.size(); i++) {
              if (ir->items[i]->type == IRItem::FTEMP ||
                  ir->items[i]->type == IRItem::ITEMP)
                temp2Temp[ir->items[0]->iVal].insert(ir->items[i]->iVal);
              else if (ir->items[i]->type == IRItem::SYMBOL)
                temp2Symbol[ir->items[0]->iVal].insert(ir->items[i]->symbol);
            }
          } else if (ir->items[0]->type == IRItem::SYMBOL) {
            for (unsigned i = 1; i < ir->items.size(); i++)
              if (ir->items[i]->type == IRItem::FTEMP ||
                  ir->items[i]->type == IRItem::ITEMP)
                symbol2Temp[ir->items[0]->symbol].insert(ir->items[i]->iVal);
          }
          break;
        default:
          break;
        }
      }
      queue<unsigned> toAddTemps;
      for (unsigned temp : temps)
        toAddTemps.push(temp);
      queue<Symbol *> toAddSymbols;
      for (Symbol *symbol : symbols)
        toAddSymbols.push(symbol);
      while (!toAddSymbols.empty() || !toAddTemps.empty()) {
        while (!toAddSymbols.empty()) {
          Symbol *symbol = toAddSymbols.front();
          toAddSymbols.pop();
          if (symbol2Temp.find(symbol) != symbol2Temp.end())
            for (unsigned newTemp : symbol2Temp[symbol])
              if (temps.find(newTemp) == temps.end()) {
                temps.insert(newTemp);
                toAddTemps.push(newTemp);
              }
        }
        while (!toAddTemps.empty()) {
          unsigned temp = toAddTemps.front();
          toAddTemps.pop();
          if (temp2Symbol.find(temp) != temp2Symbol.end())
            for (Symbol *newSymbol : temp2Symbol[temp])
              if (symbols.find(newSymbol) == symbols.end()) {
                symbols.insert(newSymbol);
                toAddSymbols.push(newSymbol);
              }
          if (temp2Temp.find(temp) != temp2Temp.end())
            for (unsigned newTemp : temp2Temp[temp])
              if (temps.find(newTemp) == temps.end()) {
                temps.insert(newTemp);
                toAddTemps.push(newTemp);
              }
        }
      }
    }
  } while (originSize < todoFuncs.size() + temps.size() + symbols.size());
  unordered_map<Symbol *, vector<IR *>> newFuncs;
  for (Symbol *func : todoFuncs) {
    if (funcIRs.find(func) == funcIRs.end())
      continue;
    vector<IR *> newIRs;
    for (IR *ir : funcIRs[func]) {
      if (ir->type == IR::CALL || ir->type == IR::GOTO || ir->type == IR::LABEL)
        newIRs.push_back(ir);
      else {
        bool flag = true;
        for (IRItem *item : ir->items)
          if (((item->type == IRItem::FTEMP || item->type == IRItem::ITEMP) &&
               temps.find(item->iVal) == temps.end()) ||
              (item->type == IRItem::SYMBOL &&
               symbols.find(item->symbol) == symbols.end())) {
            flag = false;
            break;
          }
        if (flag)
          newIRs.push_back(ir);
      }
    }
    newFuncs[func] = newIRs;
  }
  funcIRs = newFuncs;
  vector<Symbol *> newConsts;
  for (Symbol *symbol : consts)
    if (symbols.find(symbol) != symbols.end())
      newConsts.push_back(symbol);
  consts = newConsts;
  vector<Symbol *> newGlobalVars;
  for (Symbol *symbol : globalVars)
    if (symbols.find(symbol) != symbols.end())
      newGlobalVars.push_back(symbol);
  globalVars = newGlobalVars;
  for (unordered_map<Symbol *, vector<Symbol *>>::iterator it =
           localVars.begin();
       it != localVars.end(); it++) {
    vector<Symbol *> newLocalVars;
    for (Symbol *symbol : it->second)
      if (symbols.find(symbol) != symbols.end())
        newLocalVars.push_back(symbol);
    it->second = newLocalVars;
  }
}

void IROptimizer::funcInline() {
  reassignTempId();
  bool toContinue = false;
  do {
    toContinue = false;
    unordered_set<Symbol *> toInlineFuncs = getInlinableFuncs();
    for (Symbol *toInlineFunc : toInlineFuncs) {
      for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
           it != funcIRs.end(); it++) {
        Symbol *func = it->first;
        vector<IR *> &irs = it->second;
        if (func == toInlineFunc)
          continue;
        vector<IR *> newIRs;
        bool lastIsInlineCall = false;
        IRItem *retItem = nullptr;
        unordered_set<Symbol *> arrs;
        for (Symbol *param : toInlineFunc->params)
          if (!param->dimensions.empty())
            arrs.insert(param);
        for (IR *ir : irs) {
          if (lastIsInlineCall && ir->type == IR::MOV &&
              ir->items[1]->type == IRItem::RETURN) {
            ir->items[1] = retItem->clone();
            newIRs.push_back(ir);
            lastIsInlineCall = false;
            continue;
          }
          if (ir->type != IR::CALL) {
            newIRs.push_back(ir);
            lastIsInlineCall = false;
            continue;
          }
          if (ir->items[0]->symbol != toInlineFunc) {
            newIRs.push_back(ir);
            lastIsInlineCall = false;
            continue;
          }
          lastIsInlineCall = true;
          toContinue = true;
          if (toInlineFunc->dataType == Symbol::FLOAT)
            retItem = new IRItem(IRItem::FTEMP, tempId++);
          if (toInlineFunc->dataType == Symbol::INT)
            retItem = new IRItem(IRItem::ITEMP, tempId++);
          unordered_map<Symbol *, IRItem *> symbol2IRItem;
          for (unsigned i = 0; i < toInlineFunc->params.size(); i++)
            symbol2IRItem[toInlineFunc->params[i]] = ir->items[i + 1];
          vector<IR *> inlineIRs;
          unordered_map<IR *, unsigned> irIdMap;
          unordered_map<unsigned, unsigned> tempIds;
          unordered_map<Symbol *, Symbol *> localVarOld2New;
          for (unsigned i = 0; i < funcIRs[toInlineFunc].size(); i++) {
            irIdMap[funcIRs[toInlineFunc][i]] = i;
            IR *inlineIR = funcIRs[toInlineFunc][i]->clone();
            inlineIRs.push_back(inlineIR);
            if (inlineIR->type == IR::MOV &&
                inlineIR->items[0]->type == IRItem::RETURN)
              inlineIR->items[0] = retItem->clone();
            for (unsigned j = 0; j < inlineIR->items.size(); j++) {
              if (inlineIR->items[j]->type == IRItem::SYMBOL &&
                  inlineIR->items[j]->symbol->symbolType == Symbol::LOCAL_VAR) {
                if (localVarOld2New.find(inlineIR->items[j]->symbol) ==
                    localVarOld2New.end()) {
                  Symbol *newSymbol = inlineIR->items[j]->symbol->clone();
                  localVars[func].push_back(newSymbol);
                  localVarOld2New[inlineIR->items[j]->symbol] = newSymbol;
                }
                inlineIR->items[j]->symbol =
                    localVarOld2New[inlineIR->items[j]->symbol];
              }
              if (inlineIR->items[j]->type == IRItem::SYMBOL &&
                  inlineIR->items[j]->symbol->symbolType == Symbol::PARAM) {
                inlineIR->items[j] =
                    symbol2IRItem[inlineIR->items[j]->symbol]->clone();
                continue;
              }
              if ((inlineIR->items[j]->type == IRItem::FTEMP ||
                   inlineIR->items[j]->type == IRItem::ITEMP) &&
                  (retItem == nullptr ||
                   inlineIR->items[j]->iVal != retItem->iVal)) {
                if (tempIds.find(inlineIR->items[j]->iVal) == tempIds.end())
                  tempIds[inlineIR->items[j]->iVal] = tempId++;
                inlineIR->items[j]->iVal = tempIds[inlineIR->items[j]->iVal];
              }
            }
          }
          for (unsigned i = 0; i < inlineIRs.size(); i++)
            if (inlineIRs[i]->type == IR::BEQ ||
                inlineIRs[i]->type == IR::BGE ||
                inlineIRs[i]->type == IR::BGT ||
                inlineIRs[i]->type == IR::BLE ||
                inlineIRs[i]->type == IR::BLT ||
                inlineIRs[i]->type == IR::BNE || inlineIRs[i]->type == IR::GOTO)
              inlineIRs[i]->items[0]->ir =
                  inlineIRs[irIdMap[inlineIRs[i]->items[0]->ir]];
          newIRs.insert(newIRs.end(), inlineIRs.begin(), inlineIRs.end());
        }
        passArr(newIRs);
        irs = newIRs;
      }
    }
    deadCodeElimination();
  } while (toContinue);
}

unordered_set<Symbol *> IROptimizer::getInlinableFuncs() {
  unordered_set<Symbol *> funcs;
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    Symbol *func = it->first;
    if (!func->name.compare("main"))
      continue;
    vector<IR *> &irs = it->second;
    bool flag = true;
    for (IR *ir : irs)
      if (ir->type == IR::CALL && ir->items[0]->symbol == func) {
        flag = false;
        break;
      }
    if (flag)
      funcs.insert(func);
  }
  return funcs;
}

void IROptimizer::optimize() {
  singleVar2Reg();
  deadCodeElimination();
  constPassGlobal();
  constPassBlock();
  deadCodeElimination();
  assignPass();
  deadCodeElimination();
  splitTemps();
  deadCodeElimination();
  unsigned originSize;
  unsigned processedSize = 0;
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++)
    processedSize += it->second.size();
  do {
    originSize = processedSize;
    optimizeFlow();
    peepholeOptimize();
    deadCodeElimination();
    constPassGlobal();
    constPassBlock();
    assignPass();
    deadCodeElimination();
    deadArrayAssignElimination();
    optimizeFlow();
    processedSize = 0;
    for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
         it != funcIRs.end(); it++)
      processedSize += it->second.size();
  } while (originSize != processedSize);
  constLoopExpand();
  funcInline();
  global2Local();
  splitArrays();
  singleVar2Reg();
  deadCodeElimination();
  deadArrayAssignElimination();
  splitTemps();
  constLoopExpand();
  processedSize = 0;
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++)
    processedSize += it->second.size();
  do {
    originSize = processedSize;
    optimizeFlow();
    peepholeOptimize();
    deadCodeElimination();
    constPassGlobal();
    constPassBlock();
    assignPass();
    deadCodeElimination();
    deadArrayAssignElimination();
    optimizeFlow();
    processedSize = 0;
    for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
         it != funcIRs.end(); it++)
      processedSize += it->second.size();
  } while (originSize != processedSize);
  constLoopExpand();
  splitArrays();
  singleVar2Reg();
  splitTemps();
  processedSize = 0;
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++)
    processedSize += it->second.size();
  do {
    originSize = processedSize;
    optimizeFlow();
    peepholeOptimize();
    deadCodeElimination();
    constPassGlobal();
    constPassBlock();
    assignPass();
    deadCodeElimination();
    deadArrayAssignElimination();
    optimizeFlow();
    processedSize = 0;
    for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
         it != funcIRs.end(); it++)
      processedSize += it->second.size();
  } while (originSize != processedSize);
}

void IROptimizer::global2Local() {
  unordered_set<Symbol *> moveableGlobals(globalVars.begin(), globalVars.end());
  Symbol *mainFunc = nullptr;
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    Symbol *func = it->first;
    vector<IR *> &irs = it->second;
    if (!func->name.compare("main")) {
      mainFunc = func;
      continue;
    }
    for (IR *ir : irs)
      for (IRItem *item : ir->items)
        if (item->type == IRItem::SYMBOL &&
            item->symbol->symbolType == Symbol::GLOBAL_VAR)
          moveableGlobals.erase(item->symbol);
  }
  vector<IR *> newIRs;
  for (Symbol *moveableGlobal : moveableGlobals) {
    if (moveableGlobal->dimensions.empty()) {
      globalVars.erase(
          find(globalVars.begin(), globalVars.end(), moveableGlobal));
      moveableGlobal->symbolType = Symbol::LOCAL_VAR;
      localVars[mainFunc].push_back(moveableGlobal);
      if (moveableGlobal->dataType == Symbol::INT)
        newIRs.push_back(new IR(IR::MOV, {new IRItem(moveableGlobal),
                                          new IRItem(moveableGlobal->iVal)}));
      else
        newIRs.push_back(new IR(IR::MOV, {new IRItem(moveableGlobal),
                                          new IRItem(moveableGlobal->fVal)}));
    } else {
      unsigned totalSize = 1;
      for (unsigned dimension : moveableGlobal->dimensions)
        totalSize *= dimension;
      if (totalSize > 1024)
        continue;
      globalVars.erase(
          find(globalVars.begin(), globalVars.end(), moveableGlobal));
      moveableGlobal->symbolType = Symbol::LOCAL_VAR;
      localVars[mainFunc].push_back(moveableGlobal);
      for (unsigned i = 0; i < totalSize; i++) {
        vector<unsigned> dimensions(moveableGlobal->dimensions.size(), 0);
        unsigned t = i;
        for (int j = moveableGlobal->dimensions.size() - 1; j >= 0; j--) {
          dimensions[j] = t % moveableGlobal->dimensions[j];
          t /= moveableGlobal->dimensions[j];
        }
        IR *newIR =
            new IR(IR::MOV, {new IRItem(moveableGlobal),
                             moveableGlobal->dataType == Symbol::INT
                                 ? new IRItem(moveableGlobal->iMap[i])
                                 : new IRItem(moveableGlobal->fMap[i])});
        for (unsigned dimension : dimensions)
          newIR->items.push_back(new IRItem((int)dimension));
        newIRs.push_back(newIR);
      }
    }
  }
  funcIRs[mainFunc].insert(funcIRs[mainFunc].begin(), newIRs.begin(),
                           newIRs.end());
}

void IROptimizer::optimizeFlow() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    bool toContinue;
    do {
      toContinue = false;
      vector<IR *> &irs = it->second;
      unordered_map<IR *, unsigned> irIdMap;
      for (unsigned i = 0; i < irs.size(); i++)
        irIdMap[irs[i]] = i;
      unordered_set<unsigned> usedLabelId;
      for (IR *ir : irs) {
        if (ir->type == IR::BEQ || ir->type == IR::BGE || ir->type == IR::BGT ||
            ir->type == IR::BLE || ir->type == IR::BLT || ir->type == IR::BNE ||
            ir->type == IR::GOTO) {
          unsigned labelId = irIdMap[ir->items[0]->ir];
          while (labelId + 1 < irs.size() &&
                 irs[labelId + 1]->type == IR::LABEL)
            labelId++;
          usedLabelId.insert(labelId);
          ir->items[0]->ir = irs[labelId];
        }
      }
      vector<IR *> newIRs;
      for (unsigned i = 0; i < irs.size(); i++)
        if (irs[i]->type != IR::LABEL ||
            usedLabelId.find(i) != usedLabelId.end())
          newIRs.push_back(irs[i]);
      if (irs.size() != newIRs.size())
        toContinue = true;
      irs = newIRs;
      newIRs.clear();
      for (unsigned i = 0; i < irs.size(); i++)
        irIdMap[irs[i]] = i;
      queue<unsigned> frontier;
      unsigned beginId = 0;
      while (irs[beginId]->type == IR::GOTO)
        beginId = irIdMap[irs[beginId]->items[0]->ir];
      frontier.push(beginId);
      unordered_set<unsigned> visited;
      while (!frontier.empty()) {
        unsigned curId = frontier.front();
        frontier.pop();
        if (visited.find(curId) != visited.end())
          continue;
        visited.insert(curId);
        if (irs[curId]->type == IR::BEQ || irs[curId]->type == IR::BGE ||
            irs[curId]->type == IR::BGT || irs[curId]->type == IR::BLE ||
            irs[curId]->type == IR::BLT || irs[curId]->type == IR::BNE) {
          if (curId + 1 < irs.size())
            frontier.push(curId + 1);
          frontier.push(irIdMap[irs[curId]->items[0]->ir]);
        } else if (irs[curId]->type == IR::GOTO)
          frontier.push(irIdMap[irs[curId]->items[0]->ir]);
        else {
          if (curId + 1 < irs.size())
            frontier.push(curId + 1);
        }
      }
      for (unsigned i = 0; i < irs.size(); i++)
        if (visited.find(i) != visited.end())
          newIRs.push_back(irs[i]);
      if (irs.size() != newIRs.size())
        toContinue = true;
      irs = newIRs;
      newIRs.clear();
      irIdMap.clear();
      for (unsigned i = 0; i < irs.size(); i++)
        irIdMap[irs[i]] = i;
      for (unsigned i = 0; i < irs.size(); i++) {
        if ((irs[i]->type == IR::BEQ || irs[i]->type == IR::BGE ||
             irs[i]->type == IR::BGT || irs[i]->type == IR::BLE ||
             irs[i]->type == IR::BLT || irs[i]->type == IR::BNE ||
             irs[i]->type == IR::GOTO) &&
            irIdMap[irs[i]->items[0]->ir] == i + 1)
          continue;
        newIRs.push_back(irs[i]);
      }
      if (irs.size() != newIRs.size())
        toContinue = true;
      irs = newIRs;
    } while (toContinue);
  }
}

void IROptimizer::passArr(vector<IR *> &irs) {
  unordered_map<unsigned, IR *> temp2Arr;
  for (IR *ir : irs) {
    if (ir->type == IR::MOV && ir->items[0]->type == IRItem::ITEMP &&
        ir->items[1]->type == IRItem::SYMBOL &&
        ir->items.size() - 2 < ir->items[1]->symbol->dimensions.size()) {
      temp2Arr[ir->items[0]->iVal] = ir;
      continue;
    }
    if (ir->type == IR::MOV && ir->items[1]->type == IRItem::ITEMP &&
        temp2Arr.find(ir->items[1]->iVal) != temp2Arr.end()) {
      IR *arrIR = temp2Arr[ir->items[1]->iVal];
      ir->items[1] = arrIR->items[1]->clone();
      for (unsigned i = 2; i < arrIR->items.size(); i++)
        ir->items.insert(ir->items.begin() + i, arrIR->items[i]->clone());
    }
    if (ir->type == IR::MOV && ir->items[0]->type == IRItem::ITEMP &&
        temp2Arr.find(ir->items[0]->iVal) != temp2Arr.end()) {
      IR *arrIR = temp2Arr[ir->items[0]->iVal];
      ir->items[0] = arrIR->items[1]->clone();
      for (unsigned i = 2; i < arrIR->items.size(); i++)
        ir->items.insert(ir->items.begin() + i, arrIR->items[i]->clone());
    }
  }
}

void IROptimizer::peepholeOptimize() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    vector<IR *> &irs = it->second;
    vector<IR *> newIRs;
    irs.push_back(new IR(IR::NOP));
    irs.push_back(new IR(IR::NOP));
    irs.push_back(new IR(IR::NOP));
    for (unsigned i = 0; i < irs.size() - 2; i++) {
      if ((irs[i]->type == IR::BEQ || irs[i]->type == IR::BGE ||
           irs[i]->type == IR::BGT || irs[i]->type == IR::BLE ||
           irs[i]->type == IR::BLT || irs[i]->type == IR::BNE) &&
          irs[i]->items[0]->ir == irs[i + 2] && irs[i + 1]->type == IR::GOTO) {
        switch (irs[i]->type) {
        case IR::BEQ:
          irs[i + 1]->type = IR::BNE;
          break;
        case IR::BGE:
          irs[i + 1]->type = IR::BLT;
          break;
        case IR::BGT:
          irs[i + 1]->type = IR::BLE;
          break;
        case IR::BLE:
          irs[i + 1]->type = IR::BGT;
          break;
        case IR::BLT:
          irs[i + 1]->type = IR::BGE;
          break;
        case IR::BNE:
          irs[i + 1]->type = IR::BEQ;
          break;
        default:
          break;
        }
        irs[i + 1]->items.push_back(irs[i]->items[1]);
        irs[i + 1]->items.push_back(irs[i]->items[2]);
        continue;
      }
      if (irs[i]->type == IR::MOV && irs[i + 1]->type == IR::MOV) {
        if (irs[i]->items.size() == 2 && irs[i + 1]->items.size() == 2 &&
            irs[i]->items[0]->type == IRItem::SYMBOL &&
            irs[i]->items[0]->equals(irs[i + 1]->items[1]))
          irs[i + 1]->items[1] = irs[i]->items[1]->clone();
      }
      if ((irs[i]->type == IR::ADD || irs[i]->type == IR::DIV ||
           irs[i]->type == IR::SUB || irs[i]->type == IR::MOD ||
           irs[i]->type == IR::MUL) &&
          irs[i + 1]->type == IR::MOV && irs[i + 1]->items.size() == 2 &&
          irs[i]->items[0]->equals(irs[i + 1]->items[1]) &&
          irs[i]->items[1]->equals(irs[i + 1]->items[0])) {
        irs[i]->items[0] = irs[i]->items[1]->clone();
        swap(irs[i + 1]->items[0], irs[i + 1]->items[1]);
      }
      if (irs[i]->type == IR::ADD && irs[i + 1]->type == IR::ADD &&
          irs[i]->items[0]->type == IRItem::ITEMP &&
          irs[i + 1]->items[0]->type == IRItem::ITEMP &&
          irs[i]->items[2]->type == IRItem::INT &&
          irs[i + 1]->items[2]->type == IRItem::INT &&
          irs[i]->items[0]->equals(irs[i + 1]->items[1]) &&
          !irs[i]->items[0]->equals(irs[i]->items[1])) {
        irs[i + 1]->items[1] = irs[i]->items[1]->clone();
        irs[i + 1]->items[2]->iVal =
            irs[i]->items[2]->iVal + irs[i + 1]->items[2]->iVal;
      }
      if (irs[i]->type == IR::ADD && irs[i + 1]->type == IR::SUB &&
          irs[i]->items[0]->type == IRItem::ITEMP &&
          irs[i + 1]->items[0]->type == IRItem::ITEMP &&
          irs[i]->items[2]->type == IRItem::INT &&
          irs[i + 1]->items[2]->type == IRItem::INT &&
          irs[i]->items[0]->equals(irs[i + 1]->items[1]) &&
          !irs[i]->items[0]->equals(irs[i]->items[1])) {
        irs[i + 1]->items[1] = irs[i]->items[1]->clone();
        irs[i + 1]->items[2]->iVal =
            irs[i + 1]->items[2]->iVal - irs[i]->items[2]->iVal;
      }
      if (irs[i]->type == IR::SUB && irs[i + 1]->type == IR::ADD &&
          irs[i]->items[0]->type == IRItem::ITEMP &&
          irs[i + 1]->items[0]->type == IRItem::ITEMP &&
          irs[i]->items[2]->type == IRItem::INT &&
          irs[i + 1]->items[2]->type == IRItem::INT &&
          irs[i]->items[0]->equals(irs[i + 1]->items[1]) &&
          !irs[i]->items[0]->equals(irs[i]->items[1])) {
        irs[i + 1]->items[1] = irs[i]->items[1]->clone();
        irs[i + 1]->items[2]->iVal =
            irs[i + 1]->items[2]->iVal - irs[i]->items[2]->iVal;
      }
      if (irs[i]->type == IR::SUB && irs[i + 1]->type == IR::SUB &&
          irs[i]->items[0]->type == IRItem::ITEMP &&
          irs[i + 1]->items[0]->type == IRItem::ITEMP &&
          irs[i]->items[2]->type == IRItem::INT &&
          irs[i + 1]->items[2]->type == IRItem::INT &&
          irs[i]->items[0]->equals(irs[i + 1]->items[1]) &&
          !irs[i]->items[0]->equals(irs[i]->items[1])) {
        irs[i + 1]->items[1] = irs[i]->items[1]->clone();
        irs[i + 1]->items[2]->iVal =
            irs[i]->items[2]->iVal + irs[i + 1]->items[2]->iVal;
      }
      if (irs[i]->type == IR::ADD && irs[i + 1]->type == IR::ADD &&
          irs[i]->items[0]->type == IRItem::FTEMP &&
          irs[i + 1]->items[0]->type == IRItem::FTEMP &&
          irs[i]->items[2]->type == IRItem::FLOAT &&
          irs[i + 1]->items[2]->type == IRItem::FLOAT &&
          irs[i]->items[0]->equals(irs[i + 1]->items[1]) &&
          !irs[i]->items[0]->equals(irs[i]->items[1])) {
        irs[i + 1]->items[1] = irs[i]->items[1]->clone();
        irs[i + 1]->items[2]->fVal =
            irs[i]->items[2]->fVal + irs[i + 1]->items[2]->fVal;
      }
      if (irs[i]->type == IR::ADD && irs[i + 1]->type == IR::SUB &&
          irs[i]->items[0]->type == IRItem::FTEMP &&
          irs[i + 1]->items[0]->type == IRItem::FTEMP &&
          irs[i]->items[2]->type == IRItem::FLOAT &&
          irs[i + 1]->items[2]->type == IRItem::FLOAT &&
          irs[i]->items[0]->equals(irs[i + 1]->items[1]) &&
          !irs[i]->items[0]->equals(irs[i]->items[1])) {
        irs[i + 1]->items[1] = irs[i]->items[1]->clone();
        irs[i + 1]->items[2]->fVal =
            irs[i + 1]->items[2]->fVal - irs[i]->items[2]->fVal;
      }
      if (irs[i]->type == IR::SUB && irs[i + 1]->type == IR::ADD &&
          irs[i]->items[0]->type == IRItem::FTEMP &&
          irs[i + 1]->items[0]->type == IRItem::FTEMP &&
          irs[i]->items[2]->type == IRItem::FLOAT &&
          irs[i + 1]->items[2]->type == IRItem::FLOAT &&
          irs[i]->items[0]->equals(irs[i + 1]->items[1]) &&
          !irs[i]->items[0]->equals(irs[i]->items[1])) {
        irs[i + 1]->items[1] = irs[i]->items[1]->clone();
        irs[i + 1]->items[2]->fVal =
            irs[i + 1]->items[2]->fVal - irs[i]->items[2]->fVal;
      }
      if (irs[i]->type == IR::SUB && irs[i + 1]->type == IR::SUB &&
          irs[i]->items[0]->type == IRItem::FTEMP &&
          irs[i + 1]->items[0]->type == IRItem::FTEMP &&
          irs[i]->items[2]->type == IRItem::FLOAT &&
          irs[i + 1]->items[2]->type == IRItem::FLOAT &&
          irs[i]->items[0]->equals(irs[i + 1]->items[1]) &&
          !irs[i]->items[0]->equals(irs[i]->items[1])) {
        irs[i + 1]->items[1] = irs[i]->items[1]->clone();
        irs[i + 1]->items[2]->fVal =
            irs[i]->items[2]->fVal + irs[i + 1]->items[2]->fVal;
      }
      newIRs.push_back(irs[i]);
    }
    while (!newIRs.empty() && newIRs.back()->type == IR::NOP)
      newIRs.pop_back();
    irs = newIRs;
  }
}

void IROptimizer::reassignTempId() {
  tempId = 0;
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    vector<IR *> &irs = it->second;
    unordered_map<unsigned, unsigned> reassignMap;
    for (IR *ir : irs)
      for (IRItem *item : ir->items)
        if ((item->type == IRItem::FTEMP || item->type == IRItem::ITEMP) &&
            reassignMap.find(item->iVal) == reassignMap.end())
          reassignMap[item->iVal] = tempId++;
    for (IR *ir : irs)
      for (IRItem *item : ir->items)
        if (item->type == IRItem::FTEMP || item->type == IRItem::ITEMP)
          item->iVal = reassignMap[item->iVal];
  }
}

void IROptimizer::singleVar2Reg() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    Symbol *func = it->first;
    vector<IR *> &irs = it->second;
    vector<IR *> newIRs;
    unordered_map<Symbol *, unsigned> symbol2tempId;
    for (Symbol *param : func->params) {
      if (param->dimensions.empty()) {
        symbol2tempId[param] = tempId++;
        newIRs.push_back(new IR(
            IR::MOV, {new IRItem(param->dataType == Symbol::INT ? IRItem::ITEMP
                                                                : IRItem::FTEMP,
                                 symbol2tempId[param]),
                      new IRItem(param)}));
      }
    }
    for (IR *ir : irs) {
      for (unsigned i = 0; i < ir->items.size(); i++) {
        if (ir->items[i]->symbol &&
            (ir->items[i]->symbol->symbolType == Symbol::LOCAL_VAR ||
             ir->items[i]->symbol->symbolType == Symbol::PARAM) &&
            ir->items[i]->symbol->dimensions.empty()) {
          if (symbol2tempId.find(ir->items[i]->symbol) == symbol2tempId.end())
            symbol2tempId[ir->items[i]->symbol] = tempId++;
          Symbol *symbol = ir->items[i]->symbol;
          ir->items[i] = new IRItem(
              symbol->dataType == Symbol::INT ? IRItem::ITEMP : IRItem::FTEMP,
              symbol2tempId[symbol]);
        }
      }
      newIRs.push_back(ir);
    }
    irs = newIRs;
  }
}

void IROptimizer::splitArrays() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    Symbol *func = it->first;
    vector<IR *> &irs = it->second;
    unordered_set<Symbol *> possibleVars(localVars[func].begin(),
                                         localVars[func].end());
    for (IR *ir : irs) {
      if (ir->type == IR::MOV) {
        if (ir->items[0]->type == IRItem::SYMBOL &&
            !ir->items[0]->symbol->dimensions.empty())
          for (unsigned i = 2; i < ir->items.size(); i++)
            if (ir->items[i]->type != IRItem::INT) {
              possibleVars.erase(ir->items[0]->symbol);
              break;
            }
        if (ir->items[1]->type == IRItem::SYMBOL &&
            !ir->items[1]->symbol->dimensions.empty()) {
          if (ir->items[1]->symbol->dimensions.size() + 2 != ir->items.size()) {
            possibleVars.erase(ir->items[1]->symbol);
            continue;
          }
          for (unsigned i = 2; i < ir->items.size(); i++)
            if (ir->items[i]->type != IRItem::INT) {
              possibleVars.erase(ir->items[1]->symbol);
              break;
            }
        }
      }
    }
    vector<Symbol *> newLocalVars;
    for (Symbol *localVar : localVars[func])
      if (possibleVars.find(localVar) == possibleVars.end())
        newLocalVars.push_back(localVar);
    for (Symbol *possibleVar : possibleVars) {
      unordered_map<unsigned, Symbol *> newSymbols;
      for (IR *ir : irs) {
        if (ir->type == IR::MOV) {
          if (ir->items[0]->type == IRItem::SYMBOL &&
              ir->items[0]->symbol == possibleVar) {
            unsigned size = 0;
            for (unsigned i = 2; i < ir->items.size(); i++)
              size = size * possibleVar->dimensions[i - 2] + ir->items[i]->iVal;
            if (newSymbols.find(size) == newSymbols.end()) {
              newSymbols[size] = possibleVar->clone();
              newSymbols[size]->dimensions.clear();
            }
            ir->items[0]->symbol = newSymbols[size];
            ir->items.resize(2);
          }
          if (ir->items[1]->type == IRItem::SYMBOL &&
              ir->items[1]->symbol == possibleVar) {
            unsigned size = 0;
            for (unsigned i = 2; i < ir->items.size(); i++)
              size = size * possibleVar->dimensions[i - 2] + ir->items[i]->iVal;
            if (newSymbols.find(size) == newSymbols.end()) {
              newSymbols[size] = possibleVar->clone();
              newSymbols[size]->dimensions.clear();
              newLocalVars.push_back(newSymbols[size]);
            }
            ir->items[1]->symbol = newSymbols[size];
            ir->items.resize(2);
          }
        }
      }
    }
    localVars[func] = newLocalVars;
  }
}

void IROptimizer::splitTemps() {
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    vector<IR *> &irs = it->second;
    unsigned tempNum = 0;
    unordered_map<unsigned, unsigned> reassignMap;
    for (IR *ir : irs) {
      for (IRItem *item : ir->items) {
        if (item->type != IRItem::FTEMP && item->type != IRItem::ITEMP)
          continue;
        if (reassignMap.find(item->iVal) != reassignMap.end())
          continue;
        reassignMap[item->iVal] = tempNum++;
      }
    }
    unordered_map<IR *, unsigned> irIdMap;
    unsigned irNum = 0;
    for (IR *ir : irs) {
      irIdMap[ir] = irNum;
      ir->irId = irNum++;
      for (IRItem *item : ir->items) {
        if (item->type != IRItem::FTEMP && item->type != IRItem::ITEMP)
          continue;
        item->iVal = reassignMap[item->iVal];
      }
    }
    vector<vector<unsigned>> nextMap(irNum);
    vector<set<unsigned>> rMap(tempNum);
    vector<set<unsigned>> wMap(tempNum);
    for (unsigned i = 0; i < irs.size(); i++) {
      if (irs[i]->type == IR::GOTO)
        nextMap[i].push_back(irIdMap[irs[i]->items[0]->ir]);
      else if (irs[i]->type == IR::BEQ || irs[i]->type == IR::BGE ||
               irs[i]->type == IR::BGT || irs[i]->type == IR::BLE ||
               irs[i]->type == IR::BLT || irs[i]->type == IR::BNE) {
        nextMap[i].push_back(i + 1);
        nextMap[i].push_back(irIdMap[irs[i]->items[0]->ir]);
      } else if (i + 1 < irs.size())
        nextMap[i].push_back(i + 1);
      for (unsigned j = 0; j < irs[i]->items.size(); j++) {
        if (irs[i]->items[j]->type == IRItem::FTEMP ||
            irs[i]->items[j]->type == IRItem::ITEMP) {
          if (j)
            rMap[irs[i]->items[j]->iVal].insert(i);
          else
            wMap[irs[i]->items[j]->iVal].insert(i);
        }
      }
    }
    for (unsigned temp = 0; temp < tempNum; temp++) {
      unordered_map<unsigned, unordered_set<unsigned>> w2RMap;
      for (unsigned source : wMap[temp]) {
        unordered_set<unsigned> visited;
        queue<unsigned> frontier;
        frontier.push(source);
        while (!frontier.empty()) {
          unsigned cur = frontier.front();
          frontier.pop();
          for (unsigned next : nextMap[cur]) {
            if (visited.find(next) != visited.end())
              continue;
            visited.insert(next);
            if (wMap[temp].find(next) == wMap[temp].end())
              frontier.push(next);
            if (rMap[temp].find(next) != rMap[temp].end())
              w2RMap[source].insert(next);
          }
        }
      }
      vector<unsigned> rIds(rMap[temp].begin(), rMap[temp].end());
      vector<unsigned> unionSet(rMap[temp].size());
      unordered_map<unsigned, unsigned> rIdMap;
      for (unsigned i = 0; i < rIds.size(); i++) {
        unionSet[i] = i;
        rIdMap[rIds[i]] = i;
      }
      vector<pair<unordered_set<unsigned>, unordered_set<unsigned>>> w2R;
      for (unordered_map<unsigned, unordered_set<unsigned>>::iterator innerIt =
               w2RMap.begin();
           innerIt != w2RMap.end(); innerIt++) {
        w2R.emplace_back(unordered_set({innerIt->first}), innerIt->second);
      }
      unsigned w2RSize;
      do {
        w2RSize = w2R.size();
        for (unsigned i = 0; i < w2R.size(); i++)
          for (unsigned j = i + 1; j < w2R.size(); j++) {
            unordered_set<unsigned> unionSet;
            unionSet.insert(w2R[i].second.begin(), w2R[i].second.end());
            unionSet.insert(w2R[j].second.begin(), w2R[j].second.end());
            if (w2R[i].second.size() + w2R[j].second.size() > unionSet.size()) {
              w2R[i].first.insert(w2R[j].first.begin(), w2R[j].first.end());
              w2R[i].second.insert(w2R[j].second.begin(), w2R[j].second.end());
              w2R.erase(w2R.begin() + j);
              j--;
            }
          }
      } while (w2R.size() < w2RSize);
      for (pair<unordered_set<unsigned>, unordered_set<unsigned>> &w2RItem :
           w2R) {
        for (unsigned wId : w2RItem.first)
          irs[wId]->items[0]->iVal = tempId;
        for (unsigned rId : w2RItem.second)
          for (IRItem *item : irs[rId]->items)
            if ((item->type == IRItem::FTEMP || item->type == IRItem::ITEMP) &&
                (unsigned)item->iVal == temp)
              item->iVal = tempId;
        tempId++;
      }
    }
  }
  unsigned id = 0;
  for (unordered_map<Symbol *, vector<IR *>>::iterator it = funcIRs.begin();
       it != funcIRs.end(); it++) {
    vector<IR *> &irs = it->second;
    unordered_map<unsigned, unsigned> reassignMap;
    for (IR *ir : irs) {
      for (IRItem *item : ir->items) {
        if (item->type != IRItem::FTEMP && item->type != IRItem::ITEMP)
          continue;
        if (reassignMap.find(item->iVal) != reassignMap.end())
          continue;
        reassignMap[item->iVal] = id++;
      }
    }
    for (IR *ir : irs) {
      for (IRItem *item : ir->items) {
        if (item->type != IRItem::FTEMP && item->type != IRItem::ITEMP)
          continue;
        item->iVal = reassignMap[item->iVal];
      }
    }
  }
}

void IROptimizer::standardize(vector<IR *> &irs) {
  vector<IR *> newIRs;
  for (IR *ir : irs) {
    switch (ir->type) {
    case IR::ADD:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->fVal = ir->items[1]->fVal + ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal + ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::BEQ:
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal == ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal == ir->items[2]->iVal)) {
        ir->type = IR::GOTO;
        ir->items.resize(1);
        newIRs.push_back(ir);
        continue;
      }
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal != ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal != ir->items[2]->iVal))
        continue;
      newIRs.push_back(ir);
      break;
    case IR::BGE:
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal >= ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal >= ir->items[2]->iVal)) {
        ir->type = IR::GOTO;
        ir->items.resize(1);
        newIRs.push_back(ir);
        continue;
      }
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal < ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal < ir->items[2]->iVal))
        continue;
      newIRs.push_back(ir);
      break;
    case IR::BGT:
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal > ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal > ir->items[2]->iVal)) {
        ir->type = IR::GOTO;
        ir->items.resize(1);
        newIRs.push_back(ir);
        continue;
      }
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal <= ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal <= ir->items[2]->iVal))
        continue;
      newIRs.push_back(ir);
      break;
    case IR::BLE:
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal <= ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal <= ir->items[2]->iVal)) {
        ir->type = IR::GOTO;
        ir->items.resize(1);
        newIRs.push_back(ir);
        continue;
      }
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal > ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal > ir->items[2]->iVal))
        continue;
      newIRs.push_back(ir);
      break;
    case IR::BLT:
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal < ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal < ir->items[2]->iVal)) {
        ir->type = IR::GOTO;
        ir->items.resize(1);
        newIRs.push_back(ir);
        continue;
      }
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal >= ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal >= ir->items[2]->iVal))
        continue;
      newIRs.push_back(ir);
      break;
    case IR::BNE:
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal != ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal != ir->items[2]->iVal)) {
        ir->type = IR::GOTO;
        ir->items.resize(1);
        newIRs.push_back(ir);
        continue;
      }
      if ((ir->items[1]->type == IRItem::FLOAT &&
           ir->items[2]->type == IRItem::FLOAT &&
           ir->items[1]->fVal == ir->items[2]->fVal) ||
          (ir->items[1]->type == IRItem::INT &&
           ir->items[2]->type == IRItem::INT &&
           ir->items[1]->iVal == ir->items[2]->iVal))
        continue;
      newIRs.push_back(ir);
      break;
    case IR::DIV:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->fVal = ir->items[1]->fVal / ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal / ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::EQ:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::INT;
        ir->items[1]->fVal = ir->items[1]->fVal == ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal == ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::F2I:
      if (ir->items[1]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::INT;
        ir->items[1]->iVal = ir->items[1]->fVal;
      }
      newIRs.push_back(ir);
      break;
    case IR::GE:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::INT;
        ir->items[1]->fVal = ir->items[1]->fVal >= ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal >= ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::GT:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::INT;
        ir->items[1]->fVal = ir->items[1]->fVal > ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal > ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::I2F:
      if (ir->items[1]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::FLOAT;
        ir->items[1]->fVal = ir->items[1]->iVal;
      }
      newIRs.push_back(ir);
      break;
    case IR::L_NOT:
      if (ir->items[1]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::INT;
        ir->items[1]->iVal = !ir->items[1]->fVal;
      } else if (ir->items[1]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = !ir->items[1]->iVal;
      }
      newIRs.push_back(ir);
      break;
    case IR::LE:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::INT;
        ir->items[1]->fVal = ir->items[1]->fVal <= ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal <= ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::LT:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::INT;
        ir->items[1]->fVal = ir->items[1]->fVal < ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal < ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::MOD:
      if (ir->items[1]->type == IRItem::INT &&
          ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal % ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::MUL:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->fVal = ir->items[1]->fVal * ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal * ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::NE:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->type = IRItem::INT;
        ir->items[1]->fVal = ir->items[1]->fVal != ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal != ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    case IR::NEG:
      if (ir->items[1]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->fVal = -ir->items[1]->fVal;
      } else if (ir->items[1]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = -ir->items[1]->iVal;
      }
      newIRs.push_back(ir);
      break;
    case IR::SUB:
      if (ir->items[1]->type == IRItem::FLOAT &&
          ir->items[2]->type == IRItem::FLOAT) {
        ir->type = IR::MOV;
        ir->items[1]->fVal = ir->items[1]->fVal - ir->items[2]->fVal;
        ir->items.resize(2);
      } else if (ir->items[1]->type == IRItem::INT &&
                 ir->items[2]->type == IRItem::INT) {
        ir->type = IR::MOV;
        ir->items[1]->iVal = ir->items[1]->iVal - ir->items[2]->iVal;
        ir->items.resize(2);
      }
      newIRs.push_back(ir);
      break;
    default:
      newIRs.push_back(ir);
      break;
    }
  }
  irs = newIRs;
}