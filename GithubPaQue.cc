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
#include "ns3/header.h"
#include "ns3/ipv4-address.h"

#include "ns3/rng-seed-manager.h"
#include <chrono>

using namespace ns3;
using namespace dsr;


 class MyHeader : public Header 
 {
 public:
  
   MyHeader ();
   virtual ~MyHeader ();
  
  void SetData (std::string data);
  std::string GetData (void) const;
  
   static TypeId GetTypeId (void);
   virtual TypeId GetInstanceTypeId (void) const;
   virtual void Print (std::ostream &os) const;
   virtual void Serialize (Buffer::Iterator start) const;
   virtual uint32_t Deserialize (Buffer::Iterator start);
   virtual uint32_t GetSerializedSize (void) const;
 private:
  std::string m_strData;
 };
  
 MyHeader::MyHeader ()
 {
   // we must provide a public default constructor, 
   // implicit or explicit, but never private.
 }
 MyHeader::~MyHeader ()
 {
 }
  
 TypeId
 MyHeader::GetTypeId (void)
 {
   static TypeId tid = TypeId ("ns3::MyHeader")
     .SetParent<Header> ()
     .AddConstructor<MyHeader> ()
   ;
   return tid;
 }
 TypeId
 MyHeader::GetInstanceTypeId (void) const
 {
   return GetTypeId ();
 }
  
 void
 MyHeader::Print (std::ostream &os) const
 {
   // This method is invoked by the packet printing
   // routines to print the content of my header.
   //os << "data=" << m_strData << std::endl;
   os << "data=" << m_strData;
 }
uint32_t MyHeader::GetSerializedSize (void) const {
  // Calcula el tamaño necesario para serializar el string
  // Necesitamos 4 bytes para el tamaño del string y luego los bytes del propio string
  return 4 + m_strData.size();
}
void MyHeader::Serialize (Buffer::Iterator start) const {
  // Primero escribimos el tamaño del string, luego el string mismo
  start.WriteHtonU32 (m_strData.size());
  start.Write ((const uint8_t *) m_strData.c_str(), m_strData.size());
}

uint32_t MyHeader::Deserialize (Buffer::Iterator start) {
  // Primero leemos el tamaño del string
  uint32_t strSize = start.ReadNtohU32();

  // Luego leemos los caracteres del string y lo construimos
  char strData[strSize + 1]; // +1 para el caracter nulo al final
  start.Read ((uint8_t *) strData, strSize);
  strData[strSize] = '\0'; // asegurar que es una cadena terminada en nulo
  m_strData = std::string(strData);

  // Retornamos el tamaño total deserializado
  return 4 + strSize;
}
  
void MyHeader::SetData (std::string data) {
  m_strData = data;
}

std::string MyHeader::GetData (void) const {
  return m_strData;
}

NS_LOG_COMPONENT_DEFINE ("AdHocRescueSimulation");

NodeContainer rescatistas;
NodeContainer notificadores;
NodeContainer centrales;
Ipv4InterfaceContainer rescatistaInterfaces;
NodeContainer allNodes;
Ipv4InterfaceContainer allInterfaces;

int numNotificadores = 5;
int numRescatistas = 5;
int numCentrales = 2;


Ptr<Node> FindNodeWithIpAddressInInterfaces(std::string ipString, Ipv4InterfaceContainer &allInterfaces) {
    Ipv4Address ip = Ipv4Address(ipString.c_str()); // Convertir string a Ipv4Address

    // Iterar sobre todas las interfaces en el Ipv4InterfaceContainer
    for (uint32_t i = 0; i < allInterfaces.GetN(); ++i) {
        Ipv4Address addr = allInterfaces.GetAddress(i);
        if (addr == ip) {
            // Obtener la interfaz
            Ptr<Ipv4> ipv4 = allInterfaces.Get(i).first;
            // int32_t interface = allInterfaces.Get(i).second;

            // Buscar el nodo que posee esta interfaz
            Ptr<Node> node;
            for (uint32_t j = 0; j < NodeContainer::GetGlobal().GetN(); j++) {
                node = NodeContainer::GetGlobal().Get(j);
                if (node->GetObject<Ipv4>() == ipv4) {
                    return node; // Nodo encontrado
                }
            }
        }
    }

    // Si llegamos aquí, no se encontró el nodo con la dirección IP dada
    return nullptr;
}



// Declaración de funciones auxiliares (deberás implementar estas según tus necesidades)
void CourseChange (std::string context, Ptr<const MobilityModel> model);
void ReceivePacket (Ptr<Socket> socket);
void SendPacket (Ptr<Socket> socket);
void NodeStopped (Ptr<Node> node); // Puedes necesitar una lógica para manejar cuándo un nodo se detiene

// Notificador -> Central
void EnviarMensajeNotificador(Ptr<Socket> socket, Ipv4Address dstAddr, uint16_t port) {
    // Crear un paquete y añadirle datos si es necesario
    Ptr<Packet> paquete = Create<Packet>(1000);

    // Enviar el paquete al nodo central
    int bytes_enviados = socket->SendTo(paquete, 0, InetSocketAddress(dstAddr, port));
    
    if (bytes_enviados > 0) {
        // NS_LOG_INFO("Se enviaron satisfactoriamente " << bytes_enviados << " bytes desde el notificador.");
        NS_LOG_INFO("Enviado a central: " << dstAddr);
    } else {
        NS_LOG_INFO("Error al enviar el mensaje desde el notificador. Código de error: " << socket->GetErrno());
    }
}


// Central -> Rescatista
void EnviarMensajeCentral(Ptr<Socket> socket, Ipv4Address dstAddr, uint16_t port) {
    // Crear un paquete y añadirle datos si es necesario
    Ptr<Packet> paquete = Create<Packet>();

    // Enviar el paquete al nodo central
    socket->SendTo(paquete, 0, InetSocketAddress(dstAddr, port));
    NS_LOG_INFO("Enviado mensaje desde central");
}


void RecibirEnNotificadores(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from))) {
        if (packet->GetSize() > 0) {

            MyHeader RescatistaIpHeader;
            packet->RemoveHeader (RescatistaIpHeader);
            // Obtener la dirección IP de destino del header
            std::string rescatistaIp = RescatistaIpHeader.GetData ();

            NS_LOG_INFO("Notificador recibe mensaje de: " << rescatistaIp);
            NS_LOG_INFO("--------------------------------------------------------------------------------------------");

        }

    }

}


// Rescatista recibe mensaje de central
void RecibirEnRescatista(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from))) {
        if (packet->GetSize() > 0) {


            MyHeader RescatistaIpHeader;
            packet->RemoveHeader (RescatistaIpHeader);
            // Obtener la dirección IP de destino del header
            std::string rescatistaIp = RescatistaIpHeader.GetData ();
            // NS_LOG_INFO("Dirección ip del rescatista: " << rescatistaIp);

            MyHeader NotificadorIpHeader;
            packet->RemoveHeader (NotificadorIpHeader);
            // Obtener la dirección IP de destino del header
            std::string notificadorIp = NotificadorIpHeader.GetData ();
            // NS_LOG_INFO("Dirección ip del notificador: " << notificadorIp);


            // Convertir la dirección a InetSocketAddress
            InetSocketAddress address = InetSocketAddress::ConvertFrom(from);
            Ipv4Address senderIp = address.GetIpv4();

            // El rescatista ha recibido un paquete
            // NS_LOG_INFO("Rescatista de la central " << senderIp << " recibió un mensaje: " << packet->GetSize() << " bytes");


            Ptr<Node> node = centrales.Get (1);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> (); // Obtener la instancia de IPv4 asociada al nodo
            Ipv4InterfaceAddress iaddr = ipv4->GetAddress (1,0); // El índice 0 suele ser la dirección de bucle invertido
            Ipv4Address centralAddr = iaddr.GetLocal();

            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            Ptr<Node> rescatistaNodo = FindNodeWithIpAddressInInterfaces(rescatistaIp, allInterfaces);
            Ptr<Socket> source = Socket::CreateSocket(rescatistaNodo->GetObject<Node>(), tid); // 'central' debe ser el Ptr<Node> del nodo central

            InetSocketAddress remote = InetSocketAddress(centralAddr, 80); // usar el puerto que se está usando para los rescatistas
            source->Connect(remote);

            // Reenviar el paquete al rescatista
            MyHeader ipHeaderNotificador;
            ipHeaderNotificador.SetData(notificadorIp);
            packet->AddHeader(ipHeaderNotificador);

            // Reenviar el paquete al rescatista
            source->Send(packet);
            NS_LOG_INFO("Rescatista: " << rescatistaIp << " recibió de central: " << senderIp << " y envia a central " << centralAddr);

            // opcional: cerrar el socket si no se va a usar más
            source->Close();

        }

    }

}


void RecibirEnCentralDesdeRescatistas(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;   
    while ((packet = socket->RecvFrom(from))) {
        if (packet->GetSize() > 0) {


            MyHeader NotificadorIpHeader;
            packet->RemoveHeader (NotificadorIpHeader);
            // Obtener la dirección IP de destino del header
            std::string notificadorIp = NotificadorIpHeader.GetData ();
            
            // Convertir la dirección a InetSocketAddress
            InetSocketAddress address = InetSocketAddress::ConvertFrom(from);
            Ipv4Address senderIp = address.GetIpv4();;
            // NS_LOG_INFO("Rescatista " << senderIp << " a notificador " << notificadorIp);

            // Crear un socket para enviar datos
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            Ptr<Socket> source = Socket::CreateSocket(centrales.Get(1)->GetObject<Node>(), tid); // 'central' debe ser el Ptr<Node> del nodo central

            Ptr<Node> notificadorNodo = FindNodeWithIpAddressInInterfaces(notificadorIp, allInterfaces);
            Ipv4Address notificadorAddr = notificadorNodo->GetObject<Ipv4> ()->GetAddress(1, 0).GetLocal();
            
            InetSocketAddress remote = InetSocketAddress(notificadorAddr, 80); // usar el puerto que se está usando para los rescatistas
            source->Connect(remote);

            MyHeader ipHeaderRescatista;
            std::stringstream ss;
            senderIp.Print(ss);
            ipHeaderRescatista.SetData(ss.str());
            packet->AddHeader(ipHeaderRescatista);

            source->Send(packet);
            NS_LOG_INFO("Central envia a notificador: " << notificadorAddr);

            // opcional: cerrar el socket si no se va a usar más
            source->Close();
        }
    }
}


// Central recibe mensaje de notificador
void RecibirEnCentralDesdeNotificadores(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;   
    while ((packet = socket->RecvFrom(from))) {
        if (packet->GetSize() > 0) {
            // Aquí el central ha recibido un paquete y ahora va a enviarlo a un rescatista
            InetSocketAddress address = InetSocketAddress::ConvertFrom(from);
            Ipv4Address senderIp = address.GetIpv4();
            // NS_LOG_INFO("Central recibió un mensaje del rescatista: " << senderIp);
            // Seleccionar un rescatista aleatorio
            int numRescatistas = rescatistas.GetN(); // 'rescatistas' debe ser un NodeContainer accesible
            int rescatistaAleatorioIndex = rand() % numRescatistas; // selecciona un índice aleatorio
            Ptr<Node> rescatistaAleatorio = rescatistas.Get(rescatistaAleatorioIndex);

            // Obtener la dirección IP del rescatista (asumiendo que las direcciones están en rescatistaInterfaces)
            Ipv4Address rescatistaAddr = allInterfaces.GetAddress(numRescatistas+rescatistaAleatorioIndex);

            // Crear un socket para enviar datos
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            Ptr<Socket> source = Socket::CreateSocket(centrales.Get(0)->GetObject<Node>(), tid); // 'central' debe ser el Ptr<Node> del nodo central

            InetSocketAddress remote = InetSocketAddress(rescatistaAddr, 80); // usar el puerto que se está usando para los rescatistas
            source->Connect(remote);

            // Reenviar el paquete al rescatista
            MyHeader ipHeaderRescatista;
            MyHeader ipHeaderNotificador;
            std::stringstream ss;
            rescatistaAddr.Print(ss);
            ipHeaderRescatista.SetData(ss.str());
            std::stringstream ss2;
            senderIp.Print(ss2);
            ipHeaderNotificador.SetData(ss2.str());
            packet->AddHeader(ipHeaderNotificador);
            packet->AddHeader(ipHeaderRescatista);
            NS_LOG_INFO("Central envia a rescatista: " << ss.str());
            source->Send(packet);

            // opcional: cerrar el socket si no se va a usar más
            source->Close();
        }
    }
}

int main (int argc, char *argv[])
{
    // Activar NS_LOG para el componente deseado con nivel INFO
    LogComponentEnable("AdHocRescueSimulation", LOG_LEVEL_INFO);
    NS_LOG_INFO("Iniciando simulación");
    // Variables de configuración
    bool verbose = false;

    double simulationTime = 10; // tiempo de simulación en segundos
    std::string routingProtocol = "AODV"; // protocolo de enrutamiento, paramet
    // Establezca una semilla aleatoria basada en el tiempo actual
    // Esto hará que los números generados sean diferentes en cada ejecución
    ns3::RngSeedManager::SetSeed (std::chrono::system_clock::now().time_since_epoch().count());


    if (verbose) {
	LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
	LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }


    // Parsear argumentos de línea de comandos si los hay
    CommandLine cmd (__FILE__);
    cmd.AddValue ("verbose", "Activar registros de paquetes UDP", verbose);
    //cmd.AddValue ("simulationTime", "Duracion de la simulacion", simulationTime);
    cmd.AddValue ("numNotificadores", "No. de notificadores", numNotificadores);
    cmd.AddValue ("numRescatistas", "No. de rescatistas", numRescatistas);
    cmd.AddValue ("numCentrales", "No. de nodos centrales", numCentrales);
    cmd.AddValue ("routingProtocol", "Tipo de protocolo de enrutamiento", routingProtocol);
    cmd.Parse (argc, argv);

 
    notificadores.Create (numNotificadores);
    rescatistas.Create (numRescatistas);
    centrales.Create (numCentrales);

    allNodes.Add(notificadores);
    allNodes.Add(rescatistas);
    allNodes.Add(centrales);

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


    else if (routingProtocol == "OLSR") {
        OlsrHelper olsr;
        stack.SetRoutingHelper(olsr);
    }

    // stack.Install (notificadores);
    // stack.Install (rescatistas);
    // stack.Install (centrales);
    stack.Install(allNodes);

    
    if (routingProtocol == "DSR") {
	dsrMain.Install (dsr, allNodes);
    }

    
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper wifiPhy; 
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiHelper wifi;
    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");
    wifi.SetStandard (WIFI_STANDARD_80211g);

    NetDeviceContainer notificadorDevices, rescatistaDevices, centralDevices;

    notificadorDevices = wifi.Install (wifiPhy, wifiMac, notificadores);
    rescatistaDevices = wifi.Install (wifiPhy, wifiMac, rescatistas);
    centralDevices = wifi.Install (wifiPhy, wifiMac, centrales);

    NetDeviceContainer allDevices;
    allDevices.Add(notificadorDevices);
    allDevices.Add(rescatistaDevices);
    allDevices.Add(centralDevices);

    // Asignación de direcciones IP
    Ipv4AddressHelper address;

    address.SetBase("10.1.0.0", "255.255.0.0"); // todos los nodos estarán en esta subred
    allInterfaces = address.Assign(allDevices);


    // Configuración de movilidad
    MobilityHelper mobility;

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");//,
		    	       //"Bounds",
			       //RectangleValue (Rectangle (-100, 100, -100, 100)));
    mobility.Install (centrales);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
		    		  "MinX", DoubleValue (0.0),
		    		  "MinY", DoubleValue (0.0),
		    		  "DeltaX", DoubleValue (10.0),
		    		  "DeltaY", DoubleValue (20.0),
		    		  "GridWidth", UintegerValue (3),
		    		  "LayoutType", StringValue ("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
		    	      "Bounds",
			      RectangleValue(Rectangle(-100, 100, -100, 100)));
    mobility.Install(notificadores);    
    mobility.Install(rescatistas);
    

    // Crear un tipo de socket y configurarlo según sea necesario
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

    // Configurar socket en nodo central para recibir mensajes
    for (int i = 0; i < numCentrales; i++) {
        Ptr<Node> node = centrales.Get (i);
        Ptr<Socket> recvSocket = Socket::CreateSocket(node, tid);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> (); // Obtener la instancia de IPv4 asociada al nodo
        Ipv4InterfaceAddress iaddr = ipv4->GetAddress (1,0);
        InetSocketAddress local = InetSocketAddress(iaddr.GetLocal(), 80); // suponiendo que usamos el puerto 80
        recvSocket->Bind(local);
        if (i == 0){
            recvSocket->SetRecvCallback(MakeCallback(&RecibirEnCentralDesdeNotificadores));
        }else {
            recvSocket->SetRecvCallback(MakeCallback(&RecibirEnCentralDesdeRescatistas));
        }
    }

    // Configurar socket en nodos rescatistas para recibir mensajes
    for (int i = 0; i < numRescatistas; i++) {
        Ptr<Node> node = rescatistas.Get(i);
        Ptr<Socket> recvSocket = Socket::CreateSocket(node, tid);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>(); // Obtener la instancia de IPv4 asociada al nodo
        Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1, 0);
        InetSocketAddress local = InetSocketAddress(iaddr.GetLocal(), 80); // suponiendo que usamos el puerto 80
        recvSocket->Bind(local);
        recvSocket->SetRecvCallback(MakeCallback(&RecibirEnRescatista));
    }

    // Configurar socket en nodos rescatistas para recibir mensajes
    for (int i = 0; i < numNotificadores; i++) {
        Ptr<Node> node = notificadores.Get(i);
        Ptr<Socket> recvSocket = Socket::CreateSocket(node, tid);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>(); // Obtener la instancia de IPv4 asociada al nodo
        Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1, 0);
        InetSocketAddress local = InetSocketAddress(iaddr.GetLocal(), 80); // suponiendo que usamos el puerto 80
        recvSocket->Bind(local);
        recvSocket->SetRecvCallback(MakeCallback(&RecibirEnNotificadores));
    }

    Ptr<ExponentialRandomVariable> x = CreateObject<ExponentialRandomVariable> ();
    x->SetAttribute ("Mean", DoubleValue (5.0)); // La media es 5.0


    // Configurar sockets en nodos notificadores para enviar mensajes
    for (int i = 0; i < numNotificadores; i++) {
        Ptr<Socket> sendSocket = Socket::CreateSocket(notificadores.Get(i), tid);
        Ptr<Node> node = centrales.Get(0);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1, 0);
        // Programar el envío de mensajes para tiempo aleatorio
        // Crea una variable aleatoria exponencial con una media deseada.
        // Genera un valor aleatorio.
        double value = x->GetValue();
        NS_LOG_INFO("Mensaje eviado desde notificador: " << notificadores.Get(i)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() <<
        " En el segundo: " << value);
        Simulator::Schedule(Seconds(value), &EnviarMensajeNotificador, sendSocket, iaddr.GetLocal(), 80); // enviar a la primera dirección central
    }

    // Imprimir todas las direcciones IP
    // for (uint32_t i = 0; i < centrales.GetN(); ++i)
    // {
    //     Ptr<Node> node = centrales.Get (i);
    //     Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> (); // Obtener la instancia de IPv4 asociada al nodo
    //     Ipv4InterfaceAddress iaddr = ipv4->GetAddress (1,0); // El índice 0 suele ser la dirección de bucle invertido (127.0.0.1)

    //     Ipv4Address ipAddr = iaddr.GetLocal ();
    //     std::cout << "Nodo " << i << " tiene la dirección IP: " << ipAddr << std::endl;
    // }

    // Iniciar simulación
    //Simulator::Stop (Seconds (simulationTime));
    Simulator::Stop (Seconds (simulationTime));
    Simulator::Run ();


    // TODO: Procesar los resultados de la simulación para obtener métricas

    Simulator::Destroy ();
    return 0;
}
