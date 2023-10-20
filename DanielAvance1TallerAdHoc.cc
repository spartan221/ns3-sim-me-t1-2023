// Este modelo está basado en el ejemplo mixed-wired.wireless.cs
// disponible en la página web de NS3


#include "ns3/command-line.h"
#include "ns3/string.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/qos-txop.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/csma-helper.h"
#include "ns3/animation-interface.h"

using namespace ns3;

// Nombre del modelo
NS_LOG_COMPONENT_DEFINE ("TallerAdHoc");

// Rastreo de los nodos cuando cambien de posición
static void
CourseChangeCallback (std::string path, Ptr<const MobilityModel> model)
{
    Vector position = model->GetPosition ();
    std::cout << "CourseChange: " << path << " x=" << position.x << ", y=" << position.y
              << ", z=" << position.z << std::endl;
}

int
main (int argc, char *argv[])
{
    // Parámetros de la simulación
    uint32_t nCluster = 2;
    uint32_t notifiersNodes = 5;
    uint32_t helpersNodes = 3;
    uint32_t centralsNodes = 2;
    uint32_t stopTime = 20;
    bool useCourseChangeCallback = true;

    LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // 
    Config::SetDefault ("ns3::OnOffApplication::PacketSize", StringValue("1472"));
    Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue("100kb/s"));

    // Parámetros de la línea de comandos
    // ejemplo: 
    // "./ns3 run scratch/TallerAdHoc.cc --nCluster=3"

    CommandLine cmd (__FILE__);
    cmd.AddValue ("nCluster", "Número de clusters", nCluster);
    cmd.AddValue ("notifiersNodes", "Número de nodos notificadores", notifiersNodes);
    cmd.AddValue ("helpersNodes", "Número de nodos ayudantes", helpersNodes);
    cmd.AddValue ("stopTime", "Tiempo de simulación (segundos)", stopTime);
    cmd.AddValue ("useCourseChangeCallback", "Habilitar rastreo de nodos", useCourseChangeCallback);
    cmd.Parse (argc, argv);

    // Se espera que el valor mínimo del tiempo sea 10 segundos
    if (stopTime < 10)
    {
        std::cout << "El tiempo de simulación debe ser mayor o igual a 10 segundos" << std::endl;
        exit (1);
    }

    // ---------------------------
    // Creación de los clusters
    // ---------------------------

    // Creación de un contenedor para los clusters
    NodeContainer cluster;
    cluster.Create (nCluster);

    // Creación de los dispositivos wifi e instalación de los mismos en los nodos cluster usando el contenedor
    WifiHelper wifi;
    WifiMacHelper mac;
    mac.SetType ("ns3::AdhocWifiMac");
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
    				  "DataMode", StringValue ("OfdmRate54Mbps"));
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());
    NetDeviceContainer clusterDevices = wifi.Install (wifiPhy, mac, cluster);

    // Activación de OLSR para redes ad hoc,
    // Eventualmente se utilizará AODV
    NS_LOG_INFO ("Activando OLSR");
    OlsrHelper olsr;

    // Adición de los protocolos IPV4 a los nodos cluster
    InternetStackHelper internet;
    internet.SetRoutingHelper (olsr); // Se activa en el siguiente internet.Install()
    internet.Install (cluster);

    // Asignación de direcciones IPV4 a los nodos cluster
    Ipv4AddressHelper ipAddrs;
    ipAddrs.SetBase ("192.168.0.0", "255.255.255.0");
    ipAddrs.Assign (clusterDevices);

    // Asignación de modelo de movilidad a los nodos cluster
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (20.0),
                                   "MinY", DoubleValue (20.0),
                                   "DeltaX", DoubleValue (20.0),
                                   "DeltaY", DoubleValue (20.0),
                                   "GridWidth", UintegerValue (5),
                                   "LayoutType", StringValue ("RowFirst"));
    /*mobility.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
                               "Bounds", RectangleValue (Rectangle (-500, 500, -500, 500)), 
                               "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=2]"),
                               "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.2]"));*/
    mobility.Install (cluster);

    // --------------------------------------------------
    // Construcción del escenario de simulación
    // --------------------------------------------------

    // El escenario cuenta con 3 clusters, 1 en un nivel, y dos en el nivel inferior
    // El nivel más alto cuenta con un cluster que tiene un nodo que es la central de rescate
    // El nivel inferior cuenta con dos clusters, el primer cluster tiene 5 nodos que son los notificadores
    // El segundo cluster tiene 2 nodos que son los rescatistas

    //
    // Construcción de las LANs
    //

    // Asignación de la base de IPS para configurar las LANs

    ipAddrs.SetBase ("172.16.0.0", "255.255.255.0");

    for (uint32_t i = 0; i < nCluster; ++i)
    {
        NS_LOG_INFO ("Configurando LAN conectadas a cada central (clúster node)" << i);
        
        // Se crea un contenedor para los nodos de la LAN
        NodeContainer newLanNodes;
        newLanNodes.Create (centralsNodes - 1);

        // Se añade a la LAN el cluster respectivo
        NodeContainer lan (cluster.Get (i), newLanNodes);

        // Se crean los dispositivos CSMA y se instalan en los nodos de la LAN
        CsmaHelper csma;
        csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate (5000000)));
        csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
        NetDeviceContainer lanDevices = csma.Install (lan);

        // Se añaden las interfaces IPv4 a la LAN que no incluye el cluster
        internet.Install (newLanNodes);

        // Se asignan las direcciones IPv4 a los dispositivos que se acaban de crear
        ipAddrs.Assign (lanDevices);

        // Se asigna un prefijo de red para identificar la red
        ipAddrs.NewNetwork ();

        // Se añade un modelo de posición constante a los nodos de la LAN. Sin incluir el cluster
        MobilityHelper mobilityLan;
        Ptr<ListPositionAllocator> subnetAlloc = CreateObject<ListPositionAllocator> ();
        for (uint32_t j = 0; j < newLanNodes.GetN (); ++j)
        {
            subnetAlloc->Add (Vector (0.0, j * 10 + 10, 0.0));
        }
        mobilityLan.PushReferenceMobilityModel (cluster.Get (i));
        mobilityLan.SetPositionAllocator (subnetAlloc);
        mobilityLan.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        mobilityLan.Install (newLanNodes);
    }

    //-----------------------------------------------------------------
    // Construcción de las redes de bajo nivel (notificadores y ayudantes)
    //-----------------------------------------------------------------

    // Asignación de la base de IPS para configurar las redes
    ipAddrs.SetBase ("10.0.0.0", "255.255.255.0");

    for (uint32_t i = 0; i < nCluster; ++i)
    {
        NS_LOG_INFO ("Configurando red de bajo nivel (clúster node) " << i);

        // Se crea un contenedor para los nodos de la red y otro con estos nodos y el respectivo cluster
        NodeContainer stas;
        stas.Create (notifiersNodes - 1);

        NodeContainer infra (cluster.Get (i), stas);

        // Se crea una red de infraestructura
        WifiHelper wifiInfra;
        WifiMacHelper macInfra;
        wifiPhy.SetChannel (wifiChannel.Create ());

        // Se asignan valores SSID a cada nodo
        std::string ssidString ("wifi-infra");
        std::stringstream ss;
        ss << i;
        ssidString += ss.str ();
        Ssid ssid = Ssid (ssidString);

        // Se configura el tipo de MAC y el SSID
        macInfra.SetType ("ns3::StaWifiMac",
                        "Ssid", SsidValue (ssid));
        NetDeviceContainer staDevices = wifiInfra.Install (wifiPhy, macInfra, stas);

        // Se configura el tipo de MAC y el SSID para el nodo cluster
        macInfra.SetType ("ns3::ApWifiMac",
                        "Ssid", SsidValue (ssid));
        NetDeviceContainer apDevices = wifiInfra.Install (wifiPhy, macInfra, cluster.Get (i));

        // Se crea un contenedor con estos dispositivos
        NetDeviceContainer infraDevices (apDevices, staDevices);

        // Se añaden las interfaces IPv4 a la red
        internet.Install (stas);

        // Se asignan las direcciones IPv4 a los dispositivos que se acaban de crear
        ipAddrs.Assign (infraDevices);

        // Se asigna un prefijo de red para identificar la red
        ipAddrs.NewNetwork ();

        // Se añade un modelo de posición constante a los nodos de la red
        Ptr<ListPositionAllocator> subnetAlloc = CreateObject<ListPositionAllocator> ();
        for (uint32_t j = 0; j < infra.GetN (); ++j)
        {
            subnetAlloc->Add (Vector (0.0, j, 0.0));
        }
        mobility.PushReferenceMobilityModel (cluster.Get (i));
        mobility.SetPositionAllocator (subnetAlloc);
        mobility.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
                                   "Bounds", RectangleValue (Rectangle (-10, 10, -10, 10)),
                                   "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=3]"),
                                   "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.4]"));
        mobility.Install (stas);
    }

    // Asignación de la base de IPS para configurar las redes
    ipAddrs.SetBase ("11.0.0.0", "255.255.255.0");

    for (uint32_t i = 0; i < nCluster; ++i)
    {
        NS_LOG_INFO ("Configurando red de bajo nivel (clúster node) " << i);

        // Se crea un contenedor para los nodos de la red y otro con estos nodos y el respectivo cluster
        NodeContainer stas;
        stas.Create (helpersNodes - 1);

        NodeContainer infra (cluster.Get (i), stas);

        // Se crea una red de infraestructura
        WifiHelper wifiInfra;
        WifiMacHelper macInfra;
        wifiPhy.SetChannel (wifiChannel.Create ());

        // Se asignan valores SSID a cada nodo
        std::string ssidString ("wifi-infra");
        std::stringstream ss;
        ss << i;
        ssidString += ss.str ();
        Ssid ssid = Ssid (ssidString);

        // Se configura el tipo de MAC y el SSID
        macInfra.SetType ("ns3::StaWifiMac",
                        "Ssid", SsidValue (ssid));
        NetDeviceContainer staDevices = wifiInfra.Install (wifiPhy, macInfra, stas);

        // Se configura el tipo de MAC y el SSID para el nodo cluster
        macInfra.SetType ("ns3::ApWifiMac",
                        "Ssid", SsidValue (ssid));
        NetDeviceContainer apDevices = wifiInfra.Install (wifiPhy, macInfra, cluster.Get (i));

        // Se crea un contenedor con estos dispositivos
        NetDeviceContainer infraDevices (apDevices, staDevices);

        // Se añaden las interfaces IPv4 a la red
        internet.Install (stas);

        // Se asignan las direcciones IPv4 a los dispositivos que se acaban de crear
        ipAddrs.Assign (infraDevices);

        // Se asigna un prefijo de red para identificar la red
        ipAddrs.NewNetwork ();

        // Se añade un modelo de posición constante a los nodos de la red
        Ptr<ListPositionAllocator> subnetAlloc = CreateObject<ListPositionAllocator> ();
        for (uint32_t j = 0; j < infra.GetN (); ++j)
        {
            subnetAlloc->Add (Vector (0.0, j, 0.0));
        }
        mobility.PushReferenceMobilityModel (cluster.Get (i));
        mobility.SetPositionAllocator (subnetAlloc);
        mobility.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
                                   "Bounds", RectangleValue (Rectangle (-10, 10, -10, 10)),
                                   "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=3]"),
                                   "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.4]"));
        mobility.Install (stas);
    }

    // --------------------------------------------------
    // Construcción de la aplicación
    // --------------------------------------------------

    // Se crea una aplicación OnOff para enviar paquetes UDP de tamaño 210 bytes a una tasa de 10kb/s entre dos nodos
    // Se envian desde el primer nodo LAN (centro) hasta un nodo de la red de bajo nivel (rescatista)
    NS_LOG_INFO ("Configurando aplicaciones de envío de paquetes UDP");
    uint16_t port = 9; // Puerto de envío y recepción de paquetes

    // Se deben asegurar las siguientes condiciones:
    NS_ASSERT (centralsNodes > 1 && notifiersNodes > 1 && helpersNodes > 1); // Debe haber al menos un nodo en cada LAN y en cada red de bajo nivel

    // GetNode (nCluster) devuelve el primer nodo creado fuera de los clusters
    Ptr<Node> appSource = NodeList::GetNode (nCluster);

    // El receptor será el último nodo creado
    uint32_t lastNodeIndex = nCluster + nCluster * (centralsNodes - 1) + nCluster * (notifiersNodes - 1) - 1;
    Ptr<Node> appSink = NodeList::GetNode (lastNodeIndex);

    // Se busca la dirección IPv4 del nodo receptor
    Ipv4Address remoteAddr = appSink->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ();

    OnOffHelper onOff ("ns3::UdpSocketFactory", Address (InetSocketAddress (remoteAddr, port)));

    // Se instala OnOff en el nodo fuente
    ApplicationContainer apps = onOff.Install (appSource);
    apps.Start (Seconds (3));
    apps.Stop (Seconds (stopTime - 1));

    // Se crea una aplicación PacketSink para recibir los paquetes UDP
    PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    apps = sink.Install (appSink);

    // Se envía el paquete a través de la red
    apps.Start (Seconds (3));

    // -------------------------------------------------
    // Rastreo de la simulación
    // -------------------------------------------------

    NS_LOG_INFO ("Configurando rastreo de la simulación");
    CsmaHelper csma;

    // Se configura un stream de datos que guardará la información de wifiPHY, CSMA e internet
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("TallerAdHoc.tr");
    wifiPhy.EnableAsciiAll (stream);
    csma.EnableAsciiAll (stream);
    internet.EnableAsciiIpv4All (stream);

    // Se hacen configuraciones adicionales para filtrar la información que se guardará en el stream
    csma.EnablePcapAll ("TallerAdHoc", false);
    wifiPhy.EnablePcap ("TallerAdHoc", clusterDevices, false);
    wifiPhy.EnablePcap ("TallerAdHoc", appSink->GetId (), 0);

    if (useCourseChangeCallback)
    {
        // Se configura un callback para rastrear los cambios de posición de los nodos
        Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange", MakeCallback (&CourseChangeCallback));
    }

    // Se configura el rastreo de la animación
    AnimationInterface anim ("TallerAdHoc.xml");

    // ---------------------------------------------------------------------
    // Ejecución de la simulación
    // ---------------------------------------------------------------------

    NS_LOG_INFO ("Ejecutando simulación");
    Simulator::Stop (Seconds (stopTime));
    Simulator::Run ();
    Simulator::Destroy ();
}
