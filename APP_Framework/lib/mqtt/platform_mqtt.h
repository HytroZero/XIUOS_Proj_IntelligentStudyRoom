/*
* Copyright (c) 2020 AIIT XUOS Lab
* XiUOS is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*        http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

/**
* @file:    platform_mqtt.h
* @brief:   platform_mqtt.h file
* @version: 1.0
* @author:  AIIT XUOS Lab
* @date:    2023/7/27
*
*/

#ifndef _PLATFORM_MQTT_H_
#define	_PLATFORM_MQTT_H_

#include <stdint.h>
#include <stdbool.h>

#define  KEEPALIVE_TIME               300   //保活时间(单位s),300s
#define  HEART_TIME                200000   //空闲时发送心跳包的时间间隔(单位ms),200s
#define  PACK_SIZE                    512   //存放报文数据缓冲区大小
#define  MQTT_FRAME_SIZE             2048   //保存推送的PUBLISH报文中的数据负载大小,最大2k
#define  CMD_SIZE (MQTT_FRAME_SIZE + 1024)  //保存推送的PUBLISH报文中的数据缓冲区大小
#define  CLIENTID_SIZE                 64   //存放客户端ID的缓冲区大小
#define  USERNAME_SIZE                 64   //存放用户名的缓冲区大小
#define  PASSWARD_SIZE                 64   //存放密码的缓冲区大小

typedef struct{ 
    uint8_t ClientID[CLIENTID_SIZE];  //存放客户端ID的缓冲区
    uint8_t Username[USERNAME_SIZE];  //存放用户名的缓冲区
    uint8_t Passward[PASSWARD_SIZE];  //存放密码的缓冲区
    uint8_t  Pack_buff[PACK_SIZE];    //存放发送报文数据缓冲区
    uint16_t MessageID;               //记录报文标识符
    uint16_t Fixed_len;               //固定报头长度
    uint16_t Variable_len;            //可变报头长度
    uint16_t Payload_len;             //有效负荷长度
    uint16_t Remaining_len;           //保存报文剩余长度字节
    uint8_t  cmdbuff[CMD_SIZE];       //保存推送的PUBLISH报文中的数据缓冲区
}MQTT_TCB;

extern MQTT_TCB Platform_mqtt; //外部变量声明
 
int AdapterNetActive(void);
int MQTT_Send(const uint8_t* buf, int buflen);
int MQTT_Recv(uint8_t* buf, int buflen);
bool MQTT_Connect(void);
void MQTT_Disconnect(void);
bool MQTT_SubscribeTopic(uint8_t *topic_name);
bool MQTT_UnSubscribeTopic(uint8_t *topic_name);
void MQTT_PublishDataQs0(uint8_t *topic_name,uint8_t *data, uint16_t data_len);
void MQTT_PublishDataQs1(uint8_t *topic_name,uint8_t *data, uint16_t data_len);
bool MQTT_SendHeart(void);
uint16_t MQTT_DealPublishData(uint8_t *data, uint16_t data_len);
void MQTT_SetServer(const char *ip, const char *port);
#endif