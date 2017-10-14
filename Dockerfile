# usage:
# docker run -ti --log-driver=none --name gotik \
#   -v /storage/mudpro-profiles:/mudpro/profile \
#   mudpro:0.93a -f profile/local/gotik/character.conf

FROM ubuntu:16.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y libpcre3 libglib2.0-0 libpopt0
RUN cd /lib/x86_64-linux-gnu && ln -s libpcre.so.3 libpcre.so.0 && ldconfig
RUN mkdir /mudpro
COPY mudpro /mudpro/

WORKDIR /mudpro
ENTRYPOINT ["./mudpro"]
