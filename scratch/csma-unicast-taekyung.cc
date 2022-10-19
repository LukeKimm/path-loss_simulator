#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-model.h"
#include "ns3/mobility-helper.h"
#include "ns3/position-allocator.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaBroadcastExample");

int 
main (int argc, char *argv[])
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
  cmd.Parse (argc, argv);

  // std::string animFile = "csma-broadcast.xml" ;  // Name of file for animation output

  NS_LOG_INFO ("Create nodes.");
  NodeContainer c;
  //node 100개로 생성
  c.Create (101);

//   // c0, c1 두개로 나누어서 전송로 구성
//   NodeContainer c0 = NodeContainer (c.Get (0), c.Get (1));
//   NodeContainer c1 = NodeContainer (c.Get (0), c.Get (2));

  NS_LOG_INFO ("Build Topology.");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate (5000000)));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  // csma.SetChannelAttribute ("Delay", TimeValue (Seconds (0.5)));

//   // n0, n1 두개로 나누어서 전송로 구성
//   NetDeviceContainer n0 = csma.Install (c0);
//   NetDeviceContainer n1 = csma.Install (c1);
  NetDeviceContainer n = csma.Install (c);
  


  // add mobility model for animation
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  for (int i = 0; i < 10; i++){
    for (int k = 0; k < 10; k++){
      positionAlloc->Add (Vector (i*1.0, k*1.0, 0.0));
      mobility.SetPositionAllocator (positionAlloc);
      mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      mobility.Install (c);
    }
  }

  InternetStackHelper internet;
  internet.Install (c);

  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.0.0", "255.255.255.0");
  ipv4.Assign (n);


  // RFC 863 discard port ("9") indicates packet should be thrown away
  // by the system.  We allow this silent discard to be overridden
  // by the PacketSink application.
  uint16_t port = 9;

  // Create the OnOff application to send UDP datagrams of size
  // 512 bytes (default) at a rate of 500 Kb/s (default) from n0
  NS_LOG_INFO ("Create Applications.");
  // onoffhelper의 역할이 무엇인가??
  OnOffHelper onoff ("ns3::UdpSocketFactory", 
                     Address (InetSocketAddress (Ipv4Address ("255.255.255.255"), port)));
  onoff.SetConstantRate (DataRate ("500kb/s"));

  //c는 onoff install하고 
  ApplicationContainer app = onoff.Install (c.Get (0));
//   // Start the application
//   app.Start (Seconds (1.0));
//   app.Stop (Seconds (10.0));

  // Create an optional packet sink to receive these packets
  // packetsinkhelper의 역할이 무엇?
  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
  //c에 packetsinkhelper를 install하고 
  app = sink.Install (c.Get (0));
  // app.Add (sink.Install (c.Get (0)));
  app.Start (Seconds (1.0));
  app.Stop (Seconds (10.0));

  // // Configure ascii tracing of all enqueue, dequeue, and NetDevice receive 
  // // events on all devices.  Trace output will be sent to the file 
  // // "csma-one-subnet.tr"
  // AsciiTraceHelper ascii;
  // csma.EnableAsciiAll (ascii.CreateFileStream ("csma-broadcast.tr"));

  // // Also configure some tcpdump traces; each interface will be traced
  // // The output files will be named 
  // // csma-broadcast-<nodeId>-<interfaceId>.pcap
  // // and can be read by the "tcpdump -tt -r" command 

  // csma.EnablePcapAll ("csma-broadcast", false);

  AnimationInterface anim ("csma-broadcast.xml");
  
  NS_LOG_INFO ("Run Simulation.");
  
  Simulator::Run ();
  Simulator::Destroy ();
  
  NS_LOG_INFO ("Done.");
}
