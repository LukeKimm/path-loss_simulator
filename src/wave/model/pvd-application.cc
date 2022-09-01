/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 North Carolina State University
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
 * Author: Scott E. Carpenter <scarpen@ncsu.edu>
 *
 */

#include "ns3/pvd-application.h"
#include "ns3/log.h"
#include "ns3/wave-net-device.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/mobility-helper.h"

NS_LOG_COMPONENT_DEFINE ("PvdApplication");

namespace ns3 {

// (Arbitrary) port for establishing socket to transmit WAVE PVDs
// 포트 수정이 필요할까?
int PvdApplication::wavePort = 9080;

NS_OBJECT_ENSURE_REGISTERED (PvdApplication);

TypeId
PvdApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PvdApplication")
    .SetParent<Application> ()
    .SetGroupName ("Wave")
    .AddConstructor<PvdApplication> ()
    ;
  return tid;
}

// pvd message의 세부 내용 구성
PvdApplication::PvdApplication ()
  : m_wavePvdStats (0),
    // m_txSafetyRangesSq (), // safety 내용은 pvd message에는 필요하지 않으므로 제거
    m_TotalSimTime (Seconds (10)), // 총 simulation time은 환경에 따라서 값 수정 가능
    m_wavePacketSize (448), // packet size: 기존에 자료조사된 논문의 내용을 기반으로 수정
    m_numWavePackets (1), // packet number: 1 packet per 1 second
    m_waveInterval (MilliSeconds (1000)), // packet interval: send every 1 second
    m_gpsAccuracyNs (10000),
    m_adhocTxInterfaces (0),
    // m_nodesMoving (0), // node의 움직임은 pvd message상에서 필요없으므로 제거
    m_unirv (0), // gps sync를 위해서 필요한 값
    m_nodeId (0),
    m_chAccessMode (0),
    m_txMaxDelay (MilliSeconds (10)),
    m_prevTxDelay (MilliSeconds (0))
{
  NS_LOG_FUNCTION (this);
}

PvdApplication::~PvdApplication ()
{
  NS_LOG_FUNCTION (this);
}

void
PvdApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  // chain up
  Application::DoDispose ();
}

// Application Methods
void PvdApplication::StartApplication () // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);

  // setup generation of WAVE PVD messages
  Time waveInterPacketInterval = m_waveInterval;

  // PVDs are not transmitted for the first second
  Time startTime = Seconds (1.0);
  // total length of time transmitting WAVE packets
  Time totalTxTime = m_TotalSimTime - startTime;
  // total WAVE packets needing to be sent
  m_numWavePackets = (uint32_t) (totalTxTime.GetDouble () / m_waveInterval.GetDouble ());

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");

  // every node broadcasts WAVE PVD to potentially all other nodes
  Ptr<Socket> recvSink = Socket::CreateSocket (GetNode (m_nodeId), tid);
  recvSink->SetRecvCallback (MakeCallback (&PvdApplication::ReceiveWavePacket, this));
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), wavePort);
  recvSink->BindToNetDevice (GetNetDevice (m_nodeId));
  recvSink->Bind (local);
  recvSink->SetAllowBroadcast (true);

  // dest is broadcast address
  InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), wavePort);
  recvSink->Connect (remote);

  // Transmission start time for each PVD:
  // We assume that the start transmission time
  // for the first packet will be on a ns-3 time
  // "Second" boundary - e.g., 1.0 s.
  // However, the actual transmit time must reflect
  // additional effects of 1) clock drift and
  // 2) transmit delay requirements.
  // 1) Clock drift - clocks are not perfectly
  // synchronized across all nodes.  In a VANET
  // we assume all nodes sync to GPS time, which
  // itself is assumed  accurate to, say, 40-100 ns.
  // Thus, the start transmission time must be adjusted
  // by some value, t_drift.
  // 2) Transmit delay requirements - The US
  // minimum performance requirements for V2V
  // PVD transmission expect a random delay of
  // +/- 5 ms, to avoid simultaneous transmissions
  // by all vehicles congesting the channel.  Thus,
  // we need to adjust the start transmission time by
  // some value, t_tx_delay.
  // Therefore, the actual transmit time should be:
  // t_start = t_time + t_drift + t_tx_delay
  // t_drift is always added to t_time.
  // t_tx_delay is supposed to be +/- 5ms, but if we
  // allow negative numbers the time could drift to a value
  // BEFORE the interval start time (i.e., at 100 ms
  // boundaries, we do not want to drift into the
  // previous interval, such as at 95 ms.  Instead,
  // we always want to be at the 100 ms interval boundary,
  // plus [0..10] ms tx delay.
  // Thus, the average t_tx_delay will be
  // within the desired range of [0..10] ms of
  // (t_time + t_drift)

  // WAVE devices sync to GPS time
  // and all devices would like to begin broadcasting
  // their safety messages immediately at the start of
  // the CCH interval.  However, if all do so, then
  // significant collisions occur.  Thus, we assume there
  // is some GPS sync accuracy on GPS devices,
  // typically 40-100 ns.
  // Get a uniformly random number for GPS sync accuracy, in ns.
  Time tDrift = NanoSeconds (m_unirv->GetInteger (0, m_gpsAccuracyNs));

  // When transmitting at a default rate of 10 Hz,
  // the subsystem shall transmit every 100 ms +/-
  // a random value between 0 and 5 ms. [MPR-PVDTX-TXTIM-002]
  // Source: CAMP Vehicle Safety Communications 4 Consortium
  // On-board Minimum Performance Requirements
  // for V2V Safety Systems Version 1.0, December 17, 2014
  // max transmit delay (default 10ms)
  // get value for transmit delay, as number of ns
  uint32_t d_ns = static_cast<uint32_t> (m_txMaxDelay.GetInteger ());
  // convert random tx delay to ns-3 time
  // see note above regarding centering tx delay
  // offset by 5ms + a random value.
  Time txDelay = NanoSeconds (m_unirv->GetInteger (0, d_ns));
  m_prevTxDelay = txDelay;

  Time txTime = startTime + tDrift + txDelay;
  // schedule transmission of first packet
  Simulator::ScheduleWithContext (recvSink->GetNode ()->GetId (),
                                  txTime, &PvdApplication::GenerateWaveTraffic, this,
                                  recvSink, m_wavePacketSize, m_numWavePackets, waveInterPacketInterval, m_nodeId);
}

void PvdApplication::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);
}

void
PvdApplication::Setup (Ipv4InterfaceContainer & i,
                       int nodeId,
                       Time totalTime,
                       uint32_t wavePacketSize, // bytes
                       Time waveInterval,
                       double gpsAccuracyNs,
                       std::vector <double> rangesSq,           // m ^2
                       Ptr<WavePvdStats> wavePvdStats,
                       std::vector<int> * nodesMoving,
                       int chAccessMode,
                       Time txMaxDelay)
{
  NS_LOG_FUNCTION (this);

  m_unirv = CreateObject<UniformRandomVariable> ();

  m_TotalSimTime = totalTime;
  m_wavePacketSize = wavePacketSize;
  m_waveInterval = waveInterval;
  m_gpsAccuracyNs = gpsAccuracyNs;
  int size = rangesSq.size ();
  m_wavePvdStats = wavePvdStats;
  m_nodesMoving = nodesMoving;
  m_chAccessMode = chAccessMode;
  m_txSafetyRangesSq.clear ();
  m_txSafetyRangesSq.resize (size, 0);

  for (int index = 0; index < size; index++)
    {
      // stored as square of value, for optimization
      m_txSafetyRangesSq[index] = rangesSq[index];
    }

  m_adhocTxInterfaces = &i;
  m_nodeId = nodeId;
  m_txMaxDelay = txMaxDelay;
}

void
PvdApplication::GenerateWaveTraffic (Ptr<Socket> socket, uint32_t pktSize,
                                     uint32_t pktCount, Time pktInterval,
                                     uint32_t sendingNodeId)
{
  NS_LOG_FUNCTION (this);

  // more packets to send?
  if (pktCount > 0)
    {
      // for now, we cannot tell if each node has
      // started mobility.  so, as an optimization
      // only send if  this node is moving
      // if not, then skip
      int txNodeId = sendingNodeId;
      Ptr<Node> txNode = GetNode (txNodeId);
      Ptr<MobilityModel> txPosition = txNode->GetObject<MobilityModel> ();
      NS_ASSERT (txPosition != 0);

      int senderMoving = m_nodesMoving->at (txNodeId);
      if (senderMoving != 0)
        {
          // send it!
          socket->Send (Create<Packet> (pktSize));
          // count it
          m_wavePvdStats->IncTxPktCount ();
          m_wavePvdStats->IncTxByteCount (pktSize);
          int wavePktsSent = m_wavePvdStats->GetTxPktCount ();
          if ((m_wavePvdStats->GetLogging () != 0) && ((wavePktsSent % 1000) == 0))
            {
              NS_LOG_UNCOND ("Sending WAVE pkt # " << wavePktsSent );
            }

          // find other nodes within range that would be
          // expected to receive this broadbast
          int nRxNodes = m_adhocTxInterfaces->GetN ();
          for (int i = 0; i < nRxNodes; i++)
            {
              Ptr<Node> rxNode = GetNode (i);
              int rxNodeId = rxNode->GetId ();

              if (rxNodeId != txNodeId)
                {
                  Ptr<MobilityModel> rxPosition = rxNode->GetObject<MobilityModel> ();
                  NS_ASSERT (rxPosition != 0);
                  // confirm that the receiving node
                  // has also started moving in the scenario
                  // if it has not started moving, then
                  // it is not a candidate to receive a packet
                  int receiverMoving = m_nodesMoving->at (rxNodeId);
                  if (receiverMoving == 1)
                    {
                      double distSq = MobilityHelper::GetDistanceSquaredBetween (txNode, rxNode);
                      if (distSq > 0.0)
                        {
                          // dest node within range?
                          int rangeCount = m_txSafetyRangesSq.size ();
                          for (int index = 1; index <= rangeCount; index++)
                            {
                              if (distSq <= m_txSafetyRangesSq[index - 1])
                                {
                                  // we should expect dest node to receive broadcast pkt
                                  m_wavePvdStats->IncExpectedRxPktCount (index);
                                }
                            }
                        }
                    }
                }
            }
        }

      // every PVD must be scheduled with a tx time delay
      // of +/- (5) ms.  See comments in StartApplication().
      // we handle this as a tx delay of [0..10] ms
      // from the start of the pktInterval boundary
      uint32_t d_ns = static_cast<uint32_t> (m_txMaxDelay.GetInteger ());
      Time txDelay = NanoSeconds (m_unirv->GetInteger (0, d_ns));

      // do not want the tx delay to be cumulative, so
      // deduct the previous delay value.  thus we adjust
      // to schedule the next event at the next pktInterval,
      // plus some new [0..10] ms tx delay
      Time txTime = pktInterval - m_prevTxDelay + txDelay;
      m_prevTxDelay = txDelay;

      Simulator::ScheduleWithContext (socket->GetNode ()->GetId (),
                                      txTime, &PvdApplication::GenerateWaveTraffic, this,
                                      socket, pktSize, pktCount - 1, pktInterval,  socket->GetNode ()->GetId ());
    }
  else
    {
      socket->Close ();
    }
}

void PvdApplication::ReceiveWavePacket (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this);

  Ptr<Packet> packet;
  Address senderAddr;
  while ((packet = socket->RecvFrom (senderAddr)))
    {
      Ptr<Node> rxNode = socket->GetNode ();

      if (InetSocketAddress::IsMatchingType (senderAddr))
        {
          InetSocketAddress addr = InetSocketAddress::ConvertFrom (senderAddr);
          int nodes = m_adhocTxInterfaces->GetN ();
          for (int i = 0; i < nodes; i++)
            {
              if (addr.GetIpv4 () == m_adhocTxInterfaces->GetAddress (i) )
                {
                  Ptr<Node> txNode = GetNode (i);
                  HandleReceivedPvdPacket (txNode, rxNode);
                }
            }
        }
    }
}

void PvdApplication::HandleReceivedPvdPacket (Ptr<Node> txNode,
                                              Ptr<Node> rxNode)
{
  NS_LOG_FUNCTION (this);

  m_wavePvdStats->IncRxPktCount ();

  Ptr<MobilityModel> rxPosition = rxNode->GetObject<MobilityModel> ();
  NS_ASSERT (rxPosition != 0);
  // confirm that the receiving node
  // has also started moving in the scenario
  // if it has not started moving, then
  // it is not a candidate to receive a packet
  int rxNodeId = rxNode->GetId ();
  int receiverMoving = m_nodesMoving->at (rxNodeId);
  if (receiverMoving == 1)
    {
      double rxDistSq = MobilityHelper::GetDistanceSquaredBetween (rxNode, txNode);
      if (rxDistSq > 0.0)
        {
          int rangeCount = m_txSafetyRangesSq.size ();
          for (int index = 1; index <= rangeCount; index++)
            {
              if (rxDistSq <= m_txSafetyRangesSq[index - 1])
                {
                  m_wavePvdStats->IncRxPktInRangeCount (index);
                }
            }
        }
    }
}

int64_t
PvdApplication::AssignStreams (int64_t streamIndex)
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT (m_unirv);  // should be set by Setup() prevoiusly
  m_unirv->SetStream (streamIndex);

  return 1;
}

Ptr<Node>
PvdApplication::GetNode (int id)
{
  NS_LOG_FUNCTION (this);

  std::pair<Ptr<Ipv4>, uint32_t> interface = m_adhocTxInterfaces->Get (id);
  Ptr<Ipv4> pp = interface.first;
  Ptr<Node> node = pp->GetObject<Node> ();

  return node;
}

Ptr<NetDevice>
PvdApplication::GetNetDevice (int id)
{
  NS_LOG_FUNCTION (this);

  std::pair<Ptr<Ipv4>, uint32_t> interface = m_adhocTxInterfaces->Get (id);
  Ptr<Ipv4> pp = interface.first;
  Ptr<NetDevice> device = pp->GetObject<NetDevice> ();

  return device;
}

void PvdApplication::ifCCAbusy(uint32_t nodeID)
{
   Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice>(GetNode(nodeID)->GetDevice(0));
   // more than 1 device can be at each node so take the 1st one

   Ptr<WifiPhy> phy = device->GetPhy ();

   Ptr <YansWifiPhy> wfc = phy->GetObject<YansWifiPhy> ();

   // Ptr <WifiPhyStateHelper> statehelper = phy->GetState();

   PointerValue ptr;
   wfc->GetAttribute("State", ptr);
   Ptr<WifiPhyStateHelper> wpsh = ptr.Get<WifiPhyStateHelper>();

   std::cerr<<"CCA state for node "<<nodeID<<" is "<<wpsh->IsStateCcaBusy()()<<std::endl;

   if (wpsh->IsStateCcaBusy() == true )
   {
     CBRTime[nodeID].push_back(1);
   }
   else
   {
     CBRTime[nodeID].push_back(0);
   }

   CBRreviewInstants[nodeID] = Simulator::Schedule (//recvSink->GetNode ()->GetId (), //BIPLAV
                                     CBRcheckInterval, &PvdApplication::ifCCAbusy, this,
                                     nodeID);
}

} // namespace ns3
