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

RunningModule::RunningModule(vector<double> t, vector<Group> grp, ProtocolType pt, vector<string> bnBw, vector<string> bnDelay, string delay, uint32_t size)
{
    // constant setting
    nSender = 0;
    nReceiver = 0;
    for(Group g:groups)
    {
        nSender += g.txId.size();
        nReceiver += g.rxId.size();
    }
    groups = grp;
    pktSize = size;
    rtStart = t.at(0);
    rtStop = t.at(1);
    protocol = pt;
    bottleneckBw = bnBw;
    bottleneckDelay = bnDelay;
    this->delay = delay;

}

RunningModule::~RunningModule(){}

void RunningModule::buildTopology(vector<Group> grp)
{
    // use dumbbell structure for now
    // index: see SetNode() and GetNode()

    // set single p2p leaf link (drop tail by default)
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
    // debug only
    cout << "# interfaces: " << dv.at(0).GetLeft()->GetObject<Ipv4>()->GetNInterfaces() << endl;
    cout << "# devices: " << dv.at(0).GetLeft()->GetNDevices() << endl;
    
    // set node 
    NS_LOG_FUNCTION("Setting nodes...");
    for(uint32_t i = 0; i < grp.size(); i ++)
    {
        SetNode(dv.at(i).GetLeft(), 2, i, 0);
        SetNode(dv.at(i).GetRight(), 2, i, 1);
        // debug only
        // cout << "# interfaces: "<< GetNode(i, grp[i].routerId[0])->GetObject<Ipv4>()->GetNInterfaces() << endl;
    
        for(uint32_t j = 0; j < grp.at(i).txId.size(); j ++)
            SetNode(dv.at(i).GetLeft(j), 0, i, j);
        for(uint32_t j = 0; j < grp.at(i).rxId.size(); j ++)
            SetNode(dv.at(i).GetRight(j), 1, i, j);
        
    }

    // regular get node test
    NS_LOG_FUNCTION("Getting nodes tests...");
    stringstream ss;
    for(uint32_t i = 0; i < grp.size(); i ++)
    {
        for(uint32_t j = 0; j < grp.at(i).rxId.size(); j ++)
        {
            StringValue sv, dsv;
            Ptr<Node> pn = GetNode(i, grp[i].rxId[j]);
            ss << pn->GetId() << " ; ";
            uint32_t nn = pn->GetNDevices();
            int x = 0;
            pn->GetDevice(0)->GetAttribute("DataRate", sv);

            ss << "Data rate: " << sv.Get() << ", delay: " << dsv.Get() << endl;
        }
    }
    NS_LOG_FUNCTION("rx id: " + ss.str());

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

    // for debug only
    cout << "# dev of router: " << GetRouter(0, true)->GetNDevices() << endl;

    protocol = pt;
    bottleneckBw = bw;
    bottleneckDelay = delay;
    qc = setQueue(groups, bottleneckBw, bottleneckDelay, Th);
    ifc = setAddress();
    sinkApp = setSink(groups, protocol);
    senderApp = setSender(groups, protocol);
    connectMbox(mboxes, groups, 1.0, 1.0);      // manually set here 
    NS_LOG_INFO("Starting running module ... ");
    // start();

    // only for debug
    stringstream ss;
    ss << "Time setting: " << rtStart << "s ; " << rtStop << "s. ";
    NS_LOG_INFO(ss.str());
}

QueueDiscContainer RunningModule::setQueue(vector<Group> grp, vector<string> bnBw, vector<string> bnDelay, vector<double> Th)
{
    // set RED queue
    QueueDiscContainer qc;
    for(uint32_t i = 0; i < grp.size(); i ++)
    {
        TrafficControlHelper tch;
        if(Th.empty()) tch.SetRootQueueDisc("ns3::RedQueueDisc", 
                                            "LinkBandwidth", StringValue(bnBw.at(i)),
                                            "LinkDelay", StringValue(bnDelay.at(i)));
        else tch.SetRootQueueDisc("ns3::RedQueueDisc",
                                "MinTh", DoubleValue(Th.at(0)),
                                "MaxTh", DoubleValue(Th.at(1)),
                                "LinkBandwidth", StringValue(bnBw.at(i)),
                                "LinkDelay", StringValue(bnDelay.at(i)));
        qc.Add(tch.Install(GetNode(i, grp.at(i).routerId[0])->GetDevice(0)));
        // qc.Add(tch.Install(router.Get(2*i)->GetDevice(0)));       // sender's router queue needed only
    }

    // test only
    stringstream ss;
    int x = 0;
    ss << qc.Get(x)->GetTypeId() << " ; size now: " << qc.Get(x)->GetCurrentSize().GetValue() << endl;
    // ss << qc.Get(x)->GetStats() << "\nWake mode: " << qc.Get(x)->GetWakeMode();
    NS_LOG_INFO(ss.str());
    
    
    return qc;
}

Ipv4InterfaceContainer RunningModule::setAddress()
{
    // assign Ipv4 addresses
    Ipv4AddressHelper ih1("10.1.0.0", "255.255.255.0");
    // Ipv4AddressHelper ih2("11.1.0.0", "255.255.255.252");
    // Ipv4AddressHelper ih3("12.1.0.0", "255.255.255.252");
    
    // use GetRouter and GetNode()
    NetDeviceContainer ncTx, ncRx, ncRt;
    for(uint32_t i = 0; i < groups.size(); i ++)
    {
        Group g = groups.at(i);
        for(uint32_t j = 0; j < g.txId.size(); j ++)
            for(uint32_t k = 0; k < GetNode(i, g.txId.at(j))->GetNDevices(); k ++)
                ncTx.Add(GetNode(i, g.txId.at(j))->GetDevice(k));
        for(uint32_t j = 0; j < g.rxId.size(); j ++)
            for(uint32_t k = 0; k < GetNode(i, g.rxId.at(j))->GetNDevices(); k ++)
                ncTx.Add(GetNode(i, g.rxId.at(j))->GetDevice(k));    
        for(uint32_t j = 0; j < g.routerId.size(); j ++)
            for(uint32_t k = 0; k < GetNode(i, g.routerId.at(j))->GetNDevices(); k ++)
                ncTx.Add(GetNode(i, g.routerId.at(j))->GetDevice(k));
    }  

    // NetDeviceContainer ncTx, ncRx, ncRt;    // assign ipv4 address to p2p net devices
    // for(uint32_t j = 0; j < sender.GetN(); j ++) 
    //     for(uint32_t k = 0; k < sender.Get(j)->GetNDevices(); k ++)
    //         ncTx.Add(sender.Get(j)->GetDevice(k));
    // for(uint32_t j = 0; j < receiver.GetN(); j ++) 
    //     for(uint32_t k = 0; k < receiver.Get(j)->GetNDevices(); k ++)
    //         ncRx.Add(receiver.Get(j)->GetDevice(k));
    // for(uint32_t j = 0; j < router.GetN(); j ++) 
    //     for(uint32_t k = 0; k < router.Get(j)->GetNDevices(); k ++)
    //         ncRt.Add(router.Get(j)->GetDevice(k));
    
    // collect interfaces
    Ipv4InterfaceContainer ifc;
    ifc.Add(ih1.Assign(ncTx));
    ifc.Add(ih1.Assign(ncRx));
    ifc.Add(ih1.Assign(ncRt));
    // actually we can also preserve these net device in header file also

    // for debug
    stringstream ss;
    ss << "Device: " << endl;
    for(uint32_t j = 0; j < sender.GetN(); j ++)
    {
        ss << sender.Get(j)->GetNDevices() << ": ";
        // ss << sender.Get(j)->GetDevice(0) << ", " << sender.Get(j)->GetDevice(1) << "; ";
    }
    for(uint32_t j = 0; j < receiver.GetN(); j ++)
    {
        ss << receiver.Get(j)->GetNDevices() << ": ";
        // ss << receiver.Get(j)->GetDevice(0) << ", " << receiver.Get(j)->GetDevice(1) << "; ";
    }
    for(uint32_t j = 0; j < router.GetN(); j ++)
    {
        ss << "# Devices: " << router.Get(j)->GetNDevices() << "; ";
        // Ptr<Ipv4> ipv4 = router.Get(j)->GetObject<Ipv4>();
        // ss << "# interfaces: " << ipv4->GetNInterfaces() << endl;
        ss << endl;
    }
    ss << "  unit length: " << u << endl;
    NS_LOG_INFO(ss.str());

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
            ss << "TX ID: " << tid << "; RX ID: ";
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
                uint32_t tag = i*u + (uint32_t)(it2 - g.txId.begin());      // tag: index in sender
                ss << "RX id: " << it->second << "; Tag : " << tag << "; " << tId << " -> " << it->second << endl;
                appc.push_back(netFlow(i, tId, it->second, tag));
                // test output
            }
        }
    }
    NS_LOG_INFO(ss.str());
    return appc;
}

Ptr<MyApp> RunningModule::netFlow(uint32_t i, uint32_t tId, uint32_t rId, uint32_t tag)
{
    // parse rate and port
    stringstream ss;
    ss << "Group: " << i << "; TX: " << tId << "; RX: " << rId << endl;
    string rate = groups.at(i).tx2rate.at(tId);
    uint32_t port = groups.at(i).rate2port.at(rate);

    // set socket
    TypeId tpid = protocol == TCP? TcpSocketFactory::GetTypeId():UdpSocketFactory::GetTypeId();
    Ptr<Socket> skt = Socket::CreateSocket(GetNode(i, tId), tpid); 
    Address sinkAddr(InetSocketAddress(GetIpv4Addr(i, rId), port));
    Ptr<MyApp> app = CreateObject<MyApp> ();
    app->SetTagValue(tag + 1);
    app->Setup(skt, sinkAddr, pktSize, rate);
    GetNode(i, tId)->AddApplication(app);

    // for debug
    GetIpv4Addr(i, rId).Print(ss);
    NS_LOG_INFO(ss.str());

    return app;
}

// trace sink only for test
void txSink(Ptr<const Packet> p)
{
    cout << "tx sink!" << endl;
}

void RunningModule::connectMbox(vector<MiddlePoliceBox>& mboxes, vector<Group> grp, double interval, double logInterval)
{
    NS_LOG_FUNCTION("Begin.");
    for(uint32_t i = 0; i < grp.size(); i ++)
    {
        // MiddlePoliceBox& mb = mboxes.at(i);         // not sure
        Ptr<NetDevice> txRouter = GetRouter(i, true)->GetDevice(0);
        Ptr<NetDevice> rxRouter = GetRouter(i, false)->GetDevice(0);        
        // Ptr<NetDevice> txRouter = router.Get(2*i)->GetDevice(0);
        // Ptr<NetDevice> rxRouter = router.Get(2*i + 1)->GetDevice(0);

        // for debug only
        stringstream ss;
        ss << "router's dev         mtu " << endl;
        for(uint j = 0; j < GetRouter(i, true)->GetNDevices(); j ++)
        {
            Ptr<NetDevice> dev = GetRouter(i, true)->GetDevice(j);
            ss << dev << "  " << dev->GetMtu() << endl;
        }
        NS_LOG_INFO(ss.str());   


        // install mbox
        mboxes.at(i).install(txRouter);
        NS_LOG_FUNCTION("Mbox installed on router " + to_string(i));

        // tracing
        txRouter->TraceConnectWithoutContext("MacTx", MakeCallback(&MiddlePoliceBox::onMacTx, &mboxes.at(i)));
        rxRouter->TraceConnectWithoutContext("MacRx", MakeCallback(&MiddlePoliceBox::onPktRx, &mboxes.at(i)));
        qc.Get(i)->TraceConnectWithoutContext("Drop", MakeCallback(&MiddlePoliceBox::onQueueDrop, &mboxes.at(i)));        

        // debug only
        NS_LOG_FUNCTION("Router size: " + to_string(router.GetN()));
        txRouter->TraceConnectWithoutContext("MacTx", MakeCallback(&txSink));   // not work
        rxRouter->TraceConnectWithoutContext("MacTx", MakeCallback(&txSink));   // not work

        // flow control
        mboxes.at(i).flowControl(PERSENDER, interval, logInterval);
    }
}

void RunningModule::disconnectMbox(vector<MiddlePoliceBox>& mboxes, vector<Group> grp)
{
    for(uint32_t i = 0; i < grp.size(); i ++)
    {
        MiddlePoliceBox mb(mboxes.at(i));
        Ptr<NetDevice> txRouter = router.Get(2*i)->GetDevice(0);
        Ptr<NetDevice> rxRouter = router.Get(2*i + 1)->GetDevice(0);

        // stop the mbox and tracing
        mb.stop();      // stop flow control and logging 
        txRouter->TraceDisconnectWithoutContext("MacTx", MakeCallback(&MiddlePoliceBox::onMacTx, &mb));
        rxRouter->TraceDisconnectWithoutContext("MacRx", MakeCallback(&MiddlePoliceBox::onPktRx, &mb));
        qc.Get(i)->TraceDisconnectWithoutContext("Drop", MakeCallback(&MiddlePoliceBox::onQueueDrop, &mb));        

        NS_LOG_FUNCTION("Mbox " + to_string(i) + " stops.");
    }
}    

void RunningModule::pauseMbox(vector<MiddlePoliceBox>& mboxes, vector<Group> grp)
{
    for(uint32_t i = 0; i < grp.size(); i ++)
    {
        MiddlePoliceBox mb(mboxes.at(i));
        Ptr<NetDevice> txRouter = router.Get(2*i)->GetDevice(0);
        txRouter->TraceDisconnectWithoutContext("MacTx", MakeCallback(&MiddlePoliceBox::onMacTx, &mb));
        txRouter->TraceConnectWithoutContext("MacTx", MakeCallback(&MiddlePoliceBox::onMacTxWoDrop, &mb));
    }
}

void RunningModule::resumeMbox(vector<MiddlePoliceBox>& mboxes, vector<Group> grp)
{
    for(uint32_t i = 0; i < grp.size(); i ++)
    {
        MiddlePoliceBox mb(mboxes.at(i));
        Ptr<NetDevice> txRouter = router.Get(2*i)->GetDevice(0);
        txRouter->TraceDisconnectWithoutContext("MacTx", MakeCallback(&MiddlePoliceBox::onMacTxWoDrop, &mb));
        txRouter->TraceConnectWithoutContext("MacTx", MakeCallback(&MiddlePoliceBox::onMacTx, &mb));
    }
}

void RunningModule::start()
{
    sinkApp.Start(Seconds(rtStart));
    sinkApp.Stop(Seconds(rtStop));
    for(uint32_t j = 0; j < senderApp.size(); j ++)
    {
        senderApp.at(j)->SetStartTime(Seconds(rtStart));
        senderApp.at(j)->SetStopTime(Seconds(rtStop));
    }
}

void RunningModule::stop()
{
    for(uint32_t j = 0; j < senderApp.size(); j ++)
        senderApp.at(j)->SetStopTime(Seconds(0.0));     // stop now
    sinkApp.Stop(Seconds(0.0));
}   

int main ()
{
    // set start and stop time
    vector<double> t(2);
    t[0] = 0.0;
    t[1] = 5.0;

    // define bottleneck link bandwidth and delay
    vector<string> bnBw{"80kbps"};
    vector<string> bnDelay{"2ms"};
    ProtocolType pt = TCP;

    // generating groups
    cout << "Generating groups of nodes ... " << endl;
    vector<uint32_t> rtid = {25, 49};
    map<uint32_t, string> tx2rate1 = {{10, "1Mbps"}, {11, "2Mbps"}};
    vector<uint32_t> rxId1 = {2,3};
    map<string, uint32_t> rate2port1 = {{"1Mbps", 80}, {"2Mbps", 90}};
    
    Group g(rtid, tx2rate1, rxId1, rate2port1);
    g.insertLink({10, 11}, {2, 3});
    vector<Group> grps(1, g);

    // running module construction
    LogComponentEnable("RunningModule", LOG_LEVEL_FUNCTION);
    LogComponentEnable("MiddlePoliceBox", LOG_LEVEL_FUNCTION);
    cout << "Initializing running module..." << endl;
    RunningModule rm(t, grps, pt, bnBw, bnDelay, "2ms", 1000);
    
    cout << "Building topology ... " << endl;
    rm.buildTopology(grps);
 
    // for copy constructor test only
    cout << "Is MiddlePoliceBox move_constructible? " << is_move_constructible<MiddlePoliceBox>::value << endl;
    cout << "Is MiddlePoliceBox default_constructible? " << is_default_constructible<MiddlePoliceBox>::value << endl;
    cout << "Is MiddlePoliceBox copy_constructible? " << is_copy_constructible<MiddlePoliceBox>::value << endl;

    // mbox construction
    cout << "Configuring ... " << endl;    
    MiddlePoliceBox mbox(vector<uint32_t>{2,2,1,1}, t[1], pt, 0.9);      
        // limitation: mbox could only process 2 rate level!
    vector<MiddlePoliceBox> mboxes;
    mboxes.push_back(mbox);    
    rm.configure(t[1], pt, bnBw, bnDelay, mboxes);


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

    cout << " Destroying ... " << endl << endl;
    flowmon->SerializeToXmlFile ("mrun.flowmon", false, false);
    Simulator::Destroy();

    return 0;
}