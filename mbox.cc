/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "ns3/mbox.h"

using namespace std;
using namespace ns3;

MiddlePoliceBox::MiddlePoliceBox(vector<uint32_t> num, double tStop, ProtocolType prot, double beta, double th, uint32_t wnd, bool isEDrop)
{
    // constant setting
    nSender = num.at(0);
    nReceiver = num.at(1);
    nClient = num.at(2);
    nAttacker = num.at(3);
    lrTh = th;
    slrWnd = wnd;
    isEarlyDrop = isEDrop;
    this->tStop = tStop;
    normSize = 8.0 / 1000;      // byte -> kbit
    protocol = prot;

    // monitor setting
    rwnd = vector<uint32_t> (nSender, 0);   // rwnd: use to record each senders' rate
    cwnd = vector<uint32_t> (nSender, 0);   // cwnd: use to control senders' rate
    lDrop = vector<uint32_t> (nSender, 0);
    mDrop = vector<uint32_t> (nSender, 0);
    totalRx = vector<uint32_t> (nSender, 0);
    totalRxByte = vector<uint32_t> (nSender, 0);
    totalDrop = vector<uint32_t> (nSender, 0);

    slr = 0.0;
    llr = vector<double> (nSender, 0.0);

    // output setting
    string folder = "MboxStatistics";
    vector<string> name = {"DataRate", "LLR"};       // open to extension
    vector<string> sname = {"SLR", "QueueSize"};       // for data irrelated to sender 
    for(uint32_t i = 0; i < nSender; i ++)
    {
        fnames.push_back(vector<string>());
        fout.push_back(vector<ofstream>());
        for(uint32_t j = 0; j < name.size(); j ++)
        {
            fnames.at(i).push_back(folder + "/" + name[j] + "_" + to_string(i) + ".dat");
            remove(fnames.at(i).at(j).c_str());
            fout.at(i).push_back(ofstream(fnames.at(i).at(j), ios::app | ios::out));
        }
    }   // need test here

    for(uint32_t i = 0; i < sname.size(); i ++)
    {
        singleNames.push_back(folder + "/" + sname[i] + "_" + ".dat");
        remove(singleNames.at(i).c_str());
        singleFout.push_back(ofstream(singleNames.at(i), ios::app | ios::out));
    }

}

MiddlePoliceBox::~MiddlePoliceBox()
{
    clear();
    for(uint32_t i = 0; i < nSender; i ++)
    {
        singleFout.at(i).close();
        for(uint32_t j = 0; j < fnames.size(); j ++)
            fout.at(i).at(j).close();
    }
}

void
MiddlePoliceBox::install(Ptr<PointToPointNetDevice> device)
{
    this->device = device;
}

void
MiddlePoliceBox::onMacTx(Ptr<const Packet> p)
{
    Ptr<const Packet> pcp = p->Copy();
    MyTag tag;
    if(!pcp->PeekPacketTag(tag)) return;
    int index = tag.GetSimpleValue() - 1;
    rwnd[index] ++;

    // best-effort packet handling
    if(isEarlyDrop && rwnd[index] > cwnd[index] && (slr > lrTh || llr[index] > lrTh))
    {
        device->SetEarlyDrop(p);
        mDrop[index] ++;
    }
}

void
MiddlePoliceBox::onQueueDrop(Ptr<const Packet> p)
{
    Ptr<const Packet> pcp = p->Copy();
    MyTag tag;
    if(!pcp->PeekPacketTag(tag)) return;
    int index = tag.GetSimpleValue() - 1;
    lDrop[index] ++;
    totalDrop[index] ++;    // for slr
}

void
MiddlePoliceBox::onPktRx(Ptr<const Packet> p)
{
    Ptr<const Packet> pcp = p->Copy();
    MyTag tag;
    if(!pcp->PeekPacketTag(tag)) return;
    int index = tag.GetSimpleValue() - 1;
    rwnd[index] ++;
    totalRx[index] ++;      // for slr update
    double bytes = getPktSizes(pcp, protocol).at(3);     // tcp bytes
    totalRxByte[index] += bytes;

    // slr update: should be proper just after updating rx
    uint32_t curRx = accumulate(totalRx.begin(), totalRx.end(), 0);
    if(curRx - lastRx == slrWnd)
    {
        uint32_t curDrop = accumulate(totalDrop.begin(), totalDrop.end(), 0);
        slr = (double) (curDrop - lastDrop) / slrWnd;
        lastRx = curRx;
        lastDrop = curDrop; 
    }
}

void
MiddlePoliceBox::clear()
{
    for(uint32_t i = 0; i < nSender; i ++)
    {
        rwnd[i] = 0;
        cwnd[i] = 0;
        lDrop[i] = 0;
        mDrop[i] = 0;
    }
}

void
MiddlePoliceBox::flowControl(FairType fairness, double interval, double logInterval)
{
    // schedule
    Simulator::Schedule(Seconds(interval), &flowControl, fairness, interval);
    Simulator::Schedule(Seconds(0.1), &statistic, logInterval);

    // loss rate update and capacity calculation
    double capacity = 0;
    for(uint32_t i = 0; i < nSender; i ++)
    {
        double lossRate = rwnd[i] > 0? (double) (lDrop[i] + mDrop[i])/rwnd[i] : 0.0;
        llr[i] = rwnd[i] > 5? (1 - beta) * lossRate + beta * llr[i] : beta * llr[i];
        double tmp = rwnd[i] - lDrop[i] - mDrop[i]; 
        capacity += tmp > 0? tmp : 0;
    }

    // flow control policy based on fairness: add cases to extend more fairness
    for(uint32_t i = 0; i < nSender; i ++)
    {
        switch(fairness)
        {
            case NATURAL:
                cwnd[i] = rwnd[i] > lDrop[i] + mDrop[i]? rwnd[i] - lDrop[i] - mDrop[i] : 0;
                break;
            case PERSENDER:
                cwnd[i] = capacity / nSender;
                break;
            default: ;
        }
    }
    clear();
}

void
MiddlePoliceBox::statistic(double interval)
{
    static vector<uint32_t> lastRx2 = vector<uint32_t>(nSender, 0);
    Simulator::Schedule(Seconds(interval), &statistic, interval);
    
    for(uint32_t i = 0; i < nSender; i ++)
    {
        // record rx data rate
        fout.at(i)[0] << Simulator::Now().GetSeconds() << " " << 
            (double)(totalRxByte[i] - lastRx2[i]) * normSize / interval << " kbps" << endl; 
        lastRx2[i] = totalRxByte[i];

       // record llr
        fout.at(i)[1] << Simulator::Now().GetSeconds() << " " << llr.at(i) << endl;
    }

    // record slr
    singleFout.at(0) << Simulator::Now().GetSeconds() << " " << slr << endl;

    // record Queue size: later version feature
}

