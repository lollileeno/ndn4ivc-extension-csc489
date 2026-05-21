#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

uint32_t g_ipTxPackets = 0;
uint32_t g_ipRxPackets = 0;

void IpTxTrace(std::string context, Ptr<const Packet> packet) {
    g_ipTxPackets++;
}

void IpRxTrace(std::string context, Ptr<const Packet> packet, const Address &addr) {
    g_ipRxPackets++;
}

int main(int argc, char* argv[]) {
    uint32_t density = 20;    
    uint32_t cacheSize = 100; 
    uint32_t seed = 1;        
    bool isNDN = true;        
    double simTime = 150.0;     
    double g_warmUpTime = 10.0; 

    CommandLine cmd;
    cmd.AddValue("density", "Number of vehicles", density);
    cmd.AddValue("cacheSize", "Content Store size", cacheSize);
    cmd.AddValue("seed", "Random seed", seed);
    cmd.AddValue("isNDN", "Enable NDN stack (true) or IP-Push (false)", isNDN);
    cmd.Parse(argc, argv);

    RngSeedManager::SetSeed(seed);

    std::string traceFile = "traces/density_" + std::to_string(density) + ".tcl";

    NodeContainer vehicles;
    vehicles.Create(density);
    NodeContainer rsu;
    rsu.Create(1);

    Ns2MobilityHelper ns2Mobility(traceFile);
    ns2Mobility.Install();

    Ptr<ListPositionAllocator> rsuPos = CreateObject<ListPositionAllocator>();
    rsuPos->Add(Vector(500.0, 500.0, 5.0)); 
    MobilityHelper rsuMob;
    rsuMob.SetPositionAllocator(rsuPos);
    rsuMob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    rsuMob.Install(rsu);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiChannel.AddPropagationLoss("ns3::TwoRayGroundPropagationLossModel");
    
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.Set("TxPowerStart", DoubleValue(40.0)); 
    wifiPhy.Set("TxPowerEnd", DoubleValue(40.0));

    // --- PATH B FIX: Using Standard Ad-Hoc WiFi instead of WAVE ---
    WifiHelper wifi; // Fixed: Removed ::Default()
    wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6Mbps"),
                                 "ControlMode", StringValue("OfdmRate6Mbps"));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer vDevices = wifi.Install(wifiPhy, mac, vehicles);
    NetDeviceContainer rsuDevice = wifi.Install(wifiPhy, mac, rsu);
    // --------------------------------------------------------------

    if (isNDN) {
        ns3::ndn::StackHelper ndnHelper;
        ndnHelper.setCsSize(cacheSize);
        ndnHelper.setPolicy("nfd::cs::lru"); 
        ndnHelper.InstallAll();
        
        for (uint32_t i = 0; i < density; ++i) {
            Ptr<Node> node = vehicles.Get(i);
            Ptr<ns3::ndn::L3Protocol> ndnL3 = node->GetObject<ns3::ndn::L3Protocol>();
            auto face = ndnL3->getFaceByNetDevice(vDevices.Get(i));
            ns3::ndn::FibHelper::AddRoute(node, "/parking", face, 1);
        }
        
        Ptr<ns3::ndn::L3Protocol> rsuL3 = rsu.Get(0)->GetObject<ns3::ndn::L3Protocol>();
        auto rsuFace = rsuL3->getFaceByNetDevice(rsuDevice.Get(0));
        ns3::ndn::FibHelper::AddRoute(rsu.Get(0), "/parking", rsuFace, 1);
        
        ns3::ndn::StrategyChoiceHelper::InstallAll("/parking", "/localhost/nfd/strategy/multicast-vanet");

        ns3::ndn::AppHelper producerHelper("ns3::ndn::Producer");
        producerHelper.SetPrefix("/parking/mall_A/status");
        producerHelper.SetAttribute("PayloadSize", UintegerValue(1024));
        producerHelper.SetAttribute("Freshness", TimeValue(Seconds(5.0))); 
        ApplicationContainer producer = producerHelper.Install(rsu);
        producer.Start(Seconds(g_warmUpTime));
        producer.Stop(Seconds(simTime));

        ns3::ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
        consumerHelper.SetPrefix("/parking/mall_A/status");
        consumerHelper.SetAttribute("Frequency", DoubleValue(2.0));      
        consumerHelper.SetAttribute("LifeTime", TimeValue(Seconds(4.0)));
        ApplicationContainer consumers = consumerHelper.Install(vehicles);
        consumers.Start(Seconds(g_warmUpTime));
        consumers.Stop(Seconds(simTime));
        
        ns3::ndn::AppDelayTracer::InstallAll("app-delays.txt");
        ns3::ndn::L3RateTracer::InstallAll("rate-trace.txt", Seconds(1.0));
        ns3::ndn::CsTracer::InstallAll("cs-trace.txt", Seconds(1.0));
        
    } else {
        InternetStackHelper internet;
        internet.Install(vehicles);
        internet.Install(rsu);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4.Assign(vDevices);
        ipv4.Assign(rsuDevice);

        uint16_t port = 9000;
        OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address("255.255.255.255"), port)));
        onoff.SetConstantRate(DataRate("16kbps")); 
        onoff.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer rsuApp = onoff.Install(rsu);
        rsuApp.Start(Seconds(g_warmUpTime));
        rsuApp.Stop(Seconds(simTime));

        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer vApps = sink.Install(vehicles);
        vApps.Start(Seconds(g_warmUpTime));
        vApps.Stop(Seconds(simTime));
        
        Config::Connect("/NodeList/*/ApplicationList/*/$ns3::OnOffApplication/Tx", MakeCallback(&IpTxTrace));
        Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(&IpRxTrace));
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    std::cout << "\n=================================================" << std::endl;
    std::cout << "          ACTUAL SIMULATION METRICS LOG          " << std::endl;
    std::cout << "=================================================" << std::endl;
    std::cout << "Method      : " << (isNDN ? "V-NDN" : "IP-Push") << std::endl;
    std::cout << "Density     : " << density << " vehicles" << std::endl;
    
    if (isNDN) {
        std::cout << "Cache Size  : " << cacheSize << " packets" << std::endl;
        std::cout << "-------------------------------------------------" << std::endl;
        std::cout << "NDN simulation completed successfully." << std::endl;
    } else {
        std::cout << "-------------------------------------------------" << std::endl;
        std::cout << "IP Tx Packets (Overhead) : " << g_ipTxPackets << std::endl;
        std::cout << "IP Rx Packets (Delivered): " << g_ipRxPackets << std::endl;
    }
    std::cout << "=================================================\n" << std::endl;

    return 0;
}
