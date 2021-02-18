/*
Biblioteka powstała na podstawie:
The library was founded on the basis of: 
The ESP-CoAP is maintained by thingTronics Innovations.
Main contributor:

    Poornima Nagesh @poornima.nagesh@thingtronics.com
    Lovelesh Patel @lovelesh.patel@thingtronics.com

https://github.com/automote/ESP-CoAP
*/

#ifndef _coap_server_H_
#define _coap_server_H_

#include <Arduino.h>
#include <WiFiUdp.h>

//Current coap attributes
//Atrybuty CoAP'a
#define COAP_DEFAULT_PORT 5683
#define COAP_VERSION 1
#define COAP_HEADER_SIZE 4
#define COAP_OPTION_HEADER_SIZE 1
#define COAP_PAYLOAD_MARKER 0xFF

//Configuration
//Konfiguracja CoAP'a
#define MAX_OPTION_NUM 6
#define BUF_MAX_SIZE 128
#define MAX_CALLBACK 7
#define MAX_AGE_DEFAULT 60
#define MAX_OBSERVER 3

//Wyliczanie numer opcji
//Computing option number
#define COAP_OPTION_DELTA(v, n) (v < 13 ? (*n = (0xFF & v)) : (v <= 0xFF + 13 ? (*n = 13) : (*n = 14)))

//Coap message types
//CoAP: typy wiadomosci
typedef enum
{
	COAP_CON = 0,
	COAP_NONCON = 1,
	COAP_ACK = 2,
	COAP_RESET = 3,
} COAP_TYPE;

//Coap method values
//CoAP metody
typedef enum
{
	COAP_EMPTY = 0,
	COAP_GET = 1,
	COAP_POST = 2,
	COAP_PUT = 3,
	COAP_DELETE = 4,
} COAP_METHOD;

//Coap response values
//CoAP kody odpowiedzi
typedef enum
{
	COAP_EMPTY_MESSAGE = 0,
	COAP_CREATED = 65,
	COAP_DELETED = 66,
	COAP_VALID = 67,
	COAP_CHANGED = 68,
	COAP_CONTENT = 69,
	COAP_BAD_REQUEST = 128,
	COAP_UNAUTHORIZED = 129,
	COAP_BAD_OPTION = 130,
	COAP_FORBIDDEN = 131,
	COAP_NOT_FOUND = 132,
	COAP_METHOD_NOT_ALLOWD = 133,
	COAP_PRECONDITION_FAILED = 140,
	COAP_REQUEST_ENTITY_TOO_LARGE = 141,
	COAP_UNSUPPORTED_CONTENT_FORMAT = 143,
	COAP_INTERNAL_SERVER_ERROR = 160,
	COAP_NOT_IMPLEMENTED = 161,
	COAP_BAD_GATEWAY = 162,
	COAP_SERVICE_UNAVALIABLE = 163,
	COAP_GATEWAY_TIMEOUT = 164,
	COAP_PROXYING_NOT_SUPPORTED = 165
} COAP_RESPONSE_CODE;

//Coap option values
//CoAP kody opcji
typedef enum
{
	COAP_IF_MATCH = 1,
	COAP_URI_HOST = 3,
	COAP_E_TAG = 4,
	COAP_IF_NONE_MATCH = 5,
	COAP_URI_PORT = 7,
	COAP_LOCATION_PATH = 8,
	COAP_URI_PATH = 11,
	COAP_CONTENT_FORMAT = 12,
	COAP_MAX_AGE = 14,
	COAP_URI_QUERY = 15,
	COAP_ACCEPT = 17,
	COAP_LOCATION_QUERY = 20,
	COAP_BLOCK_2 = 23,
	COAP_PROXY_URI = 35,
	COAP_PROXY_SCHEME = 39,
	COAP_OBSERVE = 6
} COAP_OPTION_NUMBER;

//Coap content format types
//CoAP: kody formatow
typedef enum
{
	COAP_TEXT_PLAIN = 0,
	COAP_APPLICATION_LINK_FORMAT = 40,
	COAP_APPLICATION_XML = 41,
	COAP_APPLICATION_OCTET_STREAM = 42,
	COAP_APPLICATION_EXI = 47,
	COAP_APPLICATION_JSON = 50,
	COAP_APPLICATION_CBOR = 60
} COAP_CONTENT_TYPE;

//Coap class::used for resource discovery request
//Klasa uzywana do Discover
class resource_dis
{
public:
	String rt;
	uint8_t ct;
	//String title;
};

//Coap option class
//Klasa wykorzystywana do przechowywania opcji
class coapOption
{
public:
	uint8_t number;
	uint8_t length;
	uint8_t *buffer;
};

//Coap packet class
//Klasa definiuje pakiet CoAP
class coapPacket
{
public:
	uint8_t version;
	uint8_t type;
	uint8_t code;
	uint8_t *token;
	uint8_t tokenlen;
	uint8_t *payload;
	uint8_t payloadlen;
	uint16_t messageid;
	uint8_t optionnum;
	coapOption options[MAX_OPTION_NUM];
	//Metoda parsuje otrzymany pakiet na obiekt klasy coapPacket
	bool bufferToPacket(uint8_t buffer[], int32_t packetlen);
	//Metoda parsuje otrzymane opcje na obiekty klasy coapOption
	int parseOption(coapOption *option, uint16_t *running_delta, uint8_t **buf, size_t buflen);
	coapPacket();

	uint8_t version_();
	uint8_t type_();
	uint8_t tokenlen_();
	uint8_t code_();
	uint16_t messageid_();
	uint8_t *token_();
	uint8_t accept();
};

//Callback, pozwala na wywołanie odpowiednich funkcji przez program (automatycznie)
typedef void (*callback)(coapPacket *, IPAddress, int, int, uint8_t);

//Class defines URI
//Klasa definiuje obiekt coapUri
class coapUri
{
public:
	String u[MAX_CALLBACK];
	callback c[MAX_CALLBACK];

	coapUri();
	//Dodanie nowego uti = dodanie nowego zasobu
	void add(callback call, String url, resource_dis resource[]);
	callback find(String url);
};

//Coap class::used for maintaining the details of clients making observe request
//Klasa coapObserver odpowiada za przechowywanie i działanie Observatora
class coapObserver
{
public:
	uint8_t *observer_token;
	uint8_t observer_tokenlen;
	IPAddress observer_clientip;
	int observer_clientport;
	String observer_url;
	unsigned long observer_maxAge;
	unsigned long observer_prevMillis;
	uint8_t *observer_etag;
	uint8_t observer_etagLen;
	uint8_t *observer_storedResponse;
	uint8_t observer_storedResponseLen;
	uint8_t observer_repeatedPayload;
	//Usuwa obserwatora
	void deleteObserver();
};

//Coap server class
//Glowna klasa, steruje praca calosci
class coapServer
{

public:
	coapServer();
	//Metoda uruchamia serwer
	bool start();
	bool start(int port);
	//Metoda tworzy serwer
	void server(callback c, String url);
	//Glowna petla sewera z niej wywolywane sa odpowienie funkcje
	void loop();
	//Metoda wysylą pakiet
	bool sendPacket(coapPacket *packet, IPAddress ip, int port);
	//Metoda odkrywa dostepne zasoby
	void resourceDiscovery(coapPacket *packet, IPAddress ip, int port, resource_dis resource[]);
	//Metoda formatuje pakiet do wyslania
	void sendResponse(IPAddress ip, int port, char *payload, uint8_t payloadLen);
	void sendResponse(IPAddress ip, int port, COAP_CONTENT_TYPE contentType, char *payload, uint8_t payloadLen);
	void sendResponse(IPAddress ip, int port, COAP_CONTENT_TYPE contentType, char *payload, uint8_t payloadLen, int store);
	void sendResponse(IPAddress ip, int port, int erType, COAP_CONTENT_TYPE contentType, char *payload, uint8_t payloadLen);
	void sendResponse(IPAddress ip, int port, int erType, COAP_CONTENT_TYPE contentType, char *payload, uint8_t payloadLen, int store);
	//Metoda dodaje obserwatora
	void addObserver(String url, coapPacket *request, IPAddress ip, int port);
	//Metoda wysyla notyfikacje z nowym payloadem (Obserwator)
	void notification(char *payload, String url, uint8_t payloadLen);
	//void sendResponse(char *payload);
	//Metoda odpowiada Not Found gdy prosimy o nieistniejący zasob
	void sendError(coapPacket *packet, IPAddress ip, int port, COAP_RESPONSE_CODE error);
	//Metoda pomocnicza, porownuje dwie tablice
	bool compareArray(uint8_t a[], uint8_t b[], uint8_t lenA, uint8_t lenB);
	//Metoda pomocnicza, wylicza dlugosc tablicy
	uint8_t countLength(uint8_t messageid);
};

#endif