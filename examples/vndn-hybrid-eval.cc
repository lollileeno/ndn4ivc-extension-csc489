#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/traci-module.h"

// Helper for NDN4IVC
#include "ns3/wifi-setup-helper.h"
#include "ns3/ns3-sumo-setup.h"

using namespace ns3;

int main(int argc, char* argv[]) {
   std::string density = "medium"; 
    std::string strategy = "/localhost/nfd/strategy/hybrid-vanet/%FD%01";

    CommandLine cmd;
    cmd.AddValue("strategy", "Forwarding strategy to use", strategy);
    cmd.AddValue("density", "Traffic density: sparse, medium, or dense", density);
    cmd.Parse(argc, argv);

    std::string sumoTraceFile = "/home/ndn4ivc/ndnSIM/ns-3/contrib/ndn4ivc/traces/grid-map/" + density + ".sumocfg";
    std::string sumoNetFile = "/home/ndn4ivc/ndnSIM/ns-3/contrib/ndn4ivc/traces/grid-map/map.net.xml";

    std::cout << "Running Evaluation with Strategy: " << strategy << std::endl;
    std::cout << "Traffic Density: " << density << std::endl;

    // Wireless Settings (V2X / 802.11p)
    WifiSetupHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211p);
    wifiHelper.SetMac("ns3::OcbWifiMac");
    
    //  set the TXPower
    wifiHelper.SetTxPower(21.0); 

    NodeContainer vehNodes;
    vehNodes.Create(100); //  start with 100 vehicle

    NetDeviceContainer netDevices = wifiHelper.Install(vehNodes);

// setting NDN protocol
    ndn::StackHelper ndnHelper;
    ndnHelper.SetDefaultRoutes(true);
    ndnHelper.Install(vehNodes);

    ndn::StrategyChoiceHelper::InstallAll("/", strategy);

    
    Ptr<TraciClient> sumoClient = CreateObject<TraciClient>();
    sumoClient->SetAttribute("SumoConfigPath", StringValue(sumoTraceFile));
    sumoClient->SetAttribute("SumoNetPath", StringValue(sumoNetFile));

    Ns3SumoSetup sumoSetup;
    sumoSetup.SetSumoClient(sumoClient);
    sumoSetup.SetNodes(vehNodes);
    sumoSetup.SetupMobility(); 
  
    LogComponentEnable("HybridVanetStrategy", LOG_LEVEL_DEBUG);
    LogComponentEnable("ndn.TmsConsumer", LOG_LEVEL_INFO);

    // Start simulation
    Simulator::Stop(Seconds(600.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
