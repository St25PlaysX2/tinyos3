
#include "tinyos.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
//#include "kernel_pipes.c"

socket_cb* PORT_MAP[MAX_PORT];

int socket_read(){
	return -1;
}
int socket_write(){
	return -1;
}
int socket_close(){
	return -1;
}

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
	int success = FCB_reserve(1,&fid,&fcb);
	if(port<0||port>=MAX_PORT||success==0) return NOFILE;
	socket_cb *socket = (socket_cb*)xmalloc(sizeof(socket_cb));
	fcb->streamfunc=&socket_file_ops;
	fcb->streamobj=socket;
	socket->fcb=fcb;
	socket->type=SOCKET_UNBOUND;
	// rlnode_init(&socket->unbound_s.unbound_socket,socket);
	// if(port!=NOPORT&&PORT_MAP[port]!=NULL) {
	//   rlist_push_front(&PORT_MAP[port]->unbound_s->unbound_socket,&socket->unbound_s.unbound_socket);
	//   socket->port=port;
	// }else if(port!=NOPORT&&PORT_MAP[port]==NULL){
	//   PORT_MAP[port]=socket;
	//   socket->port=port;
	// }else socket->port=NOPORT;
	socket->port=port;
	return fid;
}

int sys_Listen(Fid_t sock)
{
	if(sock<0||sock>=MAX_FILEID) return -1;
	socket_cb* socket = (socket_cb*) get_fcb(sock)->streamobj;
	if(socket==NULL||socket->port==NOPORT||socket->type==SOCKET_LISTENER||PORT_MAP[socket->port]->type==SOCKET_LISTENER) return -1;
	socket->type=SOCKET_LISTENER;
	// rlnode temp;
	// rlnode_init(&temp,null);
	// rlnode_swap(&temp,&socket->unbound_s.unbound_socket);
	// rlnode_swap(&socket->listener_s.queue,&temp);
	// if(PORT_MAP[socket->port]!=socket) rlist_push_front(&PORT_MAP[socket->port]->unbound_s.unbound_socket,&socket->listener_s.queue);
	rlnode_init(&socket->listener_s.queue,NULL);
	PORT_MAP[socket->port]=socket;
	return 0;
}


Fid_t sys_Accept(Fid_t lsock)
{
	if(lsock<0||lsock>=MAX_FILEID) return NOFILE;
	socket_cb* socket = (socket_cb*) get_fcb(lsock)->streamobj;
	socket_cb* listener = PORT_MAP[socket->port];
	if(socket==NULL||listener==NULL||listener!=SOCKET_LISTENER) return NOFILE;
	socket->refcount++;
	while(socket!=NULL && is_rlist_empty(&listener->listener_s.queue)){
	  kernel_wait(&listener->listener_s.req_available,SCHED_IO);
	}
	socket->refcount--;
	rlnode* request_node = rlist_pop_front(&listener->listener_s.queue);
	socket_cb *client = request_node->crq->peer;
	if(socket==NULL) return NOFILE;
	pipe_t* pipe1;
	assert(Pipe(pipe1)==0);
	pipe_t* pipe2;
	assert(Pipe(pipe2)==0);
	socket->peer_s.peer=client;
	socket->peer_s.write_pipe=(pipe_cb*)get_fcb(pipe1->write)->streamobj;
	socket->peer_s.read_pipe=(pipe_cb*)get_fcb(pipe2->read)->streamobj;
	client->peer_s.peer=socket;
	client->peer_s.write_pipe=(pipe_cb*)get_fcb(pipe2->write)->streamobj;
	client->peer_s.read_pipe=(pipe_cb*)get_fcb(pipe1->read)->streamobj;
	request_node->crq->admitted=1;
	kernel_signal(&request_node->crq->connected_cv);
	//return client->;
	return NOFILE;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	if(sock<0||sock>=MAX_FILEID) return -1;
	socket_cb* socket = (socket_cb*) get_fcb(sock)->streamobj;
	if(socket==NULL||PORT_MAP[port]->type!=SOCKET_LISTENER) return -1;
	connection_request *request = (connection_request*)xmalloc(sizeof(connection_request));
	request->connected_cv=COND_INIT;
	request->admitted=0;
	request->peer=socket;
	rlnode_init(&request->queue_node,request);
	rlist_push_back(&PORT_MAP[port]->listener_s.queue,&request->queue_node);
	kernel_signal(&PORT_MAP[port]->listener_s.req_available);
	int success=-3;
	socket->refcount++;
	while(request->admitted==0&&success==-3) success = kernel_timedwait(&request->connected_cv,SCHED_USER,timeout);
	socket->refcount--;
	return (success==0);
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{	
	// if(sock<0||sock>=MAX_FILEID) return -1;
	// socket_cb* socket = (socket_cb*) get_fcb(sock)->streamobj;
	// if(socket==NULL) return -1;
	// switch(how){
	// 	case(SHUTDOWN_READ):
	// 	pipe_reader_close(socket->peer_s.read_pipe);
	// 	break;
	// 	case(SHUTDOWN_WRITE):
	// 	pipe_writer_close(socket->peer_s.write_pipe);
	// 	break;
	// 	case(SHUTDOWN_BOTH):
	// 	pipe_reader_close(socket->peer_s.read_pipe);
	// 	pipe_writer_close(socket->peer_s.write_pipe);
	// 	break;
	// }
	return -1;
}

