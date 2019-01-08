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


#ifndef MBOX_H
#define MBOX_H

#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

#include "ns3/point-to-point-layout-module.h"
#include "ns3/rtt-estimator.h"
#include "ns3/nstime.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/random-variable-stream.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/traffic-control-module.h"
#include "ns3/point-to-point-net-device.h"

#include "ns3/tag.h"
#include "tools.h"
#include "apps.h"

#include <iomanip>
#include <fstream>
#include <ctime>
#include <locale>
#include <time.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <numeric>
#include <map>
#include <cstdio>
#include <stdlib.h>

using namespace std;
using namespace ns3;

// enum ProtocolType {TCP, UCP};
namespace ns3 {


class MiddlePoliceBox
{
public:
    /**
     * \brief Initialize the rwnd, cwnd and start monitoring and control.
     * 
     * \param num The array of nSender, nReceiver, nClient and nAttacker.
     * \param size The packet size.
     * \param beta Average parameter in llr calculation.
     * \param th Loss rate threshold.
     * \param isEDrop if enable SetEarlyDrop of the net device.
     */
    MiddlePoliceBox (vector<uint32_t> num, double tStop, ProtocolType prot, double beta = 0.8, double th = 0.05, uint32_t wnd = 50, bool isEDrop = true);
    MiddlePoliceBox () {}
    /** 
     * rewrite to construct the ofstream manually to fullfill the 
     * copy-constructible requirement. 
     * */
    MiddlePoliceBox (const MiddlePoliceBox & );
    ~MiddlePoliceBox ();
    

    friend bool operator == (const MiddlePoliceBox& lhs, const MiddlePoliceBox& rhs)
    { return lhs.MID == rhs.MID; }
    friend bool operator != (const MiddlePoliceBox& lhs, const MiddlePoliceBox& rhs)
    { return lhs.MID != rhs.MID; }
    /**
     * \brief Install this mbox to given net device.
     * 
     * \param NetDevice The device that we install mbox in.
     */
    void install(Ptr<NetDevice> device);
    /**
     * \brief Install the mbox with tx router's rx side to set early drop at MacRx.
     * 
     * \param nc The rx net devices of the tx router.
     */
    void install(NetDeviceContainer nc);
    /**
     * \brief This method is called when a packet arrives and is ready to send.
     * In this method, receive window of mbox will be updated and it will set
     * early drop according to the loss rate.
     * NOTE: no need for onMacTxDrop since mDrop is updated here.
     * 
     * \param packet The packet arrived.
     */
    void txSink(Ptr<const Packet> p);   // !< for debug only
    void rxDrop(Ptr<const Packet> p);   // !< for debug only
    void onMacTx(Ptr<const Packet> p);
    /**
     * \brief This method is called when a packet arrives from all channels.
     * In this method, receive window of mbox will be updated and it will set
     * early drop according to the loss rate.
     * NOTE: no need for onMacTxDrop since mDrop is updated here.
     * 
     * \param packet The packet arrived.
     */
    void onMacRx(Ptr<const Packet> p);
    /**
     * \brief Called similar to mac tx except the mbox is paused and thus doesn't drop 
     * packet to control the flow.
     * 
     * \param packet The packet arrived.
     */
    void onMacTxWoDrop(Ptr<const Packet> p);
    /**
     * \brief Method is called when a packet is dropped by queue (e.g. RED), which 
     * means the link is dropping packet. So # link drop (lDrop) will be updated here.
     * 
     * \param packet The packet dropped.
     */
    void onQueueDrop(Ptr<const QueueDiscItem> qi);
    /**
     * \brief Method is called when a packet is received by destination (e.g. dst router).
     * # RX packet (rwnd) will be updated. And also update SLR when hitting slrWnd (periodically
     * with # RX packet but not time). 
     * 
     * \param packet The packet received by destination.
     */
    void onPktRx(Ptr<const Packet> p);
    /**
     * \brief Reset all the window value to start a new detect period.
     */
    void clear();
    /**
     * \brief Method is called to periodically monitor drop rate and control the flow.
     * Therefore, long term loss rate (llr) is calculated and cwnd's are updated here.
     * rwnd, lDrop, mDrop, cwnd are all refreshed at last here.
     * It also starts of all mbox functionality.
     * 
     * \param fairness Fairness set in prior, e.g. per-sender share fair.
     * \param interval Time interval between this and next calls, i.e. detect period.
     */
    void flowControl(FairType fairness, double interval, double logInterval);
    /**
     * \brief Method is called to periodically update and output statistics, such as 
     * data rate, drop rate of all the links' good put for visualization and analysis.
     * 
     * \param interval Time interval between this and next data collection.
     */
    void statistic(double interval);
    /**
     * \brief Stop the mbox function by setting the stop sign.
     */
    void stop();

public:     // values ideally know, initialize() to set
    vector<uint32_t> rwnd;      // receive window of mbox
    vector<uint32_t> lDrop;     // drop during window the link, basically RED
    vector<uint32_t> totalRx;   // total # packet received from start
    vector<uint32_t> totalRxByte;    // total rx bit (to calculate data rate)
    vector<uint32_t> totalDrop; // total # packet dropped by link from start
    uint32_t lastDrop;          // total last # drop packet (for slr)
    uint32_t lastRx;            // total last # rx packet

private:    // values can/should be known locally inside mbox
    vector<uint32_t> cwnd;          
    vector<uint32_t> mDrop;     // drop window due to mbox, basically setEarlyDrop()
    double slr;                 // short-term loss rate, total
    vector<double> llr;         // long-term loss rate
    vector<double> dRate;        // rx data rate in kbps
    vector<uint32_t> lastRx2;   // for rate calculation only
    
    uint32_t nSender;
    uint32_t nClient;       // initial # sender with normal traffic, might change (2nd stage work)
    uint32_t nAttacker;     // initial # sender with larger traffic
    uint32_t nReceiver;
    uint32_t slrWnd;        // # packet interval of slr update
    bool isEarlyDrop;
    double tStop;           // the stop time
    double beta;            // parameter for update llr
    double lrTh;            // loss rate threshold

    // internal parameters
    int MID;                            // mbox ID
    Ptr<PointToPointNetDevice> device;  // where mbox is installed
    vector< Ptr<PointToPointNetDevice> > macRxDev;     // to set early drop before queue drop
    vector< vector<string> > fnames;    // for statistics
    vector< string> singleNames;
    vector< vector<ofstream> > fout;    // for statistics: output data to file
    vector< ofstream > singleFout;      // data irrelated to sender, e.g. slr, queue size
    double normSize;                    // normalized in kbit
    ProtocolType protocol;
    bool isStop;
};

}

#endif