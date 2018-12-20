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

#ifndef MRUN_H
#define MRUN_H

# include "ns3/mbox.h"

using namespace std;
using namespace ns3;

class RunningModule
{
public:
    /**
     * \brief Initialize the running module, including setting start and stop time, the 
     * bottleneck link, the data rates and the topology. Currently should use dumpbell to
     * realize the symmetric topology.
     * 
     * \param t Vector containing start and stop time.
     * \param num Vector containing # sender and # receiver, e.g. {3,3} for 3-in-3-out topology.
     * \param pt Protocal type, TCP or UDP.
     * \param delay Bottleneck link delay.
     * \param bw Bottleneck link bandwidth.
     * \param rate Vector of different level of data rate, e.g. {1kbps, 10kbps, 1Mbps} 
     * for three groups.
     * \param size Packet size, 1000 kB by default.
     */
    RunningModule(vector<double> t, vector<double> num, ProtocolType pt, double delay, string bw, vector<string> rate, uint32_t size = 1000);
    ~RunningModule ();
    



public:         // network entity
    NodeContainer nodes;        // all nodes in the topology
    NetDeviceContainer dev;     // net devices, for trace 
    QueueDiscContainer qc;      // queue container for trace

private:        // parameters
    // basic
    uint32_t pktSize;           // 1000 kB
    double rtStop;              // stop time of this run
    ProtocolType protocol;
    double nSender;             // # network sender, different from that of mbox!
    double nReceiver;           // # network receiver

    // queue
    string qType = "RED";       // specified for queue disc
    double minTh = 100;
    double maxTh = 200;

    // link related
    string normalBw = "1Gbps";
    string bottleneckBw;
    vector<string> txRate = vector<string>();   // extend by push back
    string delay;
    string mtu = "1599";        // p2p link setting


};

#endif