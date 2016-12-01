/*
 *  mms_client_example.c
 *
 *  This is the most simple example. It illustrates how to create an MmsConnection
 *  object and connect to a MMS server.
 *
 *  Copyright 2013 Michael Zillgith
 *
 *	This file is part of libIEC61850.
 *
 *	libIEC61850 is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	libIEC61850 is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with libIEC61850.  If not, see <http://www.gnu.org/licenses/>.
 *
 *	See COPYING file for the complete license text.
 */

#include <stdlib.h>
#include <stdio.h>
#include "mms_client_connection.h"
#include "hal_thread.h"

int main(int argc, char** argv) {

	MmsConnection con = MmsConnection_create();

	MmsError mmsError;

	if (MmsConnection_connect(con, &mmsError, "localhost", 102)) {
		// add application code here

        Thread_sleep(1000);

	    printf("Send abort\n");
	    MmsConnection_abort(con, &mmsError);
	}
	else
	    printf("Connect to server failed!\n");

	MmsConnection_destroy(con);
}

