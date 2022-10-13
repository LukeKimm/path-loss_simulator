/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006,2007 INRIA
 * Copyright (c) 2013 Dalian University of Technology
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 * Author: Junling Bu <linlinjavaer@gmail.com>
 *
 */
/**
 * This example shows basic construction of an 802.11p node.  Two nodes
 * are constructed with 802.11p devices, and by default, one node sends a single
 * packet to another node (the number of packets and interval between
 * them can be configured by command-line arguments).  The example shows
 * typical usage of the helper classes for this mode of WiFi (where "OCB" refers
 * to "Outside the Context of a BSS")."
 */

#include "ns3/vector.h"
#include "ns3/string.h"
#include "ns3/socket.h"
#include "ns3/double.h"
#include "ns3/config.h"
#include "ns3/log.h"
#include "ns3/command-line.h"
#include "ns3/mobility-model.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/position-allocator.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/wave-bsm-helper.h"
#include "ns3/bsm-application.h"
#include <iostream>
#include <string>
#include <vector>

#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/netanim-module.h"
#include <random>


using namespace ns3;
using std::string;
using std::to_string;

#define MAX_NODE 51
#define COLUMN 10
#define ROW 5
NS_LOG_COMPONENT_DEFINE ("WifiSimpleOcb");

/*
 * In WAVE module, there is no net device class named like "Wifi80211pNetDevice",
 * instead, we need to use Wifi80211pHelper to create an object of
 * WifiNetDevice class.
 *
 * usage:
 *  NodeContainer nodes;
 *  NetDeviceContainer devices;
 *  nodes.Create (2);
 *  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
 *  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
 *  wifiPhy.SetChannel (wifiChannel.Create ());
 *  NqosWaveMacHelper wifi80211pMac = NqosWave80211pMacHelper::Default();
 *  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
 *  devices = wifi80211p.Install (wifiPhy, wifi80211pMac, nodes);
 *
 * The reason of not providing a 802.11p class is that most of modeling
 * 802.11p standard has been done in wifi module, so we only need a high
 * MAC class that enables OCB mode.
 */

// PVD
void ReceivePacket_PVD (Ptr<Socket> socket)
{
  while (socket->Recv ())
    {
      NS_LOG_UNCOND ("RSU received PVD from OBU");
    }
}
// BSM
void ReceivePacket_BSM (Ptr<Socket> socket)
{

  while (socket->Recv ())
    {
      // string BSM = to_string(socket->GetNode()->GetId()) + "th OBU received broadcasting BSM from OBU";
      // NS_LOG_UNCOND (BSM);
    }
}

static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize,
                             uint32_t pktCount, Time pktInterval )
{
  if (pktCount > 0)
    {
      socket->Send (Create<Packet> (pktSize));
      Simulator::Schedule (pktInterval, &GenerateTraffic,
                           socket, pktSize,pktCount - 1, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}

int main (int argc, char *argv[])
{
  std::string phyMode ("OfdmRate6MbpsBW10MHz");
  uint32_t packetSize = 1000; // bytes
  uint32_t numPackets = 1;
  std::string animFile = "wave-80211p.xml" ;  // Name of file for animation output
  double interval = 1.0; // seconds
  bool verbose = false;

  CommandLine cmd (__FILE__);

  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "number of packets generated", numPackets);
  cmd.AddValue ("interval", "interval (seconds) between packets", interval);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("animFile",  "File Name for Animation Output", animFile);
  cmd.Parse (argc, argv);
  // Convert to time object
  Time interPacketInterval = Seconds (interval);


  NodeContainer c;
  c.Create (MAX_NODE);

  // The below set of helpers will help us to put together the wifi NICs we want
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  Ptr<YansWifiChannel> channel = wifiChannel.Create ();
  wifiPhy.SetChannel (channel);
  // ns-3 supports generate a pcap trace
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11);
  NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
  if (verbose)
    {
      wifi80211p.EnableLogComponents ();      // Turn on all Wifi 802.11p logging
    }

  wifi80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode",StringValue (phyMode),
                                      "ControlMode",StringValue (phyMode));
  NetDeviceContainer devices = wifi80211p.Install (wifiPhy, wifi80211pMac, c);

  // Tracing
  wifiPhy.EnablePcap ("wave-simple-80211p", devices);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (45, 0.0, 0.0));

  for(int i =0; i<ROW; i++)
  {
    for(int j=0; j<COLUMN; j++)
      positionAlloc->Add (Vector (10.0*j, 10.0*(i+1), 0.0));
  }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (c);

  NS_LOG_INFO ("Enabling OLSR Routing");
  
  InternetStackHelper internet;
  internet.Install (c);

  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  /// RSU가 OBU에게 broadcast message를 보낼 때!!
  // 1. 다중 node의 동시 broadcast 구현
  // 2. broadcast Interval을 부여
  //   - ns3 내에 timer = 100ms 

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dis(0,50);


  // Ptr<Socket> recvSink = Socket::CreateSocket (c.Get (0), tid);
  // InetSocketAddress local = InetSocketAddress (i.GetAddress(0), 80);
  // recvSink->Bind (local);
  // recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));                                     

  // // for(int j=1; j<MAX_NODE; j++)
  // //   {

  //     InetSocketAddress remote = InetSocketAddress (i.GetAddress(0), 80);
  //     Ptr<Socket> source = Socket::CreateSocket (c.Get (2), tid);
  //     // source->SetAllowBroadcast (true);
  //     source->Connect (remote);
  // //  Simulator::ScheduleWithContext (source->GetNode ()->GetId (),

  //     Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
  //                             Seconds (1), &GenerateTraffic,
  //                             source, packetSize, numPackets, interPacketInterval);
                            
  //     Simulator::Schedule (Seconds (1), &GenerateTraffic, source, packetSize, numPackets, interPacketInterval);                   
  //   // }



//////////////////////////////////////////////////////////////////////////////////////
// no random PVD Unicast code  
  // int m = 0;                           
  // while(m<5)
  //   {
  //   for(int i=1; i<MAX_NODE; i++)
  //     {
  //       InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), i);
  //       Ptr<Socket> recvSink = Socket::CreateSocket (c.Get (0), tid);
  //       InetSocketAddress local = InetSocketAddress (Ipv4Address("255.255.255.255"), i);
  //       recvSink->Bind (local);
  //       recvSink->SetRecvCallback (MakeCallback(&ReceivePacket_PVD));
  //       Ptr<Socket> source = Socket::CreateSocket (c.Get (i), tid);
  //       source->SetAllowBroadcast (true);
  //       source->Connect (remote);
  //       // NS_LOG_UNCOND (source->GetNode ()->GetId ());   
  //       Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
  //                               Seconds (m+0.02*i), &GenerateTraffic,
  //                               source, packetSize, numPackets, interPacketInterval);
  //     }
  //     m++;
  //   }

//////////////////////////////////////////////////////////////////////////////////////
/// 50개씩 감! --->> BSM Broadcast Success
  InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
  int n = 1;
  int second_node = 1;
  int second_node_1 = 2;
  while(n<250)
  {
    int first_node = dis(gen);
    int first_node_1 = dis(gen);
    // NS_LOG_UNCOND (second_node);   
    // NS_LOG_UNCOND (second_node_1);   

    if(first_node != second_node && first_node != first_node_1 
                                  && first_node != second_node_1 && first_node_1 != second_node 
                                  && first_node_1 != second_node_1 && second_node != second_node_1)   
    {
      for(int j =0; j<MAX_NODE; j++)
        {
          if(first_node != j && second_node != j)
          {
          Ptr<Socket> recvSink = Socket::CreateSocket (c.Get (j), tid);
          InetSocketAddress local = InetSocketAddress (Ipv4Address("255.255.255.255"), 80);
          recvSink->Bind (local);

          // recvSink->SetRecvCallback (MakeCallback (&BsmApplication::ReceiveWavePacket, recvSink));
          recvSink->SetRecvCallback (MakeCallback (&ReceivePacket_BSM));                                     
          }
        }
        Ptr<Socket> source = Socket::CreateSocket (c.Get (first_node), tid);
        Ptr<Socket> source1 = Socket::CreateSocket (c.Get (first_node_1), tid);
        source->SetAllowBroadcast (true);
        source->Connect (remote);
        source1->SetAllowBroadcast (true);
        source1->Connect (remote);
        Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
                                        Seconds (0.02*n+0.015), &GenerateTraffic,
                                        source, packetSize, numPackets, interPacketInterval);
        Simulator::ScheduleWithContext (source1->GetNode ()->GetId (),
                                        Seconds (0.02*n+0.01), &GenerateTraffic,
                                        source1, packetSize, numPackets, interPacketInterval);
        string BSM = to_string(source->GetNode()->GetId()) + "th OBU send BSM message as broadcast";
        NS_LOG_UNCOND (BSM);
        string BSM1 = to_string(source1->GetNode()->GetId()) + "th OBU send BSM message as broadcast";
        NS_LOG_UNCOND (BSM1);
        
        second_node = first_node;
        second_node_1 = first_node_1;
    }   
      n++;
  }
  AnimationInterface anim (animFile);
  Simulator::Run ();
  std::cout << "Animation Trace file created:" << animFile.c_str ()<< std::endl;
  Simulator::Destroy ();

  return 0;
}
