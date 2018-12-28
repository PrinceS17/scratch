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

#ifndef MRUN_H
#define MRUN_H

# include "ns3/mbox.h"

using namespace std;
using namespace ns3;

// should first set before building the topology
class Group
{
public:
    Group(map <uint32_t, string> tx2rate1, vector<uint32_t> rxId1, map<string, uint32_t> rate2port1):
        tx2rate(tx2rate1), rxId(rxId1), rate2port(rate2port1)
        {

            // some direct visit exist?
            for(pair<uint32_t, string> pid:tx2rate)
            {
                // txId and rates vector construction
                if(find(txId.begin(), txId.end(), pid.first) == txId.end())
                    txId.push_back(pid.first);
                if(find(rates.begin(), rates.end(), pid.second) == rates.end())     // need testing
                    rates.push_back(pid.second);

                // rate 2 tx map initialization
                if(rate2tx.find(pid.second) == rate2tx.end())
                    rate2tx[pid.second] = vector<uint32_t>();
                rate2tx[pid.second].push_back(pid.first);
            }

            N = txId.size() + rxId.size() + 2;

            for(pair<string, uint32_t> pid:rate2port)
            {
                if(find(ports.begin(), ports.end(), pid.second) == ports.end())
                    ports.push_back(pid.second);
            }
        }
    ~Group();
    void insertLink(uint32_t tx, uint32_t rx)
    {
        tx2rx.insert(pair<uint32_t, uint32_t>(tx, rx));
    }
    /* links: txs[i] -> rxs[i] */
    void insertLink(vector<uint32_t> txs, vector<uint32_t> rxs)
    {
        for(int i = 0; i < txs.size(); i ++)
            insertLink(txs.at(i), rxs.at(i));
    }

public:
    uint32_t N;                 // number of all nodes including router
    // vector<uint32_t> nodeId;  // collection of all nodes with the same mbox (except routers), seems not needed now??
    vector<uint32_t> routerId;  // router ID, specified after building topology, [tx router, rx router]
    vector<uint32_t> txId;
    vector<uint32_t> rxId;
    vector<string> rates;       // collection all rate in this group
    vector<uint32_t> ports;


    multimap <uint32_t, uint32_t> tx2rx;      // tx-rx node map: m-in-n-out map, test m!=n in later version
    map <string, vector<uint32_t>> rate2tx;   // sort each node by its data rate (like client, attacker before)
    map <uint32_t, string> tx2rate;        // tx node to rate
    map <string, uint32_t> rate2port;     // port for every rate level (abstract for client and attacker)
};

class RunningModule
{
public:
    /**
     * \brief Initialize the running module, including setting start and stop time, the 
     * bottleneck link, the data rates and the topology. Currently should use dumpbell to
     * realize the symmetric topology. 
     * 
     * \param t Vector containing start and stop time.
     * \param grp Vector containing node group which specifies their rates and mboxes.
     * \param pt Protocal type, TCP or UDP.
     * \param delay Bottleneck link delay.
     * \param rate Vector of different level of data rate, e.g. {1kbps, 10kbps, 1Mbps} 
     * for three groups.
     * \param size Packet size, 1000 kB by default.
     */
    RunningModule(vector<double> t, vector<Group> grp, ProtocolType pt, double delay, uint32_t size = 1000);
    ~RunningModule ();
    /**
     * \brief Build the network topology from link (p2p) to network layer (stack). p2p link 
     * attributes should be carefully set (not including queue setting and IP assignment). 
     * Note the routerId of Group should be completed.
     * 
     * \param grp Vector including node group with rate level.
     */
    void buildTopology(vector<Group> grp);
    /**
     * \brief Configure all the network entities after finishing building the topology. The process
     * is: queue, ip assignment, sink app and sender app setting, mbox installation, start 
     * and stop the module. 
     * 
     * \param grp Node group with rate level.
     * \param relative stop Time Stop time of this run.
     * \param pt Transport Protocol.
     * \param bw Vector of the bottleneck links.
     * \param delay Delay of the links. (to be determined)
     * \param Th In format [MinTh, MaxTh].
     */ 
    void configure(vector<Group> grp, double stopTime, ProtocolType pt, vector<string> bw, string delay, vector<double> Th=vector<double>());
    /**
     * \brief Set queue (RED by default, may need other function for other queue) and return 
     * a queue disc container for tracing the packet drop by RED queue.
     * 
     * \param grp Vector including node group with rate level.
     * \param Th In format [MinTh, MaxTh], if empty, then use ns-3 default.
     * \param bw Link bandwidth collection.
     * \param delay Link delay.
     * 
     * \returns A queue disc container on router that we are interested in.
     */
    QueueDiscContainer setQueue(vector<Group> grp, vector<string> bw, string delay,  vector<double> Th=vector<double>());
    /**
     * \brief Set the sink application (fetch protocol from member)
     * 
     * \param Node group with rate level (containing port for different rate).
     * 
     * \returns An application container for all sink apps.
     */
    ApplicationContainer setSink(vector<Group> grp, ProtocolType pt);
    /**
     * \brief Set the sender application (may set from outside)
     * 
     * \param Node group with rate level (containing port for different rate).
     * \param pt Protocol type, TCP or UDP;
     * 
     * \returns A collection of all the pointer of MyApp.
     */
    vector< Ptr<MyApp> > setSender(vector<Group> grp, ProtocolType pt);
    /**
     * \brief Set one single network flow from sender to sink. Basically called by setSender.
     * 
     * \param index In format: [sender, receiver, tag] to indicate src and des 
     * (fetch from nodes, RxDev or so)
     * \param rate data rate of this flow.
     * \param port port of the flow.
     * \param pt Protocol type, TCP or UDP, set from outside will change member.
     * 
     * \returns A pointer to this application.
     */
    Ptr<MyApp> netFlow(vector<uint32_t> index, string rate, uint32_t port);
    /**
     * \brief Connect to the installed mboxes and begin tracing.
     * 
     * \param mboxes Mboxes that installed on the routers.
     * \param grp Node group with rate level.
     */
    void connectMbox(vector<MiddlePoliceBox> mboxes, vector<Group> grp);
    /**
     * \brief Disconnect the installed mboxes and end tracing.
     * 
     * \param mboxes Mboxes that installed on the routers.
     * \param grp Node group with rate level.
     */
    void disconnectMbox(vector<MiddlePoliceBox> mboxes, vector<Group> grp);
    /**
     * \brief Start all the application from Now() and also start the mbox detection by tracing.
     */
    void start();
    /**
     * \brief Stop all the application from Now() and also stop the mbox detection by disconnecting 
     * tracing.
     */ 
    void stop();

public:         // network entity
    NodeContainer nodes;        // all nodes in the topology
    NodeContainer sender;       // fetch each group by index/id
    NodeContainer receiver;
    NetDeviceContainer txDev;   // tx net dev, for mbox mac tx and drop tracing
    NetDeviceContainer rxDev;   // rx net dev, for mbox pkt rx tracing
    QueueDiscContainer qc;      // queue container for trace
    ApplicationContainer sinkApp;   // sink app
    vector< Ptr<MyApp> > senderApp;   // sender app: need testing!

private:        // parameters
    // basic
    uint32_t nSender;
    uint32_t nReceiver;
    vector<Group> groups;       // group node by different mbox: need testing such vector declaration

    uint32_t pktSize;           // 1000 kB
    double rtStart;             // start time of this run (the initial one)
    double rtStop;              // stop time of this run
    ProtocolType protocol;

    // queue
    string qType = "RED";       // specified for queue disc
    double minTh = 100;
    double maxTh = 200;

    // link related
    string normalBw = "1Gbps";
    vector<string> bottleneckBw = vector<string>();
    string delay;
    string mtu = "1599";        // p2p link setting

};

#endif