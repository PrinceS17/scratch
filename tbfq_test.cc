/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 NITK Surathkal
 *
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
 *
 * Author: Jinhui Song <jinhuis2@illinois.edu>
 *
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/traffic-control-module.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <map>
#include <vector>

using namespace std;
using namespace ns3;

fstream tokenOut;

// tool function to obtain the source IP address
Ipv4Address getIpSrcAddr (Ptr<const Packet> p)   // ip layer information, work for both TCP and UDP
{
    Ptr<Packet> pcp = p->Copy();
    PppHeader pppH;
    Ipv4Header ipH;
    pcp->RemoveHeader(pppH);
    pcp->RemoveHeader(ipH);
    return ipH.GetSource();
}

vector<uint32_t> getPktSize (Ptr<const Packet> p)
{
    vector<uint32_t> res;
    Ptr<Packet> pcp = p->Copy();
    PppHeader pppH;
    Ipv4Header ipH;
    res.push_back(pcp->GetSize());
    pcp->RemoveHeader(pppH);
    res.push_back(pcp->GetSize());
    return res;
}

void FirstBucketTrace (uint32_t oldV, uint32_t newV)
{
    double time = Simulator::Now().GetSeconds();
    cout << time << ":        # tokens in 1st bucket:    " << newV << endl;
    tokenOut << time << " " << newV << endl;
}

void onRightRx (Ptr<const Packet> p)
{
    Ipv4Address srcAddr = getIpSrcAddr (p);
    cout << Simulator::Now().GetSeconds() << ": - onRightRx. from ";
    srcAddr.Print(cout);
    cout << " (" << getPktSize (p)[0] << " B) " << endl;
}

void onLeftRx (Ptr<const Packet> p)
{
    Ipv4Address srcAddr = getIpSrcAddr (p);
    cout << Simulator::Now().GetSeconds() << ": - onLeftRx. from ";
    srcAddr.Print(cout);
    cout << " (" << getPktSize (p)[0] << " B) " << endl;
}

int main()
{
    // link settings
    uint32_t nLeaf = 2;
    string txRate = "8kbps";    // app sending rate
    uint32_t port = 5001;
    string linkBw = "16kbps";
    string linkDelay = "2ms";
    double tStop = 20.0;
    
    // tbf queue settings
    QueueSizeUnit mode = QueueSizeUnit::PACKETS;    // directly set in code
    uint32_t pktSize = 1000;
    uint32_t modeSize = 1;      // 1: count by pkt; size: count by byte
    uint32_t qSize = 10;        // 100 by ns-3 default
    uint32_t burst = 5000;      // 1st bucket size
    uint32_t mtu = 0;           // 2nd bucket size (0 if none)
    DataRate token_rate = DataRate ("2KB/s");   // i.e. leak rate of 1st bucket
    DataRate peakRate = DataRate ("0KB/s");     // i.e. leak rate of 2nd bucket

    if (mode == QueueSizeUnit::BYTES)
    {
        modeSize = pktSize;
        qSize *= modeSize;
    }

    // create the point-to-point link
    PointToPointHelper link;
    link.SetDeviceAttribute ("DataRate", StringValue (linkBw));
    link.SetChannelAttribute ("Delay", StringValue (linkDelay));
    PointToPointHelper leaf;
    leaf.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    leaf.SetChannelAttribute ("Delay", StringValue ("1ms"));
    PointToPointDumbbellHelper d (nLeaf, leaf, nLeaf, leaf, link);

    // install stack
    InternetStackHelper stack;
    d.InstallStack (stack);

    // set TBF queue (note: traffic from right to left)
    TrafficControlHelper tch;
    tch.SetRootQueueDisc ("ns3::TbfQueueDisc",
                          "MaxSize", QueueSizeValue (QueueSize (mode, qSize)),
                          "Burst", UintegerValue (burst),
                          "Mtu", UintegerValue (mtu),
                          "Rate", DataRateValue (token_rate),
                          "PeakRate", DataRateValue (peakRate));
    tch.Install (d.GetLeft ()->GetDevice (0));
    QueueDiscContainer qdc = tch.Install (d.GetRight ()->GetDevice (0));

    // assign IP addresses
    d.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                           Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"),
                           Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));

    // sink app
    Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper psh ("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < d.LeftCount (); ++i)
        sinkApps.Add (psh.Install (d.GetLeft (i)));
    sinkApps.Start (Seconds (0.0));
    sinkApps.Stop (Seconds (tStop));

    // client app
    OnOffHelper ooh ("ns3::UdpSocketFactory", Address ());
    ooh.SetAttribute ("DataRate", StringValue (txRate));
    ooh.SetAttribute ("PacketSize", UintegerValue (pktSize));
    ooh.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=1.]"));
    ooh.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=0.]"));
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < d.RightCount (); ++i)
    {
        AddressValue sinkAddr (InetSocketAddress (d.GetLeftIpv4Address (i), port));
        ooh.SetAttribute ("Remote", sinkAddr);
        clientApps.Add (ooh.Install (d.GetRight (i)));
    }
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (tStop));

    // output token size for visualization
    tokenOut.open ("token_out.dat", ios::out);

    // tracing: right router's MacRx, queue's token, left router's MacRx
    qdc.Get (0)->TraceConnectWithoutContext ("TokensInFirstBucket", MakeCallback (&FirstBucketTrace));
    for (uint32_t i = 0; i < d.RightCount (); ++i)
        d.GetRight ()->GetDevice (i + 1)->TraceConnectWithoutContext ("MacRx", MakeCallback (&onRightRx));
    d.GetLeft ()->GetDevice (0)->TraceConnectWithoutContext ("MacRx", MakeCallback (&onLeftRx));

    // set simulation
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    cout << "Running the simulation ... " << endl;
    Simulator::Stop (Seconds (tStop));
    Simulator::Run ();

    cout << "\nDestroying the simulation ... " << endl;
    tokenOut.close ();
    Simulator::Destroy ();
    return 0;
}