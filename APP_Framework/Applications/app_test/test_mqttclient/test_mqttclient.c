/*
 * Copyright (c) 2022 AIIT XUOS Lab
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
 * @file test_mqttclient.c
 * @brief mqtt subscribe fuction test
 * @version 3.0
 * @author AIIT XUOS Lab
 * @date 2023.8.10
 */

#include <transform.h>
#include "test_mqttclient.h"
#include <stdio.h>
#ifdef ADD_XIZI_FEATURES
#include <sys_arch.h>
#include <lwip/sockets.h>
#include "lwip/sys.h"
#include "lwip/api.h"

#include "mqtt/MQTTPacket.h"
#include "mqtt/MQTTSubscribe.h"
#include "mqtt/transport.h"
#include <cJSON_Process.h>

#endif

#ifdef ADD_NUTTX_FEATURES
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "stdio.h"
#endif


#define MQTT_DEMO_BUF_SIZE                    65535
#define MQTT_DEMO_SEND_TIMES                  20
#define LWIP_MQTT_DEMO_TASK_STACK_SIZE        4096
#define LWIP_MQTT_DEMO_TASK_PRIO              20

static char mqtt_demo_ipaddr[] = {192, 168, 76, 154};
static char mqtt_demo_netmask[] = {255, 255, 255, 0};
static char mqtt_demo_gwaddr[] = {192, 168, 76, 136};

static pthread_t mqtt_client_task;
static pthread_t mqtt_server_task;

static uint16_t mqtt_socket_port = 1883;
static char mqtt_ip_str[] = "192.168.76.154";

void MqttSocketConfigParam(char *ip_str)
{
    int ip1, ip2, ip3, ip4, port = 0;

    if(ip_str == NULL)
        return;

    if(sscanf(ip_str, "%d.%d.%d.%d:%d", &ip1, &ip2, &ip3, &ip4, &port)) {
        printf("config ip %s port %d\n", ip_str, port);
        strcpy(mqtt_ip_str, ip_str);
        if(port)
            mqtt_socket_port = port;
        return;
    }

    if(sscanf(ip_str, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4)) {
        printf("config ip %s\n", ip_str);
        strcpy(mqtt_ip_str, ip_str);
    }
}

MQTT_USER_MSG  mqtt_user_msg;

uint8_t MQTT_Connect(void)
{
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    uint8_t buf[200];
    int buflen = sizeof(buf);
    int len = 0;
    data.clientID.cstring = CLIENT_ID;                   //随机
    data.keepAliveInterval = KEEPLIVE_TIME;         //保持活跃
    data.username.cstring = USER_NAME;              //用户名
    data.password.cstring = PASSWORD;               //密钥
    data.MQTTVersion = MQTT_VERSION;                //3表示3.1版本，4表示3.11版本
    data.cleansession = 1;
    //组装消息
    len = MQTTSerialize_connect((unsigned char *)buf, buflen, &data);
    //发送消息
    transport_sendPacketBuffer(buf, len);
    
    /* 等待连接响应 */
    if (MQTTPacket_read(buf, buflen, transport_getdata) == CONNACK)
    {
        unsigned char sessionPresent, connack_rc;
        if (MQTTDeserialize_connack(&sessionPresent, &connack_rc, buf, buflen) != 1 || connack_rc != 0)
        {
          lw_print("无法连接，错误代码是: %d！\n", connack_rc);
            return Connect_NOK;
        }
        else 
        {
            lw_print("用户名与密钥验证成功，MQTT连接成功！\n");
            return Connect_OK;
        }
    }
    else
        lw_print("MQTT连接无响应！\n");
        return Connect_NOTACK;
}


int32_t MQTT_PingReq(int32_t sock)
{
	  int32_t len;
		uint8_t buf[200];
		int32_t buflen = sizeof(buf);	 
		fd_set readfd;
	  struct timeval tv;
	  tv.tv_sec = 5;
	  tv.tv_usec = 0;
	
	  FD_ZERO(&readfd);
	  FD_SET(sock,&readfd);			
	
		len = MQTTSerialize_pingreq(buf, buflen);
		transport_sendPacketBuffer(buf, len);
	
		//等待可读事件
		if(select(sock+1,&readfd,NULL,NULL,&tv) == 0)
			return -1;
		
	  //有可读事件
		if(FD_ISSET(sock,&readfd) == 0)
			return -2;
		
		if(MQTTPacket_read(buf, buflen, transport_getdata) != PINGRESP)
			return -3;
		
		return 0;
	
}


/************************************************************************
** 函数名称: MQTTSubscribe								
** 函数功能: 订阅消息
** 入口参数: int32_t sock：套接字
**           int8_t *topic：主题
**           enum QoS pos：消息质量
** 出口参数: >=0:发送成功 <0:发送失败
** 备    注: 
************************************************************************/
int32_t MQTTSubscribe(int32_t sock,char *topic,enum QoS pos)
{
	  static uint32_t PacketID = 0;
	  uint16_t packetidbk = 0;
	  int32_t conutbk = 0;
		uint8_t buf[100];
		int32_t buflen = sizeof(buf);
	  MQTTString topicString = MQTTString_initializer;  
		int32_t len;
	  int32_t req_qos,qosbk;

		fd_set readfd;
	  struct timeval tv;
	  tv.tv_sec = 2;
	  tv.tv_usec = 0;
	
	  FD_ZERO(&readfd);
	  FD_SET(sock,&readfd);		
	
	  //复制主题
    topicString.cstring = (char *)topic;
		//订阅质量
	  req_qos = pos;
	
	  //串行化订阅消息
    len = MQTTSerialize_subscribe(buf, buflen, 0, PacketID++, 1, &topicString, &req_qos);
		//发送TCP数据
	  if(transport_sendPacketBuffer(buf, len) < 0)
				return -1;
	  
    //等待可读事件--等待超时
		if(select(sock+1,&readfd,NULL,NULL,&tv) == 0)
				return -2;
		//有可读事件--没有可读事件
		if(FD_ISSET(sock,&readfd) == 0)
				return -3;

		//等待订阅返回--未收到订阅返回
		if(MQTTPacket_read(buf, buflen, transport_getdata) != SUBACK)
				return -4;	
		
		//拆订阅回应包
		if(MQTTDeserialize_suback(&packetidbk,1, &conutbk, &qosbk, buf, buflen) != 1)
				return -5;
		
		//检测返回数据的正确性
		if((qosbk == 0x80)||(packetidbk != (PacketID-1)))
				return -6;
		
    //订阅成功
		return 0;
}

int32_t ReadPacketTimeout(int32_t sock,uint8_t *buf,int32_t buflen,uint32_t timeout)
{
		fd_set readfd;
	  struct timeval tv;
	  if(timeout != 0)
		{
				tv.tv_sec = timeout;
				tv.tv_usec = 0;
				FD_ZERO(&readfd);
				FD_SET(sock,&readfd); 

				
				if(select(sock+1,&readfd,NULL,NULL,&tv) == 0)
						return -1;
				
				if(FD_ISSET(sock,&readfd) == 0)
						return -1;
	  }
		
		return MQTTPacket_read(buf, buflen, transport_getdata);
}

void deliverMessage(MQTTString  *TopicName,MQTTMessage *msg,MQTT_USER_MSG *mqtt_user_msg)
{
		//消息质量
		mqtt_user_msg->msgqos = msg->qos;
		//保存消息
		memcpy(mqtt_user_msg->msg,msg->payload,msg->payloadlen);
		mqtt_user_msg->msg[msg->payloadlen] = 0;
		//保存消息长度
		mqtt_user_msg->msglenth = msg->payloadlen;
		//消息主题
		memcpy((char *)mqtt_user_msg->topic,TopicName->lenstring.data,TopicName->lenstring.len);
		mqtt_user_msg->topic[TopicName->lenstring.len] = 0;
		//消息ID
		mqtt_user_msg->packetid = msg->id;
		//标明消息合法
		mqtt_user_msg->valid = 1;		
}

void UserMsgCtl(MQTT_USER_MSG  *msg)
{
		//这里处理数据只是打印，用户可以在这里添加自己的处理方式
	  lw_print("****收到订阅的消息******\n");
		//���غ�����Ϣ
	  switch(msg->msgqos)
		{
			case 0:
				    lw_print("MQTT>>消息质量QoS0\n");
				    break;
			case 1:
				    lw_print("MQTT>>消息质量QoS1\n");
				    break;
			case 2:
				    lw_print("MQTT>>消息质量QoS2\n");
				    break;
			default:
				    lw_print("MQTT>>错误的消息质量\n");
				    break;
		}
		lw_print("MQTT>>消息主题：%s\n",msg->topic);	
		lw_print("MQTT>>消息内容：%s\n",msg->msg);	
		lw_print("MQTT>>消息长度：%d\n",msg->msglenth);	 
    Proscess(msg->msg);
	  //处理完后销毁数据
	  msg->valid  = 0;
}


void mqtt_pktype_ctl(uint8_t packtype,uint8_t *buf,uint32_t buflen)
{
	  MQTTMessage msg;
		int32_t rc;
	  MQTTString receivedTopic;
	  uint32_t len;
    lw_print("packtype:%d\n",packtype);
		switch(packtype)
		{
			case PUBLISH:
        
        if(MQTTDeserialize_publish(&msg.dup,(int*)&msg.qos, &msg.retained, &msg.id, &receivedTopic,
          (unsigned char **)&msg.payload, &msg.payloadlen, buf, buflen) != 1)
            return;	
        
        deliverMessage(&receivedTopic,&msg,&mqtt_user_msg);
        
      
        if(msg.qos == QOS0)
        {
            //QOS0-不需要ACK
           //直接处理数据
           UserMsgCtl(&mqtt_user_msg);
           return;
        }
        //发送PUBACK消息
        if(msg.qos == QOS1)
        {
            len =MQTTSerialize_puback(buf,buflen,mqtt_user_msg.packetid);
            if(len == 0)
              return;
             //发送返回
            if(transport_sendPacketBuffer(buf,len)<0)
               return;	
             //返回后处理消息
            UserMsgCtl(&mqtt_user_msg); 
            return;												
        }

        //对于质量2,只需要发送PUBREC就可以了
        if(msg.qos == QOS2)
        {
           len = MQTTSerialize_ack(buf, buflen, PUBREC, 0, mqtt_user_msg.packetid);			                
           if(len == 0)
             return;
            //发送返回
           transport_sendPacketBuffer(buf,len);	
        }		
        break;
			case  PUBREL:				           
          //解析包数据，必须包ID相同才可以
        rc = MQTTDeserialize_ack(&msg.type,&msg.dup, &msg.id, buf,buflen);
        if((rc != 1)||(msg.type != PUBREL)||(msg.id != mqtt_user_msg.packetid))
          return ;
       //收到PUBREL，需要处理并抛弃数据
        if(mqtt_user_msg.valid == 1)
        {
            //返回后处理消息
           UserMsgCtl(&mqtt_user_msg);
        }      
        //串行化PUBCMP消息
        len = MQTTSerialize_pubcomp(buf,buflen,msg.id);	                   	
        if(len == 0)
          return;									
       //发送返回--PUBCOMP
        transport_sendPacketBuffer(buf,len);										
        break;
			case   PUBACK://等级1客户端推送数据后，服务器返回
				break;
			case   PUBREC://等级2客户端推送数据后，服务器返回
				break;
			case   PUBCOMP://等级2客户端推送PUBREL后，服务器返回
        break;
			default:
				break;
		}
}


static void *MqttSocketRecvTask(void *arg)
{
MQTT_START: 
    lw_print("Recv begin**********\n");
    int fd = -1, clientfd;
    int recv_len;
    int ret;
    char *recv_buf;
    struct sockaddr_in mqtt_addr;
    socklen_t addr_len;

     fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        lw_print("Socket error\n");
        return NULL;
    }

    struct sockaddr_in mqtt_sock;
    mqtt_sock.sin_family = AF_INET;
    mqtt_sock.sin_port = htons(mqtt_socket_port);
    mqtt_sock.sin_addr.s_addr = inet_addr(mqtt_ip_str);

    memset(&(mqtt_sock.sin_zero), 0, sizeof(mqtt_sock.sin_zero));

    ret = connect(fd, (struct sockaddr *)&mqtt_sock, sizeof(struct sockaddr));

    if (ret < 0) {
        lw_print("Unable to connect %s:%d = %d\n", mqtt_ip_str, mqtt_socket_port, ret);
        close(fd);
        return NULL;
    }

    lw_print("MQTT connect %s:%d success, begin to verify username and password.\n", mqtt_ip_str, mqtt_socket_port);
    
    if(MQTT_Connect() != Connect_OK)
    {
        lw_print("MQTT verify failed.\n");
        shutdown(fd, SHUT_WR);
        recv(fd, NULL, (size_t)0, 0);
        close(fd);
        PrivTaskDelay(1000);
        goto MQTT_START;
    }

    lw_print("MQTT subscribe begin.\n");
    if(MQTTSubscribe(fd,(char *)TOPIC,QOS1) < 0)
    {
        lw_print("MQTT subscribe failed.\n");
        shutdown(fd, SHUT_WR);
        recv(fd, NULL, (size_t)0, 0);
        close(fd);
        return NULL;
    }

    lw_print("subscribe success.\n");

    fd_set readfd;
    uint8_t no_mqtt_msg_exchange = 1;
    uint8_t buf[MSG_MAX_LEN];
	int32_t buflen = sizeof(buf);
    int32_t type;
    struct timeval tv;    
	tv.tv_sec = 0;
	tv.tv_usec = 10;

  int32_t curtick=0;

    while(1)
    {
      // lw_print("waiting********\n");
      curtick +=1;
      no_mqtt_msg_exchange = 1;
			
		  FD_ZERO(&readfd);
		  FD_SET(fd,&readfd);
      select(fd+1,&readfd,NULL,NULL,&tv);

      if(FD_ISSET(fd,&readfd) != 0)
      {
          
          type = ReadPacketTimeout(fd,buf,buflen,0);
          if(type != -1)
          {
            lw_print("ctl***********\n");
              mqtt_pktype_ctl(type,buf,buflen);
  
              no_mqtt_msg_exchange = 0;
								
					}
			}

      if( curtick >(2*10000))
        {
            curtick =0;
            //判断是否有数据交换
            if(no_mqtt_msg_exchange == 0)
            {
               //如果有数据交换，这次就不需要发送PING消息
               continue;
            }
            
            if(MQTT_PingReq(fd) < 0)
            {
               //重连服务器
               lw_print("发送保持活性ping失败....\n");
               goto CLOSE;	 
            }
            

            lw_print("发送保持活性ping作为心跳成功....\n");

            no_mqtt_msg_exchange = 0;
        } 
    }

CLOSE:
	  lw_print("MQTT subscribe failed.\n");
    shutdown(fd, SHUT_WR);
    recv(fd, NULL, (size_t)0, 0);
    close(fd);
    return NULL;


}



void MqttSocketRecvTest(int argc, char *argv[])
{
    if(argc >= 2) {
        lw_print("lw: [%s] target ip %s\n", __func__, argv[1]);
        MqttSocketConfigParam(argv[1]);
    }


    // ip4_addr_t dns_ip;
    // netconn_gethostbyname(HOST_NAME, &dns_ip);
    // char* host_ip = ip_ntoa(&dns_ip);
    // lw_print("host name : %s , host_ip : %s\n",HOST_NAME,host_ip);
    // MqttSocketConfigParam(host_ip);


#ifdef ADD_XIZI_FEATURES
    lwip_config_tcp(0, mqtt_demo_ipaddr, mqtt_demo_netmask, mqtt_demo_gwaddr);

    pthread_attr_t attr;
    attr.schedparam.sched_priority = LWIP_MQTT_DEMO_TASK_PRIO;
    attr.stacksize = LWIP_MQTT_DEMO_TASK_STACK_SIZE;
#endif

#ifdef ADD_NUTTX_FEATURES
    pthread_attr_t attr = PTHREAD_ATTR_INITIALIZER;
    attr.priority = LWIP_mqtt_DEMO_TASK_PRIO;
    attr.stacksize = LWIP_mqtt_DEMO_TASK_STACK_SIZE;
#endif

    PrivTaskCreate(&mqtt_server_task, &attr, &MqttSocketRecvTask, NULL);
    PrivTaskStartup(&mqtt_server_task);
}

PRIV_SHELL_CMD_FUNCTION(MqttSocketRecvTest, a tcp send sample, PRIV_SHELL_CMD_MAIN_ATTR);


typedef struct
{
  uint8_t  humi_high8bit;                //ԭʼ���ݣ�ʪ�ȸ�8λ
  uint8_t  humi_low8bit;                 //ԭʼ���ݣ�ʪ�ȵ�8λ
  uint8_t  temp_high8bit;                 //ԭʼ���ݣ��¶ȸ�8λ
  uint8_t  temp_low8bit;                 //ԭʼ���ݣ��¶ȸ�8λ
  uint8_t  check_sum;                     //У���
  double    humidity;        //ʵ��ʪ��
  double    temperature;     //ʵ���¶�  
} DHT11_Data_TypeDef;


uint16_t GetNextPackID(void)
{
	 static uint16_t pubpacketid = 0;
	 return pubpacketid++;
}

int32_t WaitForPacket(int32_t sock,uint8_t packettype,uint8_t times)
{
	  int32_t type;
		uint8_t buf[MSG_MAX_LEN];
	  uint8_t n = 0;
		int32_t buflen = sizeof(buf);
		do
		{
				//读取数据包
				type = ReadPacketTimeout(sock,buf,buflen,2);
			  if(type != -1)
					mqtt_pktype_ctl(type,buf,buflen);
				n++;
		}while((type != packettype)&&(n < times));
		//收到期望的包
		if(type == packettype)
			 return 0;
		else 
			 return -1;		
}

int32_t MQTTMsgPublish(int32_t sock, char *topic, int8_t qos, uint8_t* msg)
{
    int8_t retained = 0;      //保留标志位
    uint32_t msg_len;         //数据长度
		uint8_t buf[MSG_MAX_LEN];
		int32_t buflen = sizeof(buf),len;
		MQTTString topicString = MQTTString_initializer;
	  uint16_t packid = 0,packetidbk = 0;
	
		//填充主题
	  topicString.cstring = (char *)topic;

	  //填充数据包ID
	  if((qos == QOS1)||(qos == QOS2))
		{ 
			packid = GetNextPackID();
		}
		else
		{
			  qos = QOS0;
			  retained = 0;
			  packid = 0;
		}
     
    msg_len = strlen((char *)msg);
    
		//推送消息
		len = MQTTSerialize_publish(buf, buflen, 0, qos, retained, packid, topicString, (unsigned char*)msg, msg_len);
		if(len <= 0)
				return -1;
		if(transport_sendPacketBuffer(buf, len) < 0)	
				return -2;	
		
		//质量等级0，不需要返回
		if(qos == QOS0)
		{
				return 0;
		}
		
		//等级1
		if(qos == QOS1)
		{
				//等待PUBACK
			  if(WaitForPacket(sock,PUBACK,5) < 0)
					 return -3;
				return 1;
			  
		}
		//等级2
		if(qos == QOS2)	
		{
			  //等待PUBREC
			  if(WaitForPacket(sock,PUBREC,5) < 0)
					 return -3;
			  //发送PUBREL
        len = MQTTSerialize_pubrel(buf, buflen,0, packetidbk);
				if(len == 0)
					return -4;
				if(transport_sendPacketBuffer(buf, len) < 0)	
					return -6;			
			  //等待PUBCOMP
			  if(WaitForPacket(sock,PUBREC,5) < 0)
					 return -7;
				return 2;
		}
		//等级错误
		return -8;
}


static void *MqttSocketSendTask(void *arg)
{

  int fd = -1, clientfd;
    int recv_len;
    int ret;
    char *recv_buf;
    struct sockaddr_in mqtt_addr;
    socklen_t addr_len;

     fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        lw_print("Socket error\n");
        return NULL;
    }

    struct sockaddr_in mqtt_sock;
    mqtt_sock.sin_family = AF_INET;
    mqtt_sock.sin_port = htons(mqtt_socket_port);
    mqtt_sock.sin_addr.s_addr = inet_addr(mqtt_ip_str);

    memset(&(mqtt_sock.sin_zero), 0, sizeof(mqtt_sock.sin_zero));

    ret = connect(fd, (struct sockaddr *)&mqtt_sock, sizeof(struct sockaddr));

    if (ret < 0) {
        lw_print("Unable to connect %s:%d = %d\n", mqtt_ip_str, mqtt_socket_port, ret);
        close(fd);
        return NULL;
    }

    lw_print("MQTT connect %s:%d success, begin to verify hostname and password.\n", mqtt_ip_str, mqtt_socket_port);
    
    if(MQTT_Connect() != Connect_OK)
    {
        lw_print("MQTT verify failed.\n");
        shutdown(fd, SHUT_WR);
        recv(fd, NULL, (size_t)0, 0);
        close(fd);
        return NULL;
    }

    lw_print("MQTT subscribe begin.\n");
    if(MQTTSubscribe(fd,(char *)TOPIC,QOS1) < 0)
    {
        lw_print("MQTT subscribe failed.\n");
        shutdown(fd, SHUT_WR);
        recv(fd, NULL, (size_t)0, 0);
        close(fd);
        return NULL;
    }

    lw_print("subscribe success.\n");



    uint8_t no_mqtt_msg_exchange = 1;
    uint32_t curtick=0;
    uint8_t res;

    cJSON* cJSON_Data = NULL;
    cJSON_Data = cJSON_Data_Init();
    DHT11_Data_TypeDef* recv_data;

    double a,b;
    while(1)
    {
      curtick+=1;
      char* p ="Hello,here is hc";
      ret = MQTTMsgPublish(fd,(char*)TOPIC,QOS0,(uint8_t*)p);
      if(ret >= 0)
      {
          no_mqtt_msg_exchange = 0;
          PrivTaskDelay(1000);
      }
    }
}


void MqttSocketSendTest(int argc, char *argv[])
{
    if(argc >= 2) {
        lw_print("lw: [%s] target ip %s\n", __func__, argv[1]);
        MqttSocketConfigParam(argv[1]);
    }


    // ip4_addr_t dns_ip;
    // netconn_gethostbyname(HOST_NAME, &dns_ip);
    // char* host_ip = ip_ntoa(&dns_ip);
    // lw_print("host name : %s , host_ip : %s\n",HOST_NAME,host_ip);
    // MqttSocketConfigParam(host_ip);


#ifdef ADD_XIZI_FEATURES
    lwip_config_tcp(0, mqtt_demo_ipaddr, mqtt_demo_netmask, mqtt_demo_gwaddr);

    pthread_attr_t attr;
    attr.schedparam.sched_priority = LWIP_MQTT_DEMO_TASK_PRIO;
    attr.stacksize = LWIP_MQTT_DEMO_TASK_STACK_SIZE;
#endif

#ifdef ADD_NUTTX_FEATURES
    pthread_attr_t attr = PTHREAD_ATTR_INITIALIZER;
    attr.priority = LWIP_mqtt_DEMO_TASK_PRIO;
    attr.stacksize = LWIP_mqtt_DEMO_TASK_STACK_SIZE;
#endif

    PrivTaskCreate(&mqtt_client_task, &attr, &MqttSocketSendTask, NULL);
    PrivTaskStartup(&mqtt_client_task);
}

PRIV_SHELL_CMD_FUNCTION(MqttSocketSendTest, a tcp send sample, PRIV_SHELL_CMD_MAIN_ATTR);

