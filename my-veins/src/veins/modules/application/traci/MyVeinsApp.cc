//
// Copyright (C) 2016 David Eckhoff <david.eckhoff@fau.de>
//
// Documentation for these modules is at http://veins.car2x.org/
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "veins/modules/application/traci/MyVeinsApp.h"
#include "veins/modules/application/traci/TraCIDemo11pMessage_m.h"
#include "veins/base/phyLayer/MacToPhyInterface.h"
#include "veins/base/utils/Heading.h"
#include "veins/xydu/IRSInfo.h"

using namespace veins;

Define_Module(veins::MyVeinsApp);

void MyVeinsApp::initialize(int stage)
{
    DemoBaseApplLayer::initialize(stage);
    if (stage == 0) {
        // Initializing members and pointers of your application goes here
        EV << "Initializing " << par("appName").stringValue() << std::endl;
    }
    else if (stage == 1) {
        // Initializing members that require initialized other modules goes here
        // Application layer data generation
        sendRate = par("sendRate").doubleValue();
        if (dblrand() < sendRate) {
            sendWSMEvt = new cMessage("wsm evt", SEND_WSM_EVT);
            sendInterval = par("sendInterval").doubleValue();
            scheduleAt(simTime()+0.5, sendWSMEvt);
        }
    }
}

void MyVeinsApp::finish()
{
    DemoBaseApplLayer::finish();
    // statistics recording goes here
}

void MyVeinsApp::onBSM(DemoSafetyMessage* bsm)
{
    // Your application has received a beacon message from another car or RSU
    // code for handling the message goes here
}

void MyVeinsApp::onWSM(BaseFrame1609_4* wsm)
{
    // Your application has received a data message from another car or RSU
    // code for handling the message goes here, see TraciDemo11p.cc for examples
    TraCIDemo11pMessage* wsm0 = check_and_cast<TraCIDemo11pMessage*>(wsm);
    if (getSimulation()->getModule(wsm0->getSenderAddress())) {
        EV << "Node "<< getParentModule()->getIndex() << " successfully received data from Node " << getSimulation()->getModule(wsm0->getSenderAddress())->getParentModule()->getIndex() << std::endl;
    }
}

void MyVeinsApp::onWSA(DemoServiceAdvertisment* wsa)
{
    // Your application has received a service advertisement from another car or RSU
    // code for handling the message goes here, see TraciDemo11p.cc for examples
}


void MyVeinsApp::handleSelfMsg(cMessage* msg)
{
    // this method is for self messages (mostly timers)
    // it is important to call the DemoBaseApplLayer function for BSM and WSM transmission
    // DemoBaseApplLayer::handleSelfMsg(msg);
    if (msg->getKind() == SEND_WSM_EVT) {
        // Randomly select a vehicle as the recipient
        cModule *mod = getParentModule()->getParentModule();
        int i, nodeMax = 0;
        for(i=getParentModule()->getIndex();i<=150;i++) {
            if (mod->getSubmodule("node", i)) {
                nodeMax = i;
            }
        }
        int rand = intrand(nodeMax);
        // Get a rcvId that different with src in heading to send

        TraCIMobility* mobSrc = dynamic_cast<TraCIMobility*>(getParentModule()->getSubmodule("veinsmobility"));
        Heading headSrc = mobSrc->getHeading();

        MacToPhyInterface* phySrc;
        phySrc = FindModule<MacToPhyInterface*>::findSubModule(getParentModule()->getSubmodule("nic"));

        if (phySrc->getRadioState() == Radio::SLEEP) {
            LAddress::L2Type rcvId;
            int i, imod;
            for(i=rand;i<rand+nodeMax;i++) {
                if (i>=nodeMax)
                    imod = i - nodeMax;
                else
                    imod = i;
                if(mod->getSubmodule("node", imod) and imod != getParentModule()->getIndex()) {
                    TraCIMobility* mobDest = dynamic_cast<TraCIMobility*>(mod->getSubmodule("node", imod)->getSubmodule("veinsmobility"));
                    Heading headDest = mobDest->getHeading();

                    MacToPhyInterface* phyDest;
                    phyDest = FindModule<MacToPhyInterface*>::findSubModule(mod->getSubmodule("node", imod)->getSubmodule("nic"));

                    if(phyDest->getRadioState() == Radio::SLEEP and fabs(headSrc.toCoord() * headDest.toCoord()) < 1e-6) {
                        rcvId = mod->getSubmodule("node", imod)->getSubmodule("nic")->getId();

                        EV << "TX_BEGIN Node " << getParentModule()->getIndex() << " change from " << phySrc->getRadioState() << " to ";
                        phySrc->setRadioState(Radio::TX);
                        EV << phySrc->getRadioState() << std::endl;

                        EV << "RX_BEGIN Node " << imod << " change from " << phyDest->getRadioState() << " to ";
                        phyDest->setRadioState(Radio::RX);
                        EV << phyDest->getRadioState() << std::endl;

                        EV <<" sending from Node " << getParentModule()->getIndex() << " to Node "<< imod << std::endl;

                        IRSInfo irsInfo = IRSInfo(myId, rcvId);

                        TraCIDemo11pMessage* wsm = new TraCIDemo11pMessage();
                        wsm->setSenderAddress(myId);
                        populateWSM(wsm, rcvId, irsInfo);
                        sendDown(wsm);
                        break;
                    }
                }
            }
        }
        scheduleAt(simTime() + sendInterval, sendWSMEvt);
    }
    else {
        EV_WARN << "APP: Error: Got Self Message of unknown kind! Name: " << msg->getName() << endl;
    }
}

void MyVeinsApp::handlePositionUpdate(cObject* obj)
{
//    DemoBaseApplLayer::handlePositionUpdate(obj);
    // the vehicle has moved. Code that reacts to new positions goes here.
    // member variables such as currentPosition and currentSpeed are updated in the parent class
}
