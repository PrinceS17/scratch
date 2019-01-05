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


#include "ns3/mrun.h"

using namespace std;
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("testMrun");

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

}


uint32_t cnt[2] = {0, 0};

void txSink(Ptr<const Packet> p)
{
    MyTag tag;
    if(p->PeekPacketTag(tag))
        cout << "Tx sink: " << ++cnt[0] << endl;
}


void rxSink(Ptr<const Packet> p)
{
    MyTag tag;
    if(p->PeekPacketTag(tag))
        cout << "                Rx sink: " << ++cnt[1] << endl;
}

int main()
{
    vector<double> t = {0.0, 5.0};
    ProtocolType pt = TCP;
    string strPt = pt == TCP? "ns3::TcpSocketFactory":"ns3::UdpSocketFactory";
    uint32_t port = 8080;
    vector<string> bnBw{"16kbps"};
    vector<string> bnDelay{"2ms"};

    // generate groups
    cout << "Generating groups ... " << endl;
    vector<uint32_t> rtid = {25, 49};
    map<uint32_t, string> tx2rate1 = {{10, "1Mbps"}, {11, "2Mbps"}};
    vector<uint32_t> rxId1 = {2,3};
    map<string, uint32_t> rate2port1 = {{"1Mbps", 80}, {"2Mbps", 90}};
    
    Group g(rtid, tx2rate1, rxId1, rate2port1);
    g.insertLink({10, 11}, {2, 3});
    vector<Group> grps(1, g);

    // initializing running module
    cout << "Initializing running module ... " << endl;
    RunningModule rm (t, grps, pt, bnBw, bnDelay, "2ms", 1000);
    rm.buildTopology(grps);

    // no set queue for this test at first

    // set ipv4 address
    rm.dv[0].AssignIpv4Addresses(Ipv4AddressHelper ("10.1.0.0", "255.255.255.0"),
                          Ipv4AddressHelper ("11.1.0.0", "255.255.255.0"),
                          Ipv4AddressHelper ("12.1.0.0", "255.255.255.0"));
    
    // sink app
    Address sinkLocalAddr (InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkApp;
    PacketSinkHelper psk(strPt, sinkLocalAddr);
    cout << " -- sink uses GetNode() " << endl;
    for(uint32_t i = 0; i < rm.dv[0].RightCount(); i ++)
        // sinkApp.Add(psk.Install(rm.dv[0].GetRight(i)));
        sinkApp.Add(psk.Install(rm.GetNode(0, g.rxId[i])));
    sinkApp.Start(Seconds(t[0]));
    sinkApp.Stop(Seconds(t[1]));

    // sender app
    TypeId tid = pt == TCP? TcpSocketFactory::GetTypeId():UdpSocketFactory::GetTypeId();
    for(uint32_t i = 0; i < rm.dv[0].LeftCount(); i ++)
    {
        cout << " -- sender uses GetNode()" << endl;
        // Ptr<Socket> sockets = Socket::CreateSocket(rm.dv[0].GetLeft(i), tid);
        Ptr<Socket> sockets = Socket::CreateSocket(rm.GetNode(0, g.txId[i]), tid);
        Address sinkAddr(InetSocketAddress(rm.dv[0].GetRightIpv4Address(i), port));
        Ptr<MyApp> app = CreateObject<MyApp> () ;
        app->SetTagValue(i + 1);
        app->Setup(sockets, sinkAddr, 1000, grps[0].rates[i]);
        rm.dv[0].GetLeft(i)->AddApplication(app);
        app->SetStartTime(Seconds(t[0]));
        app->SetStopTime(Seconds(t[1]));
    }

    // tracing
    cout << " -- routers use GetRouter()" << endl;
    // Ptr<Node> lRouter = rm.dv[0].GetLeft();
    // Ptr<Node> rRouter = rm.dv[0].GetRight();
    Ptr<Node> lRouter = rm.GetRouter(0, true);
    Ptr<Node> rRouter = rm.GetRouter(0, false);
    lRouter->GetDevice(0)->TraceConnectWithoutContext("MacTx", MakeCallback(&txSink));
    rRouter->GetDevice(0)->TraceConnectWithoutContext("MacRx", MakeCallback(&rxSink));

    // routing table
    cout << "Populating routing table ... " << endl;
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // run, stop & destroy
    cout << "Running ... " << endl;
    Simulator::Stop(Seconds(t[1]));
    Simulator::Run();
    cout << "Destroying ... " << endl;
    Simulator::Destroy();
    return 0;
}