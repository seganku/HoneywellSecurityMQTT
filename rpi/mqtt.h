#ifndef __MQTT_H__
#define __MQTT_H__

#include <stdint.h>
#include <mosquittopp.h>

class Mqtt : public mosqpp::mosquittopp
{
    private:
        const char      *host;
        const char      *id;
        int             port;
        int             keepalive;

        void on_connect(int rc);
        void on_disconnect(int rc);
        void on_publish(int mid);

    public:
        Mqtt(const char *id, const char *host, int port);
        ~Mqtt();
        bool send(const char * _topic, const char * _message);
};

#endif
