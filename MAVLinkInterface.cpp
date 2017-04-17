// Prevent Visual Studio Intellisense from defining _WIN32 and _MSC_VER when we use 
// Visual Studio to edit Linux or Borland C++ code.
#ifdef __linux__
#	undef _WIN32
#endif // __linux__
#if defined(__GNUC__) || defined(__BORLANDC__)
#	undef _MSC_VER
#endif // defined(__GNUC__) || defined(__BORLANDC__)

#include "MAVLinkDevice.h"
#include "MAVLinkInterface.h"

#define LOCAL_TYPE_MAVLINKINTERFACE 0
#define REMOTE_TYPE_MAVLINKINTERFACE 1

RS232PORT MAVLinkInterfacePseudoRS232Port;

int connectmavlinkinterface()
{
	if (OpenRS232Port(&MAVLinkInterfacePseudoRS232Port, szMAVLinkInterfacePath) == EXIT_SUCCESS) 
	{
		printf("Unable to connect to a MAVLinkInterface.\n");
		return EXIT_FAILURE;
	}

	if (SetOptionsRS232Port(&MAVLinkInterfacePseudoRS232Port, MAVLinkInterfaceBaudRate, NOPARITY, FALSE, 8, 
		ONESTOPBIT, (UINT)MAVLinkInterfaceTimeout) != EXIT_SUCCESS)
	{
		printf("Unable to connect to a MAVLinkInterface.\n");
		CloseRS232Port(&MAVLinkInterfacePseudoRS232Port);
		return EXIT_FAILURE;
	}

	printf("MAVLinkInterface connected.\n");

	return EXIT_SUCCESS;
}

int disconnectmavlinkinterface()
{
	if (CloseRS232Port(&MAVLinkInterfacePseudoRS232Port) == EXIT_SUCCESS) 
	{
		printf("MAVLinkInterface disconnection failed.\n");
		return EXIT_FAILURE;
	}

	printf("MAVLinkInterface disconnected.\n");

	return EXIT_SUCCESS;
}

//recvlatestdatamavlinkinterface()

//sendlatestdatamavlinkinterface()

int handlemavlinkinterface()
{
	int sendbuflen = 0;
	uint8 sendbuf[MAX_NB_BYTES_MAVLINKDEVICE];

	mavlink_message_t msg;
	mavlink_heartbeat_t heartbeat;
	mavlink_gps_raw_int_t gps_raw_int;
	mavlink_attitude_t attitude;
	double lathat = 0, longhat = 0, althat = 0, headinghat = 0;

	EnterCriticalSection(&StateVariablesCS);

	EnvCoordSystem2GPS(lat_env, long_env, alt_env, angle_env, Center(xhat), Center(yhat), Center(zhat), &lathat, &longhat, &althat);
	headinghat = (fmod_2PI(-angle_env-Center(thetahat)+3.0*M_PI/2.0)+M_PI)*180.0/M_PI;

	memset(&heartbeat, 0, sizeof(mavlink_heartbeat_t));
	heartbeat.autopilot = MAV_AUTOPILOT_INVALID;
	heartbeat.base_mode = MAV_MODE_FLAG_SAFETY_ARMED;
	heartbeat.system_status = MAV_STATE_ACTIVE;
	heartbeat.type = MAV_TYPE_GENERIC;
	if (robid & SUBMARINE_ROBID_MASK) heartbeat.type = MAV_TYPE_SUBMARINE;
	if (robid & MOTORBOAT_ROBID_MASK) heartbeat.type = MAV_TYPE_SURFACE_BOAT;
	if (robid & SAILBOAT_ROBID_MASK) heartbeat.type = MAV_TYPE_SURFACE_BOAT;
	if (robid & GROUND_ROBID_MASK) heartbeat.type = MAV_TYPE_GROUND_ROVER;
	switch (robid)
	{
	case QUADRO_ROBID:
		heartbeat.type = MAV_TYPE_QUADROTOR;
		break;
	default:
		break;
	}

	memset(&gps_raw_int, 0, sizeof(mavlink_gps_raw_int_t));
	if (CheckGPSOK())
	{
		gps_raw_int.fix_type = 2;
		gps_raw_int.vel = (uint16_t)(sog*100);
		gps_raw_int.cog = (uint16_t)(fmod_360_pos((-angle_env-cog+M_PI/2.0)*180.0/M_PI)*100);
	}
	else 
	{
		gps_raw_int.fix_type = 0;
		gps_raw_int.vel = UINT16_MAX;
		gps_raw_int.cog = UINT16_MAX;
	}
	gps_raw_int.lat = (int32_t)(lathat*10000000.0);
	gps_raw_int.lon = (int32_t)(longhat*10000000.0);
	gps_raw_int.alt = (int32_t)(althat*1000.0);
	gps_raw_int.eph = UINT16_MAX;
	gps_raw_int.epv = UINT16_MAX;
	gps_raw_int.satellites_visible = 255;

	memset(&attitude, 0, sizeof(mavlink_attitude_t));
	attitude.yaw = (float)fmod_2PI(-angle_env-Center(thetahat)+M_PI/2.0);
	attitude.pitch = (float)pitch;
	attitude.roll = (float)roll;

	LeaveCriticalSection(&StateVariablesCS);

	mavlink_msg_heartbeat_encode((uint8_t)MAVLinkInterface_system_id, (uint8_t)MAVLinkInterface_component_id, &msg, &heartbeat);
	memset(sendbuf, 0, sizeof(sendbuf));
	sendbuflen = mavlink_msg_to_send_buffer((uint8_t*)sendbuf, &msg);
	if (WriteAllRS232Port(&MAVLinkInterfacePseudoRS232Port, sendbuf, sendbuflen) != EXIT_SUCCESS)
	{
		return EXIT_FAILURE;
	}

	mavlink_msg_gps_raw_int_encode((uint8_t)MAVLinkInterface_system_id, (uint8_t)MAVLinkInterface_component_id, &msg, &gps_raw_int);
	memset(sendbuf, 0, sizeof(sendbuf));
	sendbuflen = mavlink_msg_to_send_buffer((uint8_t*)sendbuf, &msg);
	if (WriteAllRS232Port(&MAVLinkInterfacePseudoRS232Port, sendbuf, sendbuflen) != EXIT_SUCCESS)
	{
		return EXIT_FAILURE;
	}

	mavlink_msg_attitude_encode((uint8_t)MAVLinkInterface_system_id, (uint8_t)MAVLinkInterface_component_id, &msg, &attitude);
	memset(sendbuf, 0, sizeof(sendbuf));
	sendbuflen = mavlink_msg_to_send_buffer((uint8_t*)sendbuf, &msg);
	if (WriteAllRS232Port(&MAVLinkInterfacePseudoRS232Port, sendbuf, sendbuflen) != EXIT_SUCCESS)
	{
		return EXIT_FAILURE;
	}

	mSleep(50);

	return EXIT_SUCCESS;
}

int handlemavlinkinterfacecli(SOCKET sockcli, void* pParam)
{
	/*
	// Should send a full image when connecting for method 0 and 1...
	//BOOL bForceSendFullImg = TRUE; 
	BOOL bInitDone = FALSE;
	char httpbuf[2048];
	*/
	int timeout = 5;

	UNREFERENCED_PARAMETER(pParam);

	if (MAVLinkInterfacePseudoRS232Port.DevType == TCP_SERVER_TYPE_RS232PORT) MAVLinkInterfacePseudoRS232Port.s = sockcli;

	for (;;)
	{
		fd_set sock_set;
		int iResult = SOCKET_ERROR;
		struct timeval tv;

		if (bExit) break;

		tv.tv_sec = (long)(timeout/1000);
		tv.tv_usec = (long)((timeout%1000)*1000);

		// Initialize a fd_set and add the socket to it.
		FD_ZERO(&sock_set); 
		FD_SET(sockcli, &sock_set);

		iResult = select((int)sockcli+1, NULL, &sock_set, NULL, &tv);

		// Remove the socket from the set.
		// No need to use FD_ISSET() here, as we only have one socket the return value of select() is 
		// sufficient to know what happened.
		FD_CLR(sockcli, &sock_set); 

		switch (iResult)
		{
		case SOCKET_ERROR:
			return EXIT_FAILURE;
		case 0:
			// The timeout on select() occured.
			break;
		default:
			{
				/*				// Receive the GET request, but do not analyze it...
				tv.tv_sec = (long)(timeout/1000);
				tv.tv_usec = (long)((timeout%1000)*1000);
				if (waitforsocket(sockcli, tv) == EXIT_SUCCESS)
				{
				memset(httpbuf, 0, sizeof(httpbuf));
				if (recv(sockcli, httpbuf, sizeof(httpbuf), 0) <= 0)
				{
				printf("recv() failed.\n");
				return EXIT_FAILURE;
				}
				memset(httpbuf, 0, sizeof(httpbuf));
				sprintf(httpbuf, 
				"HTTP/1.1 200 OK\r\n"
				"Server: RemoteWebcamMultiSrv\r\n"
				//"Connection: close\r\n"
				//"Max-Age: 0\r\n"
				//"Expires: 0\r\n"
				//"Cache-Control: no-cache, private, no-store, must-revalidate, pre-check = 0, post-check = 0, max-age = 0\r\n"
				//"Pragma: no-cache\r\n"
				"Content-Type: multipart/x-mixed-replace; boundary=--boundary\r\n"
				//"Media-type: image/jpeg\r\n"
				"\r\n");
				if (sendall(sockcli, httpbuf, strlen(httpbuf)) != EXIT_SUCCESS)
				{
				return EXIT_FAILURE;
				}
				}
				*/			
			}
/*
			// Should read the data to try to get RTCM messages...
			unsigned char rtcmdata[2048];


			if (ReceivedBytes > 0) 
			{
				EnterCriticalSection(&StateVariablesCS);
				for (int k = 0; k < ReceivedBytes; k++)
				{
					unsigned char rtcmbyte = rtcmdata[k];
					for (unsigned int j = 0; j < RTCMuserslist.size(); j++)
					{
						RTCMuserslist[j].push_back(rtcmbyte);
						if (RTCMuserslist[j].size() > MAX_NB_BYTES_RTCM_PARTS) RTCMuserslist[j].pop_front();
					}
				}
				LeaveCriticalSection(&StateVariablesCS);
			}
*/

			if (handlemavlinkinterface() != EXIT_SUCCESS)
			{
				return EXIT_FAILURE;
			}
			break;
		}

		if (bExit) break;
	}

	return EXIT_SUCCESS;
}

THREAD_PROC_RETURN_VALUE MAVLinkInterfaceThread(void* pParam)
{
	//CHRONO chrono;
	//double dt = 0, t = 0, t0 = 0;
	//struct timeval tv;

	UNREFERENCED_PARAMETER(pParam);

	// Try to determine whether it is a server TCP port.
	if ((szMAVLinkInterfacePath[0] == ':')&&(atoi(szMAVLinkInterfacePath+1) > 0))
	{
		MAVLinkInterfacePseudoRS232Port.DevType = TCP_SERVER_TYPE_RS232PORT;
		while (LaunchMultiCliTCPSrv(szMAVLinkInterfacePath+1, handlemavlinkinterfacecli, NULL) != EXIT_SUCCESS)
		{
			printf("Error launching the MAVLinkInterface server.\n");
			mSleep(4000);
			if (bExit) break;
		}
	}
	else
	{
/*
		for (;;)
		{
			if (connectmavlinkinterface() == EXIT_SUCCESS) 
			{
				mSleep(50);
				for (;;)
				{
					if (handlemavlinkinterface() != EXIT_SUCCESS)
					{
						printf("Connection to a MAVLinkInterface lost.\n");
						break;
					}
					if (bExit) break;
				}
				disconnectmavlinkinterface();
				mSleep(50);
			}
			else
			{
				mSleep(1000);
			}
			if (bExit) break;
		}
*/

		BOOL bConnected = FALSE;

		//t = 0;

		//StartChrono(&chrono);

		for (;;)
		{
			//mSleep(50);
			//t0 = t;
			//GetTimeElapsedChrono(&chrono, &t);
			//dt = t-t0;

			//printf("MAVLinkInterfaceThread period : %f s.\n", dt);

			if (!bConnected)
			{
				if (connectmavlinkinterface() == EXIT_SUCCESS) 
				{
					mSleep(50);
					bConnected = TRUE; 
				}
				else 
				{
					bConnected = FALSE;
					mSleep(1000);
				}
			}
			else
			{
				//// Time...
				//if (gettimeofday(&tv, NULL) != EXIT_SUCCESS)
				//{
				//	tv.tv_sec = 0;
				//	tv.tv_usec = 0;
				//}

				if (handlemavlinkinterface() != EXIT_SUCCESS)
				{
					printf("Connection to a MAVLinkInterface lost.\n");
					bConnected = FALSE;
					disconnectmavlinkinterface();
					mSleep(50);
				}
			}

			if (bExit) break;
		}

		//StopChrono(&chrono, &t);

		if (bConnected) disconnectmavlinkinterface();
	}

	if (!bExit) bExit = TRUE; // Unexpected program exit...

	return 0;
}
