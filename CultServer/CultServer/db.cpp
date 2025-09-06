#include "db.h"
#include <string>

SQLHENV  henv;
SQLHDBC  hdbc;
SQLHSTMT hstmt = 0;

void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode)
{
	SQLSMALLINT iRec = 0;
	SQLINTEGER  iError;
	WCHAR       wszMessage[1000];
	WCHAR       wszState[SQL_SQLSTATE_SIZE + 1];

	if (RetCode == SQL_INVALID_HANDLE) {
		std::cout << "Invalid handle!\n" << std::endl;
		return;
	}
	while (SQLGetDiagRec(hType, hHandle, ++iRec, wszState, &iError, wszMessage,
		(SQLSMALLINT)(sizeof(wszMessage) / sizeof(WCHAR)), (SQLSMALLINT*)NULL) == SQL_SUCCESS) {
		// Hide data truncated..
		if (wcsncmp(wszState, L"01004", 5)) {
			fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
		}
	}
}

void InitializeDB() {
	SQLRETURN retcode;

	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
	if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
		return;

	retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
	if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
		return;

	retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
	if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
		return;

	SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

	retcode = SQLConnect(hdbc, (SQLWCHAR*)L"Cult", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);
	if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
		return;

	retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
	if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
		return;
}

bool checkValidID(std::string user_id_str)
{
	if (hstmt == SQL_NULL_HSTMT)
		return false;

	SQLFreeStmt(hstmt, SQL_CLOSE);

	SQLWCHAR szQuery[256];
	swprintf_s(szQuery, 256, L"EXEC isValidID '%S'", user_id_str.c_str());

	SQLRETURN ret = SQLExecDirect(hstmt, szQuery, SQL_NTS);
	if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
		HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, ret);
		return false;
	}

	SQLINTEGER isValid = 0;
	SQLLEN    cbValid = 0;
	SQLBindCol(hstmt, 1, SQL_C_LONG, &isValid, sizeof(isValid), &cbValid);

	ret = SQLFetch(hstmt);
	SQLFreeStmt(hstmt, SQL_CLOSE);

	if (ret == SQL_NO_DATA) {
		std::cout << "[checkValidID] No data returned\n";
		return false;
	}
	else if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
		HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, ret);
		return false;
	}

	return (isValid == 1);
}

bool createNewID(long long user_id, short x, short y)
{
	if (hstmt == SQL_NULL_HSTMT) {
		InitializeDB();
		if (hstmt == SQL_NULL_HSTMT) {
			std::cout << "[createNewID] ERROR: hstmt == SQL_NULL_HSTMT after Init\n";
		}
		return false;
	}

	SQLFreeStmt(hstmt, SQL_CLOSE);

	SQLWCHAR szQuery[128];
	swprintf_s(szQuery, 128, L"EXEC createNewID %I64d, %d, %d", user_id, x, y);
	
	SQLRETURN ret = SQLExecDirect(hstmt, szQuery, SQL_NTS);
	if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
		HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, ret);
		SQLFreeStmt(hstmt, SQL_CLOSE);
		return false;
	}

	SQLFreeStmt(hstmt, SQL_CLOSE);
	return true;
}
