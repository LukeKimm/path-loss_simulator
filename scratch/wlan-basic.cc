#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleMpduAggregation");

int main (int argc, char *argv[])
{
	uint32_t payloadSize = 1472; //bytes
	uint64_t simulationTime = 10; //seconds

	CommandLine cmd;
	cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
	cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
	cmd.Parse (argc, argv);

	// 1. Create Nodes
	NodeContainer wifiStaNode;
	wifiStaNode.Create (1);
	NodeContainer wifiApNode;
	wifiApNode.Create (1);

	// 2. Create PHY layer (wireless channel)
	YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
	YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
	Ptr<YansWifiChannel> channel = wifiChannel.Create ();
	phy.SetChannel (channel);			// To-do #1		Create and set wireless channel

	// 3. Create MAC layer
	WifiMacHelper mac;
	Ssid ssid = Ssid ("Section6");
	mac.SetType ("ns3::StaWifiMac",
			"Ssid", SsidValue (ssid),
			"ActiveProbing", BooleanValue (false));

	// 4. Create WLAN setting
	WifiHelper wifi;
	wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);
	wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs7"), "ControlMode", StringValue ("HtMcs0"));

	// 5. Create NetDevices
	NetDeviceContainer staDevice;
	staDevice = wifi.Install (phy, mac, wifiStaNode);		// To-do #2		Install Station device

	mac.SetType ("ns3::ApWifiMac",
			"Ssid", SsidValue (ssid),
			"BeaconInterval", TimeValue (MicroSeconds (102400)),
			"BeaconGeneration", BooleanValue (true));

	NetDeviceContainer apDevice;
	apDevice = wifi.Install (phy, mac, wifiApNode);

	// 6. Create Network layer
	/* Internet stack*/
	InternetStackHelper stack;
	stack.Install (wifiApNode);
	stack.Install (wifiStaNode);

	Ipv4AddressHelper address;

	address.SetBase ("192.168.1.0", "255.255.255.0");
	Ipv4InterfaceContainer StaInterface;
	StaInterface = address.Assign (staDevice);
	Ipv4InterfaceContainer ApInterface;
	ApInterface = address.Assign (apDevice);

	// 7. Locate nodes
	/* Setting mobility model */
	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

	positionAlloc->Add (Vector (0.0, 0.0, 0.0));			// To-do #3.	Add vector x=0, y=0, z=0
	positionAlloc->Add (Vector (1.0, 0.0, 0.0));			// 						Add vector x=1, y=0, z=0
	mobility.SetPositionAllocator (positionAlloc);

	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

	mobility.Install (wifiApNode);
	mobility.Install (wifiStaNode);

	// 8. Create Transport layer (UDP)
	/* Setting applications */
	UdpServerHelper myServer (9);
	ApplicationContainer serverApp = myServer.Install (wifiStaNode);		// To-do #4.	Install server app to the station node
	serverApp.Start (Seconds (0.0));
	serverApp.Stop (Seconds (simulationTime + 1));

	UdpClientHelper myClient (StaInterface.GetAddress (0), 9);
	myClient.SetAttribute ("MaxPackets", UintegerValue (4294967295));	// Maximum number of packets = 2^32
	myClient.SetAttribute ("Interval", TimeValue (Time ("0.00002"))); //packets/s
	myClient.SetAttribute ("PacketSize", UintegerValue (payloadSize));

	ApplicationContainer clientApp = myClient.Install (wifiApNode.Get (0));
	clientApp.Start (Seconds (1.0));
	clientApp.Stop (Seconds (simulationTime + 1));

	AnimationInterface anim ("wlan.xml");

	// 9. Simulation Run and Calc. throughput
	Simulator::Stop (Seconds (simulationTime + 1));
	Simulator::Run ();
	Simulator::Destroy ();

	uint32_t totalPacketsRecv = DynamicCast<UdpServer> (serverApp.Get (0))->GetReceived ();
	double throughput = totalPacketsRecv * payloadSize * 8 / (simulationTime * 1000000.0);
	std::cout << "Throughput: " << throughput << " Mbps" << '\n';

	return 0;
}

