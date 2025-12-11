
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_proc.h"
#include "kernel_cc.h"

int pipe_write(void* pipecb_t, const char *buf, unsigned int n);
int pipe_read(void* pipecb_t, char *buf, unsigned int n);
int pipe_writer_close(void* _pipecb);
int pipe_reader_close(void* _pipecb);

static file_ops file_write_ops = {
  .Open = NULL,
  .Read = NULL,
  .Write = pipe_write,
  .Close = pipe_writer_close
};

static file_ops file_read_ops = {
  .Open = NULL,
  .Read = pipe_read,
  .Write = NULL,
  .Close = pipe_reader_close
};

int sys_Pipe(pipe_t* pipe)
{
	pipe_cb* new_pipe = (pipe_cb*)xmalloc(sizeof(pipe_cb));
	new_pipe->has_space=COND_INIT;
	new_pipe->has_data=COND_INIT;
	new_pipe->w_position=0;
	new_pipe->r_position=0;
  new_pipe->used_space=0;
  Fid_t fid[2];
  FCB* fcb[2];
	if(FCB_reserve(2,fid,fcb)==0) return -1;
  new_pipe->writer=fcb[0];
  new_pipe->reader=fcb[1];
  fcb[0]->streamobj=new_pipe;
  fcb[1]->streamobj=new_pipe;
  fcb[0]->streamfunc=&file_write_ops;
  fcb[1]->streamfunc=&file_read_ops;
  pipe->write=fid[0];
  pipe->read=fid[1];
	return 0;
}


int pipe_write(void* pipecb_t, const char *buf, unsigned int n){

  pipe_cb* pipe = (pipe_cb*)pipecb_t;

  if(pipe==NULL||pipe->writer==NULL) return -1;
  if(pipe->reader==NULL) return -1;
  while(pipe->used_space==PIPE_BUFFER_SIZE && pipe->reader!=NULL) kernel_wait(&pipe->has_space, SCHED_IO);

  unsigned int count = 0;
  while(count<n) {
    
    if(pipe->used_space==PIPE_BUFFER_SIZE) break;
    pipe->BUFFER[pipe->w_position]=buf[count];
    count++;
    pipe->used_space++;
    if(pipe->w_position==PIPE_BUFFER_SIZE-1)pipe->w_position=0;
    else pipe->w_position++;

  }
  kernel_broadcast(&pipe->has_data);

  return count;
}

int pipe_read(void* pipecb_t, char *buf, unsigned int n){

  pipe_cb* pipe = (pipe_cb*)pipecb_t;

  if(pipe==NULL||pipe->reader==NULL) return -1;
  while(pipe->used_space==0 && pipe->writer!=NULL) kernel_wait(&pipe->has_data, SCHED_IO);

  unsigned int count = 0;
  while(count<n) {

    if(pipe->used_space==0) break;
    buf[count]=pipe->BUFFER[pipe->r_position];
    count++;
    pipe->used_space--;
    if(pipe->r_position==PIPE_BUFFER_SIZE-1)pipe->r_position=0;
    else pipe->r_position++;
    
  }
  kernel_broadcast(&pipe->has_space);

  return count;
}

int pipe_writer_close(void* _pipecb){

  pipe_cb* pipe = (pipe_cb*) _pipecb;
  pipe->writer = NULL;
  if(pipe->reader==NULL && pipe->writer==NULL) free(pipe);
	return 0;
}
int pipe_reader_close(void* _pipecb){

  pipe_cb* pipe = (pipe_cb*) _pipecb;
  pipe->reader = NULL;
  if(pipe->reader==NULL && pipe->writer==NULL) free(pipe);
	return 0;
}