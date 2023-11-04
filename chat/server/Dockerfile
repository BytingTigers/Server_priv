####################
# Base
####################

FROM ubuntu:16.04 AS base

RUN apt-get update
RUN apt-get install -y gcc gcc-multilib libc6 libc6-dev net-tools make
RUN apt-get install -y libhiredis-dev libmysqlclient-dev
RUN apt-get install -y redis-server
RUN apt-get clean && rm -rf /var/lib/apt/lists/*

####################
# Build
####################

FROM base AS build

COPY ./Makefile /Makefile
COPY ./src /src
RUN make CFLAGS='-z execstack -fno-stack-protector -z norelro -g -O0'

####################
# Execution
####################

FROM build AS exec

EXPOSE 5000
CMD sh -c "redis-server --daemonize yes; exec ./server 5000"
