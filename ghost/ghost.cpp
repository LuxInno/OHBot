/*

   Copyright [2008] [Trevor Hogan]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

*/

#include "ghost.h"
#include "util.h"
#include "crc32.h"
#include "sha1.h"
#include "config.h"
#include "language.h"
#include "socket.h"
#include "ghostdb.h"
#include "ghostdbmysql.h"
#include "bnet.h"
#include "map.h"
#include "packed.h"
#include "savegame.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "gpsprotocol.h"
#include "game_base.h"
#include "game.h"

#include <signal.h>
#include <stdlib.h>

#ifdef WIN32
 #include <ws2tcpip.h>		// for WSAIoctl
#endif

#define __STORMLIB_SELF__
#include <stormlib/StormLib.h>

#ifdef WIN32
 #include <windows.h>
 #include <winsock.h>
#endif

#include <time.h>

#ifndef WIN32
 #include <sys/time.h>
#endif

#ifdef __APPLE__
 #include <mach/mach_time.h>
#endif

string gCFGFile;
string gLogFile;
uint32_t gLogMethod;
ofstream *gLog = NULL;
CGHost *gGHost = NULL;

uint32_t GetTime( )
{
	return GetTicks( ) / 1000;
}

uint32_t GetTicks( )
{
#ifdef WIN32
	// don't use GetTickCount anymore because it's not accurate enough (~16ms resolution)
	// don't use QueryPerformanceCounter anymore because it isn't guaranteed to be strictly increasing on some systems and thus requires "smoothing" code
	// use timeGetTime instead, which typically has a high resolution (5ms or more) but we request a lower resolution on startup

	return timeGetTime( );
#elif __APPLE__
	uint64_t current = mach_absolute_time( );
	static mach_timebase_info_data_t info = { 0, 0 };
	// get timebase info
	if( info.denom == 0 )
		mach_timebase_info( &info );
	uint64_t elapsednano = current * ( info.numer / info.denom );
	// convert ns to ms
	return elapsednano / 1e6;
#else
	uint32_t ticks;
	struct timespec t;
	clock_gettime( CLOCK_MONOTONIC, &t );
	ticks = t.tv_sec * 1000;
	ticks += t.tv_nsec / 1000000;
	return ticks;
#endif
}

void SignalCatcher2( int s )
{
	CONSOLE_Print( "[!!!] caught signal " + UTIL_ToString( s ) + ", exiting NOW" );

	if( gGHost )
	{
		if( gGHost->m_Exiting )
			exit( 1 );
		else
			gGHost->m_Exiting = true;
	}
	else
		exit( 1 );
}

void SignalCatcher( int s )
{
	// signal( SIGABRT, SignalCatcher2 );
	signal( SIGINT, SignalCatcher2 );

	CONSOLE_Print( "[!!!] caught signal " + UTIL_ToString( s ) + ", exiting nicely" );

	if( gGHost )
		gGHost->m_ExitingNice = true;
	else
		exit( 1 );
}

void CONSOLE_Print( string message )
{
	cout << message << endl;

	// logging

	if( !gLogFile.empty( ) )
	{
		if( gLogMethod == 1 )
		{
			ofstream Log;
			Log.open( gLogFile.c_str( ), ios :: app );

			if( !Log.fail( ) )
			{
				time_t Now = time( NULL );
				string Time = asctime( localtime( &Now ) );

				// erase the newline

				Time.erase( Time.size( ) - 1 );
				Log << "[" << Time << "] " << message << endl;
				Log.close( );
			}
		}
		else if( gLogMethod == 2 )
		{
			if( gLog && !gLog->fail( ) )
			{
				time_t Now = time( NULL );
				string Time = asctime( localtime( &Now ) );

				// erase the newline

				Time.erase( Time.size( ) - 1 );
				*gLog << "[" << Time << "] " << message << endl;
				gLog->flush( );
			}
		}
	}
}

void DEBUG_Print( string message )
{
	cout << message << endl;
}

void DEBUG_Print( BYTEARRAY b )
{
	cout << "{ ";

	for( unsigned int i = 0; i < b.size( ); i++ )
		cout << hex << (int)b[i] << " ";

	cout << "}" << endl;
}

//
// main
//

int main( int argc, char **argv )
{
	gCFGFile = "ghost.cfg";

	if( argc > 1 && argv[1] )
		gCFGFile = argv[1];

	// read config file

	CConfig CFG;
	CFG.Read( "default.cfg" );
	CFG.Read( gCFGFile );
	gLogFile = CFG.GetString( "bot_log", string( ) );
	gLogMethod = CFG.GetInt( "bot_logmethod", 1 );

	if( !gLogFile.empty( ) )
	{
		if( gLogMethod == 1 )
		{
			// log method 1: open, append, and close the log for every message
			// this works well on Linux but poorly on Windows, particularly as the log file grows in size
			// the log file can be edited/moved/deleted while GHost++ is running
		}
		else if( gLogMethod == 2 )
		{
			// log method 2: open the log on startup, flush the log for every message, close the log on shutdown
			// the log file CANNOT be edited/moved/deleted while GHost++ is running

			gLog = new ofstream( );
			gLog->open( gLogFile.c_str( ), ios :: app );
		}
	}

	CONSOLE_Print( "[GHOST] starting up" );

	if( !gLogFile.empty( ) )
	{
		if( gLogMethod == 1 )
			CONSOLE_Print( "[GHOST] using log method 1, logging is enabled and [" + gLogFile + "] will not be locked" );
		else if( gLogMethod == 2 )
		{
			if( gLog->fail( ) )
				CONSOLE_Print( "[GHOST] using log method 2 but unable to open [" + gLogFile + "] for appending, logging is disabled" );
			else
				CONSOLE_Print( "[GHOST] using log method 2, logging is enabled and [" + gLogFile + "] is now locked" );
		}
	}
	else
		CONSOLE_Print( "[GHOST] no log file specified, logging is disabled" );

	// catch SIGABRT and SIGINT

	// signal( SIGABRT, SignalCatcher );
	signal( SIGINT, SignalCatcher );

#ifndef WIN32
	// disable SIGPIPE since some systems like OS X don't define MSG_NOSIGNAL

	signal( SIGPIPE, SIG_IGN );
#endif

#ifdef WIN32
	// initialize timer resolution
	// attempt to set the resolution as low as possible from 1ms to 5ms

	unsigned int TimerResolution = 0;

	for( unsigned int i = 1; i <= 5; i++ )
	{
		if( timeBeginPeriod( i ) == TIMERR_NOERROR )
		{
			TimerResolution = i;
			break;
		}
		else if( i < 5 )
			CONSOLE_Print( "[GHOST] error setting Windows timer resolution to " + UTIL_ToString( i ) + " milliseconds, trying a higher resolution" );
		else
		{
			CONSOLE_Print( "[GHOST] error setting Windows timer resolution" );
			return 1;
		}
	}

	CONSOLE_Print( "[GHOST] using Windows timer with resolution " + UTIL_ToString( TimerResolution ) + " milliseconds" );
#elif __APPLE__
	// not sure how to get the resolution
#else
	// print the timer resolution

	struct timespec Resolution;

	if( clock_getres( CLOCK_MONOTONIC, &Resolution ) == -1 )
		CONSOLE_Print( "[GHOST] error getting monotonic timer resolution" );
	else
		CONSOLE_Print( "[GHOST] using monotonic timer with resolution " + UTIL_ToString( (double)( Resolution.tv_nsec / 1000 ), 2 ) + " microseconds" );
#endif

#ifdef WIN32
	// initialize winsock

	CONSOLE_Print( "[GHOST] starting winsock" );
	WSADATA wsadata;

	if( WSAStartup( MAKEWORD( 2, 2 ), &wsadata ) != 0 )
	{
		CONSOLE_Print( "[GHOST] error starting winsock" );
		return 1;
	}

	// increase process priority

	CONSOLE_Print( "[GHOST] setting process priority to \"above normal\"" );
	SetPriorityClass( GetCurrentProcess( ), ABOVE_NORMAL_PRIORITY_CLASS );
#endif

	// initialize ghost

	gGHost = new CGHost( &CFG );

	while( 1 )
	{
		// block for 50ms on all sockets - if you intend to perform any timed actions more frequently you should change this
		// that said it's likely we'll loop more often than this due to there being data waiting on one of the sockets but there aren't any guarantees

		if( gGHost->Update( 50000 ) )
			break;
	}

	// shutdown ghost

	CONSOLE_Print( "[GHOST] shutting down" );
	delete gGHost;
	gGHost = NULL;

#ifdef WIN32
	// shutdown winsock

	CONSOLE_Print( "[GHOST] shutting down winsock" );
	WSACleanup( );

	// shutdown timer

	timeEndPeriod( TimerResolution );
#endif

	if( gLog )
	{
		if( !gLog->fail( ) )
			gLog->close( );

		delete gLog;
	}

	return 0;
}

//
// CGHost
//

CGHost :: CGHost( CConfig *CFG )
{
    
	m_UDPSocket = new CUDPSocket( );
	m_UDPSocket->SetBroadcastTarget( CFG->GetString( "udp_broadcasttarget", string( ) ) );
	m_UDPSocket->SetDontRoute( CFG->GetInt( "udp_dontroute", 0 ) == 0 ? false : true );
	m_ReconnectSocket = NULL;
	m_GPSProtocol = new CGPSProtocol( );
	m_CRC = new CCRC32( );
	m_CRC->Initialize( );
	m_SHA = new CSHA1( );
	m_CurrentGame = NULL;
        m_CallableGetGameId = NULL;
        m_CallableGetBotConfig = NULL;
        m_CallableGetBotConfigText = NULL;
        m_CallableGetLanguages = NULL;
        m_CallableGetStatsTemplates = NULL;
        m_CallableBanList = NULL;
        m_NewGameId = 0;
        m_LastGameIdUpdate = GetTime( );
	CONSOLE_Print( "[GHOST] opening primary database" );
	m_LANWar3Version = 26;
	m_ReplayWar3Version = 26;
	m_AutoHostSplitter = "#";

        m_DB = new CGHostDBMySQL( CFG );

        /* load configs */
        m_CallableGetBotConfig = m_DB->ThreadedGetBotConfigs( );
        m_CallableGetBotConfigText = m_DB->ThreadedGetBotConfigTexts( );
        m_CallableAdminLists = m_DB->ThreadedAdminList( "" );
        m_CallableGetAliases = m_DB->ThreadedGetAliases( );
        m_CallableGetStatsTemplates = m_DB->ThreadedGetStatsTemplates( );
        m_CallableBanList = m_DB->ThreadedBanList( "" );
        
        m_LastListRefresh = GetTime();

	// get a list of local IP addresses
	// this list is used elsewhere to determine if a player connecting to the bot is local or not

	CONSOLE_Print( "[GHOST] attempting to find local IP addresses" );

#ifdef WIN32
	// use a more reliable Windows specific method since the portable method doesn't always work properly on Windows
	// code stolen from: http://tangentsoft.net/wskfaq/examples/getifaces.html

	SOCKET sd = WSASocket( AF_INET, SOCK_DGRAM, 0, 0, 0, 0 );

	if( sd == SOCKET_ERROR )
		CONSOLE_Print( "[GHOST] error finding local IP addresses - failed to create socket (error code " + UTIL_ToString( WSAGetLastError( ) ) + ")" );
	else
	{
		INTERFACE_INFO InterfaceList[20];
		unsigned long nBytesReturned;

		if( WSAIoctl( sd, SIO_GET_INTERFACE_LIST, 0, 0, &InterfaceList, sizeof(InterfaceList), &nBytesReturned, 0, 0 ) == SOCKET_ERROR )
			CONSOLE_Print( "[GHOST] error finding local IP addresses - WSAIoctl failed (error code " + UTIL_ToString( WSAGetLastError( ) ) + ")" );
		else
		{
			int nNumInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);

			for( int i = 0; i < nNumInterfaces; i++ )
			{
				sockaddr_in *pAddress;
				pAddress = (sockaddr_in *)&(InterfaceList[i].iiAddress);
				CONSOLE_Print( "[GHOST] local IP address #" + UTIL_ToString( i + 1 ) + " is [" + string( inet_ntoa( pAddress->sin_addr ) ) + "]" );
				m_LocalAddresses.push_back( UTIL_CreateByteArray( (uint32_t)pAddress->sin_addr.s_addr, false ) );
			}
		}

		closesocket( sd );
	}
#else
	// use a portable method

	char HostName[255];

	if( gethostname( HostName, 255 ) == SOCKET_ERROR )
		CONSOLE_Print( "[GHOST] error finding local IP addresses - failed to get local hostname" );
	else
	{
		CONSOLE_Print( "[GHOST] local hostname is [" + string( HostName ) + "]" );
		struct hostent *HostEnt = gethostbyname( HostName );

		if( !HostEnt )
			CONSOLE_Print( "[GHOST] error finding local IP addresses - gethostbyname failed" );
		else
		{
			for( int i = 0; HostEnt->h_addr_list[i] != NULL; i++ )
			{
				struct in_addr Address;
				memcpy( &Address, HostEnt->h_addr_list[i], sizeof(struct in_addr) );
				CONSOLE_Print( "[GHOST] local IP address #" + UTIL_ToString( i + 1 ) + " is [" + string( inet_ntoa( Address ) ) + "]" );
				m_LocalAddresses.push_back( UTIL_CreateByteArray( (uint32_t)Address.s_addr, false ) );
			}
		}
	}
#endif

	m_Language = NULL;
	m_Exiting = false;
	m_ExitingNice = false;
	m_Enabled = true;
	m_Version = "17.2";
	m_LastAutoHostTime = GetTime( );
	m_HostCounter = CFG->GetInt( "bot_hostcounter", 1 );

	if( m_TFT )
		CONSOLE_Print( "[GHOST] acting as Warcraft III: The Frozen Throne" );
	else
		CONSOLE_Print( "[GHOST] acting as Warcraft III: Reign of Chaos" );

	// load the battle.net connections
	// we're just loading the config data and creating the CBNET classes here, the connections are established later (in the Update function)

        m_Warcraft3Path = UTIL_AddPathSeperator( CFG->GetString( "bot_war3path", "C:\\Program Files\\Warcraft III\\" ) );

	CONSOLE_Print( "[GHOST] GHost++ Version " + m_Version + " (with MySQL support)" );
}

CGHost :: ~CGHost( )
{
	delete m_UDPSocket;
	delete m_ReconnectSocket;

	for( vector<CTCPSocket *> :: iterator i = m_ReconnectSockets.begin( ); i != m_ReconnectSockets.end( ); i++ )
		delete *i;

	delete m_GPSProtocol;
	delete m_CRC;
	delete m_SHA;

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		delete *i;

	delete m_CurrentGame;

	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); i++ )
		delete *i;

	delete m_DB;

	// warning: we don't delete any entries of m_Callables here because we can't be guaranteed that the associated threads have terminated
	// this is fine if the program is currently exiting because the OS will clean up after us
	// but if you try to recreate the CGHost object within a single session you will probably leak resources!

	if( !m_Callables.empty( ) )
		CONSOLE_Print( "[GHOST] warning - " + UTIL_ToString( m_Callables.size( ) ) + " orphaned callables were leaked (this is not an error)" );

	delete m_Language;
	delete m_Map;
	delete m_AutoHostMap;
	delete m_SaveGame;
}

bool CGHost :: Update( long usecBlock )
{
	// todotodo: do we really want to shutdown if there's a database error? is there any way to recover from this?

	if( m_DB->HasError( ) )
	{
		CONSOLE_Print( "[GHOST] database error - " + m_DB->GetError( ) );
		return true;
	}

	// try to exit nicely if requested to do so

	if( m_ExitingNice )
	{
		if( !m_BNETs.empty( ) )
		{
			CONSOLE_Print( "[GHOST] deleting all battle.net connections in preparation for exiting nicely" );

			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
				delete *i;

			m_BNETs.clear( );
		}

		if( m_CurrentGame )
		{
			CONSOLE_Print( "[GHOST] deleting current game in preparation for exiting nicely" );
			delete m_CurrentGame;
			m_CurrentGame = NULL;
		}

		if( m_Games.empty( ) )
		{
			if( !m_AllGamesFinished )
			{
				CONSOLE_Print( "[GHOST] all games finished, waiting 60 seconds for threads to finish" );
				CONSOLE_Print( "[GHOST] there are " + UTIL_ToString( m_Callables.size( ) ) + " threads in progress" );
				m_AllGamesFinished = true;
				m_AllGamesFinishedTime = GetTime( );
			}
			else
			{
				if( m_Callables.empty( ) )
				{
					CONSOLE_Print( "[GHOST] all threads finished, exiting nicely" );
					m_Exiting = true;
				}
				else if( GetTime( ) - m_AllGamesFinishedTime >= 60 )
				{
					CONSOLE_Print( "[GHOST] waited 60 seconds for threads to finish, exiting anyway" );
					CONSOLE_Print( "[GHOST] there are " + UTIL_ToString( m_Callables.size( ) ) + " threads still in progress which will be terminated" );
					m_Exiting = true;
				}
			}
		}
	}

	// update callables

	for( vector<CBaseCallable *> :: iterator i = m_Callables.begin( ); i != m_Callables.end( ); )
	{
		if( (*i)->GetReady( ) )
		{
			m_DB->RecoverCallable( *i );
			delete *i;
			i = m_Callables.erase( i );
		}
		else
			i++;
	}

	// create the GProxy++ reconnect listener

	if( m_Reconnect )
	{
		if( !m_ReconnectSocket )
		{
			m_ReconnectSocket = new CTCPServer( );

			if( m_ReconnectSocket->Listen( m_BindAddress, m_ReconnectPort ) )
				CONSOLE_Print( "[GHOST] listening for GProxy++ reconnects on port " + UTIL_ToString( m_ReconnectPort ) );
			else
			{
				CONSOLE_Print( "[GHOST] error listening for GProxy++ reconnects on port " + UTIL_ToString( m_ReconnectPort ) );
				delete m_ReconnectSocket;
				m_ReconnectSocket = NULL;
				m_Reconnect = false;
			}
		}
		else if( m_ReconnectSocket->HasError( ) )
		{
			CONSOLE_Print( "[GHOST] GProxy++ reconnect listener error (" + m_ReconnectSocket->GetErrorString( ) + ")" );
			delete m_ReconnectSocket;
			m_ReconnectSocket = NULL;
			m_Reconnect = false;
		}
	}

	unsigned int NumFDs = 0;

	// take every socket we own and throw it in one giant select statement so we can block on all sockets

	int nfds = 0;
	fd_set fd;
	fd_set send_fd;
	FD_ZERO( &fd );
	FD_ZERO( &send_fd );

	// 1. all battle.net sockets

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		NumFDs += (*i)->SetFD( &fd, &send_fd, &nfds );

	// 2. the current game's server and player sockets

	if( m_CurrentGame )
		NumFDs += m_CurrentGame->SetFD( &fd, &send_fd, &nfds );

	// 3. all running games' player sockets

	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); i++ )
		NumFDs += (*i)->SetFD( &fd, &send_fd, &nfds );

	// 4. the GProxy++ reconnect socket(s)

	if( m_Reconnect && m_ReconnectSocket )
	{
		m_ReconnectSocket->SetFD( &fd, &send_fd, &nfds );
		NumFDs++;
	}

	for( vector<CTCPSocket *> :: iterator i = m_ReconnectSockets.begin( ); i != m_ReconnectSockets.end( ); i++ )
	{
		(*i)->SetFD( &fd, &send_fd, &nfds );
		NumFDs++;
	}

	// before we call select we need to determine how long to block for
	// previously we just blocked for a maximum of the passed usecBlock microseconds
	// however, in an effort to make game updates happen closer to the desired latency setting we now use a dynamic block interval
	// note: we still use the passed usecBlock as a hard maximum

	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); i++ )
	{
		if( (*i)->GetNextTimedActionTicks( ) * 1000 < usecBlock )
			usecBlock = (*i)->GetNextTimedActionTicks( ) * 1000;
	}

	// always block for at least 1ms just in case something goes wrong
	// this prevents the bot from sucking up all the available CPU if a game keeps asking for immediate updates
	// it's a bit ridiculous to include this check since, in theory, the bot is programmed well enough to never make this mistake
	// however, considering who programmed it, it's worthwhile to do it anyway

	if( usecBlock < 1000 )
		usecBlock = 1000;

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = usecBlock;

	struct timeval send_tv;
	send_tv.tv_sec = 0;
	send_tv.tv_usec = 0;

#ifdef WIN32
	select( 1, &fd, NULL, NULL, &tv );
	select( 1, NULL, &send_fd, NULL, &send_tv );
#else
	select( nfds + 1, &fd, NULL, NULL, &tv );
	select( nfds + 1, NULL, &send_fd, NULL, &send_tv );
#endif

	if( NumFDs == 0 )
	{
		// we don't have any sockets (i.e. we aren't connected to battle.net maybe due to a lost connection and there aren't any games running)
		// select will return immediately and we'll chew up the CPU if we let it loop so just sleep for 50ms to kill some time

		MILLISLEEP( 50 );
	}

	bool AdminExit = false;
	bool BNETExit = false;

	// update current game

	if( m_CurrentGame )
	{
		if( m_CurrentGame->Update( &fd, &send_fd ) )
		{
			CONSOLE_Print( "[GHOST] deleting current game [" + m_CurrentGame->GetGameName( ) + "]" );
			delete m_CurrentGame;
			m_CurrentGame = NULL;

			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
			{
				(*i)->QueueGameUncreate( );
				(*i)->QueueEnterChat( );
			}
		}
		else if( m_CurrentGame )
			m_CurrentGame->UpdatePost( &send_fd );
	}

	// update running games

	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); )
	{
		if( (*i)->Update( &fd, &send_fd ) )
		{
			CONSOLE_Print( "[GHOST] deleting game [" + (*i)->GetGameName( ) + "]" );
			EventGameDeleted( *i );
			delete *i;
			i = m_Games.erase( i );
		}
		else
		{
			(*i)->UpdatePost( &send_fd );
			i++;
		}
	}

	// update battle.net connections

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
	{
		if( (*i)->Update( &fd, &send_fd ) )
			BNETExit = true;
	}

	// update GProxy++ reliable reconnect sockets

	if( m_Reconnect && m_ReconnectSocket )
	{
		CTCPSocket *NewSocket = m_ReconnectSocket->Accept( &fd );

		if( NewSocket )
			m_ReconnectSockets.push_back( NewSocket );
	}

	for( vector<CTCPSocket *> :: iterator i = m_ReconnectSockets.begin( ); i != m_ReconnectSockets.end( ); )
	{
		if( (*i)->HasError( ) || !(*i)->GetConnected( ) || GetTime( ) - (*i)->GetLastRecv( ) >= 10 )
		{
			delete *i;
			i = m_ReconnectSockets.erase( i );
			continue;
		}

		(*i)->DoRecv( &fd );
		string *RecvBuffer = (*i)->GetBytes( );
		BYTEARRAY Bytes = UTIL_CreateByteArray( (unsigned char *)RecvBuffer->c_str( ), RecvBuffer->size( ) );

		// a packet is at least 4 bytes

		if( Bytes.size( ) >= 4 )
		{
			if( Bytes[0] == GPS_HEADER_CONSTANT )
			{
				// bytes 2 and 3 contain the length of the packet

				uint16_t Length = UTIL_ByteArrayToUInt16( Bytes, false, 2 );

				if( Length >= 4 )
				{
					if( Bytes.size( ) >= Length )
					{
						if( Bytes[1] == CGPSProtocol :: GPS_RECONNECT && Length == 13 )
						{
							unsigned char PID = Bytes[4];
							uint32_t ReconnectKey = UTIL_ByteArrayToUInt32( Bytes, false, 5 );
							uint32_t LastPacket = UTIL_ByteArrayToUInt32( Bytes, false, 9 );

							// look for a matching player in a running game

							CGamePlayer *Match = NULL;

							for( vector<CBaseGame *> :: iterator j = m_Games.begin( ); j != m_Games.end( ); j++ )
							{
								if( (*j)->GetGameLoaded( ) )
								{
									CGamePlayer *Player = (*j)->GetPlayerFromPID( PID );

									if( Player && Player->GetGProxy( ) && Player->GetGProxyReconnectKey( ) == ReconnectKey )
									{
										Match = Player;
										break;
									}
								}
							}

							if( Match )
							{
								// reconnect successful!

								*RecvBuffer = RecvBuffer->substr( Length );
								Match->EventGProxyReconnect( *i, LastPacket );
								i = m_ReconnectSockets.erase( i );
								continue;
							}
							else
							{
								(*i)->PutBytes( m_GPSProtocol->SEND_GPSS_REJECT( REJECTGPS_NOTFOUND ) );
								(*i)->DoSend( &send_fd );
								delete *i;
								i = m_ReconnectSockets.erase( i );
								continue;
							}
						}
						else
						{
							(*i)->PutBytes( m_GPSProtocol->SEND_GPSS_REJECT( REJECTGPS_INVALID ) );
							(*i)->DoSend( &send_fd );
							delete *i;
							i = m_ReconnectSockets.erase( i );
							continue;
						}
					}
				}
				else
				{
					(*i)->PutBytes( m_GPSProtocol->SEND_GPSS_REJECT( REJECTGPS_INVALID ) );
					(*i)->DoSend( &send_fd );
					delete *i;
					i = m_ReconnectSockets.erase( i );
					continue;
				}
			}
			else
			{
				(*i)->PutBytes( m_GPSProtocol->SEND_GPSS_REJECT( REJECTGPS_INVALID ) );
				(*i)->DoSend( &send_fd );
				delete *i;
				i = m_ReconnectSockets.erase( i );
				continue;
			}
		}

		(*i)->DoSend( &send_fd );
		i++;
	}

	// autohost

	if( !m_AutoHostGameName.empty( ) && m_AutoHostMaximumGames != 0 && m_AutoHostAutoStartPlayers != 0 && GetTime( ) - m_LastAutoHostTime >= 10 && m_NewGameId != 0 )
	{
		// copy all the checks from CGHost :: CreateGame here because we don't want to spam the chat when there's an error
		// instead we fail silently and try again soon

		if( !m_ExitingNice && m_Enabled && !m_CurrentGame && m_Games.size( ) < m_MaxGames && m_Games.size( ) < m_AutoHostMaximumGames )
		{
			if( m_AutoHostMap->GetValid( ) )
			{
				string GameName = m_AutoHostGameName + " " + m_AutoHostSplitter + UTIL_ToString( m_NewGameId % 100 );

				if( GameName.size( ) <= 31 )
				{
					CreateGame( m_AutoHostMap, GAME_PUBLIC, false, GameName, m_AutoHostOwner, m_AutoHostOwner, m_AutoHostServer, false );

					if( m_CurrentGame )
					{
						m_CurrentGame->SetAutoStartPlayers( m_AutoHostAutoStartPlayers );

						if( m_AutoHostMatchMaking )
						{
							if( !m_Map->GetMapMatchMakingCategory( ).empty( ) )
							{
								if( !( m_Map->GetMapOptions( ) & MAPOPT_FIXEDPLAYERSETTINGS ) )
									CONSOLE_Print( "[GHOST] autohostmm - map_matchmakingcategory [" + m_Map->GetMapMatchMakingCategory( ) + "] found but matchmaking can only be used with fixed player settings, matchmaking disabled" );
								else
								{
									CONSOLE_Print( "[GHOST] autohostmm - map_matchmakingcategory [" + m_Map->GetMapMatchMakingCategory( ) + "] found, matchmaking enabled" );

									m_CurrentGame->SetMatchMaking( true );
									m_CurrentGame->SetMinimumScore( m_AutoHostMinimumScore );
									m_CurrentGame->SetMaximumScore( m_AutoHostMaximumScore );
								}
							}
							else
								CONSOLE_Print( "[GHOST] autohostmm - map_matchmakingcategory not found, matchmaking disabled" );
						}
					}
				}
				else
				{
					CONSOLE_Print( "[GHOST] stopped auto hosting, next game name [" + GameName + "] is too long (the maximum is 31 characters)" );
					m_AutoHostGameName.clear( );
					m_AutoHostOwner.clear( );
					m_AutoHostServer.clear( );
					m_AutoHostMaximumGames = 0;
					m_AutoHostAutoStartPlayers = 0;
					m_AutoHostMatchMaking = false;
					m_AutoHostMinimumScore = 0.0;
					m_AutoHostMaximumScore = 0.0;
				}
			}
			else
			{
				CONSOLE_Print( "[GHOST] stopped auto hosting, map config is invalid" );
				m_AutoHostGameName.clear( );
				m_AutoHostOwner.clear( );
				m_AutoHostServer.clear( );
				m_AutoHostMaximumGames = 0;
				m_AutoHostAutoStartPlayers = 0;
				m_AutoHostMatchMaking = false;
				m_AutoHostMinimumScore = 0.0;
				m_AutoHostMaximumScore = 0.0;
			}
		}

		m_LastAutoHostTime = GetTime( );
	}
    
    // load a new gameid
    if( m_NewGameId == 0 && m_LastGameIdUpdate != 0 && GetTime( ) - m_LastGameIdUpdate >= 5 )
    {
        m_CallableGetGameId = m_DB->ThreadedGetGameId( );
        m_LastGameIdUpdate = GetTime();
    }

    if( m_CallableGetGameId && m_CallableGetGameId->GetReady( ) )
    {
        m_NewGameId = m_CallableGetGameId->GetResult( );
        CONSOLE_Print("[OHSYSTEM] Got new gameid: " + UTIL_ToString(m_NewGameId));
        m_DB->RecoverCallable( m_CallableGetGameId );
        delete m_CallableGetGameId;
        m_CallableGetGameId = NULL;
    }

    if( m_CallableGetBotConfig && m_CallableGetBotConfig->GetReady( )) {
        map<string, string> configs = m_CallableGetBotConfig->GetResult( );
        ParseConfigValues( configs );
         
        m_DB->RecoverCallable( m_CallableGetBotConfig );
        delete m_CallableGetBotConfig;
        m_CallableGetBotConfig = NULL;
    }

    if( m_CallableGetBotConfigText && m_CallableGetBotConfigText->GetReady( )) {
        map<string, vector<string>> texts = m_CallableGetBotConfigText->GetResult( );
        ParseConfigTexts( texts );
        
        m_DB->RecoverCallable( m_CallableGetBotConfigText );
        delete m_CallableGetBotConfigText;
        m_CallableGetBotConfigText = NULL;
    }

    if( m_CallableGetLanguages && m_CallableGetLanguages->GetReady( )) {
        m_Translations = m_CallableGetLanguages->GetResult( );
        
        m_DB->RecoverCallable( m_CallableGetLanguages );
        delete m_CallableGetLanguages;
        m_CallableGetLanguages = NULL;
    }

    if( m_CallableGetMapConfig && m_CallableGetMapConfig->GetReady( )) {
        if(! m_CurrentGame ){
            m_Map = new CMap( this, m_CallableGetMapConfig->GetResult( ) );
            m_AutoHostMap = new CMap( *m_Map );
            
            m_SaveGame = new CSaveGame( );
        }
        
        m_DB->RecoverCallable( m_CallableGetMapConfig );
        delete m_CallableGetMapConfig;
        m_CallableGetMapConfig = NULL;
    }
    
    if( m_CallableAdminLists && m_CallableAdminLists->GetReady( )) {
        m_AdminList = m_CallableAdminLists->GetResult( );
        CONSOLE_Print("[OHSystem] Loaded " + UTIL_ToString(m_AdminList.size()) + " users.");
        
        m_DB->RecoverCallable( m_CallableAdminLists );
        delete m_CallableAdminLists;
        m_CallableAdminLists = NULL;
    }
    
    if( m_CallableGetAliases && m_CallableGetAliases->GetReady( )) {
        m_Aliases = m_CallableGetAliases->GetResult( );
        CONSOLE_Print("[OHSystem] Loaded " + UTIL_ToString(m_Aliases.size()) + " aliases.");
        
        m_DB->RecoverCallable( m_CallableGetAliases );
        delete m_CallableGetAliases;
        m_CallableGetAliases = NULL;
    }
    
    if( m_CallableGetStatsTemplates && m_CallableGetStatsTemplates->GetReady( )) {
        m_StatsTemplates = m_CallableGetStatsTemplates->GetResult( );
        CONSOLE_Print("[OHSystem] Loaded " + UTIL_ToString(m_StatsTemplates.size()) + " stats templates.");

        m_DB->RecoverCallable( m_CallableGetStatsTemplates );
        delete m_CallableGetStatsTemplates;
        m_CallableGetStatsTemplates = NULL;
    }
       
    if(! m_CallableBanList && GetTime( ) - m_LastListRefresh >= 300) {
        m_CallableBanList = m_DB->ThreadedBanList("");
        m_CallableAdminLists = m_DB->ThreadedAdminList("");
        
        m_LastListRefresh = GetTime( );
    }
	
    if( m_CallableBanList && m_CallableBanList->GetReady( )) {
        m_BanList = m_CallableBanList->GetResult( );
        CONSOLE_Print("[OHSystem] Loaded " + UTIL_ToString(m_BanList.size()) + " bans");

        m_DB->RecoverCallable( m_CallableBanList );
        delete m_CallableBanList;
        m_CallableBanList = NULL;
    }
        
    return m_Exiting || AdminExit || BNETExit;
}

void CGHost :: EventBNETConnecting( CBNET *bnet )
{
	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->ConnectingToBNET( bnet->GetServer( ) ) );
}

void CGHost :: EventBNETConnected( CBNET *bnet )
{
	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->ConnectedToBNET( bnet->GetServer( ) ) );
}

void CGHost :: EventBNETDisconnected( CBNET *bnet )
{
	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->DisconnectedFromBNET( bnet->GetServer( ) ) );
}

void CGHost :: EventBNETLoggedIn( CBNET *bnet )
{
	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->LoggedInToBNET( bnet->GetServer( ) ) );
}

void CGHost :: EventBNETGameRefreshed( CBNET *bnet )
{
	if( m_CurrentGame )
		m_CurrentGame->EventGameRefreshed( bnet->GetServer( ) );
}

void CGHost :: EventBNETGameRefreshFailed( CBNET *bnet )
{
	if( m_CurrentGame )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			(*i)->QueueChatCommand( m_Language->UnableToCreateGameTryAnotherName( bnet->GetServer( ), m_CurrentGame->GetGameName( ) ) );

			if( (*i)->GetServer( ) == m_CurrentGame->GetCreatorServer( ) )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameTryAnotherName( bnet->GetServer( ), m_CurrentGame->GetGameName( ) ), m_CurrentGame->GetCreatorName( ), true );
		}

		m_CurrentGame->SendAllChat( m_Language->UnableToCreateGameTryAnotherName( bnet->GetServer( ), m_CurrentGame->GetGameName( ) ) );

		// we take the easy route and simply close the lobby if a refresh fails
		// it's possible at least one refresh succeeded and therefore the game is still joinable on at least one battle.net (plus on the local network) but we don't keep track of that
		// we only close the game if it has no players since we support game rehosting (via !priv and !pub in the lobby)

		if( m_CurrentGame->GetNumHumanPlayers( ) == 0 )
			m_CurrentGame->SetExiting( true );

		m_CurrentGame->SetRefreshError( true );
	}
}

void CGHost :: EventBNETConnectTimedOut( CBNET *bnet )
{
	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->ConnectingToBNETTimedOut( bnet->GetServer( ) ) );
}

void CGHost :: EventBNETWhisper( CBNET *bnet, string user, string message ){}

void CGHost :: EventBNETChat( CBNET *bnet, string user, string message ){}

void CGHost :: EventBNETEmote( CBNET *bnet, string user, string message ){}

void CGHost :: EventGameDeleted( CBaseGame *game )
{
	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
	{
		(*i)->QueueChatCommand( m_Language->GameIsOver( game->GetDescription( ) ) );

		if( (*i)->GetServer( ) == game->GetCreatorServer( ) )
			(*i)->QueueChatCommand( m_Language->GameIsOver( game->GetDescription( ) ), game->GetCreatorName( ), true );
	}
}

void CGHost :: ReloadConfigs( )
{
    m_CallableGetBotConfig = m_DB->ThreadedGetBotConfigs( );
    m_CallableGetBotConfigText = m_DB->ThreadedGetBotConfigTexts( );
    m_CallableGetLanguages = m_DB->ThreadedGetLanguages( );
}

void CGHost :: ExtractScripts( )
{
	string PatchMPQFileName = m_Warcraft3Path + "War3Patch.mpq";
	HANDLE PatchMPQ;

	if( SFileOpenArchive( PatchMPQFileName.c_str( ), 0, MPQ_OPEN_FORCE_MPQ_V1, &PatchMPQ ) )
	{
		CONSOLE_Print( "[GHOST] loading MPQ file [" + PatchMPQFileName + "]" );
		HANDLE SubFile;

		// common.j

		if( SFileOpenFileEx( PatchMPQ, "Scripts\\common.j", 0, &SubFile ) )
		{
			uint32_t FileLength = SFileGetFileSize( SubFile, NULL );

			if( FileLength > 0 && FileLength != 0xFFFFFFFF )
			{
				char *SubFileData = new char[FileLength];
				DWORD BytesRead = 0;

				if( SFileReadFile( SubFile, SubFileData, FileLength, &BytesRead ) )
				{
					CONSOLE_Print( "[GHOST] extracting Scripts\\common.j from MPQ file to [" + m_MapPath + "common.j]" );
					UTIL_FileWrite( m_MapPath + "common.j", (unsigned char *)SubFileData, BytesRead );
				}
				else
					CONSOLE_Print( "[GHOST] warning - unable to extract Scripts\\common.j from MPQ file" );

				delete [] SubFileData;
			}

			SFileCloseFile( SubFile );
		}
		else
			CONSOLE_Print( "[GHOST] couldn't find Scripts\\common.j in MPQ file" );

		// blizzard.j

		if( SFileOpenFileEx( PatchMPQ, "Scripts\\blizzard.j", 0, &SubFile ) )
		{
			uint32_t FileLength = SFileGetFileSize( SubFile, NULL );

			if( FileLength > 0 && FileLength != 0xFFFFFFFF )
			{
				char *SubFileData = new char[FileLength];
				DWORD BytesRead = 0;

				if( SFileReadFile( SubFile, SubFileData, FileLength, &BytesRead ) )
				{
					CONSOLE_Print( "[GHOST] extracting Scripts\\blizzard.j from MPQ file to [" + m_MapPath + "blizzard.j]" );
					UTIL_FileWrite( m_MapPath + "blizzard.j", (unsigned char *)SubFileData, BytesRead );
				}
				else
					CONSOLE_Print( "[GHOST] warning - unable to extract Scripts\\blizzard.j from MPQ file" );

				delete [] SubFileData;
			}

			SFileCloseFile( SubFile );
		}
		else
			CONSOLE_Print( "[GHOST] couldn't find Scripts\\blizzard.j in MPQ file" );

		SFileCloseArchive( PatchMPQ );
	}
	else
		CONSOLE_Print( "[GHOST] warning - unable to load MPQ file [" + PatchMPQFileName + "] - error code " + UTIL_ToString( GetLastError( ) ) );
}

void CGHost :: CreateGame( CMap *map, unsigned char gameState, bool saveGame, string gameName, string ownerName, string creatorName, string creatorServer, bool whisper )
{
	if( !m_Enabled )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameDisabled( gameName ), creatorName, whisper );
		}

		return;
	}

	if( gameName.size( ) > 31 )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameNameTooLong( gameName ), creatorName, whisper );
		}

		return;
	}

	if( !map->GetValid( ) )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameInvalidMap( gameName ), creatorName, whisper );
		}

		return;
	}

	if( saveGame )
	{
		if( !m_SaveGame->GetValid( ) )
		{
			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
			{
				if( (*i)->GetServer( ) == creatorServer )
					(*i)->QueueChatCommand( m_Language->UnableToCreateGameInvalidSaveGame( gameName ), creatorName, whisper );
			}

			return;
		}

		if( m_EnforcePlayers.empty( ) )
		{
			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
			{
				if( (*i)->GetServer( ) == creatorServer )
					(*i)->QueueChatCommand( m_Language->UnableToCreateGameMustEnforceFirst( gameName ), creatorName, whisper );
			}

			return;
		}
	}

	if( m_CurrentGame )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameAnotherGameInLobby( gameName, m_CurrentGame->GetDescription( ) ), creatorName, whisper );
		}

		return;
	}

	if( m_Games.size( ) >= m_MaxGames )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->UnableToCreateGameMaxGamesReached( gameName, UTIL_ToString( m_MaxGames ) ), creatorName, whisper );
		}

		return;
	}

	CONSOLE_Print( "[GHOST] creating game [" + gameName + "]" );

	if( saveGame )
		m_CurrentGame = new CGame( this, map, m_SaveGame, m_HostPort, gameState, gameName, ownerName, creatorName, creatorServer, m_NewGameId );
	else
		m_CurrentGame = new CGame( this, map, NULL, m_HostPort, gameState, gameName, ownerName, creatorName, creatorServer, m_NewGameId );

	// todotodo: check if listening failed and report the error to the user

	if( m_SaveGame )
	{
		m_CurrentGame->SetEnforcePlayers( m_EnforcePlayers );
		m_EnforcePlayers.clear( );
	}

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
	{
		if( whisper && (*i)->GetServer( ) == creatorServer )
		{
			// note that we send this whisper only on the creator server

			if( gameState == GAME_PRIVATE )
				(*i)->QueueChatCommand( m_Language->CreatingPrivateGame( gameName, ownerName ), creatorName, whisper );
			else if( gameState == GAME_PUBLIC )
				(*i)->QueueChatCommand( m_Language->CreatingPublicGame( gameName, ownerName ), creatorName, whisper );
		}
		else
		{
			// note that we send this chat message on all other bnet servers

			if( gameState == GAME_PRIVATE )
				(*i)->QueueChatCommand( m_Language->CreatingPrivateGame( gameName, ownerName ) );
			else if( gameState == GAME_PUBLIC )
				(*i)->QueueChatCommand( m_Language->CreatingPublicGame( gameName, ownerName ) );
		}

		if( saveGame )
			(*i)->QueueGameCreate( gameState, gameName, string( ), map, m_SaveGame, m_CurrentGame->GetHostCounter( ) );
		else
			(*i)->QueueGameCreate( gameState, gameName, string( ), map, NULL, m_CurrentGame->GetHostCounter( ) );
	}

	// if we're creating a private game we don't need to send any game refresh messages so we can rejoin the chat immediately
	// unfortunately this doesn't work on PVPGN servers because they consider an enterchat message to be a gameuncreate message when in a game
	// so don't rejoin the chat if we're using PVPGN

	if( gameState == GAME_PRIVATE )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetPasswordHashType( ) != "pvpgn" )
				(*i)->QueueEnterChat( );
		}
	}

	// hold friends and/or clan members

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
	{
		if( (*i)->GetHoldFriends( ) )
			(*i)->HoldFriends( m_CurrentGame );

		if( (*i)->GetHoldClan( ) )
			(*i)->HoldClan( m_CurrentGame );
	}
        
        m_Callables.push_back(m_DB->ThreadedUpdateGameInfo(m_NewGameId, gameName));
        m_NewGameId = 0;
}

void CGHost :: ParseConfigValues( map<string, string> configs )
{
    typedef map<string, string>::iterator config_iterator;
    
    for(config_iterator iterator = configs.begin(); iterator != configs.end(); iterator++)
    {
        if(iterator->first == "bot_language") {
            delete m_Language;
            m_Language = new CLanguage( iterator->second );
        } else if(iterator->first == "bot_tft") {
            m_TFT = UTIL_ToUInt32(iterator->second) == 0 ? false : true;
        } else if(iterator->first == "bot_bindaddress") {
            m_BindAddress = iterator->second;
        } else if(iterator->first == "bot_hostport") {
            m_HostPort = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "bot_reconnect") {
            m_Reconnect = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "bot_reconnectport") {
            m_ReconnectPort = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "bot_reconnectwaittime") {
            m_ReconnectWaitTime = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "bot_maxgames") {
            m_MaxGames = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "bot_commandtrigger") {
            m_CommandTrigger = iterator->second[0];
        } else if(iterator->first == "bot_mapcfgpath") {
            m_MapCFGPath = iterator->second;
        } else if(iterator->first == "bot_savegamepath") {
            m_SaveGamePath = iterator->second;
        } else if(iterator->first == "bot_mappath") {
            m_MapPath = UTIL_AddPathSeperator(iterator->second);
        } else if(iterator->first == "bot_savereplays") {
            m_SaveReplays = UTIL_ToUInt32(iterator->second) == 0 ? false : true;
        } else if(iterator->first == "bot_replaypath") {
            m_ReplayPath = iterator->second;
        } else if(iterator->first == "replay_war3version") {
            m_ReplayWar3Version = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "replay_buildnumber") {
            m_ReplayBuildNumber = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "bot_virtualhostname") {
            m_VirtualHostName = iterator->second;
        } else if(iterator->first == "bot_hideipaddresses") {
            m_HideIPAddresses = UTIL_ToUInt32(iterator->second) == 0 ? false : true;
        } else if(iterator->first == "bot_checkmultipleipusage") {
            m_CheckMultipleIPUsage = UTIL_ToUInt32(iterator->second) == 0 ? false : true;
        } else if(iterator->first == "bot_spoofchecks") {
            m_SpoofChecks = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "bot_requirespoofchecks") {
            m_RequireSpoofChecks = UTIL_ToUInt32(iterator->second) == 0 ? false : true;
        } else if(iterator->first == "bot_reserveadmins") {
            m_ReserveAdmins = UTIL_ToUInt32(iterator->second) == 0 ? false : true;
        } else if(iterator->first == "bot_refreshmessages") {
            m_RefreshMessages = UTIL_ToUInt32(iterator->second) == 0 ? false : true;
        } else if(iterator->first == "bot_autolock") {
            m_AutoLock = UTIL_ToUInt32(iterator->second) == 0 ? false : true;
        } else if(iterator->first == "bot_autosave") {
            m_AutoSave = UTIL_ToUInt32(iterator->second) == 0 ? false : true;
        } else if(iterator->first == "bot_allowdownloads") {
            m_AllowDownloads = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "bot_pingduringdownloads") {
            m_PingDuringDownloads = UTIL_ToUInt32(iterator->second) == 0 ? false : true;
        } else if(iterator->first == "bot_maxdownloaders") {
            m_MaxDownloaders = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "bot_maxdownloadspeed") {
            m_MaxDownloadSpeed = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "bot_lcpings") {
            m_LCPings = UTIL_ToUInt32(iterator->second) == 0 ? false : true;
        } else if(iterator->first == "bot_autokickping") {
            m_AutoKickPing = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "bot_banmethod") {
            m_BanMethod = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "bot_ipblacklistfile") {
            m_IPBlackListFile = iterator->second;
        } else if(iterator->first == "bot_lobbytimelimit") {
            m_LobbyTimeLimit = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "bot_latency") {
            m_Latency = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "bot_synclimit") {
            m_SyncLimit = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "bot_votekickallowed") {
            m_VoteKickAllowed = UTIL_ToUInt32(iterator->second) == 0 ? false : true;
        } else if(iterator->first == "bot_votekickpercentage") {
            m_VoteKickPercentage = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "bot_defaultmap") {
            m_DefaultMap = iterator->second;
        } else if(iterator->first == "tcp_nodelay") {
            m_TCPNoDelay = UTIL_ToUInt32(iterator->second) == 0 ? false : true;
        } else if(iterator->first == "bot_matchmakingmethod") {
            m_MatchMakingMethod = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "lan_war3version") {
            m_LANWar3Version = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "udp_broadcasttarget") {
            m_UDPSocket = new CUDPSocket( );
            m_UDPSocket->SetBroadcastTarget( iterator->second );
        } else if(iterator->first == "udp_dontroute") {
            m_UDPSocket->SetDontRoute( UTIL_ToUInt32(iterator->second) );
        } else if(iterator->first == "autohost_maxgames") {
            m_AutoHostMaximumGames = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "autohost_startplayers") {
            m_AutoHostAutoStartPlayers = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "autohost_gamename") {
            m_AutoHostGameName = iterator->second;
        } else if(iterator->first == "autohost_owner") {
            m_AutoHostOwner = iterator->second;
        } else if(iterator->first.substr(0, 4) == "bnet") {
            int bnetNumber = 0;
            int pos = 5;
            
            if(iterator->first.substr(4, 1) != "_") {
                string swaggynumber = iterator->first.substr(4, 1);
                bnetNumber = UTIL_ToUInt32(swaggynumber);
                pos = 6;
            }
            m_BNetCollection[bnetNumber][iterator->first.substr(pos)] = iterator->second;
        } else if(iterator->first == "ohs_alias_id") {
            m_AliasId = UTIL_ToUInt32(iterator->second);
        } else if(iterator->first == "ohs_banlast_time") {
            m_BanLastTime = UTIL_ToUInt32(iterator->second);
	} else if(iterator->first == "ohs_votestart") {
            m_AllowVoteStart = UTIL_ToUInt32(iterator->second) == 0 ? false : true;
	} else if(iterator->first == "ohs_votestart_minplayers") {
            m_VoteStartMinPlayers = UTIL_ToUInt32(iterator->second);
	} else if(iterator->first == "autohost_symbol") {
	    m_AutoHostSplitter = iterator->second;
        } else {
            CONSOLE_Print("Unused!! " + iterator->first + " of value " + iterator->second);
        }
    }
    
    ExtractScripts( );
    ConnectToBNets();
    
    if(! m_CurrentGame) {
        m_CallableGetMapConfig = m_DB->ThreadedGetMapConfig( m_DefaultMap );
    }
}

void CGHost :: ParseConfigTexts( map<string, vector<string>> texts )
{
    typedef map<string, vector<string>>::iterator text_iterator;
    
    for(text_iterator iterator = texts.begin(); iterator != texts.end(); iterator++)
    {
        if(iterator->first == "gameloaded") {
            m_GameLoaded = iterator->second;
        } else if(iterator->first == "gameover") {
            m_GameOver = iterator->second;
        } else if(iterator->first == "motd") {
            m_MOTD = iterator->second;
        } else {
            CONSOLE_Print("Didn't use '" + iterator->first + "' data!");
        }
    }
}


void CGHost :: ConnectToBNets( )
{
    typedef map<int, map<string, string>>::iterator bnet_iterator;
    uint32_t counter = 0;
    for(bnet_iterator i = m_BNetCollection.begin(); i != m_BNetCollection.end(); i++)
    {
	string Server = "";
	string ServerAlias = "";
	string CDKeyROC = "";
	string CDKeyTFT = "";
	string CountryAbbrev = "DE";
	string Country = "Germany";
	string Locale = "1031";
	string UserName = "";
	string UserPassword = "";
	string FirstChannel = "The Void";
	string RootAdmin = "";
	string BNETCommandTrigger = ".";
	bool HoldFriends = false;
	bool HoldClan = false;
	bool PublicCommands = false;
	unsigned char War3Version = 26;
	BYTEARRAY EXEVersion = {};
	BYTEARRAY EXEVersionHash = {};
	string PasswordHashType = "";
 	string PVPGNRealmName = "PvPGN Realm";
 	uint32_t MaxMessageLength = 200;
         
        typedef map<string, string>::iterator options_iterator;
        for(options_iterator j = i->second.begin(); j != i->second.end(); j++)
        {
            size_t pos = j->first.find_first_of("_") != string::npos;
            string key = j->first.substr(pos);
            if(key == "server" ) {
                Server = j->second;
            } else if(key == "serveralias") {
                ServerAlias = j->second;
            } else if(key == "cdkeyroc") {
                CDKeyROC = j->second;
            } else if(key == "cdkeytft") {
                CDKeyTFT = j->second;
            } else if(key == "countryabbrev") {
                CountryAbbrev = j->second;
            } else if(key == "country") {
                Country = j->second;
            } else if(key == "locale") {
                Locale = j->second;
            } else if(key == "username") {
                UserName = j->second;
            } else if(key == "password") {
                UserPassword = j->second;
            } else if(key == "firstchannel") {
                FirstChannel = j->second;
            } else if(key == "rootadmin") {
                RootAdmin = j->second;
            } else if(key == "commandtrigger") {
                BNETCommandTrigger = j->second[0];
            } else if(key == "holdfriends") {
                HoldFriends = UTIL_ToUInt32(j->second) == 0 ? false : true;
            } else if(key == "holdclan") {
                HoldClan = UTIL_ToUInt32(j->second) == 0 ? false : true;
            } else if(key == "publiccommands") {
                PublicCommands = UTIL_ToUInt32(j->second) == 0 ? false : true;
            } else if(key == "customwar3version") {
                War3Version = UTIL_ToUInt32(j->second);
            } else if(key == "customexeversion") {
                EXEVersion = UTIL_ExtractNumbers(j->second, 4);
            } else if(key == "customexeversionhash") {
                EXEVersionHash = UTIL_ExtractNumbers(j->second, 4);
            } else if(key == "custompasswordhashtype") {
                PasswordHashType = j->second;
            } else if(key == "custompvpgnrealmname") {
                PVPGNRealmName = j->second;
            } else if(key == "custommaxmessagelength") {
                MaxMessageLength = UTIL_ToUInt32(j->second);
            } else {
                CONSOLE_Print("UNUSED!! " + j->first + " with " + j->second);
            }
        }
        
        if( Server.empty( ) )
	{
	    CONSOLE_Print("Didn't found 'Server'");
            break;
	}

        if( CDKeyROC.empty( ) )
        {
            CONSOLE_Print( "[GHOST] missing cdkeyroc, skipping this battle.net connection" );
            continue;
        }

        if( m_TFT && CDKeyTFT.empty( ) )
        {
            CONSOLE_Print( "[GHOST] missing cdkeytft, skipping this battle.net connection" );
            continue;
        }

        if( UserName.empty( ) )
        {
            CONSOLE_Print( "[GHOST] missing username, skipping this battle.net connection" );
            continue;
        }

        if( UserPassword.empty( ) )
        {
            CONSOLE_Print( "[GHOST] missing password, skipping this battle.net connection" );
            continue;
        }

        CONSOLE_Print( "[GHOST] found battle.net connection for server [" + Server + "]" );

        m_BNETs.push_back( new CBNET( this, Server, ServerAlias, "", 0, 0, CDKeyROC, CDKeyTFT, CountryAbbrev, Country, UTIL_ToUInt32(Locale), UserName, UserPassword, FirstChannel, RootAdmin, BNETCommandTrigger[0], HoldFriends, HoldClan, PublicCommands, War3Version, EXEVersion, EXEVersionHash, PasswordHashType, PVPGNRealmName, MaxMessageLength, counter) );
        counter++;
    }
    
}
