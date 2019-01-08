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


#include "mbox.h"
// #include "ns3/apps.h"

using namespace std;

namespace ns3{

NS_LOG_COMPONENT_DEFINE ("MiddlePoliceBox");


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




MiddlePoliceBox::MiddlePoliceBox(vector<uint32_t> num, double tStop, ProtocolType prot, double beta, double th, uint32_t wnd, bool isEDrop)
{
    // constant setting
    nSender = num.at(0);
    nReceiver = num.at(1);
    nClient = num.at(2);
    nAttacker = num.at(3);
    slrWnd = wnd;
    isEarlyDrop = isEDrop;
    this->tStop = tStop;
    this->beta = beta;
    lrTh = th; 

    // internal parameters
    // srand(time(0));          // should be global in main
    MID = rand();
    protocol = prot;
    normSize = 8.0 / 1000;      // byte -> kbit
    isStop = false;
    
    // monitor setting
    rwnd = vector<uint32_t> (nSender, 0);   // rwnd: use to record each senders' rate
    cwnd = vector<uint32_t> (nSender, 0);   // cwnd: use to control senders' rate
    lDrop = vector<uint32_t> (nSender, 0);
    mDrop = vector<uint32_t> (nSender, 0);
    totalRx = vector<uint32_t> (nSender, 0);
    totalRxByte = vector<uint32_t> (nSender, 0);
    totalDrop = vector<uint32_t> (nSender, 0);
    dRate = vector<double> (nSender, 0);
    lastRx2 = vector<uint32_t>(nSender, 0);
    lastDrop = 0;
    lastRx = 0;
    slr = 0.0;
    llr = vector<double> (nSender, 0.0);

    // output setting
    string folder = "MboxStatistics";
    vector<string> name = {"DataRate", "LLR"};       // open to extension
    vector<string> sname = {"SLR", "QueueSize"};       // for data irrelated to sender 
    for(uint32_t i = 0; i < 2; i ++)
    {
        name[i] += "_" + to_string(MID);
        sname[i] += "_" + to_string(MID);
    }
    for(uint32_t i = 0; i < nSender; i ++)
    {
        fnames.push_back(vector<string>());
        fout.push_back(vector<ofstream>());
        for(uint32_t j = 0; j < name.size(); j ++)
        {
            fnames.at(i).push_back(folder + "/" + name[j] + "_" + to_string(i) + ".dat");
            remove(fnames.at(i).at(j).c_str());
            fout.at(i).push_back(ofstream(fnames.at(i).at(j), ios::app | ios::out));
        }
    }   // need test here

    for(uint32_t i = 0; i < sname.size(); i ++)
    {
        singleNames.push_back(folder + "/" + sname[i] + ".dat");
        remove(singleNames.at(i).c_str());
        singleFout.push_back(ofstream(singleNames.at(i), ios::app | ios::out));
    }

    // log function only for debug
    stringstream ss;
    ss << "\n # sender: " << nSender << ", # receiver: " << nReceiver << endl;
    ss << " # client: " << nClient << ", # attacker: " << nAttacker << endl;
    ss << " Stop time: " << tStop << ", protocol: " << (protocol == TCP? "TCP":"UDP") << endl;
    ss << "\n Test file name llr[0]: " << fnames.at(0).at(1) << endl;
    ss << " Test file name slr: " << singleNames.at(0) << endl;
    NS_LOG_INFO(ss.str());
}

MiddlePoliceBox::MiddlePoliceBox(const MiddlePoliceBox& mb):
  rwnd(mb.rwnd), cwnd(mb.cwnd), mDrop(mb.mDrop), lDrop(mb.lDrop), totalRx(mb.totalRx), totalRxByte(mb.totalRxByte),
  totalDrop(mb.totalDrop), lastDrop(mb.lastDrop), lastRx(mb.lastRx), lastRx2(mb.lastRx2), slr(mb.slr), llr(mb.llr), dRate(mb.dRate),
  nSender(mb.nSender), nClient(mb.nClient), nAttacker(mb.nAttacker), nReceiver(mb.nReceiver), slrWnd(mb.slrWnd),
  isEarlyDrop(mb.isEarlyDrop), tStop(mb.tStop), beta(mb.beta), lrTh(mb.lrTh), MID(mb.MID), device(mb.device),
  fnames(mb.fnames), singleNames(mb.singleNames), normSize(mb.normSize), protocol(mb.protocol), isStop(mb.isStop) 
{
    NS_LOG_FUNCTION(" Copy constructor. ");
    for(uint32_t i = 0; i < nSender; i ++)
    {
        fout.push_back(vector<ofstream>());
        for(uint32_t j = 0; j < fnames.at(i).size(); j ++)
        {
            remove(fnames.at(i).at(j).c_str());
            fout.at(i).push_back(ofstream(fnames.at(i).at(j), ios::app | ios::out));
        }
    }
    for(uint32_t i = 0; i < singleNames.size(); i ++)
    {
        remove(singleNames.at(i).c_str());
        singleFout.push_back(ofstream(singleNames.at(i), ios::app | ios::out));
    }
}

MiddlePoliceBox::~MiddlePoliceBox()
{
    NS_LOG_FUNCTION(" Destructor. ");
    clear();
    for(uint32_t i = 0; i < nSender; i ++)
    {
        singleFout.at(i).close();
        for(uint32_t j = 0; j < fnames.size(); j ++)
            fout.at(i).at(j).close();
    }
}

void
MiddlePoliceBox::install(Ptr<NetDevice> device)
{
    Ptr<PointToPointNetDevice> p2pDev = DynamicCast<PointToPointNetDevice> (device);
    this->device = p2pDev;
}

void
MiddlePoliceBox::install(NetDeviceContainer nc)
{
    macRxDev = vector<Ptr<PointToPointNetDevice> >(nc.GetN());
    for(uint32_t i = 0; i < nc.GetN(); i ++)
        macRxDev.at(i) = DynamicCast<PointToPointNetDevice> (nc.Get(i));
}

// for test/debug only
void MiddlePoliceBox::txSink(Ptr<const Packet> p)
{
    // for debug only
    static vector<uint32_t> cnt = {0,0,0,0};

    Ptr<const Packet> pcp = p->Copy();
    MyTag tag;
    if(!pcp->PeekPacketTag(tag)) return;
    int index = tag.GetSimpleValue() - 1;
    cnt[index] ++;
    double bytes = getPktSizes(pcp, UDP).at(3);
    NS_LOG_FUNCTION(" Begin: " + to_string(index));
    
    stringstream ss;
    ss << "MacTx: No. " << index << ", " << cnt[index];
    NS_LOG_FUNCTION(ss.str());
}

// for test/debug only
void MiddlePoliceBox::rxDrop(Ptr<const Packet> p)
{
    Ptr<const Packet> pcp = p->Copy();
    MyTag tag;
    if(!pcp->PeekPacketTag(tag)) return;
    int index = tag.GetSimpleValue() - 1;
    double bytes = getPktSizes(pcp, UDP).at(3);
    NS_LOG_FUNCTION(" Begin: " + to_string(index));
    
}

void
MiddlePoliceBox::onMacTx(Ptr<const Packet> p)
{
    Ptr<const Packet> pcp = p->Copy();
    MyTag tag;
    if(!pcp->PeekPacketTag(tag)) return;
    int index = tag.GetSimpleValue() - 1;
    rwnd[index] ++;
    NS_LOG_FUNCTION(" Begin: " + to_string(index));

    // best-effort packet handling
    if(isEarlyDrop && rwnd[index] > cwnd[index] && (slr > lrTh || llr[index] > lrTh))
    {

        device->SetEarlyDrop(true);
        mDrop[index] ++;
    }

    // only for debug
    NS_LOG_FUNCTION(" rwnd[" + to_string(index) + "] = " + to_string(rwnd[index]));
}

void
MiddlePoliceBox::onMacRx(Ptr<const Packet> p)
{
    Ptr<const Packet> pcp = p->Copy();
    MyTag tag;
    if(!pcp->PeekPacketTag(tag)) return;
    int index = tag.GetSimpleValue() - 1;
    rwnd[index] ++;
    NS_LOG_FUNCTION(" Begin: " + to_string(index));

    // best-effort packet handling
    if(isEarlyDrop && rwnd[index] > cwnd[index] && (slr > lrTh || llr[index] > lrTh))
    {
        // device->SetEarlyDrop(true);
        macRxDev.at(index)->SetEarlyDrop(true);
        mDrop[index] ++;
    }

    // only for debug
    NS_LOG_FUNCTION(" rwnd[" + to_string(index) + "] = " + to_string(rwnd[index]));
}

void
MiddlePoliceBox::onMacTxWoDrop(Ptr<const Packet> p)
{
    NS_LOG_FUNCTION(" Begin ... ");
    Ptr<const Packet> pcp = p->Copy();
    MyTag tag;
    if(!pcp->PeekPacketTag(tag)) return;
    int index = tag.GetSimpleValue() - 1;
    rwnd[index] ++;

    NS_LOG_FUNCTION(" rwnd[" + to_string(index) + "] = " + to_string(rwnd[index]) + " (no drop here)");
}

void
MiddlePoliceBox::onQueueDrop(Ptr<const QueueDiscItem> qi)
{   
    NS_LOG_FUNCTION(" Begin ... ");
    Ptr<const Packet> pcp = qi->GetPacket()->Copy();
    MyTag tag;
    if(!pcp->PeekPacketTag(tag)) return;
    int index = tag.GetSimpleValue() - 1;
    // rwnd[index] ++;          // compensate for the dropped packet not count in MacTx, should be delete if later mbox is before tc layer
    lDrop[index] ++;
    totalDrop[index] ++;    // for slr

    // only for debug
    NS_LOG_FUNCTION(" link drop [" + to_string(index) + "] = " + to_string(lDrop[index]));

}

void
MiddlePoliceBox::onPktRx(Ptr<const Packet> p)
{
    Ptr<const Packet> pcp = p->Copy();
    MyTag tag;
    if(!pcp->PeekPacketTag(tag)) return;
    int index = tag.GetSimpleValue() - 1;
    totalRx[index] ++;      // for slr update
    double bytes = getPktSizes(pcp, protocol).at(3);     // tcp bytes
    totalRxByte[index] += bytes;
    NS_LOG_FUNCTION(" Begin: " + to_string(index) + ".");
    //only for debug
    NS_LOG_FUNCTION("pkt rx bytes: " + to_string(bytes) + " B");

    // slr update: should be proper just after updating rx
    bool isUpdate = false;
    uint32_t curRx = accumulate(totalRx.begin(), totalRx.end(), 0);
    if(curRx - lastRx == slrWnd)
    {
        uint32_t curDrop = accumulate(totalDrop.begin(), totalDrop.end(), 0);
        slr = (double) (curDrop - lastDrop) / slrWnd;
        lastRx = curRx;
        lastDrop = curDrop; 
        isUpdate = true;
    }

    // only for debug
    stringstream ss;
    ss << " total rx [" << index << "] = " << totalRx[index] << ", bytes: " << totalRxByte[index] << endl;
    // NS_LOG_FUNCTION(ss.str());
    if(isUpdate)
    {
        ss << "  # RX update: " << lastRx << ", # link drop update: " << lastDrop << "; " << bytes << "B";
        NS_LOG_FUNCTION(ss.str());
        isUpdate = false;
    }
}

void
MiddlePoliceBox::clear()
{
    for(uint32_t i = 0; i < nSender; i ++)
    {
        rwnd[i] = 0;
        // cwnd[i] = 0;
        lDrop[i] = 0;
        mDrop[i] = 0;
    }
    NS_LOG_FUNCTION(" window reset to 0.");
}

void
MiddlePoliceBox::flowControl(FairType fairness, double interval, double logInterval)
{
    // schedule
    if(!isStop) 
        Simulator::Schedule(Seconds(interval), &MiddlePoliceBox::flowControl, this, fairness, interval, logInterval);
    if(Simulator::Now().GetSeconds() < logInterval)
        Simulator::Schedule(Seconds(0.1), &MiddlePoliceBox::statistic, this, logInterval);
    NS_LOG_FUNCTION(" Begin ... ");

    // loss rate update and capacity calculation
    double capacity = 0;
    for(uint32_t i = 0; i < nSender; i ++)
    {
        // double lossRate = rwnd[i] > 0? (double) (lDrop[i] + mDrop[i])/rwnd[i] : 0.0;
        // debug only: what if only count link drop into llr?
        double lossRate = rwnd[i] > 0? (double) lDrop[i] / rwnd[i] : 0.0;

        llr[i] = rwnd[i] > 5? (1 - beta) * lossRate + beta * llr[i] : beta * llr[i];
        double tmp = rwnd[i] - lDrop[i] - mDrop[i]; 
        capacity += rwnd[i] > lDrop[i] + mDrop[i]? tmp : 0;
    }

    // flow control policy based on fairness: add cases to extend more fairness
    for(uint32_t i = 0; i < nSender; i ++)
    {
        switch(fairness)
        {
            case NATURAL:
                cwnd[i] = rwnd[i] > lDrop[i] + mDrop[i]? rwnd[i] - lDrop[i] - mDrop[i] : 0;
                break;
            case PERSENDER:
                cwnd[i] = capacity / nSender;
                break;
            default: ;
        }
    }

    // only for debug
    stringstream ss;
    ss << "\n                MID: " << MID << "; capacity: " << capacity << "; slr: " << slr << endl;
    ss << "flow control:   No.  rwnd   cwnd   mdrop  ldrop  llr" << endl;
    for (uint32_t i = 0; i < nSender; i ++)
        ss << "                " << i << "    " << rwnd[i] << "    " << cwnd[i] << "    " << mDrop[i]
            << "    " << lDrop[i] << "    " << llr[i] << endl;
    NS_LOG_INFO(ss.str());

    clear();
}

void
MiddlePoliceBox::statistic(double interval)
{
    // static vector<uint32_t> lastRx2 = vector<uint32_t>(nSender, 0);
    if(!isStop) 
        Simulator::Schedule(Seconds(interval), &MiddlePoliceBox::statistic, this, interval);
    NS_LOG_FUNCTION(" Begin, nSender: " + to_string(nSender));
        
    for(uint32_t i = 0; i < nSender; i ++)
    {
        dRate[i] = (double)(totalRxByte[i] - lastRx2[i]) * normSize / interval;     // record rx data rate
        fout.at(i)[0] << Simulator::Now().GetSeconds() << " " << dRate.at(i) << " kbps" << endl; 
        lastRx2[i] = totalRxByte[i];                                                // record llr
        fout.at(i)[1] << Simulator::Now().GetSeconds() << " " << llr.at(i) << endl; 
    }

    singleFout.at(0) << Simulator::Now().GetSeconds() << " " << slr << endl;      // record slr

    // record Queue size: later version feature

    // only for debug
    stringstream ss;
    ss << "                MID: " << MID << "\nstatistics:     No.  rate(kbps)  LLR" << endl;
    for(uint32_t i = 0; i < nSender; i ++)
        ss << "                " << i << "    " << dRate.at(i) << "    " << llr.at(i) << endl;
    NS_LOG_INFO(ss.str());
}

void MiddlePoliceBox::stop()
{
    isStop = true;
    NS_LOG_INFO("Stop mbox now.");
}

}

// int
// main()
// {
//     double tStop = 5.0;
//     double interval = 1.0;
//     double logInterval = 1.0;
//     ProtocolType ptt = UDP;
//     string Protocol = ptt == TCP? "ns3::TcpSocketFactory":"ns3::UdpSocketFactory";
//     uint32_t port = 8080;
//     string bw = "10Mbps";
//     string txRate = "100Mbps";
//     LogComponentEnable("MiddlePoliceBox", LOG_LEVEL_INFO);    // see function log to test
//     // LogComponentEnable("PointToPointNetDevice", LOG_FUNCTION);

//     NodeContainer n1;
//     n1.Create(2);
//     PointToPointHelper p2p;
//     p2p.SetChannelAttribute("Delay", StringValue("2ms"));
//     p2p.SetDeviceAttribute("DataRate", StringValue(bw));
//     NetDeviceContainer dev = p2p.Install(n1);
//     InternetStackHelper ish;
//     ish.Install(n1);

//     // queue setting
//     QueueDiscContainer qc;
//     TrafficControlHelper tch1;
//     // tch1.Uninstall(dev);
//     tch1.SetRootQueueDisc("ns3::RedQueueDisc",
//                         "MinTh", DoubleValue(5),
//                         "MaxTh", DoubleValue(25),
//                         "LinkBandwidth", StringValue(bw),
//                         "LinkDelay", StringValue("2ms"));
//     qc = tch1.Install(dev);
    
//     Ipv4AddressHelper idh;
//     idh.SetBase("10.1.1.0", "255.255.255.0");
//     Ipv4InterfaceContainer ifc = idh.Assign(dev);
    
//     // sink app
//     Address sinkLocalAddr (InetSocketAddress (Ipv4Address::GetAny (), port));
//     ApplicationContainer sinkApp;
//     PacketSinkHelper psk(Protocol, sinkLocalAddr);
//     sinkApp = psk.Install(n1.Get(1));
//     sinkApp.Start(Seconds(0.0));
//     sinkApp.Stop(Seconds(tStop));

//     // tx application
//     TypeId tid = ptt == TCP? TcpSocketFactory::GetTypeId():UdpSocketFactory::GetTypeId();
//     Ptr<Socket> sockets = Socket::CreateSocket(n1.Get(0), tid);
//     Address sinkAddr(InetSocketAddress(ifc.GetAddress(1), port));
//     Ptr<MyApp> app = CreateObject<MyApp> () ;
//     app->SetTagValue(1);
//     app->Setup(sockets, sinkAddr, 1000, DataRate(txRate));
//     n1.Get(0)->AddApplication(app);
//     app->SetStartTime(Seconds(0));
//     app->SetStopTime(Seconds(tStop));

//     // test process here
//     MiddlePoliceBox mbox1(vector<uint32_t>{1,1,1,1}, tStop, ptt, 0.9);
//     mbox1.install(dev.Get(0));
//     cout << " Mbox Intalled. " << endl << endl;

//     // trace: need test, example online is: MakeCallback(&function, objptr), objptr is object pointer
//     dev.Get(0)->TraceConnectWithoutContext("MacTx", MakeCallback(&MiddlePoliceBox::onMacTx, &mbox1));
//     dev.Get(1)->TraceConnectWithoutContext("MacRx", MakeCallback(&MiddlePoliceBox::onPktRx, &mbox1));
//     qc.Get(0)->TraceConnectWithoutContext("Drop", MakeCallback(&MiddlePoliceBox::onQueueDrop, &mbox1));

//     // begin flow control
//     mbox1.flowControl(PERSENDER, interval, logInterval);

//     // routing
//     Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

//     // stop & run
//     cout << " Running ... " << endl << endl;
//     Simulator::Stop(Seconds(tStop));
//     Simulator::Run();

//     cout << " Destroying ... " << endl << endl;
//     Simulator::Destroy();

//     return 0;
// }