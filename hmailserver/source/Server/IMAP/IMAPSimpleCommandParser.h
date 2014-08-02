// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#pragma once

namespace HM
{

   class IMAPCommandArgument;

   class IMAPSimpleWord
   {
   public:
      IMAPSimpleWord();
      virtual ~IMAPSimpleWord();

      bool Quoted();
      void Quoted(bool bNewVal) {is_quoted_ = bNewVal; }

      bool Paranthezied();
      void Paranthezied(bool bNewVal) {is_paranthezied_ = bNewVal; }

      bool Clammerized ();
      void Clammerized(bool bNewVal) {is_clammerized_ = bNewVal; }

      String Value();
      void Value(const String &sNewVal);

      String LiteralData() {return literal_data_;}
      void LiteralData(const String &sNewVal) {literal_data_ = sNewVal; }

   private:
      String word_;
      String literal_data_;

      bool is_quoted_;
      bool is_paranthezied_;
      bool is_clammerized_;

   };

   class IMAPSimpleCommandParser  
   {
   public:
	   IMAPSimpleCommandParser();
	   virtual ~IMAPSimpleCommandParser();

      void Parse(shared_ptr<IMAPCommandArgument> pArgument);
      int WordCount() {return parsed_words_.size(); }
      int ParamCount() {return parsed_words_.size() - 1; }
      
      String GetParamValue(shared_ptr<IMAPCommandArgument> pArguments, int iParamIndex);

      shared_ptr<IMAPSimpleWord> Word(int iIndex) {return parsed_words_[iIndex]; }

      shared_ptr<IMAPSimpleWord> QuotedWord();
      shared_ptr<IMAPSimpleWord> ParantheziedWord();
      shared_ptr<IMAPSimpleWord> ClammerizedWord();

      void AddWord(shared_ptr<IMAPSimpleWord> pWord) {parsed_words_.push_back(pWord); }

      void RemoveWord(int iWordIdx);

      void UnliteralData();

   private:


      int _FindEndOfQuotedString(const String &sInputString, int iWordStartPos);
      std::vector<shared_ptr<IMAPSimpleWord> > parsed_words_;

      bool _Validate(const String &command);

   };

   class IMAPSimpleCommandParserTester
   {
   public:
      IMAPSimpleCommandParserTester();

      bool Test();
   };

}
;