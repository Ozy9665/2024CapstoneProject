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

int logIn(std::string, std::string);
// 0- 성공
// 1- id 없음
// 2- 비번 오류
// 3- db에러
// 4- 접속중인 아이디