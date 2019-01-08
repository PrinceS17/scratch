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

#include "ns3/apps.h"                           // including basic header, MyTag and MyApp
#include "ns3/tools.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/tag.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include <cstring>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("p2p_test");

uint32_t cnt[2] = {0};
uint32_t cnt1[2] = {0};
uint32_t cnt2[2] = {0};
uint32_t cnt3[2] = {0};

Ptr<PointToPointNetDevice> p2pDev;

void handler(QueueDiscContainer qDisc)
{
    double curt = Simulator::Now().GetSeconds();
    cout << curt << "s: # pkt in Q: " << qDisc.Get(0)->GetCurrentSize() << endl;
    Simulator::Schedule(Seconds(5.0), &handler, qDisc);
}

void
DevPktInQ(uint32_t vOld, uint32_t vNew)
{
    cout << "# device packet in queue: " << vNew << endl;
}

void
TcPktInQ(uint32_t vOld, uint32_t vNew)
{
    cout << "Tc packet in queue: " << vNew << endl;
}

void
SojournTimeTrace (Time sojournTime)
{
  std::cout << "Sojourn time " << sojournTime.ToDouble (Time::MS) << "ms" << std::endl;
}

static
void PktMacTx(Ptr<const Packet> p)
{
    Ptr<Packet> pcp = p->Copy();
    MyTag tag;
    if(pcp->PeekPacketTag(tag))
    {
        int idx = tag.GetSimpleValue();
        stringstream ss;
        ss << Simulator::Now().GetSeconds() << "s: Mac Tx: " << idx << ". " << cnt[idx - 1] ++ << endl;
        NS_LOG_INFO(ss.str());
    }
}

static
void PktMacRx(Ptr<const Packet> p)
{
    Ptr<Packet> pcp = p->Copy();
    MyTag tag;
    if(pcp->PeekPacketTag(tag))
    {
        int idx = tag.GetSimpleValue();
        stringstream ss;
        ss << Simulator::Now().GetSeconds() << "s: Mac Rx (drop): " << idx << ". " << cnt3[idx - 1] ++;
        NS_LOG_INFO(ss.str());

        // for test
        p2pDev->SetEarlyDrop(true);

    }
}

static
void PktDrop(Ptr<const Packet> p)
{
    Ptr<Packet> pcp = p->Copy();
    MyTag tag;
    if(pcp->PeekPacketTag(tag))
    {
        int idx = tag.GetSimpleValue();
        stringstream ss;
        ss << "! " << Simulator::Now().GetSeconds() << "s: Drop: " << idx << ". " << ++ cnt1[idx - 1] << endl;
        NS_LOG_INFO(ss.str());
    }  
}

static
void QueueOp(Ptr<const QueueDiscItem> qi)
{
    // q->GetNBytes();
    stringstream ss;
    ss << " " << Simulator::Now().GetSeconds() << "s: Queue op. ";
    ss << qi->GetPacket()->GetUid();
    NS_LOG_INFO(ss.str());
}


static
void QueueDrop(Ptr<const QueueDiscItem> qi)
{
    // q->GetNBytes();
    Ptr<Packet> p = qi->GetPacket();
    MyTag tag;
    if(p->PeekPacketTag(tag))
    {
        int idx = tag.GetSimpleValue();
        stringstream ss;
        ss << " " << Simulator::Now().GetSeconds() << "s: Queue drop! " << idx << ". " << ++ cnt2[idx - 1];
        NS_LOG_INFO(ss.str());
    }  
}

int
main(int argc, char *argv[])
{
    string cRate = "800kbps";
    string bRate = "80kbps";
    string bDelay = "2ms";
    double tStop = 5.0;
    string qType = "RED";
    uint16_t port = 5001;

    CommandLine cmd;
    cmd.AddValue("cRate", "Data rate of client", cRate);
    cmd.AddValue("bRate", "Data rate of bottleneck link", bRate);
    cmd.AddValue("tStop", "Stop time", tStop);
    cmd.AddValue("queue", "Queue Type", qType);
    cmd.Parse (argc, argv);
    LogComponentEnable ("p2p_test", LOG_LEVEL_INFO); /// test here

    // p2p topology
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(bRate));
    p2p.SetChannelAttribute("Delay", StringValue(bDelay));
    p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1p"));

    PointToPointHelper leaf;
    leaf.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    leaf.SetChannelAttribute("Delay", StringValue("1ms"));

    PointToPointDumbbellHelper d (2, leaf, 2, leaf, p2p);

    // stack
    InternetStackHelper stack;
    d.InstallStack (stack);

    // queue disc: different from old version
    TrafficControlHelper tch;
    // tch.SetRootQueueDisc("ns3::RedQueueDisc",
    //                     "MinTh", DoubleValue(5.0),
    //                     "MaxTh", DoubleValue(10.0),
    //                     "LinkBandwidth", StringValue(bRate),
    //                     "LinkDelay", StringValue(bDelay));
    tch.SetRootQueueDisc("ns3::RedQueueDisc");
    tch.Install(d.GetLeft()->GetDevice(0));
    QueueDiscContainer qDiscs = tch.Install(d.GetRight()->GetDevice(0));    // QueueDiscContainer qdisc;
    cout << "MAC: " << d.GetRight()->GetDevice(0)->GetAddress() << endl;

    Simulator::Schedule(Seconds(10), &handler, qDiscs);

    // Assign IP Addresses
    d.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                        Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"),
                        Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));

    // sink App
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper psh("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps;
    for(uint32_t i = 0; i < d.LeftCount(); i ++)
        sinkApps.Add(psh.Install(d.GetLeft(i)));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(tStop));

    // client App
    for(uint32_t i = 0; i < d.RightCount(); i ++)
    {
        Ptr<Socket> skt = Socket::CreateSocket(d.GetRight(i), TcpSocketFactory::GetTypeId());
        Address addr(InetSocketAddress(d.GetLeftIpv4Address(i), port));
        Ptr<MyApp> app = CreateObject<MyApp> ();
        app->SetTagValue(i + 1);
        app->Setup(skt, addr, 1000, DataRate(cRate));
        d.GetRight(i)->AddApplication(app);     // not a helper -> no install()
        app->SetStartTime(Seconds(0.1));
        app->SetStopTime(Seconds(tStop));
    }

    // set the p2p device
    p2pDev = DynamicCast<PointToPointNetDevice> (d.GetRight()->GetDevice(0));
    

    // processing and tracing here
    for (uint32_t i = 0; i < d.RightCount() + 1; i ++)
    {
        d.GetRight()->GetDevice(i)->TraceConnectWithoutContext("MacRx", MakeCallback(&PktMacRx));
        // d.GetRight()->GetDevice(i)->TraceConnectWithoutContext("MacTxDrop", MakeCallback(&PktDrop));
        d.GetRight()->GetDevice(i)->TraceConnectWithoutContext("PhyRxDrop", MakeCallback(&PktDrop));
    }

    // d.GetRight()->GetDevice(0)->TraceConnectWithoutContext("MacTx", MakeCallback(&PktMacTx));

    // tracing queue by visiting it from dumbbell d
    Ptr<Queue<Packet>> qp = DynamicCast<PointToPointNetDevice>(d.GetRight()->GetDevice(0))->GetQueue();
    qp->TraceConnectWithoutContext("Drop", MakeCallback(&PktDrop));     // 
    // qp->TraceConnectWithoutContext("EnQueue", MakeCallback(&PktDrop));
    // qp->TraceConnectWithoutContext("DeQueue", MakeCallback(&PktDrop));

    // tracing from traffic-control.cc
    // qp->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&DevPktInQ));
    Ptr<QueueDisc> q = qDiscs.Get(0);
    q->TraceConnectWithoutContext("Drop", MakeCallback(&QueueDrop));
    q->TraceConnectWithoutContext("EnQueue", MakeCallback(&QueueOp));   // not fired
    // q->TraceConnectWithoutContext("DeQueue", MakeCallback(&QueueOp));   // not fired
    // q->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&TcPktInQ));

    // set simulation
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    cout << "Running the simulation..." << endl;
    Simulator::Stop(Seconds(tStop));
    Simulator::Run();

    // stats
    QueueDisc::Stats st = qDiscs.Get(0)->GetStats();
    st.Print(cout);
    if (st.GetNDroppedPackets (RedQueueDisc::UNFORCED_DROP) == 0)
        cout << "There should be some unforced drops" << endl;
    if (st.GetNDroppedPackets (QueueDisc::INTERNAL_QUEUE_DROP) != 0)
        cout << "There should be zero drops due to queue full" << endl;

    // end
    cout << "\nDestroying the simulation..." << endl;
    Simulator::Destroy();
    return 0;

}