/*
 * IRSInfo.h
 *
 *  Created on: Dec 6, 2021
 *      Author: duxuyang
 */

#ifndef SRC_VEINS_XYDU_IRSINFO_H_
#define SRC_VEINS_XYDU_IRSINFO_H_

#pragma once

#include <string>

#include "veins/veins.h"
#include "veins/base/utils/SimpleAddress.h"

namespace veins {

class VEINS_API IRSInfo {
protected:
    LAddress::L2Type srcAddr;
    LAddress::L2Type destAddr;
    int IRSIndex;
    int IRSNum;
    int lastNode;
    int hop;

public:
    IRSInfo()
        : srcAddr(-1)
        , destAddr(-1)
        , IRSIndex(-1)
        , IRSNum(0)
        , lastNode(-1)
        , hop(0)
        {
        }
    IRSInfo(const LAddress::L2Type Addr1, const LAddress::L2Type Addr2, int id, int Node, int h)
        : srcAddr(Addr1)
        , destAddr(Addr2)
        , IRSIndex(id)
        , IRSNum(0)
        , lastNode(Node)
        , hop(h)
        {
        }

    LAddress::L2Type getSrcAddr()
    {
        return srcAddr;
    }

    LAddress::L2Type getDestAddr()
    {
        return destAddr;
    }

    int getIRSIndex()
    {
        return IRSIndex;
    }

    int getIRSNum()
    {
        return IRSNum;
    }
    int getlastNode()
    {
        return lastNode;
    }
    int gethop()
    {
        return hop;
    }

    virtual ~IRSInfo(){}
};

} // namespace veins

#endif /* SRC_VEINS_XYDU_IRSINFO_H_ */
