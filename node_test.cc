/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 NITK Surathkal
 *
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
 *
 * Author: Jinhui Song <jinhuis2@illinois.edu>
 *
 */

#include "ns3/mbox-module.h"

using namespace std;
using namespace ns3;

enum HostType {TX, RX, ROUTER};

class Host
{
public:
    // function needs considering the operations

private:
    uint32_t id;                    // index in array
    vector<uint32_t> neighbor;      // neighbor id
    HostType type;                  // type of the host
    uint32_t port;                  // port No. using by simulation
    Ptr<Node> node;
    QueueDiscContainer qdc;         // queue ptr container for tracing
    Ipv4InterfaceContainer ifc;     // ipv4 interface ptr container for getting address
    vector< Ptr<MyApp> > senderApp; // sender applications
    ApplicationContainer sinkApp;   // sink applications

    friend class Link;
};

// hash map: (id, neighbor id) - serialize -> 'id' + '_' + 'neighbor id' -> link ptr

class Link
{
public:

private:
    string lid;                     // serialized from (n1 id, n2 id)
    vector<uint32_t> node;          // id of 2 ends
    string bandwidth;
    string delay;
    string mtu;                     // for network layer fragmentation

    friend class Host;
};

