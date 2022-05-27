/*
 * OutGate.h
 *
 *  Created on: Dec 8, 2021
 *      Author: duxuyang
 */

#ifndef SRC_VEINS_XYDU_OUTGATE_H_
#define SRC_VEINS_XYDU_OUTGATE_H_

#include "veins/veins.h"
#include "veins/base/modules/BaseModule.h"


namespace veins {

class VEINS_API OutGate : public BaseModule{
protected:

    void initialize(int stage) override;
    void handleMessage(cMessage*) override;
    void finish() override;

};




}



#endif /* SRC_VEINS_XYDU_OUTGATE_H_ */
