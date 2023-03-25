/*
 * IRSctrl.h
 *
 *  Created on: Sep 12, 2022
 *      Author: xydu
 */

#ifndef SRC_VEINS_XYDU_IRSCTRL_H_
#define SRC_VEINS_XYDU_IRSCTRL_H_

#include "veins/veins.h"
#include "veins/base/connectionManager/BaseConnectionManager.h"

namespace veins {

struct Task {
    Task()
    : sendNode(-1)
    , desNode(-1)
    , lastNode(-1)
    , IRSIndex(-1)
    , hop(1)
    {
    }
    Task(int send, int des, int last, int IRS, int h)
    : sendNode(send)
    , desNode(des)
    , lastNode(last)
    , IRSIndex(IRS)
    , hop(h)
    {
    }
    int sendNode;
    int desNode;
    int lastNode;
    int IRSIndex;
    int hop;
};

struct Line {
    // y = ax + b, x in [p1.x,p2.x]
    Line()
    : exist(true)
    , a(0)
    , b(0)
    , p1(Coord())
    , p2(Coord())
    {
    }
    Line(bool exist_, double a_, double b_, Coord p1_, Coord p2_)
    : exist(exist_)
    , a(a_)
    , b(b_)
    , p1(p1_)
    , p2(p2_)
    {
    }
    Line(Coord p1_, Coord p2_) {
        if(p1_.x > p2_.x) {
            std::swap(p1_, p2_);
        }
        if(p1_.x == p2_.x) {
            exist = false;
            a = 0;
            b = 0;
            p1 = p1_;
            p2 = p2_;
        }
        else {
            double a_ = (p1_.y - p2_.y) / (p1_.x - p2_.x);
            double b_ = (p1_.x * p2_.y - p2_.x * p1_.y) / (p1_.x - p2_.x);
            exist = true;
            a = a_;
            b = b_;
            p1 = p1_;
            p2 = p2_;
        }
    }
    bool exist;
    double a;
    double b;
    Coord p1;
    Coord p2;

    bool intersectWith(const Line line)
    {
        if(a == line.a || !exist && !line.exist)
        {
            return false;
        }
        Coord cross;
        if(!exist)
        {
            cross = Coord(p1.x, line.a * p1.x + line.b);

        }
        else if(!line.exist)
        {
            cross = Coord(line.p1.x, a * line.p1.x + b);
        }
        else
        {
            double crossX = (line.b - b) / (a - line.a);
            double crossY = (a * line.b - line.a * b) / (a - line.a);
            cross = Coord(crossX, crossY);
        }
        if(cross.x >= std::min(p1.x, p2.x) - 1e-6 && cross.x <= std::max(p1.x, p2.x) + 1e-6
        && cross.y >= std::min(p1.y, p2.y) - 1e-6 && cross.y <= std::max(p1.y, p2.y) + 1e-6
        && cross.x >= std::min(line.p1.x, line.p2.x) - 1e-6 && cross.x <= std::max(line.p1.x, line.p2.x) + 1e-6
        && cross.y >= std::min(line.p1.y, line.p2.y) - 1e-6 && cross.y <= std::max(line.p1.y, line.p2.y) + 1e-6)
        {
            return true;
        }
        else {
            return false;
        }
    }

    bool passThrough(const Coord p)
    {
        return false;
    }
};


class VEINS_API IRSctrl : public cSimpleModule {
public:
    void addTask(Task task);
    enum IRSctrlMessageKinds {
        BEGIN_CALCULATE_EVT
    };
    ~IRSctrl() override;
    uint32_t generatedPackets;
    uint32_t receivedPackets;
    uint32_t hopsCount;

    std::vector<bool> used;

protected:
    void handleMessage(cMessage* msg) override;
    void initialize(int stage) override;
    void calculation();

    BaseConnectionManager* cc;

    cMessage* beginCalculateEvt;

    std::vector<int> getIdleNodes();
    bool isBlocked(Coord sendPos, Coord nodePos);
    bool isDeadNode(int nodeId, int lastId);
    double getForward(int sendId, int nodeId, int lastId);
    void addWallLines();

    std::vector<Task> tasks;
    std::vector<Line> wallLines;

    void finish() override;
};
}

#endif /* SRC_VEINS_XYDU_IRSCTRL_H_ */
