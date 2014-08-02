// Copyright (c) 2010 Martin Knafve / hMailServer.com.  
// http://www.hmailserver.com

#pragma once


namespace HM
{
   class Result 
   {
   public:
      Result(void);
      ~Result(void);

      void SetValue(long lNewVal){value_ = lNewVal; }
      long GetValue() const; 

      void SetParameter(int lNewVal){_parameter = lNewVal; }
      int GetParameter() const; 


      void SetMessage(const String& sValue){message_ = sValue; }
      String GetMessage() const;

   private:
      long value_;
      int _parameter;

      String message_;
   };
}