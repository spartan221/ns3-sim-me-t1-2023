#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"

// Protocolos de enrutamiento
#include "ns3/aodv-helper.h" 
#include "ns3/dsr-module.h"
#include "ns3/olsr-module.h"

#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;
using namespace dsr;

NS_LOG_COMPONENT_DEFINE ("AdHocRescueSimulation");

// Declaración de funciones auxiliares (deberás implementar estas según tus necesidades)
void CourseChange (std::string context, Ptr<const MobilityModel> model);
void ReceivePacket (Ptr<Socket> socket);
void SendPacket (Ptr<Socket> socket);
void NodeStopped (Ptr<Node> node); // Puedes necesitar una lógica para manejar cuándo un nodo se detiene

int main (int argc, char *argv[])
{
    // Variables de configuración
    double simulationTime = 10; // tiempo de simulación en segundos
    std::string routingProtocol = "AODV"; // protocolo de enrutamiento, parametrizable

    // Parsear argumentos de línea de comandos si los hay
    CommandLine cmd (__FILE__);
    cmd.AddValue ("routingProtocol", "Tipo de protocolo de enrutamiento", routingProtocol);
    cmd.Parse (argc, argv);

    // Configuración de nodos
    NodeContainer notificadores;
    notificadores.Create (5);

    NodeContainer rescatistas;
    rescatistas.Create (5);

    NodeContainer centrales;
    centrales.Create (2);

    // Configuración de la pila de protocolos de internet
    InternetStackHelper stack;

    // En caso de que el protocolo sea DSR es necesario instancias
    // un DsrMainHelper para su configuracion en la pila (también
    // es necesario instanciar DsrHelper para no confundirse con
    // el namespace. 
    DsrHelper dsr;
    DsrMainHelper dsrMain;

    if (routingProtocol == "AODV") {
        AodvHelper aodv;
        stack.SetRoutingHelper (aodv);  
    } 
    //else if (routingProtocol == "DSR") {
    //    stack.SetRoutingHelper(dsr);
    //}
    else if (routingProtocol == "OLSR") {
        OlsrHelper olsr;
        stack.SetRoutingHelper(olsr);
    }

    stack.Install (notificadores);
    stack.Install (rescatistas);
    stack.Install (centrales);

    
    if (routingProtocol == "DSR") {
	dsrMain.Install (dsr, notificadores);
	dsrMain.Install (dsr, rescatistas);
	dsrMain.Install (dsr, centrales);
    }

    // Configuración de canal y dispositivos de red
    WifiHelper wifi;
    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");
    wifi.SetStandard (WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode",
                                StringValue ("OfdmRate54Mbps"));

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());


    NetDeviceContainer notificadorDevices, rescatistaDevices, centralDevices;
    notificadorDevices = wifi.Install (wifiPhy, wifiMac, notificadores);
    rescatistaDevices = wifi.Install (wifiPhy, wifiMac, rescatistas);
    centralDevices = wifi.Install (wifiPhy, wifiMac, centrales);

    // Asignación de direcciones IP
    Ipv4AddressHelper address;

    // Para notificadores
    address.SetBase ("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer notificadorInterfaces;
    notificadorInterfaces = address.Assign (notificadorDevices);

    // Para rescatistas
    address.SetBase ("10.2.0.0", "255.255.0.0");
    Ipv4InterfaceContainer rescatistaInterfaces;
    rescatistaInterfaces = address.Assign (rescatistaDevices);

    // Para centrales
    address.SetBase ("10.3.0.0", "255.255.0.0");
    Ipv4InterfaceContainer centralInterfaces;
    centralInterfaces = address.Assign (centralDevices);


    // Configuración de movilidad
    MobilityHelper mobility;

    // Posición para el clúster de la capa superior (central)
    Ptr<ListPositionAllocator> positionAllocCentral = CreateObject<ListPositionAllocator> ();
    // Suponiendo que quieres que el clúster central esté en las coordenadas (0,0)
    positionAllocCentral->Add (Vector (0.0, 0.0, 0.0)); // Si tienes más nodos en el clúster central, añade más posiciones aquí
    mobility.SetPositionAllocator (positionAllocCentral);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (centrales);

    // Posición para los clústeres de la capa inferior (notificadores y rescatistas)
    // Aquí, estamos colocando los notificadores y rescatistas en círculos alrededor del clúster central
    int numNotificadores = notificadores.GetN();
    int numRescatistas = rescatistas.GetN();
    double radioNotificadores = 30.0; // Define el radio del círculo para los notificadores
    double radioRescatistas = 50.0; // Define el radio del círculo para los rescatistas

    Ptr<ListPositionAllocator> positionAllocNotificadores = CreateObject<ListPositionAllocator> ();
    for (int i = 0; i < numNotificadores; i++) {
        double angle = (i * 2 * M_PI) / numNotificadores;
        double x = cos(angle) * radioNotificadores;
        double y = sin(angle) * radioNotificadores;
        positionAllocNotificadores->Add(Vector(x, y, 0.0));
    }
    mobility.SetPositionAllocator(positionAllocNotificadores);
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)));
    mobility.Install(notificadores);

    Ptr<ListPositionAllocator> positionAllocRescatistas = CreateObject<ListPositionAllocator> ();
    for (int i = 0; i < numRescatistas; i++) {
        double angle = (i * 2 * M_PI) / numRescatistas;
        double x = cos(angle) * radioRescatistas;
        double y = sin(angle) * radioRescatistas;
        positionAllocRescatistas->Add(Vector(x, y, 0.0));
    }

    mobility.SetPositionAllocator(positionAllocRescatistas);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel"); // Suponiendo que los rescatistas no se mueven; cambia si es necesario
    mobility.Install(rescatistas);

    mobility.SetPositionAllocator (positionAllocNotificadores);
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50))); // ejemplo de área de movimiento
    mobility.Install (notificadores);


    

    // Configuración de aplicaciones (aquí necesitas configurar tus aplicaciones de envío/recepción de mensajes)
    // ...

    // Callbacks para rastrear la movilidad o recepción/envío de paquetes
    // ...

    // Iniciar simulación
    Simulator::Stop (Seconds (simulationTime));
    Simulator::Run ();

    // Procesar los resultados de la simulación para obtener métricas
    // ...

    Simulator::Destroy ();
    return 0;
}
