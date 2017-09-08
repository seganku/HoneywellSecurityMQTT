#include "digitalDecoder.h"
#include "mqtt.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>



#define SYNC_MASK    0xFFFF000000000000ul
#define SYNC_PATTERN 0xFFFE000000000000ul

void DigitalDecoder::setRxGood(bool state)
{
    if (state != rxGood)
    {
        mqtt.send("/security/sensors345/rx_status", state ? "OK" : "FAILED");
    }
    rxGood = state;
}

void DigitalDecoder::updateDeviceState(uint32_t serial, uint8_t state)
{
    deviceState_t ds;
    uint8_t alarmState;
    std::ostringstream alarmTopic;
    alarmTopic << "/security/sensors345/" << serial;
    std::ostringstream statusTopic(alarmTopic.str());
    alarmTopic << "/alarm";
    statusTopic << "/status";
    
    // Extract prior info
    if(deviceStateMap.count(serial))
    {
        ds = deviceStateMap[serial];
    }
    else
    {
        ds.minAlarmStateSeen = 0xFF;
    }
    
    // Update minimum/OK state if needed
    // Look only at the non-tamper loop bits
    alarmState = (state & 0xB);
    if(alarmState < ds.minAlarmStateSeen) ds.minAlarmStateSeen = alarmState; 
    
    // Decode alarm bits
    // We just alarm on any active loop that has been previously observed as inactive
    // This hopefully avoids having to use per-sensor configuration
    ds.alarm = (alarmState > ds.minAlarmStateSeen);
    
    // Decode tamper bit
    ds.tamper = (state & 0x40);
    
    // Decode battery low bit    
    ds.batteryLow = (state & 0x08);
    
    // Timestamp
    timeval now;
    gettimeofday(&now, nullptr);
    ds.lastUpdateTime = now.tv_sec;
    
    if(ds.alarm) ds.lastAlarmTime = now.tv_sec;

    // Put the answer back in the map
    deviceStateMap[serial] = ds;
    
    // Send the notification if something changed
    if(state != ds.lastRawState)
    {
        std::ostringstream status;

        // Send alarm state
        mqtt.send(alarmTopic.str().c_str(), ds.alarm ? "ALARM" : "OK");

        // Build and send combined status
        if (!ds.tamper && !ds.batteryLow)
        {
            status << "OK";
        } else {

            if (ds.tamper)
            {
                status << "TAMPER ";
            }
            
            if (ds.batteryLow)
            {
                status << "LOWBATT";
            }
        }
        mqtt.send(statusTopic.str().c_str(), status.str().c_str());

    }
    deviceStateMap[serial].lastRawState = state;
    
    for(const auto &dd : deviceStateMap)
    {
        printf("%sDevice %7u: %s %s %s %s\n",dd.first==serial ? "*" : " ", dd.first, dd.second.alarm ? "ALARM" : "OK", dd.second.tamper ? "TAMPER" : "", dd.second.batteryLow ? "LOWBATT" : "", dd.second.timeout ? "TIMEOUT" : "");
    }
    printf("\n");
}

void DigitalDecoder::handlePayload(uint64_t payload)
{
    uint64_t sof = (payload & 0xF00000000000) >> 44;
    uint64_t ser = (payload & 0x0FFFFF000000) >> 24;
    uint64_t typ = (payload & 0x000000FF0000) >> 16;
    uint64_t crc = (payload & 0x00000000FFFF) >>  0;
    
    //
    // Check CRC
    //
    uint64_t polynomial;
    if (sof == 0x2 || sof == 0xA) {
        // 2GIG brand
        polynomial = 0x18050;
    } else {
        // sof == 0x8
        polynomial = 0x18005;
    }
    uint64_t sum = payload & (~SYNC_MASK);
    uint64_t current_divisor = polynomial << 31;
    
    while(current_divisor >= polynomial)
    {
#ifdef __arm__
        if(__builtin_clzll(sum) == __builtin_clzll(current_divisor))
#else        
        if(__builtin_clzl(sum) == __builtin_clzl(current_divisor))
#endif
        {
            sum ^= current_divisor;
        }        
        current_divisor >>= 1;
    }
    
    const bool valid = (sum == 0);
    
    //
    // Tell the world
    //
    if(valid)
    {
        // We received a valid packet so the receiver must be working
        setRxGood(true);
        // Update the device
        updateDeviceState(ser, typ);
    }
    
    
    //
    // Print Packet
    //
 #ifdef __arm__
     if(valid)    
         printf("Valid Payload: %llX (Serial %llu, Status %llX)\n", payload, ser, typ);
     else
         printf("Invalid Payload: %llX\n", payload);
 #else    
     if(valid)    
         printf("Valid Payload: %lX (Serial %lu, Status %lX)\n", payload, ser, typ);
     else
         printf("Invalid Payload: %lX\n", payload);
 #endif
    
    static uint32_t packetCount = 0;
    static uint32_t errorCount = 0;
    
    packetCount++;
    if(!valid)
    {
        errorCount++;
        printf("%u/%u packets failed CRC\n", errorCount, packetCount);
    }
}



void DigitalDecoder::handleBit(bool value)
{
    static uint64_t payload = 0;
    
    payload <<= 1;
    payload |= (value ? 1 : 0);
    
//#ifdef __arm__
//    printf("Got bit: %d, payload is now %llX\n", value?1:0, payload);
//#else
//    printf("Got bit: %d, payload is now %lX\n", value?1:0, payload);
//#endif     
    
    if((payload & SYNC_MASK) == SYNC_PATTERN)
    {
        handlePayload(payload);
        payload = 0;
    }
}

void DigitalDecoder::decodeBit(bool value)
{
    enum ManchesterState
    {
        LOW_PHASE_A,
        LOW_PHASE_B,
        HIGH_PHASE_A,
        HIGH_PHASE_B
    };
    
    static ManchesterState state = LOW_PHASE_A;
    
    switch(state)
    {
        case LOW_PHASE_A:
        {
            state = value ? HIGH_PHASE_B : LOW_PHASE_A;
            break;
        }
        case LOW_PHASE_B:
        {
            handleBit(false);
            state = value ? HIGH_PHASE_A : LOW_PHASE_A;
            break;
        }
        case HIGH_PHASE_A:
        {
            state = value ? HIGH_PHASE_A : LOW_PHASE_B;
            break;
        }
        case HIGH_PHASE_B:
        {
            handleBit(true);
            state = value ? HIGH_PHASE_A : LOW_PHASE_A;
            break;
        }
    }
}

void DigitalDecoder::handleData(char data)
{
    static const int samplesPerBit = 8;
    
        
    if(data != 0 && data != 1) return;
        
    const bool thisSample = (data == 1);
    
    if(thisSample == lastSample)
    {
        samplesSinceEdge++;
        
        //if(samplesSinceEdge < 100)
        //{
        //    printf("At %d for %u\n", thisSample?1:0, samplesSinceEdge);
        //}
        
        if((samplesSinceEdge % samplesPerBit) == (samplesPerBit/2))
        {
            // This Sample is a new bit
            decodeBit(thisSample);
        }
    }
    else
    {
        samplesSinceEdge = 1;
    }
    lastSample = thisSample;
}
