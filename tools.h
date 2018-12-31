#ifndef TOOLS_H
#define TOOLS_H

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

# include <vector>
#include "ns3/tag.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"

using namespace std;
using namespace ns3;

enum ProtocolType {TCP, UDP};
enum FairType {NATURAL, PERSENDER};     // Fairness type

// tool function

vector<int> getPktSizes(Ptr <const Packet> p, ProtocolType pt)     // get [p2p size, ip size, tcp size, data size]
{
  Ptr<Packet> pktCopy = p->Copy();
  vector<int> res;
  PppHeader pppH;
  Ipv4Header ipH;
  TcpHeader tcpH;
  UdpHeader udpH;

  res.push_back((int)pktCopy->GetSize());
  pktCopy->RemoveHeader(pppH);
  res.push_back((int)pktCopy->GetSize());
  pktCopy->RemoveHeader(ipH);
  res.push_back((int)pktCopy->GetSize());
  if(pt == TCP) pktCopy->RemoveHeader(tcpH);
  else pktCopy->RemoveHeader(udpH);
  res.push_back((int)pktCopy->GetSize());
  return res;
}

uint32_t getTcpSize(Ptr <const Packet> p)
{
  Ptr<Packet> pktCopy = p->Copy();
  PppHeader pppH;
  Ipv4Header ipH;
  TcpHeader tcpH;
  pktCopy->RemoveHeader(pppH);
  pktCopy->RemoveHeader(ipH);
  pktCopy->RemoveHeader(tcpH);
  return pktCopy->GetSize();
}

string 
printPkt(Ptr <const Packet> p)
{
  Ptr<Packet> pktCopy = p->Copy();
  stringstream ss;
  pktCopy->Print(ss);
  return ss.str();
}

string
logIpv4Header (Ptr<const Packet> p)
{
  Ptr<Packet> pktCopy = p->Copy ();
  PppHeader pppH;
  Ipv4Header ipH;
  pktCopy->RemoveHeader (pppH);
  pktCopy->PeekHeader (ipH); /// need to know the exact structure of header
  stringstream ss;
  ipH.Print (ss);
  return ss.str ();
}

string
logPppHeader (Ptr<const Packet> p)
{
  Ptr<Packet> pktCopy = p->Copy ();
  PppHeader pppH;
  pktCopy->PeekHeader (pppH);
  stringstream ss;
  pppH.Print (ss);
  return ss.str ();
}

string
logTcpHeader (Ptr<const Packet> p)
{
  Ptr<Packet> pktCopy = p->Copy ();
  PppHeader pppH;
  Ipv4Header ipH;
  TcpHeader tcpH;
  pktCopy->RemoveHeader (pppH);
  pktCopy->RemoveHeader (ipH);
  pktCopy->PeekHeader (tcpH);
  stringstream ss;
  tcpH.Print (ss);
  return ss.str ();
}

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

# endif