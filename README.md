Overview
========

`ngx_txserial` is a module that exposes a set of variables that APNIC Labs
uses to drive 1x1 experiments


txodd is an odd number in the sequence txstart .. txend
txeven is the next number in the same sequence. 

They increment by txincr which should usually be left alone as 2.

As long as a single process group runs nginx, this sequence is
handed out in sequence, and loops round every txend calls.

txweek is a week number derived from the start of the experiment,
and is used to prevent replay of data. It increments in units of
86400 * 7 counts from txbmon, as an integer value added to txbase.

txsec is the seconds since 1970. It has proved useful as a value,
alongside msec which is the same with microseconds after a point.

Build
=====

  * Configure and nginx with `--add-module=path/to/ngx_txserial`

Example
=======

    server {
        listen       80;
        server_name  example.com;
        access_log   logs/example.com/conns.log conn;
        access_log   logs/example.com/agents.log agent;

        txstart 1; 		# default 1
        txend   249999;		# default 499999
        txincr  2;		# default 2

	txbase  4000;		# default 5000
	txbmon  1379894400;	# time in seconds since 1970 to calculate week count from.

        location / {
		echo $txodd;
		echo $txeven;
		echo $txweek;
		echo $txsec;
        }
    }
```

Background
==========

The code was copied from txid and then heavily modified.

