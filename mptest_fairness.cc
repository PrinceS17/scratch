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

/*
 * NaturalShare policy, test Mbox with 2 senders & 1 receiver
 * MP policy realized in pktArrival of right router
 */

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

//packet tag head files
#include "ns3/tag.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"

//head files
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

#include "ns3/tools.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <fstream>
#include <cstdio>
#include <ctime>
#include <locale>
#include <time.h>
#include <stdlib.h>

using namespace std;
using namespace ns3;

/*
 * Gloable Variable
 */

#define ARRAY_SIZE 1000

std::string attackerDataRate = "4Mbps";        // currently no use, but keep for compatibility
std::string clientDataRate = "1Mbps";
double period = 0.5; // will it influence the cwnd / rwnd value?
double duration = period / 1;
double moment = 0;    
uint16_t port = 5001;
uint16_t attackerport = 5003;
uint16_t mmtu = 1599;
std::ofstream queueFile ("queueLength.dat", std::ios::out);
//std::ofstream congestionUsageFile ("congestionUsage.dat", std::ios::out);
//std::ofstream normalUsageFile ("normalUsage.dat", std::ios::out);
std::ofstream droprateFile ("dropRate.dat", std::ios::out);
//std::ofstream rawdropFile ("rawDropRate.dat", std::ios::out);
Ptr<Node> router;
Ptr<PointToPointNetDevice> p2pDevice;
// uint32_t nLeaf = 4;
// uint32_t nAttacker = 4;
// int size = nLeaf + nAttacker;
uint32_t nLeft = 4; // # receiver
uint32_t nRight = 4; // # senders
uint32_t nAttacker = 2;

double detectPeriod = 0.0;
uint32_t lock = 0;
uint32_t dropArray[ARRAY_SIZE];
//uint32_t dropTag[5000];
uint32_t congWin[ARRAY_SIZE];
//uint32_t verifyWin[5000]; // representing the verified capabilities
//uint32_t tagWin[5000]; // representing the tagged packets
uint32_t receiveWin[ARRAY_SIZE]; // representing the received packets (usage)   /// rwnd: RX's largest # packets it can receive without ACK, actual # packets = min (rwnd, cwnd)
uint16_t enableEarlyDrop = 1;
uint32_t bootStrap = 0;

// Used for measuring real time loss rate
uint32_t realtimePeriod = 0;
uint32_t realtimePacketFeedback = 50;
double realtimeLossRate = 0;
uint32_t realtimeDrop = 0;
double lossRateArray[ARRAY_SIZE];

// Used for crossing traffic
uint32_t nCrossing = 0;

// parameters
double lossRateThreshold = 0.05;
double beta = 0.8;

// for fairshare
double total_capacity = 0;

// for RTT record
// vector< vector<double> > rtt;
vector<string> fileName;
string qFileName;
double tPkt[ARRAY_SIZE];
int txCnt[ARRAY_SIZE];
int rxCnt[ARRAY_SIZE];
double recvData[ARRAY_SIZE];  // record tcp data for rate calculation
double recvBit[ARRAY_SIZE];   // record p2p packet size 
bool isPrintHeader;
bool isPrintTx;
bool isPrintLeft;
bool isPrintRx;
bool isChangeFair;
bool firstRtt = true;
bool isTcp;
string fairness;

// log component definition
NS_LOG_COMPONENT_DEFINE ("TestForMiddlePolice");

// for recording the congestion window
std::ofstream windowFile ("natural_flat_window.data", std::ios::out);
std::ofstream lossRateFile ("natural_flat_LR.data", std::ios::out);

// tool function

// vector<int> getPktSizes(Ptr <const Packet> p)     // get [p2p size, ip size, tcp size, data size]
// {
//   Ptr<Packet> pktCopy = p->Copy();
//   vector<int> res;
//   PppHeader pppH;
//   Ipv4Header ipH;
//   TcpHeader tcpH;

//   res.push_back((int)pktCopy->GetSize());
//   pktCopy->RemoveHeader(pppH);
//   res.push_back((int)pktCopy->GetSize());
//   pktCopy->RemoveHeader(ipH);
//   res.push_back((int)pktCopy->GetSize());
//   pktCopy->RemoveHeader(tcpH);
//   res.push_back((int)pktCopy->GetSize());
//   return res;
// }

// uint32_t getTcpSize(Ptr <const Packet> p)
// {
//   Ptr<Packet> pktCopy = p->Copy();
//   PppHeader pppH;
//   Ipv4Header ipH;
//   TcpHeader tcpH;
//   pktCopy->RemoveHeader(pppH);
//   pktCopy->RemoveHeader(ipH);
//   pktCopy->RemoveHeader(tcpH);
//   return pktCopy->GetSize();
// }

// string 
// printPkt(Ptr <const Packet> p)
// {
//   Ptr<Packet> pktCopy = p->Copy();
//   stringstream ss;
//   pktCopy->Print(ss);
//   return ss.str();
// }

// string
// logIpv4Header (Ptr<const Packet> p)
// {
//   Ptr<Packet> pktCopy = p->Copy ();
//   PppHeader pppH;
//   Ipv4Header ipH;
//   pktCopy->RemoveHeader (pppH);
//   pktCopy->PeekHeader (ipH); /// need to know the exact structure of header
//   stringstream ss;
//   ipH.Print (ss);
//   return ss.str ();
// }

// string
// logPppHeader (Ptr<const Packet> p)
// {
//   Ptr<Packet> pktCopy = p->Copy ();
//   PppHeader pppH;
//   pktCopy->PeekHeader (pppH);
//   stringstream ss;
//   pppH.Print (ss);
//   return ss.str ();
// }

// string
// logTcpHeader (Ptr<const Packet> p)
// {
//   Ptr<Packet> pktCopy = p->Copy ();
//   PppHeader pppH;
//   Ipv4Header ipH;
//   TcpHeader tcpH;
//   pktCopy->RemoveHeader (pppH);
//   pktCopy->RemoveHeader (ipH);
//   pktCopy->PeekHeader (tcpH);
//   stringstream ss;
//   tcpH.Print (ss);
//   return ss.str ();
// }

// string
// logPktIpv4Address (Ptr<const Packet> p)
// {
//   Ptr<Packet> pktCopy = p->Copy ();
//   PppHeader pppH;
//   Ipv4Header ipH;
//   pktCopy->RemoveHeader (pppH);
//   pktCopy->PeekHeader (ipH);
//   stringstream ss;
//   ipH.GetSource ().Print (ss);
//   ss << " > ";
//   ipH.GetDestination ().Print (ss);
//   return ss.str ();
// }

//=========================================================================//
//=========================Begin of TAG definition=========================//
//=========================================================================//
class MyTag : public Tag
{
public:
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (TagBuffer i) const;
  virtual void Deserialize (TagBuffer i);
  virtual void Print (std::ostream &os) const;

  // these are our accessors to our tag structure
  void SetSimpleValue (uint32_t value);
  uint32_t GetSimpleValue (void) const;

private:
  uint32_t m_simpleValue;
};

TypeId
MyTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MyTag")
                          .SetParent<Tag> ()
                          .AddConstructor<MyTag> ()
                          .AddAttribute ("SimpleValue", "A simple value", EmptyAttributeValue (),
                                         MakeUintegerAccessor (&MyTag::GetSimpleValue),
                                         MakeUintegerChecker<uint32_t> ());
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
  os << "v=" << (uint32_t) m_simpleValue;
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

class MyApp : public Application
{
public:
  MyApp ();
  virtual ~MyApp ();

  //void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);
  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate);
  void SetTagValue (uint32_t value);
  void SetDataRate (DataRate rate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  //uint32_t        m_nPackets;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_running;
  //uint32_t        m_packetsSent;
  uint32_t m_tagValue;
};

MyApp::MyApp ()
    : m_socket (0),
      m_peer (),
      m_packetSize (0),
      //m_nPackets (0),
      m_dataRate (0),
      m_sendEvent (),
      m_running (false),
      //m_packetsSent (0)
      m_tagValue (0)
{
}

MyApp::~MyApp ()
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
MyApp::SetDataRate (DataRate rate)
{
  m_dataRate = rate;
}

void
MyApp::SetTagValue (uint32_t value)
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
  MyTag tag;
  tag.SetSimpleValue (m_tagValue);

  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  packet->AddPacketTag (tag); //add tags
  m_socket->Send (packet);
  ScheduleTx ();

  txCnt[m_tagValue - 1] ++;
  // if(txCnt[m_tagValue - 1] % 500 == 0)
    stringstream ss;
    ss << "TX: " << Simulator::Now ().GetSeconds () << " s: " << m_tagValue // << ". " << m_cnt - 1
      << " ; total: " << txCnt[m_tagValue - 1];
    if (isPrintTx)
      NS_LOG_INFO (ss.str ());
      //}
}

void
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      // mark the count
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

void attackFlow(PointToPointDumbbellHelper d, uint32_t nAttacker)
{
  for (uint32_t i = d.RightCount() - nAttacker; i < d.RightCount(); ++i)
  {
    Ptr<Socket> ns3Socket = isTcp? Socket::CreateSocket(d.GetRight(i), TcpSocketFactory::GetTypeId())
                            :Socket::CreateSocket (d.GetRight(i), UdpSocketFactory::GetTypeId ());

    Address sinkAddress(InetSocketAddress(d.GetLeftIpv4Address(i), attackerport));
    Ptr<MyApp> app = CreateObject<MyApp>();
    uint32_t tagValue = i + 1; //take the least significant 8 bits
    app->SetTagValue(tagValue);
    app->Setup(ns3Socket, sinkAddress, 1000, DataRate(attackerDataRate));
    d.GetRight(i)->AddApplication(app);
    app->SetStartTime(Seconds(0.0));
    // app->SetStopTime(Seconds(duration));

  }
}

void
clearArray ()
{
  for (uint32_t j = 0; j < nRight; ++j)
    {
      //std::cout << "client no: " << j << "; arrived: "<< receiveWin[j] << "; drop rate: " << 1.0 * dropArray[j] / receiveWin[j] << std::endl;
      receiveWin[j] = 0;
      dropArray[j] = 0;
      recvData[j] = 0.0;
      recvBit[j] = 0.0;
      rxCnt[j] = 0;
    }
  total_capacity = 0;
}

// the sloping probability for best effort packets
bool
slopingProb (double lossRate)
{
  if (lossRate > lossRateThreshold)
    {
      return true;
    }
  else
    {
      double dropP = 20.0 * lossRate;
      double randP = (double) rand () / RAND_MAX;
      if (dropP <= randP)
        {
          return true;
        }
    }

  return false;
}

int rxTagCnt = 0;

static void
PktArrival (Ptr<const Packet> p)
{

  Ptr<Packet> pktCopy = p->Copy ();
  MyTag tag;
  // printHeader(p);
  vector<int> tmp = getPktSizes(p, isTcp? TCP:UDP);
  // vector<int> tmp{500,500,500,500};
  // printPkt(pktCopy);

  // functionality: switch the fairness
  if(isChangeFair) 
  if(floor(Simulator::Now().GetSeconds()) == 50 || floor(Simulator::Now().GetSeconds() == 100) )
    {
      // fairness = fairness == "persender"? "natural":"persender";
      if(fairness == "natural") fairness = "persender";
      else fairness = "natural";
      cout << "Fairness change!" << endl;
      isChangeFair = false;
    }

  if (pktCopy->PeekPacketTag (tag)) // if find a tag
    {
      /// logging header/address

      // compatible with the setting
      uint32_t index = tag.GetSimpleValue () - 1;

      rxCnt[index] ++;
      receiveWin[index] ++;
      double ttemp = Simulator::Now ().GetSeconds ();
      stringstream ss;
      // if(!index)
      ss << "- RX: " << Simulator::Now ().GetSeconds () << " : " << index + 1  << ". " << rxCnt[index]
         << " : rt pkt time = " << ttemp - tPkt[index] << " s "; // tcp size: " << getTcpSize(p);
      if (isPrintRx)
        NS_LOG_INFO (ss.str () + "; " + logPktIpv4Address (p));
      if (isPrintHeader)
        // NS_LOG_INFO ("- TCP Header: " + logTcpHeader (p));
        NS_LOG_INFO (printPkt(p));

      
      tPkt[index] = ttemp;
      
      if (++realtimePeriod ==
          realtimePacketFeedback) // calculating a real-time loss rate every 50 packets
        {
          realtimeLossRate = (double) realtimeDrop / (double)realtimePeriod;
          realtimePeriod = 0;
          realtimeDrop = 0;
        }

      bool tmpDrop = false;
      if (enableEarlyDrop > 0)
        {
          if (receiveWin[index] > congWin[index])
            {
              // cout << index << " : rwnd = " << receiveWin[index] << " ; cwnd = " << congWin[index] <<
              //     "; rtLR = " << realtimeLossRate << "; LLR = " << lossRateArray[index] << endl;
              if (realtimeLossRate > lossRateThreshold || lossRateArray[index] > lossRateThreshold)
              // why this condition: if not, no good put, bw -> 0

                {
                  tmpDrop = true;
                  // realtimeDrop--;   // shouldn't -- after I cancel the MacTxDrop
                }

            }
        }
        if(tmpDrop) p2pDevice->SetEarlyDrop (true);
        else
        { 
          recvData[index] += (double) tmp[3] / 1000.0;   // size in KB
          recvBit[index] += (double) tmp[0] / 1000.0;    // raw size in KB
        }
    }

  // filename[n] // should defined globally
  ofstream fout[nRight];
  for (int i = 0; i < nRight; i++)
    fout[i].open (fileName[i], ios::out | ios::app);

  if (Simulator::Now ().GetSeconds () > detectPeriod)
    {

      cout << endl
           << Simulator::Now ().GetSeconds () << "s detection period: " << bootStrap++ << endl;
      if (windowFile.is_open ())
        windowFile << Simulator::Now ().GetSeconds () << " ";
      if (lossRateFile.is_open ())
        lossRateFile << Simulator::Now ().GetSeconds () << " ";

      for (uint32_t j = 0; j < nRight; j++)
        {
          // loss rate & cwnd update
          double lossRate = receiveWin[j] > 0 ? (double) dropArray[j] / receiveWin[j] : 0.0;
          lossRateArray[j] = receiveWin[j] > 5 ? (1 - beta) * lossRate + beta * lossRateArray[j]
                                               : beta * lossRateArray[j]; // similar to a time average


          total_capacity += receiveWin[j] >= dropArray[j]? receiveWin[j] - dropArray[j] : 0;
        }
        cout << "total capacity: " << total_capacity << endl;

        // rate allocation
        for(uint32_t j = 0; j < nRight; ++ j)
        {
            if(fairness == "natural") 
                congWin[j] = receiveWin[j] >= dropArray[j]? receiveWin[j] - dropArray[j] : 0;
            else if(fairness == "persender")
                congWin[j] = total_capacity / nRight;
            else ;


          // output to file & stdout
          if (windowFile.is_open ())
            windowFile << congWin[j] << " ";
          if (lossRateFile.is_open ())
            lossRateFile << lossRateArray[j] << " ";
          cout << "Client No." << j << "; cwnd: " << congWin[j]
               << "; loss rate: " << lossRateArray[j] << "; rwnd: " << receiveWin[j]
               << "; drop window: " << dropArray[j] << "; realtime loss: " << realtimeLossRate
               << endl;
          // fout[j] << Simulator::Now().GetSeconds() << " " << congWin[j] << endl;
          cout << Simulator::Now ().GetSeconds () << ": raw: " << recvBit[j] * 8.0 / period << " kbps"
               << "; data: " << recvData[j] * 8.0 / period << " kbps" << endl; 
          // cout << "  = pkt sizes: " << tmp[0] << " " << tmp[1] << " " << tmp[2] << " " << tmp[3] << endl;
          fout[j] << Simulator::Now ().GetSeconds () << " " << recvData[j] * 8.0 / period << " kbps"
                  << endl; // output the ephermal data rate (omit pkt size 1kB)
        }

      if (windowFile.is_open ())
        windowFile << endl;
      if (lossRateFile.is_open ())
        lossRateFile << endl;
      detectPeriod += period; // update for next period
      clearArray ();
    }

  // close the file
  for (int i = 0; i < nRight; i++)
    fout[i].close ();
}

int pktLeft = 0;
int pktTagLeft = 0;

static void
PktArrivalLeft (Ptr<const Packet> p)
{
  pktLeft++;
  Ptr<Packet> pktCopy = p->Copy ();
  MyTag tag;

  if (pktCopy->PeekPacketTag (tag)) // if find a tag
    {
      // compatible with the setting
      uint32_t index = tag.GetSimpleValue () - 1;

      stringstream ss;
      ss << "   Left router: " << Simulator::Now ().GetSeconds () << ": " << index + 1
         << ". " // << cnt
         << " ; total(tag): " << ++pktTagLeft << " ; total: " << pktLeft;
      if (isPrintLeft)
        NS_LOG_INFO (ss.str () + "; " + logPktIpv4Address (p));
      if (isPrintHeader)
        // NS_LOG_INFO ("- TCP Header: " + logTcpHeader (p));
        // NS_LOG_INFO ("- IP header: " + logIpv4Header(p));
        NS_LOG_INFO (printPkt(p));
    }
}

static void
PktDropLeft (Ptr<const Packet> p)
{
  stringstream ss;
  ss << "    - Left: drop packet!";
  NS_LOG_INFO (ss.str ());
}

static void
PktDrop (Ptr<const Packet> p)
{
  realtimeDrop += 1;
  MyTag tag;
  if (p->PeekPacketTag (tag))
    dropArray[tag.GetSimpleValue () - 1] += 1;
  // if(dropArray[tag.GetSimpleValue() - 1] % 20 == 0)
    // cout << Simulator::Now().GetSeconds() << " Drop packet!" << endl;
}

static void
PktDropOverflowLeft (Ptr<const Packet> p)
{
  stringstream ss;
  ss << "    - Left: drop for overflow!" << endl;
  NS_LOG_INFO (ss.str ());
}

static void
PktDropOverflow (Ptr<const Packet> p)
{
  cout << "Dropping for overflow" << endl;
}

uint32_t cnt2[ARRAY_SIZE] = {0};

static void
QueueChange (uint32_t vOld, uint32_t vNew)
{
  stringstream ss;
  ss << "  " << Simulator::Now().GetSeconds() << "s: Queue #packet changed to " << vNew;
  // NS_LOG_INFO (ss.str());

  fstream fout;
  fout.open(qFileName, ios::out | ios::app);
  fout << Simulator::Now().GetSeconds() << " " << vNew << endl;     // keep trace for draw the queue size
  fout.close();
}

static void
PktEnqueue (Ptr<const Packet> p)
{
  stringstream ss;
  
  NS_LOG_INFO (ss.str());
  Ptr<Packet> pcp = p->Copy();
  MyTag tag;
  if(pcp->PeekPacketTag(tag))
  {
    int idx = tag.GetSimpleValue() - 1;
    stringstream ss;
    // ss << " " << Simulator::Now().GetSeconds() << "s: Tc Queue drop! " << idx + 1<< ". " << ++ cnt2[idx - 1];
    ss << "  " << Simulator::Now().GetSeconds() << "s: packet enqueued! node " << idx + 1;
    NS_LOG_INFO(ss.str());
  }   
}

static void
PktDequeue (Ptr<const Packet> p)
{
  stringstream ss;
  ss << " " << Simulator::Now().GetSeconds() << "s: packet dequeued!";
  NS_LOG_INFO (ss.str());
}

static void
QueueDrop (Ptr<const QueueDiscItem> qi)
{
  realtimeDrop += 1;
  Ptr<Packet> p = qi->GetPacket();
  MyTag tag;
  if(p->PeekPacketTag(tag))
  {
    int idx = tag.GetSimpleValue() - 1;
    dropArray[idx] += 1;
    stringstream ss;
    ss << " " << Simulator::Now().GetSeconds() << "s: Tc Queue drop! " << idx + 1<< ". " << ++ cnt2[idx - 1];
    // if(cnt2[idx - 1] % 20 == 0) 
    // NS_LOG_INFO(ss.str());
    cout << ss.str() << endl;
  }   
}

static void
RttTracer (Time oldval, Time newval)
{
  stringstream ss;
  if (firstRtt)
    {
      ss << "0.0 " << oldval.GetSeconds () << std::endl;
      firstRtt = false;
    }
  ss << Simulator::Now ().GetSeconds () << " " << newval.GetSeconds () << std::endl;
  cout << ss.str() << endl;
}


// following is my test tracing function
int cntn[4][4] = {0};
MyTag tag;
uint32_t idx;

static void
PktMacRx (Ptr<const Packet> p)
{
  Ptr<Packet> pktCopy = p->Copy();
  if(pktCopy->PeekPacketTag(tag)) idx = tag.GetSimpleValue() - 1;
  if(idx == 0)
    cout << "  " << Simulator::Now().GetSeconds() << "s " << idx + 1 << " : " << cntn[idx][0] ++ <<" : Mac Rx, packet passed up from Phy" << endl;
}

static void
PktPhyTxBegin (Ptr<const Packet> p)
{
  Ptr<Packet> pktCopy = p->Copy();
  if(pktCopy->PeekPacketTag(tag)) idx = tag.GetSimpleValue() - 1;
  if(idx == 0)
    cout << idx + 1 << " : " << cntn[idx][1] ++ <<" : Phy Tx Begin" << endl;
}

static void
PktPhyTxEnd (Ptr<const Packet> p)
{
  Ptr<Packet> pktCopy = p->Copy();
  if(pktCopy->PeekPacketTag(tag)) idx = tag.GetSimpleValue() - 1;
  if(idx == 0)
    cout << "  " << idx + 1 << " : " << cntn[idx][2] ++ <<" : Phy Tx End" << endl;
}

static void
PktPhyRxEnd (Ptr<const Packet> p)
{
  Ptr<Packet> pktCopy = p->Copy();
  if(pktCopy->PeekPacketTag(tag)) idx = tag.GetSimpleValue() - 1;
  if(idx == 0)
    cout << "    " << idx + 1 << " : " << cntn[idx][3] ++ <<" : Phy Rx End" << endl;
}

static void
PktTxDrop (Ptr<const Packet> p)
{
  Ptr<Packet> pktCopy = p->Copy();
  if(pktCopy->PeekPacketTag(tag)) idx = tag.GetSimpleValue() - 1;
  if(idx == 0)
    cout << "==" << idx + 1 << " : " << cntn[idx][3] ++ <<" : Phy Tx Drop!!!" << endl;
}

//===========================Main Function=============================//
int
main (int argc, char *argv[])
{
  uint32_t maxPackets = 250;
  uint32_t modeBytes = 0;
  double minTh = 100;
  double maxTh = 200;
  uint32_t pktSize = 1000; // 1000 KB
  double stopTime = 3;
  string pktPrint;
  string prot;
  isPrintHeader = false; // if print tcp header
  isPrintTx = false;
  isPrintLeft = false;
  isPrintRx = false;
  isTcp = true;
  isChangeFair = false;
  uint32_t maxWinSize = 131070;
  fairness = "persender";         // choose from: natural, persender

  string appDataRate = "20Mbps"; // no use here
  string queueType = "RED";
  string bottleNeckLinkBw = "100Mbps";
  string bottleNeckLinkDelay = "5ms";
  string attackFlowType;
  
  // just copy the original below
  std::string mtu = "1599";

  // define the file names here
  ofstream fout[nRight];
  for (int i = 0; i < nRight; i++)
    {
      fileName.push_back ("wndOutput_" + to_string (i) + ".dat");
      remove (fileName[i].c_str ());
    }
  qFileName = "queue.dat";
  remove(qFileName.c_str());

  // initialize tPkt
  for (int i = 0; i < ARRAY_SIZE; i++)
  {
    tPkt[i] = 0.0;
    txCnt[i] = 0;
    rxCnt[i] = 0;
  }

  //get the local time
  std::time_t t = std::time (NULL);
  char localTime[100];
  std::strftime (localTime, 100, "%c", std::localtime (&t));

  CommandLine cmd;
  cmd.AddValue ("nLeft", "Number of left side nodes", nLeft);
  cmd.AddValue ("enableEarlyDrop", "enableEarlyDrop", enableEarlyDrop);
  cmd.AddValue ("attackerDataRate", "attack data rate", attackerDataRate);
  cmd.AddValue ("clientDataRate", "legitimate users data rate", clientDataRate);
  cmd.AddValue ("bottleNeckLinkDelay", "bottle neck link delay", bottleNeckLinkDelay);    /// added
  cmd.AddValue ("bottleNeckLinkBw", "bottle neck link bandwidth", bottleNeckLinkBw);
  cmd.AddValue ("stopTime", "Stopping time for simulation", stopTime);
  cmd.AddValue ("attackFlowType", "Type of attacking flows", attackFlowType);
  cmd.AddValue ("nRight", "Number of TCP attacking flows", nRight);
  cmd.AddValue ("maxPackets", "Max Packets allowed in the queue", maxPackets);
  cmd.AddValue ("queueType", "Set Queue type to DropTail or RED", queueType);
  cmd.AddValue ("appDataRate", "Set OnOff App DataRate", appDataRate);
  cmd.AddValue ("modeBytes", "Set Queue mode to Packets <0> or bytes <1>", modeBytes);
  cmd.AddValue ("nCrossing", "The number of crossing traffic flows", nCrossing);

  cmd.AddValue ("enablePacketPrint", "enable printing packet detail in log", pktPrint);
  cmd.AddValue ("fairness", "Set the fairness policy of the mbox", fairness);
  cmd.AddValue ("protocol", "The protocol of TCP / UDP", prot);

  cmd.AddValue ("redMinTh", "RED queue minimum threshold", minTh);
  cmd.AddValue ("redMaxTh", "RED queue maximum threshold", maxTh);
  cmd.AddValue ("maxWindowSize", "Max TCP window size", maxWinSize);
  cmd.Parse (argc, argv);

  cout << "bottlelNeck Link BW: " << bottleNeckLinkBw << endl;
  cout << "client Data Rate: " << clientDataRate << endl;
  cout << "attacker Data Rate: " << attackerDataRate << endl;
  cout << "bottleNeck Link Delay: " << bottleNeckLinkDelay << endl;
  cout << "fairness: " << fairness << endl;
  
  isPrintHeader = pktPrint == "y"? true : (pktPrint == "n"? false : isPrintHeader);
  isTcp = prot == "TCP"? true: (prot == "UDP"? false : isTcp);
  cout << "protocol: " << prot << endl;
  attackFlowType = isTcp? "ns3::TcpSocketFactory":"ns3::UdpSocketFactory"; // if we need here?
  string flowType = isTcp? "ns3::TcpSocketFactory":"ns3::UdpSocketFactory";       // change all to udp



//   TcpOptionWinScale winScale;
//   cout << "win scale: " << winScale.GetScale() << "; id: " << winScale.GetTypeId() << endl;
//   cout << winScale.GetSerializedSize() << endl;
  
//   winScale.SetScale(2);
//   cout << "win scale: " << winScale.GetScale() << endl;

  // from command line arguments
  // Config::SetDefault("ns3::TcpSocketBase::MaxWindowSize", UintegerValue(131070));     // doesn't work
  // Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue (maxWinSize)); 

  /*  1. DropTail: simple active queuing management(AQM) algorithm, drop packets after queue is full (not fair, -> bad global tcp sync)
      2. RED (Random Early Detection): monitor average queue size, mark/drop packet based on probability (control queue size, avoid global sync, ...)
      see http://www.roman10.net/2011/11/10/drop-tail-and-redtwo-aqm-mechanisms/ */

  Packet::EnablePrinting ();
  Packet::EnableChecking ();

  if ((queueType != "RED") && (queueType != "DropTail"))
    {
      NS_ABORT_MSG ("Invalid queue type: Use --queueType=RED or --queueType=DropTail");
    }

  //configuration
  //Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (pktSize));
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  //Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (appDataRate));

  // if (modeBytes)
  // {
  //   Config::SetDefault("ns3::DropTailQueue::Mode", StringValue("QUEUE_MODE_PACKETS"));
  //   Config::SetDefault("ns3::DropTailQueue::MaxPackets", UintegerValue(maxPackets));
  //   Config::SetDefault("ns3::RedQueue::Mode", StringValue("QUEUE_MODE_PACKETS"));
  //   Config::SetDefault("ns3::RedQueue::QueueLimit", UintegerValue(maxPackets));
  // }
  // else
  // {
  //   Config::SetDefault("ns3::DropTailQueue::Mode", StringValue("QUEUE_MODE_BYTES"));
  //   Config::SetDefault("ns3::DropTailQueue::MaxBytes", UintegerValue(maxPackets * pktSize));
  //   Config::SetDefault("ns3::RedQueue::Mode", StringValue("QUEUE_MODE_BYTES"));
  //   Config::SetDefault("ns3::RedQueue::QueueLimit", UintegerValue(maxPackets * pktSize));
  //   minTh *= pktSize;
  //   maxTh *= pktSize;
  // }

  // minTh *= pktSize;
  // maxTh *= pktSize;

  /// set log components
  LogComponentEnable ("TestForMiddlePolice", LOG_LEVEL_INFO); /// test here
  // LogComponentEnable ("PointToPointDumbbellHelper", LOG_LEVEL_INFO);    /// none
  // LogComponentEnable ("PointToPointHelper", LOG_LEVEL_INFO);            /// none
  // LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
  // LogComponentEnable ("PacketSocket", LOG_LEVEL_INFO);                  /// after testing: no this module in the code
  // LogComponentEnable ("PacketSocketClient", LOG_LEVEL_INFO);            /// no this
  // LogComponentEnable ("PacketSocketServer", LOG_LEVEL_INFO);            /// no this

  //===================Create network topology===========================//
  // Need to create three links for this topo
  // 1. The connection between left nodes and left router
  // 2. The connection between right nodes and right router
  // 3. The connection between two routers
  PointToPointHelper bottleNeckLink;
  bottleNeckLink.SetDeviceAttribute ("DataRate", StringValue (bottleNeckLinkBw));
  bottleNeckLink.SetDeviceAttribute ("Mtu", StringValue (mtu)); /// maximum transmission unit, largest packet or frame size, that can be sent in a packet- or frame-based network
  bottleNeckLink.SetChannelAttribute ("Delay", StringValue (bottleNeckLinkDelay));

  bottleNeckLink.SetQueue(
    "ns3::DropTailQueue",
    "MaxSize", StringValue("100p"));       // normal 200p, 20p only for test

  //leaf helper:
  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute ("DataRate", StringValue ("100000Mbps")); // 1 Tbps
  pointToPointLeaf.SetChannelAttribute ("Delay", StringValue ("1ms"));

  // Dumbbell constructor: nLeaf normal flows and nAttacker attack flows
  /// left nodes <-> router <-> router <-> right nodes, like the shape of dumpbell

  PointToPointDumbbellHelper d1 (nLeft, pointToPointLeaf, nRight, pointToPointLeaf, bottleNeckLink);

  // Install Stack to the whole nodes
  InternetStackHelper stack;
  d1.InstallStack (stack);
  // for debug
  cout << "#interfaces: " << d1.GetLeft()->GetObject<Ipv4>()->GetNInterfaces() << endl;
  cout << endl;

  // RED queue disc
  QueueDiscContainer qdRight;
  QueueDiscContainer qdLeft;
  if (queueType == "RED")
  {
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc",
                        "MinTh", DoubleValue(minTh),
                        "MaxTh", DoubleValue(maxTh),
                        "LinkBandwidth", StringValue(bottleNeckLinkBw),
                        "LinkDelay", StringValue(bottleNeckLinkDelay));
    // tch.SetRootQueueDisc("ns3::RedQueueDisc");
    qdRight = tch.Install(d1.GetRight()->GetDevice(0));
    qdLeft = tch.Install(d1.GetLeft()->GetDevice(0));
  }

  // for debug
  cout << "Before assign...\n#interfaces: " << d1.GetLeft()->GetObject<Ipv4>()->GetNInterfaces() << endl;
  cout << endl;

  // Assign IP Addresses
  // Three sets of address: the left, the right and the router
  d1.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.0.0", "255.255.255.252"),
                          Ipv4AddressHelper ("11.1.0.0", "255.255.255.252"),
                          Ipv4AddressHelper ("12.1.0.0", "255.255.255.252"));

  // obtain the p2p devices
  //===================End of Creating Network Topology===================//

// for debug
  cout << "before sink... \n #interfaces: " << d1.GetLeft()->GetObject<Ipv4>()->GetNInterfaces() << endl;
  cout << endl;

  //==========================================================================//
  //===================Config the LEFT side nodes: sink or receiver===========//
  //==========================================================================//

  // return the address of that endpoint
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  Address attackerSinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), attackerport));

  /* what's the difference of packet sink & cross sink ? packet sink is what we care?*/
  // create the package sink application, which means that the endpoint will receive packets using certain protocols

  // PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  PacketSinkHelper packetSinkHelper (flowType, sinkLocalAddress);

  PacketSinkHelper attackerPacketSinkHelper(attackFlowType, attackerSinkLocalAddress);

  // Create the normal TCP flows sink applications
  ApplicationContainer sinkApps;
  //for (uint32_t i = 0; i < d1.LeftCount () - nAttacker; ++i)
  for (uint32_t i = 0; i < d1.LeftCount () - nAttacker; ++i)
    {
      // packetSinkHelper.Install (node): install the sink app on this node   /// which is tcp on sink local addr
      // sinkApps.add (app): add one single application to the container
      sinkApps.Add (packetSinkHelper.Install (d1.GetLeft (i)));
    }

  // Create the attack flow sink application
  ApplicationContainer attackerSinkApps;
  for (uint32_t j = d1.LeftCount() - nAttacker; j < d1.LeftCount(); ++j)
  {
    attackerSinkApps.Add(attackerPacketSinkHelper.Install(d1.GetLeft(j)));
  }

  // Arrange all application in the container to start and stop
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (stopTime));
  attackerSinkApps.Start(Seconds(0.0));
  attackerSinkApps.Stop(Seconds(stopTime));

  //==========================================================================//
  //======================End of sink applications============================//
  //===========================================================================//
// for debug
  cout << "Before sender...\n #interfaces: " << d1.GetLeft()->GetObject<Ipv4>()->GetNInterfaces() << endl;
  cout << endl;

  //==========================================================================//
  //================Creating the normal client applications===================//
  //==========================================================================//

  for (uint32_t i = 0; i < d1.RightCount () - nAttacker; ++i)
    {
      Ptr<Socket> ns3Socket = isTcp? Socket::CreateSocket (d1.GetRight (i), TcpSocketFactory::GetTypeId ())
                              :Socket::CreateSocket(d1.GetRight (i), UdpSocketFactory::GetTypeId());
      Address sinkAddress (
          InetSocketAddress (d1.GetLeftIpv4Address (i % nLeft),
                             port)); /// send to i th left node, map right node to left node
      Ptr<MyApp> app = CreateObject<MyApp> ();
      uint32_t tagValue = i + 1; //take the least significant 8 bits

      // 2nd line for idtentify each packet to get the RTT
      app->SetTagValue (tagValue);

      app->Setup (ns3Socket, sinkAddress, pktSize, DataRate (clientDataRate));
      d1.GetRight (i)->AddApplication (app);
      app->SetStartTime (Seconds (0));
      app->SetStopTime (Seconds (stopTime));
    }
  //==========================================================================//
  //==================End of normal client applications=======================//
  //==========================================================================//

  //========================flow monitor======================================//

  Ptr<FlowMonitor> flowmon;
  FlowMonitorHelper flowmonHelper;
  flowmon = flowmonHelper.InstallAll ();
  flowmon->Start (Seconds (0.0));
  flowmon->Stop (Seconds (stopTime));

  //==========================================================================//
  //================Creating the attack client applications===================//
  //==========================================================================//
  double start = 0.1;
  for (; start < stopTime; start += period)
  {
    //Simulator::Schedule(Seconds (start), &measureDropRate, flowmon, start);
    Simulator::Schedule(Seconds(start), &attackFlow, d1, nAttacker);
  }



  //==========================================================================//

  for (uint32_t j = 0; j < nRight; ++j)
    {
      dropArray[j] = 0;
      congWin[j] = 0;
      receiveWin[j] = 0;
      lossRateArray[j] = 0;
      recvData[j] = 0.0;
      recvBit[j] = 0.0;
    }
  //=============================Trace source============================//
  router = d1.GetRight ();
  Ptr<Node> rightRouter = d1.GetRight ();
  Ptr<Node> leftRouter = d1.GetLeft ();
  // std::cout << "number of devices: " << rightRouter->GetNDevices() << std::endl;
  cout << "trace (right): " << endl;


  cout << "left router: " << leftRouter->GetDevice(0);


  /* 
  why do this stupid scan and use mtu to find router? we can directly visit it!
    - the index of router among net devices will change from 0?
     
  Ptr<NetDevice> rightLeaf = d1.GetRight()->GetDevice(i);
  Ptr<NetDevice> rightRouter = d1.GetRight()->GetDevice(0);
  */

  // for (uint32_t i = 0; i < rightRouter->GetNDevices (); ++i)
  //   {
      // Find the bottleneck device (p2p device), check the other devices
      // cout << rightRouter-> GetDevice(i) -> GetTypeId() << " : " << rightRouter->GetDevice(i)->GetMtu() << " =? " << mmtu << endl;

      /*
      ********************** i = 0: important!!!  **************************
      */
      uint32_t i = 0;       

      // if ((rightRouter->GetDevice (i)->GetMtu ()) == mmtu) // don
      if(true)
        { /// get mtu = mmtu: what's that mean
          // bottleNeckLink.EnablePcap ("rrouter", rightRouter->GetDevice (i)); /// PCAP: a binary format for packet capture
          p2pDevice = DynamicCast<PointToPointNetDevice> (router->GetDevice (i));       // it's used for setEarlyDrop
          // rightRouter->GetDevice (i)->TraceConnectWithoutContext ("MacTx", MakeCallback (&PktArrival));
          
          // // MacRx for testing
          // rightRouter->GetDevice (i)->TraceConnectWithoutContext ("MacRx", MakeCallback (&PktMacRx));

          // rightRouter->GetDevice (i)->TraceConnectWithoutContext ("MacTxDrop", MakeCallback (&PktDrop));;
          // rightRouter->GetDevice (i)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&PktDropOverflow));
          // // following: only for test
          // rightRouter->GetDevice (i)->TraceConnectWithoutContext ("PhyTxDrop", MakeCallback (&PktTxDrop));
          

          d1.GetRight()->GetDevice(i)->TraceConnectWithoutContext ("MacTxDrop", MakeCallback (&PktDrop));    // it's mbox 's control, con't count in LLR
          d1.GetRight()->GetDevice(i)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&PktDrop));
          d1.GetRight()->GetDevice(i)->TraceConnectWithoutContext ("MacTx", MakeCallback (&PktArrival));
          // d1.GetRight()->GetDevice(i)->TraceConnectWithoutContext ("MacRx", MakeCallback (&PktMacRx));

          Ptr<Queue<Packet> > qDev = p2pDevice->GetQueue();
          // qDev->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&QueueChange));
          // qDev->TraceConnectWithoutContext("Enqueue", MakeCallback(&PktEnqueue));
          // qDev->TraceConnectWithoutContext("Dequeue", MakeCallback(&PktDequeue));
          qDev->TraceConnectWithoutContext("Drop", MakeCallback(&PktDrop));     // it's in dev queue, counting in LLR
          // qdev, mactxdrop, phyrxdrop don't drop pkt here


          if(queueType == "RED")
          {
            Ptr<QueueDisc> qd = qdRight.Get(0);
            qd->TraceConnectWithoutContext("Drop", MakeCallback(&QueueDrop));   // it's in RED queue, counting in LLR
            qd->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&QueueChange));
          }

        //   Simulator::Schedule (Seconds (0.00001), &TraceRtt, "mptest-rtt.data");
        }
    // }

  Config::ConnectWithoutContext ("/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/RTT",
                                 MakeCallback (&RttTracer));

  //===========================================================================//

//   // ascii tracing
//   AsciiTraceHelper ascii;
//   bottleNeckLink.EnableAsciiAll (ascii.CreateFileStream ("mptest_woTag.tr"));
//   bottleNeckLink.EnablePcapAll ("mptest_woTag");

  // Routing table
  Ipv4GlobalRoutingHelper::PopulateRoutingTables (); /// exact meaning? necessary or not?

  std::cout << "Running the simulation" << std::endl;
  // The last event scheduled by the simulator
  Simulator::Stop (Seconds (stopTime));

  Simulator::Run ();

  // Calculating the sending rate
  // Legitimate flows
  double clientCounter = 0;
  double clientCounterSquare = 0;
  double totalCounter = 0;
  double totalCounterSquare = 0;

  for (uint32_t i = 0; i < nRight - nAttacker; i++)
    {
      /// number of packet we care about
      Ptr<Application> app = sinkApps.Get (i);
      // PacketSink: receive and consume the traffic generated to the IP address and port
      Ptr<PacketSink> pktSink = DynamicCast<PacketSink> (app);

      // GetTotalRx: total bytes received in a sink app
      double bytes =
          1.0 * pktSink->GetTotalRx () * 8 / 1000000; /// why multiply by 8? I think it's bits??
      totalCounter += bytes;
      clientCounter += bytes;
      clientCounterSquare += (bytes * bytes); /// -> used to compute client index below
      totalCounterSquare += (bytes * bytes);
    }

    //Attack flows
  double attackerCounter = 0;
  double attackerCounterSquare = 0;
  for (uint32_t j = 0; j < attackerSinkApps.GetN(); j++)
  {
    Ptr<Application> app = attackerSinkApps.Get(j);
    // PacketSink: receive and consume the traffic generated to the IP address and port
    Ptr<PacketSink> pktSink = DynamicCast<PacketSink>(app);

    // GetTotalRx: total bytes received in a sink app
    double bytes = 1.0 * pktSink->GetTotalRx() * 8 / 1000000;
    attackerCounter += bytes;
    totalCounter += bytes;
    totalCounterSquare += (bytes * bytes);
    attackerCounterSquare += (bytes * bytes);
  }

  double normalAverageRate = clientCounter / Simulator::Now ().GetSeconds () / nRight;
  double client_index = clientCounter * clientCounter / clientCounterSquare / nRight;
  double attackerAverageRate = nAttacker == 0 ? 0 : attackerCounter / Simulator::Now().GetSeconds() / nAttacker;
  double total_index =
      totalCounter * totalCounter / totalCounterSquare / nRight; /// what's the meaning?

  cout << endl << "Final bytes: " << totalCounter << " Mbit; mean rate: " << normalAverageRate * 1000 << " kbps" << endl;

  //output to a file
  std::ofstream outputFile ("samerate", std::ios::out | std::ios::app);
  //std::ofstream outputFile ("output", std::ios::out | std::ios::app);
  //std::ofstream outputFile ("sstf", std::ios::out | std::ios::app);
  if (outputFile.is_open ())
    {
      outputFile << "==============================================================="
                 << "\nMbox test with flat rate: "
                 << "\nRun simulation at: " << localTime 
                 << "\nSimulation duration: " << stopTime
                 //<< "\nAttack Flow Type: " << attackFlowType
                 << "\nBottleneck link bandwidth: " << bottleNeckLinkBw
                 << "\nBottleneck link delay: " << bottleNeckLinkDelay
                 << "\nAttacker data rate: " << attackerDataRate
                 << "\nLegitimate data rate: " << clientDataRate 
                 << "\nEnable early drop " << enableEarlyDrop 
                //  << "\nNumber of attackers: " << 0
                 << "\nNumber of normal users: " << nRight
                 << "\nNumber of crossing users: " << nCrossing 
                 << "\nQueue Type: " << queueType
                 << "\nPeriod: " << period << ", attack duration: " << duration
                 << "\nLoss rate threshold: "<< lossRateThreshold
                 //<< "\nAttack Rate: " << attackerDataRate
                 //<< "\nSimulation Time: "
                 //<< Simulator::Now ().GetSeconds ()
                 //<< "\nNormal Flows Received Bytes: "
                 //<< totalRxBytesCounter
                 << "\nNormal Averaged Flows Rate: " << normalAverageRate << " Mbps"
                 << "\nAttack Averaged Flow Rate: 0"
                 //  << attackerAverageRate << " Mbps"
                 << "\nClient fairness index: " << client_index
                 << "\nTotal fairness index: " << total_index
                 << "\n==============================================================\n";

      outputFile.close ();
      //outfile.close();
    }
  else
    {
      std::cout << "Open File error" << std::endl;
    }

  //flow monitor output
  flowmon->SerializeToXmlFile ("red.flowmon", false, false);

  // stat standard output

  std::cout << "Destroying the simulation" << std::endl;
  Simulator::Destroy ();
  return 0;
}