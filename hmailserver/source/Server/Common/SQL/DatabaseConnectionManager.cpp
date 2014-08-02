// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#include "StdAfx.h"
#include ".\databaseconnectionmanager.h"

#include "DALConnection.h"
#include "DALConnectionFactory.h"
#include "DatabaseSettings.h"

#include "ADORecordset.h"
#include "MySQLRecordset.h"
#include "PGRecordset.h"
#include "SQLCERecordset.h"
#include "MySQLInterface.h"

#include "SQLCommand.h"

#include "Prerequisites/PrerequisiteList.h"
#include "SQLScriptRunner.h"

#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif


#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

namespace HM
{
   DatabaseConnectionManager::DatabaseConnectionManager(void)
   {
   }

   DatabaseConnectionManager::~DatabaseConnectionManager(void)
   {
   }

   bool
   DatabaseConnectionManager::CreateConnections(String &sErrorMessage)
   {
      int iNoOfConnectionAttempts = IniFileSettings::Instance()->GetNumberOfDatabaseConnectionAttempts();
      int iDelayBetweenConnectionAttempt = IniFileSettings::Instance()->GetDBConnectionAttemptsDelay() * 1000;


      bool bConnectionOK = false;
      for (int iTry = 1; iTry <= iNoOfConnectionAttempts ; iTry++)
      {     
         DALConnection::ConnectionResult iResult = _Connect(sErrorMessage);

         switch (iResult)
         {
         case DALConnection::Connected:
            // Reset the error message. If we failed to connect the first time,
            // but succeeded the second, it's not really an error to care about.
            sErrorMessage = "";
            return true;
         case DALConnection::FatalError:
            // Skip out and report error.
            return false;
         case DALConnection::TemporaryFailure:
            // We failed to connect to the database server.
            // Pause a few seconds and then try again.
            Sleep(iDelayBetweenConnectionAttempt);
            break;
         }




      }  

      return false;
   }

   DALConnection::ConnectionResult
   DatabaseConnectionManager::_Connect(String &sErrorMessage)
   {
      int iNoOfConnections = IniFileSettings::Instance()->GetNumberOfDatabaseConnections();

      IniFileSettings *pIniFileSettings = IniFileSettings::Instance();

      String sServer = pIniFileSettings->GetDatabaseServer();
      String sUsername = pIniFileSettings->GetUsername();
      String sPassword = pIniFileSettings->GetPassword();
      String sDatabase = pIniFileSettings->GetDatabaseName();
      String sDatabaseDirectory = pIniFileSettings->GetDatabaseDirectory();
      String sDatabaseServerFailoverPartner = pIniFileSettings->GetDatabaseServerFailoverPartner();
      long lDBPort = pIniFileSettings->GetDatabasePort();
      HM::DatabaseSettings::SQLDBType dbType = IniFileSettings::Instance()->GetDatabaseType();

      shared_ptr<DatabaseSettings> pSettings = shared_ptr<DatabaseSettings> 
         (new DatabaseSettings(sServer, sDatabase, sUsername, sPassword, sDatabaseDirectory, sDatabaseServerFailoverPartner, dbType, lDBPort));

      for (int i = 0; i < iNoOfConnections; i++)
      {
         shared_ptr<DALConnection> pConnection = DALConnectionFactory::CreateConnection(pSettings);
         DALConnection::ConnectionResult result = pConnection->Connect(sErrorMessage);

         if (result != DALConnection::Connected)
            return result;

         available_connections_.insert(pConnection);
      }

      // Fetch first connection
      std::set<shared_ptr<DALConnection> >::iterator iter = available_connections_.begin();
      if (!(*iter)->CheckServerVersion(sErrorMessage))
         return DALConnection::FatalError;

      (*iter)->OnConnected();

      return DALConnection::Connected;
   }

   void
   DatabaseConnectionManager::Disconnect()
   {
      boost::lock_guard<boost::recursive_mutex> guard(_mutex);

      //std::set<shared_ptr<DALConnection> >::iterator iterConnection = available_connections_.begin();
      boost_foreach(shared_ptr<DALConnection> pConnection, available_connections_)
      {
         pConnection->Disconnect();
      }
      available_connections_.clear();

      boost_foreach(shared_ptr<DALConnection> pConnection, busy_connections_)
      {
         pConnection->Disconnect();
      }

      busy_connections_.clear();
   }
   
   bool 
   DatabaseConnectionManager::Execute(const SQLStatement &statement, __int64 *iInsertID, int iIgnoreErrors, String &sErrorMessage)
   {
      return Execute(statement.GetCommand(), iInsertID, iIgnoreErrors, sErrorMessage);
   }

   bool 
   DatabaseConnectionManager::Execute(const SQLCommand &command, __int64 *iInsertID, int iIgnoreErrors, String &sErrorMessage)
   {
      shared_ptr<DALConnection> pDALConn = _GetConnection();

      if (!pDALConn)
      {

         assert(0);
         return false;
      }

      bool bResult = pDALConn->Execute(command, sErrorMessage, iInsertID, iIgnoreErrors);

      _ReleaseConnection(pDALConn);

      return bResult;
   }

   shared_ptr<DALRecordset> 
   DatabaseConnectionManager::OpenRecordset(const SQLStatement &statement)
   {
      return OpenRecordset(statement.GetCommand());
   }

   shared_ptr<DALRecordset> 
   DatabaseConnectionManager::OpenRecordset(const SQLCommand &command)
   {
      shared_ptr<DALRecordset> pRecordset;

      shared_ptr<DALConnection> pDALConn = _GetConnection();

      if (!pDALConn)
      {
         assert(0);
         return pRecordset;
      }

      pRecordset = pDALConn->CreateRecordset();

      if (!pRecordset->Open(pDALConn, command))
         pRecordset.reset();

      _ReleaseConnection(pDALConn);

      return pRecordset;


   }

   void
   DatabaseConnectionManager::_ReleaseConnection(shared_ptr<DALConnection> pConnection)
   {
      boost::lock_guard<boost::recursive_mutex> guard(_mutex);

      std::set<shared_ptr<DALConnection> >::iterator iterConnection = busy_connections_.find(pConnection);
      if (iterConnection == busy_connections_.end())
      {
         assert(0);
         return;
      }

      busy_connections_.erase(iterConnection);

      // Locate an available connection
      available_connections_.insert(pConnection);
   }

   int 
   DatabaseConnectionManager::GetCurrentDatabaseVersion()
   {
      SQLCommand command("select * from hm_dbversion");
      shared_ptr<DALRecordset> pRS = OpenRecordset(command);
      if (!pRS)
         return 0;

      int iRetVal = pRS->GetLongValue("value");

      return iRetVal;
   }

   shared_ptr<DALConnection>
   DatabaseConnectionManager::_GetConnection()
   {
      // Loop until we find a free connection
      while (1)
      {
         // We want to sleep outside of the lock-scope. Hence the inner scope here.
         {
            boost::lock_guard<boost::recursive_mutex> guard(_mutex);

            // Locate an available connection
            std::set<shared_ptr<DALConnection> >::iterator iterConnection = available_connections_.begin();

            if (iterConnection != available_connections_.end())
            {
               // Remove the connection from free and add to busy
               shared_ptr<DALConnection> pConn = (*iterConnection);

               // Remove it from the list of available connections
               available_connections_.erase(iterConnection);

               busy_connections_.insert(pConn);
               return pConn;
            }
            else
            {
               if (busy_connections_.size() == 0 &&
                  available_connections_.size() == 0)
               {
                  // There's no available connections at all. Nothing to wait for.
                  shared_ptr<DALConnection> pEmpty;
                  return pEmpty;
               }
            }
         }

         Sleep(10);
      }
   }

   shared_ptr<DALConnection> 
   DatabaseConnectionManager::BeginTransaction(String &sErrorMessage)
   {
      shared_ptr<DALConnection> pDALConnection = _GetConnection();
      if (!pDALConnection->BeginTransaction(sErrorMessage))
      {
         // Could not start database transaction.
         shared_ptr<DALConnection> pEmpty;
         return pEmpty;
      }

      return pDALConnection;
   }

   bool
   DatabaseConnectionManager::CommitTransaction(shared_ptr<DALConnection> pConnection, String &sErrorMessage)
   {
      bool bResult = pConnection->CommitTransaction(sErrorMessage);
      _ReleaseConnection(pConnection);

      return bResult;
   }

   bool
   DatabaseConnectionManager::RollbackTransaction(shared_ptr<DALConnection> pConnection, String &sErrorMessage)
   {
      bool bResult = pConnection->RollbackTransaction(sErrorMessage);
      _ReleaseConnection(pConnection);

      return bResult;
   }

   bool
   DatabaseConnectionManager::GetIsConnected()
   {
      boost::lock_guard<boost::recursive_mutex> guard(_mutex);
      size_t iNoOfConnections = busy_connections_.size() + available_connections_.size();

      if (iNoOfConnections == 0)
         return false;

      return true;
   }

   bool
   DatabaseConnectionManager::ExecuteScript(const String &sFile, String &sErrorMessage)
   {
      shared_ptr<DALConnection> pConnection = _GetConnection();

      SQLScriptRunner scriptRunner;
      bool result = scriptRunner.ExecuteScript(pConnection, sFile, sErrorMessage);

      _ReleaseConnection(pConnection);

      return result;
   }

   bool
   DatabaseConnectionManager::EnsuresPrerequisites(long DBVersion, String &sErrorMessage)
   {
      PrerequisiteList prerequisites;

      shared_ptr<DALConnection> pConnection = _GetConnection();

      bool result = prerequisites.Ensure(pConnection, DBVersion, sErrorMessage);

      _ReleaseConnection(pConnection);

      return result;
   }
}