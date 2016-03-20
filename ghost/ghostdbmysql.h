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

#ifdef GHOST_MYSQL

#ifndef GHOSTDBMYSQL_H
#define GHOSTDBMYSQL_H

//
// CGHostDBMySQL
//

class CGHostDBMySQL : public CGHostDB
{
private:
	string m_Server;
	string m_Database;
	string m_User;
	string m_Password;
	uint16_t m_Port;
	uint32_t m_BotID;
	queue<void *> m_IdleConnections;
	uint32_t m_NumConnections;
	uint32_t m_OutstandingCallables;

public:
	CGHostDBMySQL( CConfig *CFG );
	virtual ~CGHostDBMySQL( );

	virtual string GetStatus( );

	virtual void RecoverCallable( CBaseCallable *callable );

	// threaded database functions

	virtual void CreateThread( CBaseCallable *callable );
	virtual CCallableAdminCount *ThreadedAdminCount( string server );
	virtual CCallableAdminCheck *ThreadedAdminCheck( string server, string user );
	virtual CCallableAdminAdd *ThreadedAdminAdd( string server, string user );
	virtual CCallableAdminRemove *ThreadedAdminRemove( string server, string user );
	virtual CCallableAdminList *ThreadedAdminList( string server );
	virtual CCallableBanCount *ThreadedBanCount( string server );
	virtual CCallableBanCheck *ThreadedBanCheck( string server, string user, string ip );
	virtual CCallableBanAdd *ThreadedBanAdd( string server, string user, string ip, string gamename, string admin, string reason, uint32_t banlength );
	virtual CCallableBanRemove *ThreadedBanRemove( string server, string user );
	virtual CCallableBanRemove *ThreadedBanRemove( string user );
	virtual CCallableBanList *ThreadedBanList( string server );
	virtual CCallableGameAdd *ThreadedGameAdd( string server, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, string creatorserver, uint32_t gameid, uint32_t aliasid, vector<string> lobbylog, vector<string> gamelog, string elochange );
	virtual CCallableGamePlayerAdd *ThreadedGamePlayerAdd( uint32_t gameid, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t reserved, uint32_t loadingtime, uint32_t left, string leftreason, uint32_t team, uint32_t colour, uint32_t playerid );
	virtual CCallableGamePlayerSummaryCheck *ThreadedGamePlayerSummaryCheck( string name );
	virtual CCallableDotAGameAdd *ThreadedDotAGameAdd( uint32_t gameid, uint32_t winner, uint32_t min, uint32_t sec );
	virtual CCallableDotAPlayerAdd *ThreadedDotAPlayerAdd( uint32_t gameid, uint32_t colour, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t assists, uint32_t gold, uint32_t neutralkills, string item1, string item2, string item3, string item4, string item5, string item6, string hero, uint32_t newcolour, uint32_t towerkills, uint32_t raxkills, uint32_t courierkills );
	virtual CCallableDotAPlayerSummaryCheck *ThreadedDotAPlayerSummaryCheck( string name );
	virtual CCallableDownloadAdd *ThreadedDownloadAdd( string map, uint32_t mapsize, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t downloadtime );
	virtual CCallableScoreCheck *ThreadedScoreCheck( string category, string name, string server );
	virtual CCallableW3MMDPlayerAdd *ThreadedW3MMDPlayerAdd( string category, uint32_t gameid, uint32_t pid, string name, string flag, uint32_t leaver, uint32_t practicing );
	virtual CCallableW3MMDVarAdd *ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,int32_t> var_ints );
	virtual CCallableW3MMDVarAdd *ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,double> var_reals );
	virtual CCallableW3MMDVarAdd *ThreadedW3MMDVarAdd( uint32_t gameid, map<VarP,string> var_strings );

	// other database functions
	virtual CCallableGetPlayerId *ThreadedGetPlayerId( string user );
	virtual CCallableCreatePlayerId *ThreadedCreatePlayerId( string user, string ip, string realm );
        virtual CCallableGetGameId *ThreadedGetGameId( );
        virtual CCallableGetBotConfigs *ThreadedGetBotConfigs( );
        virtual CCallableGetBotConfigTexts *ThreadedGetBotConfigTexts( );
        virtual CCallableGetLanguages *ThreadedGetLanguages( );
        virtual CCallableGetMapConfig *ThreadedGetMapConfig( string configname );
        virtual CCallableGameUpdate *ThreadedGameUpdate( uint32_t hostcounter, uint32_t lobby, string map_type, uint32_t duration, string gamename, string ownername, string creatorname, string map, uint32_t players, uint32_t total, vector<PlayerOfPlayerList> playerlist );
        virtual CCallableGetAliases *ThreadedGetAliases( );
        virtual CCallableGetStatsTemplates *ThreadedGetStatsTemplates( );
        virtual CCallableGetPlayerStats *ThreadedGetPlayerStats( uint32_t aliasid, uint32_t playerid ); 
        virtual CCallableGetPlayerScore *ThreadedGetPlayerScore( uint32_t aliasid, uint32_t playerid );   
 
	virtual void *GetIdleConnection( );
};

//
// global helper functions
//

uint32_t MySQLAdminCount( void *conn, string *error, uint32_t botid, string server );
bool MySQLAdminCheck( void *conn, string *error, uint32_t botid, string server, string user );
bool MySQLAdminAdd( void *conn, string *error, uint32_t botid, string server, string user );
bool MySQLAdminRemove( void *conn, string *error, uint32_t botid, string server, string user );
map<string, uint32_t> MySQLAdminList( void *conn, string *error, uint32_t botid, string server );
uint32_t MySQLBanCount( void *conn, string *error, uint32_t botid, string server );
CDBBan *MySQLBanCheck( void *conn, string *error, uint32_t botid, string server, string user, string ip );
bool MySQLBanAdd( void *conn, string *error, uint32_t botid, string server, string user, string ip, string gamename, string admin, string reason, uint32_t banlength );
bool MySQLBanRemove( void *conn, string *error, uint32_t botid, string server, string user );
bool MySQLBanRemove( void *conn, string *error, uint32_t botid, string user );
vector<CDBBan *> MySQLBanList( void *conn, string *error, uint32_t botid, string server );
uint32_t MySQLGameAdd( void *conn, string *error, uint32_t botid, string server, string map, string gamename, string ownername, uint32_t duration, uint32_t gamestate, string creatorname, string creatorserver, uint32_t gameid, uint32_t aliasid, vector<string> lobbylog, vector<string> gamelog, string elochange );
uint32_t MySQLGamePlayerAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t reserved, uint32_t loadingtime, uint32_t left, string leftreason, uint32_t team, uint32_t colour, uint32_t playerid );
CDBGamePlayerSummary *MySQLGamePlayerSummaryCheck( void *conn, string *error, uint32_t botid, string name );
uint32_t MySQLDotAGameAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, uint32_t winner, uint32_t min, uint32_t sec );
uint32_t MySQLDotAPlayerAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, uint32_t colour, uint32_t kills, uint32_t deaths, uint32_t creepkills, uint32_t creepdenies, uint32_t assists, uint32_t gold, uint32_t neutralkills, string item1, string item2, string item3, string item4, string item5, string item6, string hero, uint32_t newcolour, uint32_t towerkills, uint32_t raxkills, uint32_t courierkills );
CDBDotAPlayerSummary *MySQLDotAPlayerSummaryCheck( void *conn, string *error, uint32_t botid, string name );
bool MySQLDownloadAdd( void *conn, string *error, uint32_t botid, string map, uint32_t mapsize, string name, string ip, uint32_t spoofed, string spoofedrealm, uint32_t downloadtime );
double MySQLScoreCheck( void *conn, string *error, uint32_t botid, string category, string name, string server );
uint32_t MySQLW3MMDPlayerAdd( void *conn, string *error, uint32_t botid, string category, uint32_t gameid, uint32_t pid, string name, string flag, uint32_t leaver, uint32_t practicing );
bool MySQLW3MMDVarAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, map<VarP,int32_t> var_ints );
bool MySQLW3MMDVarAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, map<VarP,double> var_reals );
bool MySQLW3MMDVarAdd( void *conn, string *error, uint32_t botid, uint32_t gameid, map<VarP,string> var_strings );
uint32_t MySQLGetPlayerId( void *conn, string *error, uint32_t botid, string user );
uint32_t MySQLCreatePlayerId( void *conn, string *error, uint32_t botid, string user, string ip, string realm );
uint32_t MySQLGetGameId( void *conn, string *error, uint32_t botid );
map<string, string> MySQLGetBotConfigs( void *conn, string *error, uint32_t botid );
map<string, vector<string> > MySQLGetBotConfigTexts( void *conn, string *error, uint32_t botid );
map<string, map<uint32_t, string> > MySQLGetLanguages( void *conn, string *error, uint32_t botid );
map<string, string> MySQLGetMapConfig( void *conn, string *error, uint32_t botid, string configname);
string MySQLGameUpdate( void *conn, string *error, uint32_t botid, uint32_t hostcounter, uint32_t lobby, string map_type, uint32_t duration, string gamename, string ownername, string creatorname, string map, uint32_t players, uint32_t total, vector<PlayerOfPlayerList> playerlist );
map<uint32_t, string> MySQLGetAliases( void *conn, string *error, uint32_t botid );
map<uint32_t, string> MySQLGetStatsTemplates( void *conn, string *error, uint32_t botid );
map<string, string> MySQLGetPlayerStats( void *conn, string *error, uint32_t botid, uint32_t aliasid, uint32_t playerid );
double MySQLGetPlayerScore( void *conn, string *error, uint32_t botid, uint32_t aliasid, uint32_t playerid );


//
// MySQL Callables
//

class CMySQLCallable : virtual public CBaseCallable
{
protected:
	void *m_Connection;
	string m_SQLServer;
	string m_SQLDatabase;
	string m_SQLUser;
	string m_SQLPassword;
	uint16_t m_SQLPort;
	uint32_t m_SQLBotID;

public:
	CMySQLCallable( void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), m_Connection( nConnection ), m_SQLBotID( nSQLBotID ), m_SQLServer( nSQLServer ), m_SQLDatabase( nSQLDatabase ), m_SQLUser( nSQLUser ), m_SQLPassword( nSQLPassword ), m_SQLPort( nSQLPort ) { }
	virtual ~CMySQLCallable( ) { }

	virtual void *GetConnection( )	{ return m_Connection; }

	virtual void Init( );
	virtual void Close( );
};

class CMySQLCallableAdminCount : public CCallableAdminCount, public CMySQLCallable
{
public:
	CMySQLCallableAdminCount( string nServer, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableAdminCount( nServer ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableAdminCount( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableAdminCheck : public CCallableAdminCheck, public CMySQLCallable
{
public:
	CMySQLCallableAdminCheck( string nServer, string nUser, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableAdminCheck( nServer, nUser ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableAdminCheck( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableAdminAdd : public CCallableAdminAdd, public CMySQLCallable
{
public:
	CMySQLCallableAdminAdd( string nServer, string nUser, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableAdminAdd( nServer, nUser ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableAdminAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableAdminRemove : public CCallableAdminRemove, public CMySQLCallable
{
public:
	CMySQLCallableAdminRemove( string nServer, string nUser, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableAdminRemove( nServer, nUser ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableAdminRemove( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableAdminList : public CCallableAdminList, public CMySQLCallable
{
public:
	CMySQLCallableAdminList( string nServer, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableAdminList( nServer ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableAdminList( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableBanCount : public CCallableBanCount, public CMySQLCallable
{
public:
	CMySQLCallableBanCount( string nServer, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableBanCount( nServer ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableBanCount( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableBanCheck : public CCallableBanCheck, public CMySQLCallable
{
public:
	CMySQLCallableBanCheck( string nServer, string nUser, string nIP, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableBanCheck( nServer, nUser, nIP ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableBanCheck( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableBanAdd : public CCallableBanAdd, public CMySQLCallable
{
public:
	CMySQLCallableBanAdd( string nServer, string nUser, string nIP, string nGameName, string nAdmin, string nReason, uint32_t nBanTime, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableBanAdd( nServer, nUser, nIP, nGameName, nAdmin, nReason, nBanTime ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableBanAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableBanRemove : public CCallableBanRemove, public CMySQLCallable
{
public:
	CMySQLCallableBanRemove( string nServer, string nUser, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableBanRemove( nServer, nUser ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableBanRemove( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableBanList : public CCallableBanList, public CMySQLCallable
{
public:
	CMySQLCallableBanList( string nServer, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableBanList( nServer ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableBanList( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableGameAdd : public CCallableGameAdd, public CMySQLCallable
{
public:
	CMySQLCallableGameAdd( string nServer, string nMap, string nGameName, string nOwnerName, uint32_t nDuration, uint32_t nGameState, string nCreatorName, string nCreatorServer, uint32_t nGameId, uint32_t nAliasId, vector<string> nLobbyLog, vector<string> nGameLog, string nEloChange, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableGameAdd( nServer, nMap, nGameName, nOwnerName, nDuration, nGameState, nCreatorName, nCreatorServer, nGameId, nAliasId, nLobbyLog, nGameLog, nEloChange ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableGameAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableGamePlayerAdd : public CCallableGamePlayerAdd, public CMySQLCallable
{
public:
	CMySQLCallableGamePlayerAdd( uint32_t nGameID, string nName, string nIP, uint32_t nSpoofed, string nSpoofedRealm, uint32_t nReserved, uint32_t nLoadingTime, uint32_t nLeft, string nLeftReason, uint32_t nTeam, uint32_t nColour, uint32_t nPlayerId, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableGamePlayerAdd( nGameID, nName, nIP, nSpoofed, nSpoofedRealm, nReserved, nLoadingTime, nLeft, nLeftReason, nTeam, nColour, nPlayerId ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableGamePlayerAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableGamePlayerSummaryCheck : public CCallableGamePlayerSummaryCheck, public CMySQLCallable
{
public:
	CMySQLCallableGamePlayerSummaryCheck( string nName, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableGamePlayerSummaryCheck( nName ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableGamePlayerSummaryCheck( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableDotAGameAdd : public CCallableDotAGameAdd, public CMySQLCallable
{
public:
	CMySQLCallableDotAGameAdd( uint32_t nGameID, uint32_t nWinner, uint32_t nMin, uint32_t nSec, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableDotAGameAdd( nGameID, nWinner, nMin, nSec ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableDotAGameAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableDotAPlayerAdd : public CCallableDotAPlayerAdd, public CMySQLCallable
{
public:
	CMySQLCallableDotAPlayerAdd( uint32_t nGameID, uint32_t nColour, uint32_t nKills, uint32_t nDeaths, uint32_t nCreepKills, uint32_t nCreepDenies, uint32_t nAssists, uint32_t nGold, uint32_t nNeutralKills, string nItem1, string nItem2, string nItem3, string nItem4, string nItem5, string nItem6, string nHero, uint32_t nNewColour, uint32_t nTowerKills, uint32_t nRaxKills, uint32_t nCourierKills, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableDotAPlayerAdd( nGameID, nColour, nKills, nDeaths, nCreepKills, nCreepDenies, nAssists, nGold, nNeutralKills, nItem1, nItem2, nItem3, nItem4, nItem5, nItem6, nHero, nNewColour, nTowerKills, nRaxKills, nCourierKills ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableDotAPlayerAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableDotAPlayerSummaryCheck : public CCallableDotAPlayerSummaryCheck, public CMySQLCallable
{
public:
	CMySQLCallableDotAPlayerSummaryCheck( string nName, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableDotAPlayerSummaryCheck( nName ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableDotAPlayerSummaryCheck( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableDownloadAdd : public CCallableDownloadAdd, public CMySQLCallable
{
public:
	CMySQLCallableDownloadAdd( string nMap, uint32_t nMapSize, string nName, string nIP, uint32_t nSpoofed, string nSpoofedRealm, uint32_t nDownloadTime, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableDownloadAdd( nMap, nMapSize, nName, nIP, nSpoofed, nSpoofedRealm, nDownloadTime ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableDownloadAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableScoreCheck : public CCallableScoreCheck, public CMySQLCallable
{
public:
	CMySQLCallableScoreCheck( string nCategory, string nName, string nServer, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableScoreCheck( nCategory, nName, nServer ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableScoreCheck( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableW3MMDPlayerAdd : public CCallableW3MMDPlayerAdd, public CMySQLCallable
{
public:
	CMySQLCallableW3MMDPlayerAdd( string nCategory, uint32_t nGameID, uint32_t nPID, string nName, string nFlag, uint32_t nLeaver, uint32_t nPracticing, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableW3MMDPlayerAdd( nCategory, nGameID, nPID, nName, nFlag, nLeaver, nPracticing ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableW3MMDPlayerAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableW3MMDVarAdd : public CCallableW3MMDVarAdd, public CMySQLCallable
{
public:
	CMySQLCallableW3MMDVarAdd( uint32_t nGameID, map<VarP,int32_t> nVarInts, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableW3MMDVarAdd( nGameID, nVarInts ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	CMySQLCallableW3MMDVarAdd( uint32_t nGameID, map<VarP,double> nVarReals, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableW3MMDVarAdd( nGameID, nVarReals ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	CMySQLCallableW3MMDVarAdd( uint32_t nGameID, map<VarP,string> nVarStrings, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableW3MMDVarAdd( nGameID, nVarStrings ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
	virtual ~CMySQLCallableW3MMDVarAdd( ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableGetPlayerId : public CCallableGetPlayerId, public CMySQLCallable
{
public:
	CMySQLCallableGetPlayerId( string nUser, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableGetPlayerId( nUser ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableCreatePlayerId : public CCallableCreatePlayerId, public CMySQLCallable
{
public:
	CMySQLCallableCreatePlayerId( string nUser, string nIP, string nRealm, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableCreatePlayerId( nUser, nIP, nRealm ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableGetGameId : public CCallableGetGameId, public CMySQLCallable
{
public:
	CMySQLCallableGetGameId( void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableGetGameId( ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableGetBotConfigs : public CCallableGetBotConfigs, public CMySQLCallable
{
public:
	CMySQLCallableGetBotConfigs( void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableGetBotConfigs( ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableGetBotConfigTexts : public CCallableGetBotConfigTexts, public CMySQLCallable
{
public:
	CMySQLCallableGetBotConfigTexts( void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableGetBotConfigTexts( ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableGetLanguages : public CCallableGetLanguages, public CMySQLCallable
{
public:
	CMySQLCallableGetLanguages( void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableGetLanguages( ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableGetMapConfig : public CCallableGetMapConfig, public CMySQLCallable
{
public:
	CMySQLCallableGetMapConfig( string nConfigName, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableGetMapConfig( nConfigName ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableGameUpdate : public CCallableGameUpdate, public CMySQLCallable
{
public:
    CMySQLCallableGameUpdate( uint32_t hostcounter, uint32_t lobby, string map_type, uint32_t duration, string gamename, string ownername, string creatorname, string map, uint32_t players, uint32_t total, vector<PlayerOfPlayerList> playerlist, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableGameUpdate( hostcounter, lobby, map_type, duration, gamename, ownername, creatorname, map, players, total,  playerlist ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }
    virtual ~CMySQLCallableGameUpdate( ) { }

    virtual void operator( )( );
    virtual void Init( ) {
        CMySQLCallable :: Init( );
    }
    virtual void Close( ) {
        CMySQLCallable :: Close( );
    }
};

class CMySQLCallableGetAliases : public CCallableGetAliases, public CMySQLCallable
{
public:
	CMySQLCallableGetAliases( void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableGetAliases( ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }

	virtual void operator( )( );
	virtual void Init( ) { CMySQLCallable :: Init( ); }
	virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableGetStatsTemplates : public CCallableGetStatsTemplates, public CMySQLCallable
{
public:
        CMySQLCallableGetStatsTemplates( void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableGetStatsTemplates( ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }

        virtual void operator( )( );
        virtual void Init( ) { CMySQLCallable :: Init( ); }
        virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableGetPlayerStats : public CCallableGetPlayerStats, public CMySQLCallable
{
public:
        CMySQLCallableGetPlayerStats( uint32_t nAliasId, uint32_t nPlayerId, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableGetPlayerStats( nAliasId, nPlayerId ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }

        virtual void operator( )( );
        virtual void Init( ) { CMySQLCallable :: Init( ); }
        virtual void Close( ) { CMySQLCallable :: Close( ); }
};

class CMySQLCallableGetPlayerScore : public CCallableGetPlayerScore, public CMySQLCallable
{
public:
        CMySQLCallableGetPlayerScore( uint32_t nAliasId, uint32_t nPlayerId, void *nConnection, uint32_t nSQLBotID, string nSQLServer, string nSQLDatabase, string nSQLUser, string nSQLPassword, uint16_t nSQLPort ) : CBaseCallable( ), CCallableGetPlayerScore( nAliasId, nPlayerId ), CMySQLCallable( nConnection, nSQLBotID, nSQLServer, nSQLDatabase, nSQLUser, nSQLPassword, nSQLPort ) { }

        virtual void operator( )( );
        virtual void Init( ) { CMySQLCallable :: Init( ); }
        virtual void Close( ) { CMySQLCallable :: Close( ); }
};
#endif

#endif
