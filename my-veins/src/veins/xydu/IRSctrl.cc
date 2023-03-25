/*
 * IRSctrl.cc
 *
 *  Created on: Sep 12, 2022
 *      Author: xydu
 */

#include "veins/xydu/IRSctrl.h"
#include "veins/modules/application/traci/MyVeinsApp.h"

using namespace veins;

Define_Module(veins::IRSctrl);

void IRSctrl::addWallLines()
{
    wallLines.push_back(Line(Coord(35, 35), Coord(35, 215)));
    wallLines.push_back(Line(Coord(35, 35), Coord(215, 35)));
    wallLines.push_back(Line(Coord(35, 215), Coord(215, 215)));
    wallLines.push_back(Line(Coord(215, 35), Coord(215, 215)));

    wallLines.push_back(Line(Coord(235, 35), Coord(235, 215)));
    wallLines.push_back(Line(Coord(235, 35), Coord(415, 35)));
    wallLines.push_back(Line(Coord(235, 215), Coord(415, 215)));
    wallLines.push_back(Line(Coord(415, 35), Coord(415, 215)));

    wallLines.push_back(Line(Coord(35, 235), Coord(35, 415)));
    wallLines.push_back(Line(Coord(35, 235), Coord(215, 235)));
    wallLines.push_back(Line(Coord(35, 415), Coord(215, 415)));
    wallLines.push_back(Line(Coord(215, 235), Coord(215, 415)));

    wallLines.push_back(Line(Coord(235, 235), Coord(235, 415)));
    wallLines.push_back(Line(Coord(235, 235), Coord(415, 235)));
    wallLines.push_back(Line(Coord(235, 415), Coord(415, 415)));
    wallLines.push_back(Line(Coord(415, 235), Coord(415, 415)));
}

bool IRSctrl::isDeadNode(int nodeId, int lastId)
{
    if(nodeId == lastId)
    {
        return false;
    }
    for(int i = 0 ; i < 1000; i++)
    {
        if(getParentModule()->getSubmodule("node", i))
        {
            cModule *simMod = getParentModule();
            LAddress::L2Type nodeNic = simMod->getSubmodule("node", nodeId)->getSubmodule("nic")->getId();
            Coord nodePos = cc->getNicPos(nodeNic);
            LAddress::L2Type iNic = simMod->getSubmodule("node", i)->getSubmodule("nic")->getId();
            Coord iPos = cc->getNicPos(iNic);
            if(!isBlocked(nodePos, iPos))
            {
                if(getForward(nodeId, i, lastId) > 0)
                {
                    return false;
                }
            }
        }
    }
    return true;
}

bool IRSctrl::isBlocked(Coord sendPos, Coord nodePos)
{
    if(sendPos.distance(nodePos) > 35) {
        return true;
    }
    Line line = Line(sendPos, nodePos);
    for(Line wall : wallLines) {
        if(line.intersectWith(wall)) {
            return true;
        }
    }
    return false;
}

double IRSctrl::getForward(int sendId, int nodeId, int lastId)
{
    if(nodeId == lastId)
    {
        return INT_MAX;
    }
    cModule *simMod = getParentModule();
    LAddress::L2Type sendNic = simMod->getSubmodule("node", sendId)->getSubmodule("nic")->getId();
    Coord sendPos = cc->getNicPos(sendNic);
    LAddress::L2Type nodeNic = simMod->getSubmodule("node", nodeId)->getSubmodule("nic")->getId();
    Coord nodePos = cc->getNicPos(nodeNic);
    LAddress::L2Type lastNic = simMod->getSubmodule("node", lastId)->getSubmodule("nic")->getId();
    Coord lastPos = cc->getNicPos(lastNic);
    lastPos.x = lastPos.x < 125 ? 25 : (lastPos.x < 325 ? 225 : 425);
    lastPos.y = lastPos.y < 125 ? 25 : (lastPos.y < 325 ? 225 : 425);
    double dis1 = fabs(sendPos.x - lastPos.x) + fabs(sendPos.y - lastPos.y);
    double dis2 = fabs(nodePos.x - lastPos.x) + fabs(nodePos.y - lastPos.y);
    return dis1 - dis2;
}

std::vector<int> IRSctrl::getIdleNodes()
{
    std::vector<int> res(0);
    int nMax = 1000;
    for(int i = 0; i < nMax; i++) {
        if(getParentModule()->getSubmodule("node", i))
        {
            int j = 0;
            for(j = 0; j < tasks.size(); j++) {
                if(i == tasks[j].sendNode) {
                    break;
                }
            }
            if(j == tasks.size()) {
                res.push_back(i);
            }
        }
    }
    return res;
}

void IRSctrl::initialize(int stage)
{
    cSimpleModule::initialize(stage);
    if (stage == 0) {
        cc = dynamic_cast<BaseConnectionManager*>(getParentModule()->getSubmodule("connectionManager"));
        beginCalculateEvt = new cMessage("calculate evt", BEGIN_CALCULATE_EVT);
        generatedPackets = 0;
        receivedPackets = 0;
        hopsCount = 0;
        used = std::vector<bool>(24, false);
        tasks = std::vector<Task>();
        addWallLines();
        EV << "Initializing IRSctrl" << std::endl;
    }
}

void IRSctrl::addTask(Task task)
{
    if(task.hop >= 40) {
        return;
    }
    if (!beginCalculateEvt->isScheduled())
    {
        if(task.hop == 1) {
            tasks = std::vector<Task>();
        }
        scheduleAt(simTime() + 1e-3, beginCalculateEvt);
    }
    tasks.push_back(task);
}

void IRSctrl::calculation()
{   //Remaining work to be done

    std::vector<bool> setted(tasks.size(), false);
    std::vector<int> idleNodes = getIdleNodes();
    std::vector<Coord> irsPos = {Coord(34, 34), Coord(34, 234), Coord(34, 416)
            , Coord(234, 34), Coord(234, 234), Coord(234, 416)
            , Coord(416, 34), Coord(416, 234), Coord(416, 416)};
//    std::vector<Coord> irsPos = {Coord(34, 125), Coord(34, 325), Coord(234, 125)
//            , Coord(234, 325), Coord(416, 125), Coord(416, 325)
//            , Coord(125, 34), Coord(325, 34), Coord(125, 234)
//            , Coord(325, 234), Coord(125, 416), Coord(325, 416)};
    std::vector<std::pair<Task, double>> directTasks;

    for(int i = 0; i < tasks.size(); i++)
    {
        int sendId = tasks[i].sendNode;
        int lastId = tasks[i].lastNode;

        for(int nodeId : idleNodes)
        {
            cModule *simMod = getParentModule();
            LAddress::L2Type sendNic = simMod->getSubmodule("node", sendId)->getSubmodule("nic")->getId();
            Coord sendPos = cc->getNicPos(sendNic);
            LAddress::L2Type nodeNic = simMod->getSubmodule("node", nodeId)->getSubmodule("nic")->getId();
            Coord nodePos = cc->getNicPos(nodeNic);
            if(!isBlocked(sendPos, nodePos) && !isDeadNode(nodeId, lastId))
            {
                double forward = getForward(sendId, nodeId, lastId);
                if(forward < 0) {
                    continue;
                }
                Task taskTemp = Task(sendId, nodeId, lastId, -1, tasks[i].hop);
                directTasks.push_back({taskTemp, forward});
            }
//            for(int s = 0; s < irsPos.size(); s++)
//            {
//                if(!isBlocked(sendPos, irsPos[s]) && !isBlocked(irsPos[s], nodePos) && !isDeadNode(nodeId, lastId))
//                {
//                    double forward = getForward(sendId, nodeId, lastId);
//                    if(forward < 0) {
//                        continue;
//                    }
//                    Task taskTemp = Task(sendId, nodeId, lastId, s, tasks[i].hop);
//                    directTasks.push_back({taskTemp, forward - sendPos.distance(irsPos[s]) * nodePos.distance(irsPos[s]) / 1e4});
//                }
//            }
        }
    }
    std::sort(directTasks.begin(), directTasks.end(), [](std::pair<Task, double> a, std::pair<Task, double> b){
        return a.second > b.second;
    });

    std::vector<std::pair<Task, double>> directTasksTemp(0);

    while(!directTasks.empty())
    {
        Task taskTemp = directTasks[0].first;
        for(int i = 0; i < tasks.size(); i++)
        {
            if(!setted[i] && tasks[i].sendNode == taskTemp.sendNode)
            {
                tasks[i].desNode = taskTemp.desNode;
                tasks[i].IRSIndex = taskTemp.IRSIndex;
                MyVeinsApp *appl = dynamic_cast<MyVeinsApp*>(getParentModule()->getSubmodule("node", tasks[i].sendNode)->getSubmodule("appl"));
                appl->setTask(tasks[i]);
                setted[i] = true;

                int usedNode1 = tasks[i].sendNode;
                int usedNode2 = tasks[i].desNode;
                directTasksTemp = std::vector<std::pair<Task, double>>();
                for(auto p : directTasks)
                {
                    Task taskTest = p.first;
                    if(taskTest.sendNode == usedNode1 or taskTest.sendNode == usedNode2
                            or taskTest.desNode == usedNode1 or taskTest.desNode == usedNode2)
                    {
                        continue;
                    }
                    directTasksTemp.push_back(p);
                }
                directTasks = directTasksTemp;
                break;
            }
        }
    }

    std::vector<Task> tasks0;
    for(int i = 0; i < setted.size(); i++)
    {
        if(setted[i])
        {
            continue;
        }
        tasks[i].hop++;
        tasks0.push_back(tasks[i]);
    }
    tasks = tasks0;
    used = std::vector<bool>(24, false);
}

void IRSctrl::handleMessage(cMessage* msg)
{
    if (msg->getKind() == BEGIN_CALCULATE_EVT)
    {
        calculation();
    }
    else
    {
        EV_WARN << "IRSctrl: Error: Got Message of unknown kind! Name: " << msg->getName() << endl;
    }
}

void IRSctrl::finish()
{
    EV << "generatedPackets:" << generatedPackets << endl;
    EV << "receivedPackets" << receivedPackets << endl;
    EV << "hopsCount" << hopsCount << endl;
    recordScalar("generatedPackets", generatedPackets);
    recordScalar("receivedPackets", receivedPackets);
    recordScalar("hopsCount", hopsCount);
    cSimpleModule::finish();
}

IRSctrl::~IRSctrl()
{
    cancelAndDelete(beginCalculateEvt);
}

