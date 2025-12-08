
#include "tinyos.h"
#include "kernel_proc"

int socket_read();
int socket_write();
int socket_close();

static file_ops socket_file_ops = {
  .Open = NULL,
  .Read = socket_read,
  .Write = socket_write,
  .Close = socket_close
};

Fid_t sys_Socket(port_t port)
{
	Fid_t fid;
	FCB *fcb;
	int success = FCB_reserve(1,&fid,&fid);
	if(port<0||port>=MAX_PORT||success==0) return NOFILE;
	socket_cb *socket = (socket_cb*)xmalloc(sizeof(socket_cb));
	fcb->streamfunc->&socket_file_ops;
	fcb->streamobj->socket;
	socket->fcb=fcb;
	socket->type=SOCKET_UNBOUND;
	rlnode_init(&socket->unbound_s.unbound_socket,socket);
	if(port!=NOPORT&&PORT_MAP[port]!=NULL) {
	  rlist_push_front(&PORT_MAP[port]->unbound_s->unbound_socket,&socket->unbound_s.unbound_socket);
	  socket->port=port;
	}else if(port!=NOPORT&&PORT_MAP[port]==NULL){
	  PORT_MAP[port]=socket;
	  socket->port=port;
	}else socket->port=NOPORT;
	return fid;
}

int sys_Listen(Fid_t sock)
{
	if(sock<0||sock>=MAX_FILEID) return -1;
	socket_cb* socket = (socket_cb*) get_fcb(sock)->streamobj;
	if(socket==NULL||socket->port==NOPORT||socket->type==SOCKET_LISTENER||PORT_MAP[socket->port]->unbound_s.unbound_socket->scb->type==SOCKET_LISTENER) return -1;
	socket->type=SOCKET_LISTENER;
	rlnode temp;
	rlnode_swap(&temp,&socket->unbound_s.unbound_socket);
	rlnode_swap(&socket->listener_s.queue,&temp);
	if(PORT_MAP[socket->port]!=socket) rlist_push_front(&PORT_MAP[socket->port]->unbound_s.unbound_socket,&socket->listener_s.queue);
	PORT_MAP[socket->port]=socket;
	return 0;
}


Fid_t sys_Accept(Fid_t lsock)
{
	if(lsock<0||lsock>=MAX_FILEID) return NOFILE;
	socket_cb* socket = (socket_cb*) get_fcb(sock)->streamobj;
	if(socket==NULL||socket->type==SOCKET_LISTENER) return NOFILE;
	socket->refcount++;
	while(socket!=closed && avother_variable==something){
	  kernel_wait(&socket->listener_s.req_available,SCHED_IO);
	}
	refcount--;
	if(socket==closed) return NOFILE;
	return connected_fid;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	return -1;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	return -1;
}

