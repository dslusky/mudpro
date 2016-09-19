FROM ubuntu:14.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update
RUN apt-get install -y libpcre3
RUN apt-get install -y libglib2.0-0
RUN cd /lib/x86_64-linux-gnu && ln -s libpcre.so.3 libpcre.so.0 
RUN ldconfig
RUN mkdir /mudpro

WORKDIR /mudpro

ADD mudpro /mudpro/

ENTRYPOINT ["./mudpro"]
