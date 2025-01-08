struct my_msgbuf {
	long mtype;

	// pid will sleep for io_time
	int pid;
	int io_time;
};
