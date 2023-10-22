/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2023 Universidad de Colombia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or GITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Int., 59 Temple Place, Suite 330, Boston, MA 02111-1207 USA
 *
 * Authors: Santiago Acosta 	<sacostaa@unal.edu.co>
 * 	    Julio Bedoya	
 * 	    Jordan Escarraga	<jescarraga@unal.edu.co>
 * 	    Luis Mendez
 * 	    Ivan Morales
 * 	    Daniel Vargas       <danvargasgo@unal.edu.co>
 *
 */

#include "ns3/core-module.h"

#include "ns3/network-module.h"

#include "ns3/mobility-module.h"

#include "ns3/wifi-module.h"

#include "ns3/internet-module.h"

// Protocolos de enrutamiento
#include "ns3/aodv-helper.h"

#include "ns3/dsr-module.h"

#include "ns3/olsr-module.h"

#include "ns3/dsdv-module.h"

#include "ns3/applications-module.h"

#include "ns3/header.h"

#include "ns3/ipv4-address.h"

#include "ns3/rng-seed-manager.h"

#include <chrono>

#include <fstream>

#include <iostream>

using namespace ns3;
using namespace dsr;

// Nombre del programa
NS_LOG_COMPONENT_DEFINE("AdHocRescueSimulation");

// VARIABLES GLOBALES
//
// Contenedores de nodos
NodeContainer rescatistas;
NodeContainer notificadores;
NodeContainer centrales;
NodeContainer allNodes;

// Contenedor de interfaces IPv4 de todos los nodos
Ipv4InterfaceContainer allInterfaces;

int numNotificadores = 5;
int numRescatistas = 5;
int numCentrales = 2;

// Variables para el CSV
int numeroIntentosComunicacion = 0;
int comunicacionesEfectivas = 0;

std::string routingProtocol = "AODV"; // protocolo de enrutamiento AODV o OLSR o DLSR
double simulationTime = 10; // tiempo de simulación en segundos

std::string CSVfileName = "output-simulation.csv";

// Clase y métodos para el header perzonalizado
class MyHeader: public Header {
  public:

    MyHeader();
  virtual~MyHeader();

  void SetData(std::string data);
  std::string GetData(void) const;

  static TypeId GetTypeId(void);
  virtual TypeId GetInstanceTypeId(void) const;
  virtual void Print(std::ostream & os) const;
  virtual void Serialize(Buffer::Iterator start) const;
  virtual uint32_t Deserialize(Buffer::Iterator start);
  virtual uint32_t GetSerializedSize(void) const;
  private: std::string m_strData;
};

MyHeader::MyHeader() {
  // debemos proveer un constructor público por defecto, 
  // implícito o explícito, pero nunca privado.
}
MyHeader::~MyHeader() {}

TypeId
MyHeader::GetTypeId(void) {
  static TypeId tid = TypeId("ns3::MyHeader")
    .SetParent < Header > ()
    .AddConstructor < MyHeader > ();
  return tid;
}
TypeId
MyHeader::GetInstanceTypeId(void) const {
  return GetTypeId();
}

void
MyHeader::Print(std::ostream & os) const {
  // Este método es invocado por los métodos de impresión de paquetes
  // para imprimir el contenido de mi header.
  //os << "data=" << m_strData << std::endl;

  os << "data=" << m_strData;
}
uint32_t MyHeader::GetSerializedSize(void) const {
  // Calcula el tamaño necesario para serializar el string
  // Necesitamos 4 bytes para el tamaño del string y luego los bytes del propio string
  return 4 + m_strData.size();
}
void MyHeader::Serialize(Buffer::Iterator start) const {
  // Primero escribimos el tamaño del string, luego el string mismo
  start.WriteHtonU32(m_strData.size());
  start.Write((const uint8_t * ) m_strData.c_str(), m_strData.size());
}

uint32_t MyHeader::Deserialize(Buffer::Iterator start) {
  // Primero leemos el tamaño del string
  uint32_t strSize = start.ReadNtohU32();

  // Luego leemos los caracteres del string y lo construimos
  char strData[strSize + 1]; // +1 para el caracter nulo al final
  start.Read((uint8_t * ) strData, strSize);
  strData[strSize] = '\0'; // asegurar que es una cadena terminada en nulo
  m_strData = std::string(strData);

  // Retornamos el tamaño total deserializado
  return 4 + strSize;
}

void MyHeader::SetData(std::string data) {
  m_strData = data;
}

std::string MyHeader::GetData(void) const {
  return m_strData;
}

// Función para escribir en el archivo CSV
void
WriteCSVFile(double time, std::string trafficType, Ipv4Address ipSource,
  Ipv4Address ipDest, int bytesSent) {
  std::ofstream out(CSVfileName.c_str(), std::ios::app);

  out << time << "," <<
    trafficType << "," <<
    ipSource << "," <<
    ipDest << "," <<
    bytesSent << "" <<
    std::endl;

  out.close();
}

// Función para imprimir los resultados de la simulación
void FinalPrint() {
  std::cout << "---------------------------------------------------------------\n";
  std::cout << "Resumen de datos\n";
  std::cout << "Tiempo de simulación: " << simulationTime << " segundos \n";
  std::cout << "Protocolo de enrutamiento usado: " << routingProtocol << "\n";
  std::cout << "Número de comunicaciones efectivas: " << comunicacionesEfectivas << "\n";
  std::cout << "Número de intentos de comunicaciones: " << numeroIntentosComunicacion << "\n";
  std::cout << "Porcentaje de comunicaciones exitosas: " << (double) comunicacionesEfectivas / numeroIntentosComunicacion * 100 << "\n";
}

// Función para buscar un nodo con una dirección IP dada
Ptr < Node > FindNodeWithIpAddressInInterfaces(std::string ipString, Ipv4InterfaceContainer & allInterfaces) {
  Ipv4Address ip = Ipv4Address(ipString.c_str()); // Convertir string a Ipv4Address

  // Iterar sobre todas las interfaces en el Ipv4InterfaceContainer
  for (uint32_t i = 0; i < allInterfaces.GetN(); ++i) {
    Ipv4Address addr = allInterfaces.GetAddress(i);
    if (addr == ip) {
      // Obtener la interfaz
      Ptr < Ipv4 > ipv4 = allInterfaces.Get(i).first;
      // int32_t interface = allInterfaces.Get(i).second;

      // Buscar el nodo que posee esta interfaz
      Ptr < Node > node;
      for (uint32_t j = 0; j < NodeContainer::GetGlobal().GetN(); j++) {
        node = NodeContainer::GetGlobal().Get(j);
        if (node -> GetObject < Ipv4 > () == ipv4) {
          return node; // Nodo encontrado
        }
      }
    }
  }

  // Si llegamos aquí, no se encontró el nodo con la dirección IP dada
  return nullptr;
}

// Envío de mensaje de Notificador -> Central
void EnviarMensajeNotificador(Ptr < Socket > socket, Ipv4Address dstAddr, uint16_t port) {
  // Crear un paquete y añadirle datos si es necesario
  Ptr < Packet > paquete = Create < Packet > (1000);

  // Enviar el paquete al nodo central
  int bytes_enviados = socket -> SendTo(paquete, 0, InetSocketAddress(dstAddr, port));

  if (bytes_enviados > 0) {
    // NS_LOG_INFO("Se enviaron satisfactoriamente " << bytes_enviados << " bytes desde el notificador.");
    // NS_LOG_INFO("Enviado a central: " << dstAddr);

    Ptr < Node > node = socket -> GetNode();
    Ptr < Ipv4 > ipv4 = node -> GetObject < Ipv4 > ();
    Ipv4InterfaceAddress iaddr = ipv4 -> GetAddress(1, 0);
    Ipv4Address ipAddr = iaddr.GetLocal();

    WriteCSVFile(Simulator::Now().GetSeconds(), "request", ipAddr, dstAddr,
      bytes_enviados);
  } else {
    NS_LOG_INFO("Error al enviar el mensaje desde el notificador. Código de error: " << socket -> GetErrno());
  }
  numeroIntentosComunicacion++;
}

// Recepción de mensaje Notificador <- Central
void RecibirEnNotificadores(Ptr < Socket > socket) {
  Ptr < Packet > packet;
  Address from;
  while ((packet = socket -> RecvFrom(from))) {
    if (packet -> GetSize() > 0) {
      MyHeader NotificadorIpHeader;
      packet -> RemoveHeader(NotificadorIpHeader);
      // Obtener la dirección IP de destino del header
      std::string notificadorIp = NotificadorIpHeader.GetData();

      MyHeader RescatistaIpHeader;
      packet -> RemoveHeader(RescatistaIpHeader);
      // Obtener la dirección IP de destino del header
      std::string rescatistaIp = RescatistaIpHeader.GetData();

      // Obtener los bytes enviados para el CSV
      uint32_t bytes_sent = packet -> GetSize();

      NS_LOG_INFO("Notificador con ip: " << notificadorIp << " recibe mensaje de rescatista con ip: " << rescatistaIp);
      WriteCSVFile(Simulator::Now().GetSeconds(), "reply",
        Ipv4Address(rescatistaIp.c_str()),
        Ipv4Address(notificadorIp.c_str()),
        static_cast < int > (bytes_sent));
      comunicacionesEfectivas++;
      // NS_LOG_INFO("--------------------------------------------------------------------------------------------");

    }

  }

}

// Recepción de mensaje Rescatista <- Central, y reenvío desde Rescatista -> Central
void RecibirEnRescatista(Ptr < Socket > socket) {
  Ptr < Packet > packet;
  Address from;
  while ((packet = socket -> RecvFrom(from))) {
    if (packet -> GetSize() > 0) {
      MyHeader RescatistaIpHeader;
      packet -> RemoveHeader(RescatistaIpHeader);
      // Obtener la dirección IP de destino del header
      std::string rescatistaIp = RescatistaIpHeader.GetData();
      // NS_LOG_INFO("Dirección ip del rescatista: " << rescatistaIp);

      MyHeader NotificadorIpHeader;
      packet -> RemoveHeader(NotificadorIpHeader);
      // Obtener la dirección IP de destino del header
      std::string notificadorIp = NotificadorIpHeader.GetData();
      // NS_LOG_INFO("Dirección ip del notificador: " << notificadorIp);

      // Convertir la dirección a InetSocketAddress
      // InetSocketAddress address = InetSocketAddress::ConvertFrom(from);
      // Ipv4Address senderIp = address.GetIpv4();

      // El rescatista ha recibido un paquete
      // NS_LOG_INFO("Rescatista de la central " << senderIp << " recibió un mensaje: " << packet->GetSize() << " bytes");

      Ptr < Node > node = centrales.Get(1);
      Ptr < Ipv4 > ipv4 = node -> GetObject < Ipv4 > (); // Obtener la instancia de IPv4 asociada al nodo
      Ipv4InterfaceAddress iaddr = ipv4 -> GetAddress(1, 0); // El índice 0 suele ser la dirección de bucle invertido
      Ipv4Address centralAddr = iaddr.GetLocal();

      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      Ptr < Node > rescatistaNodo = FindNodeWithIpAddressInInterfaces(rescatistaIp, allInterfaces);
      Ptr < Socket > source = Socket::CreateSocket(rescatistaNodo -> GetObject < Node > (), tid);

      InetSocketAddress remote = InetSocketAddress(centralAddr, 80);
      source -> Connect(remote);

      // Reenviar el paquete al notificador, pasando por central
      MyHeader ipHeaderNotificador;
      ipHeaderNotificador.SetData(notificadorIp);
      packet -> AddHeader(ipHeaderNotificador);

      // Reenviar el paquete al central
      source -> Send(packet);
      // NS_LOG_INFO("Rescatista: " << rescatistaIp << " recibió de central: " << senderIp << " y envia a central " << centralAddr);

      source -> Close();

    }

  }

}

// Recepción de mensaje Central <- Rescatista, y reenvío desde Central -> Notificador
void RecibirEnCentralDesdeRescatistas(Ptr < Socket > socket) {
  Ptr < Packet > packet;
  Address from;
  while ((packet = socket -> RecvFrom(from))) {
    if (packet -> GetSize() > 0) {

      MyHeader NotificadorIpHeader;
      packet -> RemoveHeader(NotificadorIpHeader);
      // Obtener la dirección IP de destino del header
      std::string notificadorIp = NotificadorIpHeader.GetData();

      // Convertir la dirección a InetSocketAddress
      InetSocketAddress address = InetSocketAddress::ConvertFrom(from);
      Ipv4Address senderIp = address.GetIpv4();;
      // NS_LOG_INFO("Rescatista " << senderIp << " a notificador " << notificadorIp);

      // Crear un socket para enviar datos
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      Ptr < Socket > source = Socket::CreateSocket(centrales.Get(1) -> GetObject < Node > (), tid);

      Ptr < Node > notificadorNodo = FindNodeWithIpAddressInInterfaces(notificadorIp, allInterfaces);
      Ipv4Address notificadorAddr = notificadorNodo -> GetObject < Ipv4 > () -> GetAddress(1, 0).GetLocal();

      InetSocketAddress remote = InetSocketAddress(notificadorAddr, 80);
      source -> Connect(remote);

      MyHeader ipHeaderRescatista;
      std::stringstream ss;
      senderIp.Print(ss);
      ipHeaderRescatista.SetData(ss.str());
      packet -> AddHeader(ipHeaderRescatista);
      MyHeader ipHeaderNotificador;
      ipHeaderNotificador.SetData(notificadorIp);
      packet -> AddHeader(ipHeaderNotificador);

      source -> Send(packet);
      // NS_LOG_INFO("Central envia a notificador: " << notificadorAddr);

      source -> Close();
    }
  }
}

// Recepción de mensaje Central <- Notificador, y reenvío desde Central -> Rescatista
void RecibirEnCentralDesdeNotificadores(Ptr < Socket > socket) {
  Ptr < Packet > packet;
  Address from;
  while ((packet = socket -> RecvFrom(from))) {
    if (packet -> GetSize() > 0) {
      // Aquí el central ha recibido un paquete y ahora va a enviarlo a un rescatista
      InetSocketAddress address = InetSocketAddress::ConvertFrom(from);
      Ipv4Address senderIp = address.GetIpv4();
      // NS_LOG_INFO("Central recibió un mensaje del rescatista: " << senderIp);
      // Seleccionar un rescatista aleatorio
      int numRescatistas = rescatistas.GetN();
      int rescatistaAleatorioIndex = rand() % numRescatistas; // selecciona un índice aleatorio
      Ptr < Node > rescatistaAleatorio = rescatistas.Get(rescatistaAleatorioIndex);

      // Obtener la dirección IP del rescatista
      Ipv4Address rescatistaAddr = allInterfaces.GetAddress(numRescatistas + rescatistaAleatorioIndex);

      // Crear un socket para enviar datos
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      Ptr < Socket > source = Socket::CreateSocket(centrales.Get(0) -> GetObject < Node > (), tid);

      InetSocketAddress remote = InetSocketAddress(rescatistaAddr, 80);
      source -> Connect(remote);

      // Reenviar el paquete al rescatista
      MyHeader ipHeaderRescatista;
      MyHeader ipHeaderNotificador;

      std::stringstream ss;
      rescatistaAddr.Print(ss);
      ipHeaderRescatista.SetData(ss.str());

      std::stringstream ss2;
      senderIp.Print(ss2);
      ipHeaderNotificador.SetData(ss2.str());

      packet -> AddHeader(ipHeaderNotificador);
      packet -> AddHeader(ipHeaderRescatista);
      // NS_LOG_INFO("Central envia a rescatista: " << ss.str());
      source -> Send(packet);

      source -> Close();
    }
  }
}

int main(int argc, char * argv[]) {
  // Activar NS_LOG para el componente deseado con nivel INFO
  LogComponentEnable("AdHocRescueSimulation", LOG_LEVEL_INFO);
  NS_LOG_INFO("Iniciando simulación");

  // Se establece una semilla aleatoria basada en el tiempo actual
  // Esto hará que los números generados sean diferentes en cada ejecución
  ns3::RngSeedManager::SetSeed(std::chrono::system_clock::now().time_since_epoch().count());

  std::string errorModelType;
  errorModelType = "ns3::YansErrorRateModel";

  // Parsear argumentos de línea de comandos si los hay
  CommandLine cmd(__FILE__);
  //cmd.AddValue ("simulationTime", "Duracion de la simulacion", simulationTime);
  cmd.AddValue("numNotificadores", "No. de notificadores", numNotificadores);
  cmd.AddValue("numRescatistas", "No. de rescatistas", numRescatistas);
  cmd.AddValue("numCentrales", "No. de nodos centrales", numCentrales);
  cmd.AddValue("routingProtocol", "Tipo de protocolo de enrutamiento", routingProtocol);
  cmd.AddValue("CSVfileName", "Nombre del archivo CSV", CSVfileName);
  cmd.Parse(argc, argv);

  // Escribir columnas en el archivo de salida .csv
  std::ofstream out(CSVfileName.c_str());
  out << "Time," <<
    "Type," <<
    "Source," <<
    "Destination," <<
    "Bytes_sent" <<
    std::endl;
  out.close();

  // Crear los contenedores de nodos
  notificadores.Create(numNotificadores);
  rescatistas.Create(numRescatistas);
  centrales.Create(numCentrales);

  // Agregar todos los nodos a un contenedor
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

  // Configurar el protocolo de enrutamiento
  if (routingProtocol == "AODV") {
    AodvHelper aodv;
    stack.SetRoutingHelper(aodv);
  } else if (routingProtocol == "OLSR") {
    OlsrHelper olsr;
    stack.SetRoutingHelper(olsr);
  } else if (routingProtocol == "DSDV") {
    DsdvHelper dsdv;
    stack.SetRoutingHelper(dsdv);
  }

  // stack.Install (notificadores);
  // stack.Install (rescatistas);
  // stack.Install (centrales);
  stack.Install(allNodes);

  if (routingProtocol == "DSR") {
    dsrMain.Install(dsr, allNodes);
  }

  // Configuración del canal de comunicación
  YansWifiChannelHelper wifiChannel; // = YansWifiChannelHelper::Default ();
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetChannel(wifiChannel.Create());
  wifiPhy.SetErrorRateModel(errorModelType);

  /*wifiPhy.Set ("TxPowerStart", DoubleValue (7.5));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (7.5));
  wifiPhy.Set ("Antennas", UintegerValue (2));
  wifiPhy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (2));
  wifiPhy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (2));*/

  WifiHelper wifi;
  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  wifi.SetStandard(WIFI_STANDARD_80211g);

  /*wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ);

  StringValue dataRate = StringValue ("HtMcs10");
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", dataRate, "ControlMode", dataRate);*/

  // Instalar dispositivos wifi en los nodos
  NetDeviceContainer notificadorDevices, rescatistaDevices, centralDevices;

  notificadorDevices = wifi.Install(wifiPhy, wifiMac, notificadores);
  rescatistaDevices = wifi.Install(wifiPhy, wifiMac, rescatistas);
  centralDevices = wifi.Install(wifiPhy, wifiMac, centrales);

  // Agregar todos los dispositivos a un contenedor
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

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel"); //,
  //"Bounds",
  //RectangleValue (Rectangle (-100, 100, -100, 100)));
  mobility.Install(centrales);

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
    "MinX", DoubleValue(0.0),
    "MinY", DoubleValue(0.0),
    "DeltaX", DoubleValue(10.0),
    "DeltaY", DoubleValue(20.0),
    "GridWidth", UintegerValue(3),
    "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
    "Bounds",
    RectangleValue(Rectangle(-100, 100, -100, 100)));
  mobility.Install(notificadores);
  mobility.Install(rescatistas);

  // Crear un tipo de socket y configurarlo
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

  // Configurar socket en nodo central para recibir mensajes
  for (int i = 0; i < numCentrales; i++) {
    Ptr < Node > node = centrales.Get(i);
    Ptr < Socket > recvSocket = Socket::CreateSocket(node, tid);
    Ptr < Ipv4 > ipv4 = node -> GetObject < Ipv4 > (); // Obtener la instancia de IPv4 asociada al nodo
    Ipv4InterfaceAddress iaddr = ipv4 -> GetAddress(1, 0);
    InetSocketAddress local = InetSocketAddress(iaddr.GetLocal(), 80);
    recvSocket -> Bind(local);
    if (i == 0) {
      recvSocket -> SetRecvCallback(MakeCallback( & RecibirEnCentralDesdeNotificadores));
    } else {
      recvSocket -> SetRecvCallback(MakeCallback( & RecibirEnCentralDesdeRescatistas));
    }
  }

  // Configurar socket en nodos rescatistas para recibir mensajes
  for (int i = 0; i < numRescatistas; i++) {
    Ptr < Node > node = rescatistas.Get(i);
    Ptr < Socket > recvSocket = Socket::CreateSocket(node, tid);
    Ptr < Ipv4 > ipv4 = node -> GetObject < Ipv4 > (); // Obtener la instancia de IPv4 asociada al nodo
    Ipv4InterfaceAddress iaddr = ipv4 -> GetAddress(1, 0);
    InetSocketAddress local = InetSocketAddress(iaddr.GetLocal(), 80);
    recvSocket -> Bind(local);
    recvSocket -> SetRecvCallback(MakeCallback( & RecibirEnRescatista));
  }

  // Configurar socket en nodos notificadores para recibir mensajes
  for (int i = 0; i < numNotificadores; i++) {
    Ptr < Node > node = notificadores.Get(i);
    Ptr < Socket > recvSocket = Socket::CreateSocket(node, tid);
    Ptr < Ipv4 > ipv4 = node -> GetObject < Ipv4 > (); // Obtener la instancia de IPv4 asociada al nodo
    Ipv4InterfaceAddress iaddr = ipv4 -> GetAddress(1, 0);
    InetSocketAddress local = InetSocketAddress(iaddr.GetLocal(), 80);
    recvSocket -> Bind(local);
    recvSocket -> SetRecvCallback(MakeCallback( & RecibirEnNotificadores));
  }

  // Crear una variable aleatoria exponencial para el tiempo de envío de mensajes
  Ptr < ExponentialRandomVariable > x = CreateObject < ExponentialRandomVariable > ();
  x -> SetAttribute("Mean", DoubleValue(5.0)); // La media es 5.0

  int eventos = 100;
  // Configurar sockets en nodos notificadores para enviar mensajes
  for (int i = 0; i < eventos; i++) {
    Ptr < Socket > sendSocket = Socket::CreateSocket(notificadores.Get(i % numNotificadores), tid);
    Ptr < Node > node = centrales.Get(0);
    Ptr < Ipv4 > ipv4 = node -> GetObject < Ipv4 > ();
    Ipv4InterfaceAddress iaddr = ipv4 -> GetAddress(1, 0);
    // Programar el envío de mensajes para tiempo aleatorio
    // Genera un valor aleatorio.
    double value = x -> GetValue();
    // NS_LOG_INFO("Mensaje eviado desde notificador: " << notificadores.Get(i % numNotificadores)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() <<
    // " En el segundo: " << value);
    Simulator::Schedule(Seconds(value), & EnviarMensajeNotificador, sendSocket, iaddr.GetLocal(), 80); // enviar a la primera dirección central
  }

  Simulator::Schedule(Seconds(simulationTime), & FinalPrint);

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
  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  // TODO: Procesar los resultados de la simulación para obtener métricas

  Simulator::Destroy();
  return 0;
}
