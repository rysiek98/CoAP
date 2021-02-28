/*
Biblioteka powstała na podstawie:
The library was founded on the basis of: 
The ESP-CoAP is maintained by thingTronics Innovations.
Main contributor:

    Poornima Nagesh @poornima.nagesh@thingtronics.com
    Lovelesh Patel @lovelesh.patel@thingtronics.com

https://github.com/automote/ESP-CoAP
*/

#include "coap_server.h"

WiFiUDP Udp;
coapUri uri;
//Resources table (contains all available resources)
//Tworzymy tablice obiektow klasy resource_dis, reprezentuja dostepne zasoby
resource_dis resource[MAX_CALLBACK];
coapPacket *request = new coapPacket();
//Observers table (contains all actual observers)
//Tworzymy tablice obiektow klasy coapObserver, bedą w niej przechowywani aktualni obserwatorowie
coapObserver observer[MAX_OBSERVER];
coapObserver *actualObserver = nullptr;
//Added resources counter
//Licznik dodanych zasobow
uint8_t resourceCounter = 0;
//Observers counter
//Licznik obserwatorow
uint8_t obsCounter = 0;
uint16_t obsState = 10;
uint8_t obsStateArray[2];

//Variables store response
//Zmienne globalne stored przechowują zapisaną odpowiedz.
uint8_t *storedETag = nullptr;
uint8_t storedETagLen = 0;
uint8_t *storedResponse = nullptr;
uint8_t storedResponseLen = 0;
IPAddress storedIP;

coapUri::coapUri()
{
    for (int i = 0; i < MAX_CALLBACK; i++)
    {
        u[i] = "";
        c[i] = NULL;
    }
}

void coapUri::add(callback call, String url, resource_dis resource[])
{

    for (int i = 0; i < MAX_CALLBACK; i++)
        if (c[i] != NULL && u[i].equals(url)) //aktualizacja zasobu
        {
            c[i] = call;          //Dodanie "wywolania" do tablicy callback
            resourceCounter++;    //Zwiekszenie licznika zasobow
            resource[i].rt = url; //Dodanie adresu URL do tablicy obiektow klasy resource_dis (obsluga fun. Discovery)
            resource[i].ct = 0;   //Ustawienie parametru control flag obiektu klasy resource_dis
                                  /*   if (i == 0)
                //resource[i].title="observable resource";
                return;*/
        }

    for (int i = 0; i < MAX_CALLBACK; i++)
        if (c[i] == NULL) //Nowy zasob.
        {
            c[i] = call;
            u[i] = url;
            resourceCounter++;
            resource[i].rt = url;
            resource[i].ct = 0;
            return;
        }
}
//Wyszukiwanie zadanego uri.
callback coapUri::find(String url)
{
    for (int i = 0; i < MAX_CALLBACK; i++)
        if (c[i] != NULL && u[i].equals(url))
            return c[i];
    return NULL;
}
//Funkcja dodaje nowy zasob.
void coapServer::server(callback c, String url)
{
    uri.add(c, url, resource);
}
//Konstruktor klasy Coap.
coapServer::coapServer()
{
}

//Rozpoczynamy dzialanie serwera.
bool coapServer::start()
{
    this->start(COAP_DEFAULT_PORT);
    return true;
}

bool coapServer::start(int port)
{
    Udp.begin(port);
    return true;
}
//Konstruktor klasy CoapPacket.
coapPacket::coapPacket()
{
}

uint8_t coapPacket::version_()
{
    return version;
}

uint8_t coapPacket::type_()
{
    return type;
}

uint8_t coapPacket::code_()
{
    return code;
}

uint16_t coapPacket::messageid_()
{
    return messageid;
}

uint8_t *coapPacket::token_()
{
    return token;
}
//Parsowanie dodatkowej opcji.
int coapPacket::parseOption(coapOption *option, uint16_t *running_delta, uint8_t **buf, size_t buflen)
{

    uint8_t *p = *buf;
    uint8_t headlen = 1;
    uint16_t len = 0;
    uint16_t delta = 0;

    if (buflen < headlen)
        return -1;

    //Początkowa wartosc delty.
    delta = (p[0] & 0xF0) >> 4;
    len = p[0] & 0x0F;

    if (delta == 13)
    {
        headlen++;
        if (buflen < headlen)
            return -1;
        delta = p[1] + 13;
        p++;
    }
    else if (delta == 14)
    {
        headlen += 2;
        if (buflen < headlen)
            return -1;
        delta = ((p[1] << 8) | p[2]) + 269;
        p += 2;
    }
    else if (delta == 15)
        return -1;

    if (len == 13)
    {
        headlen++;
        if (buflen < headlen)
            return -1;
        len = p[1] + 13;
        p++;
    }
    else if (len == 14)
    {
        headlen += 2;
        if (buflen < headlen)
            return -1;
        len = ((p[1] << 8) | p[2]) + 269;
        p += 2;
    }
    else if (len == 15)
        return -1;

    if ((p + 1 + len) > (*buf + buflen))
        return -1;
    option->number = delta + *running_delta;
    option->buffer = p + 1;
    option->length = len;
    *buf = p + 1 + len;
    *running_delta += delta;

    return 0;
}

//Glowna pętla programu.
void coapServer::loop()
{
    //Rezerwujemy pamiec na odebrany pakiet.
    uint8_t *buffer = new uint8_t[BUF_MAX_SIZE];
    for (int i = 0; i < BUF_MAX_SIZE; i++)
    {
        buffer[i] = 0;
    }
    int32_t packetlen = Udp.parsePacket();

    //Warunek decydujący o rozpoczęciu przetwarzania odebranego pakietu.
    if (packetlen > 0)
    {

        packetlen = Udp.read(buffer, packetlen >= BUF_MAX_SIZE ? BUF_MAX_SIZE : packetlen);

        //Odebrany pakiet parsujemy na obiekt klasy coapPacket.
        //Recived packet is parsed to coapPacket object.
        if (request->bufferToPacket(buffer, packetlen))
        {
            //Sprawdzamy czy pakiet ma URI_PATH i odczytujemy jego zawartość.
            String url = "";
            for (int i = 0; i < request->optionnum; i++)
            {
                if (request->options[i].number == COAP_URI_PATH && request->options[i].length > 0)
                {
                    char urlname[request->options[i].length + 1];
                    memcpy(urlname, request->options[i].buffer, request->options[i].length);
                    urlname[request->options[i].length] = '\0';
                    if (url.length() > 0)
                        url += "/";
                    url += urlname;
                }
            }
            //Jesli odebrane URI nie jest w liscie zasobow, nie jest to .well-known/core a pakiet nie jest pusty to  wysli Not Found.
            if (!uri.find(url) && !(url == String(".well-known/core")) && !(request->code_() == COAP_EMPTY))
            {
                sendError(request, Udp.remoteIP(), Udp.remotePort(), COAP_NOT_FOUND);
            }
            else
            { //Gdy dostaniemy pusty pakiet.
                if (request->code_() == COAP_EMPTY && (request->type_() == COAP_CON || request->type_() == COAP_RESET || request->type_() == COAP_NONCON))
                {
                    if (request->type_() == COAP_CON)
                    {
                        request->type = COAP_ACK;
                    }
                    else
                    {
                        request->type = COAP_RESET;
                    }
                    request->code = COAP_EMPTY_MESSAGE;
                    request->payload = nullptr;
                    request->payloadlen = 0;
                    request->optionnum = 0;

                    if (request->type_() == COAP_RESET)
                    { //Gdy otrzymamy pusty pakiet, z codem Reset to czyscimy liste obserwatorow z tego IP.
                        //Gdy dostaniemy puste zadanie typu COAP_RESET, usuwamy uzytkownika z listy obserwatorow.
                        for (uint8_t i = 0; i < obsCounter; i++)
                        {
                            if (observer[i].clientip == Udp.remoteIP())
                            {
                                observer[i].deleteObserver();
                                if (i < (MAX_OBSERVER - 1))
                                {
                                    observer[i] = observer[i + 1];
                                    observer[i + 1] = {0};
                                }
                                else
                                {
                                    observer[i] = {0};
                                }
                                obsCounter--;
                            }
                        }
                    }
                    sendPacket(request, Udp.remoteIP(), Udp.remotePort());
                }
                else if (request->code_() == COAP_GET || request->code_() == COAP_PUT ||
                         request->code_() == COAP_POST || request->code_() == COAP_DELETE)
                {
                    //Generacja odpowiedzi na zadanie: GET, PUT, POST i DELETE
                    //Dla zadania GET sprawdzamy czy jest ustawiona opcja Obserwe.
                    if (request->code_() == COAP_GET)
                    {
                        uint8_t num = 0;
                        for (uint8_t i = 0; i <= request->optionnum; i++)
                        {
                            if (request->options[i].number == COAP_OBSERVE)
                            {
                                num = i;
                                break;
                            }
                        }
                        //Usuniecie uzytkownika z listy obserwatorow.
                        if (request->options[num].number == COAP_OBSERVE)
                        {
                            if (*(request->options[num].buffer) == 1)
                            {
                                for (uint8_t i = 0; i < obsCounter; i++)
                                {
                                    if (observer[i].clientip == Udp.remoteIP() &&
                                        observer[i].url == url)
                                    {
                                        observer[i].deleteObserver();
                                        if (i < (MAX_OBSERVER - 1))
                                        {
                                            observer[i] = observer[i + 1];
                                            observer[i + 1] = {0};
                                        }
                                        else
                                        {
                                            observer[i] = {0};
                                        }
                                        obsCounter--;
                                    }
                                }
                                //Wyszukiwanie zadanego zasobu i wywolanie callback zasobu.
                                uri.find(url)(request, Udp.remoteIP(), Udp.remotePort(), 0, request->accept());
                            }
                            else
                            {
                                //Dodanie uzytkownika do listy obserwatorow.
                                if (url != String(".well-known/core") && obsCounter < MAX_OBSERVER)
                                {
                                    addObserver(url, request, Udp.remoteIP(), Udp.remotePort());
                                }
                                else
                                {
                                    sendError(request, Udp.remoteIP(), Udp.remotePort(), COAP_METHOD_NOT_ALLOWD);
                                }
                            }
                        }
                        else if (url == String(".well-known/core"))
                        {
                            //Gdy wyszukiwane URI to .well-known/core wykonuje funkcje Discovery.
                            resourceDiscovery(request, Udp.remoteIP(), Udp.remotePort(), resource);
                        }
                        else
                        {
                            //Gdy znajdziesz zadane uri wywolaj callback zasobu.
                            uri.find(url)(request, Udp.remoteIP(), Udp.remotePort(), 0, request->accept());
                        }
                    }
                    else if (request->code_() != COAP_GET && url == String(".well-known/core"))
                    {
                        sendError(request, Udp.remoteIP(), Udp.remotePort(), COAP_METHOD_NOT_ALLOWD);
                    }
                    else if (request->code_() == COAP_PUT)
                    {
                        uri.find(url)(request, Udp.remoteIP(), Udp.remotePort(), 0, request->accept());
                    }
                    else if (request->code_() == COAP_POST)
                    {
                        uri.find(url)(request, Udp.remoteIP(), Udp.remotePort(), 0, request->accept());
                    }
                    else if (request->code_() == COAP_DELETE)
                    {
                        uri.find(url)(request, Udp.remoteIP(), Udp.remotePort(), 0, request->accept());
                    }
                }
            }
        }
        else
        {
            sendError(request, Udp.remoteIP(), Udp.remotePort(), COAP_BAD_OPTION);
        }
    }

    //Checking for the change for resource
    //Obsluga obserwatora.  Funkcjonalnosc znacznie zmodyfikowana.
    unsigned long currentMillis = millis();
    for (int i = 0; i < obsCounter; i++)
    { //Sprawdzamy czy czas jaki czas uplynal od ostatniego wyslanego pakietu do obserwatora (czy jeszcze jest swiezy czy już nalezy go odswiezyc).
        if (currentMillis - observer[i].prevMillis >= (unsigned long)observer[i].maxAge)
        {
            //Jesli wymagane jest odswiezenie waznosci odpowiedzi to ustaw wartosci pakietu do wyslania i wywolaj callback zasobu.
            request->version = COAP_VERSION;
            request->code = COAP_GET;
            request->type = COAP_NONCON;
            request->optionnum = 0;
            //Zmienna pomocnicza, pomaga programowi w obsłudze Obserwatora
            //An auxiliary variable, helps to correctly use Observer service
            uint8_t tmpObsState = 10;
            request->options[0].buffer = &tmpObsState;
            request->options[0].length = 1;
            request->options[0].number = COAP_OBSERVE;
            request->optionnum = 1;
            //Ustawia wskaznik na aktualnie aktualizowanego obserwatora.
            actualObserver = &observer[i];
            uri.find(observer[i].url)(request, observer[i].clientip,
                                      observer[i].clientport, 1, request->accept());
            currentMillis = (unsigned long)millis();
            //Aktualizuje czas ostatniej aktualizacji zasobu.
            observer[i].prevMillis = currentMillis;
            actualObserver = nullptr;
        }
    }

    //Resetowanie atrybutow obiektu.
    delete[] request->token;
    request->token = nullptr;
    request->tokenlen = 0;
    request->optionnum = 0;
    request->payloadlen = 0;
    request->payload = nullptr;
    request->type = 0;
    request->code = 0;
    request->version = COAP_VERSION;
    if (request->messageid < 0 && request->messageid > 32000)
    {
        request->messageid = 10;
    }
    //Oproznianie zarezerwowanej pamieci.
    delete[] buffer;
    buffer = nullptr;
}

//Parsowanie zawartosci buffora na obiekt klasy coapPacket.
bool coapPacket::bufferToPacket(uint8_t buffer[], int32_t packetlen)
{
    //Parsownie naglowka
    //parse coap packet header
    version = (buffer[0] & 0xC0) >> 6;
    type = (buffer[0] & 0x30) >> 4;
    tokenlen = buffer[0] & 0x0F;
    code = buffer[1];
    messageid = 0xFF00 & (buffer[2] << 8);
    messageid |= 0x00FF & buffer[3];
    if (tokenlen == 0)
        token = nullptr;
    else if (tokenlen <= 8)
    {
        token = new uint8_t[tokenlen];
        memset(token, 0, tokenlen);

        for (int i = 0; i < tokenlen; i++)
        {
            token[i] = buffer[4 + i];
        }
    }
    else
    {
        tokenlen = 0;
        token = nullptr;
        return false;
    }
    //Parsowanie opcji dodatkowych i payload'u
    //parse packet options/payload
    if (COAP_HEADER_SIZE + tokenlen < packetlen)
    {
        int optionIndex = 0;
        uint16_t delta = 0;
        uint8_t *end = buffer + packetlen;
        uint8_t *p = buffer + COAP_HEADER_SIZE + tokenlen;

        while (optionIndex < MAX_OPTION_NUM && *p != 0xFF && p < end)
        {
            if (0 == parseOption(&options[optionIndex], &delta, &p, end - p))
                optionIndex++;
        }
        optionnum = optionIndex;
        payload = nullptr;

        if (p + 1 < end && *p == 0xFF)
        {
            payload = p + 1;
            payloadlen = end - (p + 1);
        }
        else
        {
            payload = nullptr;
            payloadlen = 0;
        }
    }
    return true;
}

//Metoda pozwalajaca wyslac pakiet.
bool coapServer::sendPacket(coapPacket *packet, IPAddress ip, int port)
{
    if (packet->type_() == COAP_RESET || packet->type_() == COAP_NONCON || packet->type_() == COAP_CON)
    {
        packet->messageid++;
    }
    request->messageid = packet->messageid;

    uint8_t buffer[BUF_MAX_SIZE];
    for (int i = 0; i < BUF_MAX_SIZE; i++)
    {
        buffer[i] = 0;
    }
    uint8_t *p = buffer;
    uint16_t running_delta = 0;
    uint16_t packetSize = 0;
    //Tworzenie naglowka
    *p = (1) << 6;
    *p |= (packet->type & 0x03) << 4;
    *p++ |= (packet->tokenlen & 0x0F);
    *p++ = packet->code;
    *p++ = (packet->messageid >> 8);
    *p++ = (packet->messageid & 0xFF);
    p = buffer + COAP_HEADER_SIZE;
    packetSize += 4;

    //Token
    if (packet->token != nullptr && packet->tokenlen <= 8)
    {
        memcpy(p, packet->token, packet->tokenlen);
        p += packet->tokenlen;
        packetSize += packet->tokenlen;
    }
    //Opcje dodatkowe
    for (int i = 0; i < packet->optionnum; i++)
    {
        uint32_t optdelta = 0;
        uint8_t len = 0;
        uint8_t delta = 0;

        if (packetSize + 5 + packet->options[i].length >= BUF_MAX_SIZE)
        {
            return false;
        }
        optdelta = packet->options[i].number - running_delta;
        COAP_OPTION_DELTA(optdelta, &delta);
        COAP_OPTION_DELTA((uint32_t)packet->options[i].length, &len);

        *p++ = (0xFF & (delta << 4 | len));
        if (delta == 13)
        {
            *p++ = (optdelta - 13);
            packetSize++;
        }
        else if (delta == 14)
        {
            *p++ = ((optdelta - 269) >> 8);
            *p++ = (0xFF & (optdelta - 269));
            packetSize += 2;
        }
        if (len == 13)
        {
            *p++ = (packet->options[i].length - 13);
            packetSize++;
        }
        else if (len == 14)
        {
            *p++ = (packet->options[i].length >> 8);
            *p++ = (0xFF & (packet->options[i].length - 269));
            packetSize += 2;
        }

        memcpy(p, packet->options[i].buffer, packet->options[i].length);
        p += packet->options[i].length;
        packetSize += packet->options[i].length + 1;
        running_delta = packet->options[i].number;
    }
    //Payload
    if (packet->payloadlen > 0)
    {
        if ((packetSize + 1 + packet->payloadlen) >= BUF_MAX_SIZE)
        {
            return false;
        }
        *p++ = 0xFF;
        memcpy(p, packet->payload, packet->payloadlen);
        packetSize += 1 + packet->payloadlen;
    }
    Udp.beginPacket(ip, port);
    Udp.write(buffer, packetSize);
    Udp.endPacket();
    return true;
}

//Funkcja Discovery.
void coapServer::resourceDiscovery(coapPacket *response, IPAddress ip, int port, resource_dis resource[])
{
    int length = 0;
    int j = 0;
    //Obliczamy długość payload'u
    for (int i = 0; i < resourceCounter; i++)
    {
        length += resource[i].rt.length();
        length += 5;
    }
    length--;
    char payload[length];
    //String str_res;
    for (int i = 0; i < resourceCounter; i++)
    {
        //        str_res += "</";
        //        str_res += resource[i].rt;
        //        str_res += ">;";
        //        str_res += resource[i].rt;
        //        str_res += ";rt=";
        //        str_res += "\"";
        //        str_res += "observe";
        //        str_res += "\"";
        //
        //        str_res += ";";
        //        str_res += "ct=";
        //        str_res += resource[i].ct;
        //        str_res += ";";
        //        if (i == 0)
        //        {
        //            str_res += "title=\"";
        //            str_res += "observable resource";
        //            str_res += "\"";
        //        }
        //        str_res += ",";
        payload[j++] = '<';
        payload[j++] = '/';
        for (unsigned int k = 0; k < resource[i].rt.length(); k++)
        {
            payload[j++] = resource[i].rt[k];
        }
        payload[j++] = '>';
        payload[j++] = ';';
        payload[j++] = ',';
    }
    //const char *payload = str_res.c_str();

    response->optionnum = 0;
    char optionBuffer[2] = {0};
    //Dodajemy opcje z formatowaniem payloadu'u w formacie COAP_APPLICATION_LINK_FORMAT
    optionBuffer[0] = ((uint16_t)COAP_APPLICATION_LINK_FORMAT & 0xFF00) >> 8;
    optionBuffer[1] = ((uint16_t)COAP_APPLICATION_LINK_FORMAT & 0x00FF);

    response->options[response->optionnum].buffer = (uint8_t *)optionBuffer;
    response->options[response->optionnum].length = 2;
    response->options[response->optionnum++].number = COAP_CONTENT_FORMAT;

    response->code = COAP_CONTENT;
    response->payload = (uint8_t *)&payload;
    response->payloadlen = length;

    sendPacket(response, Udp.remoteIP(), Udp.remotePort());
}

//Metoda wysyła ACK w przypadku gdy chcemy wysłać ACK i payload oddzielnie
void coapServer::sendSeparateResponse(IPAddress ip, int port)
{
    //Tworzymy obiekt w ktorym bedziemy przechowywac dane do wyslania.
    coapPacket *response = new coapPacket();

    response->version = COAP_VERSION;
    response->tokenlen = 0;
    response->token = nullptr;
    response->messageid = request->messageid;
    response->type = COAP_ACK;
    response->payloadlen = 0;
    response->payload = nullptr;
    response->optionnum = 0;
    response->code = COAP_EMPTY_MESSAGE;

    sendPacket(response, ip, port);
    //Usuwamy obiekt przechowujacy pakiet po wyslaniu pakietu.
    delete response;
    response = nullptr;
    request->type = COAP_ACK;
}

//Metoda sluzy do przygotowania pakietu z odpowiedzią do wyslania.
void coapServer::sendResponse(IPAddress ip, int port, int erType, COAP_CONTENT_TYPE contentType, char *payload, uint8_t payloadLen, int store)
{
    //Tworzymy obiekt w ktorym bedziemy przechowywac dane do wyslania.
    coapPacket *response = new coapPacket();

    response->version = COAP_VERSION;
    response->tokenlen = request->tokenlen;
    response->messageid = request->messageid;

    if (request->tokenlen > 0)
    {
        response->token = request->token;
    }

    if (request->type_() == COAP_CON)
    {
        response->type = COAP_ACK;
    }
    else if (request->type_() == COAP_NONCON)
    {
        response->type = COAP_NONCON;
    }
    else if (request->type_() == COAP_ACK)
    {
        response->type = COAP_CON;
    }

    uint8_t num = 0, num2 = 0;
    for (uint8_t i = 0; i <= request->optionnum; i++)
    {
        if (request->options[i].number == COAP_OBSERVE)
        {
            num = i;
        }

        if (request->options[i].number == COAP_E_TAG)
        {
            num2 = i;
        }
    }

    if (request->code_() == COAP_GET)
    {

        if (erType != -1)
        {
            response->code = erType;
        }
        else
        {
            response->code = COAP_CONTENT;
        }
        response->payloadlen = payloadLen;
        if (payloadLen > 0)
        {
            response->payload = (uint8_t *)payload;
        }
        else
        {
            response->payload = nullptr;
        }

        response->optionnum = 0;

        if (request->options[num2].number == COAP_E_TAG)
        {

            //Skoro klient w żądaniu GET zawarł ETag oznacza to że przechowują odpowiedź oznaczoną ETag'iem i można używać mechanizmu validacji
            //If client used ETag we know that client store response so we could use Validation mechanic
            for (int i = 0; i < obsCounter; i++)
            {
                if (*observer[i].etag == *request->options[num2].buffer)
                {
                    observer[i].eTagActivated = true;
                }
            }

            //Sprawdzamy czy ETag zawarty w żądaniu jest taki sam jak ETag przechowywanego żądania, używamy mechanizmu walidacji
            //If request includes ETag server compare ETag from request with stored Etag, if ETags are identical server use validation mechanic
            if (storedETag != nullptr && request->options[num].number != COAP_OBSERVE)
            {
                if (*request->options[num2].buffer == *storedETag)
                {
                    response->options[response->optionnum].buffer = storedETag;
                    response->options[response->optionnum].length = storedETagLen;
                    response->options[response->optionnum++].number = COAP_E_TAG;
                    response->code = COAP_VALID;
                    response->payloadlen = 0;
                    response->payload = nullptr;
                }
            }
        }

        //Obsluga ETag dla COAP_GET.
        if (store && (store && !(request->options[num].number == COAP_OBSERVE)))
        {
            if (storedETag == nullptr)
            {
                storedResponse = new uint8_t[payloadLen];
                memcpy(storedResponse, (uint8_t *)payload, payloadLen);
                storedResponseLen = payloadLen;
                uint8_t eTag = (uint8_t)response->messageid + (uint8_t)55;
                storedETagLen = countLength(eTag);
                storedETag = new uint8_t[storedETagLen];
                memcpy(storedETag, &eTag, storedETagLen);
                response->options[response->optionnum].buffer = storedETag;
                response->options[response->optionnum].length = storedETagLen;
                response->options[response->optionnum++].number = COAP_E_TAG;
                eTag = 0;
                storedIP = ip;
            }
        }
        //Obsluga obserwatora.
        if (request->options[num].number == COAP_OBSERVE && *request->options[num].buffer != 1)
        {
            response->token = actualObserver->token;
            response->tokenlen = actualObserver->tokenlen;
            //Obsluga ETag dla obserwatora.
            if (actualObserver != nullptr)
            {
                if (compareArray(actualObserver->storedResponse, (uint8_t *)payload, actualObserver->storedResponseLen, payloadLen) &&
                    actualObserver->eTagActivated)
                {
                    response->options[response->optionnum].buffer = actualObserver->etag;
                    response->options[response->optionnum].length = actualObserver->etagLen;
                    response->options[response->optionnum++].number = COAP_E_TAG;
                    response->code = COAP_VALID;
                    actualObserver->repeatedPayload = 0;
                    response->payloadlen = 0;
                    response->payload = nullptr;
                }
                else if (actualObserver->storedResponse == nullptr)
                {
                    actualObserver->storedResponse = new uint8_t[response->payloadlen];
                    memcpy(actualObserver->storedResponse, (uint8_t *)payload, response->payloadlen);
                    actualObserver->storedResponseLen = response->payloadlen;
                    uint8_t eTag = (uint8_t)response->messageid + obsState;
                    actualObserver->etagLen = countLength(eTag);
                    actualObserver->etag = new uint8_t[actualObserver->etagLen];
                    memcpy(actualObserver->etag, &eTag, actualObserver->etagLen);
                    response->options[response->optionnum].buffer = actualObserver->etag;
                    response->options[response->optionnum].length = actualObserver->etagLen;
                    response->options[response->optionnum++].number = COAP_E_TAG;
                    eTag = 0;
                }
                // else if (actualObserver->repeatedPayload >= 3)
                // {
                //     actualObserver->repeatedPayload = 0;
                //     delete[] actualObserver->storedResponse;
                //     delete[] actualObserver->etag;
                //     actualObserver->observer_storedResponse = nullptr;
                //     actualObserver->etag = nullptr;
                //     actualObserver->observer_storedResponse = new uint8_t[response->payloadlen];
                //     memcpy(actualObserver->observer_storedResponse, (uint8_t *)payload, response->payloadlen);
                //     actualObserver->storedResponseLen = response->payloadlen;
                //     uint8_t eTag = (uint8_t)response->messageid + obsState;
                //     actualObserver->etagLen = countLength(eTag);
                //     actualObserver->etag = new uint8_t[actualObserver->etagLen];
                //     memcpy(actualObserver->etag, &eTag, actualObserver->etagLen);
                //     response->options[response->optionnum].buffer = actualObserver->etag;
                //     response->options[response->optionnum].length = actualObserver->etagLen;
                //     response->options[response->optionnum++].number = COAP_E_TAG;
                //     eTag = 0;
                // }
                else
                {
                    actualObserver->repeatedPayload++;
                }
            }
            //Licznik stanu obsState jest typu uint16_t, stąd gdy dojdziemy do granicznej wartości musimy go zrestartować
            //obsState counter is uint16_t type, so if we reach frontier value we should rest counter to default value
            if (obsState >= 65532)
            {
                obsState = 10;
            }
            else
            {
                obsState++;
            }
            //Konwersja zmiennej typu uint16_t na tablice typu uint8_t
            //Conversion from uint16_t type to int8_t type array
            obsStateArray[0] = (obsState >> 8);
            obsStateArray[1] = obsState & 0xff;
            response->options[response->optionnum].buffer = (uint8_t *)obsStateArray;
            response->options[response->optionnum].length = 2;
            response->options[response->optionnum++].number = COAP_OBSERVE;
        }

        //Obsługa Content Format. Dodana funkcjonalność.
        char optionBuffer[2] = {0};
        optionBuffer[0] = ((uint16_t)contentType & 0xFF00) >> 8;
        optionBuffer[1] = ((uint16_t)contentType & 0x00FF);
        response->options[response->optionnum].buffer = (uint8_t *)optionBuffer;
        response->options[response->optionnum].length = 2;
        response->options[response->optionnum++].number = COAP_CONTENT_FORMAT;

        //Obsluga MAX_AGE. Dodana funkcjonalnosc.
        uint8_t maxAge = 0;
        if (actualObserver != nullptr)
        {
            maxAge = (uint8_t)(actualObserver->maxAge / 1000);
            response->options[response->optionnum].buffer = &maxAge;
            response->options[response->optionnum].length = 1;
            response->options[response->optionnum++].number = COAP_MAX_AGE;
        }

        sendPacket(response, ip, port);
    }
    else if (request->code_() == COAP_PUT)
    {
        //To improve...
        if (erType != -1)
        {
            response->code = erType;
        }
        else
        {
            response->code = COAP_CHANGED;
        }
        response->payloadlen = payloadLen;
        if (payloadLen > 0)
        {
            response->payload = (uint8_t *)payload;
        }
        else
        {
            response->payload = nullptr;
        }
        response->optionnum = 0;
        char optionBuffer[2];
        optionBuffer[0] = ((uint16_t)contentType & 0xFF00) >> 8;
        optionBuffer[1] = ((uint16_t)contentType & 0x00FF);
        response->options[response->optionnum].buffer = (uint8_t *)optionBuffer;
        response->options[response->optionnum].length = 2;
        response->options[response->optionnum++].number = COAP_CONTENT_FORMAT;

        sendPacket(response, ip, port);
    }
    else if (request->code_() == COAP_POST || request->code_() == COAP_DELETE)
    {
        //To do...
        response->code = erType;
        response->payloadlen = payloadLen;
        if (payloadLen > 0)
        {
            response->payload = (uint8_t *)payload;
        }
        else
        {
            response->payload = nullptr;
        }
        response->optionnum = 0;
        char optionBuffer[2];
        optionBuffer[0] = ((uint16_t)contentType & 0xFF00) >> 8;
        optionBuffer[1] = ((uint16_t)contentType & 0x00FF);
        response->options[response->optionnum].buffer = (uint8_t *)optionBuffer;
        response->options[response->optionnum].length = 2;
        response->options[response->optionnum++].number = COAP_CONTENT_FORMAT;
        sendPacket(response, ip, port);
    }

    //Usuwamy obiekt przechowujacy pakiet po wyslaniu pakietu.
    delete response;
    response = nullptr;
}
//Przeciazona funkcja sendResponse.
void coapServer::sendResponse(IPAddress ip, int port, int erType, COAP_CONTENT_TYPE contentType, char *payload, uint8_t payloadLen)
{
    this->sendResponse(ip, port, erType, contentType, payload, payloadLen, 0);
}
//Przeciazona funkcja sendResponse
void coapServer::sendResponse(IPAddress ip, int port, COAP_CONTENT_TYPE contentType, char *payload, uint8_t payloadLen, int store)
{
    this->sendResponse(ip, port, -1, contentType, payload, payloadLen, store);
}
//Przeciazona funkcja sendResponse
void coapServer::sendResponse(IPAddress ip, int port, COAP_CONTENT_TYPE contentType, char *payload, uint8_t payloadLen)
{
    this->sendResponse(ip, port, -1, contentType, payload, payloadLen, 0);
}
//Przeciazona funkcja sendResponse
void coapServer::sendResponse(IPAddress ip, int port, char *payload, uint8_t payloadLen)
{
    this->sendResponse(ip, port, COAP_TEXT_PLAIN, payload, payloadLen, 0);
}

//Dodawanie obserwatora.
void coapServer::addObserver(String url, coapPacket *request, IPAddress ip, int port)
{
    //storing the details of clients
    observer[obsCounter].tokenlen = request->tokenlen;
    observer[obsCounter].token = new uint8_t[observer[obsCounter].tokenlen];
    memcpy(observer[obsCounter].token, request->token, request->tokenlen);
    observer[obsCounter].clientip = ip;
    observer[obsCounter].clientport = port;
    observer[obsCounter].url = url;
    observer[obsCounter].maxAge = MAX_AGE_DEFAULT * 1000;
    for (int i = 0; i < request->optionnum; i++)
    {
        if (request->options[i].number == COAP_MAX_AGE)
        {
            observer[obsCounter].maxAge = ((unsigned long)(*request->options[i].buffer)) * 1000;
            break;
        }
    }
    observer[obsCounter].etag = nullptr;
    observer[obsCounter].etagLen = 0;
    observer[obsCounter].storedResponse = nullptr;
    observer[obsCounter].storedResponseLen = 0;
    observer[obsCounter].eTagActivated = false;
    observer[obsCounter++].repeatedPayload = 0;
}

//Wywolywanie notyfikacji obserwatora. Funkcja modyfikowana.
void coapServer::notification(char *payload, String url, uint8_t payloadLen)
{
    coapPacket *response = new coapPacket();

    response->version = COAP_VERSION;
    response->code = COAP_CONTENT;
    response->type = COAP_NONCON;
    response->payload = nullptr;
    response->payload = (uint8_t *)payload;
    response->payloadlen = payloadLen;
    response->optionnum = 0;
    //Licznik stanu obsState jest typu uint16_t, stąd gdy dojdziemy do granicznej wartości musimy go zrestartować
    //obsState counter is uint16_t type, so if we reach frontier value we should rest counter to default value
    if (obsState >= 65532)
    {
        obsState = 10;
    }
    else
    {
        obsState++;
    }
    //Konwersja zmiennej typu uint16_t na tablice typu uint8_t
    //Conversion from uint16_t type to int8_t type array
    obsStateArray[0] = (obsState >> 8);
    obsStateArray[1] = obsState & 0xff;
    response->options[response->optionnum].buffer = (uint8_t *)obsStateArray;
    response->options[response->optionnum].length = 2;
    response->options[response->optionnum++].number = COAP_OBSERVE;
    response->options[response->optionnum].buffer = 0;
    response->options[response->optionnum].length = 0;
    response->options[response->optionnum++].number = COAP_CONTENT_FORMAT;

    for (uint8_t i = 0; i < obsCounter; i++)
    {
        //send notification
        if (observer[i].url == url)
        {
            observer[i].repeatedPayload++;
            response->tokenlen = observer[i].tokenlen;
            response->token = observer[i].token;
            uint8_t maxAge = (uint8_t)(observer[i].maxAge / 1000);
            response->options[response->optionnum].buffer = &maxAge;
            response->options[response->optionnum].length = 1;
            response->options[response->optionnum++].number = COAP_MAX_AGE;
            sendPacket(response, observer[i].clientip,
                       observer[i].clientport);
            observer[i].prevMillis = (unsigned long)millis();
        }
    }
    delete response;
    response = nullptr;
}

//Funkcja wysyla odpowieni kod gdy nie znajdzie zadanego zasobu. Funkcja wlasna.
void coapServer::sendError(coapPacket *packet, IPAddress ip, int port, COAP_RESPONSE_CODE error)
{
    //Gdy zadany zasob nie wystepuje w bazie, serwer wysyla odp. z kodem NOT FOUND
    request->payload = nullptr;
    request->payloadlen = 0;
    request->code = error;
    request->optionnum = 0;

    char optionBuffer[2];
    optionBuffer[0] = ((uint16_t)COAP_TEXT_PLAIN & 0xFF00) >> 8;
    optionBuffer[1] = ((uint16_t)COAP_TEXT_PLAIN & 0x00FF);
    request->options[request->optionnum].buffer = (uint8_t *)optionBuffer;
    request->options[request->optionnum].length = 2;
    request->options[request->optionnum++].number = COAP_CONTENT_FORMAT;
    sendPacket(request, ip, port);
}

//Funkcja zwraca format payload'u oczekiwany przez klienta. Funkcja wlasna.
uint8_t coapPacket::accept()
{
    if (optionnum != 0)
    {
        for (int i = 0; i < optionnum; ++i)
        {
            if (options[i].number == COAP_ACCEPT)
            {
                return *options[i].buffer;
            }
        }
        return (uint8_t)100;
    }
    else
        return (uint8_t)100;
}

//Funkcja porownuje dwie tablice. Funkcja wlasna.
bool coapServer::compareArray(uint8_t a[], uint8_t b[], uint8_t lenA, uint8_t lenB)
{
    if (a == NULL || b == NULL)
    {
        return false;
    }
    if (lenA != lenB)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < lenB; i++)
        {
            if (a[i] != b[i])
            {
                return false;
            }
        }
        return true;
    }
}

//Funkcja oblicza dlugosc tablicy w jakiej zmiesci się int. Funkcja wlasna.
uint8_t coapServer::countLength(uint8_t messageid)
{
    int tmp = messageid;
    int len = 0;
    while (tmp > 10)
    {
        tmp /= 10;
        len++;
    }
    len++;
    return len;
}

//Funkcja wywolywana w momencie usuwania obserwatora z listy. Zeruje dane. Funkcja wlasna.
void coapObserver::deleteObserver()
{
    if (token != NULL)
    {
        delete[] token;
        token = nullptr;
    }
    tokenlen = 0;
    url = "";
    maxAge = 0;
    prevMillis = 0;
    etagLen = 0;
    tokenlen = 0;
    delete[] etag;
    etag = nullptr;
    repeatedPayload = 0;
    eTagActivated = false;
    if (storedResponse != NULL)
    {
        delete[] storedResponse;
        storedResponse = nullptr;
    }
    storedResponseLen = 0;
}
