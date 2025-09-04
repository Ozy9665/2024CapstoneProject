#pragma once

#include <iostream>
#include <windows.h>
#include <sqlext.h>  
#include <locale.h>

void show_error() {
	std::cout << "error" << std::endl;
}

/************************************************************************
/* HandleDiagnosticRecord : display error/warning information
/*
/* Parameters:
/*      hHandle     ODBC handle
/*      hType       Type of handle (SQL_HANDLE_STMT, SQL_HANDLE_ENV, SQL_HANDLE_DBC)
/*      RetCode     Return code of failing command
/************************************************************************/

void HandleDiagnosticRecord(SQLHANDLE, SQLSMALLINT, RETCODE);

void InitializeThreadDB();

bool checkValidID(int);

bool createNewID(long long , short , short );