#ifndef CHAT_H
#define CHAT_H

#include "client.h"
#include "client_handler.h"
#include "debug_print.h"
#include <arpa/inet.h>
#include <hiredis/hiredis.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void *start_chat_server(void *port);

#endif // CHAT_H
