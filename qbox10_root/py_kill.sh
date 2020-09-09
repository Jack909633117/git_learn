#!/bin/sh

pid1=`ps | grep -v grep | grep 'py_websocket' | awk '{print $1}'`
pid2=`ps | grep -v grep | grep 'py_http' | awk '{print $1}'`
kill $pid1 $pid2

