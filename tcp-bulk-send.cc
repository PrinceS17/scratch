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

// Network topology
//
//       n0 ----------- n1
//            500 Kbps
//             5 ms
//
// - Flow from n0 to n1 using BulkSendApplication.
// - Tracing of queues and packet receptions to file "tcp-bulk-send.tr"
//   and pcap tracing available when tracing is turned on.

#include <string>
#include <fstream>
#include <sstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("TcpBulkSendExample");

// test if pkt arrival will also count double in bulk send example

int rxCnt = 0;
double pktTime = 0;

string
logPktIpv4Address (Ptr<const Packet> p)
{
  Ptr<Packet> pktCopy = p->Copy ();
  PppHeader pppH;
  Ipv4Header ipH;
  pktCopy->RemoveHeader (pppH);
  pktCopy->PeekHeader (ipH);
  stringstream ss;
  ipH.GetSource ().Print (ss);
  ss << " > ";
  ipH.GetDestination ().Print (ss);
  return ss.str ();
}

static void
PktArrival (Ptr<const Packet> p)
{
  Ptr<Packet> pktCopy = p->Copy ();
  rxCnt++;
  stringstream ss;
  double t0 = Simulator::Now ().GetSeconds ();
  ss << t0 << " : inter pkt: " << t0 - pktTime << " s; cnt: " << rxCnt;
  if(rxCnt % 20 == 0) 
  {
    NS_LOG_INFO(ss.str() + " . " + logPktIpv4Address(p));
  }
  pktTime = Simulator::Now().GetSeconds();
}



int
main (int argc, char *argv[])
{

  bool tracing = true;
  uint32_t maxBytes = 0;

//
// Allow the user to override any of the defaults at
// run-time, via command-line arguments
//
  CommandLine cmd;
  cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue ("maxBytes",
                "Total number of bytes for application to send", maxBytes);
  cmd.Parse (argc, argv);

  LogComponentEnable ("TcpBulkSendExample", LOG_LEVEL_INFO); /// test here
//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (2);

  NS_LOG_INFO ("Create channels.");

//
// Explicitly create the point-to-point link required by the topology (shown above).
//
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("500Kbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("5ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

//
// Install the internet stack on the nodes
//
  InternetStackHelper internet;
  internet.Install (nodes);

//
// We've got the "hardware" in place.  Now we need to add IP addresses.
//
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  NS_LOG_INFO ("Create Applications.");

//
// Create a BulkSendApplication and install it on node 0
//
  uint16_t port = 9;  // well-known echo port number


  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (i.GetAddress (1), port));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  source.SetAttribute("SendSize", UintegerValue(1000));
  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (0.0));
  sourceApps.Stop (Seconds (10.0));

//
// Create a PacketSinkApplication and install it on node 1
//
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

//
// Set up tracing if enabled
//
  if (tracing)
    {
      AsciiTraceHelper ascii;
      pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("tcp-bulk-send.tr"));
      pointToPoint.EnablePcapAll ("tcp-bulk-send", false);
    }



  // trace of packet sink helper????
  // Config::ConnectWithoutContext("/NodeList/1/*/Mac/MacTx", MakeCallback (&PktArrival));

  // NodeList::GetNode(1)->TraceConnectWithoutContext("MacRx", MakeCallback (&PktArrival));
  Config::ConnectWithoutContext("/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/MacRx", MakeCallback(&PktArrival));
//
// Now, do the actual simulation.
//
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");


  Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApps.Get (0));
  std::cout << "Total Bytes Received: " << sink1->GetTotalRx () << " = " << sink1->GetTotalRx() * 8.0 / 1000000 << " Mbit" << endl;
  std::cout << "Through packet: " << rxCnt * 8000.0 / 1000000 << " Mbit" << endl;
}
