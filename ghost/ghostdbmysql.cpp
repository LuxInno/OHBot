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
#include "config.h"
#include "ghostdb.h"
#include "ghostdbmysql.h"

#include <signal.h>

#ifdef WIN32
 #include <winsock.h>
#endif

#include <mysql/mysql.h>
#include <boost/thread.hpp>
#include <initializer_list>

//
// CGHostDBMySQL
//

CGHostDBMySQL :: CGHostDBMySQL( CConfig *CFG ) : CGHostDB( CFG )
{
	m_Server = CFG->GetString( "db_mysql_server", string( ) );
	m_Database = CFG->GetString( "db_mysql_database", "ghost" );
	m_User = CFG->GetString( "db_mysql_user", string( ) );
	m_Password = CFG->GetString( "db_mysql_password", string( ) );
	m_Port = CFG->GetInt( "db_mysql_port", 0 );
	m_BotID = CFG->GetInt( "db_mysql_botid", 0 );
	m_NumConnections = 1;
	m_OutstandingCallables = 0;

	mysql_library_init( 0, NULL, NULL );

	// create the first connection

	CONSOLE_Print( "[MYSQL] connecting to database server" );
	MYSQL *Connection = NULL;

	if( !( Connection = mysql_init( NULL ) ) )
	{
		CONSOLE_Print( string( "[MYSQL] " ) + mysql_error( Connection ) );
		m_HasError = true;
		m_Error = "error initializing MySQL connection";
		return;
	}

	my_bool Reconnect = true;
	mysql_options( Connection, MYSQL_OPT_RECONNECT, &Reconnect );

	if( !( mysql_real_connect( Connection, m_Server.c_str( ), m_User.c_str( ), m_Password.c_str( ), m_Database.c_str( ), m_Port, NULL, 0 ) ) )
	{
		CONSOLE_Print( string( "[MYSQL] " ) + mysql_error( Connection ) );
		m_HasError = true;
		m_Error = "error connecting to MySQL server";
		return;
	}

	m_IdleConnections.push( Connection );
}

CGHostDBMySQL :: ~CGHostDBMySQL( )
{
	CONSOLE_Print( "[MYSQL] closing " + UTIL_ToString( m_IdleConnections.size( ) ) + "/" + UTIL_ToString( m_NumConnections ) + " idle MySQL connections" );

	while( !m_IdleConnections.empty( ) )
	{
		mysql_close( (MYSQL *)m_IdleConnections.front( ) );
		m_IdleConnections.pop( );
	}

	if( m_OutstandingCallables > 0 )
		CONSOLE_Print( "[MYSQL] " + UTIL_ToString( m_OutstandingCallables ) + " outstanding callables were never recovered" );

	mysql_library_end( );
}

string CGHostDBMySQL :: GetStatus( )
{
	return "DB STATUS --- Connections: " + UTIL_ToString( m_IdleConnections.size( ) ) + "/" + UTIL_ToString( m_NumConnections ) + " idle. Outstanding callables: " + UTIL_ToString( m_OutstandingCallables ) + ".";
}

void CGHostDBMySQL :: RecoverCallable( CBaseCallable *callable )
{
	CMySQLCallable *MySQLCallable = dynamic_cast<CMySQLCallable *>( callable );

	if( MySQLCallable )
	{
		if( m_IdleConnections.size( ) > 30 )
		{
			mysql_close( (MYSQL *)MySQLCallable->GetConnection( ) );
			m_NumConnections--;
		}
		else
			m_IdleConnections.push( MySQLCallable->GetConnection( ) );

		if( m_OutstandingCallables == 0 )
			CONSOLE_Print( "[MYSQL] recovered a mysql callable with zero outstanding" );
		else
			m_OutstandingCallables--;

		if( !MySQLCallable->GetError( ).empty( ) )
			CONSOLE_Print( "[MYSQL] error --- " + MySQLCallable->GetError( ) );
	}
	else
		CONSOLE_Print( "[MYSQL] tried to recover a non-mysql callable" );
}

void CGHostDBMySQL :: CreateThread( CBaseCallable *callable )
{
	try
	{
		boost :: thread Thread( boost :: ref( *callable ) );
	}
	catch( boost :: thread_resource_error tre )
	{
		CONSOLE_Print( "[MYSQL] error spawning thread on attempt #1 [" + string( tre.what( ) ) + "], pausing execution and trying again in 50ms" );
		MILLISLEEP( 50 );

		try
		{
			boost :: thread Thread( boost :: ref( *callable ) );
		}
		catch( boost :: thread_resource_error tre2 )
		{
			CONSOLE_Print( "[MYSQL] error spawning thread on attempt #2 [" + string( tre2.what( ) ) + "], giving up" );
			callable->SetReady( true );
		}
	}
}

CCallableAdminCount *CGHostDBMySQL :: ThreadedAdminCount( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableAdminCount *Callable = new CMySQLCallableAdminCount( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableAdminCheck *CGHostDBMySQL :: ThreadedAdminCheck( string server, string user )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableAdminCheck *Callable = new CMySQLCallableAdminCheck( server, user, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableAdminAdd *CGHostDBMySQL :: ThreadedAdminAdd( string server, string user )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableAdminAdd *Callable = new CMySQLCallableAdminAdd( server, user, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableAdminRemove *CGHostDBMySQL :: ThreadedAdminRemove( string server, string user )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableAdminRemove *Callable = new CMySQLCallableAdminRemove( server, user, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableAdminList *CGHostDBMySQL :: ThreadedAdminList( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableAdminList *Callable = new CMySQLCallableAdminList( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanCount *CGHostDBMySQL :: ThreadedBanCount( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanCount *Callable = new CMySQLCallableBanCount( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanCheck *CGHostDBMySQL :: ThreadedBanCheck( string server, string user, string ip )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanCheck *Callable = new CMySQLCallableBanCheck( server, user, ip, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanAdd *CGHostDBMySQL :: ThreadedBanAdd( string server, string user, string ip, string gamename, string admin, string reason, uint32_t banlength )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanAdd *Callable = new CMySQLCallableBanAdd( server, user, ip, gamename, admin, reason, banlength, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanRemove *CGHostDBMySQL :: ThreadedBanRemove( string server, string user )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanRemove *Callable = new CMySQLCallableBanRemove( server, user, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanRemove *CGHostDBMySQL :: ThreadedBanRemove( string user )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanRemove *Callable = new CMySQLCallableBanRemove( string( ), user, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableBanList *CGHostDBMySQL :: ThreadedBanList( string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableBanList *Callable = new CMySQLCallableBanList( server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableGameAdd *CGHostDBMySQL :: ThreadedGameAdd( string server, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, string creatorserver , uint32_t gameid, uint32_t aliasid, vector<string> lobbylog, vector<string> gamelog, string elochange)
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableGameAdd *Callable = new CMySQLCallableGameAdd( server, map, gamename, ownername, duration, gamestate, creatorname, creatorserver, gameid, aliasid, lobbylog, gamelog, elochange, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableGamePlayerAdd *CGHostDBMySQL :: ThreadedGamePlayerAdd( uint32_t gameid, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t reserved, uint32_t loadingtime, uint32_t left, string leftreason, uint32_t team, uint32_t colour, uint32_t playerid )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableGamePlayerAdd *Callable = new CMySQLCallableGamePlayerAdd( gameid, name, ip, spoofed, spoofedrealm, reserved, loadingtime, left, leftreason, team, colour, playerid, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableGamePlayerSummaryCheck *CGHostDBMySQL :: ThreadedGamePlayerSummaryCheck( string name )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableGamePlayerSummaryCheck *Callable = new CMySQLCallableGamePlayerSummaryCheck( name, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableDotAGameAdd *CGHostDBMySQL :: ThreadedDotAGameAdd( uint32_t gameid, uint32_t winner, uint32_t min, uint32_t sec )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableDotAGameAdd *Callable = new CMySQLCallableDotAGameAdd( gameid, winner, min, sec, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableDotAPlayerAdd *CGHostDBMySQL :: ThreadedDotAPlayerAdd( uint32_t gameid, uint32_t colour, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t assists, uint32_t gold, uint32_t neutralkills, string item1, string item2, string item3, string item4, string item5, string item6, string skill1, string skill2, string skill3, string skill4, string skill5, string skill6, string hero, uint32_t newcolour, uint32_t towerkills, uint32_t raxkills, uint32_t courierkills, uint32_t level )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableDotAPlayerAdd *Callable = new CMySQLCallableDotAPlayerAdd( gameid, colour, kills, deaths, creepkills, creepdenies, assists, gold, neutralkills, item1, item2, item3, item4, item5, item6, skill1, skill2, skill3, skill4, skill5, skill6, hero, newcolour, towerkills, raxkills, courierkills, level, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableDotAPlayerSummaryCheck *CGHostDBMySQL :: ThreadedDotAPlayerSummaryCheck( string name )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableDotAPlayerSummaryCheck *Callable = new CMySQLCallableDotAPlayerSummaryCheck( name, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableDownloadAdd *CGHostDBMySQL :: ThreadedDownloadAdd( string map, uint32_t mapsize, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t downloadtime )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableDownloadAdd *Callable = new CMySQLCallableDownloadAdd( map, mapsize, name, ip, spoofed, spoofedrealm, downloadtime, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableScoreCheck *CGHostDBMySQL :: ThreadedScoreCheck( string category, string name, string server )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableScoreCheck *Callable = new CMySQLCallableScoreCheck( category, name, server, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableW3MMDPlayerAdd *CGHostDBMySQL :: ThreadedW3MMDPlayerAdd( string category, uint32_t gameid, uint32_t pid, string name, string flag, uint32_t leaver, uint32_t practicing )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableW3MMDPlayerAdd *Callable = new CMySQLCallableW3MMDPlayerAdd( category, gameid, pid, name, flag, leaver, practicing, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableW3MMDVarAdd *CGHostDBMySQL :: ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,int32_t> var_ints )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableW3MMDVarAdd *Callable = new CMySQLCallableW3MMDVarAdd( gameid, var_ints, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableW3MMDVarAdd *CGHostDBMySQL :: ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,double> var_reals )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableW3MMDVarAdd *Callable = new CMySQLCallableW3MMDVarAdd( gameid, var_reals, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableW3MMDVarAdd *CGHostDBMySQL :: ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,string> var_strings )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableW3MMDVarAdd *Callable = new CMySQLCallableW3MMDVarAdd( gameid, var_strings, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableGetPlayerId *CGHostDBMySQL :: ThreadedGetPlayerId( string user )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableGetPlayerId *Callable = new CMySQLCallableGetPlayerId( user, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableCreatePlayerId *CGHostDBMySQL :: ThreadedCreatePlayerId( string user, string ip, string realm )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableCreatePlayerId *Callable = new CMySQLCallableCreatePlayerId( user, ip, realm, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableGetGameId *CGHostDBMySQL :: ThreadedGetGameId( )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableGetGameId *Callable = new CMySQLCallableGetGameId( Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableGetBotConfigs *CGHostDBMySQL :: ThreadedGetBotConfigs( )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableGetBotConfigs *Callable = new CMySQLCallableGetBotConfigs( Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableGetBotConfigTexts *CGHostDBMySQL :: ThreadedGetBotConfigTexts( )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableGetBotConfigTexts *Callable = new CMySQLCallableGetBotConfigTexts( Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableGetLanguages *CGHostDBMySQL :: ThreadedGetLanguages( )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableGetLanguages *Callable = new CMySQLCallableGetLanguages( Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableGetMapConfig *CGHostDBMySQL :: ThreadedGetMapConfig( string m_ConfigName )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableGetMapConfig *Callable = new CMySQLCallableGetMapConfig( m_ConfigName, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableGameUpdate *CGHostDBMySQL :: ThreadedGameUpdate( uint32_t hostcounter, uint32_t lobby, string map_type, uint32_t duration, string gamename, string ownername, string creatorname, string map, uint32_t players, uint32_t total, vector<PlayerOfPlayerList> playerlist )
{
    void *Connection = GetIdleConnection( );

    if( !Connection )
        ++m_NumConnections;

    CCallableGameUpdate *Callable = new CMySQLCallableGameUpdate( hostcounter, lobby, map_type, duration, gamename, ownername, creatorname, map, players, total,  playerlist, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
    CreateThread( Callable );
    ++m_OutstandingCallables;
    return Callable;
}

CCallableGetAliases *CGHostDBMySQL :: ThreadedGetAliases( )
{
	void *Connection = GetIdleConnection( );

	if( !Connection )
		m_NumConnections++;

	CCallableGetAliases *Callable = new CMySQLCallableGetAliases( Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
	CreateThread( Callable );
	m_OutstandingCallables++;
	return Callable;
}

CCallableGetStatsTemplates *CGHostDBMySQL :: ThreadedGetStatsTemplates( )
{
        void *Connection = GetIdleConnection( );

        if( !Connection )
                m_NumConnections++;

        CCallableGetStatsTemplates *Callable = new CMySQLCallableGetStatsTemplates( Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
        CreateThread( Callable );
        m_OutstandingCallables++;
        return Callable;
}

CCallableGetPlayerStats *CGHostDBMySQL :: ThreadedGetPlayerStats( uint32_t aliasid, uint32_t playerid )
{
        void *Connection = GetIdleConnection( );

        if( !Connection )
                m_NumConnections++;

        CCallableGetPlayerStats *Callable = new CMySQLCallableGetPlayerStats( aliasid, playerid, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
        CreateThread( Callable );
        m_OutstandingCallables++;
        return Callable;
}

CCallableGetPlayerScore *CGHostDBMySQL :: ThreadedGetPlayerScore( uint32_t aliasid, uint32_t playerid )
{
        void *Connection = GetIdleConnection( );

        if( !Connection )
                m_NumConnections++;

        CCallableGetPlayerScore *Callable = new CMySQLCallableGetPlayerScore( aliasid, playerid, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
        CreateThread( Callable );
        m_OutstandingCallables++;
        return Callable;
}

CCallableUpdateGameInfo *CGHostDBMySQL :: ThreadedUpdateGameInfo( uint32_t gameid, string gamename )
{
        void *Connection = GetIdleConnection( );

        if( !Connection )
                m_NumConnections++;

        CCallableUpdateGameInfo *Callable = new CMySQLCallableUpdateGameInfo( gameid, gamename, Connection, m_BotID, m_Server, m_Database, m_User, m_Password, m_Port );
        CreateThread( Callable );
        m_OutstandingCallables++;
        return Callable;
}

void *CGHostDBMySQL :: GetIdleConnection( )
{
	void *Connection = NULL;

	if( !m_IdleConnections.empty( ) )
	{
		Connection = m_IdleConnections.front( );
		m_IdleConnections.pop( );
	}

	return Connection;
}

//
// unprototyped global helper functions
//

string MySQLEscapeString( void *conn, string str )
{
	char *to = new char[str.size( ) * 2 + 1];
	unsigned long size = mysql_real_escape_string( (MYSQL *)conn, to, str.c_str( ), str.size( ) );
	string result( to, size );
	delete [] to;
	return result;
}

vector<string> MySQLFetchRow( MYSQL_RES *res )
{
	vector<string> Result;

	MYSQL_ROW Row = mysql_fetch_row( res );

	if( Row )
	{
		unsigned long *Lengths;
		Lengths = mysql_fetch_lengths( res );

		for( unsigned int i = 0; i < mysql_num_fields( res ); i++ )
		{
			if( Row[i] )
				Result.push_back( string( Row[i], Lengths[i] ) );
			else
				Result.push_back( string( ) );
		}
	}

	return Result;
}

//
// global helper functions
//

uint32_t MySQLAdminCount( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	uint32_t Count = 0;
	string Query = "SELECT COUNT(*) FROM admins WHERE server='" + EscServer + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
				Count = UTIL_ToUInt32( Row[0] );
			else
				*error = "error counting admins [" + server + "] - row doesn't have 1 column";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Count;
}

bool MySQLAdminCheck( void *conn, string *error, uint32_t botid, string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	bool IsAdmin = false;
	string Query = "SELECT * FROM admins WHERE server='" + EscServer + "' AND name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( !Row.empty( ) )
				IsAdmin = true;

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return IsAdmin;
}

bool MySQLAdminAdd( void *conn, string *error, uint32_t botid, string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	bool Success = false;
	string Query = "INSERT INTO admins ( botid, server, name ) VALUES ( " + UTIL_ToString( botid ) + ", '" + EscServer + "', '" + EscUser + "' )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

bool MySQLAdminRemove( void *conn, string *error, uint32_t botid, string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	bool Success = false;
	string Query = "DELETE FROM admins WHERE server='" + EscServer + "' AND name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

map<string, uint32_t> MySQLAdminList( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	map<string, uint32_t> AdminList;
	string Query = "SELECT bnet_username, user_level FROM oh_users WHERE user_bnet = 2 AND user_level != 0 ";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( !Row.empty( ) )
			{
                string username = Row[0];
                transform( username.begin( ), username.end( ), username.begin( ), (int(*)(int))tolower );
				AdminList[username] = UTIL_ToUInt32(Row[1]);
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return AdminList;
}

uint32_t MySQLBanCount( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	uint32_t Count = 0;
	string Query = "SELECT COUNT(*) FROM oh_bans WHERE server='" + EscServer + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
				Count = UTIL_ToUInt32( Row[0] );
			else
				*error = "error counting bans [" + server + "] - row doesn't have 1 column";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Count;
}

CDBBan *MySQLBanCheck( void *conn, string *error, uint32_t botid, string server, string user, string ip )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	string EscIP = MySQLEscapeString( conn, ip );
	CDBBan *Ban = NULL;
	string Query;

	if( ip.empty( ) )
		Query = "SELECT name, ip, DATE(date), gamename, admin, reason FROM bans WHERE server='" + EscServer + "' AND name='" + EscUser + "'";
	else
		Query = "SELECT name, ip, DATE(date), gamename, admin, reason FROM bans WHERE (server='" + EscServer + "' AND name='" + EscUser + "') OR ip='" + EscIP + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 6 )
				Ban = new CDBBan( server, Row[0], Row[1], Row[2], Row[3], Row[4], Row[5] );
			/* else
				*error = "error checking ban [" + server + " : " + user + "] - row doesn't have 6 columns"; */

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Ban;
}

bool MySQLBanAdd( void *conn, string *error, uint32_t botid, string server, string user, string ip, string gamename, string admin, string reason, uint32_t banlength )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	string EscIP = MySQLEscapeString( conn, ip );
	string EscGameName = MySQLEscapeString( conn, gamename );
	string EscAdmin = MySQLEscapeString( conn, admin );
	string EscReason = MySQLEscapeString( conn, reason );
	bool Success = false;
	string Query = "INSERT INTO bans ( botid, server, name, ip, date, gamename, admin, reason, expiredate ) VALUES ( " + UTIL_ToString( botid ) + ", '" + EscServer + "', '" + EscUser + "', '" + EscIP + "', CURDATE( ), '" + EscGameName + "', '" + EscAdmin + "', '" + EscReason + "', FROM_UNIXTIME( UNIX_TIMESTAMP( ) + " + UTIL_ToString(banlength) + ") )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

bool MySQLBanRemove( void *conn, string *error, uint32_t botid, string server, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscServer = MySQLEscapeString( conn, server );
	string EscUser = MySQLEscapeString( conn, user );
	bool Success = false;
	string Query = "DELETE FROM bans WHERE server='" + EscServer + "' AND name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

bool MySQLBanRemove( void *conn, string *error, uint32_t botid, string user )
{
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
	string EscUser = MySQLEscapeString( conn, user );
	bool Success = false;
	string Query = "DELETE FROM bans WHERE name='" + EscUser + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

vector<CDBBan *> MySQLBanList( void *conn, string *error, uint32_t botid, string server )
{
	string EscServer = MySQLEscapeString( conn, server );
	vector<CDBBan *> BanList;
	string Query = "SELECT name, ip, DATE(date), gamename, admin, reason FROM oh_bans WHERE expiredate>=NOW() OR expiredate = '0000-00-00 00:00:00';";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( Row.size( ) == 6 )
			{
				BanList.push_back( new CDBBan( server, Row[0], Row[1], Row[2], Row[3], Row[4], Row[5] ) );
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return BanList;
}

uint32_t MySQLGameAdd( void *conn, string *error, uint32_t botid, string server, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, string creatorserver, uint32_t gameid, uint32_t aliasid, vector<string> lobbylog, vector<string> gamelog, string elochange )
{
	string EscServer = MySQLEscapeString( conn, server );
	string EscMap = MySQLEscapeString( conn, map );
	string EscGameName = MySQLEscapeString( conn, gamename );
	string EscOwnerName = MySQLEscapeString( conn, ownername );
	string EscCreatorName = MySQLEscapeString( conn, creatorname );
	string EscCreatorServer = MySQLEscapeString( conn, creatorserver );
	string EscEloChange = MySQLEscapeString( conn, elochange );
	string Query = "UPDATE oh_games SET  `gamestatus` = '1', server='" + EscServer + "', map='" + EscMap + "', datetime=NOW(), gamename='" + EscGameName + "', ownername='" + EscOwnerName + "', duration='" + UTIL_ToString( duration ) + "', gamestate='" + UTIL_ToString( gamestate ) + "', creatorname='" + EscCreatorName + "', creatorserver='" + EscCreatorName + "', alias_id='" + UTIL_ToString(aliasid) + "', elochange = '" + EscEloChange + "' WHERE id='" + UTIL_ToString(gameid) + "';";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );

        string LobbyLog;
        for( vector<string> :: iterator i = lobbylog.begin( ); i != lobbylog.end( ); i++ )
            LobbyLog.append( (*i) + '\n' );

        string EscLobbyLog = MySQLEscapeString( conn, LobbyLog );

        string GameLog;
        for( vector<string> :: iterator i = gamelog.begin( ); i != gamelog.end( ); i++ )
            GameLog.append( (*i) + '\n' );

        string EscGameLog = MySQLEscapeString( conn, GameLog );

        string InsertQ = "INSERT INTO oh_lobby_game_logs ( gameid, botid, lobbylog, gamelog ) VALUES ( "+UTIL_ToString(gameid)+", "+UTIL_ToString(botid)+", '"+EscLobbyLog+"', '"+EscGameLog+"' )";

        if( mysql_real_query( (MYSQL *)conn, InsertQ.c_str( ), InsertQ.size( ) ) != 0 )
            *error = mysql_error( (MYSQL *)conn );

        lobbylog.clear( );
        gamelog.clear( );
        
	return gameid;
}

uint32_t MySQLGamePlayerAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t reserved, uint32_t loadingtime, uint32_t left, string leftreason, uint32_t team, uint32_t colour, uint32_t playerid )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	uint32_t RowID = 0;
	string EscName = MySQLEscapeString( conn, name );
	string EscIP = MySQLEscapeString( conn, ip );
	string EscSpoofedRealm = MySQLEscapeString( conn, spoofedrealm );
	string EscLeftReason = MySQLEscapeString( conn, leftreason );
	string Query = "INSERT INTO oh_gameplayers ( botid, player_id, gameid, name, ip, spoofed, reserved, loadingtime, `left`, leftreason, team, colour, spoofedrealm ) VALUES ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString(playerid) + ", " + UTIL_ToString( gameid ) + ", '" + EscName + "', '" + EscIP + "', " + UTIL_ToString( spoofed ) + ", " + UTIL_ToString( reserved ) + ", " + UTIL_ToString( loadingtime ) + ", " + UTIL_ToString( left ) + ", '" + EscLeftReason + "', " + UTIL_ToString( team ) + ", " + UTIL_ToString( colour ) + ", '" + EscSpoofedRealm + "' )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		RowID = mysql_insert_id( (MYSQL *)conn );

	return RowID;
}

CDBGamePlayerSummary *MySQLGamePlayerSummaryCheck( void *conn, string *error, uint32_t botid, string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	string EscName = MySQLEscapeString( conn, name );
	CDBGamePlayerSummary *GamePlayerSummary = NULL;
	string Query = "SELECT MIN(DATE(datetime)), MAX(DATE(datetime)), COUNT(*), MIN(loadingtime), AVG(loadingtime), MAX(loadingtime), MIN(`left`/duration)*100, AVG(`left`/duration)*100, MAX(`left`/duration)*100, MIN(duration), AVG(duration), MAX(duration) FROM gameplayers LEFT JOIN games ON games.id=gameid WHERE LOWER(name)='" + EscName + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 12 )
			{
				string FirstGameDateTime = Row[0];
				string LastGameDateTime = Row[1];
				uint32_t TotalGames = UTIL_ToUInt32( Row[2] );
				uint32_t MinLoadingTime = UTIL_ToUInt32( Row[3] );
				uint32_t AvgLoadingTime = UTIL_ToUInt32( Row[4] );
				uint32_t MaxLoadingTime = UTIL_ToUInt32( Row[5] );
				uint32_t MinLeftPercent = UTIL_ToUInt32( Row[6] );
				uint32_t AvgLeftPercent = UTIL_ToUInt32( Row[7] );
				uint32_t MaxLeftPercent = UTIL_ToUInt32( Row[8] );
				uint32_t MinDuration = UTIL_ToUInt32( Row[9] );
				uint32_t AvgDuration = UTIL_ToUInt32( Row[10] );
				uint32_t MaxDuration = UTIL_ToUInt32( Row[11] );
				GamePlayerSummary = new CDBGamePlayerSummary( string( ), name, FirstGameDateTime, LastGameDateTime, TotalGames, MinLoadingTime, AvgLoadingTime, MaxLoadingTime, MinLeftPercent, AvgLeftPercent, MaxLeftPercent, MinDuration, AvgDuration, MaxDuration );
			}
			else
				*error = "error checking gameplayersummary [" + name + "] - row doesn't have 12 columns";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return GamePlayerSummary;
}

uint32_t MySQLDotAGameAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, uint32_t winner, uint32_t min, uint32_t sec )
{
	uint32_t RowID = 0;
	string Query = "INSERT INTO oh_dotagames ( botid, gameid, winner, min, sec ) VALUES ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( winner ) + ", " + UTIL_ToString( min ) + ", " + UTIL_ToString( sec ) + " )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		RowID = mysql_insert_id( (MYSQL *)conn );

	return RowID;
}

uint32_t MySQLDotAPlayerAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, uint32_t colour, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t assists, uint32_t gold, uint32_t neutralkills, string item1, string item2, string item3, string item4, string item5, string item6, string skill1, string skill2, string skill3, string skill4, string skill5, string skill6, string hero, uint32_t newcolour, uint32_t towerkills, uint32_t raxkills, uint32_t courierkills, uint32_t level )
{
	uint32_t RowID = 0;
	string EscItem1 = MySQLEscapeString( conn, item1 );
	string EscItem2 = MySQLEscapeString( conn, item2 );
	string EscItem3 = MySQLEscapeString( conn, item3 );
	string EscItem4 = MySQLEscapeString( conn, item4 );
	string EscItem5 = MySQLEscapeString( conn, item5 );
	string EscItem6 = MySQLEscapeString( conn, item6 );
	string EscSkill1 = MySQLEscapeString( conn, skill1 );
	string EscSkill2 = MySQLEscapeString( conn, skill2 );
	string EscSkill3 = MySQLEscapeString( conn, skill3 );
	string EscSkill4 = MySQLEscapeString( conn, skill4 );
	string EscSkill5 = MySQLEscapeString( conn, skill5 );
	string EscSkill6 = MySQLEscapeString( conn, skill6 );
	string EscHero = MySQLEscapeString( conn, hero );
	string Query = "INSERT INTO oh_dotaplayers ( botid, gameid, colour, kills, deaths, creepkills, creepdenies, assists, gold, neutralkills, item1, item2, item3, item4, item5, item6, spell1, spell2, spell3, spell4, spell5, spell6, hero, newcolour, towerkills, raxkills, courierkills, level ) VALUES ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( colour ) + ", " + UTIL_ToString( kills ) + ", " + UTIL_ToString( deaths ) + ", " + UTIL_ToString( creepkills ) + ", " + UTIL_ToString( creepdenies ) + ", " + UTIL_ToString( assists ) + ", " + UTIL_ToString( gold ) + ", " + UTIL_ToString( neutralkills ) + ", '" + EscItem1 + "', '" + EscItem2 + "', '" + EscItem3 + "', '" + EscItem4 + "', '" + EscItem5 + "', '" + EscItem6 + "', '" + EscSkill1 + "', '" + EscSkill2 + "', '" + EscSkill3 + "', '" + EscSkill4 + "', '" + EscSkill5 + "', '" + EscSkill6 + "', '" + EscHero + "', " + UTIL_ToString( newcolour ) + ", " + UTIL_ToString( towerkills ) + ", " + UTIL_ToString( raxkills ) + ", " + UTIL_ToString( courierkills ) + ", " + UTIL_ToString( level ) + " )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		RowID = mysql_insert_id( (MYSQL *)conn );

	return RowID;
}

CDBDotAPlayerSummary *MySQLDotAPlayerSummaryCheck( void *conn, string *error, uint32_t botid, string name )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	string EscName = MySQLEscapeString( conn, name );
	CDBDotAPlayerSummary *DotAPlayerSummary = NULL;
	string Query = "SELECT COUNT(dotaplayers.id), SUM(kills), SUM(deaths), SUM(creepkills), SUM(creepdenies), SUM(assists), SUM(neutralkills), SUM(towerkills), SUM(raxkills), SUM(courierkills) FROM gameplayers LEFT JOIN games ON games.id=gameplayers.gameid LEFT JOIN dotaplayers ON dotaplayers.gameid=games.id AND dotaplayers.colour=gameplayers.colour WHERE LOWER(name)='" + EscName + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 10 )
			{
				uint32_t TotalGames = UTIL_ToUInt32( Row[0] );

				if( TotalGames > 0 )
				{
					uint32_t TotalWins = 0;
					uint32_t TotalLosses = 0;
					uint32_t TotalKills = UTIL_ToUInt32( Row[1] );
					uint32_t TotalDeaths = UTIL_ToUInt32( Row[2] );
					uint32_t TotalCreepKills = UTIL_ToUInt32( Row[3] );
					uint32_t TotalCreepDenies = UTIL_ToUInt32( Row[4] );
					uint32_t TotalAssists = UTIL_ToUInt32( Row[5] );
					uint32_t TotalNeutralKills = UTIL_ToUInt32( Row[6] );
					uint32_t TotalTowerKills = UTIL_ToUInt32( Row[7] );
					uint32_t TotalRaxKills = UTIL_ToUInt32( Row[8] );
					uint32_t TotalCourierKills = UTIL_ToUInt32( Row[9] );

					// calculate total wins

					string Query2 = "SELECT COUNT(*) FROM gameplayers LEFT JOIN games ON games.id=gameplayers.gameid LEFT JOIN dotaplayers ON dotaplayers.gameid=games.id AND dotaplayers.colour=gameplayers.colour LEFT JOIN dotagames ON games.id=dotagames.gameid WHERE name='" + EscName + "' AND ((winner=1 AND dotaplayers.newcolour>=1 AND dotaplayers.newcolour<=5) OR (winner=2 AND dotaplayers.newcolour>=7 AND dotaplayers.newcolour<=11))";

					if( mysql_real_query( (MYSQL *)conn, Query2.c_str( ), Query2.size( ) ) != 0 )
						*error = mysql_error( (MYSQL *)conn );
					else
					{
						MYSQL_RES *Result2 = mysql_store_result( (MYSQL *)conn );

						if( Result2 )
						{
							vector<string> Row2 = MySQLFetchRow( Result2 );

							if( Row2.size( ) == 1 )
								TotalWins = UTIL_ToUInt32( Row2[0] );
							else
								*error = "error checking dotaplayersummary wins [" + name + "] - row doesn't have 1 column";

							mysql_free_result( Result2 );
						}
						else
							*error = mysql_error( (MYSQL *)conn );
					}

					// calculate total losses

					string Query3 = "SELECT COUNT(*) FROM gameplayers LEFT JOIN games ON games.id=gameplayers.gameid LEFT JOIN dotaplayers ON dotaplayers.gameid=games.id AND dotaplayers.colour=gameplayers.colour LEFT JOIN dotagames ON games.id=dotagames.gameid WHERE name='" + EscName + "' AND ((winner=2 AND dotaplayers.newcolour>=1 AND dotaplayers.newcolour<=5) OR (winner=1 AND dotaplayers.newcolour>=7 AND dotaplayers.newcolour<=11))";

					if( mysql_real_query( (MYSQL *)conn, Query3.c_str( ), Query3.size( ) ) != 0 )
						*error = mysql_error( (MYSQL *)conn );
					else
					{
						MYSQL_RES *Result3 = mysql_store_result( (MYSQL *)conn );

						if( Result3 )
						{
							vector<string> Row3 = MySQLFetchRow( Result3 );

							if( Row3.size( ) == 1 )
								TotalLosses = UTIL_ToUInt32( Row3[0] );
							else
								*error = "error checking dotaplayersummary losses [" + name + "] - row doesn't have 1 column";

							mysql_free_result( Result3 );
						}
						else
							*error = mysql_error( (MYSQL *)conn );
					}

					// done

					DotAPlayerSummary = new CDBDotAPlayerSummary( string( ), name, TotalGames, TotalWins, TotalLosses, TotalKills, TotalDeaths, TotalCreepKills, TotalCreepDenies, TotalAssists, TotalNeutralKills, TotalTowerKills, TotalRaxKills, TotalCourierKills );
				}
			}
			else
				*error = "error checking dotaplayersummary [" + name + "] - row doesn't have 10 columns";

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return DotAPlayerSummary;
}

bool MySQLDownloadAdd( void *conn, string *error, uint32_t botid, string map, uint32_t mapsize, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t downloadtime )
{
	bool Success = false;
	string EscMap = MySQLEscapeString( conn, map );
	string EscName = MySQLEscapeString( conn, name );
	string EscIP = MySQLEscapeString( conn, ip );
	string EscSpoofedRealm = MySQLEscapeString( conn, spoofedrealm );
	string Query = "INSERT INTO oh_downloads ( botid, map, mapsize, datetime, name, ip, spoofed, spoofedrealm, downloadtime ) VALUES ( " + UTIL_ToString( botid ) + ", '" + EscMap + "', " + UTIL_ToString( mapsize ) + ", NOW( ), '" + EscName + "', '" + EscIP + "', " + UTIL_ToString( spoofed ) + ", '" + EscSpoofedRealm + "', " + UTIL_ToString( downloadtime ) + " )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

double MySQLScoreCheck( void *conn, string *error, uint32_t botid, string category, string name, string server )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	string EscCategory = MySQLEscapeString( conn, category );
	string EscName = MySQLEscapeString( conn, name );
	string EscServer = MySQLEscapeString( conn, server );
	double Score = -100000.0;
	string Query = "SELECT score FROM scores WHERE category='" + EscCategory + "' AND name='" + EscName + "' AND server='" + EscServer + "'";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
	{
		MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			if( Row.size( ) == 1 )
				Score = UTIL_ToDouble( Row[0] );
			/* else
				*error = "error checking score [" + category + " : " + name + " : " + server + "] - row doesn't have 1 column"; */

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
	}

	return Score;
}

uint32_t MySQLW3MMDPlayerAdd( void *conn, string *error, uint32_t botid, string category, uint32_t gameid, uint32_t pid, string name, string flag, uint32_t leaver, uint32_t practicing )
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	uint32_t RowID = 0;
	string EscCategory = MySQLEscapeString( conn, category );
	string EscName = MySQLEscapeString( conn, name );
	string EscFlag = MySQLEscapeString( conn, flag );
	string Query = "INSERT INTO oh_w3mmdplayers ( botid, category, gameid, pid, name, flag, leaver, practicing ) VALUES ( " + UTIL_ToString( botid ) + ", '" + EscCategory + "', " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( pid ) + ", '" + EscName + "', '" + EscFlag + "', " + UTIL_ToString( leaver ) + ", " + UTIL_ToString( practicing ) + " )";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		RowID = mysql_insert_id( (MYSQL *)conn );

	return RowID;
}

bool MySQLW3MMDVarAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, map<VarP,int32_t> var_ints )
{
	if( var_ints.empty( ) )
		return false;

	bool Success = false;
	string Query;

	for( map<VarP,int32_t> :: iterator i = var_ints.begin( ); i != var_ints.end( ); i++ )
	{
		string EscVarName = MySQLEscapeString( conn, i->first.second );

		if( Query.empty( ) )
			Query = "INSERT INTO oh_w3mmdvars ( botid, gameid, pid, varname, value_int ) VALUES ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', " + UTIL_ToString( i->second ) + " )";
		else
			Query += ", ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', " + UTIL_ToString( i->second ) + " )";
	}

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

bool MySQLW3MMDVarAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, map<VarP,double> var_reals )
{
	if( var_reals.empty( ) )
		return false;

	bool Success = false;
	string Query;

	for( map<VarP,double> :: iterator i = var_reals.begin( ); i != var_reals.end( ); i++ )
	{
		string EscVarName = MySQLEscapeString( conn, i->first.second );

		if( Query.empty( ) )
			Query = "INSERT INTO oh_w3mmdvars ( botid, gameid, pid, varname, value_real ) VALUES ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', " + UTIL_ToString( i->second, 10 ) + " )";
		else
			Query += ", ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', " + UTIL_ToString( i->second, 10 ) + " )";
	}

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

bool MySQLW3MMDVarAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, map<VarP,string> var_strings )
{
	if( var_strings.empty( ) )
		return false;

	bool Success = false;
	string Query;

	for( map<VarP,string> :: iterator i = var_strings.begin( ); i != var_strings.end( ); i++ )
	{
		string EscVarName = MySQLEscapeString( conn, i->first.second );
		string EscValueString = MySQLEscapeString( conn, i->second );

		if( Query.empty( ) )
			Query = "INSERT INTO oh_w3mmdvars ( botid, gameid, pid, varname, value_string ) VALUES ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', '" + EscValueString + "' )";
		else
			Query += ", ( " + UTIL_ToString( botid ) + ", " + UTIL_ToString( gameid ) + ", " + UTIL_ToString( i->first.first ) + ", '" + EscVarName + "', '" + EscValueString + "' )";
	}

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
	else
		Success = true;

	return Success;
}

uint32_t MySQLGetPlayerId( void *conn, string *error, uint32_t botid, string user )
{
    transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
    string EscLowerName = MySQLEscapeString( conn, user );
    uint32_t RowID = 0;
    string Query = "SELECT id FROM oh_stats_players WHERE player_lower = '" + EscLowerName + "'";

    if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
        *error = mysql_error( (MYSQL *)conn );
    else
    {
        MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

        if( Result )
        {
            vector<string> Row = MySQLFetchRow( Result );

            if( Row.size( ) == 1 )
            {
                RowID = UTIL_ToUInt32(Row[0]);
                Row = MySQLFetchRow( Result );
            }

            mysql_free_result( Result );
        }
        else
        {
            CONSOLE_Print(user + " has no id yet, creating one.");
        }
    }

    return RowID;
}

uint32_t MySQLCreatePlayerId( void *conn, string *error, uint32_t botid, string user, string ip, string realm )
{
    string EscName = MySQLEscapeString( conn, user );
	transform( user.begin( ), user.end( ), user.begin( ), (int(*)(int))tolower );
    string EscLowerName = MySQLEscapeString( conn, user );
    string EscIP = MySQLEscapeString( conn, ip );
    string EscRealm = MySQLEscapeString( conn, realm );
    
    uint32_t RowID = 0;
    string Query = "INSERT INTO oh_stats_players (player, player_lower, ip, realm, player_language) VALUES ('"+EscName+"','"+EscLowerName+"','"+EscIP+"','"+EscRealm+"', 'en');";

    if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
        *error = mysql_error( (MYSQL *)conn );
    else
        RowID = mysql_insert_id( (MYSQL *)conn );
    
    return RowID;
}

uint32_t MySQLGetGameId( void *conn, string *error, uint32_t botid )
{
	uint32_t RowID = 0;
        
        string SelectQuery = "SELECT id FROM oh_games WHERE gamename = 'RESERVED' AND gamestatus = 0 AND botid=" + UTIL_ToString(botid)+";";
        
        if( mysql_real_query( (MYSQL *)conn, SelectQuery.c_str( ), SelectQuery.size( ) ) != 0 )
                *error = mysql_error( (MYSQL *)conn );
        else {
            MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

            if( Result )
            {
                vector<string> Row = MySQLFetchRow( Result );


                if( Row.size( ) == 1 )
                {
                        RowID = UTIL_ToUInt32(Row[0]);
                        Row = MySQLFetchRow( Result );
                }

                mysql_free_result( Result );
            }
            else
                *error = mysql_error( (MYSQL *)conn );
        }

        if(RowID == 0) {
            string Query = "INSERT INTO oh_games (botid, gamename, gamestatus, datetime) VALUES (" + UTIL_ToString(botid) + ", 'RESERVED', 0, CURRENT_TIMESTAMP());";
            if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
                    *error = mysql_error( (MYSQL *)conn );
            else
                    RowID = mysql_insert_id( (MYSQL *)conn );
        }

	return RowID;
}

map<string, string> MySQLGetBotConfigs( void *conn, string *error, uint32_t botid )
{
    map<string, string> m_Configs;
	string Query = "SELECT cfg_name, cfg_value FROM oh_bot_configuration WHERE cfg_botid='" + UTIL_ToString(botid) + "';";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
    else
    {
        MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

        if( Result )
        {
            vector<string> Row = MySQLFetchRow( Result );


            while( Row.size( ) == 2 )
            {
                    m_Configs[Row[0]] = Row[1];
                    Row = MySQLFetchRow( Result );
            }

            mysql_free_result( Result );
        }
        else
            *error = mysql_error( (MYSQL *)conn );
    }

	return m_Configs;
}

map<string, vector<string> > MySQLGetBotConfigTexts( void *conn, string *error, uint32_t botid )
{
    map<string, vector<string>> m_ConfigTexts;
    vector<string> texts;
	string Query = "SELECT cfg_name, cfg_value FROM oh_bot_configuration WHERE cfg_botid='" + UTIL_ToString(botid) + "' AND (cfg_name='motd' OR cfg_name='gameloaded' OR cfg_name='gameover');";

	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
    else
    {
        MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );
            
            
			while( Row.size( ) == 2 )
			{
                texts.clear();
                
                std::stringstream ss(Row[1]);
                std::string line;
                while(std::getline(ss,line,'\n')){
                    texts.push_back(line);
                }
				m_ConfigTexts[Row[0]] = texts;
                
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
    }

	return m_ConfigTexts;
}

map<string, map<uint32_t, string> > MySQLGetLanguages( void *conn, string *error, uint32_t botid )
{
    map<string, map<uint32_t, string>> m_Languages;
	string Query = "SELECT translation_id, language_code, translation FROM oh_lang_trasnlation";
    
	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
    else
    {
        MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );
            
            
			while( Row.size( ) == 3 )
			{
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
    }

	return m_Languages;
}

map<string, string> MySQLGetMapConfig( void *conn, string *error, uint32_t botid, string configname )
{
	transform( configname.begin( ), configname.end( ), configname.begin( ), (int(*)(int))tolower );
    string EscConfigName = MySQLEscapeString( conn, configname );
    map<string, string> m_Configs;
	string Query = "SELECT cfg_key, cfg_value FROM oh_mapcfgs WHERE cfg_name ='" + EscConfigName + "';";
    
	if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
		*error = mysql_error( (MYSQL *)conn );
    else
    {
        MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );
            
            
			while( Row.size( ) == 2 )
			{
                m_Configs[Row[0]] = Row[1];
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
    }

	return m_Configs;
}

string MySQLGameUpdate( void *conn, string *error, uint32_t botid, uint32_t hostcounter, uint32_t lobby, string map_type, uint32_t duration, string gamename, string ownername, string creatorname, string map, uint32_t players, uint32_t total, vector<PlayerOfPlayerList> playerlist )
{
    if( !gamename.empty( ) ) {
        string EscMapType = MySQLEscapeString( conn, map_type );
        string EscGameName = MySQLEscapeString( conn, gamename );
        string EscOwnerName = MySQLEscapeString( conn, ownername );
        string EscCreatorName = MySQLEscapeString( conn, creatorname );
        string EscMap = MySQLEscapeString( conn, map );
        string Users ="";
        string Splitter =",";
        string PlayerSlpitter="#";
        for( vector<PlayerOfPlayerList> :: iterator i = playerlist.begin( ); i != playerlist.end( ); ++i ) {
            Users += UTIL_ToString(i->Slot)+Splitter+UTIL_ToString(i->Team)+Splitter+UTIL_ToString(i->Color)+Splitter+i->Username+Splitter+i->Realm+Splitter+UTIL_ToString(i->Ping)+Splitter+i->IP+Splitter+UTIL_ToString(i->LeftTime)+Splitter+i->LeftReason+PlayerSlpitter;
        }
        string EscPlayerList = MySQLEscapeString( conn, Users );

        string Query = "INSERT INTO oh_gamelist (botid, gameid, lobby, map_type, gamename, ownername, creatorname, map) VALUES ('" + UTIL_ToString( botid ) + "', '" + UTIL_ToString( hostcounter ) + "', '" + UTIL_ToString( lobby ) + "', '" + EscMapType + "', '" + EscGameName + "', '" + EscOwnerName + "', '" + EscCreatorName + "', '" + EscMap + "') ON DUPLICATE KEY UPDATE lobby = '" + UTIL_ToString( lobby ) + "', duration = '" + UTIL_ToString( duration ) + "', ownername = '" + EscOwnerName + "', players = '" + UTIL_ToString( players ) + "', total = '" + UTIL_ToString( total ) + "', users = '" + EscPlayerList + "'";

        if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
            *error = mysql_error( (MYSQL *)conn );
    } else {
        string Query = "DELETE FROM oh_gamelist WHERE botid = " + UTIL_ToString( botid ) + " AND ( gameid = " + UTIL_ToString( hostcounter ) + " OR lobby = 1 )";

        if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
            *error = mysql_error( (MYSQL *)conn );
    }
    return "";
}

map<uint32_t, string> MySQLGetAliases( void *conn, string *error, uint32_t botid )
{
    map<uint32_t, string> m_Aliases;
    string Query = "SELECT alias_id, alias_name FROM oh_aliases;";

    if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
            *error = mysql_error( (MYSQL *)conn );
    else
    {
        MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );
            
			while( Row.size( ) == 2 )
			{
                m_Aliases[UTIL_ToUInt32(Row[0])] = Row[1];
				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
			*error = mysql_error( (MYSQL *)conn );
    }

    return m_Aliases;
}

map<uint32_t, string> MySQLGetStatsTemplates( void *conn, string *error, uint32_t botid )
{
    map<uint32_t, string> m_Templates;
    string Query = "SELECT alias_id, alias_template FROM oh_aliases;";

    if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
       *error = mysql_error( (MYSQL *)conn );
    else
    {
        MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

        if( Result )
        {
            vector<string> Row = MySQLFetchRow( Result );
            while( Row.size( ) == 2 )
            {
		if(Row[1].length() != 0) {
			m_Templates[UTIL_ToUInt32(Row[0])] = Row[1];
		}
                Row = MySQLFetchRow( Result );
            }

            mysql_free_result( Result );
        }
        else
            *error = mysql_error( (MYSQL *)conn );

    }

    return m_Templates;
}

map<string, string> MySQLGetPlayerStats( void *conn, string *error, uint32_t botid, uint32_t aliasid, uint32_t playerid )
{
    map<string, string> m_Stats;
    string Query = "";
    if(aliasid != 0) {
        Query = "SELECT games as 'VALUE_01', score as 'VALUE_02', `wins` as 'VALUE_03', `losses` as 'VALUE_04', `draw` as 'VALUE_05', `kills` as 'VALUE_06', `deaths` as 'VALUE_07', `assists` as 'VALUE_08', `creeps` as 'VALUE_09', `denies` as 'VALUE_10', `neutrals` as 'VALUE_11', `towers` as 'VALUE_12', `rax` as 'VALUE_13', `realm` as 'REALM', `streak` as 'STREAK', `maxstreak` as 'MAXSTREAK', `losingstreak` as 'LOSINGSTREAK', `maxlosingstreak` as 'MAXLOSINGSTREAK', `zerodeaths` as 'ZERODEATHS', TRUNCATE(((`wins`*100)/`games`), 2) AS 'WINPRECENTAGE', player, leaver, `playtime` FROM oh_stats_global WHERE `alias_id` = " + UTIL_ToString(aliasid) + " AND `pid` = " + UTIL_ToString(playerid) + ";";
    } else {
        Query = "SELECT SUM(games) as 'VALUE_01', SUM(score) as 'VALUE_02', SUM(`wins`) as 'VALUE_03', SUM(`losses`) as 'VALUE_04', SUM(`draw`) as 'VALUE_05', SUM(`kills`) as 'VALUE_06', SUM(`deaths`) as 'VALUE_07', SUM(`assists`) as 'VALUE_08', SUM(`creeps`) as 'VALUE_09', SUM(`denies`) as 'VALUE_10', SUM(`neutrals`) as 'VALUE_11', SUM(`towers`) as 'VALUE_12', SUM(`rax`) as 'VALUE_13', `realm` as 'REALM', MAX(`streak`) as 'STREAK', MAX(`maxstreak`) as 'MAXSTREAK', MAX(`losingstreak`) as 'LOSINGSTREAK', MAX(`maxlosingstreak`) as 'MAXLOSINGSTREAK', SUM(`zerodeaths`) as 'ZERODEATHS', TRUNCATE(((SUM(`wins`)*100)/SUM(`games`)), 2) AS 'WINPRECENTAGE', player, SUM(leaver), SUM(`playtime`) FROM oh_stats_global WHERE `pid` = " + UTIL_ToString(playerid) + ";";
    }

    if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
        *error = mysql_error( (MYSQL *)conn );
    else
    {
        MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

        if( Result )
        {
            vector<string> Row = MySQLFetchRow( Result );
            if( Row.size( ) == 23 )
            {
                m_Stats["VALUE_01"] = Row[0];
                m_Stats["VALUE_02"] = Row[1];
                m_Stats["VALUE_03"] = Row[2];
                m_Stats["VALUE_04"] = Row[3];
                m_Stats["VALUE_05"] = Row[4];
                m_Stats["VALUE_06"] = Row[5];
                m_Stats["VALUE_07"] = Row[6];
                m_Stats["VALUE_08"] = Row[7];
                m_Stats["VALUE_09"] = Row[8];
                m_Stats["VALUE_10"] = Row[9];
                m_Stats["VALUE_11"] = Row[10];
                m_Stats["VALUE_12"] = Row[11];
                m_Stats["VALUE_13"] = Row[12];
                m_Stats["REALM"] = Row[13];
                m_Stats["STREAK"] = Row[14];
                m_Stats["MAXSTREAK"] = Row[15];
                m_Stats["LOSINGSTREAK"] = Row[16];
                m_Stats["MAXLOSINGSTREAK"] = Row[17];
                m_Stats["ZERODEATHS"] = Row[18];
                m_Stats["WINPRECENTAGE"] = Row[19];
                m_Stats["NAME"] = Row[20];
                m_Stats["STAYPERCENTAGE"] = Row[21];
                m_Stats["PLAYTIME"] = Row[22];
                
                Row = MySQLFetchRow( Result );
            }

            mysql_free_result( Result );
        }
        else
            *error = mysql_error( (MYSQL *)conn );
    }

    return m_Stats;
}

double MySQLGetPlayerScore( void *conn, string *error, uint32_t botid, uint32_t aliasid, uint32_t playerid )
{
    double m_Score = 1000.00;
    string Query = "SELECT score FROM oh_stats_global WHERE `alias_id` = " + UTIL_ToString(aliasid) + " AND `pid` = " + UTIL_ToString(playerid) + ";";

    if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
        *error = mysql_error( (MYSQL *)conn );
    else
    {
        MYSQL_RES *Result = mysql_store_result( (MYSQL *)conn );

        if( Result )
        {
            vector<string> Row = MySQLFetchRow( Result );
            if( Row.size( ) == 1 )
            {
                m_Score = UTIL_ToDouble(Row[0]);
                Row = MySQLFetchRow( Result );
            }

            mysql_free_result( Result );
        }
        else
            *error = mysql_error( (MYSQL *)conn );
    }

    return m_Score;
}

void MySQLUpdateGameInfo( void *conn, string *error, uint32_t botid, uint32_t gameid, string gamename )
{
    string EscGameName = MySQLEscapeString( conn, gamename );
    string Query = "UPDATE oh_games SET gamename='" + EscGameName + "' WHERE id = '" + UTIL_ToString(gameid) + "';";

    if( mysql_real_query( (MYSQL *)conn, Query.c_str( ), Query.size( ) ) != 0 )
        *error = mysql_error( (MYSQL *)conn );

    return;
}

//
// MySQL Callables
//

void CMySQLCallable :: Init( )
{
	CBaseCallable :: Init( );

#ifndef WIN32
	// disable SIGPIPE since this is (or should be) a new thread and it doesn't inherit the spawning thread's signal handlers
	// MySQL should automatically disable SIGPIPE when we initialize it but we do so anyway here

	signal( SIGPIPE, SIG_IGN );
#endif

	mysql_thread_init( );

	if( !m_Connection )
	{
		if( !( m_Connection = mysql_init( NULL ) ) )
			m_Error = mysql_error( (MYSQL *)m_Connection );

		my_bool Reconnect = true;
		mysql_options( (MYSQL *)m_Connection, MYSQL_OPT_RECONNECT, &Reconnect );

		if( !( mysql_real_connect( (MYSQL *)m_Connection, m_SQLServer.c_str( ), m_SQLUser.c_str( ), m_SQLPassword.c_str( ), m_SQLDatabase.c_str( ), m_SQLPort, NULL, 0 ) ) )
			m_Error = mysql_error( (MYSQL *)m_Connection );
	}
	else if( mysql_ping( (MYSQL *)m_Connection ) != 0 )
		m_Error = mysql_error( (MYSQL *)m_Connection );
}

void CMySQLCallable :: Close( )
{
	mysql_thread_end( );

	CBaseCallable :: Close( );
}

void CMySQLCallableAdminCount :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLAdminCount( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableAdminCheck :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLAdminCheck( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User );

	Close( );
}

void CMySQLCallableAdminAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLAdminAdd( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User );

	Close( );
}

void CMySQLCallableAdminRemove :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLAdminRemove( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User );

	Close( );
}

void CMySQLCallableAdminList :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLAdminList( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableBanCount :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLBanCount( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableBanCheck :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLBanCheck( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User, m_IP );

	Close( );
}

void CMySQLCallableBanAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLBanAdd( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User, m_IP, m_GameName, m_Admin, m_Reason, m_BanTime );

	Close( );
}

void CMySQLCallableBanRemove :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
	{
		if( m_Server.empty( ) )
			m_Result = MySQLBanRemove( m_Connection, &m_Error, m_SQLBotID, m_User );
		else
			m_Result = MySQLBanRemove( m_Connection, &m_Error, m_SQLBotID, m_Server, m_User );
	}

	Close( );
}

void CMySQLCallableBanList :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLBanList( m_Connection, &m_Error, m_SQLBotID, m_Server );

	Close( );
}

void CMySQLCallableGameAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLGameAdd( m_Connection, &m_Error, m_SQLBotID, m_Server, m_Map, m_GameName, m_OwnerName, m_Duration, m_GameState, m_CreatorName, m_CreatorServer, m_GameId, m_AliasId, m_LobbyLog, m_GameLog, m_EloChange );

	Close( );
}

void CMySQLCallableGamePlayerAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLGamePlayerAdd( m_Connection, &m_Error, m_SQLBotID, m_GameID, m_Name, m_IP, m_Spoofed, m_SpoofedRealm, m_Reserved, m_LoadingTime, m_Left, m_LeftReason, m_Team, m_Colour, m_PlayerId );

	Close( );
}

void CMySQLCallableGamePlayerSummaryCheck :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLGamePlayerSummaryCheck( m_Connection, &m_Error, m_SQLBotID, m_Name );

	Close( );
}

void CMySQLCallableDotAGameAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLDotAGameAdd( m_Connection, &m_Error, m_SQLBotID, m_GameID, m_Winner, m_Min, m_Sec );

	Close( );
}

void CMySQLCallableDotAPlayerAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLDotAPlayerAdd( m_Connection, &m_Error, m_SQLBotID, m_GameID, m_Colour, m_Kills, m_Deaths, m_CreepKills, m_CreepDenies, m_Assists, m_Gold, m_NeutralKills, m_Item1, m_Item2, m_Item3, m_Item4, m_Item5, m_Item6, m_Skill1, m_Skill2, m_Skill3, m_Skill4, m_Skill5, m_Skill6, m_Hero, m_NewColour, m_TowerKills, m_RaxKills, m_CourierKills, m_Level );

	Close( );
}

void CMySQLCallableDotAPlayerSummaryCheck :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLDotAPlayerSummaryCheck( m_Connection, &m_Error, m_SQLBotID, m_Name );

	Close( );
}

void CMySQLCallableDownloadAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLDownloadAdd( m_Connection, &m_Error, m_SQLBotID, m_Map, m_MapSize, m_Name, m_IP, m_Spoofed, m_SpoofedRealm, m_DownloadTime );

	Close( );
}

void CMySQLCallableScoreCheck :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLScoreCheck( m_Connection, &m_Error, m_SQLBotID, m_Category, m_Name, m_Server );

	Close( );
}

void CMySQLCallableW3MMDPlayerAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLW3MMDPlayerAdd( m_Connection, &m_Error, m_SQLBotID, m_Category, m_GameID, m_PID, m_Name, m_Flag, m_Leaver, m_Practicing );

	Close( );
}

void CMySQLCallableW3MMDVarAdd :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
	{
		if( m_ValueType == VALUETYPE_INT )
			m_Result = MySQLW3MMDVarAdd( m_Connection, &m_Error, m_SQLBotID, m_GameID, m_VarInts );
		else if( m_ValueType == VALUETYPE_REAL )
			m_Result = MySQLW3MMDVarAdd( m_Connection, &m_Error, m_SQLBotID, m_GameID, m_VarReals );
		else
			m_Result = MySQLW3MMDVarAdd( m_Connection, &m_Error, m_SQLBotID, m_GameID, m_VarStrings );
	}

	Close( );
}

void CMySQLCallableGetPlayerId :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLGetPlayerId( m_Connection, &m_Error, m_SQLBotID, m_User );

	Close( );
}

void CMySQLCallableCreatePlayerId :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLCreatePlayerId( m_Connection, &m_Error, m_SQLBotID, m_User, m_IP, m_Realm );

	Close( );
}

void CMySQLCallableGetGameId :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLGetGameId( m_Connection, &m_Error, m_SQLBotID );

	Close( );
}

void CMySQLCallableGetBotConfigs :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLGetBotConfigs( m_Connection, &m_Error, m_SQLBotID );

	Close( );
}

void CMySQLCallableGetBotConfigTexts :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLGetBotConfigTexts( m_Connection, &m_Error, m_SQLBotID );

	Close( );
}

void CMySQLCallableGetLanguages :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLGetLanguages( m_Connection, &m_Error, m_SQLBotID );

	Close( );
}

void CMySQLCallableGetMapConfig :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLGetMapConfig( m_Connection, &m_Error, m_SQLBotID, m_ConfigName );

	Close( );
}

void CMySQLCallableGameUpdate :: operator( )( )
{
    Init( );

    if( m_Error.empty( ) )
        m_Result = MySQLGameUpdate( m_Connection, &m_Error, m_SQLBotID, m_Hostcounter, m_Lobby, m_MapType, m_Duration, m_GameName, m_OwnerName, m_CreatorName, m_Map, m_Players, m_Total, m_Playerlist );

    Close( );
}

void CMySQLCallableGetAliases :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = MySQLGetAliases( m_Connection, &m_Error, m_SQLBotID );

	Close( );
}

void CMySQLCallableGetStatsTemplates :: operator( )( )
{
        Init( );

        if( m_Error.empty( ) )
                m_Result = MySQLGetStatsTemplates( m_Connection, &m_Error, m_SQLBotID );

        Close( );
}

void CMySQLCallableGetPlayerStats :: operator( )( )
{
        Init( );

        if( m_Error.empty( ) )
                m_Result = MySQLGetPlayerStats( m_Connection, &m_Error, m_SQLBotID, m_AliasId, m_PlayerId );

        Close( );
}

void CMySQLCallableGetPlayerScore :: operator( )( )
{
        Init( );

        if( m_Error.empty( ) )
                m_Result = MySQLGetPlayerScore( m_Connection, &m_Error, m_SQLBotID, m_AliasId, m_PlayerId );

        Close( );
}

void CMySQLCallableUpdateGameInfo :: operator( )( )
{
        Init( );

        if( m_Error.empty( ) )
                MySQLUpdateGameInfo( m_Connection, &m_Error, m_SQLBotID, m_GameId, m_GameName );

        Close( );
}