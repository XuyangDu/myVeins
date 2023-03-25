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


using namespace veins;

Define_Module(veins::MyVeinsApp);

int MyVeinsApp::getLastNode(int node) {
    if(node < 240)
    {
        return -1;
    }
    cModule *simMod = getParentModule()->getParentModule();
    Coord nodePos = cc->getNicPos(simMod->getSubmodule("node", node)->getSubmodule("nic")->getId());
    int y = (node / 24 - 1) * 24;
    for(int i = 0; i < 24; i++) {
        if(irsCtrl->used[i])
        {
            continue;
        }
        int iId = y + i;
        Coord iPos = cc->getNicPos(simMod->getSubmodule("node", iId)->getSubmodule("nic")->getId());
        if(nodePos.distance(iPos) > 500)
        {
            irsCtrl->used[i] = true;
            return iId;
        }
    }
    return -1;
}

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
        task = Task();
        irsCtrl = dynamic_cast<IRSctrl*>(getParentModule()->getParentModule()->getSubmodule("ctrl"));
        cc = dynamic_cast<BaseConnectionManager*>(getParentModule()->getParentModule()->getSubmodule("connectionManager"));
        hopsCount = 0;
        countTask = 0;
        genTaskEvt = new cMessage("gen task evt", GEN_TASK_EVT);
        sendWSMEvt = new cMessage("wsm evt", SEND_WSM_EVT);
        scheduleAt(simTime() + 1e-6, genTaskEvt);
    }
}

void MyVeinsApp::setTask(Task task_) {
    task = task_;
    scheduleAt(simTime() + 1e-6, sendWSMEvt);
}

void MyVeinsApp::finish()
{
    recordScalar("hopsCount", hopsCount);
    DemoBaseApplLayer::finish();
    // statistics recording goes here
}

void MyVeinsApp::onWSM(BaseFrame1609_4* wsm)
{
    // Your application has received a data message from another car or RSU
    // code for handling the message goes here, see TraciDemo11p.cc for examples
    TraCIDemo11pMessage* wsm0 = check_and_cast<TraCIDemo11pMessage*>(wsm);
    if (getSimulation()->getModule(wsm0->getSenderAddress())) {
        IRSInfo irsInfo = wsm0->getIrsInfo();
        int nodeId = getParentModule()->getIndex();
        if(irsInfo.getlastNode() == nodeId) {
            hopsCount = irsInfo.gethop();
            irsCtrl->hopsCount += irsInfo.gethop();
            irsCtrl->receivedPackets++;
        }
        else {
            task = Task(nodeId, -1, irsInfo.getlastNode(), -1, irsInfo.gethop() + 1);
            irsCtrl->addTask(task);
        }
        EV << "Node "<< getParentModule()->getIndex() << " successfully received data from Node " << getSimulation()->getModule(wsm0->getSenderAddress())->getParentModule()->getIndex() << std::endl;
    }
}

void MyVeinsApp::handleSelfMsg(cMessage* msg)
{
    // this method is for self messages (mostly timers)
    // it is important to call the DemoBaseApplLayer function for BSM and WSM transmission
    // DemoBaseApplLayer::handleSelfMsg(msg);
    if (msg->getKind() == SEND_WSM_EVT) {
        MacToPhyInterface* phySrc;
        phySrc = FindModule<MacToPhyInterface*>::findSubModule(getParentModule()->getSubmodule("nic"));

        int nodeId = getParentModule()->getIndex();

        cModule *simMod = getParentModule()->getParentModule();
        MacToPhyInterface* phyDest;
        phyDest = FindModule<MacToPhyInterface*>::findSubModule(simMod->getSubmodule("node", task.desNode)->getSubmodule("nic"));

        EV << "TX_BEGIN Node " << nodeId << " change from " << phySrc->getRadioState() << " to ";
        phySrc->setRadioState(Radio::TX);
        EV << phySrc->getRadioState() << std::endl;

        EV << "RX_BEGIN Node " << task.desNode << " change from " << phyDest->getRadioState() << " to ";
        phyDest->setRadioState(Radio::RX);
        EV << phyDest->getRadioState() << std::endl;

        EV << "hop " << task.hop << ": sending from Node " << nodeId << " to Node "<< task.desNode << " final destination is Node " << task.lastNode << std::endl;

        LAddress::L2Type rcvId;
        rcvId = simMod->getSubmodule("node", task.desNode)->getSubmodule("nic")->getId();
        IRSInfo irsInfo = IRSInfo(myId, rcvId, task.IRSIndex, task.lastNode, task.hop);
        TraCIDemo11pMessage* wsm = new TraCIDemo11pMessage();
        wsm->setSenderAddress(myId);
        populateWSM(wsm, rcvId, irsInfo);
        sendDown(wsm);
    }
    else if (msg->getKind() == GEN_TASK_EVT) {
        int nodeId = getParentModule()->getIndex();
        int last = getLastNode(nodeId);
        if(last != -1)
        {
            task = Task(nodeId, -1, last, -1, 1);
            cModule *simMod = getParentModule()->getParentModule();
            irsCtrl->addTask(task);
            irsCtrl->generatedPackets++;

            countTask++;
            if(countTask < 1) {
                scheduleAt(simTime() + 1 + 1e-6, genTaskEvt);
            }
        }
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
