#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

/*
�����û��뿪�Ĵ���

1�����û��������ֱ�ӹرտͻ��������
a.receive_handle��ֱ�ӽ���error�ķ�֧
b.��error��֧�н�tcp_client.availible_�ó� false,��ʶsocket������
c.��error��֧����on_error.
d.��game_client.on_error�н���simulate_receive.BYE�����뿪֪ͨ�߼�����
e.on_error�������delete this.

2�����û�����Ƿ����ݡ����緢�ͳ���:
a.��receive_handle�������������˳����ļ�飬���ȵ���game_client.on_error��������error��
b.��game_client.on_error�н���simulate_receive.BYE�����û����뿪֪ͨ�߼�����
c.��game_service.on_client_leave�н������ԭ��֪ͨ�û����������ﲢ���ر�socket
d.�߼����ص�receive_handle��delete this. ����ʵ��

3�����û�����BYE�������뿪
a.��receive_handle�е�on_data�н��д���
b.on_data��BYE�͸��߼�������������game_service.on_client_leave�� �ɸķ�������client.disconnect��availiable��false�����ر�socket
c.���ص�receive_handle������䴦��������䷢�־���on_data��availible�Ѿ���false,˵���û��Ѿ������ã�delete this.

4���첽�ļ�飬�����û���ʱ��
a.�粻���ԣ���ֱ�ӵ���game_client.disconnect����availible��false�����ر�socket.
b.�ر�socket�� receive_handle����ⷵ�أ�������error��֧.
c.error��֧��֧����on_error
d.��game_client.on_error�н���simulate_receive.BYE�����뿪֪ͨ�߼�����
e.on_error�������delete this.
*/


#include <iostream>
#include <boost\bind.hpp>
#include <boost\asio.hpp>
#include <boost\noncopyable.hpp>
#include "service_error.h"

namespace dooqu_server
{
	namespace net
	{
		using namespace boost::asio;
		using namespace boost::asio::ip;

		class tcp_server;

		class tcp_client : boost::noncopyable
		{
			//��tcp_server����Ϊ����
			friend class tcp_server;

		protected:

			//�������ݻ������Ĵ�С
			enum{ MAX_BUFFER_SIZE = 65};

			//�������ݻ�����
			char buffer[MAX_BUFFER_SIZE];


			//ָ�򻺳�����ָ�룬ʹ��asio::buffer�ᶪʧλ��
			char* p_buffer;


			//��ǰ��������λ��
			unsigned int buffer_pos ;


			//io_service����
			io_service& ios;


			//��ǰ��tcp_server����
			tcp_server* tcpserver;


			//������socket����
			tcp::socket t_socket;	


			//������ʶsocket�����Ƿ����
			bool available_;



			//��ʼ�ӿͻ��˶�ȡ����
			void read_from_client();


			//�����������ݵĻص�����
			void receive_handle(const boost::system::error_code& error, size_t bytes_received);


			//�������ݵĻص�����
			void send_handle(const boost::system::error_code& error);		



			//�������ݵ���ʱ�ᱻ���ã��÷���Ϊ�鷽������������о����߼��Ĵ��������
			virtual void on_data(char* client_data) = 0;


			//���ͻ��˳��ֹ������ʱ���е��ã� �÷���Ϊ�鷽������������о�����߼���ʵ�֡�
			virtual void on_error(const int error) = 0;

		public:
			tcp_client(io_service& ios);	


			tcp::socket& socket(){ return this->t_socket; }


			virtual bool available(){ return this->available_ && this->t_socket.is_open();}


			void write(char* data);


			void write(bool asynchronized, char* data);


			void write(bool asynchronized, ...);


			virtual void disconnect();


			virtual ~tcp_client();
		};
	}
}


#endif