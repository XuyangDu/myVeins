/*
 * IRS.h
 *
 *  Created on: Dec 8, 2021
 *      Author: duxuyang
 */

#ifndef SRC_VEINS_XYDU_PHYIRS_H_
#define SRC_VEINS_XYDU_PHYIRS_H_

#include "veins/veins.h"

#include "veins/modules/phy/PhyLayer80211p.h"
#include "veins/base/utils/Coord.h"
#include "veins/base/connectionManager/BaseConnectionManager.h"

namespace veins {

class VEINS_API PhyIRS : public PhyLayer80211p {

public:
    Coord pos;

protected:

    cMessage* reflect = nullptr;
    AirFrame* frameToSend;


    void initialize(int stage) override;
    void handleSelfMessage(cMessage* msg) override;
    void handleAirFrame(AirFrame* frame) override;

};




}


#endif /* SRC_VEINS_XYDU_PHYIRS_H_ */
