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

RunningModule::RunningModule(vector<double> t, vector<Group> grp, ProtocolType pt, double delay, uint32_t size)
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
    protocol = pt;
    this->delay = delay;

}

void RunningModule::buildTopology(vector<Group> grp)
{
    // use dumbbell structure for now
    

}

int main ()
{


    return 0;
}