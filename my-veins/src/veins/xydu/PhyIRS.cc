/*
 * IRS.cc
 *
 *  Created on: Dec 8, 2021
 *      Author: duxuyang
 */

#include "veins/xydu/PhyIRS.h"
#include "veins/xydu/IRSInfo.h"


using namespace veins;

using std::unique_ptr;

Define_Module(veins::PhyIRS);

void PhyIRS::initialize(int stage) {
    PhyLayer80211p::initialize(stage);
    if (stage == 0) {
        pos = Coord(par("posX"), par("posY"));

        std::ostringstream osDisplayTag;
        cDisplayString& disp = getParentModule()->getDisplayString();
        osDisplayTag << std::fixed;
        osDisplayTag.precision(4);

        osDisplayTag << (pos.x);
        disp.setTagArg("p", 0, osDisplayTag.str().data());

        osDisplayTag.str(""); // reset
        osDisplayTag << (pos.y);
        disp.setTagArg("p", 1, osDisplayTag.str().data());

        disp.setTagArg("b", 0, 5);
        disp.setTagArg("b", 1, 5);

        // radio of IRS is always RX
        setRadioState(Radio::RX);

        antennaPosition = AntennaPosition(getId(), pos, Coord(0, 0), simTime());
    }
}

void PhyIRS::handleAirFrame(AirFrame* frame) {

    Signal& signal = frame->getSignal();

    if (usePropagationDelay) {
        simtime_t delay = simTime() - signal.getSendingStart();
        signal.setPropagationDelay(delay);
    }

// processing senderGain
    // Extract position and orientation of sender and receiver (this module) first
    const AntennaPosition receiverPosition = antennaPosition;
    const Coord receiverOrientation = antennaHeading.toCoord();
    // get POA from frame with the sender's position, orientation and antenna
    POA& senderPOA = frame->getPoa();
    const AntennaPosition senderPosition = senderPOA.pos;
    const Coord senderOrientation = senderPOA.orientation;

    // add position information to signal
    signal.setSenderPoa(senderPOA);
    signal.setReceiverPoa({receiverPosition, receiverOrientation, antenna});

    // compute gains at sender and receiver antenna
    double senderGain = senderPOA.antenna->getGain(senderPosition.getPositionAt(), senderOrientation, receiverPosition.getPositionAt());

    // add the resulting total gain to the attenuations list
    EV_TRACE << "Sender's antenna gain: " << senderGain << endl;

    signal *= senderGain;

// processing path loss
    // go on with AnalogueModels
    // attach analogue models suitable for thresholding to signal (for later evaluation)
    signal.setAnalogueModelList(&analogueModelsThresholding);

    // apply all analouge models that are *not* suitable for thresholding now
    for (auto& analogueModel : analogueModels) {
        analogueModel->filterSignal(&signal);
    }
    signal.smallerAtCenterFrequency(minPowerLevel);

// send to destine node after processing on signal
    // update frame's sender Poa as the IRS
    frame->setPoa({receiverPosition, receiverOrientation, antenna});

    frameToSend = frame;

    reflect = new cMessage("reflect", 0);
    scheduleAt(simTime(), reflect);
}


void PhyIRS::handleSelfMessage(cMessage* msg) {
    EV_TRACE << "sendToChannel: sending to gates\n";

    IRSInfo irsInfo = frameToSend->getIrsInfo();

    const auto& gateList = cc->getGateList(irsInfo.getSrcAddr());

    for (auto&& entry : gateList) {
        const auto gate = entry.second;

        Coord senderPos = antennaPosition.getPositionAt();
        Coord receiverPos = cc->getNicPos(irsInfo.getDestAddr());

        simtime_t propagationDelay = receiverPos.distance(senderPos) / BaseWorldUtility::speedOfLight();

        for (int gateIndex = gate->getBaseId(); gateIndex < gate->getBaseId() + gate->size(); gateIndex++) {
            sendDirect(frameToSend->dup(), propagationDelay, frameToSend->getDuration(), gate->getOwnerModule(), gateIndex);
        }
    }
    // Original message no longer needed, copies have been sent to all possible receivers.
    delete frameToSend;
}












