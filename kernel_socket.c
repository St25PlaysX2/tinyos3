
#include "tinyos.h"
#include "kernel_proc.h"
#include "kernel_cc.h"

socket_cb* PORT_MAP[MAX_PORT+1];

int socket_read(void* pipecb_t, char *buf, unsigned int n);
int socket_write(void* pipecb_t, const char *buf, unsigned int n);
int socket_close(void* sockb);

static file_ops socket_file_ops = {
  .Open = NULL,
  .Read = socket_read,
  .Write = socket_write,
  .Close = socket_close
};

Fid_t sys_Socket(port_t port)
{
	if(port<0||port>MAX_PORT) return NOFILE;
	Fid_t fid;
	FCB *fcb;
	int success = FCB_reserve(1,&fid,&fcb);
	if(success==0) return NOFILE;
	socket_cb *socket = (socket_cb*)xmalloc(sizeof(socket_cb));
	fcb->streamfunc=&socket_file_ops;
	fcb->streamobj=socket;
	socket->fcb=fcb;
	socket->type=SOCKET_UNBOUND;
	socket->refcount=1;
	socket->port=port;
	return fid;
}

int sys_Listen(Fid_t sock)
{
	if(sock<0||sock>=MAX_FILEID) return -1;
	FCB* fcb_temp = get_fcb(sock);
	if(fcb_temp==NULL) return -1;
	socket_cb* socket = (socket_cb*) fcb_temp->streamobj;
	if(socket==NULL||socket->port==NOPORT||socket->type!=SOCKET_UNBOUND||(PORT_MAP[socket->port]!=NULL&&PORT_MAP[socket->port]->type==SOCKET_LISTENER)) return -1;
	socket->type=SOCKET_LISTENER;
	rlnode_init(&socket->listener_s.queue,NULL);
	socket->listener_s.req_available=COND_INIT;
	PORT_MAP[socket->port]=socket;
	return 0;
}


Fid_t sys_Accept(Fid_t lsock)
{
	if(lsock<0||lsock>=MAX_FILEID) return NOFILE;
	FCB* fcb_temp = get_fcb(lsock);
	if(fcb_temp==NULL) 
	  return NOFILE;
	socket_cb* listener = (socket_cb*) fcb_temp->streamobj;
	if(listener==NULL||listener->type!=SOCKET_LISTENER) 
	  return NOFILE;
	listener->refcount++;
	while(PORT_MAP[listener->port]!=NULL && is_rlist_empty(&listener->listener_s.queue)){
	  kernel_wait(&listener->listener_s.req_available,SCHED_IO);
	}
	listener->refcount--;
	if(PORT_MAP[listener->port]==NULL) 
	  return NOFILE;

	rlnode* request_node = rlist_pop_front(&listener->listener_s.queue);
	socket_cb *client = request_node->crq->peer;
	Fid_t fid = sys_Socket(client->port);
	if(fid==NOFILE) return NOFILE;
	socket_cb* socket = (socket_cb*)get_fcb(fid)->streamobj;
	if(socket==NULL) return NOFILE;
	client->type=SOCKET_PEER;
	socket->type=SOCKET_PEER;

	pipe_cb* pipe1 = (pipe_cb*)xmalloc(sizeof(pipe_cb));
	pipe1->has_space=COND_INIT;
	pipe1->has_data=COND_INIT;
	pipe1->w_position=0;
	pipe1->r_position=0;
	pipe1->used_space=0;
	pipe1->reader=get_fcb(fid);
	pipe1->writer=client->fcb;

	pipe_cb* pipe2 = (pipe_cb*)xmalloc(sizeof(pipe_cb));
	pipe2->has_space=COND_INIT;
	pipe2->has_data=COND_INIT;
	pipe2->w_position=0;
	pipe2->r_position=0;
	pipe2->used_space=0;
	pipe2->reader=client->fcb;
	pipe2->writer=get_fcb(fid);

	socket->peer_s.peer=client;
	socket->peer_s.write_pipe=pipe1;
	socket->peer_s.read_pipe=pipe2;
	client->peer_s.peer=socket;
	client->peer_s.write_pipe=pipe2;
	client->peer_s.read_pipe=pipe1;
	request_node->crq->admitted=1;
	kernel_signal(&request_node->crq->connected_cv);
	
	return fid;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	if(sock<0||sock>=MAX_FILEID) return -1;
	FCB* fcb_temp = get_fcb(sock);
	if(fcb_temp==NULL) return -1;
	socket_cb* socket = (socket_cb*) fcb_temp->streamobj;
	if(port<0||port>MAX_PORT||port==NOPORT) return -1;
	if(socket==NULL||PORT_MAP[port]==NULL||PORT_MAP[port]->type!=SOCKET_LISTENER) return -1;
	connection_request *request = (connection_request*)xmalloc(sizeof(connection_request));
	request->connected_cv=COND_INIT;
	request->admitted=0;
	request->peer=socket;
	rlnode_init(&request->queue_node,request);
	rlist_push_back(&PORT_MAP[port]->listener_s.queue,&request->queue_node);
	kernel_signal(&PORT_MAP[port]->listener_s.req_available);
	int success;
	socket->refcount++;
	while(request->admitted==0) {
	   success = kernel_timedwait(&request->connected_cv,SCHED_USER,timeout);
	   if (success == 0)
	     break;
	}
	socket->refcount--;
	int admitted = request->admitted;
	if(!admitted) rlist_remove(&request->queue_node);
	free(request);
	return admitted==1 ? 0 : -1 ;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{	
	if(sock<0||sock>=MAX_FILEID) return -1;
	FCB* fcb_temp = get_fcb(sock);
	if (fcb_temp==NULL) return -1;
	socket_cb* socket = (socket_cb*) fcb_temp->streamobj;
	if(socket==NULL) return -1;
	switch(how){
		case(SHUTDOWN_READ):
		pipe_reader_close(socket->peer_s.read_pipe);
		break;
		case(SHUTDOWN_WRITE):
		pipe_writer_close(socket->peer_s.write_pipe);
		break;
		case(SHUTDOWN_BOTH):
		pipe_reader_close(socket->peer_s.read_pipe);
		pipe_writer_close(socket->peer_s.write_pipe);
		break;
	}
	return 0;
}

int socket_read(void* socketcb_t, char *buf, unsigned int n){
	socket_cb* socket = (socket_cb*)socketcb_t;
  	if(socket==NULL||socket->type!=SOCKET_PEER) return -1;
	return pipe_read(socket->peer_s.read_pipe,buf,n);
}
int socket_write(void* socketcb_t, const char *buf, unsigned int n){
	socket_cb* socket = (socket_cb*)socketcb_t;
  	if(socket==NULL||socket->type!=SOCKET_PEER) return -1;
	return pipe_write(socket->peer_s.write_pipe,buf,n);
}
int socket_close(void* _sockcb){
	socket_cb* socket = (socket_cb*)_sockcb;
	if(socket==NULL) return -1;
	if(socket->type==SOCKET_PEER){
		if (socket->peer_s.read_pipe != NULL){
	      pipe_reader_close(socket->peer_s.read_pipe);
		  socket->peer_s.read_pipe = NULL;
		}
		if (socket->peer_s.write_pipe != NULL) {
	      pipe_writer_close(socket->peer_s.write_pipe); 
		  socket->peer_s.write_pipe = NULL;
		}
	}else if(socket->type==SOCKET_LISTENER){
		socket_cb* listener = PORT_MAP[socket->port];
		PORT_MAP[socket->port]=NULL;
		kernel_broadcast(&listener->listener_s.req_available);
	}
	socket->refcount--;
	if (socket->refcount == 0)
	  free(socket);
	return 0;
}