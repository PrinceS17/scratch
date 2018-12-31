#ifndef APPS_H
#define APPS_H

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

class MyApp : public Application 
{
public:

  MyApp ();
  virtual ~MyApp();

  //void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);
  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate);
  void SetTagValue(uint32_t value);
  void SetDataRate(DataRate rate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  //uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  //uint32_t        m_packetsSent;
  uint32_t         m_tagValue;
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
  tag.SetSimpleValue (m_tagValue);

  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  packet -> AddPacketTag (tag);//add tags

  m_socket->Send (packet);

  //if (++m_packetsSent < m_nPackets)
    //{
      ScheduleTx ();
    //}
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
//=========================================================================//
//===================End of Application definition=========================//
//=========================================================================//

#endif
