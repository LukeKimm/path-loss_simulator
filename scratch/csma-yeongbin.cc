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

//
// Example of the sending of a datagram to a broadcast address
//
// Network topology
//     ==============
//       |          |
//       n0    n1   n2
//       |     |
//     ==========
//
//   n0 originates UDP broadcast to 255.255.255.255/discard port, which 
//   is replicated and received on both n1 and n2

#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-helper.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/netanim-module.h"
#include <random>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaBroadcastExample");

int main (int argc, char *argv[])
{
  // Users may find it convenient to turn on explicit debugging
  // for selected modules; the below lines suggest how to do this

#if 0
  LogComponentEnable ("CsmaBroadcastExample", LOG_LEVEL_INFO);
#endif
  LogComponentEnable ("CsmaBroadcastExample", LOG_PREFIX_TIME);

  // Allow the user to override any of the defaults and the above
  // Bind()s at run-time, via command-line arguments
  CommandLine cmd (__FILE__);
  std::string animFile = "CSMA.xml" ;  // Name of file for animation output

  cmd.AddValue ("animFile",  "File Name for Animation Output", animFile);

  cmd.Parse (argc, argv);
  NS_LOG_INFO ("Create nodes.");
  NodeContainer c;
  c.Create (100);
  // NodeContainer c0 = NodeContainer (c.Get (0), c.Get (1));
  // NodeContainer c1 = NodeContainer (c.Get (0), c.Get (2));
  NodeContainer c1 = NodeContainer (c,1,100);
  // NodeContainer c2 = NodeContainer::Add(c.Get(0));
  // NodeContainer c2 = NodeContainer::Add(c.Get(1));
  NodeContainer c2 = NodeContainer(c, 2 ,100);
  NS_LOG_INFO ("Build Topology.");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate (5000000)));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  // NetDeviceContainer n0 = csma.Install (c0);
  NetDeviceContainer n0 = csma.Install (c2);
  NetDeviceContainer n1 = csma.Install (c1);


  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for(int i =0; i<10; i++)
  {
    for(int j=0;j<10;j++)
      positionAlloc->Add (Vector (0.2*j, 0.2*(i+1), 0.0));
  }
  mobility.SetPositionAllocator (positionAlloc);  
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (c);


  InternetStackHelper internet;
  internet.Install (c);


  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.0.0", "255.255.255.0");
  ipv4.Assign (n0);
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  ipv4.Assign (n1);


  // RFC 863 discard port ("9") indicates packet should be thrown away
  // by the system.  We allow this silent discard to be overridden
  // by the PacketSink application.
  uint16_t port = 9;

  // Create the OnOff application to send UDP datagrams of size
  // 512 bytes (default) at a rate of 500 Kb/s (default) from n0
  NS_LOG_INFO ("Create Applications.");
  OnOffHelper onoff ("ns3::UdpSocketFactory", 
                     Address (InetSocketAddress (Ipv4Address ("255.255.255.255"), port)));
  onoff.SetConstantRate (DataRate ("500kb/s"));

  // ApplicationContainer app = onoff.Install (c0.Get (0));
  ApplicationContainer app = onoff.Install (c2.Get (0));
  // Start the application
  app.Start (Seconds (1.0));
  app.Stop (Seconds (10.0));

  // Create an optional packet sink to receive these packets
  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
  // app = sink.Install (c0.Get (1));
  app = sink.Install (c2.Get (1));
  app.Add (sink.Install (c1.Get (1)));
  app.Start (Seconds (1.0));
  app.Stop (Seconds (10.0));

  OnOffHelper onoff1 ("ns3::UdpSocketFactory", 
                     Address (InetSocketAddress (Ipv4Address ("255.255.255.255"), port+1)));
  onoff1.SetConstantRate (DataRate ("500kb/s"));
  ApplicationContainer app1 = onoff1.Install (c2.Get (2));
  // Start the application
  app1.Start (Seconds (1.0));
  app1.Stop (Seconds (10.0));

  // Create an optional packet sink to receive these packets
  PacketSinkHelper sink1 ("ns3::UdpSocketFactory",
                         Address (InetSocketAddress (Ipv4Address::GetAny (), port+1)));
  // app = sink.Install (c0.Get (1));
  app1 = sink1.Install (c2.Get (2));
  app1.Add (sink1.Install (c1.Get (2)));
  app1.Start (Seconds (1.0));
  app1.Stop (Seconds (10.0));

  // for(int i=0; i<100; i++)
  // {
  //   ApplicationContainer app = onoff.Install (c2.Get (i));
  //   // Start the application
  //   app.Start (Seconds (1.0));
  //   app.Stop (Seconds (10.0));

  //   // Create an optional packet sink to receive these packets
  //   PacketSinkHelper sink ("ns3::UdpSocketFactory",
  //                         Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
  //   // app = sink.Install (c0.Get (1));
  //   if(i<51)
  //   {
  //   app = sink.Install (c2.Get (i+1));
  //   app.Add (sink.Install (c1.Get (i+1)));
  //   app.Start (Seconds (1.0));
  //   app.Stop (Seconds (10.0));
  //   }
  //   else
  //   {
  //   app = sink.Install (c2.Get (i-1));
  //   app.Add (sink.Install (c1.Get (i-1)));
  //   app.Start (Seconds (1.0));
  //   app.Stop (Seconds (10.0));  
  //   }
  // }

  // Configure ascii tracing of all enqueue, dequeue, and NetDevice receive 
  // events on all devices.  Trace output will be sent to the file 
  // "csma-one-subnet.tr"
  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("csma-broadcast.tr"));

  // Also configure some tcpdump traces; each interface will be traced
  // The output files will be named 
  // csma-broadcast-<nodeId>-<interfaceId>.pcap
  // and can be read by the "tcpdump -tt -r" command 
  csma.EnablePcapAll ("csma-broadcast", false);

  NS_LOG_INFO ("Run Simulation.");
  
  AnimationInterface anim (animFile);
  Simulator::Run ();
  std::cout << "Animation Trace file created:" << animFile.c_str ()<< std::endl;
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
