#pragma once

#ifndef HEADER_CONTROLLERS_SOFTWARE_STARTROUTEREQUESTHANDLER
#define HEADER_CONTROLLERS_SOFTWARE_STARTROUTEREQUESTHANDLER

#include <CivetServer.h>

int startRouteRequestHandler(struct mg_connection* conn, void* cbdata);

#endif
