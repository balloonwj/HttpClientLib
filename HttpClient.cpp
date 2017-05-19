/**
 * WinInet封装类，可用于windows上http请求, HttpClient.cpp
 * zhangyl 2016.07.15 mail: balloonwj@qq.com
 */
#include "HttpClient.h"

CHttpClient::CHttpClient(void)
{
    m_hInternet = NULL;
    m_hConnect = NULL;
    m_hRequest = NULL;

    m_hCompleteEvent = NULL;
    m_hCancelEvent = NULL;

    m_dwConnectTimeOut = 60 * 1000;
    m_dwContext = 0;
}

CHttpClient::~CHttpClient(void)
{
    CloseRequest();
}


// 打开HTTP请求函数
BOOL CHttpClient::OpenRequest(LPCTSTR lpszUrl, HTTP_REQ_METHOD nReqMethod/* = REQ_METHOD_GET*/)
{
    TCHAR szScheme[INTERNET_MAX_URL_LENGTH] = { 0 };
    TCHAR szHostName[INTERNET_MAX_URL_LENGTH] = { 0 };
    TCHAR szUrlPath[INTERNET_MAX_URL_LENGTH] = { 0 };

    WORD nPort = 0;
    DWORD dwAccessType;

    LPCTSTR lpszProxy;
    // 解析Url
    BOOL bRet = ParseURL(lpszUrl, szScheme, INTERNET_MAX_URL_LENGTH, szHostName, INTERNET_MAX_URL_LENGTH, nPort, szUrlPath, INTERNET_MAX_URL_LENGTH);
    if (!bRet)
        return FALSE;


    m_hCompleteEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);     // 创建事件句柄
    m_hCancelEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

    if (NULL == m_hCompleteEvent || NULL == m_hCancelEvent)
    {
        CloseRequest();
        return FALSE;
    }

    if (m_strProxy.size() > 0)
    {
        dwAccessType = INTERNET_OPEN_TYPE_PROXY;
        lpszProxy = m_strProxy.c_str();
    }
    else
    {
        dwAccessType = INTERNET_OPEN_TYPE_PRECONFIG;
        lpszProxy = NULL;
    }

    m_hInternet = ::InternetOpen(IE8_USER_AGENT, dwAccessType, lpszProxy, NULL, INTERNET_FLAG_ASYNC);
    if (NULL == m_hInternet)
    {
        CloseRequest();
        return FALSE;
    }

    if (!m_strUser.empty())   // 设置代理用户名
    {
        ::InternetSetOptionEx(m_hInternet, INTERNET_OPTION_PROXY_USERNAME, (LPVOID)m_strUser.c_str(), m_strUser.size() + 1, 0);
    }

    if (!m_strPwd.empty())    // 设置代理密码
    {
        ::InternetSetOptionEx(m_hInternet, INTERNET_OPTION_PROXY_PASSWORD, (LPVOID)m_strPwd.c_str(), m_strPwd.size() + 1, 0);
    }

    // DWORDdwTimeOut;
    // DWORDdwSize = sizeof(dwTimeOut);
    // ::InternetQueryOption(m_hInternet,INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&dwTimeOut, &dwSize);

    INTERNET_STATUS_CALLBACKlpCallBackFunc;    // 设置状态回调函数
    lpCallBackFunc = ::InternetSetStatusCallback(m_hInternet, (INTERNET_STATUS_CALLBACK)&StatusCallback);
    if (INTERNET_INVALID_STATUS_CALLBACK == lpCallBackFunc)
    {
        CloseRequest();
        return FALSE;
    }

    m_dwContext = CONNECTED_EVENT;
    m_hConnect = ::InternetConnect(m_hInternet, szHostName, nPort, NULL, _T("HTTP/1.1"), INTERNET_SERVICE_HTTP, 0, (DWORD_PTR)this);
    if (NULL == m_hConnect)
    {
        if (::GetLastError() != ERROR_IO_PENDING)
        {
            CloseRequest();
            return FALSE;
        }
    }

    bRet = WaitForEvent(CONNECTED_EVENT, m_dwConnectTimeOut);
    if (!bRet)
    {
        CloseRequest();
        return FALSE;
    }

    TCHAR* lpMethod;
    if (nReqMethod == REQ_METHOD_GET)
        lpMethod = _T("GET");
    else
        lpMethod = _T("POST");

    DWORD dwFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (INTERNET_DEFAULT_HTTPS_PORT == nPort)
        dwFlags |= INTERNET_FLAG_SECURE;

    m_dwContext = REQUEST_OPENED_EVENT;
    m_hRequest = ::HttpOpenRequest(m_hConnect, lpMethod, szUrlPath, _T("HTTP/1.1"), NULL, NULL, dwFlags, (DWORD_PTR)this);
    if (NULL == m_hRequest)
    {
        if (::GetLastError() != ERROR_IO_PENDING)
        {
            CloseRequest();
            return FALSE;
        }
    }

    bRet = WaitForEvent(REQUEST_OPENED_EVENT, INFINITE);
    if (!bRet)
    {
        CloseRequest();
        return FALSE;
    }

    return TRUE;
}



// 添加一个或多个HTTP请求头函数
BOOL CHttpClient::AddReqHeaders(LPCTSTR lpHeaders)
{
    if (NULL == m_hRequest || NULL == lpHeaders)
        return FALSE;

    return ::HttpAddRequestHeaders(m_hRequest, lpHeaders, _tcslen(lpHeaders), HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);
}

// 发送HTTP请求函数
BOOL CHttpClient::SendRequest(const CHAR * lpData, DWORD dwLen)
{
    if (NULL == m_hRequest)
        return FALSE;

    BOOL bRet = ::HttpSendRequest(m_hRequest, NULL, 0, (LPVOID)lpData, dwLen);
    if (!bRet)
    {
        if (::GetLastError() != ERROR_IO_PENDING)
            return FALSE;
    }

    bRet = WaitForEvent(REQUEST_COMPLETE_EVENT, INFINITE);
    if (!bRet)
        return FALSE;
    
    return TRUE;
}

// 开始发送HTTP请求函数
BOOL CHttpClient::SendRequestEx(DWORD dwLen)
{   
    if (NULL == m_hRequest)
        return FALSE;

    INTERNET_BUFFERS stInetBuf = { 0 };    
    stInetBuf.dwStructSize = sizeof(INTERNET_BUFFERS);
    stInetBuf.dwBufferTotal = dwLen;

    BOOL bRet = ::HttpSendRequestEx(m_hRequest, &stInetBuf, NULL, 0, (DWORD_PTR)this);
    if (!bRet)
    {
        if (::GetLastError() != ERROR_IO_PENDING)
            return FALSE;

        bRet = WaitForEvent(REQUEST_COMPLETE_EVENT, INFINITE);
        if (!bRet)
            return FALSE;
    }

    return TRUE;
}

// 发送HTTP请求消息体数据函数
BOOL CHttpClient::SendReqBodyData(const CHAR * lpBuf, DWORD dwLen, DWORD& dwSendLen)
{
    if (NULL == m_hRequest || NULL == lpBuf || dwLen <= 0)
        return FALSE;

    dwSendLen = 0;
    BOOL bRet = ::InternetWriteFile(m_hRequest, lpBuf, dwLen, &dwSendLen);
    if (!bRet)
    {
        if (::GetLastError() != ERROR_IO_PENDING)
            return FALSE;

        bRet = WaitForEvent(REQUEST_COMPLETE_EVENT, INFINITE);
        if (!bRet)
            return FALSE;
    }
    else
    {
        bRet = WaitForEvent(USER_CANCEL_EVENT, 0);
        if (!bRet)
            return FALSE;
    }

    return TRUE;
}



// 结束发送HTTP请求函数
BOOL CHttpClient::EndSendRequest()
{
    BOOL bRet;
    if (NULL == m_hRequest)
        return FALSE;

    bRet = ::HttpEndRequest(m_hRequest, NULL, HSR_INITIATE, (DWORD_PTR)this);
    if (!bRet)
    {
        if (::GetLastError() != ERROR_IO_PENDING)
            return FALSE;

        bRet = WaitForEvent(REQUEST_COMPLETE_EVENT, INFINITE);
        if (!bRet)
            return FALSE;
    }

    return TRUE;
}

// 获取HTTP响应码函数
DWORD CHttpClient::GetRespCode()
{
    DWORD dwRespCode = 0;
    DWORD dwSize = sizeof(dwRespCode);
    BOOL bRet = ::HttpQueryInfo(m_hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &dwRespCode, &dwSize, NULL);
    if (bRet)
        return dwRespCode;
    else
        return 0;
}

// 获取全部HTTP头
tstring CHttpClient::GetRespHeader()
{
    CHAR * lpRespHeader = NULL;
    DWORD dwRespHeaderLen = 0;
    tstring strRespHeader;
    BOOL bRet = ::HttpQueryInfo(m_hRequest, HTTP_QUERY_RAW_HEADERS_CRLF, (LPVOID)lpRespHeader, &dwRespHeaderLen, NULL);
    if (!bRet)
    {
        if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            lpRespHeader = new CHAR[dwRespHeaderLen];
            if (lpRespHeader != NULL)
            {
                memset(lpRespHeader, 0, dwRespHeaderLen);
                bRet = ::HttpQueryInfo(m_hRequest, HTTP_QUERY_RAW_HEADERS_CRLF, (LPVOID)lpRespHeader, &dwRespHeaderLen, NULL);
                if (bRet)
                {
                    strRespHeader = (TCHAR *)lpRespHeader;
                }
            }
        }
    }

    if (lpRespHeader != NULL)
    {
        delete[]lpRespHeader;
        lpRespHeader = NULL;
    }

    return strRespHeader;
}

// 获取HTTP头指定字段名称的值
tstring CHttpClient::GetRespHeader(LPCTSTR lpszName, int nIndex/* = 0*/)
{ 
    if (NULL == lpszName)
        return _T("");

    tstring strLine;
    int nNameLen, nIndex2 = 0;
    nNameLen = _tcslen(lpszName);
    if (nNameLen <= 0)
        return _T("");

    if (m_arrRespHeader.size() <= 0)
    {
        if (!__GetRespHeader())
            return _T("");
    }


    for (int i = 0; i < (int)m_arrRespHeader.size(); i++)
    {
        strLine = m_arrRespHeader[i];
        if (!_tcsnicmp(strLine.c_str(), lpszName, nNameLen))
        {
            if (nIndex == nIndex2)
            {
                int nPos = nNameLen;
                int nLineLen = (int)strLine.size();

                for (; nPos < nLineLen &&strLine[nPos] == _T(' '); ++nPos)
                    ; // 跳过前导空格
                if (strLine[nPos] == _T(':'))   // 跳过“:”符号
                    nPos++;

                for (; nPos < nLineLen &&strLine[nPos] == _T(' '); ++nPos)
                    ; // 跳过前导空格

                return strLine.substr(nPos);
            }

            nIndex2++;
        }
    }

    return _T("");
}

// 获取HTTP头指定字段名称的值(Int)
int CHttpClient::GetRespHeaderByInt(LPCTSTR lpszName, int nIndex/* = 0*/)
{
    tstring strValue = GetRespHeader(lpszName, nIndex);
    return _tstoi(strValue.c_str());
}

// 获取HTTP响应消息体数据函数
BOOL CHttpClient::GetRespBodyData(CHAR * lpBuf, DWORD dwLen, DWORD& dwRecvLen)
{
    if (NULL == m_hRequest || NULL == lpBuf || dwLen <= 0)
        return FALSE;

    INTERNET_BUFFERSA stInetBuf = { 0 };
    BOOL bRet;
    dwRecvLen = 0;
    memset(lpBuf, 0, dwLen);

    stInetBuf.dwStructSize = sizeof(stInetBuf);
    stInetBuf.lpvBuffer = lpBuf;
    stInetBuf.dwBufferLength = dwLen;

    bRet = ::InternetReadFileExA(m_hRequest, &stInetBuf, 0, (DWORD_PTR)this);
    if (!bRet)
    {
        if (::GetLastError() != ERROR_IO_PENDING)
            return FALSE;

        bRet = WaitForEvent(REQUEST_COMPLETE_EVENT, INFINITE);
        if (!bRet)
            return FALSE;
    }
    else
    {
        bRet = WaitForEvent(USER_CANCEL_EVENT, 0);
        if (!bRet)
            return FALSE;
    }

    dwRecvLen = stInetBuf.dwBufferLength;

    return TRUE;
}

// 关闭HTTP请求函数
void CHttpClient::CloseRequest()
{
    if (m_hCompleteEvent != NULL)
    {
        ::CloseHandle(m_hCompleteEvent);
        m_hCompleteEvent = NULL;
    }

    if (m_hCancelEvent != NULL)
    {
        ::CloseHandle(m_hCancelEvent);
        m_hCancelEvent = NULL;
    }

    if (m_hRequest)
    {
        ::InternetCloseHandle(m_hRequest);
        m_hRequest = NULL;
    }

    if (m_hConnect)
    {
        ::InternetCloseHandle(m_hConnect);
        m_hConnect = NULL;
    }

    if (m_hInternet)
    {
        ::InternetCloseHandle(m_hInternet);
        m_hInternet = NULL;
    }


    m_arrRespHeader.clear();

    m_dwContext = 0;
    m_strProxy = _T("");
    m_strUser = _T("");
    m_strPwd = _T("");
}

// 设置连接超时(单位：毫秒)
void CHttpClient::SetConnectTimeOut(DWORD dwTimeOut)
{
    m_dwConnectTimeOut = dwTimeOut;
}

// 设置取消事件函数
void CHttpClient::SetCancalEvent()
{
    if (m_hCancelEvent != NULL)
        ::SetEvent(m_hCancelEvent);
}

// 设置HTTP代理服务器
void CHttpClient::SetProxy(LPCTSTR lpszServer, WORD nPort, LPCTSTR lpszUser/* = NULL*/, LPCTSTR lpszPwd/* = NULL*/)
{
    if (NULL == lpszServer)
        return;

    TCHAR szProxy[INTERNET_MAX_URL_LENGTH] = { 0 };
    wsprintf(szProxy, _T("%s:%d"), lpszServer, nPort);
    m_strProxy = szProxy;
    if (lpszUser != NULL)
        m_strUser = lpszUser;

    if (lpszPwd != NULL)
        m_strPwd = lpszPwd;
}

// 状态回调函数
void CHttpClient::StatusCallback(HINTERNET hInternet, DWORD dwContext, DWORD dwInternetStatus, LPVOID lpStatusInfo, DWORD dwStatusInfoLen)
{
    CHttpClient * lpThis = (CHttpClient *)dwContext;
    if (NULL == lpThis)
        return;

    switch (dwInternetStatus)
    {
    case INTERNET_STATUS_HANDLE_CREATED:
    {

        if (CONNECTED_EVENT == lpThis->m_dwContext)

        {

            INTERNET_ASYNC_RESULT * lpRes = (INTERNET_ASYNC_RESULT *)lpStatusInfo;

            lpThis->m_hConnect = (HINTERNET)lpRes->dwResult;

            ::SetEvent(lpThis->m_hCompleteEvent);

        }
        else if (REQUEST_OPENED_EVENT == lpThis->m_dwContext)
        {
            INTERNET_ASYNC_RESULT * lpRes = (INTERNET_ASYNC_RESULT *)lpStatusInfo;
            lpThis->m_hRequest = (HINTERNET)lpRes->dwResult;
            ::SetEvent(lpThis->m_hCompleteEvent);
        }
    }

        break;

    case INTERNET_STATUS_REQUEST_SENT:
    {
        DWORD * lpBytesSent = (DWORD *)lpStatusInfo;
    }

        break;

    case INTERNET_STATUS_REQUEST_COMPLETE:
    {

        INTERNET_ASYNC_RESULT * lpRes = (INTERNET_ASYNC_RESULT *)lpStatusInfo;
        ::SetEvent(lpThis->m_hCompleteEvent);
    }

        break;

    case INTERNET_STATUS_RECEIVING_RESPONSE:
    {

    }
        break;

    case INTERNET_STATUS_RESPONSE_RECEIVED:
    {
        DWORD * dwBytesReceived = (DWORD *)lpStatusInfo;
    }
        break;
    }
}

// 解析Url函数(协议，主机名，端口，文件路径)
BOOL CHttpClient::ParseURL(LPCTSTR lpszUrl, LPTSTR lpszScheme, DWORD dwSchemeLength,
                           LPTSTR lpszHostName, DWORD dwHostNameLength, WORD& nPort,
                           LPTSTR lpszUrlPath, DWORD dwUrlPathLength)
{
    URL_COMPONENTS stUrlComponents = { 0 };
    stUrlComponents.dwStructSize = sizeof(URL_COMPONENTS);
    stUrlComponents.lpszScheme = lpszScheme;
    stUrlComponents.dwSchemeLength = dwSchemeLength;
    stUrlComponents.lpszHostName = lpszHostName;
    stUrlComponents.dwHostNameLength = dwHostNameLength;
    stUrlComponents.lpszUrlPath = lpszUrlPath;
    stUrlComponents.dwUrlPathLength = dwUrlPathLength;

    BOOL bRet = ::InternetCrackUrl(lpszUrl, 0, 0, &stUrlComponents);
    if (bRet)
    {
        nPort = stUrlComponents.nPort;
    }

    return bRet;
}



// 等待事件函数

BOOL CHttpClient::WaitForEvent(HTTP_STATUS_EVENT nEvent, DWORD dwTimeOut)
{
    HANDLE hEvent[2] = { 0 };
    int nCount = 0;
    switch (nEvent)
    {
    case CONNECTED_EVENT:
    case REQUEST_OPENED_EVENT:
    case REQUEST_COMPLETE_EVENT:
    {
        hEvent[0] = m_hCancelEvent;
        hEvent[1] = m_hCompleteEvent;
        nCount = 2;
    }
        break;


    case USER_CANCEL_EVENT:
    {
        hEvent[0] = m_hCancelEvent;
        nCount = 1;
    }
        break;


    default:
        return FALSE;
    }

    if (1 == nCount)
    {
        DWORD dwRet = ::WaitForSingleObject(hEvent[0], dwTimeOut);
        if (WAIT_OBJECT_0 == dwRet)
            return FALSE;
        else
            return TRUE;
    }
    else
    {
        DWORD dwRet = ::WaitForMultipleObjects(2, hEvent, FALSE, dwTimeOut);
        if (dwRet != WAIT_OBJECT_0 + 1)
            return FALSE;
        else
            return TRUE;
    }
}



// 获取HTTP响应头长度
DWORD CHttpClient::__GetRespHeaderLen()
{
    BOOL bRet;
    LPVOID lpBuffer = NULL;
    DWORD dwBufferLength = 0;
    bRet = ::HttpQueryInfo(m_hRequest, HTTP_QUERY_RAW_HEADERS_CRLF, lpBuffer, &dwBufferLength, NULL);
    if (!bRet)
    {
        if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            return dwBufferLength;
        }
    }

    return 0;
}

// 获取HTTP响应头，按行保存在m_arrRespHeader数组
BOOL CHttpClient::__GetRespHeader()
{
    CHAR * lpRespHeader;
    DWORD dwRespHeaderLen;
    m_arrRespHeader.clear();

    dwRespHeaderLen = __GetRespHeaderLen();
    if (dwRespHeaderLen <= 0)
        return FALSE;

    lpRespHeader = new CHAR[dwRespHeaderLen];
    if (NULL == lpRespHeader)
        return FALSE;

    memset(lpRespHeader, 0, dwRespHeaderLen);

    BOOL bRet = ::HttpQueryInfo(m_hRequest, HTTP_QUERY_RAW_HEADERS_CRLF, (LPVOID)lpRespHeader, &dwRespHeaderLen, NULL);
    if (!bRet)
    {
        delete[]lpRespHeader;
        lpRespHeader = NULL;
        return FALSE;
    }

    tstring strHeader = (TCHAR *)lpRespHeader;
    tstring strLine;
    int nStart = 0;
    tstring::size_type nPos = strHeader.find(_T("\r\n"), nStart);
    while (nPos != tstring::npos)
    {
        strLine = strHeader.substr(nStart, nPos - nStart);
        if (strLine != _T(""))
            m_arrRespHeader.push_back(strLine);

        nStart = nPos + 2;
        nPos = strHeader.find(_T("\r\n"), nStart);
    }

    delete[]lpRespHeader;
    lpRespHeader = NULL;
    return TRUE;
}
