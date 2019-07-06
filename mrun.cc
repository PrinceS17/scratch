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

#include <type_traits>
#include "ns3/mrun.h"

using namespace std;
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RunningModule");

typedef multimap<uint32_t, uint32_t>::iterator mmap_iter;
double controlInterval = 0.02;
double rateUpInterval = 0.002;

/* ------------- Begin: implementation of CapabilityHelper ------------- */
CapabilityHelper::CapabilityHelper(uint32_t flow_id, Ptr<Node> node, Ptr<NetDevice> device, Address addr)
{
    install(flow_id, node, device, addr);

    // this->flow_id = flow_id;
    // curAckNo = -1;
    // Ptr<PointToPointNetDevice> p2pDev = DynamicCast<PointToPointNetDevice> (device);
    // this->device = p2pDev;

    // // set socket
    // TypeId tpid = UdpSocketFactory::GetTypeId();
    // Ptr<Socket> skt = Socket::CreateSocket(node, tpid);

    // // set application
    // ackApp = CreateObject<MyApp> ();
    // ackApp->isTrackPkt = false;            // seems doesn't matter
    // ackApp->SetTagValue(flow_id + 1);      // begin from 1
    // ackApp->Setup(skt, addr, 50, DataRate('1Mbps'));   // pkt size and rate aren't necessary
    // node->AddApplication(ackApp);

}

vector<int> CapabilityHelper::GetNumFromTag(Ptr<const Packet> p)
{
    Ptr<const Packet> pcp = p->Copy();
    MyTag tag;
    if (!pcp->PeekPacketTag(tag)) return {-1, -1};
    MyApp temp;
    uint32_t tagScale = temp.tagScale;
    int index = tag.GetSimpleValue() / tagScale - 1;        // tag should include cnt, even not in debug mode
    int cnt = tag.GetSimpleValue() % tagScale;
    return {index, cnt};
}

void CapabilityHelper::SendAck(Ptr<const Packet> p)
{
    vector<int> vec = GetNumFromTag(p);
    if(vec[1] < 0) 
    { 
        // cout << "   - CapabilityHelper:: ACK not sent! " << vec[0] << ". " << vec[1] << endl; 
        return;
    }
    uint32_t ackNo = vec[1];
    curAckNo = ackNo;
    ackApp->SendAck(ackNo);
    // cout << "   - CapabilityHelper:: ACK " << ackNo << " of flow " << flow_id << " sent!" << endl;
}

void CapabilityHelper::install(uint32_t flow_id, Ptr<Node> node, Ptr<NetDevice> device, Address addr)
{
    this->flow_id = flow_id;
    curAckNo = 9999;
    Ptr<PointToPointNetDevice> p2pDev = DynamicCast<PointToPointNetDevice> (device);
    this->device = p2pDev;

    // only for debug
    cout << "   - flow-id: " << this->flow_id << ", cur ack: " << curAckNo << ", ptr: " << this << endl;

    // set socket
    TypeId tpid = UdpSocketFactory::GetTypeId();        // currently only for UDP
    Ptr<Socket> skt = Socket::CreateSocket(node, tpid);

    // set application
    ackApp = CreateObject<MyApp> ();
    ackApp->isTrackPkt = true;            // seems doesn't matter
    ackApp->SetTagValue(flow_id + 1);      // begin from 1
    ackApp->Setup(skt, addr, 15, DataRate('1Mbps'));   // pkt size and rate aren't necessary
    // node->AddApplication(ackApp);
    ackApp->StartAck();

    // set tracing
    // this->device->TraceConnectWithoutContext("MacRx", MakeCallback(&CapabilityHelper::SendAck, this));

    cout << "   - CapabilityHelper is installed on RX node " << flow_id << ": tag scale = " << ackApp->tagScale << endl;
}

uint32_t CapabilityHelper::getFlowId()
{
    return flow_id;
}

vector<uint32_t> CapabilityHelper::getCurAck()
{
    return {flow_id, curAckNo};
}

/* ------------- Begin: implementation of RunningModule ------------- */

void RunningModule::SetNode(Ptr<Node> pn, uint32_t i, uint32_t id)
{
    Group g = groups.at(i);
    vector<uint32_t>::iterator j = find(g.txId.begin(), g.txId.end(), id);
    if( j != g.txId.end()) 
    {
        sender.Get(i*u + (uint32_t)(j - g.txId.begin())) = pn;
        pn = nullptr;
        return;
    }
    j = find(g.rxId.begin(), g.rxId.end(), id);
    if( j != g.rxId.end()) 
    {
        receiver.Get(i*u + (uint32_t)(j - g.rxId.begin())) = pn;
        pn = nullptr;
        return;
    }
    j = find(g.routerId.begin(), g.routerId.end(), id);     // should be 0/1
    if(j != g.routerId.end()) 
    {
        router.Get(i * 2 + (uint32_t)(j - g.routerId.begin())) = pn;
        pn = nullptr;
    }
    else NS_LOG_INFO("Fail to set the node!");
}

void RunningModule::SetNode(Ptr<Node> pn, uint32_t type, uint32_t i, uint32_t n)
{
    Group g = groups.at(i);
    switch(type)
    {
    case 0:
        sender.Get(i*u + n) = pn;
        break;
    case 1:
        receiver.Get(i*u + n) = pn;
        break;
    case 2:
        router.Get(i*2 + n) = pn;
        break;
    default: ;
    }
    pn = nullptr;
}

Ptr<Node> RunningModule::GetNode(uint32_t i, uint32_t id)
{
    // new: directly visit dumbbell node
    Group g = groups.at(i);
    vector<uint32_t>::iterator j = find(g.txId.begin(), g.txId.end(), id);
    if( j != g.txId.end()) 
        return dv.at(i).GetLeft((uint32_t)(j - g.txId.begin()));
    j = find(g.rxId.begin(), g.rxId.end(), id);
    if( j != g.rxId.end()) 
        return dv.at(i).GetRight((uint32_t)(j - g.rxId.begin()));
    j = find(g.routerId.begin(), g.routerId.end(), id);     // should be 0/1
    return j == g.routerId.begin()? dv.at(i).GetLeft():dv.at(i).GetRight();

    // // original: fetch from node container
    // Group g = groups.at(i);
    // vector<uint32_t>::iterator j = find(g.txId.begin(), g.txId.end(), id);
    // if( j != g.txId.end()) 
    //     return sender.Get(i*u + (uint32_t)(j - g.txId.begin()));
    // j = find(g.rxId.begin(), g.rxId.end(), id);
    // if( j != g.rxId.end()) 
    //     return receiver.Get(i*u + (uint32_t)(j - g.rxId.begin()));
    // j = find(g.routerId.begin(), g.routerId.end(), id);     // should be 0/1
    // return router.Get(i * 2 + (uint32_t)(j - g.routerId.begin()));
}

Ptr<Node> RunningModule::GetRouter(uint32_t i, bool ifTx)
{
    return ifTx? dv.at(i).GetLeft() : dv.at(i).GetRight();
}

Ipv4Address RunningModule::GetIpv4Addr(uint32_t i, uint32_t id)     // counting index easily goes wrong: need testing 
{
    Group g = groups.at(i);
    NS_LOG_FUNCTION("id: " + to_string(id));   
    
    vector<uint32_t>::iterator j = find(g.txId.begin(), g.txId.end(), id);
    if(j != g.txId.end()) 
        return ifc.GetAddress(i*u + (uint32_t)(j - g.txId.begin()));
    j = find(g.rxId.begin(), g.rxId.end(), id);
    
    if( j != g.rxId.end()) 
    {   
        return ifc.GetAddress(sender.GetN() + i*u + (uint32_t)(j - g.rxId.begin()));
    }
    j = find(g.routerId.begin(), g.routerId.end(), id);
    return ifc.GetAddress(sender.GetN() + receiver.GetN() + i*2 + (uint32_t)(j - g.routerId.begin()));
}

uint32_t RunningModule::GetId()
{   return ID; }

RunningModule::RunningModule(vector<double> t, vector<Group> grp, ProtocolType pt, vector<string> bnBw, vector<string> bnDelay, string delay, vector<bool> fls, uint32_t size)
{
    // constant setting
    ID = rand() % 10000;
    nSender = 0;
    nReceiver = 0;
    groups = grp;
    for(Group g:groups)
    {
        nSender += g.txId.size();
        nReceiver += g.rxId.size();
    }
    pktSize = size;
    rtStart = t.at(0);
    rtStop = t.at(1);

    for(uint32_t i = 0; i < nSender; i ++)
    {
        txStart.push_back(t.at(i + 2));
        txEnd.push_back(t.at(i + nSender + 2));
    }
    for (auto e:txStart) cout << e << ' ';
    cout << "tx Start pushed!" << endl;

    for (auto e:txEnd) cout << e << ' ';
    cout << "tx End pushed!" << endl;

    protocol = pt;
    bottleneckBw = bnBw;
    bottleneckDelay = bnDelay;
    this->delay = delay;
    isTrackPkt = fls.at(0);
    bypassMacRx = fls.at(1);
}

RunningModule::~RunningModule(){}

void RunningModule::buildTopology(vector<Group> grp)
{

    // set single p2p leaf link (drop tail by default), use dumbbell structure for now
    PointToPointHelper leaf;
    leaf.SetDeviceAttribute("DataRate", StringValue(normalBw));
    leaf.SetChannelAttribute("Delay", StringValue(delay));
    
    // set bottleneck link and the dumbbell
    NS_LOG_FUNCTION("Setting p2p links...");
    // vector<PointToPointDumbbellHelper> dv;
    uint32_t umax = 0;
    for(uint32_t i = 0; i < grp.size(); i ++)
    {
        PointToPointHelper bottleneck;
        NS_LOG_FUNCTION("setting data rate");
        bottleneck.SetDeviceAttribute("DataRate", StringValue(bottleneckBw.at(i)));
        NS_LOG_FUNCTION("setting delay");
        bottleneck.SetChannelAttribute("Delay", StringValue(bottleneckDelay.at(i)));
        NS_LOG_FUNCTION("setting mtu");        
        bottleneck.SetDeviceAttribute("Mtu", StringValue(mtu));
        
        PointToPointDumbbellHelper d(grp[i].txId.size(), leaf, grp[i].rxId.size(), leaf, bottleneck);
        dv.push_back(d);

        umax = max(umax, grp[i].N);
    }
    NS_LOG_FUNCTION("Creating nodes...");
    u = (umax - 2)/ 2;
    NS_LOG_FUNCTION("unit length: " + to_string(u));
    sender.Create(u * grp.size());
    receiver.Create(u * grp.size());
    router.Create(2 * grp.size());

    // install network stack
    InternetStackHelper stack;
    for(uint32_t i = 0; i < dv.size(); i ++)
        dv.at(i).InstallStack(stack);
     
    // set node 
    NS_LOG_FUNCTION("Setting nodes...");
    for(uint32_t i = 0; i < grp.size(); i ++)
    {
        SetNode(dv.at(i).GetLeft(), 2, i, 0);
        SetNode(dv.at(i).GetRight(), 2, i, 1);
        for(uint32_t j = 0; j < grp.at(i).txId.size(); j ++)
            SetNode(dv.at(i).GetLeft(j), 0, i, j);
        for(uint32_t j = 0; j < grp.at(i).rxId.size(); j ++)
            SetNode(dv.at(i).GetRight(j), 1, i, j);
        
    }

    // check network stack
    NS_LOG_FUNCTION("Tesing stack...");
    stringstream ss1;
    Ptr<Node> nod = GetNode(0, grp[0].txId[0]);
    Ptr<Node> nod2 = GetNode(0, grp[0].txId[1]);
    ss1 << "# Dev: node 0: " << nod->GetNDevices() << "; node 1: " << nod2->GetNDevices() << endl;
    for(uint32_t k = 0; k < nod->GetNDevices(); k ++)    ss1 << nod->GetDevice(k)->GetAddress() << " ; ";
    ss1 << endl;
    for(uint32_t k = 0; k < nod2->GetNDevices(); k ++)    ss1 << nod2->GetDevice(k)->GetAddress() << " ; ";
    NS_LOG_FUNCTION(ss1.str());

}

void RunningModule::configure(double stopTime, ProtocolType pt, vector<string> bw, vector<string> delay, vector<MiddlePoliceBox>& mboxes, vector<double> Th)
{
    NS_LOG_FUNCTION("Begin.");
    NS_LOG_INFO("Starting running module ... ");
    protocol = pt;
    bottleneckBw = bw;
    bottleneckDelay = delay;
    qc = setQueue(groups, bottleneckBw, bottleneckDelay, Th);

    ifc = setAddress();
    sinkApp = setSink(groups, protocol);

    if(pt == UDP)
    {
        setCapabilityHelper(groups);     // need testing!
        // for debug only
        for(uint32_t i = 0; i < chelpers.size(); i ++)
        {
            cout << "    -- After installation: flow " << chelpers[i].getFlowId() << " has ptr " << &chelpers.at(i) << endl;
        }
    }

    senderApp = setSender(groups, protocol);
    this->mboxes = mboxes;
    connectMbox(groups, controlInterval, 0.2, rateUpInterval);        // not work either
    // connectMbox(groups, controlInterval, 1, rateUpInterval);
    start();
}

QueueDiscContainer RunningModule::setQueue(vector<Group> grp, vector<string> bnBw, vector<string> bnDelay, vector<double> Th)
{
    NS_LOG_FUNCTION("Begin.");

    // set RED queue
    QueueDiscContainer qc;
    for(uint32_t i = 0; i < grp.size(); i ++)
    {
        TrafficControlHelper tch;
        if(Th.empty()) tch.SetRootQueueDisc("ns3::RedQueueDisc", 
                                            "MinTh", DoubleValue(5),
                                            "MaxTh", DoubleValue(15),
                                            "LinkBandwidth", StringValue(bnBw.at(i)),
                                            "LinkDelay", StringValue(bnDelay.at(i)));
        else tch.SetRootQueueDisc("ns3::RedQueueDisc",
                                "MinTh", DoubleValue(Th.at(0)),
                                "MaxTh", DoubleValue(Th.at(1)),
                                "LinkBandwidth", StringValue(bnBw.at(i)),
                                "LinkDelay", StringValue(bnDelay.at(i)));

        // qc.Add(tch.Install(GetNode(i, grp.at(i).routerId[0])->GetDevice(0)));
        Ptr<NetDevice> txRouter = GetRouter(i, true)->GetDevice(0);
        Ptr<NetDevice> rxRouter = GetRouter(i, false)->GetDevice(0);
        qc.Add(tch.Install(txRouter));
        // tch.Install(rxRouter);
        cout << "type id of queue: " << qc.Get(0)->GetInstanceTypeId() << endl;

        // qc.Add(tch.Install(GetNode(i, grp.at(i).routerId[1])->GetDevice(0)));
        // qc.Add(tch.Install(router.Get(2*i)->GetDevice(0)));       // sender's router queue needed only
    }

    return qc;
}

Ipv4InterfaceContainer RunningModule::setAddress()
{
    // assign Ipv4 addresses
    Ipv4AddressHelper ih1("10.1.0.0", "255.255.255.0");
    
    for(uint32_t i = 0; i < groups.size(); i ++)
    {
        Group g = groups.at(i);
        vector<string> addr(3, "");
        for(uint32_t j = 0; j < 3; j ++)
            addr[j] = to_string(i + 1) + "." + to_string(j + 1) + ".0.0";
        dv.at(i).AssignIpv4Addresses(Ipv4AddressHelper (addr[0].c_str(), "255.255.255.0"),
                          Ipv4AddressHelper (addr[1].c_str(), "255.255.255.0"),
                          Ipv4AddressHelper (addr[2].c_str(), "255.255.255.0"));
    }

    // // use GetRouter and GetNode()
    // NetDeviceContainer ncTx, ncRx, ncRt;
    // for(uint32_t i = 0; i < groups.size(); i ++)
    // {
    //     Group g = groups.at(i);
    //     for(uint32_t j = 0; j < g.txId.size(); j ++)
    //         for(uint32_t k = 0; k < GetNode(i, g.txId.at(j))->GetNDevices(); k ++)
    //             ncTx.Add(GetNode(i, g.txId.at(j))->GetDevice(k));
    //     for(uint32_t j = 0; j < g.rxId.size(); j ++)
    //         for(uint32_t k = 0; k < GetNode(i, g.rxId.at(j))->GetNDevices(); k ++)
    //             ncTx.Add(GetNode(i, g.rxId.at(j))->GetDevice(k));    
    //     for(uint32_t j = 0; j < g.routerId.size(); j ++)
    //         for(uint32_t k = 0; k < GetNode(i, g.routerId.at(j))->GetNDevices(); k ++)
    //             ncTx.Add(GetNode(i, g.routerId.at(j))->GetDevice(k));
    // }  

    // // collect interfaces
    // Ipv4InterfaceContainer ifc;
    // ifc.Add(ih1.Assign(ncTx));
    // ifc.Add(ih1.Assign(ncRx));
    // ifc.Add(ih1.Assign(ncRt));
    // actually we can also preserve these net device in header file also

    Ipv4InterfaceContainer ifc;
    return ifc;
}

ApplicationContainer RunningModule::setSink(vector<Group> grp, ProtocolType pt)
{
    NS_LOG_FUNCTION("Begin.");
    string ptStr = pt == TCP? "ns3::TcpSocketFactory":"ns3::UdpSocketFactory";
    for(uint32_t i = 0; i < grp.size(); i ++)
    {
        Group g = grp.at(i);
        for(uint32_t j = 0; j < g.txId.size(); j ++)
        {
            uint32_t tid = g.txId.at(j);
            uint32_t port = g.rate2port[ g.tx2rate[tid] ];
            Address sinkLocalAddr (InetSocketAddress(Ipv4Address::GetAny(), port));
            PacketSinkHelper psk(ptStr, sinkLocalAddr);

            // find RX corresponding to the txId: equal_range, need testing
            pair<mmap_iter, mmap_iter> res = g.tx2rx.equal_range(tid);
            stringstream ss;
            ss << "TX ID: " << tid << ";    RX ID: ";
            for(mmap_iter it = res.first; it != res.second; it ++)
            {
                sinkApp.Add(psk.Install(GetNode(i, it->second)));   // it->second: rx ids
                ss << it->second << " ";
            }
            NS_LOG_INFO(ss.str());
        }
    }
    return sinkApp;
}

vector< CapabilityHelper > RunningModule::setCapabilityHelper(vector<Group> grp)
{
    NS_LOG_FUNCTION("Begin.");
    int idx = 0;
    for (uint32_t i = 0; i < grp.size(); i ++)
    {
        Group g = grp.at(i);
        uint32_t n = g.rxId.size();
        
        for (uint32_t j = 0; j < g.txId.size(); j ++)
        {
            uint32_t tId = g.txId[j];
            // uint32_t rx_id = g.tx2rx.equal_range(tId);
            uint32_t rx_id = g.rxId[j];                 // short cut, not flexible for the case that TX are not equal to RX!!!
            uint32_t ri = find(g.rxId.begin(), g.rxId.end(), rx_id) - g.rxId.begin();
            string rate = g.tx2rate.at(tId);
            uint32_t port = g.rate2port.at(rate);
            Address sourceAddr(InetSocketAddress(dv.at(i).GetLeftIpv4Address(j), port));

            Ptr<Node> rx_node = GetNode(i, rx_id);
            Ptr<NetDevice> rx_device = rx_node->GetDevice(0);
            
            CapabilityHelper tmp;
            chelpers.push_back(tmp);
            chelpers.at(idx).install(ri, rx_node, rx_device, sourceAddr);
            
            idx ++;
        }
    }

    // set tracing in another part to avoid coupling
    idx = 0;
    for (uint32_t i = 0; i < grp.size(); i ++)
    {
        Group g = grp.at(i);
        for (uint32_t j = 0; j < g.txId.size(); j ++)
        {
            uint32_t rId = g.rxId[j];
            uint32_t ri = j;
            Ptr<Node> rx_node = GetNode(i, rId);
            Ptr<NetDevice> rx_device = rx_node->GetDevice(0);
            rx_device->TraceConnectWithoutContext("MacRx", MakeCallback(&CapabilityHelper::SendAck, &chelpers.at(idx)));
            cout << "   - f " << j << " 's addr in set chelper: " << &chelpers.at(idx) << endl;
            idx ++;
        }
    }
    return chelpers;
}

vector< Ptr<MyApp> > RunningModule::setSender(vector<Group> grp, ProtocolType pt)
{
    NS_LOG_FUNCTION("Begin.");
    vector<Ptr<MyApp>> appc;
    stringstream ss;
    for(uint32_t i = 0; i < grp.size(); i ++)
    {
        ss << "Set sender " << i << " ... " << endl;

        Group g = grp.at(i);
        for(auto tId:g.txId)
        {
            pair<mmap_iter, mmap_iter> res = g.tx2rx.equal_range(tId);
            for(mmap_iter it = res.first; it != res.second; it ++)
            {
                vector<uint32_t>::iterator it2 = find(g.txId.begin(), g.txId.end(), tId);
                // uint32_t tag = i*u + (uint32_t)(it2 - g.txId.begin()) + 1;      // tag: index in sender
                uint32_t tag = (uint32_t)(it2 - g.txId.begin()) + 1;
                cout << "RX id: " << it->second << "; Tag : " << tag << "; " << tId << " -> " << it->second << endl;
                appc.push_back(netFlow(i, tId, it->second, tag));
            }
        }
    }
    NS_LOG_INFO(ss.str());
    return appc;
}

Ptr<MyApp> RunningModule::netFlow(uint32_t i, uint32_t tId, uint32_t rId, uint32_t tag)
{
    // parse rate and port
    Group g = groups.at(i);
    string rate = g.tx2rate.at(tId);
    uint32_t port = g.rate2port.at(rate);
    uint32_t ri = find(g.rxId.begin(), g.rxId.end(), rId) - g.rxId.begin();
    uint32_t ti = find(g.txId.begin(), g.txId.end(), tId) - g.txId.begin();

    // set socket
    TypeId tpid = protocol == TCP? TcpSocketFactory::GetTypeId():UdpSocketFactory::GetTypeId();
    Ptr<Socket> skt = Socket::CreateSocket(GetNode(i, tId), tpid); 
    if(protocol == TCP) skt->SetAttribute ("Sack", BooleanValue (false));
    // Address sinkAddr(InetSocketAddress(GetIpv4Addr(i, rId), port));
    Address sinkAddr(InetSocketAddress(dv.at(i).GetRightIpv4Address(ri), port));
    
    Ptr<MyApp> app = CreateObject<MyApp> ();
    app->isTrackPkt = isTrackPkt;
    app->SetTagValue(tag);
    app->Setup(skt, sinkAddr, pktSize, DataRate(rate));
    GetNode(i, tId)->AddApplication(app);

    // logging
    stringstream ss;
    ss << "Group " << i << ": " << tId << "->" << rId << " ; port: " << port;
    NS_LOG_INFO(ss.str());
    return app;
}

void RunningModule::connectMbox(vector<Group> grp, double interval, double logInterval, double ruInterval)
{
    NS_LOG_FUNCTION("Connect Mbox ... ");
    for(uint32_t i = 0; i < grp.size(); i ++)
    {
        // MiddlePoliceBox& mb = mboxes.at(i);         // not sure
        Ptr<NetDevice> txRouter = GetRouter(i, true)->GetDevice(0);
        Ptr<NetDevice> rxRouter = GetRouter(i, false)->GetDevice(0);
        Ptr<Node> txNode = GetRouter(i, true);        
        // Ptr<NetDevice> txRouter = router.Get(2*i)->GetDevice(0);
        // Ptr<NetDevice> rxRouter = router.Get(2*i + 1)->GetDevice(0);
        NetDeviceContainer nc;
        for(uint32_t j = 1; j < txNode->GetNDevices(); j ++)
            nc.Add(txNode->GetDevice(j));               // add rx side devices

        // install mbox
        if(bypassMacRx) mboxes.at(i).install(txRouter);     // install only the router net dev for bottleneck link
        else mboxes.at(i).install(nc);                      // install probes for all tx router's mac rx side

        NS_LOG_FUNCTION("Mbox installed on router " + to_string(i));
        
        // set weight, rtt, rto & start mbox
        vector<double> rtts;
        for(uint32_t j = 0; j < grp.at(i).txId.size(); j ++)
        {
            double bnDelay = (bottleneckDelay.at(i).c_str()[0] - '0') / 1000.0;
            double dely = (delay.c_str()[0] - '0') / 1000.0;
            NS_LOG_INFO("rtt[" + to_string(j) + "]: " + to_string(2 *(bnDelay + 2 * dely)));
            rtts.push_back(2 * (bnDelay + 2 * dely) );
        }
        mboxes.at(i).SetWeight(grp.at(i).weight);
        mboxes.at(i).SetRttRto(rtts);
        mboxes.at(i).start();
        
        // tracing
        if(bypassMacRx)         // by pass the wierd bug of rwnd update: function of MacTx moved to onMacRx
            txRouter->TraceConnectWithoutContext("MacTx", MakeCallback(&MiddlePoliceBox::onMacRx, &mboxes.at(i))); 
        else
        {
            // for(uint32_t j = 0; j < groups.at(i).txId.size(); j ++)         // I think index error is the main cause of mac rx error!!!!
            for(uint32_t j = 1; j < txNode->GetNDevices(); j ++)
                bool res = txNode->GetDevice(j)->TraceConnectWithoutContext("MacRx", MakeCallback(&MiddlePoliceBox::onMacRx, &mboxes.at(i)));
            txRouter->TraceConnectWithoutContext("MacTx", MakeCallback(&MiddlePoliceBox::onMacTx, &mboxes.at(i)));      // necessary for TCP loss detection
        }

        rxRouter->TraceConnectWithoutContext("MacRx", MakeCallback(&MiddlePoliceBox::onPktRx, &mboxes.at(i)));
        qc.Get(i)->TraceConnectWithoutContext("Drop", MakeCallback(&MiddlePoliceBox::onQueueDrop, &mboxes.at(i)));
        txRouter->TraceConnectWithoutContext("MacRx", MakeCallback(&MiddlePoliceBox::onMboxAck, &mboxes.at(i)));    // test passeds
    
        // debug queue size
        qc.Get(i)->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&MiddlePoliceBox::TcPktInQ, &mboxes.at(i)));
        Ptr<Queue<Packet>> qp = DynamicCast<PointToPointNetDevice> (txRouter) -> GetQueue();
        qp->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&MiddlePoliceBox::DevPktInQ, &mboxes.at(i)));

        // trace TCP congestion window and RTT: tested
        for(uint32_t j = 0; j < groups.at(i).txId.size(); j ++)
        {
            string context1 = "/NodeList/0/$ns3::TcpL4Protocol/SocketList/" + to_string(j) + "/CongestionWindow";
            string context2 = "/NodeList/0/$ns3::TcpL4Protocol/SocketList/" + to_string(j) + "/RTT";

            Ptr<Socket> skt = senderApp.at(i * groups[0].N + j)->GetSocket();       // maybe limited for different groups
            skt->TraceConnect("CongestionWindow", context1, MakeCallback(&MiddlePoliceBox::onCwndChange, &mboxes.at(i)));
            skt->TraceConnect("RTT", context2, MakeCallback(&MiddlePoliceBox::onRttChange, &mboxes.at(i)));
        }

        // test the rx side device index
        Ptr<Node> rtNode = GetRouter(i, false);
        cout << " rx0: # dev: " << rtNode->GetNDevices() << "; # rx: " << grp.at(i).rxId.size() << endl;
        for(uint32_t k = 0; k < rtNode->GetNDevices(); k ++)
        {
            cout << " -- rx[" << k << "] addr: " << rtNode->GetDevice(k)->GetAddress() << endl;
            cout << " -- rx end[" << k << "] addr: " << GetNode(i, grp.at(i).rxId[k])->GetDevice(0)->GetAddress() << endl;
        }            

        // for debug chain 1
        for(uint32_t k = 0; k < grp.at(i).txId.size(); k ++)
        {
            Ptr<NetDevice> senderNode = dv.at(i).GetLeft(k)->GetDevice(0);
            senderNode->TraceConnectWithoutContext("MacTx", MakeCallback(&MiddlePoliceBox::onSenderTx, &mboxes.at(i)));
            senderNode->TraceConnectWithoutContext("MacRx", MakeCallback(&MiddlePoliceBox::onAckRx, &mboxes.at(i)));
        }


        // flow control
        mboxes.at(i).flowControl(mboxes.at(i).GetFairness(), interval, logInterval, ruInterval);
    }
}

void RunningModule::disconnectMbox(vector<Group> grp)
{
    NS_LOG_INFO("Disconnect Mbox ... ");

    for(uint32_t i = 0; i < grp.size(); i ++)
    {
        Ptr<NetDevice> txRouter = GetRouter(i, true)->GetDevice(0);
        Ptr<NetDevice> rxRouter = GetRouter(i, false)->GetDevice(0);
        Ptr<Node> txNode = GetRouter(i, true);
        
        // stop the mbox and tracing
        mboxes.at(i).stop();     // stop flow control and logging 
        
        for(uint32_t j = 0; j < txNode->GetNDevices(); j ++)
            txNode->GetDevice(j)->TraceDisconnectWithoutContext("MacRx", MakeCallback(&MiddlePoliceBox::onMacRx, &mboxes.at(i)));
        if(!rxRouter->TraceDisconnectWithoutContext("MacRx", MakeCallback(&MiddlePoliceBox::onPktRx, &mboxes[i])))
            NS_LOG_INFO("Failed to disconnect onPktRx!");
        // if(!qc.Get(i)->TraceDisconnectWithoutContext("Drop", MakeCallback(&MiddlePoliceBox::onQueueDrop, &mboxes.at(i))))
        //     NS_LOG_INFO("Failed to disconnect onQueueDrop!");        

        NS_LOG_FUNCTION("Mbox " + to_string(i) + " stops.");
    }
}    

void RunningModule::pauseMbox(vector<Group> grp)
{
    NS_LOG_INFO("Pause Mbox: stop early drop ... ");
    for(uint32_t i = 0; i < grp.size(); i ++)
    {
        Ptr<Node> txNode = GetRouter(i, true);
        Ptr<NetDevice> txRouter = txNode->GetDevice(0);
        txRouter->TraceDisconnectWithoutContext("MacTx", MakeCallback(&MiddlePoliceBox::onMacRx, &mboxes.at(i)));
        txRouter->TraceConnectWithoutContext("MacTx", MakeCallback(&MiddlePoliceBox::onMacRxWoDrop, &mboxes.at(i)));
        // for(uint32_t j = 0; j < txNode->GetNDevices(); j ++)
        // {
        //     txNode->GetDevice(j)->TraceDisconnectWithoutContext("MacRx", MakeCallback(&MiddlePoliceBox::onMacRx, &mboxes.at(i)));
        //     txNode->GetDevice(j)->TraceConnectWithoutContext("MacRx", MakeCallback(&MiddlePoliceBox::onMacRxWoDrop, &mboxes.at(i)));
        // }
        
    }
}

void RunningModule::resumeMbox(vector<Group> grp)
{
    NS_LOG_INFO("Resume Mbox: restart early drop ... ");    
    for(uint32_t i = 0; i < grp.size(); i ++)
    {
        Ptr<Node> txNode = GetRouter(i, true);
        // txRouter->TraceDisconnectWithoutContext("MacTx", MakeCallback(&MiddlePoliceBox::onMacTx, &mb));
        // txRouter->TraceConnectWithoutContext("MacTx", MakeCallback(&MiddlePoliceBox::onMacRxWoDrop, &mb));
        for(uint32_t j = 0; j < txNode->GetNDevices(); j ++)
        {
            txNode->GetDevice(j)->TraceDisconnectWithoutContext("MacRx", MakeCallback(&MiddlePoliceBox::onMacRxWoDrop, &mboxes.at(i)));
            txNode->GetDevice(j)->TraceConnectWithoutContext("MacRx", MakeCallback(&MiddlePoliceBox::onMacRx, &mboxes.at(i)));
        }
    }
}

void RunningModule::start()
{
    NS_LOG_FUNCTION("Start.");   
    sinkApp.Start(Seconds(rtStart));
    sinkApp.Stop(Seconds(rtStop));
    for(uint32_t j = 0; j < senderApp.size(); j ++)
    {
        // senderApp.at(j)->SetStartTime(Seconds(rtStart));
        // senderApp.at(j)->SetStopTime(Seconds(rtStop));
        senderApp.at(j)->SetStartTime(Seconds(txStart.at(j)));
        cout << j << ": start at " << txStart.at(j) << endl;
        senderApp.at(j)->SetStopTime(Seconds(txEnd.at(j)));
    }
}

void RunningModule::stop()
{
    NS_LOG_FUNCTION("Stop.");
    for(uint32_t j = 0; j < senderApp.size(); j ++)
        senderApp.at(j)->SetStopTime(Seconds(0.0));     // stop now
    sinkApp.Stop(Seconds(0.0));
}   

int main (int argc, char *argv[])
{

    srand(time(0));
    Packet::EnablePrinting ();          // enable printing the metadata of packet
    Packet::EnableChecking ();

    // define the test options and parameteres
    ProtocolType pt = UDP;
    int TCP_var = 1;
    FairType fairness = PRIORITY;
    bool isTrackPkt = false;
    bool isEbrc = false;            // don't use EBRC now, but also want to use loss assignment
    bool isTax = true;              // true: scheme 1, tax; false: scheme 2, counter
    bool isBypass = false;
    uint32_t nTx = 3;               // sender number, i.e. link number
    uint32_t nGrp = 1;              // group number
    vector<double> Th;              // threshold of slr/llr
    double slrTh = 0.01;
    double llrTh = 0.01;
    double tStop = 300;
    uint32_t MID1 = 0;
    uint32_t MID2 = 0;
    double alpha;
    uint32_t maxPkts = 1;
    double scale = 5e3;

    // specify the TCP socket type in ns-3
    if(TCP_var == 1) Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));  
    else if(TCP_var == 2) Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpVeno"));
    else if(TCP_var == 3) Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpScalable"));  
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1400));   
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4096 * 1024));      // 128 (KB) by default, allow at most 85Mbps for 12ms rtt
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(4096 * 1024));      // here we use 4096 KB
    // Config::SetDefault ("ns3::RedQueueDisc::MaxSize",
                        //   QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, 25)));  // set the unit to packet but not byte
    Config::SetDefault ("ns3::RedQueueDisc::MaxSize", StringValue ("25p"));
    Config::SetDefault ("ns3::RedQueueDisc::LInterm", DoubleValue (10));              // default 50 -> prob = 0.02, low
    Config::SetDefault ("ns3::QueueBase::MaxSize", StringValue (std::to_string (maxPkts) + "p"));

    double pSize = 1.4 * 8;         // ip pkt size: 1.4 kbit
    vector<double> wt(20, -1.0);

    // command line parameters: focus on slr and llr threshold first, should display in figure name
    CommandLine cmd;
    cmd.AddValue ("tStop", "Stop time of the simulation", tStop);
    cmd.AddValue ("slrTh", "SLR threshold", slrTh);
    cmd.AddValue ("llrTh", "LLR threshold", llrTh);
    cmd.AddValue ("mid1", "Mbox 1 ID", MID1);
    cmd.AddValue ("mid2", "Mbox 2 ID", MID2);
    cmd.AddValue ("isTrackPkt", "whether track each packet", isTrackPkt);       // input 0/1
    cmd.AddValue ("cInt", "Control interval of mbox", controlInterval);
    cmd.AddValue ("rInt", "Rate update interval", rateUpInterval);
    cmd.AddValue ("weight1", "weight[1] for 2 weight run", wt[0]);
    cmd.AddValue ("weight2", "weight[2] for 2 weight run", wt[1]);

    cmd.Parse (argc, argv);

    Th = {slrTh, llrTh};
    // t[1] = tStop;
    cout << "Stop time: " << tStop << endl;
    cout << "SLR threshold: " << slrTh << endl;
    cout << "LLR threshold: " << llrTh << endl;
    cout << "MID 1: " << MID1 << endl;
    cout << "MID 2: " << MID2 << endl;
    cout << "Control interval: " << controlInterval << endl;
    cout << "Rate update interval: " << rateUpInterval << endl;
    cout << "Weight: " << wt[0] << " " << wt[1] << endl;

    int i = 0;
    while (wt[i] >= 0) i++;
    if (i > 0) nTx = i;

    // define bottleneck link bandwidth and delay, protocol, fairness
    vector<string> bnBw, bnDelay;
    if(nGrp == 1)
    {
        bnBw = {"120Mbps"};
        // bnBw = {"20Mbps"};
        // bnBw = {"2Mbps"};     // low rate test for queue drop and mac tx
        bnDelay = {"2ms"};
        alpha = rateUpInterval / 0.1;     // over the cover period we want
    }
    else if(nGrp == 2)
    {
        bnBw = {"100Mbps", "100Mbps"};
        bnDelay = {"2ms", "2ms"};
        alpha = rateUpInterval / 0.1; 
    }

    // generating groups
    cout << "Generating groups of nodes ... " << endl;
    vector<double> weight;
    vector<uint32_t> txId1;
    vector<uint32_t> rtid, rtid2, rxId1, rxId2;
    map<uint32_t, string> tx2rate1, tx2rate2;
    map<string, uint32_t> rate2port1, rate2port2;
    Group g1, g2;
    vector<Group> grps;

    if(nTx == 2 && nGrp == 1) // group: 2*1, 1
    {
        rtid = {5, 6};
        tx2rate1 = {{1, "200Mbps"}, {2, "200Mbps"}};
        // tx2rate1 = {{1, "0.01Mbps"}, {2, "20Mbps"}};               // for TCP drop debug only!
        rxId1 = {7, 8};
        rate2port1 = {{"200Mbps", 80}, {"200Mbps", 90}};
        // rate2port1 = {{"0.01Mbps", 80}, {"20Mbps", 90}};           // for TCP drop debug only!
        // weight = {0.7, 0.3};
        weight = vector<double> (wt.begin(), wt.begin() + 2);
        g1 = Group(rtid, tx2rate1, rxId1, rate2port1, weight);      // skeptical
        g1.insertLink({1, 2}, {7, 8});
        grps = {g1};
    }
    else if(nTx == 4 && nGrp == 1) // group: 2*2, 1
    {
        rtid = {5, 6};
        tx2rate1 = {{1, "200Mbps"}, {2, "200Mbps"}, {3, "400Mbps"}, {4, "400Mbps"}};
        rxId1 = {7, 8, 9, 10};
        rate2port1 = {{"200Mbps", 80}, {"400Mbps", 90}};
        weight = {0.4, 0.3, 0.2, 0.1};
        g1 = Group(rtid, tx2rate1, rxId1, rate2port1, weight);
        g1.insertLink({1, 2, 3, 4}, {7, 8, 9, 10});
        grps = {g1};
    }
    else if(nTx == 3 && nGrp == 1) // group: 3, 1
    {
        rtid = {5, 6};
        tx2rate1 = {{1, "100Mbps"}, {2, "100Mbps"}, {3, "100Mbps"}};
        rxId1 = {7, 8, 9};
        rate2port1 = {{"100Mbps", 80}};
        // weight = {0.6, 0.2, 0.2};
        weight = {0.6, 0.3, 0.1};
        g1 = Group(rtid, tx2rate1, rxId1, rate2port1, weight);
        g1.insertLink({1, 2, 3}, {7, 8, 9});
        grps = {g1};
    }
    else if(nTx == 3 && nGrp == 2) // group: 3, 2
    {
        rtid = {25, 49};
        tx2rate1 = {{10, "200Mbps"}, {11, "400Mbps"}};
        rxId1 = {2,3};
        rate2port1 = {{"200Mbps", 80}, {"400Mbps", 90}};
        weight = {0.7, 0.3};
        g1 = Group(rtid, tx2rate1, rxId1, rate2port1, weight);
        g1.insertLink({10, 11}, {2, 3});

        rtid2 = {26, 50};
        tx2rate2 = {{10, "200Mbps"}, {12, "400Mbps"}};
        rxId2 = {2,4};
        rate2port2 = {{"200Mbps", 80}, {"400Mbps", 90}};
        g2 = Group(rtid2, tx2rate2, rxId2, rate2port2, weight);
        g2.insertLink({10, 12}, {2, 4});
        grps = {g1, g2};
    }
    else if(nTx == 10)
    {
        rtid = {95, 96};
        for(int i = 0; i < 10; i ++)
        {
            tx2rate1[i] = "100Mbps";
            txId1.push_back(i);
            rxId1.push_back(i + 10);
        }
        rate2port1 = {{"100Mbps", 80}};
        weight = {0.5, 0.055, 0.055, 0.055, 0.055, 0.056, 0.056, 0.056, 0.056, 0.056}; 
        g1 = Group(rtid, tx2rate1, rxId1, rate2port1, weight);
        g1.insertLink(txId1, rxId1);
        grps = {g1};
        scale = 2e3;
    }
    else if (nGrp == 1)
    {
        rtid = {95, 96};
        for(int i = 0; i < nTx; i ++)
        {
            tx2rate1[i] = "100Mbps";
            txId1.push_back(i);
            rxId1.push_back(i + nTx);
        }
        rate2port1 = {{"100Mbps", 80}};
        weight.push_back(0.5);
        for(int i = 0; i < nTx - 1; i ++)
            weight.push_back(0.5 / (nTx - 1) );
        g1 = Group(rtid, tx2rate1, rxId1, rate2port1, weight);
        g1.insertLink(txId1, rxId1);
        grps = {g1};
        scale = 2e3;
    }

    // set start and stop time
    cout << "Time assigned: " << endl;
    uint32_t N = grps[0].txId.size();       // just use 1st group, cannot extend to multiple groups
    vector<double> t(2 + N * 2);
    t[0] = 0.0;
    t[1] = tStop;
    t[2] = 0.0;             // flow 0 start later than other flows
    // for(uint32_t i = 3; i < 2*N + 2; i ++)      // Debug: 0, 10, 20s
    // {
    //     if(i < N + 2) t[i] = (i - 2) * 10;
    //     else t[i] = tStop;
    //     cout << i << ": " << t[i] << endl;
    // }

    // Experiment setting: start: 0, 50, 100; stop: 400, 450, 500 (s)
    // for (uint32_t i = 3; i < 2*N + 2; i ++)
    // {
    //     double step = 50.0;
    //     if(i > N + 1) t[i] = tStop - (2*N + 1 - i) * step;
    //     else t[i] = (i - 2) * step;
    // }

    // Normal case: stop at specified time
    for (uint32_t i = 3; i < 2*N + 2; i ++)
    {
        if (i > N + 1) t[i] = tStop;
        else t[i] = 0.0;
    }


    // running module construction
    LogComponentEnable("RunningModule", LOG_LEVEL_INFO);
    LogComponentEnable("MiddlePoliceBox", LOG_LEVEL_INFO);
    cout << "Initializing running module..." << endl;
    RunningModule rm(t, grps, pt, bnBw, bnDelay, "2ms", {isTrackPkt, isBypass}, 1000);
    cout << "Building topology ... " << endl;
    rm.buildTopology(grps);

    // mbox construction
    cout << "Configuring ... " << endl;
    // vector<MiddlePoliceBox> mboxes;
    MiddlePoliceBox mbox1, mbox2;
    double beta = 0.98;
    vector<uint32_t> num;
    if(nGrp == 1 && nTx == 4)           // limitation: current mbox could only process 2 rate level!
        num = vector<uint32_t> {4,4,2,2};
    else if(nGrp == 1 && nTx == 3) 
        num = vector<uint32_t> {3,3,2,1};
    else if(nTx == 10) 
        num = vector<uint32_t> {10,10,1,9};
    else if(nTx == 2)
        num = vector<uint32_t> {2,2,1,1};
    else if(nGrp == 1)
        num = vector<uint32_t> {nTx, nTx, 1, nTx - 1};
    mbox1 = MiddlePoliceBox(num, t[1], pt, fairness, pSize, isTrackPkt, beta, Th, MID1, 50, {isEbrc, isTax, isBypass}, alpha, scale);         // vector{nSender, nReceiver, nClient, nAttacker}

    vector<MiddlePoliceBox> mboxes({mbox1});
    if(nGrp == 2) 
    {
        mbox2 = MiddlePoliceBox(vector<uint32_t>{2,2,1,1}, t[1], pt, fairness, pSize, isTrackPkt, beta, Th, MID2, 50, {isEbrc, isTax, isBypass}, alpha, scale);
        mboxes.push_back(mbox2);
    }
    rm.configure(t[1], pt, bnBw, bnDelay, mboxes);

    // // test pause, resume and disconnect mbox
    // Simulator::Schedule(Seconds(5.1), &RunningModule::disconnectMbox, &rm, grps);
    // Simulator::Schedule(Seconds(8.1), &RunningModule::connectMbox, &rm, grps, 1.0, 1.0);
    // Simulator::Schedule(Seconds(0.01), &RunningModule::pauseMbox, &rm, grps);
    // Simulator::Schedule(Seconds(1.01), &RunningModule::resumeMbox, &rm, grps);

    // flow monitor
    Ptr<FlowMonitor> flowmon;
    FlowMonitorHelper flowmonHelper;
    flowmon = flowmonHelper.InstallAll ();
    flowmon->Start (Seconds (0.0));
    flowmon->Stop (Seconds (t[1]));
    
    // run the simulation
    cout << "Begin populate routing tables ... " << endl;
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    cout << " Start running ... " << endl << endl;
    Simulator::Stop(Seconds(t[1]));
    Simulator::Run();

    cout << " MID: " << MID1 << "," << MID2 << " Destroying ..." << endl << endl;
    flowmon->SerializeToXmlFile ("mrun.flowmon", false, false);
    Simulator::Destroy();

    return 0;
}