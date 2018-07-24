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
 *
 */

/* Test program for multi-interface host, static routing, 
 * 
 * 
 * Single source!

         Destination host (10.20.1.2)
                 |
                 | 10.20.1.0/24             (Core link: 50Mbps)
              DSTRTR 
  10.10.1.0/24 /   \  10.10.2.0/24          (Backbone link: 30Mbps)
              / \
           Rtr1    Rtr2
 10.1.1.0/24 |      | 10.1.2.0/24
             |      /
              \    /
             Source
 */

#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <stdlib.h>
#include <stdbool.h>  //for rand()

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/wifi-module.h"
#include "src/core/model/log.h"
#include "ns3/flow-monitor-module.h"
#include "src/core/model/double.h"
#include "src/core/model/pointer.h"
#include "ns3/lte-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/lte-module.h"
#include "src/wifi/helper/yans-wifi-helper.h"
#include "ns3/wave-helper.h"
#include "src/core/model/int64x64-128.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SocketBoundTcpRoutingExample");

static const uint32_t totalTxBytes = 2000000000;
static uint32_t currentTxBytes = 0;
static const uint32_t writeSize = 1040;
uint8_t data[writeSize];
static int packetSize = 520;
//static int mssSize = 630;

Ptr<Socket> Socket1;
Ptr<Socket> Socket2;

Ptr<Socket> Socket3;
Ptr<Socket> Socket4;


std::string rttFileName = "rtt1.txt"; //file for logging rtt values
std::string rttFileName1 = "rtt2.txt";
Time RttValue;

static int schedule_decider = 0;

/*** Values for RTT Scheduler */
static float RttSocket1old = 0;
static float RttSocket2old = 0;
static const float alpha = 0.8;
static float SRttSocket1 = 0;
static float SRttSocket2 = 0;
/* Values end here */

/***Values for Queue 
 ***/
Ptr<QueueDisc> qDiscRTR1;
Ptr<QueueDisc> qDiscSrcRTR2;
static uint32_t queueSizeRTR1 = 0;
static uint32_t queueSizeRTR2 = 0;
static float estServiceRTR1 = 0;
static float estServiceRTR2 = 0;

static double dequeueold1 = 0;
static double TS_obs1 = 0;
static double TS_avg1 = 0;
static double ToA1 = 0;
static uint32_t num_dequeue1 = 0;
static uint32_t dequeue_count1 = 0;
static float dequeueold2 = 0;
static float TS_obs2 = 0;
static float TS_avg2 = 0;
static double ToA2 = 0;
static uint32_t num_dequeue2 = 0;
static uint32_t dequeue_count2 = 0;
static uint32_t toaCounter1 = 0;
static uint32_t toaCounter2 = 0;
static uint32_t enqueue_count1 = 0;
static uint32_t enqueue_count2 = 0;

static double queueStore1[20][2]; //queueStore[Time][queueSize]
static int lastQueueIndex1;
static int lastToAIndex1 = 20;
static double queueStore2[20][2]; //queueStore[Time][queueSize]
static int lastQueueIndex2;
static int lastToAIndex2 = 20;

/***  Values end here  ***/

/**Congestion Window availability Checker*/
static uint32_t cwnd1 = 0;
static uint32_t cwnd2 = 0;
static uint32_t bif1 = 0;
static uint32_t bif2 = 0;
static bool cwndAvailable1 = true;
static bool cwndAvailable2 = true;
/**Values end here**/

AsciiTraceHelper ascii;
Ptr<OutputStreamWrapper> rttStream = ascii.CreateFileStream(rttFileName.c_str());
Ptr<OutputStreamWrapper> rttStream1 = ascii.CreateFileStream(rttFileName1.c_str());

std::ofstream fileQueueSize("queuesize_socketbound.txt", std::ios::app);
std::ofstream fileCwndSrc("cwndsize_socketbound.txt", std::ios::app);
std::ofstream filerttf1("RTT_Flow1.txt", std::ios::app);
std::ofstream filerttf2("RTT_Flow2.txt", std::ios::app);
std::ofstream filetoa1("TimeOnAir1.txt", std::ios::app);
std::ofstream filetoa2("TimeOnAir2.txt", std::ios::app);
std::ofstream fileeservice1("ServiceTime1.txt", std::ios::app);
std::ofstream fileeservice2("ServiceTime2.txt", std::ios::app);

std::ofstream fileenqueue1("Enqueue_Flow1.txt", std::ios::app);
std::ofstream fileenqueue2("Enqueue_Flow2.txt", std::ios::app);
std::ofstream filedequeue("Dequeue.txt", std::ios::app);
//std::ofstream fileservice("ServiceTime1.txt", std::ios::app);
//std::ofstream fileservice1("ServiceTime2.txt", std::ios::app);
//std::ofstream filewait("WaitTime1.txt", std::ios::app);
std::ofstream filewait("SendTimeFromAck.txt", std::ios::app);
std::ofstream filepacketsent1("packetsent_flow1.txt", std::ios::app);
std::ofstream filepacketsent2("packetsent_flow2.txt", std::ios::app);

static int GetIndexValue(double time) {
    int index = 2 * (static_cast<int> (time * 10) % 10);
    if ((static_cast<int> (time * 100) % 10) >= 5) {
        index++;
    }
    return index;
}

static void CalculateAirTime1(double time, double rtt) {
    double packetSendTime = time - rtt;
    int index = GetIndexValue(packetSendTime);
    int packetsInQueue = queueStore1[index][1];
    double waitTime = packetsInQueue * TS_avg1;
//    filewait << packetSendTime << "\t" << waitTime << std::endl;
    double timeonair = rtt - waitTime;
    double airtime;
    //if (index != lastToAIndex1 && timeonair>0) // only changing the time on air value when using new queue value, considering all packets to have same time on air in 0.5 sec period (consider changing with counter)
    if (toaCounter1==0 && timeonair>0)
    {
        ToA1 = (double)alpha * ToA1;
        airtime = (double)(1-alpha)*timeonair;
        ToA1 = ToA1 + airtime;      // Taking weighted average
        lastToAIndex1 = index;
    }
    toaCounter1 = (toaCounter1+1)%20;      //log the value after every x acks
    filetoa1 << time << "\t" << ToA1 << std::endl;
}

static void CalculateAirTime2(double time, double rtt) {
    double packetSendTime = time - rtt;
    int index = GetIndexValue(packetSendTime);
    int packetsInQueue = queueStore2[index][1];
    double waitTime = packetsInQueue * TS_avg2;
//    filewait1 << packetSendTime << "\t" << waitTime << std::endl;
    double timeonair = rtt - waitTime;
    double airtime;
    //if (index != lastToAIndex2  && timeonair>0) // only changing the time on air value when using new queue value, considering all packets to have same time on air in 0.5 sec period (consider changing with counter)
    if (toaCounter2==0 && timeonair>0)
    {
        ToA2 = (double)alpha * ToA2;
        airtime = (double)(1-alpha)*timeonair;
        ToA2 = ToA2 + airtime;      // Taking weighted average
        lastToAIndex2 = index;
    }
    toaCounter2 = (toaCounter2+1)%20;      //log the value after every x acks
    filetoa2<<time<<"\t"<<ToA2<<std::endl;
}

static void CalculateSRttS1(Time newRtt, int SocketID) {
    float srtt, srtt1;
    if (SRttSocket1 == 0) {
        SRttSocket1 = (float) (newRtt.GetSeconds());
    } else {
        srtt = alpha*SRttSocket1;
        srtt1 = (1 - alpha)*(float) (newRtt.GetSeconds());
        srtt1 = srtt + srtt1;
        SRttSocket1 = srtt1;
    }
    return;
}

static void CalculateSRttS2(Time newRtt, int SocketID) {
    float srtt2, srtt3;
    if (SRttSocket2 == 0) {
        SRttSocket2 = (float) (newRtt.GetSeconds());
    } else {
        srtt2 = alpha*SRttSocket2;
        srtt3 = (1 - alpha)*(float) (newRtt.GetSeconds());
        srtt3 = srtt2 + srtt3;
        SRttSocket2 = srtt3;
    }
    //std::cout<<Simulator::Now().GetSeconds() <<"     "<<SRttSocket1<<"       "<<SRttSocket2<<std::endl;
    return;
}

static void CalculateEServiceTimeRTR1() {
    //estServiceRTR1 = (float) (queueSizeRTR1 + 1) * SRttSocket1;
    estServiceRTR1 = (float) (queueSizeRTR1 + 1) * ToA1; // decouple srtt into wait time and time on air
    fileeservice1<<Simulator::Now().GetSeconds()<< "\t" << estServiceRTR1 << std::endl;
}

static void CalculateEServiceTimeRTR2() {
    //estServiceRTR2 = (float) (queueSizeRTR2 + 1) * SRttSocket2;
    estServiceRTR2 = (float) (queueSizeRTR2 + 1) * ToA2; //Time on Air as metric
    fileeservice2<<Simulator::Now().GetSeconds()<< "\t" << estServiceRTR2 << std::endl;
}

static void
RttTracer1(Ptr<OutputStreamWrapper> stream, Time oldval, Time newval) {
    int socketID = 1;
    double time = Simulator::Now().GetSeconds();
    if (RttSocket1old != (float) (newval.GetSeconds())) {
        Simulator::ScheduleNow(&CalculateSRttS1, newval, socketID);
        Simulator::ScheduleNow(&CalculateAirTime1, time, newval.GetSeconds());
        filerttf1 << time << "\t" << RttSocket1old << "\t" << (float) (newval.GetSeconds()) << "\t" << SRttSocket1 << std::endl;
        RttSocket1old = (float) (newval.GetSeconds());
    }
    //*stream->GetStream() << Simulator::Now().GetSeconds() << " " << oldval.GetSeconds() << " " << newval.GetSeconds() << "   "<<std::endl;
    RttValue = newval;
}

static void
RttTracer2(Ptr<OutputStreamWrapper> stream, Time oldval, Time newval) {
    int socketID = 2;
    double time = Simulator::Now().GetSeconds();
    if (RttSocket2old != (float) (newval.GetSeconds())) {
        Simulator::ScheduleNow(&CalculateSRttS2, newval, socketID);
        Simulator::ScheduleNow(&CalculateAirTime2, time, newval.GetSeconds());
        filerttf2 << Simulator::Now().GetSeconds() << "\t" << RttSocket2old << "\t" << (float) (newval.GetSeconds()) << "\t" << SRttSocket2 << std::endl;
        RttSocket2old = (float) (newval.GetSeconds());
    }
    //float Srtt = CalculateSRtt(newval, 2);
    //*stream->GetStream() << Simulator::Now().GetSeconds() << " " << oldval.GetSeconds() << " " << newval.GetSeconds() << " " << std::endl;//Srtt.GetSeconds() << std::endl;
    RttValue = newval;
}

static void
TraceRtt0(std::string rtt_file_name) {
    //AsciiTraceHelper ascii;
    //Ptr<OutputStreamWrapper> rttStream = ascii.CreateFileStream(rtt_file_name.c_str());
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/RTT", MakeBoundCallback(&RttTracer1, rttStream)); //Tap into RTT for socket 
    //std::cout<<Simulator::Now().GetSeconds() <<"     "<<SRttSocket1<<"       "<<SRtSocket2<<std::endl;
}

static void
TraceRtt1(std::string rtt_file_name) {
    //AsciiTraceHelper ascii;
    //Ptr<OutputStreamWrapper> rttStream = ascii.CreateFileStream(rtt_file_name.c_str());
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/1/RTT", MakeBoundCallback(&RttTracer2, rttStream1)); //Tap into RTT of socket 2
}

static void ServiceTime1() {
    double TS_sum;
    if (TS_avg1 == 0) {
        TS_avg1 = TS_obs1;
        ++num_dequeue1;
    } else if (TS_obs1 > 0.005) { // Assuming that the packet has arrived in the queue after a gap; Change this value if you change last mile parameter
        num_dequeue1 = 1; //Take past average value as defacto and reset counter
    } else {
//        if (num_dequeue1 > 100) {
//            num_dequeue1 = 1;
//        }
        TS_sum = TS_avg1 * (double) (num_dequeue1);
        TS_sum = TS_sum + TS_obs1;
        TS_avg1 = TS_sum / (double) (++num_dequeue1);
    }
    //    fileservice << Simulator::Now().GetSeconds() << "\t" << TS_obs1 << "\t" << num_dequeue1 << "\t" << TS_avg1 << std::endl;
}

static void ServiceTime2() {
    double TS_sum;
    if (TS_avg2 == 0) {
        TS_avg2 = TS_obs2;
        ++num_dequeue2;
    } else if (TS_obs2 > 0.005) { // Assuming that the packet has arrived in the queue after a gap; Change this value if you change last mile parameter
        num_dequeue2 = 1; //Take past average value as defacto and reset counter
    } else {
//        if (num_dequeue2 > 100) {
//            num_dequeue2 = 1;
//        }
        TS_sum = TS_avg2 * (double) (num_dequeue2);
        TS_sum = TS_sum + TS_obs2;
        TS_avg2 = TS_sum / (double) (++num_dequeue2);
    }
    //    fileservice1 << Simulator::Now().GetSeconds() << "\t" << TS_obs2 << "\t" << num_dequeue2 << "\t" << TS_avg2 << std::endl;
}

static void DequeueTracer1(Ptr<const WifiMacQueueItem> dequeue) {
    double dequeue_time = Simulator::Now().GetSeconds();
    dequeue_count1 = dequeue_count1 + 1;
    TS_obs1 = dequeue_time - dequeueold1;
    dequeueold1 = dequeue_time;
    //filedequeue << dequeue_time << "\t"<< dequeue_count1 << std::endl;
    Simulator::ScheduleNow(&ServiceTime1);
}

static void DequeueTracer2(Ptr<const WifiMacQueueItem> dequeue) {
    double dequeue_time = Simulator::Now().GetSeconds();
    dequeue_count2 = dequeue_count2 + 1;
    TS_obs2 = dequeue_time - dequeueold2;
    dequeueold2 = dequeue_time;
    filedequeue << dequeue_time << "\t"<< dequeue_count1 << std::endl;
    Simulator::ScheduleNow(&ServiceTime2);
}

static void EnqueueTracer1(Ptr<const WifiMacQueueItem> enqueue)
{
    double enqueue_time = Simulator::Now().GetSeconds();
    fileenqueue1 << enqueue_time << "\t"<< ++enqueue_count1 << std::endl;
}

static void EnqueueTracer2(Ptr<const WifiMacQueueItem> enqueue)
{
    double enqueue_time1 = Simulator::Now().GetSeconds();
    fileenqueue2 << enqueue_time1 << "\t"<< ++enqueue_count2 << std::endl;
}

static void CwndTracer1(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd) {
    cwnd1 = newCwnd;
}

static void CwndTracer2(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd) {
    cwnd2 = newCwnd;
}

static void ByteinFlightTracer1(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
    bif1 = newVal;
}

static void ByteinFlightTracer2(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal) {
    bif2 = newVal;
}

static void CongWindowAvailabilityFlow1() {
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeBoundCallback(&CwndTracer1, rttStream));
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/BytesInFlight", MakeBoundCallback(&ByteinFlightTracer1, rttStream1));

    if (cwnd1 - bif1 > (uint32_t)packetSize || cwnd1 == 0 || bif1 == 0) {
        cwndAvailable1 = true;
    } else {
        cwndAvailable1 = false;
    }
}

static void CongWindowAvailabilityFlow2() {
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/1/CongestionWindow", MakeBoundCallback(&CwndTracer2, rttStream));
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/1/BytesInFlight", MakeBoundCallback(&ByteinFlightTracer2, rttStream1));

    if (cwnd2 - bif2 > (uint32_t)packetSize || cwnd2 == 0 || bif2 == 0) {
        cwndAvailable2 = true;
    } else {
        cwndAvailable2 = false;
    }
}

/*static void CheckQueueSize(Ptr<QueueDisc> queue1, Ptr<QueueDisc> queue2) {      // Use this method if you are using Queue Discs
    uint32_t qSize1 = queue1->GetNPackets();
    uint32_t qSize2 = queue2->GetNPackets();
    fileQueueSize << Simulator::Now().GetSeconds() << " " << qSize1 << "    " << qSize2 << std::endl;
    queueSizeRTR1 = qSize1;
    queueSizeRTR2 = qSize2;
}*/

//static void TcPacketsInQueue(Ptr<OutputStreamWrapper> stream, uint32_t oldValue, uint32_t newValue)
//{
//    std::cout << Simulator::Now().GetSeconds() << "     "<< oldValue << "  to   " << newValue <<std::endl;
//}

static void DevicePacketsInQueueRTR1(uint32_t oldValue, uint32_t newValue) {
    queueSizeRTR1 = newValue;
    double time = Simulator::Now().GetSeconds();
    int index = GetIndexValue(time);
    if ((lastQueueIndex1 != index) || (queueStore1[index][0] == 0 && queueSizeRTR1 != 0)) {
        queueStore1[index][0] = time;
        queueStore1[index][1] = queueSizeRTR1;
        lastQueueIndex1 = index;
    }
    //std::cout << Simulator::Now().GetSeconds() << "     "<< oldValue << "   to   " << newValue <<std::endl;
}

static void DevicePacketsInQueueRTR2(uint32_t oldValue, uint32_t newValue) {
    queueSizeRTR2 = newValue;
    double time = Simulator::Now().GetSeconds();
    int index = GetIndexValue(time);
    if ((lastQueueIndex2 != index) || (queueStore2[index][0] == 0 && queueSizeRTR2 != 0)) {
        queueStore2[index][0] = time;
        queueStore2[index][1] = queueSizeRTR2;
        lastQueueIndex2 = index;
    }
    // std::cout << Simulator::Now().GetSeconds() << "     "<< oldValue << "   to   " << newValue <<std::endl;
}

void LogValues() {
    fileCwndSrc << Simulator::Now().GetSeconds() << "\t" << cwnd1 << "\t" << cwnd2 << "\t" << bif1 << "\t" << bif2 << "\t" << cwndAvailable1 << "\t" << cwndAvailable2 << std::endl;
    fileQueueSize << Simulator::Now().GetSeconds() << "\t" << queueSizeRTR1 << "\t" << queueSizeRTR2 << "\t" << SRttSocket1 << "\t" << SRttSocket2 << "\t" << estServiceRTR1 << "\t" << estServiceRTR2 << std::endl;

    Simulator::Schedule(MilliSeconds(1), &LogValues);
}

//static void CheckQueueSize()
//{
//        //uint32_t queue = qSrcToRTR1->GetNPackets();
//        //std::cout << Simulator::Now().GetSeconds()<< "    "<< queue << std::endl;
//}

///======APP STARTS =============///

class MyApp : public Application {
public:

    MyApp();
    virtual ~MyApp();

    void Setup(Ptr<Socket> socket1, Ptr<Socket> socket2, Ipv4Address address, uint16_t port, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);
    void ChangeRate(DataRate newrate);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void ScheduleTx(void);
    void SendPacket(void);

    Ptr<Socket> m_socket1;
    Ptr<Socket> m_socket2;
    Ipv4Address m_server;
    uint16_t m_servPort;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    DataRate m_dataRate;
    EventId m_sendEvent;
    bool m_running;
    uint32_t m_packetsSent;
    bool m_packetSent;
    uint32_t m_packetSentOn;
    uint32_t m_packetsFlow1;
    uint32_t m_packetsFlow2;
};

MyApp::MyApp()
: m_socket1(0),
m_socket2(0),
m_server(),
m_servPort(0),
m_packetSize(0),
m_nPackets(0),
m_dataRate(0),
m_sendEvent(),
m_running(false),
m_packetsSent(0),
m_packetSent(false),
m_packetSentOn(0),
m_packetsFlow1(0),
m_packetsFlow2(0){
}

MyApp::~MyApp() {
    m_socket1 = 0;
    m_socket2 = 0;
}

void
MyApp::Setup(Ptr<Socket> socket1, Ptr<Socket> socket2, Ipv4Address address, uint16_t port, uint32_t packetSize, uint32_t nPackets, DataRate dataRate) {
    m_socket1 = socket1;
    m_socket2 = socket2;
    m_server = address;
    m_servPort = port;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
}

void
MyApp::StartApplication(void) {
    m_running = true;
    m_packetsSent = 0;
    m_packetsFlow1 = 0;
    m_packetsFlow2 = 0;
    
    m_socket1->Bind();
    m_socket1->Connect(InetSocketAddress(m_server, m_servPort));

    m_socket2->Bind();
    m_socket2->Connect(InetSocketAddress(m_server, m_servPort));

    SendPacket();
    //StopApplication();
}

void
MyApp::StopApplication(void) {
    m_running = false;

    if (m_sendEvent.IsRunning()) {
        Simulator::Cancel(m_sendEvent);
    }

    if (m_socket1) {
        m_socket1->Close();
    }

    if (m_socket2) {
        m_socket2->Close();
    }
}

void
MyApp::SendPacket(void) {
    m_packetSent = false;
    m_packetSentOn = 0;

    std::ostringstream msg;
    msg << "Sequence Number: " << m_packetsSent << '\0';
    //Ptr<Packet> packet = Create<Packet> ((uint8_t*) msg.str().c_str(), m_packetSize);
    Ptr<Packet> packet = Create<Packet> (m_packetSize);

    if (((int) Simulator::Now().GetSeconds() > 1)) {
        if ((cwndAvailable1 == true && cwndAvailable2 == true)) //|| (cwndAvailable1 == false && cwndAvailable2 == false)) //if both subflows are available; use scheduler to decide which flow/txbuffer to send data
        {
            ///===== SRTT-based scheduler=======////
//                   if (SRttSocket1 <= SRttSocket2) {
//                        m_socket1->Send(packet);
//                        m_packetSentOn = 1;
//                        m_packetsFlow1++;
//                   } else {
//                      m_socket2->Send(packet);
//                      m_packetSentOn = 2;
//                      m_packetsFlow2++;
//                  }
            ///====== Scheduler over ==========//////
            //
            //        ///===== Queue-based scheduler=======////
            if (estServiceRTR1 <= estServiceRTR2) {
                m_socket1->Send(packet);
                m_packetSentOn = 1;
                m_packetsFlow1++;
            } else {
                m_socket2->Send(packet);
                m_packetSentOn = 2;
                m_packetsFlow2++;   
            }
            //        ///====== Scheduler over ==========//////
            m_packetSent = true;
        } else if (cwndAvailable1 == true && cwndAvailable2 == false) {
            m_socket1->Send(packet);
            m_packetSent = true;
            m_packetSentOn = 1;
            m_packetsFlow1++;
        } else if (cwndAvailable1 == false && cwndAvailable2 == true) {
            m_socket2->Send(packet);
            m_packetSent = true;
            m_packetSentOn = 2;
            m_packetsFlow2++;
        }

//            if(((int)Simulator::Now().GetSeconds() > 1) && (cwndAvailable2 == true)){
////                filepacketsent<<Simulator::Now().GetSeconds()<< "\t" << m_packetsSent << std::endl;
//                m_socket2->Send(packet);
//                m_packetSentOn = 2;
//                m_packetSent = true; 
//            }

        if (m_packetSent == true) {
            ++m_packetsSent;
            if (m_packetSentOn == 1) {
                filepacketsent1 << Simulator::Now().GetSeconds() << "\t" << m_packetsFlow1 << "\t" << m_packetsSent << std::endl;
            } else {
                filepacketsent2 << Simulator::Now().GetSeconds() << "\t" << m_packetsFlow2 << "\t" << m_packetsSent << std::endl;
            }
        }
        //**Constant data flow part**//
    }
    
   //if (m_packetsSent < m_nPackets) {    //**Comment this check if you need to have constant flow of data**//
    Simulator::ScheduleNow(&MyApp::ScheduleTx, this);
  // }
}

void
MyApp::ScheduleTx(void) {
    Simulator::ScheduleNow(&TraceRtt0, rttFileName);
    Simulator::ScheduleNow(&TraceRtt1, rttFileName1);

    Simulator::ScheduleNow(&CalculateEServiceTimeRTR1);
    Simulator::ScheduleNow(&CalculateEServiceTimeRTR2);

    Simulator::ScheduleNow(&CongWindowAvailabilityFlow1);
    Simulator::ScheduleNow(&CongWindowAvailabilityFlow2);

    if (m_running) {
        Time tNext(Seconds(m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate())));
        m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
    }
}

void
MyApp::ChangeRate(DataRate newrate) {
    m_dataRate = newrate;
    return;
}

///======================================================///



//void StartFlow(Ptr<Socket>, Ptr<Socket>, Ipv4Address, uint16_t);
void StartFlow(Ipv4Address, uint16_t);
void WriteUntilBufferFull(Ptr<Socket>, uint32_t);
void LogVariableValues();
void SendPackettoSocket(Ptr<Socket>, uint32_t, uint32_t);
void QueueSizeScheduler(Ptr<Socket>, Ptr<Socket>);
void RttScheduler(Ptr<Socket>, Ptr<Socket>);
void BindSock(Ptr<Socket> sock, Ptr<NetDevice> netdev);
void ClientSocketReceive(Ptr<Socket>);

int
main(int argc, char *argv[]) {

    // Allow the user to override any of the defaults and the above
    // DefaultValue::Bind ()s at run-time, via command-line arguments

    ////******Choose topology for simulation. Choose only one of three******////
    bool wifi_wifi = true; //Both Wifi interfaces
    //bool wifi_lte = false;
    //    bool wifi_rxbufferbloat = true;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    Ptr<Node> nSrc = CreateObject<Node> ();
    Ptr<Node> nDst = CreateObject<Node> ();
    Ptr<Node> nRtr1 = CreateObject<Node> ();
    Ptr<Node> nRtr2 = CreateObject<Node> ();
    Ptr<Node> nRtr3 = CreateObject<Node> ();
    Ptr<Node> nRtr4 = CreateObject<Node> ();
    //Ptr<Node> nDstRtr = CreateObject<Node> ();

    //Rx Bufferbloating node//       
    //    Ptr<Node> nSrc1 = CreateObject<Node> ();

    NodeContainer c;
    //    NodeContainer sta;

    c = NodeContainer(nSrc, nDst, nRtr1, nRtr2, nRtr3);
    c.Add(nRtr4);

    InternetStackHelper internet;
    internet.Install(c);

    NodeContainer nSrcnRtr1;
    NodeContainer nSrcnRtr2;
    NodeContainer nRtr1nRtr3;
    NodeContainer nRtr2nRtr4;
    NodeContainer nRtr3nDst;
    NodeContainer nRtr4nDst;

    Ipv4AddressHelper ipv4;

    NetDeviceContainer dSrcdRtr1;
    NetDeviceContainer dSrcdRtr2;
    NetDeviceContainer dRtr1dRtr3;
    NetDeviceContainer dRtr2dRtr4;
    NetDeviceContainer dRtr3dDst;
    NetDeviceContainer dRtr4dDst;

    PointToPointHelper p2p;

    YansWifiChannelHelper channel;
    YansWifiPhyHelper phy;

    Ptr<NetDevice> SrcToRtr1;
    Ptr<NetDevice> SrcToRtr2;
    
    Ptr<NetDevice> DstToRtr3;
    Ptr<NetDevice> DstToRtr4;

    ///**For AP bufferbloat checks**//
    //    sta = NodeContainer(nSrc1);
    //    internet.Install(sta);
    //    NodeContainer nSrc1nRTR1;
    //    NodeContainer nSrc1nRTR2;
    //    NetDeviceContainer dSrc1dRtr1;
    //    NetDeviceContainer dSrc2dRtr2;

    //    YansWifiChannelHelper channel1;
    //    YansWavePhyHelper phy1;

    //    Ptr<NetDevice> Src1ToRTR1;
    //    Ptr<NetDevice> Src1ToRtr2;

    ///***AP Bufferbloat conditions over***//

    if (wifi_wifi == true) {
        // Point-to-point links
        nSrcnRtr1 = NodeContainer(nSrc, nRtr1);
        nSrcnRtr2 = NodeContainer(nSrc, nRtr2);
        nRtr1nRtr3 = NodeContainer(nRtr1, nRtr3);
        nRtr2nRtr4 = NodeContainer(nRtr2, nRtr4);
        nRtr3nDst = NodeContainer(nRtr3, nDst);
        nRtr4nDst = NodeContainer(nRtr4, nDst);

        //Wifi Links
        NodeContainer staWifiNode = NodeContainer(nSrc); //nSRC from P2p link
        NodeContainer apWifiNode = NodeContainer(nRtr1nRtr3.Get(0)); //nRtr1 from P2P link

        NodeContainer sta1WifiNode = NodeContainer(nSrc); //nSRC from P2p link
        NodeContainer ap1WifiNode = NodeContainer(nRtr2nRtr4.Get(0)); //nRtr1 from P2P link
        
        //DST side WiFi links 
        NodeContainer dstWifiNode = NodeContainer(nDst); //nSRC from P2p link
        NodeContainer apDstWifiNode = NodeContainer(nRtr1nRtr3.Get(1)); //nRtr1 from P2P link

        NodeContainer dst1WifiNode = NodeContainer(nDst); //nSRC from P2p link
        NodeContainer ap1DstWifiNode = NodeContainer(nRtr2nRtr4.Get(1)); //nRtr1 from P2P link
        
        //Create channel for wifi connection
        channel = YansWifiChannelHelper::Default();
        phy = YansWifiPhyHelper::Default();
        //phy.SetChannel(channel.Create());

        WifiHelper wifiHelper;
        wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate12Mbps"));

        NqosWifiMacHelper wifiMac;
        //wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);
        
        // Src WiFi set up
        
        phy.Set("ChannelNumber", UintegerValue(1));
        phy.SetChannel(channel.Create());

        //        phy.Set ("RxNoiseFigure", DoubleValue (10));
        //        phy.Set ("CcaMode1Threshold", DoubleValue (-79));

        Ssid ssid = Ssid("network");
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "QosSupported", BooleanValue(false));
        NetDeviceContainer dSrcdRtr1 = wifiHelper.Install(phy, wifiMac, staWifiNode);

        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "QosSupported", BooleanValue(false));
        NetDeviceContainer apRtr1 = wifiHelper.Install(phy, wifiMac, apWifiNode);

        phy.Set("ChannelNumber", UintegerValue(11));
        phy.SetChannel(channel.Create());

        //        phy.Set ("RxNoiseFigure", DoubleValue (40));
        //        phy.Set ("CcaMode1Threshold", DoubleValue (-79));
        
        wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate12Mbps"));

        Ssid ssid1 = Ssid("network1");
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "QosSupported", BooleanValue(false));
        NetDeviceContainer dSrcdRtr2 = wifiHelper.Install(phy, wifiMac, sta1WifiNode);

        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1), "QosSupported", BooleanValue(false));
        NetDeviceContainer apRtr2 = wifiHelper.Install(phy, wifiMac, ap1WifiNode);

        // Dst WiFi setup
        
        phy.Set("ChannelNumber", UintegerValue(4));
        phy.SetChannel(channel.Create());

        //        phy.Set ("RxNoiseFigure", DoubleValue (10));
        //        phy.Set ("CcaMode1Threshold", DoubleValue (-79));

        Ssid ssid = Ssid("DstNetwork");
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "QosSupported", BooleanValue(false));
        NetDeviceContainer dDstdRtr3 = wifiHelper.Install(phy, wifiMac, dstWifiNode);

        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "QosSupported", BooleanValue(false));
        NetDeviceContainer apRtr3 = wifiHelper.Install(phy, wifiMac, apDstWifiNode);

        phy.Set("ChannelNumber", UintegerValue(8));
        phy.SetChannel(channel.Create());

        //        phy.Set ("RxNoiseFigure", DoubleValue (40));
        //        phy.Set ("CcaMode1Threshold", DoubleValue (-79));
        
        wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate12Mbps"));

        Ssid ssid1 = Ssid("network1");
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "QosSupported", BooleanValue(false));
        NetDeviceContainer dDstdRtr4 = wifiHelper.Install(phy, wifiMac, dst1WifiNode);

        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1), "QosSupported", BooleanValue(false));
        NetDeviceContainer apRtr4 = wifiHelper.Install(phy, wifiMac, ap1DstWifiNode);

        
        // We create the Point to point channels for connecting the two routers
        //        p2p.SetDeviceAttribute("DataRate", StringValue("6Mbps")); // the wire transmission rate must be slower than application data rate for queueing to occur in QueueDisc
        p2p.SetChannelAttribute("Delay", StringValue("1ms")); //Play with delays on both paths to throw off srtt scheduler
        //dSrcdRtr2 = p2p.Install(nSrcnRtr2);

        p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps")); //Backbone link (30)
        dRtr1dRtr3 = p2p.Install(nRtr1nRtr3);
        dRtr2dRtr4 = p2p.Install(nRtr2nRtr4);

//        p2p.SetDeviceAttribute("DataRate", StringValue("50Mbps")); //Core network link (50)
//        dDstRtrdDst = p2p.Install(nDstRtrnDst);

        SrcToRtr1 = dSrcdRtr1.Get(0);
        SrcToRtr2 = dSrcdRtr2.Get(0);

        DstToRtr3 = dDstdRtr3.Get(0);
        DstToRtr4 = dDstdRtr4.Get(0);

        //******Uncomment this code to introduce errors on paths****// 
//        int random_seed = rand() % 5 + 1;
//        int random_seed1 = rand() % 5 + 1;
//        double errorrate = random_seed * 0.00001;
//        double errorrate2 = random_seed1 * 0.000015;
//        //        //Adding errors on RTR1
//                Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
//                em->SetAttribute ("ErrorRate", DoubleValue (0.0001));
//                dRtr1dDstRtr.Get(1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
//        //
//        //        //Adding errors on RTR2
//                Ptr<RateErrorModel> em1 = CreateObject<RateErrorModel> ();
//                em1->SetAttribute ("ErrorRate", DoubleValue (errorrate2));
//                dRtr2dDstRtr.Get(1)->SetAttribute ("ReceiveErrorModel", PointerValue (em1));
        //***Error inducing code is over***//

        //Set up wifi mobility
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                "MinX", DoubleValue(0.0),
                "MinY", DoubleValue(0.0),
                "DeltaX", DoubleValue(5.0),
                "DeltaY", DoubleValue(10.0),
                "GridWidth", UintegerValue(3),
                "LayoutType", StringValue("RowFirst"));

        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(staWifiNode);
        mobility.Install(apWifiNode);

        mobility.Install(sta1WifiNode);
        mobility.Install(ap1WifiNode);

        Ptr<WifiNetDevice> wifiSrcToRTR1 = DynamicCast<WifiNetDevice> (SrcToRtr1); //Look into WifiMacQueue if you are using Wifi
        PointerValue ptr;
        wifiSrcToRTR1->GetAttribute("Mac", ptr);
        Ptr<StaWifiMac> mac = ptr.Get<StaWifiMac>();
        mac->GetAttribute("DcaTxop", ptr);
        Ptr<DcaTxop> dca = ptr.Get<DcaTxop>();
        Ptr<WifiMacQueue> qSrcToRTR1 = dca->GetQueue();
        qSrcToRTR1->SetMaxPackets(1000);
        qSrcToRTR1->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&DevicePacketsInQueueRTR1));
        //ByteQueue:
        
        //qSrcToRTR1->TraceConnectWithoutContext("BytesInQueue", MakeCallback(&DevicePacketsInQueueRTR1));
        qSrcToRTR1->TraceConnectWithoutContext("Dequeue", MakeCallback(&DequeueTracer1));
        qSrcToRTR1->TraceConnectWithoutContext("Enqueue", MakeCallback(&EnqueueTracer1));

        Ptr<WifiNetDevice> wifiSrcToRTR2 = DynamicCast<WifiNetDevice> (SrcToRtr2); //Look into WifiMacQueue if you are using Wifi
        PointerValue ptr1;
        wifiSrcToRTR2->GetAttribute("Mac", ptr1);
        Ptr<StaWifiMac> mac1 = ptr1.Get<StaWifiMac>();
        mac1->GetAttribute("DcaTxop", ptr1);
        Ptr<DcaTxop> dca1 = ptr1.Get<DcaTxop>();
        Ptr<WifiMacQueue> qSrcToRTR2 = dca1->GetQueue();
        qSrcToRTR2->SetMaxPackets(1000);
        qSrcToRTR2->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&DevicePacketsInQueueRTR2));
        //ByteQueue:
        //qSrcToRTR2->TraceConnectWithoutContext("BytesInQueue", MakeCallback(&DevicePacketsInQueueRTR2));
        qSrcToRTR2->TraceConnectWithoutContext("Dequeue", MakeCallback(&DequeueTracer2));
        qSrcToRTR2->TraceConnectWithoutContext("Enqueue", MakeCallback(&EnqueueTracer2));

        // Later, we add IP addresses.
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer iSrcWifi;
        iSrcWifi = ipv4.Assign(dSrcdRtr1);
        Ipv4InterfaceContainer iRtr1Wifi;
        iRtr1Wifi = ipv4.Assign(apRtr1);

        ipv4.SetBase("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer iSrcWifi1;
        iSrcWifi1 = ipv4.Assign(dSrcdRtr2);
        Ipv4InterfaceContainer iRtr2Wifi;
        iRtr2Wifi = ipv4.Assign(apRtr2);
    }

    
    //    if(wifi_rxbufferbloat == true)
    //    {
    //            // Point-to-point links
    //        nSrcnRtr1 = NodeContainer(nSrc, nRtr1);
    //        nSrcnRtr2 = NodeContainer(nSrc, nRtr2);
    //        nSrc1nRTR1 = NodeContainer(nSrc1, nRtr1);
    //        nSrc1nRTR2 = NodeContainer(nSrc1, nRtr2);
    //        nRtr1nDstRtr = NodeContainer(nRtr1, nDstRtr);
    //        nRtr2nDstRtr = NodeContainer(nRtr2, nDstRtr);
    //        nDstRtrnDst = NodeContainer(nDstRtr, nDst);
    //
    //        //Wifi Links
    //        NodeContainer staWifiNode = NodeContainer(nSrc); //nSRC from P2p link
    //        NodeContainer apWifiNode = NodeContainer(nRtr1nDstRtr.Get(0)); //nRtr1 from P2P link
    //        
    //        NodeContainer sta1WifiNode = NodeContainer(nSrc); //nSRC from P2p link
    //        NodeContainer ap1WifiNode = NodeContainer(nRtr2nDstRtr.Get(0)); //nRtr1 from P2P link
    //        
    //        staWifiNode.Add(NodeContainer(nSrc1));
    //        sta1WifiNode.Add(NodeContainer(nSrc1));
    ////        //Wifi Links Rx Bufferbloat
    ////        NodeContainer staWifiNode1 = NodeContainer(nSrc1);
    ////        NodeContainer sta1WifiNode1 = NodeContainer(nSrc1);
    ////        
    //        //Create channel for wifi connection
    //        channel = YansWifiChannelHelper::Default();
    //        phy = YansWifiPhyHelper::Default();
    //        //phy.SetChannel(channel.Create());
    //
    //        WifiHelper wifiHelper;
    //        wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"));
    //
    //        NqosWifiMacHelper wifiMac;
    //        //wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);
    //        
    //        phy.Set("ChannelNumber", UintegerValue(1));
    //        phy.SetChannel(channel.Create());
    //        
    ////        phy.Set ("RxNoiseFigure", DoubleValue (10));
    ////        phy.Set ("CcaMode1Threshold", DoubleValue (-79));
    //        
    //        Ssid ssid = Ssid("network");
    //        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "QosSupported", BooleanValue(false));
    //        NetDeviceContainer dSrcdRtr1 = wifiHelper.Install(phy, wifiMac, staWifiNode);
    //        
    //        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "QosSupported", BooleanValue(false));
    //        NetDeviceContainer apRtr1 = wifiHelper.Install(phy, wifiMac, apWifiNode);
    //        
    //        phy.Set("ChannelNumber", UintegerValue(11));
    //        phy.SetChannel(channel.Create());
    //        
    ////        phy.Set ("RxNoiseFigure", DoubleValue (40));
    ////        phy.Set ("CcaMode1Threshold", DoubleValue (-79));
    //        
    //        Ssid ssid1 = Ssid("network1");
    //        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "QosSupported", BooleanValue(false));
    //        NetDeviceContainer dSrcdRtr2 = wifiHelper.Install(phy, wifiMac, sta1WifiNode);
    //
    //        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1), "QosSupported", BooleanValue(false));
    //        NetDeviceContainer apRtr2 = wifiHelper.Install(phy, wifiMac, ap1WifiNode);
    //        
    ////        //Add RxBufferBloat nodes to Wifi AP
    ////        phy.Set("ChannelNumber", UintegerValue(7));
    ////        phy.SetChannel(channel.Create());
    //        
    ////        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "QosSupported", BooleanValue(false));
    ////        NetDeviceContainer dSrc1dRtr1 = wifiHelper.Install(phy, wifiMac, staWifiNode1);
    ////        
    ////        phy.Set("ChannelNumber", UintegerValue(4));
    ////        phy.SetChannel(channel.Create());
    ////        
    ////        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "QosSupported", BooleanValue(false));
    ////        NetDeviceContainer dSrc1dRtr2 = wifiHelper.Install(phy, wifiMac, sta1WifiNode1);
    //        
    //        // We create the Point to point channels first without any IP addressing information
    ////        p2p.SetDeviceAttribute("DataRate", StringValue("6Mbps")); // the wire transmission rate must be slower than application data rate for queueing to occur in QueueDisc
    //        p2p.SetChannelAttribute("Delay", StringValue("2ms")); //Play with delays on both paths to throw off srtt scheduler
    //        //dSrcdRtr2 = p2p.Install(nSrcnRtr2);
    //        
    //        p2p.SetDeviceAttribute("DataRate", StringValue("30Mbps"));      //Backbone link (30)
    //        dRtr1dDstRtr = p2p.Install(nRtr1nDstRtr);
    //        dRtr2dDstRtr = p2p.Install(nRtr2nDstRtr);
    //        
    //        p2p.SetDeviceAttribute("DataRate", StringValue("50Mbps"));      //Core network link (50)
    //        dDstRtrdDst = p2p.Install(nDstRtrnDst);
    //
    //        SrcToRtr1 = dSrcdRtr1.Get(0);
    //        SrcToRtr2 = dSrcdRtr2.Get(0);
    //        
    //        Src1ToRTR1 = dSrcdRtr1.Get(1);
    //        Src1ToRtr2 = dSrcdRtr2.Get(1);
    //        
    //       //******Uncomment this code to introduce errors on paths****// 
    ////        //Adding errors on RTR1
    ////        Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
    ////        em->SetAttribute ("ErrorRate", DoubleValue (0.00001));
    ////        dRtr1dDstRtr.Get(1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
    ////
    ////        //Adding errors on RTR2
    ////        Ptr<RateErrorModel> em1 = CreateObject<RateErrorModel> ();
    ////        em->SetAttribute ("ErrorRate", DoubleValue (0.00003));
    ////        dRtr2dDstRtr.Get(1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
    //       //***Error inducing code is over***//
    //        
    //        //Set up wifi mobility
    //        MobilityHelper mobility;
    //        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
    //                "MinX", DoubleValue(0.0),
    //                "MinY", DoubleValue(0.0),
    //                "DeltaX", DoubleValue(5.0),
    //                "DeltaY", DoubleValue(10.0),
    //                "GridWidth", UintegerValue(3),
    //                "LayoutType", StringValue("RowFirst"));
    //        
    //        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    //        mobility.Install(staWifiNode);
    //        mobility.Install(apWifiNode);
    //
    //        mobility.Install(sta1WifiNode);
    //        mobility.Install(ap1WifiNode);
    //        
    ////        mobility.Install(sta1WifiNode1);
    ////        mobility.Install(staWifiNode1);
    //        
    //        Ptr<WifiNetDevice> wifiSrcToRTR1 = DynamicCast<WifiNetDevice> (SrcToRtr1); //Look into WifiMacQueue if you are using Wifi
    //        PointerValue ptr;
    //        wifiSrcToRTR1->GetAttribute("Mac", ptr);
    //        Ptr<StaWifiMac> mac = ptr.Get<StaWifiMac>();
    //        mac->GetAttribute("DcaTxop", ptr);
    //        Ptr<DcaTxop> dca = ptr.Get<DcaTxop>();
    //        Ptr<WifiMacQueue> qSrcToRTR1 = dca->GetQueue();
    //        qSrcToRTR1->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&DevicePacketsInQueueRTR1));
    //
    //        Ptr<WifiNetDevice> wifiSrcToRTR2 = DynamicCast<WifiNetDevice> (SrcToRtr2); //Look into WifiMacQueue if you are using Wifi
    //        PointerValue ptr1;
    //        wifiSrcToRTR2->GetAttribute("Mac", ptr1);
    //        Ptr<StaWifiMac> mac1 = ptr1.Get<StaWifiMac>();
    //        mac1->GetAttribute("DcaTxop", ptr1);
    //        Ptr<DcaTxop> dca1 = ptr1.Get<DcaTxop>();
    //        Ptr<WifiMacQueue> qSrcToRTR2 = dca1->GetQueue();
    //        qSrcToRTR2->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&DevicePacketsInQueueRTR2));
    //
    //        // Later, we add IP addresses.
    //        ipv4.SetBase("10.1.1.0", "255.255.255.0");
    //        Ipv4InterfaceContainer iSrcWifi;
    //        iSrcWifi = ipv4.Assign(dSrcdRtr1.Get(0));
    //        Ipv4InterfaceContainer iRtr1Wifi;
    //        iRtr1Wifi = ipv4.Assign(apRtr1);
    //        iSrcWifi.Add(ipv4.Assign(dSrcdRtr1.Get(1)));
    //        
    ////        ipv4.SetBase("10.2.1.0", "255.255.255.0");
    ////        Ipv4InterfaceContainer iSrc1Wifi;
    ////        iSrc1Wifi = ipv4.Assign(dSrc1dRtr1);
    ////        Ipv4InterfaceContainer iRtr1Wifi1;
    ////        iRtr1Wifi1 = ipv4.Assign(apRtr1);
    //        
    //        ipv4.SetBase("10.1.2.0", "255.255.255.0");
    //        Ipv4InterfaceContainer iSrcWifi1;
    //        iSrcWifi1 = ipv4.Assign(dSrcdRtr2.Get(0));
    //        Ipv4InterfaceContainer iRtr2Wifi;
    //        iRtr2Wifi = ipv4.Assign(apRtr2);
    //        iSrcWifi1.Add(ipv4.Assign(dSrcdRtr2.Get(1)));
    ////        
    ////        ipv4.SetBase("10.2.2.0", "255.255.255.0");
    ////        Ipv4InterfaceContainer iRtr2Wifi1;
    ////        iRtr2Wifi1 = ipv4.Assign(apRtr2);
    ////        Ipv4InterfaceContainer iSrc1Wifi1;
    ////        iSrc1Wifi1 = ipv4.Assign(dSrc1dRtr2);
    //
    //
    //    }

    //********Install Queueing Discipline on the link*****//       
    //    TrafficControlHelper tchPfifo;                //Enable queueDisc to avoid packet drops on buffer overflow
    //    uint16_t handle = tchPfifo.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
    //    tchPfifo.AddInternalQueues(handle, 3, "ns3::DropTailQueue", "MaxPackets", UintegerValue(1000));
    //
    //    QueueDiscContainer qdiscrtr1 = tchPfifo.Install(dSrcdRtr1);
    //    //QueueDiscContainer qdiscrtr2 = tchPfifo.Install(dSrcdRtr2);
    //    
    //    Ptr<QueueDisc> q1 = qdiscrtr1.Get(0);
    //    q1->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&TcPacketsInQueue));

    //Ptr<QueueDisc> q2 = qdiscrtr2.Get(0);
    //q2->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&TcPacketsInQueue));

    ipv4.SetBase("10.10.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iRtr1iDstRtr = ipv4.Assign(dRtr1dDstRtr);
    ipv4.SetBase("10.10.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iRtr2iDstRtr = ipv4.Assign(dRtr2dDstRtr);
    ipv4.SetBase("10.20.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iDstRtrDst = ipv4.Assign(dDstRtrdDst);

    AnimationInterface anim("socket-bound-tcp-static-routing.xml");

    anim.SetConstantPosition(nDst, 25.0, 40.0); //DST node
    anim.SetConstantPosition(nDstRtr, 25.0, 30.0); //DSTRtr node
    anim.SetConstantPosition(nRtr1, 15.0, 15.0); //RTR1 node
    anim.SetConstantPosition(nRtr2, 35.0, 15.0); //RTR2 node
    anim.SetConstantPosition(nSrc, 25.0, 2.0); //SRC node
    //anim.SetConstantPosition(nSrc1, 35.0, 2.0);
    anim.SetMaxPktsPerTraceFile(5000000000);

    //Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    Ptr<Ipv4> ipv4Src = nSrc->GetObject<Ipv4> ();
    Ptr<Ipv4> ipv4Rtr1 = nRtr1->GetObject<Ipv4> ();
    Ptr<Ipv4> ipv4Rtr2 = nRtr2->GetObject<Ipv4> ();
    Ptr<Ipv4> ipv4DstRtr = nDstRtr->GetObject<Ipv4> ();
    Ptr<Ipv4> ipv4Dst = nDst->GetObject<Ipv4> ();

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> staticRoutingSrc = ipv4RoutingHelper.GetStaticRouting(ipv4Src);
    Ptr<Ipv4StaticRouting> staticRoutingRtr1 = ipv4RoutingHelper.GetStaticRouting(ipv4Rtr1);
    Ptr<Ipv4StaticRouting> staticRoutingRtr2 = ipv4RoutingHelper.GetStaticRouting(ipv4Rtr2);
    Ptr<Ipv4StaticRouting> staticRoutingDstRtr = ipv4RoutingHelper.GetStaticRouting(ipv4DstRtr);
    Ptr<Ipv4StaticRouting> staticRoutingDst = ipv4RoutingHelper.GetStaticRouting(ipv4Dst);

    //    Ptr<Ipv4> ipv4Src1 = nSrc1->GetObject<Ipv4> ();
    //    Ptr<Ipv4StaticRouting> staticRoutingSrc1 = ipv4RoutingHelper.GetStaticRouting(ipv4Src1);

    // Create static routes from Src to Dst
    staticRoutingRtr1->AddHostRouteTo(Ipv4Address("10.20.1.2"), Ipv4Address("10.10.1.2"), 2);
    staticRoutingRtr2->AddHostRouteTo(Ipv4Address("10.20.1.2"), Ipv4Address("10.10.2.2"), 2);

    // Two routes to same destination - setting separate metrics. 
    // You can switch these to see how traffic gets diverted via different routes
    staticRoutingSrc->AddHostRouteTo(Ipv4Address("10.20.1.2"), Ipv4Address("10.1.1.2"), 1, 5);
    staticRoutingSrc->AddHostRouteTo(Ipv4Address("10.20.1.2"), Ipv4Address("10.1.2.2"), 2, 5);

    //Creating route from DST to SRC pointing via RTR1
    staticRoutingDst->AddHostRouteTo(Ipv4Address("10.1.1.1"), Ipv4Address("10.20.1.1"), 1);
    staticRoutingDstRtr->AddHostRouteTo(Ipv4Address("10.1.1.1"), Ipv4Address("10.10.1.1"), 1);
    staticRoutingRtr1->AddHostRouteTo(Ipv4Address("10.1.1.1"), Ipv4Address("10.1.1.1"), 1);
    staticRoutingRtr1->AddHostRouteTo(Ipv4Address("10.1.2.1"), Ipv4Address("10.1.1.1"), 1);


    //    staticRoutingDst->AddHostRouteTo(Ipv4Address("10.1.1.3"), Ipv4Address("10.20.1.1"), 1);
    //    staticRoutingDstRtr->AddHostRouteTo(Ipv4Address("10.1.1.3"), Ipv4Address("10.10.1.1"), 1);
    //    staticRoutingRtr1->AddHostRouteTo(Ipv4Address("10.1.1.3"), Ipv4Address("10.1.1.3"), 1);
    //    staticRoutingRtr1->AddHostRouteTo(Ipv4Address("10.1.2.3"), Ipv4Address("10.1.1.3"), 1);

    //Creating route from DST to SRC pointing via RTR2
    staticRoutingDst->AddHostRouteTo(Ipv4Address("10.1.2.1"), Ipv4Address("10.20.1.1"), 1);
    staticRoutingDstRtr->AddHostRouteTo(Ipv4Address("10.1.2.1"), Ipv4Address("10.10.2.1"), 2);
    staticRoutingRtr2->AddHostRouteTo(Ipv4Address("10.1.2.1"), Ipv4Address("10.1.2.1"), 1);
    staticRoutingRtr2->AddHostRouteTo(Ipv4Address("10.1.1.1"), Ipv4Address("10.1.2.1"), 1);

    //    staticRoutingDst->AddHostRouteTo(Ipv4Address("10.1.2.3"), Ipv4Address("10.20.1.1"), 1);
    //    staticRoutingDstRtr->AddHostRouteTo(Ipv4Address("10.1.2.3"), Ipv4Address("10.10.2.1"), 2);
    //    staticRoutingRtr2->AddHostRouteTo(Ipv4Address("10.1.2.3"), Ipv4Address("10.1.2.3"), 1);
    //    staticRoutingRtr2->AddHostRouteTo(Ipv4Address("10.1.1.3"), Ipv4Address("10.1.2.3"), 1);

    //RxBufferbloat
    //    staticRoutingSrc1->AddHostRouteTo(Ipv4Address("10.20.1.2"), Ipv4Address("10.1.1.3"), 1, 5);
    //    staticRoutingSrc1->AddHostRouteTo(Ipv4Address("10.20.1.2"), Ipv4Address("10.1.2.3"), 2, 5);

    // There are no apps that can utilize the Socket Option so doing the work directly..
    // Taken from tcp-large-transfer example

    Socket1 = Socket::CreateSocket(nSrc, TypeId::LookupByName("ns3::TcpSocketFactory"));
    Socket2 = Socket::CreateSocket(nSrc, TypeId::LookupByName("ns3::TcpSocketFactory"));

    //    Socket3 = Socket::CreateSocket(nSrc1, TypeId::LookupByName("ns3::TcpSocketFactory"));
    //    Socket4 = Socket::CreateSocket(nSrc1, TypeId::LookupByName("ns3::TcpSocketFactory"));

    //    Socket1->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndTracer1));
    //    Socket2->TraceConnectWithoutContext("CongestionWindow", MakeCallback (&CwndTracer2));

    uint16_t dstport = 12345;
    Ipv4Address dstaddr("10.20.1.2");

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dstport));
    ApplicationContainer apps = sink.Install(nDst);
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(10.0));

    // Create TCP application at n0
    Ptr<MyApp> app = CreateObject<MyApp> ();
    app->Setup(Socket1, Socket2, dstaddr, dstport, packetSize, 200000, DataRate("18Mbps")); //original packet length: 1020
    c.Get(0)->AddApplication(app);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(10.0));

    //    Ptr<MyApp1> app1 = CreateObject<MyApp1> ();
    //    app1->Setup(Socket3, Socket4, dstaddr, dstport, 1040, 30000, DataRate("1Mbps"));
    //    sta.Get(0)->AddApplication(app1);
    //    app1->SetStartTime(Seconds(1.0));
    //    app1->SetStopTime(Seconds(10.0));

    //qDiscRTR1 = qdiscrtr1.Get(0);
    //qDiscRTR2 = qdiscrtr2.Get(0);

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("socket-bound-tcp-static-routing.tr"));
    p2p.EnablePcapAll("socket-bound-tcp-static-routing");

    phy.EnableAsciiAll(ascii.CreateFileStream("wifi-trace.tr"));

    if (wifi_wifi == true) {
        phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy.EnablePcapAll("socket-bound-tcp-static-routing-wifi");
    }

    LogComponentEnableAll(LOG_PREFIX_TIME);
    LogComponentEnable("SocketBoundTcpRoutingExample", LOG_LEVEL_INFO);

    fileQueueSize << "Time" << "\t" << "QueueSize(RTR1)" << "\t" << "QueueSize(RTR2)" << "\t" << " SRTT(RTR1)" << "\t" << " SRTT(RTR2)" << "\t" << "EstimatedService(RTR1)" << "\t" << "EstimatedService(RTR2)" << std::endl;
    fileCwndSrc << "Time" << "\t" << "CwndSize(RTR1)" << "\t" << "CwndSize(RTR2)" << "\t" << "BytesInFlight(RTR1)" << "\t" << "BytesInFlight(RTR2)" << "\t" << "SpaceInCwnd(RTR1)" << "\t" << "SpaceInCwnd(RTR2)" << std::endl;
    filerttf1 << "Time" << "\t" << "RTT_old" << "\t" << "RTT_new" << "\t" << " SRTT" << std::endl;
    filerttf2 << "Time" << "\t" << "RTT_old" << "\t" << "RTT_new" << "\t" << " SRTT" << std::endl;

    Simulator::Schedule(Seconds(0.0), &BindSock, Socket1, SrcToRtr1);
    Simulator::Schedule(Seconds(0.0), &BindSock, Socket2, SrcToRtr2);

    //    Simulator::Schedule(Seconds(0.0), &BindSock, Socket3, Src1ToRTR1);
    //    Simulator::Schedule(Seconds(0.0), &BindSock, Socket4, Src1ToRtr2);

    Simulator::Schedule(Seconds(0.0), &LogValues);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor;
    monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(15.0));
    Simulator::Run();

    flowmon.SerializeToXmlFile("socket-bound.flowmon", true, true);

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Tx Packets:    " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets:    " << i->second.rxPackets << "\n";
        std::cout << "  Goodput: " << i->second.rxBytes * 8.0 / 5.0 / (1024 * 1024) << " Mbps\n";
        std::cout << "  Packet Loss Ratio: " << (i->second.txPackets - i->second.rxPackets)*100 / (double) i->second.txPackets << " %\n";
        //std::cout << "  Packet Dropped: " << i->second.packetsDropped  << "%\n";
        std::cout << "  mean Delay: " << i->second.delaySum.GetSeconds()*1000 / i->second.rxPackets << " ms\n";
        std::cout << "  mean Jitter: " << i->second.jitterSum.GetSeconds()*1000 / (i->second.rxPackets - 1) << " ms\n";
        std::cout << "  mean Hop count: " << 1 + i->second.timesForwarded / (double) i->second.rxPackets << "\n";
    }

    Simulator::Destroy();

    return 0;
}

void BindSock(Ptr<Socket> sock, Ptr<NetDevice> netdev) {
    sock->BindToNetDevice(netdev);
    return;
}

////***** CODE ENDS HERE ******////////

//// FORGET THIS PART OF THE CODE. KEPT AS BACKUP/////////////////////

//void StartFlow(Ptr<Socket> Socket1, Ptr<Socket> Socket2, Ipv4Address servAddress, uint16_t servPort) {

void StartFlow(Ipv4Address servAddress, uint16_t servPort) {
    NS_LOG_INFO("Starting flow at time " << Simulator::Now().GetSeconds());
    currentTxBytes = 0;
    // int amountSent = 0;
    Socket1->Bind();
    Socket1->Connect(InetSocketAddress(servAddress, servPort)); //connect

    Socket2->Bind();
    Socket2->Connect(InetSocketAddress(servAddress, servPort));

    /*while (currentTxBytes < totalTxBytes) {
        uint32_t left = totalTxBytes - currentTxBytes;
        uint32_t dataOffset = currentTxBytes % writeSize;
        uint32_t toWrite = writeSize - dataOffset;
        toWrite = std::min(toWrite, left);
         Scheduler code/call should be here
         Currently we are deciding the flow solely based on a random throw of dice
          
       int decider = rand() % 3;
        
        TraceRtt0(rttFileName);
        TraceRtt1(rttFileName1);
        if (SRttSocket1 <= SRttSocket2) {
            schedule_decider = 1;
        } else {
            schedule_decider = 2;
        }
        //std::cout<<SRttSocket1<<"   "<<SRttSocket2<<"   "<<schedule_decider<<std::endl;
        if (decider == 1) {
            toWrite = std::min(toWrite, Socket1->GetTxAvailable());
            SendPackettoSocket(Socket1, dataOffset, toWrite);
            amountSent = toWrite;
        }
        if (decider == 2){
            toWrite = std::min(toWrite, Socket2->GetTxAvailable());
            SendPackettoSocket(Socket2, dataOffset, toWrite);
            amountSent = toWrite;
        }
        currentTxBytes += amountSent;
        std::cout<<"Decider Value"<<decider<<"    "<<currentTxBytes<<std::endl;
        CheckQueueSize(qDiscRTR1, qDiscRTR2);
    }*/
    Socket2->SetSendCallback(MakeCallback(&WriteUntilBufferFull));
    Socket1->SetSendCallback(MakeCallback(&WriteUntilBufferFull));
    //MakeCallback(&WriteUntilBufferFull);
    //WriteUntilBufferFull (Socket2, Socket2->GetTxAvailable ());
    Socket1->Send(&data[0], 1040, 0);
    Socket1->Send(&data[0], 1040, 0);

    Socket1->Close();
    Socket2->Close();
    // tell the tcp implementation to call WriteUntilBufferFull again
    // if we blocked and new tx buffer space becomes available
    /*Socket1->SetSendCallback(MakeCallback(&WriteUntilBufferFull)); //notify the application that space in tx buffer is available, so go to next line of code.
    WriteUntilBufferFull(Socket1, Socket1->GetTxAvailable());*/
}

void WriteUntilBufferFull(Ptr<Socket> localSocket, uint32_t txSpace) {
    /*Ptr<Socket> localSocket1;
    if (localSocket==Socket1){
        localSocket1 = Socket2;
    }else{
        localSocket1 = Socket1;
    }*/
    NS_LOG_INFO("Packet send called at time");
    NS_LOG_INFO(Simulator::Now().GetSeconds());
    int amountSent;
    while (currentTxBytes < totalTxBytes && Socket1->GetTxAvailable() > 0 && Socket2->GetTxAvailable() > 0) { //&& localSocket1->GetTxAvailable() > 0) {
        uint32_t left = totalTxBytes - currentTxBytes;
        uint32_t dataOffset = currentTxBytes % writeSize;
        uint32_t toWrite = writeSize - dataOffset;
        toWrite = std::min(toWrite, left);
        uint32_t toWrite1 = std::min(toWrite, Socket1->GetTxAvailable());
        //uint32_t toWrite2 = std::min(toWrite, Socket2->GetTxAvailable());

        if (currentTxBytes == 0) {
            amountSent = Socket1->Send(&data[dataOffset], toWrite1, 0);
            amountSent = Socket2->Send(&data[dataOffset], toWrite1, 0);
            amountSent += amountSent;
            currentTxBytes += amountSent;

            Socket1->SetRecvCallback(MakeCallback(&ClientSocketReceive));
            Socket2->SetRecvCallback(MakeCallback(&ClientSocketReceive));
            continue;
        }

        if (SRttSocket1 <= SRttSocket2) {
            amountSent = Socket1->Send(&data[dataOffset], toWrite1, 0);
        } else {
            amountSent = Socket2->Send(&data[dataOffset], toWrite1, 0);
        }

        //CheckQueueSize(qDiscRTR1, qDiscRTR2);
        //fileQueueSize << Simulator::Now().GetSeconds() << SRttSocket1 << "         " << SRttSocket2 << std::endl;
        if (amountSent < 0) { //|| amountSent1 < 0) {
            // we will be called again when new tx space becomes available.
            return;
        }
        currentTxBytes += amountSent;
    }
    //localSocket->Close();
}

void SendPackettoSocket(Ptr<Socket> localSocket, uint32_t dataOffset, uint32_t writeBuffer) {
    localSocket->Send(&data[dataOffset], writeBuffer, 0);
    return;
}

void QueueSizeScheduler(Ptr<Socket> Socket1, Ptr<Socket> Socket2) {
    return;
}

void ClientSocketReceive(Ptr<Socket> localSocket) {
    NS_LOG_INFO("Packet received at time ");
    NS_LOG_INFO(Simulator::Now().GetSeconds());
    TraceRtt0(rttFileName);
    TraceRtt1(rttFileName1);
}

void RttScheduler(Ptr<Socket> Socket1, Ptr<Socket> Socket2) {
    //int SocketID=0;
    if (SRttSocket1 < SRttSocket2) {
        schedule_decider = 1;
    } else {
        schedule_decider = 2;
    }
    return;
}

/*void LogVariableValues ()
{
    fileVariableLog << Simulator::Now().GetSeconds() << " " << queueSizeRTR1 << "   " << queueSizeRTR2 <<std::endl;
}*/

