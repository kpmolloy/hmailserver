// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#pragma once

namespace HM
{

   class DatabaseSettings;

   class SQLScriptParser  
   {
   public:
      SQLScriptParser(shared_ptr<DatabaseSettings> pSettings, const String &sFile);
      virtual ~SQLScriptParser();

      bool Parse(String &sErrorMessage);

      int GetNoOfCommands() {return (int)commands_.size(); }
      String GetCommand(int index) {return commands_[index]; }

   private:

      bool _PreprocessLine(String &sLine);

      shared_ptr<DatabaseSettings> settings_;
      String file_;
      vector<String> commands_;

   };

}
