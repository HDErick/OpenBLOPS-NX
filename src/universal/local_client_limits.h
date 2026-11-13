#pragma once

// Normal builds have one local client. Experimental PC/Switch split-screen
// builds reserve two slots for the host and the separately rendered guest.
#ifndef MAX_LOCAL_CLIENTS
#define MAX_LOCAL_CLIENTS 1
#endif
