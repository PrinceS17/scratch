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
#include "ns3/udp-socket-factory.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/tag.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include <cstring>
#include <fstream>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("p2p_test");

uint32_t cnt[2] = {0};
uint32_t cnt1[2] = {0};
uint32_t cnt2[2] = {0};
uint32_t cnt3[2] = {0};
uint32_t cntt[2] = {0};

fstream fout;
Ptr<PointToPointNetDevice> p2pDev;


TypeId 
MyTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MyTag")
    .SetParent<Tag> ()
    .AddConstructor<MyTag> ()
    .AddAttribute ("SimpleValue",
                   "A simple value",
                   EmptyAttributeValue (),
                   MakeUintegerAccessor (&MyTag::GetSimpleValue),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}
TypeId 
MyTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
uint32_t 
MyTag::GetSerializedSize (void) const
{
  return 4;
}
void 
MyTag::Serialize (TagBuffer i) const
{
  i.WriteU32 (m_simpleValue);
}
void 
MyTag::Deserialize (TagBuffer i)
{
  m_simpleValue = i.ReadU32 ();
}
void 
MyTag::Print (std::ostream &os) const
{
  os << "v=" << (uint32_t)m_simpleValue;
}
void 
MyTag::SetSimpleValue (uint32_t value)
{
  m_simpleValue = value;
}
uint32_t 
MyTag::GetSimpleValue (void) const
{
  return m_simpleValue;
}

//=========================================================================//
//===============Beigining of Application definition=======================//
//=========================================================================//

MyApp::MyApp ()
  : m_socket (0), 
    m_peer (), 
    m_packetSize (0), 
    //m_nPackets (0), 
    m_dataRate (0), 
    m_sendEvent (), 
    m_running (false), 
    //m_packetsSent (0)
    m_tagValue (0),
    m_cnt (0)
{
    isTrackPkt = false;
}

MyApp::~MyApp()
{
  m_socket = 0;
}

void
//MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  //m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::SetDataRate(DataRate rate)
{
  m_dataRate = rate;
}

void
MyApp::SetTagValue(uint32_t value)
{
  m_tagValue = value;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  //m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void 
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void 
MyApp::SendPacket (void)
{
  //create the tags
  MyTag tag;
  if(!isTrackPkt)  tag.SetSimpleValue (m_tagValue);
  else   tag.SetSimpleValue (m_tagValue * tagScale + ++m_cnt);
  stringstream ss;
  ss << "- Tx: " << m_tagValue - 1 << ". " << m_cnt;
//   NS_LOG_INFO(ss.str());

  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  packet -> AddPacketTag (tag); //add tags
  m_socket->Send (packet);

  // for debug only
//   NS_LOG_INFO(" socket for " + to_string(m_tagValue - 1) + ": sending pkt, cnt = " + to_string(m_cnt));
  ScheduleTx ();
}

void 
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

Ptr<Socket> MyApp::GetSocket()
{
    return m_socket;
}
//=========================================================================//
//===================End of Application definition=========================//
//=========================================================================//



void handler(QueueDiscContainer qDisc)
{
    double curt = Simulator::Now().GetSeconds();
    cout << curt << "s: # pkt in Q: " << qDisc.Get(0)->GetCurrentSize() << endl;
    Simulator::Schedule(Seconds(5.0), &handler, qDisc);
}

void
onCwndChange(string context, uint32_t oldValue, uint32_t newValue)
{
    size_t pos = context.find("/Congestion");
    uint32_t i = context.substr(pos - 1, 1)[0] - '0';       // get the flow index, need testing
    // uint32_t i = context.c_str()[0] - '0';

    NS_LOG_INFO("  - CongWnd = " + to_string(newValue) + "\n");

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
    fout << Simulator::Now().GetSeconds() << " " << vNew << endl;
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
void PktMacTxDrop(Ptr<const Packet> p)
{
    Ptr<Packet> pcp = p->Copy();
    MyTag tag;
    if(pcp->PeekPacketTag(tag))
    {
        int idx = tag.GetSimpleValue();
        stringstream ss;
        ss << Simulator::Now().GetSeconds() << "s: Mac Tx Drop: " << idx << ". " << cntt[idx - 1] ++ << endl;
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
        ss << Simulator::Now().GetSeconds() << "s: Mac Rx: " << idx << ". " << cnt3[idx - 1] ++;
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
    string cRate = "20Mbps";
    string bRate = "5Mbps";
    string bDelay = "2ms";
    double tStop = 50.0;
    string qType = "RED";
    uint16_t port = 8080;
    string fname = "REDout.dat";

    remove(fname.c_str());
    fout.open(fname, ios::app | ios::out);

    CommandLine cmd;
    cmd.AddValue("cRate", "Data rate of client", cRate);
    cmd.AddValue("bRate", "Data rate of bottleneck link", bRate);
    cmd.AddValue("tStop", "Stop time", tStop);
    cmd.AddValue("queue", "Queue Type", qType);
    cmd.Parse (argc, argv);
    LogComponentEnable ("p2p_test", LOG_LEVEL_INFO); /// test here

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));     
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1000));  

    // p2p topology
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(bRate));
    p2p.SetChannelAttribute("Delay", StringValue(bDelay));
    // p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1p"));

    PointToPointHelper leaf;
    leaf.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    leaf.SetChannelAttribute("Delay", StringValue("1ms"));

    PointToPointDumbbellHelper d (2, leaf, 2, leaf, p2p);

    // stack
    InternetStackHelper stack;
    d.InstallStack (stack);

    // queue disc: different from old version
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc");
    tch.Install(d.GetLeft()->GetDevice(0));
    QueueDiscContainer qDiscs = tch.Install(d.GetRight()->GetDevice(0));    // QueueDiscContainer qdisc;
    cout << "MAC: " << d.GetRight()->GetDevice(0)->GetAddress() << endl;

    Simulator::Schedule(Seconds(10), &handler, qDiscs);

    // Assign IP Addresses
    d.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                        Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"),
                        Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));

    // IP address test
    stringstream ss1;
    ss1 << "TX: " << endl;
    for(uint32_t i = 0; i < d.RightCount(); i ++)
    {
        ss1 << "    ";
        d.GetRightIpv4Address(i).Print(ss1);
        ss1 << endl;
    }
    ss1 << "RX: " << endl;
    for(uint32_t i = 0; i < d.LeftCount(); i ++)
    {
        ss1 << "    ";
        d.GetLeftIpv4Address(i).Print(ss1);
        ss1 << endl;
    }
    NS_LOG_INFO(ss1.str());

    // sink App
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    // PacketSinkHelper psh("ns3::TcpSocketFactory", sinkLocalAddress);
    PacketSinkHelper psh("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps;
    for(uint32_t i = 0; i < d.LeftCount(); i ++)
        sinkApps.Add(psh.Install(d.GetLeft(i)));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(tStop));

    // client App
    for(uint32_t i = 0; i < d.RightCount(); i ++)
    {
        // Ptr<Socket> skt = Socket::CreateSocket(d.GetRight(i), TcpSocketFactory::GetTypeId());
        Ptr<Socket> skt = Socket::CreateSocket(d.GetRight(i), UdpSocketFactory::GetTypeId());
        Address addr(InetSocketAddress(d.GetLeftIpv4Address(i), port));
        Ptr<MyApp> app = CreateObject<MyApp> ();
        app->SetTagValue(i + 1);
        app->Setup(skt, addr, 1000, DataRate(cRate));
        d.GetRight(i)->AddApplication(app);     // not a helper -> no install()
        app->SetStartTime(Seconds(0.1));
        app->SetStopTime(Seconds(tStop));

        // tracing cwnd chagne: need test
        // string context1 = "/NodeList/0/$ns3::TcpL4Protocol/SocketList/" + to_string(i) + "/CongestionWindow";
        // app->GetSocket()->TraceConnect("CongestionWindow", context1, MakeCallback(&onCwndChange));
        // skt->TraceConnectWithoutContext("Congestionwindow",  MakeCallback(&onCwndChange));
    }

    // set the p2p device
    p2pDev = DynamicCast<PointToPointNetDevice> (d.GetRight()->GetDevice(0));
    

    // processing and tracing here
    for (uint32_t i = 0; i < d.RightCount() + 1; i ++)
    {
        d.GetRight()->GetDevice(i)->TraceConnectWithoutContext("MacRx", MakeCallback(&PktMacRx));
        d.GetRight()->GetDevice(i)->TraceConnectWithoutContext("PhyRxDrop", MakeCallback(&PktDrop));
    }

    d.GetRight()->GetDevice(0)->TraceConnectWithoutContext("MacTx", MakeCallback(&PktMacTx));
    d.GetRight()->GetDevice(0)->TraceConnectWithoutContext("MacTxDrop", MakeCallback(&PktMacTx));
    

    // tracing queue by visiting it from dumbbell d
    Ptr<Queue<Packet>> qp = DynamicCast<PointToPointNetDevice>(d.GetRight()->GetDevice(0))->GetQueue();
    qp->TraceConnectWithoutContext("Drop", MakeCallback(&PktDrop));

    // tracing from traffic-control.cc
    // qp->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&DevPktInQ));
    Ptr<QueueDisc> q = qDiscs.Get(0);
    q->TraceConnectWithoutContext("Drop", MakeCallback(&QueueDrop));
    q->TraceConnectWithoutContext("EnQueue", MakeCallback(&QueueOp));   // not fired
    // q->TraceConnectWithoutContext("DeQueue", MakeCallback(&QueueOp));   // not fired
    q->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&TcPktInQ));

    // set simulation
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    cout << "Running the simulation..." << endl;
    Simulator::Stop(Seconds(tStop));
    Simulator::Run();

    // stats
    QueueDisc::Stats st = qDiscs.Get(0)->GetStats();
    // st.Print(cout);
    if (st.GetNDroppedPackets (RedQueueDisc::UNFORCED_DROP) == 0)
        cout << "There should be some unforced drops" << endl;
    if (st.GetNDroppedPackets (QueueDisc::INTERNAL_QUEUE_DROP) != 0)
        cout << "There should be zero drops due to queue full" << endl;

    // end
    fout.close();
    cout << "\nDestroying the simulation..." << endl;
    Simulator::Destroy();
    return 0;

}