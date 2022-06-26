#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

//ns3는 C++의 namespace에 있다. 이를 불러오는것
using namespace ns3;

// 로그를 출력하기 위해서 기본적으로 정의해주는것
NS_LOG_COMPONENT_DEFINE ("FirstScriptExample");

int 
main (int argc, char *argv[])
{
    CommandLine cmd (__FILE__);
    cmd.Parse (argc, argv);

    // 이 시나리오에서 호스트가 실행하는 어플리케이션은 UdpEchoClientApplication, UdpEchoserverApplication 두가지이다. 
    // 각각의 어플리케이션에서 어느 레벨로 로그를 출력할 것인지 설정해주는 코드
    Time::SetResolution (Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoserverApplication", LOG_LEVEL_INFO);

    // void Create (uint32_t n): creates n nodes, n: # of Nodes
    // void Add (Ptr<Node> node): append a single node
    // Ptr<Node> Get (uint32_t i): returns i th node, i: noe index
    // uint32_t GetN (void): returns total number of nodes

    // 두개의 노드를 생성
    NodeContainer nodes;
    nodes.Create(2);

    // void SetDeviceAttribute (name, value)
    // void SetChannelAttribute (name, value)
    // NetDeviceContainer Install (NodeContainer c)


    // 두개의 노드를 연결해 줄 P2P링크를 만든다. 
    // 이때 링크의 속도와 딜레이를 설정해 줄 수 있다.
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    pointToPoint.SetDeviceAttribute ("Delay", StringValue ("2ms"));

    // void Add (Ptr<NetDevice> device): append a single device
    // Ptr<NetDevice> GetN (void): returns i th netdevice
    // uint32_t GetN (void): returns total number of net devices


    // NetDevice는 NIC(network interface card)와 동일한 역할을 한다. 
    NetDeviceContainer devices;
    devices = pointToPoint.Install (nodes);

    // void Install (NodeContainer c)


    // InternetStack은 TCP/IP 레이어와 같은 역할을 한다. 
    InternetStackHelper stack;
    stack.Install (nodes);

    // void SetBase (Ipv4Address network, Ipv4Mask mask, Ipv4Address base = "0.0.0.1")
    // Ipv4InterfaceContainer Assign (const NetDeviceContainer &c)


    // P2P링크를 노드에 연결하면 노드의 NIC가 반환된다. 
    // 이렇게 반환된 NIC에 ip address를 설정해 줄 수 있다.
    // address.SetBase는 subnet id와 subnet mask를 주면 자동으로 NIC에 아이피를 할당해준다. 
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Ptr<Application> Get (uint32_t i): returns i th application
    // uint32_t GetN (void): returns total number of net devices 
    // void Start (Time start): all applications start at start at start time
    // void Stop (Time stop): all applications stop at stop time


    // 노드에 설치할 서버 애플리케이션을 설정해준다.
    // 서버의 포트번호는 9번을 사용한다. 
    UdpEchoServerHelper echoServer (9);

    ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
    serverApps.Start (Seconds (1.0)); // 서버 어플리케이션은 1초에 시작
    serverApps.Stop (Seconds (10.0)); // 10초에 종료한다. 


    // 노드에 설치할 클라이언트 어플리케이션을 설정해준다. 
    // 이때 서버의 ip주소와 포트번호를 넘겨준다. 
    UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9); // ip주소를 NIC에 할당할 때 interface 객체가 생기고, 이 객체는 할당된 아이피 주소의 정보를 가지고 있다. 이 객체를 이용하여 서버의 ip주소를 알 수 있다.
    echoClient.SetAttribute ("MaxPackets", UintegerValue (1)); // 서버에 보내는 패킷의 갯수를 1개로 설정
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0))); // 서버에 패킷을 보내는 간격을 1로 지정
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024)); // 서버로 보내지는 패킷의 크기는 1024바이트로 지정
    

    // 위처럼 만들어진 클라이언트 어플리케이션을 첫번째 노드에 설치
    // 클라이언트 어플리케이션은 2초부터 시작하여 10초에 종료한다. 
    ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Start (Seconds (10.0));


    // 위에서 설정한 시뮬레이션을 실행하고 시뮬레이션이 종료되면 프로그램을 종료한다. 
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}