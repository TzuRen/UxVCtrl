// Prevent Visual Studio Intellisense from defining _WIN32 and _MSC_VER when we use 
// Visual Studio to edit Linux or Borland C++ code.
#ifdef __linux__
#	undef _WIN32
#endif // __linux__
#if defined(__GNUC__) || defined(__BORLANDC__)
#	undef _MSC_VER
#endif // defined(__GNUC__) || defined(__BORLANDC__)

#include "Config.h"
#include "MDM.h"

inline int WaitNsMDM(int n)
{
	int i = n;

	if (bExit||!AcousticCommandMDM) return EXIT_FAILURE;
	while (i)
	{
		mSleep(1000);
		if (bExit||!AcousticCommandMDM) return EXIT_FAILURE;
		i--;
	}

	return EXIT_SUCCESS;
}

inline int PurgeDataAndWaitNsMDM(MDM* pMDM, BOOL* pbError, int n)
{
	if (PurgeDataMDM(pMDM) != EXIT_SUCCESS)
	{
		*pbError = TRUE;
		return EXIT_FAILURE;
	}
	if (WaitNsMDM(n) != EXIT_SUCCESS) return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

THREAD_PROC_RETURN_VALUE MDMThread(void* pParam)
{
	MDM mdm;
	struct timeval tv;
	//int buflen = 0;
	int receivedbytes = 0;
	uint8 buf[256];
	u2sShortMSG_MDM sendxy;
	u2sShortMSG_MDM recvxy;
	u2sShortMSG_MDM recvxy1;
	u2sShortMSG_MDM recvxy2;
	//u2sShortMSG_MDM recvxy3;
	//uInt8_MDM bits;
	//int msgcounter = 0;
	BOOL bError = FALSE;
	BOOL bConnected = FALSE;
	int i = 0;
	char szSaveFilePath[256];
	char szTemp[256];

	UNREFERENCED_PARAMETER(pParam);

	memset(&mdm, 0, sizeof(MDM));

	memset(buf, 0, sizeof(buf));
	memset(&sendxy, 0, sizeof(sendxy));
	memset(&recvxy, 0, sizeof(recvxy));
	memset(&recvxy1, 0, sizeof(recvxy1));
	memset(&recvxy2, 0, sizeof(recvxy2));
	//memset(&recvxy3, 0, sizeof(recvxy3));

	for (;;)
	{
		mSleep(100);

		if (!bConnected)
		{
			if (ConnectMDM(&mdm, "MDM0.txt") == EXIT_SUCCESS) 
			{
				bConnected = TRUE; 

				if (mdm.pfSaveFile != NULL)
				{
					fclose(mdm.pfSaveFile); 
					mdm.pfSaveFile = NULL;
				}
				if ((mdm.bSaveRawData)&&(mdm.pfSaveFile == NULL)) 
				{
					if (strlen(mdm.szCfgFilePath) > 0)
					{
						sprintf(szTemp, "%.127s", mdm.szCfgFilePath);
					}
					else
					{
						sprintf(szTemp, "mdm");
					}
					// Remove the extension.
					for (i = strlen(szTemp)-1; i >= 0; i--) { if (szTemp[i] == '.') break; }
					if ((i > 0)&&(i < (int)strlen(szTemp))) memset(szTemp+i, 0, strlen(szTemp)-i);
					//if (strlen(szTemp) > 4) memset(szTemp+strlen(szTemp)-4, 0, 4);
					EnterCriticalSection(&strtimeCS);
					sprintf(szSaveFilePath, LOG_FOLDER"%.127s_%.64s.csv", szTemp, strtime_fns());
					LeaveCriticalSection(&strtimeCS);
					mdm.pfSaveFile = fopen(szSaveFilePath, "w");
					if (mdm.pfSaveFile == NULL) 
					{
						printf("Unable to create MDM data file.\n");
						break;
					}
					fprintf(mdm.pfSaveFile, "tv_sec;tv_usec;name;sent/received;data;\n"); 
					fflush(mdm.pfSaveFile);
				}
			}
			else 
			{
				bConnected = FALSE;
				mSleep(1000);
			}
		}
		else
		{
			bError = FALSE;

			EnterCriticalSection(&MDMCS);
			switch (AcousticCommandMDM)
			{
			case RNG_MSG :
				LeaveCriticalSection(&MDMCS);
				memset(buf, 0, sizeof(buf));
				strcpy((char*)buf, "rng\n");
				for (i = 0; i < 1; i++)
				{
					if (SendAllDataMDM(&mdm, buf, strlen((char*)buf)) != EXIT_SUCCESS)
					{
						bError = TRUE;
						break;
					}
					//printf("SEND : %.4s\n", buf);
					// Time...
					if (gettimeofday(&tv, NULL) != EXIT_SUCCESS)
					{
						tv.tv_sec = 0;
						tv.tv_usec = 0;
					}
					if (mdm.bSaveRawData)
					{
						fprintf(mdm.pfSaveFile, "%d;%d;%d;%c;%.4s;\n", (int)tv.tv_sec, (int)tv.tv_usec, RNG_MSG, 's', buf);
						fflush(mdm.pfSaveFile);
					}
					if (bExit||!AcousticCommandMDM) break;
					mSleep(100);
					if (PurgeDataAndWaitNsMDM(&mdm, &bError, 3) != EXIT_SUCCESS) break;
					receivedbytes = 0;
					memset(buf, 0, sizeof(buf));
					if (RecvDataMDM(&mdm, buf, sizeof(buf)-1, &receivedbytes) != EXIT_SUCCESS)
					{
						bError = TRUE;
						break;
					}
					//printf("RECV : %.255s", buf);
					// Time...
					if (gettimeofday(&tv, NULL) != EXIT_SUCCESS)
					{
						tv.tv_sec = 0;
						tv.tv_usec = 0;
					}
					if (mdm.bSaveRawData)
					{
						fprintf(mdm.pfSaveFile, "%d;%d;%d;%c;%.255s;\n", (int)tv.tv_sec, (int)tv.tv_usec, RNG_MSG, 'r', buf);
						fflush(mdm.pfSaveFile);
					}
					EnterCriticalSection(&StateVariablesCS);
					acousticmodem_r = 0;
					if (sscanf((char*)buf, "\n\rRange=%lfm", &acousticmodem_r) != 1)
					{
						acousticmodem_r = 0;
					}
					LeaveCriticalSection(&StateVariablesCS);
					if (WaitNsMDM(8) != EXIT_SUCCESS) break;
				}
				break;
			case SENDXY_MSG :
				LeaveCriticalSection(&MDMCS);
				memset(&sendxy, 0, sizeof(sendxy));
				sendxy.s[0] = (short)Center(xhat);
				sendxy.s[1] = (short)Center(yhat);
				for (i = 0; i < 2; i++)
				{
					if (SendAllDataMDM(&mdm, (uint8*)&sendxy, sizeof(sendxy)) != EXIT_SUCCESS)
					{
						bError = TRUE;
						break;
					}
					//printf("SEND : 0x%.2x%.2x%.2x%.2x (%d,%d)\n", 
					//	(unsigned int)sendxy.uc[3], (unsigned int)sendxy.uc[2], (unsigned int)sendxy.uc[1], (unsigned int)sendxy.uc[0], 
					//	(int)sendxy.s[0], (int)sendxy.s[1]);
					// Time...
					if (gettimeofday(&tv, NULL) != EXIT_SUCCESS)
					{
						tv.tv_sec = 0;
						tv.tv_usec = 0;
					}
					if (mdm.bSaveRawData)
					{
						fprintf(mdm.pfSaveFile, "%d;%d;%d;%c;0x%.2x%.2x%.2x%.2x (%d,%d);\n", (int)tv.tv_sec, (int)tv.tv_usec, SENDXY_MSG, 's', 
							(unsigned int)sendxy.uc[3], (unsigned int)sendxy.uc[2], (unsigned int)sendxy.uc[1], (unsigned int)sendxy.uc[0], 
							(int)sendxy.s[0], (int)sendxy.s[1]);
						fflush(mdm.pfSaveFile);
					}
					if (WaitNsMDM(8) != EXIT_SUCCESS) break;
				}
				break;
			case RECVXY_MSG :
				LeaveCriticalSection(&MDMCS);
				if (PurgeDataAndWaitNsMDM(&mdm, &bError, 2) != EXIT_SUCCESS) break;
				memset(&recvxy, 0, sizeof(recvxy));
				if (RecvAllDataMDM(&mdm, (uint8*)&recvxy, sizeof(recvxy)) != EXIT_SUCCESS)
				{
					//bError = TRUE;
					break;
				}
				//printf("RECV : 0x%.2x%.2x%.2x%.2x (%d,%d)\n", 
				//	(unsigned int)recvxy.uc[3], (unsigned int)recvxy.uc[2], (unsigned int)recvxy.uc[1], (unsigned int)recvxy.uc[0], 
				//	(int)recvxy.s[0], (int)recvxy.s[1]);
				// Time...
				if (gettimeofday(&tv, NULL) != EXIT_SUCCESS)
				{
					tv.tv_sec = 0;
					tv.tv_usec = 0;
				}
				if (mdm.bSaveRawData)
				{
					fprintf(mdm.pfSaveFile, "%d;%d;%d;%c;0x%.2x%.2x%.2x%.2x (%d,%d);\n", (int)tv.tv_sec, (int)tv.tv_usec, RECVXY_MSG, 'r', 
						(unsigned int)recvxy.uc[3], (unsigned int)recvxy.uc[2], (unsigned int)recvxy.uc[1], (unsigned int)recvxy.uc[0], 
						(int)recvxy.s[0], (int)recvxy.s[1]);
					fflush(mdm.pfSaveFile);
				}
				EnterCriticalSection(&StateVariablesCS);
				//recvxy3 = recvxy2;
				recvxy2 = recvxy1;
				recvxy1 = recvxy;
				if (recvxy1.u == recvxy2.u)
				{
					acousticmodem_x = recvxy.s[0]; 
					acousticmodem_y = recvxy.s[1];
				}
				else
				{
					acousticmodem_x = 0; 
					acousticmodem_y = 0;
				}
				LeaveCriticalSection(&StateVariablesCS);
				if (bExit||!AcousticCommandMDM) break;
				break;
			case SENDASK_MSG :
				LeaveCriticalSection(&MDMCS);
				memset(buf, 0, sizeof(buf));
				strcpy((char*)buf, "ask\n");
				for (i = 0; i < 1; i++)
				{
					if (SendAllDataMDM(&mdm, buf, strlen((char*)buf)) != EXIT_SUCCESS)
					{
						bError = TRUE;
						break;
					}
					//printf("SEND : %.4s\n", buf);
					// Time...
					if (gettimeofday(&tv, NULL) != EXIT_SUCCESS)
					{
						tv.tv_sec = 0;
						tv.tv_usec = 0;
					}
					if (mdm.bSaveRawData)
					{
						fprintf(mdm.pfSaveFile, "%d;%d;%d;%c;%.4s;\n", (int)tv.tv_sec, (int)tv.tv_usec, SENDASK_MSG, 's', buf);
						fflush(mdm.pfSaveFile);
					}
					if (WaitNsMDM(8) != EXIT_SUCCESS) break;
				}
				break;
			case RECVASK_MSG :
				LeaveCriticalSection(&MDMCS);
				if (PurgeDataAndWaitNsMDM(&mdm, &bError, 2) != EXIT_SUCCESS) break;
				memset(buf, 0, sizeof(buf));
				if (RecvAllDataMDM(&mdm, buf, strlen("ask\n")) != EXIT_SUCCESS)
				{
					//bError = TRUE;
					break;
				}
				//printf("RECV : %.4s", buf);
				// Time...
				if (gettimeofday(&tv, NULL) != EXIT_SUCCESS)
				{
					tv.tv_sec = 0;
					tv.tv_usec = 0;
				}
				if (mdm.bSaveRawData)
				{
					fprintf(mdm.pfSaveFile, "%d;%d;%d;%c;%.4s;\n", (int)tv.tv_sec, (int)tv.tv_usec, RECVASK_MSG, 'r', buf);
					fflush(mdm.pfSaveFile);
				}
				if (strncmp((char*)buf, "ask\n", strlen("ask\n")) == 0)
				{
					// Trigger continuous SENDXY_MSG.
					EnterCriticalSection(&MDMCS);
					AcousticCommandMDM = SENDXY_MSG;
					LeaveCriticalSection(&MDMCS);
				}
				if (bExit||!AcousticCommandMDM) break;
				break;
			case SENDSPWT_MSG :
				LeaveCriticalSection(&MDMCS);
				memset(buf, 0, sizeof(buf));
				strcpy((char*)buf, "SPWT");
				for (i = 0; i < 1; i++)
				{
					if (SendAllDataMDM(&mdm, buf, strlen((char*)buf)) != EXIT_SUCCESS)
					{
						bError = TRUE;
						break;
					}
					//printf("SEND : %.4s\n", buf);
					// Time...
					if (gettimeofday(&tv, NULL) != EXIT_SUCCESS)
					{
						tv.tv_sec = 0;
						tv.tv_usec = 0;
					}
					if (mdm.bSaveRawData)
					{
						fprintf(mdm.pfSaveFile, "%d;%d;%d;%c;%.4s;\n", (int)tv.tv_sec, (int)tv.tv_usec, SENDSPWT_MSG, 's', buf);
						fflush(mdm.pfSaveFile);
					}
					if (WaitNsMDM(8) != EXIT_SUCCESS) break;
				}
				break;
			case RECVSPWT_MSG :
				LeaveCriticalSection(&MDMCS);
				if (PurgeDataAndWaitNsMDM(&mdm, &bError, 2) != EXIT_SUCCESS) break;
				memset(buf, 0, sizeof(buf));
				if (RecvAllDataMDM(&mdm, buf, strlen("SPWT")) != EXIT_SUCCESS)
				{
					//bError = TRUE;
					break;
				}
				//printf("RECV : %.4s", buf);
				// Time...
				if (gettimeofday(&tv, NULL) != EXIT_SUCCESS)
				{
					tv.tv_sec = 0;
					tv.tv_usec = 0;
				}
				if (mdm.bSaveRawData)
				{
					fprintf(mdm.pfSaveFile, "%d;%d;%d;%c;%.4s;\n", (int)tv.tv_sec, (int)tv.tv_usec, RECVSPWT_MSG, 'r', buf);
					fflush(mdm.pfSaveFile);
				}
				if (strncmp((char*)buf, "SPWT", strlen("SPWT")) == 0)
				{
					// Release blocking command.
					EnterCriticalSection(&MDMCS);
					AcousticCommandMDM = 0;
					LeaveCriticalSection(&MDMCS);
				}
				if (bExit||!AcousticCommandMDM) break;
				break;
			case SENDSHH_MSG :
				LeaveCriticalSection(&MDMCS);
				memset(buf, 0, sizeof(buf));
				strcpy((char*)buf, "shh\n");
				for (i = 0; i < 1; i++)
				{
					if (SendAllDataMDM(&mdm, buf, strlen((char*)buf)) != EXIT_SUCCESS)
					{
						bError = TRUE;
						break;
					}
					//printf("SEND : %.4s\n", buf);
					// Time...
					if (gettimeofday(&tv, NULL) != EXIT_SUCCESS)
					{
						tv.tv_sec = 0;
						tv.tv_usec = 0;
					}
					if (mdm.bSaveRawData)
					{
						fprintf(mdm.pfSaveFile, "%d;%d;%d;%c;%.4s;\n", (int)tv.tv_sec, (int)tv.tv_usec, SENDSHH_MSG, 's', buf);
						fflush(mdm.pfSaveFile);
					}
					if (WaitNsMDM(8) != EXIT_SUCCESS) break;
				}
				break;
			case RECVSHH_MSG :
				LeaveCriticalSection(&MDMCS);
				if (PurgeDataAndWaitNsMDM(&mdm, &bError, 2) != EXIT_SUCCESS) break;
				memset(buf, 0, sizeof(buf));
				if (RecvAllDataMDM(&mdm, buf, strlen("shh\n")) != EXIT_SUCCESS)
				{
					//bError = TRUE;
					break;
				}
				//printf("RECV : %.4s", buf);
				// Time...
				if (gettimeofday(&tv, NULL) != EXIT_SUCCESS)
				{
					tv.tv_sec = 0;
					tv.tv_usec = 0;
				}
				if (mdm.bSaveRawData)
				{
					fprintf(mdm.pfSaveFile, "%d;%d;%d;%c;%.4s;\n", (int)tv.tv_sec, (int)tv.tv_usec, RECVSHH_MSG, 'r', buf);
					fflush(mdm.pfSaveFile);
				}
				if (strncmp((char*)buf, "shh\n", strlen("shh\n")) == 0)
				{
					// Release any blocking command and stop any continuously sent or received message.
					EnterCriticalSection(&MDMCS);
					AcousticCommandMDM = 0;
					LeaveCriticalSection(&MDMCS);
				}
				if (bExit||!AcousticCommandMDM) break;
				break;
			case RECVXY_RNG_MSG :
				LeaveCriticalSection(&MDMCS);
				if (PurgeDataAndWaitNsMDM(&mdm, &bError, 2) != EXIT_SUCCESS) break;
				memset(&recvxy, 0, sizeof(recvxy));
				if (RecvAllDataMDM(&mdm, (uint8*)&recvxy, sizeof(recvxy)) != EXIT_SUCCESS)
				{
					//bError = TRUE;
					break;
				}
				//printf("RECV : 0x%.2x%.2x%.2x%.2x (%d,%d)\n", 
				//	(unsigned int)recvxy.uc[3], (unsigned int)recvxy.uc[2], (unsigned int)recvxy.uc[1], (unsigned int)recvxy.uc[0], 
				//	(int)recvxy.s[0], (int)recvxy.s[1]);
				// Time...
				if (gettimeofday(&tv, NULL) != EXIT_SUCCESS)
				{
					tv.tv_sec = 0;
					tv.tv_usec = 0;
				}
				if (mdm.bSaveRawData)
				{
					fprintf(mdm.pfSaveFile, "%d;%d;%d;%c;0x%.2x%.2x%.2x%.2x (%d,%d);\n", (int)tv.tv_sec, (int)tv.tv_usec, RECVXY_MSG, 'r', 
						(unsigned int)recvxy.uc[3], (unsigned int)recvxy.uc[2], (unsigned int)recvxy.uc[1], (unsigned int)recvxy.uc[0], 
						(int)recvxy.s[0], (int)recvxy.s[1]);
					fflush(mdm.pfSaveFile);
				}
				EnterCriticalSection(&StateVariablesCS);
				//recvxy3 = recvxy2;
				recvxy2 = recvxy1;
				recvxy1 = recvxy;
				if (recvxy1.u == recvxy2.u)
				{
					acousticmodem_x = recvxy.s[0]; 
					acousticmodem_y = recvxy.s[1];
				}
				else
				{
					acousticmodem_x = 0; 
					acousticmodem_y = 0;
				}
				LeaveCriticalSection(&StateVariablesCS);
				if (bExit||!AcousticCommandMDM) break;

				mSleep(1000);
				if (bExit||!AcousticCommandMDM) break;

				memset(buf, 0, sizeof(buf));
				strcpy((char*)buf, "rng\n");
				for (i = 0; i < 1; i++)
				{
					if (SendAllDataMDM(&mdm, buf, strlen((char*)buf)) != EXIT_SUCCESS)
					{
						bError = TRUE;
						break;
					}
					//printf("SEND : %.4s\n", buf);
					// Time...
					if (gettimeofday(&tv, NULL) != EXIT_SUCCESS)
					{
						tv.tv_sec = 0;
						tv.tv_usec = 0;
					}
					if (mdm.bSaveRawData)
					{
						fprintf(mdm.pfSaveFile, "%d;%d;%d;%c;%.4s;\n", (int)tv.tv_sec, (int)tv.tv_usec, RNG_MSG, 's', buf);
						fflush(mdm.pfSaveFile);
					}
					if (bExit||!AcousticCommandMDM) break;
					mSleep(100);
					if (PurgeDataAndWaitNsMDM(&mdm, &bError, 3) != EXIT_SUCCESS) break;
					receivedbytes = 0;
					memset(buf, 0, sizeof(buf));
					if (RecvDataMDM(&mdm, buf, sizeof(buf)-1, &receivedbytes) != EXIT_SUCCESS)
					{
						bError = TRUE;
						break;
					}
					//printf("RECV : %.255s", buf);
					// Time...
					if (gettimeofday(&tv, NULL) != EXIT_SUCCESS)
					{
						tv.tv_sec = 0;
						tv.tv_usec = 0;
					}
					if (mdm.bSaveRawData)
					{
						fprintf(mdm.pfSaveFile, "%d;%d;%d;%c;%.255s;\n", (int)tv.tv_sec, (int)tv.tv_usec, RNG_MSG, 'r', buf);
						fflush(mdm.pfSaveFile);
					}
					EnterCriticalSection(&StateVariablesCS);
					acousticmodem_r = 0;
					if (sscanf((char*)buf, "\n\rRange=%lfm", &acousticmodem_r) != 1)
					{
						acousticmodem_r = 0;
					}
					LeaveCriticalSection(&StateVariablesCS);

					if ((acousticmodem_x != 0)&&(acousticmodem_y != 0)&&(acousticmodem_r != 0))
					{
						// Initial box to be able to contract...?
						EnterCriticalSection(&StateVariablesCS);
						box P = box(xhat,yhat);
						if (P.IsEmpty()) P = box(interval(-MAX_UNCERTAINTY,MAX_UNCERTAINTY),interval(-MAX_UNCERTAINTY,MAX_UNCERTAINTY));
						box M = box(
							interval(acousticmodem_x-x_max_err,acousticmodem_x+x_max_err),
							interval(acousticmodem_y-y_max_err,acousticmodem_y+y_max_err)
							);
						interval R = interval(acousticmodem_r-(x_max_err+y_max_err)/2.0,acousticmodem_r+(x_max_err+y_max_err)/2.0);
						Cdistance(R, P, M);
						if (R.isEmpty||P.IsEmpty()||M.IsEmpty()) 
						{
							// Expand initial box to be able to contract next time and because we are probably lost...
							P = box(xhat,yhat)+box(interval(-x_max_err,x_max_err),interval(-y_max_err,y_max_err));
							// Bug?
							//xhat = P[1];
							//yhat = P[2];
						}
						else
						{
							xhat = P[1];
							yhat = P[2];
						}
						LeaveCriticalSection(&StateVariablesCS);
					}
				}
				break;
			default : 
				LeaveCriticalSection(&MDMCS);
				break;
			}

			if (bError)
			{
				printf("Connection to a MDM lost.\n");
				bConnected = FALSE;
				DisconnectMDM(&mdm);
			}	

			if (bRestartMDM && bConnected)
			{
				printf("Restarting a MDM.\n");
				bRestartMDM = FALSE;
				bConnected = FALSE;
				DisconnectMDM(&mdm);
			}
		}

		if (bExit) break;
	}

	if (mdm.pfSaveFile != NULL)
	{
		fclose(mdm.pfSaveFile); 
		mdm.pfSaveFile = NULL;
	}

	if (bConnected) DisconnectMDM(&mdm);

	return 0;
}