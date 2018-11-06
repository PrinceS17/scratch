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

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("TestTcpModel");


class MyApp : public Application 
{
public:

  MyApp ();
  virtual ~MyApp();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (0), 
    m_peer (), 
    m_packetSize (0), 
    m_nPackets (0), 
    m_dataRate (0), 
    m_sendEvent (), 
    m_running (false), 
    m_packetsSent (0)
{
}

MyApp::~MyApp()
{
  m_socket = 0;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
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
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
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

static void
CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
}

static void
RxDrop (Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
}


// try to create a 4-node network, 1, 3 send to 2, 4

int main(int argc, char* argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);
  Time::SetResolution(Time::NS);
  NS_LOG_INFO("Hello! Begin creating p2p link for tcp model!");
  
  NodeContainer nc1;
  nc1.Create(2);
  
  PointToPointHelper p2p1;
  p2p1.SetChannelAttribute("Delay", StringValue("1ms"));
  p2p1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  
  NetDeviceContainer dev1 = p2p1.Install(nc1);

  InternetStackHelper stack;
  stack.Install(nc1);

  Ipv4AddressHelper addr;
  addr.SetBase("20.1.1.0", "255.255.255.252");    // addr begins at 20.1.1.0; after mask, only 4 hosts in a sub network
  Ipv4InterfaceContainer interfaces = addr.Assign(dev1);  // contain ip addresses
  
  // what's the typical usage of tcp TX & RX? make a summary!
  uint16_t sinkPort = 8080;       
  Address sinkAddr(InetSocketAddress(interfaces.GetAddress (1), sinkPort)); // specify socket addr by ip & port
  // 1st: packet sink helper
  PacketSinkHelper sinkHelp("ns3::TcpSocketFactory", 
    InetSocketAddress(Ipv4Address::GetAny(), 8080));       // create a socket by using string in helper
  cout << Ipv4Address::GetAny() << endl;
  ApplicationContainer sinkApp = sinkHelp.Install(nc1.Get(1));  // install the application
  sinkApp.Start(Seconds(0.));
  sinkApp.Stop(Seconds(10.));

  // 2nd: create socket sender directly, in a lower level
  // must? don't have source helper...
  Ptr<Socket> socket1 = Socket::CreateSocket(nc1.Get(0), TcpSocketFactory::GetTypeId());
          // create a socket of <type_id> kind for some node
  socket1->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));

  
  Ptr<MyApp> app = CreateObject<MyApp>();
  app->Setup(socket1, sinkAddr, 2000, 1000, DataRate("5Mbps"));
  nc1.Get(0)->AddApplication(app);
  app->SetStartTime(Seconds(2.));
  app->SetStopTime(Seconds(7.));

  dev1.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));

  Simulator::Stop(Seconds(20));
  Simulator::Run();
  Simulator::Destroy();


}