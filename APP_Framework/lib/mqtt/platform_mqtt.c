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
* @file:    platform_mqtt.c
* @brief:   platform_mqtt.c file
* @version: 1.0
* @author:  AIIT XUOS Lab
* @date:    2023/7/27
*
*/

#include <string.h>
#include <stdint.h>
#include <adapter.h>
#include <transform.h>
#include "platform_mqtt.h"
#include "utils_hmacsha1.h"

MQTT_TCB Platform_mqtt;  //创建一个用于连接云平台mqtt的结构体
static struct Adapter *adapter;
static const uint8_t parket_connetAck[] = {0x20,0x02,0x00,0x00};   //连接成功服务器回应报文
static const uint8_t parket_disconnet[] = {0xE0,0x00};             //客户端主动断开连接发送报文
static const uint8_t parket_heart[] = {0xC0,0x00};                 //客户端发送保活心跳包
static const uint8_t parket_subAck[] = {0x90,0x03,0x00,0x0A,0x01}; //订阅成功服务器回应报文
static const uint8_t parket_unsubAck[] = {0xB0,0x02,0x00,0x0A};    //取消订阅成功服务器回应报文
static uint8_t mqtt_rxbuf[16];
static char server_ip[64];
static char server_port[16];

void MQTT_SetServer(const char *ip, const char *port)
{
    memset(server_ip, 0, sizeof(server_ip));
    memset(server_port, 0, sizeof(server_port));
    if (ip) { strncpy(server_ip, ip, sizeof(server_ip) - 1); }
    if (port) { strncpy(server_port, port, sizeof(server_port) - 1); }
}

/*******************************************************************************
* 函 数 名: AdapterNetActive
* 功能描述: 使能设备的网络模块连接,连接TCP服务器并进入透传模式，当前使用4G方式
* 形    参: 无
* 返 回 值: 0表示成功,其他值表示失败
*******************************************************************************/
int AdapterNetActive(void)
{
    int ret = 0;
    uint32_t baud_rate = BAUD_RATE_115200;
    adapter =  AdapterDeviceFindByName(ADAPTER_WIFI_NAME);
    adapter->socket.socket_id = 0;
    
    ret = AdapterDeviceOpen(adapter);
    if (ret < 0) 
    {
        goto out;   
    }
       
    ret = AdapterDeviceControl(adapter, OPE_INT, &baud_rate);
    if (ret < 0)
    {
        goto out;
    }

    // 使用动态设置的 MQTT 服务器地址和端口
    ret = AdapterDeviceConnect(adapter, CLIENT, server_ip, server_port, IPV4);
    if (ret < 0) 
    {
        goto out;
    }

out:
    if (ret < 0) 
    {
        AdapterDeviceClose(adapter);
    }

    return ret;
}



/*******************************************************************************
* 函 数 名: MQTT_Send
* 功能描述: MQTT client数据发送函数
* 形    参: buf:要发送的数据,buflen:要发送的数据长度
* 返 回 值: 发送成功为0,发送失败为-1
*******************************************************************************/
int MQTT_Send(const uint8_t* buf, int buflen)
{
    return AdapterDeviceSend(adapter, buf, buflen) ;
}


/*******************************************************************************
* 函 数 名: MQTT_Recv
* 功能描述: MQTT client数据接收函数
* 形    参: buf:数据缓冲区,buflen:期望接收的数据长度
* 返 回 值: 实际接收到的数据长度,接收失败为-1
*******************************************************************************/
int MQTT_Recv(uint8_t* buf, int buflen)
{
    return AdapterDeviceRecv(adapter, buf, buflen) ;
}


/*******************************************************************************
* 函 数 名: MQTT_Connect
* 功能描述: 登录MQTT服务器
* 形    参: 无
* 返 回 值: true表示成功,false表示失败
*******************************************************************************/
bool MQTT_Connect(void)
{
    uint8_t TryConnect_time = 10;  //尝试登录次数

    memset(&Platform_mqtt,0,sizeof(Platform_mqtt));
#ifdef XIUOS_PLATFORM
    sprintf(Platform_mqtt.ClientID,"%s",CLIENTID);   //客户端ID存入缓冲区
    sprintf(Platform_mqtt.Username,"%s",USERNAME);   //用户名存入缓冲区
    sprintf(Platform_mqtt.Passward,"%s",PASSWORD);   //用户名存入缓冲区
#endif
#ifdef ALIBABA_PLATFORM
    uint8_t passwdtemp[PASSWARD_SIZE];
    memset(passwdtemp,0,sizeof(passwdtemp));
    sprintf(Platform_mqtt.ClientID,"%s|securemode=3,signmethod=hmacsha1|",CLIENT_DEVICENAME);   //构建客户端ID并存入缓冲区
    sprintf(Platform_mqtt.Username,"%s&%s",CLIENT_DEVICENAME,PLATFORM_PRODUCTKEY);              //构建用户名并存入缓冲区
    sprintf(passwdtemp,"clientId%sdeviceName%sproductKey%s",CLIENT_DEVICENAME,CLIENT_DEVICENAME,PLATFORM_PRODUCTKEY);  //构建加密时的明文   
    utils_hmac_sha1(passwdtemp,strlen(passwdtemp),Platform_mqtt.Passward,(char *)CLIENT_DEVICESECRET,strlen(CLIENT_DEVICESECRET)); //以DeviceSecret为秘钥对temp中的明文进行hmacsha1加密即为密码
#endif

    Platform_mqtt.MessageID = 0;      //报文标识符清零,CONNECT报文虽然不需要添加报文标识符,但是CONNECT报文是第一个发送的报文,在此清零报文标识符为后续报文做准备
    Platform_mqtt.Fixed_len = 1;      //CONNECT报文固定报头长度暂定为1
    Platform_mqtt.Variable_len = 10;  //CONNECT报文可变报头长度为10
    Platform_mqtt.Payload_len = (2+strlen(Platform_mqtt.ClientID)) + (2+strlen(Platform_mqtt.Username)) + (2+strlen(Platform_mqtt.Passward)); //CONNECT报文中负载长度
    Platform_mqtt.Remaining_len = Platform_mqtt.Variable_len + Platform_mqtt.Payload_len; //剩余长度=可变报头长度+负载长度
    memset(Platform_mqtt.Pack_buff,0,sizeof(Platform_mqtt.Pack_buff));

    Platform_mqtt.Pack_buff[0] = 0x10;        //CONNECT报文,固定报头第1个字节0x10	
    do{
        if((Platform_mqtt.Remaining_len/128) == 0)
        {
            Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len] = Platform_mqtt.Remaining_len;
        }
        else
        {
            Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len] = (Platform_mqtt.Remaining_len%128)|0x80;
        }
        Platform_mqtt.Fixed_len++;
        Platform_mqtt.Remaining_len = Platform_mqtt.Remaining_len/128;
    }while(Platform_mqtt.Remaining_len);

    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+0] = 0x00;  //CONNECT报文,可变报头第1个字节:固定0x00
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+1] = 0x04;  //CONNECT报文,可变报头第2个字节:固定0x04
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+2] = 0x4D;  //CONNECT报文,可变报头第3个字节:固定0x4D,大写字母M
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+3] = 0x51;  //CONNECT报文,可变报头第4个字节:固定0x51,大写字母Q
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+4] = 0x54;  //CONNECT报文,可变报头第5个字节:固定0x54,大写字母T
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+5] = 0x54;  //CONNECT报文,可变报头第6个字节:固定0x54,大写字母T
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+6] = 0x04;  //CONNECT报文,可变报头第7个字节:固定0x04
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+7] = 0xC2;  //CONNECT报文,可变报头第8个字节:使能用户名和密码校验,不使用遗嘱功能,不保留会话功能
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+8] = KEEPALIVE_TIME/256;  //CONNECT报文,可变报头第9个字节:保活时间高字节
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+9] = KEEPALIVE_TIME%256;  //CONNECT报文,可变报头第10个字节:保活时间低字节,单位s

    /* CLIENT_ID */
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+10] = strlen(Platform_mqtt.ClientID)/256;   //客户端ID长度高字节
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+11] = strlen(Platform_mqtt.ClientID)%256;   //客户端ID长度低字节
    memcpy(&Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+12],Platform_mqtt.ClientID,strlen(Platform_mqtt.ClientID)); //复制过来客户端ID字串
    /* USER_NAME */
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+12+strlen(Platform_mqtt.ClientID)] = strlen(Platform_mqtt.Username)/256;  //用户名长度高字节
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+13+strlen(Platform_mqtt.ClientID)] = strlen(Platform_mqtt.Username)%256;  //用户名长度低字节
    memcpy(&Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+14+strlen(Platform_mqtt.ClientID)],Platform_mqtt.Username,strlen(Platform_mqtt.Username)); //复制过来用户名字串
    /* PASSWARD */
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+14+strlen(Platform_mqtt.ClientID)+strlen(Platform_mqtt.Username)] = strlen(Platform_mqtt.Passward)/256;  //密码长度高字节
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+15+strlen(Platform_mqtt.ClientID)+strlen(Platform_mqtt.Username)] = strlen(Platform_mqtt.Passward)%256;  //密码长度低字节
    memcpy(&Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+16+strlen(Platform_mqtt.ClientID)+strlen(Platform_mqtt.Username)],Platform_mqtt.Passward,strlen(Platform_mqtt.Passward)); //复制过来密码字串
    
    while(TryConnect_time > 0)
    {
        memset(mqtt_rxbuf,0,sizeof(mqtt_rxbuf));
        MQTT_Send(Platform_mqtt.Pack_buff,Platform_mqtt.Fixed_len + Platform_mqtt.Variable_len + Platform_mqtt.Payload_len);
        MdelayKTask(50);
        MQTT_Recv(mqtt_rxbuf, 4);
        if(mqtt_rxbuf[0] == parket_connetAck[0] && mqtt_rxbuf[1] == parket_connetAck[1]) //连接成功
        {
            return true;
        }
        TryConnect_time--;
    }
    return false;
}


/*******************************************************************************
* 函 数 名: MQTT_Disconnect
* 功能描述: 断开与MQTT服务器的连接
* 形    参: 无
* 返 回 值: 无
*******************************************************************************/
void MQTT_Disconnect(void)
{
    while(MQTT_Send(parket_disconnet,sizeof(parket_disconnet)) < 0);
}


/*******************************************************************************
* 函 数 名: MQTT_SubscribeTopic
* 功能描述: MQTT订阅单个主题
* 形    参: topic_name:要订阅的主题
* 返 回 值: true表示订阅成功,false表示订阅失败
*******************************************************************************/
bool MQTT_SubscribeTopic(uint8_t *topic_name)
{
    uint8_t TrySub_time = 10; //尝试订阅次数

    Platform_mqtt.Fixed_len = 1;   //SUBSCRIBE报文,固定报头长度暂定为1
    Platform_mqtt.Variable_len = 2;//SUBSCRIBE报文,可变报头长度=2,2为字节报文标识符
    Platform_mqtt.Payload_len = 0; //SUBSCRIBE报文,负载数据长度暂定为0

    Platform_mqtt.Payload_len = strlen(topic_name) + 2 + 1; //每个需要订阅的topic除了本身的字符串长度,还包含表示topic字符串长度的2字节,以及订阅等级1字节
    Platform_mqtt.Remaining_len = Platform_mqtt.Variable_len + Platform_mqtt.Payload_len; //计算剩余长度=可变报头长度+负载长度
    memset(Platform_mqtt.Pack_buff,0,sizeof(Platform_mqtt.Pack_buff));

    Platform_mqtt.Pack_buff[0]=0x82;  //SUBSCRIBE报文,固定报头第1个字节0x82
    do{
        if((Platform_mqtt.Remaining_len/128) == 0)
        {
            Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len] = Platform_mqtt.Remaining_len;
        }
        else
        {
            Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len] = (Platform_mqtt.Remaining_len%128)|0x80;
        }
        Platform_mqtt.Fixed_len++;
        Platform_mqtt.Remaining_len = Platform_mqtt.Remaining_len/128;
    }while(Platform_mqtt.Remaining_len);

    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+0] = Platform_mqtt.MessageID/256; //报文标识符高字节
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+1] = Platform_mqtt.MessageID%256; //报文标识符低字节
    Platform_mqtt.MessageID++;                                                    //每用一次MessageID加1

    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+2] = strlen(topic_name)/256;               //主题长度高字节
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+3] = strlen(topic_name)%256;               //主题长度低字节
    memcpy(&Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+4],topic_name,strlen(topic_name)); //复制主题字串
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+4+strlen(topic_name)] = 0;                 //QOS等级设置为0
    
    while(TrySub_time > 0)
    {
        memset(mqtt_rxbuf,0,sizeof(mqtt_rxbuf));
        MQTT_Send(Platform_mqtt.Pack_buff,Platform_mqtt.Fixed_len + Platform_mqtt.Variable_len + Platform_mqtt.Payload_len);
        MdelayKTask(50);
        MQTT_Recv(mqtt_rxbuf, 5);
        if(mqtt_rxbuf[0] == parket_subAck[0] && mqtt_rxbuf[1] == parket_subAck[1]) //订阅成功
        {
            return true;
        }
        TrySub_time--;
    }
    return false;
}


/*******************************************************************************
* 函 数 名: MQTT_UnSubscribeTopic
* 功能描述: MQTT取消订阅单个主题
* 形    参: topic_name:要取消订阅的主题
* 返 回 值: true表示取消订阅成功,false表示取消订阅失败
*******************************************************************************/
bool MQTT_UnSubscribeTopic(uint8_t *topic_name)
{
    uint8_t TryUnSub_time = 10; //尝试取消订阅次数

    Platform_mqtt.Fixed_len = 1;                        //UNSUBSCRIBE报文,固定报头长度暂定为1
    Platform_mqtt.Variable_len = 2;                     //UNSUBSCRIBE报文,可变报头长度=2,2为字节报文标识符
    Platform_mqtt.Payload_len = strlen(topic_name) + 2; //每个需要取消的订阅topic除了本身的字符串长度,还包含表示topic字符串长度的2字节
    Platform_mqtt.Remaining_len = Platform_mqtt.Variable_len + Platform_mqtt.Payload_len; //计算剩余长度=可变报头长度+负载长度
    memset(Platform_mqtt.Pack_buff,0,sizeof(Platform_mqtt.Pack_buff));

    Platform_mqtt.Pack_buff[0]=0xA0; //UNSUBSCRIBE报文,固定报头第1个字节0xA0
    do{
        if((Platform_mqtt.Remaining_len/128) == 0)
        {
            Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len] = Platform_mqtt.Remaining_len;
        }
        else
        {
            Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len] = (Platform_mqtt.Remaining_len%128)|0x80;
        }
        Platform_mqtt.Fixed_len++;
        Platform_mqtt.Remaining_len = Platform_mqtt.Remaining_len/128;
    }while(Platform_mqtt.Remaining_len);

    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+0] = Platform_mqtt.MessageID/256; //报文标识符高字节
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+1] = Platform_mqtt.MessageID%256; //报文标识符低字节
    Platform_mqtt.MessageID++;                                                        //每用一次MessageID加1

    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+2] = strlen(topic_name)/256;               //主题长度高字节
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+3] = strlen(topic_name)%256;               //主题长度低字节
    memcpy(&Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+4],topic_name,strlen(topic_name)); //复制主题字串
    
    while(TryUnSub_time > 0)
    {
        memset(mqtt_rxbuf,0,sizeof(mqtt_rxbuf));
        MQTT_Send(Platform_mqtt.Pack_buff,Platform_mqtt.Fixed_len + Platform_mqtt.Variable_len + Platform_mqtt.Payload_len);
        MdelayKTask(50);
        MQTT_Recv(mqtt_rxbuf, 4);
        if(mqtt_rxbuf[0] == parket_unsubAck[0] && mqtt_rxbuf[1] == parket_unsubAck[1]) //取消订阅成功
        {
            return true;
        }
        TryUnSub_time--;
    }
    return false;
}


/*******************************************************************************
* 函 数 名: MQTT_PublishDataQs0
* 功能描述: 向服务器发送等级0的Publish报文
* 形    参: topic_name:主题名称
            data:数据缓存
            data_len:数据长度
* 返 回 值: 发布Qs=0的消息服务器不返回确认消息
*******************************************************************************/
void MQTT_PublishDataQs0(uint8_t *topic_name,uint8_t *data, uint16_t data_len)
{   
    Platform_mqtt.Fixed_len = 1;                            //PUBLISH等级0报文固定报头长度暂定为1
    Platform_mqtt.Variable_len = 2 + strlen(topic_name);    //PUBLISH等级0报文,可变报头长度=2字节topic长度标识字节+topic字符串的长度
    Platform_mqtt.Payload_len = data_len;                   //PUBLISH等级0报文,负载数据长度=data_len
    Platform_mqtt.Remaining_len = Platform_mqtt.Variable_len + Platform_mqtt.Payload_len; //计算剩余长度=可变报头长度+负载长度
    memset(Platform_mqtt.Pack_buff,0,sizeof(Platform_mqtt.Pack_buff));
            
    Platform_mqtt.Pack_buff[0]=0x30; //PUBLISH等级0报文固定报头第1个字节0x30
    do{
        if((Platform_mqtt.Remaining_len/128) == 0)
        {
            Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len] = Platform_mqtt.Remaining_len;
        }
        else
        {
            Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len] = (Platform_mqtt.Remaining_len%128)|0x80;
        }
        Platform_mqtt.Fixed_len++;
        Platform_mqtt.Remaining_len = Platform_mqtt.Remaining_len/128;
    }while(Platform_mqtt.Remaining_len);

    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+0]=strlen(topic_name)/256;                    //主题长度高字节
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+1]=strlen(topic_name)%256;                    //主题长度低字节
    memcpy(&Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+2],topic_name,strlen(topic_name));    //复制主题字串
    memcpy(&Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+2+strlen(topic_name)],data,data_len); //复制data数据

    MQTT_Send(Platform_mqtt.Pack_buff, Platform_mqtt.Fixed_len + Platform_mqtt.Variable_len + Platform_mqtt.Payload_len);
}


/*******************************************************************************
* 函 数 名: MQTT_PublishDataQs1
* 功能描述: 向服务器发送等级1的Publish报文
* 形    参: topic_name:主题名称
            data:数据缓存
            data_len:数据长度
* 返 回 值: 无
*******************************************************************************/
void MQTT_PublishDataQs1(uint8_t *topic_name,uint8_t *data, uint16_t data_len)
{
    Platform_mqtt.Fixed_len = 1;                             //PUBLISH等级1报文固定报头长度暂定为1
    Platform_mqtt.Variable_len = 2 + 2 + strlen(topic_name); //PUBLISH等级1报文,可变报头长度=2字节消息标识符+2字节topic长度标识字节+topic字符串的长度
    Platform_mqtt.Payload_len = data_len;                    //PUBLISH等级1报文,负载数据长度=data_len
    Platform_mqtt.Remaining_len = Platform_mqtt.Variable_len + Platform_mqtt.Payload_len; //计算剩余长度=可变报头长度+负载长度

    Platform_mqtt.Pack_buff[0] = 0x32; //等级1的Publish报文固定报头第1个字节，0x32
    do{
        if(Platform_mqtt.Remaining_len/128 == 0)
        {
            Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len] = Platform_mqtt.Remaining_len;
        }
        else
        {
            Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len] = (Platform_mqtt.Remaining_len%128)|0x80;
        }
        Platform_mqtt.Fixed_len++;
        Platform_mqtt.Remaining_len = Platform_mqtt.Remaining_len/128;
    }while(Platform_mqtt.Remaining_len);

    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+0] = strlen(topic_name)/256;               //主题长度高字节
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+1] = strlen(topic_name)%256;               //主题长度低字节
    memcpy(&Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+2],topic_name,strlen(topic_name)); //复制主题字串
 
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+2+strlen(topic_name)] = Platform_mqtt.MessageID/256; //报文标识符高字节
    Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+3+strlen(topic_name)] = Platform_mqtt.MessageID%256; //报文标识符低字节
    Platform_mqtt.MessageID++;                                                                           //每用一次MessageID加1

    memcpy(&Platform_mqtt.Pack_buff[Platform_mqtt.Fixed_len+4+strlen(topic_name)],data,strlen(data));  //复制data数据

    MQTT_Send(Platform_mqtt.Pack_buff,Platform_mqtt.Fixed_len + Platform_mqtt.Variable_len + Platform_mqtt.Payload_len);
}


/*******************************************************************************
* 函 数 名: MQTT_SendHeart
* 功能描述: 发送心跳保活包
* 形    参: 无
* 返 回 值: true表示发送成功,false表示发送失败
*******************************************************************************/
bool MQTT_SendHeart(void)
{
    uint8_t TrySentHeart_time = 10; //尝试发送心跳保活次数
    while(TrySentHeart_time > 0)
    {
        memset(mqtt_rxbuf,0,sizeof(mqtt_rxbuf));
        MQTT_Send(parket_heart,sizeof(parket_heart));
        MdelayKTask(50);
        MQTT_Recv(mqtt_rxbuf, 2);
        if(mqtt_rxbuf[0] == 0xD0 && mqtt_rxbuf[1] == 0x00)
        {
            return true;
        }
        TrySentHeart_time--;
    }
    return false;
}


/*******************************************************************************
* 函 数 名: MQTT_DealPublishData
* 功能描述: 处理服务器发来的等级0的推送数据,附带topic信息
* 形    参: data:接收的数据,data_len:要处理的数据长度
* 返 回 值: 报文中主题部分+实际负载的长度
*******************************************************************************/
uint16_t MQTT_DealPublishData(uint8_t *data, uint16_t data_len)
{
    uint8_t i;
    uint16_t startPos,payloadLen;

    for(i = 1;i < 5;i++)
    {
        //查找可变报头的长度字段,如果最高位为0表示该字节是长度字段的最后一个字节
        if((data[i] & 0x80) == 0)
            break;
    }

    //1代表固定报头占一个字节,i代表可变报头长度字段所占用字节数,2代表主题长度字段占2字节,startPos代表报文里主题名称起始位置
    startPos = 1+i+2;
    //data_len减去1+i+2就是报文中主题部分+实际负载的长度
    payloadLen = data_len-(1+i+2);

    if(data_len <= CMD_SIZE)
    {
        memset(Platform_mqtt.cmdbuff, 0, CMD_SIZE);
        memcpy(Platform_mqtt.cmdbuff, &data[startPos], payloadLen);
    }

    return payloadLen;
}
