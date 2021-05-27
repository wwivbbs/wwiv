#ifndef PIPE_H
#define PIPE_H

#define PIPE_BUFFER_SIZE 4000


int DosPeekNmPipe(int handle);

class __far Pipe {
 public:
  /** 
   * Constructs a set of pipes for FOSSIL communication. 
   * There will be 2 pipes, the one pointed by FN will be a bi-direcitonal
   * pipe for data, and a second named fn + "C" will be created for control
   * information.
   *
   * N.B: The pipe is expected to be created before installing a TSR
   * or interrupt handler.
   */
  Pipe(const char* fn, int timeout_secs);
  /** Destroys the pipe, closing if needed */
  ~Pipe();

  /** 
   * A non-blocking read of 1 char.  A value if 0 or -1 is returned
   * if there is nothing to read.
   */
  int read();

  /**
   * A blocking version of read.
   */
  int blocking_read();

  /**
   * Writes a single char to the pipe.  Returning the number of characters
   * written or -1 on error.
   */
  int write(int ch);

  /**
   * Writes a block of chars to the pipe.  Returning the number of characters
   * written or -1 on error.
   */
  int write(const char __far * buf, int maxlen);

  /** Send a control code to the remote side */
  int send_control(char code);

  /** Gets the current control code */
  char control_code();

  /** 
   * A non-blocking peek of 1 char.  A value if 0 or -1 is returned
   * if there is nothing to read.
   */
  int peek();

  /**
   * returns true (1) if the pipe is open.
   */
  int is_open();

  /**
   * Closes this end of the pipe.
   */
  void close();

  /**
   * Returns the native DOS handle for the pipe.  
   * May be needed to pass to interrupt or _dos_xxxx calls
   */
  int handle() { return handle_; }

  /**
   * Returns the total number of write calls.
   */
  int num_writes() { return num_writes_; }

  /**
   * Returns the total number of write calls that ended in an error.
   */
  int num_errors() { return num_errors_; }
 
  /**
   * Returns the bytes written.
   */
  long bytes_written() { return bytes_written_; }

  /**
   * Returns the bytes written.
   */
  long bytes_read() { return bytes_read_; }

 private:
  int handle_;
  int control_handle_;
  int num_writes_;
  int num_errors_;
  long bytes_written_;
  long bytes_read_;
  int next_char_;
};

#endif // PIPE_H


