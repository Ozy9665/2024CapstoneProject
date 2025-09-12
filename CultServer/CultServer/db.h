#pragma once

#include <iostream>
#include <windows.h>
#include <sqlext.h>  
#include <locale.h>

/************************************************************************
/* HandleDiagnosticRecord : display error/warning information
/*
/* Parameters:
/*      hHandle     ODBC handle
/*      hType       Type of handle (SQL_HANDLE_STMT, SQL_HANDLE_ENV, SQL_HANDLE_DBC)
/*      RetCode     Return code of failing command
/************************************************************************/

void HandleDiagnosticRecord(SQLHANDLE, SQLSMALLINT, RETCODE);

bool InitializeDB();

bool checkValidID(std::string);

bool createNewID(std::string, std::string);