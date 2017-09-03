#include "mqtt.h"

#include <mosquittopp.h>
#include <iostream>
#include <stdio.h>
#include <string.h>

Mqtt::Mqtt(const char * _id, const char * _host, int _port) : mosquittopp(_id)
{
    int version = MQTT_PROTOCOL_V311;
    mosqpp::lib_init();
    this->keepalive = 60;    
    this->id = _id;
    this->port = _port;
    this->host = _host;
    // Set version to 3.1.1
    opts_set(MOSQ_OPT_PROTOCOL_VERSION, &version);
    // non blocking connection to broker request;
    connect_async(host, port, keepalive);
    // Start thread managing connection / publish / subscribekeepalive);
    loop_start();
};

Mqtt::~Mqtt() {
    loop_stop();
    mosqpp::lib_cleanup();
}


void Mqtt::on_disconnect(int rc) {
    std::cout << ">> Mqtt - disconnected(" << rc << ")" << std::endl;
}

void Mqtt::on_connect(int rc)
{
    if ( rc == 0 ) {
        std::cout << ">> Mqtt - connected" << std::endl;
    } else {
        std::cout << ">> Mqtt - failed to connect: (" << rc << ")" << std::endl;
    }
}

void Mqtt::on_publish(int mid)
{
    std::cout << ">> Mqtt - Message (" << mid << ") published " << std::endl;
}

bool Mqtt::send(const char * _topic, const char * _message)
{
    // Send - depending on QoS, mosquitto lib managed re-submission this the thread
    //
    // * NULL : Message Id (int *) this allow to latter get status of each message
    // * topic : topic to be used
    // * length of the message
    // * message
    // * qos (0,1,2)
    // * retain (boolean) - indicates if message is retained on broker or not
    // Should return MOSQ_ERR_SUCCESS
    int ret = publish(NULL,_topic,strlen(_message),_message,1,false);
    return ( ret == MOSQ_ERR_SUCCESS );
}
