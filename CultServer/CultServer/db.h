#pragma once

#include <sqlext.h>  
#include <locale.h>
#include <stdio.h>

void show_error() {
	printf("error\n");
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