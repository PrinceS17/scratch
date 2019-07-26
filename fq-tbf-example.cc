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
 *
 * Author: Jinhui Song<jinhuis2@illinois.edu>
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include <fstream>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("FqTbfExample");

fstream fqTbfOut;
bool if_changed = false;
QueueDiscContainer qc;

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

void TbfUpdate (uint32_t oldV, uint32_t newV)
{
    double time = Simulator::Now().GetSeconds ();
    string gap = newV > oldV? "  " : "";
    cout << time << ": " << gap << oldV << " -> " << newV << endl;
    fqTbfOut << time << " " << newV << endl;
}

void bucketUpdate (uint32_t oldV, uint32_t newV)
{
    double time = Simulator::Now().GetSeconds ();
    cout << time << ": 1st bucket: " << newV << endl;
}

void onRightRx (Ptr<const Packet> p)
{
    Ipv4Address srcAddr = getIpSrcAddr (p);
    double time = Simulator::Now().GetSeconds();
    cout << time << ": - onRightRx. from ";
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

int 
main (int argc, char *argv[])
{
    double tStop = 10;
    uint32_t nLeaf = 3;
    uint32_t burst = 10000;
    uint32_t mtu = 2000;
    string txRate = "80kbps";       // 10 pkt /s 
    string rate = "16kbps";          // 2 pkt /s
    string peakRate = "48kbps";     // 2 pkt /s    
    string linkDelay = "2ms";
    string linkRate = "320kbps";     // 40 pkt /s
    uint32_t port = 7;

    Packet::EnablePrinting();

    // topology
    PointToPointHelper leaf, link;
    leaf.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
    leaf.SetChannelAttribute ("Delay", StringValue ("2ms"));
    link.SetDeviceAttribute ("DataRate", StringValue (linkRate));
    link.SetChannelAttribute ("Delay", StringValue (linkDelay));

    PointToPointDumbbellHelper d (nLeaf, leaf, nLeaf, leaf, link);
    InternetStackHelper stack;
    d.InstallStack (stack);

    // queue tests
    TrafficControlHelper tch;
    tch.SetRootQueueDisc ("ns3::FqTbfQueueDisc", "Burst", UintegerValue (burst),
                                                 "Rate", StringValue (rate),
                                                 "Mtu", UintegerValue (mtu),
                                                 "PeakRate", StringValue (peakRate));
    qc = tch.Install (d.GetRight ()->GetDevice (0));

    d.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                           Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"),
                           Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));
    
    // sink
    Address localAddr (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper psh ("ns3::UdpSocketFactory", localAddr);
    ApplicationContainer sinkApp;
    for (uint32_t i = 0; i < d.LeftCount (); i ++)
        sinkApp.Add (psh.Install (d.GetLeft (i)));
    sinkApp.Start (Seconds (0.0));
    sinkApp.Stop (Seconds(tStop));

    // client
    OnOffHelper onoff ("ns3::UdpSocketFactory", Ipv4Address::GetAny ());
    onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
    onoff.SetAttribute ("PacketSize", UintegerValue (1000));
    onoff.SetAttribute ("DataRate", StringValue (txRate));
    ApplicationContainer apps;
    for (uint32_t i = 0; i < d.RightCount (); i ++)
    {
        AddressValue addr (InetSocketAddress (d.GetLeftIpv4Address (i), port));
        onoff.SetAttribute ("Remote", addr);
        apps.Add (onoff.Install (d.GetRight (i)));
    }
    apps.Start (Seconds (1.0));
    apps.Stop (Seconds (tStop));

    // output & tracing: queue size, pcap
    fqTbfOut.open ("fq_tbf_out.dat", ios::out);
    qc.Get (0)->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&TbfUpdate));
    for (uint32_t i = 0; i < d.RightCount (); ++i)
        d.GetRight ()->GetDevice (i + 1)->TraceConnectWithoutContext ("MacRx", MakeCallback (&onRightRx));
    d.GetLeft ()->GetDevice (0)->TraceConnectWithoutContext ("MacRx", MakeCallback (&onLeftRx));

    // usage of set new token rate
    Ptr<FqTbfQueueDisc> ftq = DynamicCast<FqTbfQueueDisc> (qc.Get (0));
    Ipv4Address ipv4 = d.GetLeftIpv4Address(0);
    cout << "Ipv4 input: ";
    ipv4.Print(cout);
    cout << endl;
    string newRate = "8kbps";      // 1 pkt /s

    Simulator::Schedule (Seconds(5), &FqTbfQueueDisc::SetTokenRate, ftq, ipv4, newRate);
    link.EnablePcapAll("fq-router");

    // test mac map
    map<Address, uint32_t> hmap;
    Address add1 = d.GetLeft(0)->GetDevice(0)->GetAddress();
    hmap[add1] = 1;
    NS_ASSERT_MSG(hmap[add1] == 1, "Map of address doesn't work!");
    NS_LOG_INFO ("Map of Address test passed!");

    // set simulation
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    cout << "Running the simulation ... " << endl;
    Simulator::Stop (Seconds (tStop));
    Simulator::Run ();

    cout << "\nDestroying the simulation ... " << endl;
    fqTbfOut.close ();
    Simulator::Destroy ();

    return 0;
}
